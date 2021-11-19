#include "Mesh.hpp"

#include "src/3D/BVH.hpp"
#include "src/3D/Gl.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nonstd/span.hpp>

#include <memory>
#include <optional>
#include <vector>

using namespace osc;

union PackedIndex {
    uint32_t u32;
    struct U16Pack { uint16_t a; uint16_t b; } u16;
};

static_assert(sizeof(PackedIndex) == sizeof(uint32_t));
static_assert(alignof(PackedIndex) == alignof(uint32_t));

// internal datastructure of a mesh
struct osc::Mesh::Impl final {

    MeshTopography topography = MeshTopography::Triangles;
    std::vector<glm::vec3> verts;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    IndexFormat indexFormat = IndexFormat::UInt16;
    int numIndices = 0;
    std::vector<PackedIndex> indicesData;
    AABB aabb{};
    Sphere boundingSphere{};
    BVH triangleBVH{};
    bool gpuBuffersOutOfDate = false;

    // lazily-loaded on request
    //
    // this is so that any thread can edit the fields above without touching
    // the GPU, and then the UI thread can load these in when needed
    std::optional<gl::Buffer> maybeVBO = std::nullopt;
    std::optional<gl::Buffer> maybeEBO = std::nullopt;
    std::optional<gl::VertexArray> maybeVAO = std::nullopt;

    Impl() = default;

    Impl(Impl const& other) :
        topography{other.topography},
        verts{other.verts},
        normals{other.normals},
        texCoords{other.texCoords},
        indexFormat{other.indexFormat},
        numIndices{other.numIndices},
        indicesData{other.indicesData},
        aabb{other.aabb},
        boundingSphere{other.boundingSphere},
        triangleBVH{other.triangleBVH},
        gpuBuffersOutOfDate{false},
        maybeVBO{std::nullopt},
        maybeEBO{std::nullopt},
        maybeVAO{std::nullopt}
    {
    }
};

static bool IsGreaterThanU16Max(uint32_t v) noexcept
{
    return v > std::numeric_limits<uint16_t>::max();
}

static bool AnyIndicesGreaterThanU16Max(nonstd::span<uint32_t const> vs)
{
    return std::any_of(vs.begin(), vs.end(), IsGreaterThanU16Max);
}

static std::vector<PackedIndex> repackU32IndicesToU16(nonstd::span<uint32_t const> vs)
{
    int srcN = static_cast<int>(vs.size());
    int destN = (srcN+1)/2;
    std::vector<PackedIndex> rv(destN);

    if (srcN == 0) {
        return rv;
    }

    for (int srcIdx = 0, end = srcN-2; srcIdx < end; srcIdx += 2) {
        int destIdx = srcIdx/2;
        rv[destIdx].u16.a = static_cast<uint16_t>(vs[srcIdx]);
        rv[destIdx].u16.b = static_cast<uint16_t>(vs[srcIdx+1]);
    }

    if (srcN % 2 != 0) {
        int destIdx = destN-1;
        rv[destIdx].u16.a = static_cast<uint16_t>(vs[srcN-1]);
        rv[destIdx].u16.b = 0x0000;
    } else {
        int destIdx = destN-1;
        rv[destIdx].u16.a = static_cast<uint16_t>(vs[srcN-2]);
        rv[destIdx].u16.b = static_cast<uint16_t>(vs[srcN-1]);
    }

    return rv;
}

static std::vector<PackedIndex> unpackU16IndicesToU32(nonstd::span<uint16_t const> vs)
{
    std::vector<PackedIndex> rv(vs.size());

    for (int i = 0; i < static_cast<int>(vs.size()); ++i) {
        rv[i].u32 = static_cast<uint32_t>(vs[i]);
    }

    return rv;
}

static std::vector<PackedIndex> copyU32IndicesToU32(nonstd::span<uint32_t const> vs)
{
    std::vector<PackedIndex> rv(vs.size());

    PackedIndex const* start = reinterpret_cast<PackedIndex const*>(vs.data());
    PackedIndex const* end = start + vs.size();
    std::copy(start, end, rv.data());
    return rv;
}

static std::vector<PackedIndex> copyU16IndicesToU16(nonstd::span<uint16_t const> vs)
{
    int srcN = static_cast<int>(vs.size());
    int destN = (srcN+1)/2;
    std::vector<PackedIndex> rv(destN);

    if (srcN == 0) {
        return rv;
    }

    uint16_t* ptr = &rv[0].u16.a;
    std::copy(vs.data(), vs.data() + vs.size(), ptr);

    if (srcN % 2) {
        ptr[destN-1] = 0x0000;
    }

    return rv;
}

static void RecalculateBounds(osc::Mesh::Impl& impl)
{
    impl.aabb = AABBFromVerts(impl.verts.data(), impl.verts.size());
    impl.boundingSphere = SphereFromVerts(impl.verts.data(), impl.verts.size());
    BVH_BuildFromTriangles(impl.triangleBVH, impl.verts.data(), impl.verts.size());
}

osc::Mesh::Mesh() : m_Impl{new Impl{}}
{
}

osc::Mesh::Mesh(MeshData cpuMesh) :
    m_Impl{new Impl{}}
{
    SetMeshData(std::move(cpuMesh));
}

osc::Mesh::Mesh(Mesh const& other) noexcept :
    m_Impl{new Impl{*other.m_Impl}}
{
}

osc::Mesh::Mesh(Mesh&&) noexcept = default;

osc::Mesh::~Mesh() noexcept = default;

Mesh& osc::Mesh::operator=(Mesh const& other) noexcept
{
    Mesh cpy{other};
    std::swap(m_Impl, cpy.m_Impl);
    return *this;
}

Mesh& osc::Mesh::operator=(Mesh&&) noexcept = default;

bool osc::Mesh::operator==(Mesh const& other) const noexcept
{
    return m_Impl == other.m_Impl;
}

bool osc::Mesh::operator!=(Mesh const& other) const noexcept
{
    return m_Impl != other.m_Impl;
}

bool osc::Mesh::operator<(Mesh const& other) const noexcept
{
    return m_Impl < other.m_Impl;
}

bool osc::Mesh::operator>(Mesh const& other) const noexcept
{
    return m_Impl > other.m_Impl;
}

bool osc::Mesh::operator<=(Mesh const& other) const noexcept
{
    return m_Impl <= other.m_Impl;
}

bool osc::Mesh::operator>=(Mesh const& other) const noexcept
{
    return m_Impl >= other.m_Impl;
}

void osc::Mesh::SetMeshData(MeshData md)
{
    m_Impl->topography = md.topography;
    m_Impl->verts = std::move(md.verts);
    m_Impl->normals = std::move(md.normals);
    m_Impl->texCoords = std::move(md.texcoords);

    // repack indices (if necessary)
    m_Impl->indexFormat = AnyIndicesGreaterThanU16Max(md.indices) ? IndexFormat::UInt32 : IndexFormat::UInt16;
    m_Impl->numIndices = static_cast<int>(md.indices.size());
    if (m_Impl->indexFormat == IndexFormat::UInt32) {
        m_Impl->indicesData = copyU32IndicesToU32(md.indices);
    } else {
        m_Impl->indicesData = repackU32IndicesToU16(md.indices);
    }

    m_Impl->aabb = AABBFromVerts(m_Impl->verts.data(), m_Impl->verts.size());
    m_Impl->boundingSphere = SphereFromVerts(m_Impl->verts.data(), m_Impl->verts.size());
    BVH_BuildFromTriangles(m_Impl->triangleBVH, m_Impl->verts.data(), m_Impl->verts.size());
    m_Impl->gpuBuffersOutOfDate = true;
}

osc::MeshTopography osc::Mesh::GetTopography() const noexcept
{
    return m_Impl->topography;
}

GLenum osc::Mesh::GetTopographyOpenGL() const
{
    switch (m_Impl->topography) {
    case MeshTopography::Triangles:
        return GL_TRIANGLES;
    case MeshTopography::Lines:
        return GL_LINES;
    default:
        throw std::runtime_error{"unsuppored topography"};
    }
}

void osc::Mesh::SetTopography(MeshTopography t)
{
    m_Impl->topography = t;
}

nonstd::span<glm::vec3 const> osc::Mesh::GetVerts() const
{
    return m_Impl->verts;
}

void osc::Mesh::SetVerts(nonstd::span<const glm::vec3> vs)
{
    auto& verts = m_Impl->verts;
    verts.clear();
    verts.reserve(vs.size());
    for (glm::vec3 const& v : vs) {
        verts.push_back(v);
    }

    RecalculateBounds(*m_Impl);
    m_Impl->gpuBuffersOutOfDate = true;
}

nonstd::span<glm::vec3 const> osc::Mesh::GetNormals() const
{
    return m_Impl->normals;
}

void osc::Mesh::SetNormals(nonstd::span<const glm::vec3> ns)
{
    auto& norms = m_Impl->normals;
    norms.clear();
    norms.reserve(ns.size());
    for (glm::vec3 const& v : ns) {
        norms.push_back(v);
    }

    m_Impl->gpuBuffersOutOfDate = true;
}

nonstd::span<glm::vec2 const> osc::Mesh::GetTexCoords() const
{
    return m_Impl->texCoords;
}

void osc::Mesh::SetTexCoords(nonstd::span<const glm::vec2> tc)
{
    auto& coords = m_Impl->texCoords;
    coords.clear();
    coords.reserve(tc.size());
    for (glm::vec2 const& t : tc) {
        coords.push_back(t);
    }

    m_Impl->gpuBuffersOutOfDate = true;
}

void osc::Mesh::ScaleTexCoords(float factor)
{
    for (auto& tc : m_Impl->texCoords) {
        tc *= factor;
    }
    m_Impl->gpuBuffersOutOfDate = true;
}

IndexFormat osc::Mesh::GetIndexFormat() const
{
    return m_Impl->indexFormat;
}

GLenum osc::Mesh::GetIndexFormatOpenGL() const
{
    return m_Impl->indexFormat == IndexFormat::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

void osc::Mesh::SetIndexFormat(IndexFormat newFormat)
{
    IndexFormat oldFormat = m_Impl->indexFormat;

    if (newFormat == oldFormat) {
        return;
    }

    m_Impl->indexFormat = newFormat;

    // format changed: need to pack/unpack the data
    PackedIndex const* pis = m_Impl->indicesData.data();

    if (newFormat == IndexFormat::UInt16) {
        uint32_t const* existing = &pis[0].u32;
        nonstd::span<uint32_t const> s(existing, m_Impl->numIndices);
        m_Impl->indicesData = repackU32IndicesToU16(s);
    } else {
        uint16_t const* existing = &pis[0].u16.a;
        nonstd::span<uint16_t const> s(existing, m_Impl->numIndices);
        m_Impl->indicesData = unpackU16IndicesToU32(s);
    }

    m_Impl->gpuBuffersOutOfDate = true;
}

int osc::Mesh::GetNumIndices() const
{
    return m_Impl->numIndices;
}

std::vector<uint32_t> osc::Mesh::GetIndices() const
{
    int numIndices = m_Impl->numIndices;
    PackedIndex const* pis = m_Impl->indicesData.data();

    if (m_Impl->indexFormat == IndexFormat::UInt16) {
        uint16_t const* ptr = &pis[0].u16.a;
        nonstd::span<uint16_t const> s(ptr, numIndices);

        std::vector<uint32_t> rv;
        rv.reserve(numIndices);
        for (uint16_t v : s) {
            rv.push_back(static_cast<uint32_t>(v));
        }
        return rv;
    } else {
        uint32_t const* ptr = &pis[0].u32;
        nonstd::span<uint32_t const> s(ptr, numIndices);

        return std::vector<uint32_t>(s.begin(), s.end());
    }
}

void osc::Mesh::SetIndicesU16(nonstd::span<const uint16_t> vs)
{
    if (m_Impl->indexFormat == IndexFormat::UInt16) {
        m_Impl->indicesData = copyU16IndicesToU16(vs);
    } else {
        m_Impl->indicesData = unpackU16IndicesToU32(vs);
    }

    RecalculateBounds(*m_Impl);
    m_Impl->numIndices = static_cast<int>(vs.size());
    m_Impl->gpuBuffersOutOfDate = true;
}

void osc::Mesh::SetIndicesU32(nonstd::span<const uint32_t> vs)
{
    if (m_Impl->indexFormat == IndexFormat::UInt16) {
        m_Impl->indicesData = repackU32IndicesToU16(vs);
    } else {
        m_Impl->indicesData = copyU32IndicesToU32(vs);
    }

    RecalculateBounds(*m_Impl);
    m_Impl->numIndices = static_cast<int>(vs.size());
    m_Impl->gpuBuffersOutOfDate = true;
}

AABB const& osc::Mesh::GetAABB() const
{
    return m_Impl->aabb;
}

Sphere const& osc::Mesh::GetBoundingSphere() const
{
    return m_Impl->boundingSphere;
}

RayCollision osc::Mesh::GetClosestRayTriangleCollisionModelspace(Ray const& ray) const
{
    if (m_Impl->topography != MeshTopography::Triangles) {
        return RayCollision{false, 0.0f};
    }

    BVHCollision coll;
    bool collided = BVH_GetClosestRayTriangleCollision(m_Impl->triangleBVH, m_Impl->verts.data(), m_Impl->verts.size(), ray, &coll);

    if (collided) {
        return RayCollision{true, coll.distance};
    } else {
        return RayCollision{false, 0.0f};
    }
}

RayCollision osc::Mesh::GetRayMeshCollisionInWorldspace(glm::mat4 const& model2world, Ray const& worldspaceLine) const
{
    // do a fast ray-to-AABB collision test
    AABB modelspaceAABB = GetAABB();
    AABB worldspaceAABB = AABBApplyXform(modelspaceAABB, model2world);

    RayCollision rayAABBCollision = GetRayCollisionAABB(worldspaceLine, worldspaceAABB);

    if (!rayAABBCollision.hit) {
        return rayAABBCollision;  // missed the AABB, so *definitely* missed the mesh
    }

    // it hit the AABB, so it *may* have hit a triangle in the mesh
    //
    // refine the hittest by doing a slower ray-to-triangle test
    glm::mat4 world2model = glm::inverse(model2world);
    Ray modelspaceLine = RayApplyXform(worldspaceLine, world2model);

    return GetClosestRayTriangleCollisionModelspace(modelspaceLine);
}

void osc::Mesh::clear()
{
    *this = Mesh{};
}

void osc::Mesh::UploadToGPU()
{
    // pack CPU-side mesh data (verts, etc.), which is separate, into a
    // suitable GPU-side buffer
    nonstd::span<glm::vec3 const> verts = GetVerts();
    nonstd::span<glm::vec3 const> normals = GetNormals();
    nonstd::span<glm::vec2 const> uvs = GetTexCoords();

    bool hasNormals = !normals.empty();
    bool hasUvs = !uvs.empty();

    if (hasNormals && normals.size() != verts.size()) {
        throw std::runtime_error{"number of normals != number of verts"};
    }

    if (hasUvs && uvs.size() != verts.size()) {
        throw std::runtime_error{"number of uvs != number of verts"};
    }

    size_t stride = sizeof(decltype(verts)::element_type);
    if (hasNormals) {
        stride += sizeof(decltype(normals)::element_type);
    }
    if (hasUvs) {
        stride += sizeof(decltype(uvs)::element_type);
    }

    std::vector<unsigned char> data;
    data.reserve(stride * verts.size());

    auto pushFloat = [&data](float v) {
        data.push_back(reinterpret_cast<unsigned char*>(&v)[0]);
        data.push_back(reinterpret_cast<unsigned char*>(&v)[1]);
        data.push_back(reinterpret_cast<unsigned char*>(&v)[2]);
        data.push_back(reinterpret_cast<unsigned char*>(&v)[3]);
    };

    for (size_t i = 0; i < verts.size(); ++i) {
        pushFloat(verts[i].x);
        pushFloat(verts[i].y);
        pushFloat(verts[i].z);
        if (hasNormals) {
            pushFloat(normals[i].x);
            pushFloat(normals[i].y);
            pushFloat(normals[i].z);
        }
        if (hasUvs) {
            pushFloat(uvs[i].x);
            pushFloat(uvs[i].y);
        }
    }

    if (data.size() != stride * verts.size()) {
        throw std::runtime_error{"unexpected size"};
    }

    // allocate VBO handle on GPU if not-yet allocated. Upload the
    // data to the VBO
    if (!m_Impl->maybeVBO) {
        m_Impl->maybeVBO = gl::Buffer{};
    }
    gl::Buffer& vbo = *m_Impl->maybeVBO;
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);

    // allocate EBO handle on GPU if not-yet allocated. Upload the
    // data to the EBO
    if (!m_Impl->maybeEBO) {
        m_Impl->maybeEBO = gl::Buffer{};
    }
    gl::Buffer& ebo = *m_Impl->maybeEBO;
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    size_t eboSize = m_Impl->indexFormat == IndexFormat::UInt16 ? sizeof(uint16_t) * m_Impl->numIndices : sizeof(uint32_t) * m_Impl->numIndices;
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, eboSize, m_Impl->indicesData.data(), GL_STATIC_DRAW);

    // always allocate a new VAO in case the old one has stuff lying around
    // in it from an old format
    //
    // upload the packing format to the VAO
    m_Impl->maybeVAO = gl::VertexArray{};
    gl::VertexArray& vao = *m_Impl->maybeVAO;

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    int offset = 0;
    auto int2void = [](int v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); };

    glVertexAttribPointer(SHADER_LOC_VERTEX_POSITION, 3, GL_FLOAT, false, static_cast<GLsizei>(stride), int2void(offset));
    glEnableVertexAttribArray(SHADER_LOC_VERTEX_POSITION);
    offset += 3 * sizeof(float);

    if (hasNormals) {
        glVertexAttribPointer(SHADER_LOC_VERTEX_NORMAL, 3, GL_FLOAT, false, static_cast<GLsizei>(stride), int2void(offset));
        glEnableVertexAttribArray(SHADER_LOC_VERTEX_NORMAL);
        offset += 3 * sizeof(float);
    }

    if (hasUvs) {
        glVertexAttribPointer(SHADER_LOC_VERTEX_TEXCOORD01, 2, GL_FLOAT, false, static_cast<GLsizei>(stride), int2void(offset));
        glEnableVertexAttribArray(SHADER_LOC_VERTEX_TEXCOORD01);
    }

    gl::BindVertexArray();

    m_Impl->gpuBuffersOutOfDate = false;
}

gl::VertexArray& osc::Mesh::GetVertexArray()
{
    if (m_Impl->gpuBuffersOutOfDate ||
            !m_Impl->maybeVBO ||
            !m_Impl->maybeVAO ||
            !m_Impl->maybeEBO) {

        UploadToGPU();
    }

    return *m_Impl->maybeVAO;
}

void osc::Mesh::Draw() const
{
    gl::DrawElements(GetTopographyOpenGL(), GetNumIndices(), GetIndexFormatOpenGL(), nullptr);
}

void osc::Mesh::DrawInstanced(size_t n) const
{
    glDrawElementsInstanced(GetTopographyOpenGL(), GetNumIndices(), GetIndexFormatOpenGL(), nullptr, static_cast<GLsizei>(n));
}
