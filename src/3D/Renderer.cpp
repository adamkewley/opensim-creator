#include "Renderer.hpp"

#include "src/Utils/UID.hpp"

#define OSC_NOT_IMPLEMENTED


// 3D implementation notes
//
// - almost all public-API types are designed to be copy-on-write classes that
//   downstream code can easily store, compare, etc. "as-if" the callers were
//   dealing with value-types
//
// - the value types are comparable, assignable, etc. so that they can be easily
//   used downstream. Most of them are also printable
//
// - hashing etc. needs to ensure a unique hash value is generated even if the
//   same implementation pointer is hashed (e.g. because the caller could be
//   using the hash as a way of caching downstream data). Over-hashing is
//   preferred to under-hashing (i.e. it's preferable for the same "value" to
//   hash to something different because something like a version number was
//   incremented, even if the values are the same, because that's better than
//   emitting the same hash value for different actual values)
//
// - all the implementation details are stuffed in here because the rendering
//   classes are going to be used all over the codebase and it's feasible that
//   little internal changes will happen here as other backends, caching, etc.
//   are supported

// helpers
namespace
{
    template<typename T>
    void DoCopyOnWrite(std::shared_ptr<T>& p)
    {
        if (p.use_count() == 1)
        {
            return;  // sole owner
        }

        p = std::make_shared<T>(*p);
    }

    size_t CombineHashes(size_t a, size_t b)
    {
        return a ^ (b + 0x9e3779b9 + (a<<6) + (a>>2));
    }
}

// basic command buffer impl
//
// this is how the backend stores raw drawcalls ready for rendering via the
// platform's graphics API
namespace
{
}

namespace
{
    enum class IndexFormat {
        UInt16,
        UInt32,
    };

    // represents index data, which may be packed in-memory depending
    // on how it's represented
    union PackedIndex {
        uint32_t u32;
        struct U16Pack { uint16_t a; uint16_t b; } u16;
    };

    static_assert(sizeof(PackedIndex) == sizeof(uint32_t), "double-check that uint32_t is correctly represented on this machine");
    static_assert(alignof(PackedIndex) == alignof(uint32_t), "careful: the union is type-punning between 16- and 32-bit data");

    bool IsAnyValueGreaterThanU16Max(nonstd::span<uint32_t const> vs)
    {
        return std::any_of(vs.begin(), vs.end(), [](uint32_t v)
        {
            return v > std::numeric_limits<uint16_t>::max();
        });
    }

    // pack u16 indices into a u16 `PackedIndex` vector
    void PackAsU16(nonstd::span<uint16_t const> vs, std::vector<PackedIndex>& out)
    {
        if (vs.empty())
        {
            out.clear();
            return;
        }

        out.resize((vs.size()+1)/2);

        std::copy(vs.data(), vs.data() + vs.size(), &out.front().u16.a);

        // zero out the second part of the packed index if there was an odd number
        // of input indices (the 2nd half will be left untouched by the copy above)
        if (vs.size() % 2)
        {
            out.back().u16.b = {};
        }
    }

    void PackAsU16(nonstd::span<uint32_t const> vs, std::vector<PackedIndex>& out)
    {
        if (vs.empty())
        {
            out.clear();
            return;
        }

        out.resize((vs.size()+1)/2);

        // pack two 32-bit source values into one `PackedIndex` per iteration
        for (size_t i = 0, end = vs.size()-1; i < end; i += 2)
        {
            PackedIndex& pi = out[i/2];
            pi.u16.a = static_cast<uint16_t>(vs[i]);
            pi.u16.b = static_cast<uint16_t>(vs[i+1]);
        }

        // with an odd number of source values, pack the trailing 32-bit value into
        // the first part of the packed index and zero out the second part
        if (vs.size() % 2)
        {
            PackedIndex& pi = out.back();
            pi.u16.a = static_cast<uint16_t>(vs.back());
            pi.u16.b = {};
        }
    }

    void PackAsU32(nonstd::span<uint16_t const> vs, std::vector<PackedIndex>& out)
    {
        out.clear();
        out.reserve(vs.size());

        for (uint16_t const& v : vs)
        {
            out.push_back(PackedIndex{static_cast<uint32_t>(v)});
        }
    }

    void PackAsU32(nonstd::span<uint32_t const> vs, std::vector<PackedIndex>& out)
    {
        out.clear();
        out.resize(vs.size());
        std::copy(vs.data(), vs.data() + vs.size(), &out.front().u32);
    }

    std::vector<uint32_t> UnpackU32ToU32(nonstd::span<PackedIndex const> data, int numIndices)
    {
        uint32_t const* begin = &data[0].u32;
        uint32_t const* end = begin + numIndices;
        return std::vector<uint32_t>(begin, end);
    }

    std::vector<uint32_t> UnpackU16ToU32(nonstd::span<PackedIndex const> data, int numIndices)
    {
        std::vector<uint32_t> rv;
        rv.reserve(numIndices);

        uint16_t const* it = &data[0].u16.a;
        uint16_t const* end = it + numIndices;
        for (; it != end; ++it)
        {
            rv.push_back(static_cast<uint32_t>(*it));
        }

        return rv;
    }
}


// osc::Mesh::Impl

class osc::Mesh::Impl final {
public:

    MeshTopographyNew getTopography() const
    {
        return m_Topography;
    }

    void setTopography(MeshTopographyNew mt)
    {
        m_Topography = mt;
        m_VersionCounter++;
    }

    nonstd::span<glm::vec3 const> getVerts() const
    {
        return m_Verts;
    }

    void setVerts(nonstd::span<const glm::vec3> vs)
    {
        m_Verts.clear();
        m_Verts.reserve(vs.size());

        for (glm::vec3 const& v : vs)
        {
            m_Verts.push_back(v);
        }

        m_GpuBuffersOutOfDate = true;
        m_VersionCounter++;
        recalculateBounds();
    }

    nonstd::span<glm::vec3 const> getNormals() const
    {
        return m_Normals;
    }

    void setNormals(nonstd::span<const glm::vec3> ns)
    {
        m_Normals.clear();
        m_Normals.reserve(ns.size());

        for (glm::vec3 const& v : ns)
        {
            m_Normals.push_back(v);
        }

        m_GpuBuffersOutOfDate = true;
        m_VersionCounter++;
    }

    nonstd::span<glm::vec2 const> getTexCoords() const
    {
        return m_TexCoords;
    }

    void setTexCoords(nonstd::span<const glm::vec2> tc)
    {
        m_TexCoords.clear();
        m_TexCoords.reserve(tc.size());

        for (glm::vec2 const& t : tc)
        {
            m_TexCoords.push_back(t);
        }

        m_GpuBuffersOutOfDate = true;
        m_VersionCounter++;
    }

    void scaleTexCoords(float factor)
    {
        for (auto& tc : m_TexCoords)
        {
            tc *= factor;
        }

        m_GpuBuffersOutOfDate = true;
        m_VersionCounter++;
    }

    int getNumIndices() const
    {
        return m_NumIndices;
    }

    std::vector<uint32_t> getIndices() const
    {
        if (m_IndicesData.empty())
        {
            return std::vector<uint32_t>{};
        }
        else if (m_IndexFormat == IndexFormat::UInt32)
        {
            return UnpackU32ToU32(m_IndicesData, m_NumIndices);
        }
        else
        {
            return UnpackU16ToU32(m_IndicesData, m_NumIndices);
        }
    }

    template<typename T>
    void setIndices(nonstd::span<T const> vs)
    {
        if (m_IndexFormat == IndexFormat::UInt16)
        {
            PackAsU16(vs, m_IndicesData);
        }
        else
        {
            PackAsU32(vs, m_IndicesData);
        }

        m_NumIndices = static_cast<int>(vs.size());
        m_GpuBuffersOutOfDate = true;
        m_VersionCounter++;
        recalculateBounds();
    }

    AABB const& getBounds() const
    {
        return m_AABB;
    }

    void clear()
    {
        m_Verts.clear();
        m_Normals.clear();
        m_TexCoords.clear();
        m_NumIndices = 0;
        m_IndicesData.clear();
        m_AABB = AABB{};
        m_GpuBuffersOutOfDate = true;
        // don't reset m_VersionCounter - we don't want to risk hash collisions
        // TODO: reset any GPU handles
    }

    void recalculateBounds()
    {
        // TODO
    }

    size_t getHash() const
    {
        size_t idHash = std::hash<UID>{}(m_ID);
        size_t versionHash = std::hash<int>{}(versionHash);
        return CombineHashes(idHash, versionHash);
    }

private:
    UID m_ID = GenerateID();  // TODO: reset on copy
    MeshTopographyNew m_Topography = MeshTopographyNew_Triangles;
    std::vector<glm::vec3> m_Verts;
    std::vector<glm::vec3> m_Normals;
    std::vector<glm::vec2> m_TexCoords;
    IndexFormat m_IndexFormat = IndexFormat::UInt16;
    int m_NumIndices = 0;
    std::vector<PackedIndex> m_IndicesData;
    AABB m_AABB = {};
    int m_VersionCounter = 0;
    bool m_GpuBuffersOutOfDate = true;  // TODO: reset on copy

    // TODO: backend-dependent data
};


// osc::Texture2D::Impl

class osc::Texture2D::Impl final {

};


// osc::Shader::Impl

class osc::Shader::Impl final {

};


// osc::Material::Impl

class osc::Material::Impl final {

};


// osc::MaterialPropertyBlock::Impl

class osc::MaterialPropertyBlock::Impl final {

};


// osc::CameraNew::Impl

class osc::CameraNew::Impl final {

};



// PUBLIC API


// osc::MeshTopographyNew

std::ostream& osc::operator<<(std::ostream&, MeshTopographyNew)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(MeshTopographyNew)
{
    OSC_NOT_IMPLEMENTED
}


// osc::Mesh impl

osc::Mesh::Mesh() : m_Impl{std::make_shared<Impl>()} {}

osc::Mesh::Mesh(Mesh const&) = default;

osc::Mesh::Mesh(Mesh&&) noexcept = default;

osc::Mesh::~Mesh() noexcept = default;

osc::Mesh& osc::Mesh::operator=(Mesh const&) = default;

osc::Mesh& osc::Mesh::operator=(Mesh&&) noexcept = default;

bool osc::Mesh::operator==(Mesh const& o) const
{
    return m_Impl == o.m_Impl;
}

bool osc::Mesh::operator!=(Mesh const& o) const
{
    return m_Impl != o.m_Impl;
}

bool osc::Mesh::operator<(Mesh const& o) const
{
    return m_Impl < o.m_Impl;
}

osc::MeshTopographyNew osc::Mesh::getTopography() const
{
    return m_Impl->getTopography();
}

void osc::Mesh::setTopography(MeshTopographyNew mt)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTopography(mt);
}

nonstd::span<glm::vec3 const> osc::Mesh::getVerts() const
{
    return m_Impl->getVerts();
}

void osc::Mesh::setVerts(nonstd::span<const glm::vec3> vs)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVerts(vs);
}

nonstd::span<glm::vec3 const> osc::Mesh::getNormals() const
{
    return m_Impl->getNormals();
}

void osc::Mesh::setNormals(nonstd::span<const glm::vec3> vs)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setNormals(vs);
}

nonstd::span<glm::vec2 const> osc::Mesh::getTexCoords() const
{
    return m_Impl->getTexCoords();
}

void osc::Mesh::setTexCoords(nonstd::span<const glm::vec2> tc)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexCoords(tc);
}

void osc::Mesh::scaleTexCoords(float factor)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->scaleTexCoords(factor);
}

int osc::Mesh::getNumIndices() const
{
    return m_Impl->getNumIndices();
}

std::vector<uint32_t> osc::Mesh::getIndices() const
{
    return m_Impl->getIndices();
}

void osc::Mesh::setIndices(nonstd::span<const uint16_t> vs)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setIndices(vs);
}

void osc::Mesh::setIndices(nonstd::span<const uint32_t> vs)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setIndices(vs);
}

osc::AABB const& osc::Mesh::getBounds() const
{
    return m_Impl->getBounds();
}

void osc::Mesh::clear()
{
    DoCopyOnWrite(m_Impl);
    m_Impl->clear();
}

std::ostream& osc::operator<<(std::ostream&, Mesh const&)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(Mesh const&)
{
    OSC_NOT_IMPLEMENTED
}

size_t std::hash<osc::Mesh>::operator()(osc::Mesh const& mesh) const
{
    return mesh.m_Impl->getHash();
}


// osc::TextureWrapMode

std::ostream& osc::operator<<(std::ostream&, osc::TextureWrapMode)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(TextureWrapMode)
{
    OSC_NOT_IMPLEMENTED
}


// osc::TextureFilterMode

std::ostream& osc::operator<<(std::ostream&, osc::TextureFilterMode)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(TextureFilterMode)
{
    OSC_NOT_IMPLEMENTED
}


// osc::Texture2D

osc::Texture2D::Texture2D(int width, int height, nonstd::span<Rgba32 const>)
{
    OSC_NOT_IMPLEMENTED
}

osc::Texture2D::Texture2D(int width, int height, nonstd::span<glm::vec4 const>)
{
    OSC_NOT_IMPLEMENTED
}

osc::Texture2D::Texture2D(Texture2D const&) = default;

osc::Texture2D::Texture2D(Texture2D&&) noexcept = default;

osc::Texture2D::~Texture2D() noexcept = default;

osc::Texture2D& osc::Texture2D::operator=(Texture2D const&) = default;

osc::Texture2D& osc::Texture2D::operator=(Texture2D&&) noexcept = default;

bool osc::Texture2D::operator==(Texture2D const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Texture2D::operator!=(Texture2D const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Texture2D::operator<(Texture2D const&) const
{
    OSC_NOT_IMPLEMENTED
}

int osc::Texture2D::getWidth() const
{
    OSC_NOT_IMPLEMENTED
}

int osc::Texture2D::getHeight() const
{
    OSC_NOT_IMPLEMENTED
}

float osc::Texture2D::getAspectRatio() const
{
    OSC_NOT_IMPLEMENTED
}

osc::TextureWrapMode osc::Texture2D::getWrapMode() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Texture2D::setWrapMode(TextureWrapMode)
{
    OSC_NOT_IMPLEMENTED
}

osc::TextureWrapMode osc::Texture2D::getWrapModeU() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Texture2D::setWrapModeU(TextureWrapMode)
{
    OSC_NOT_IMPLEMENTED
}

osc::TextureWrapMode osc::Texture2D::getWrapModeV() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Texture2D::setWrapModeV(TextureWrapMode)
{
    OSC_NOT_IMPLEMENTED
}

osc::TextureWrapMode osc::Texture2D::getWrapModeW() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Texture2D::setWrapModeW(TextureWrapMode)
{
    OSC_NOT_IMPLEMENTED
}

osc::TextureFilterMode osc::Texture2D::getFilterMode() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Texture2D::setFilterMode(TextureFilterMode)
{
    OSC_NOT_IMPLEMENTED
}

void* osc::Texture2D::getRawHandle()
{
    OSC_NOT_IMPLEMENTED
}

std::ostream& osc::operator<<(std::ostream&, Texture2D const&)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(Texture2D const&)
{
    OSC_NOT_IMPLEMENTED
}

size_t std::hash<osc::Texture2D>::operator()(osc::Texture2D const&)
{
    OSC_NOT_IMPLEMENTED
}


// osc::ShaderType

std::ostream& osc::operator<<(std::ostream&, ShaderType)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(ShaderType)
{
    OSC_NOT_IMPLEMENTED
}


// shader IDs

size_t osc::ConvertPropertyNameToNameID(std::string_view)
{
    OSC_NOT_IMPLEMENTED
}


// osc::Shader

osc::Shader::Shader(char const*)
{
    OSC_NOT_IMPLEMENTED
}

osc::Shader osc::Shader::compile(const char *)
{
    OSC_NOT_IMPLEMENTED
}

osc::Shader::Shader(Shader const&) = default;

osc::Shader::Shader(Shader&&) noexcept = default;

osc::Shader::~Shader() noexcept = default;

osc::Shader& osc::Shader::operator=(Shader const&) = default;

osc::Shader& osc::Shader::operator=(Shader&&) noexcept = default;

bool osc::Shader::operator==(Shader const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Shader::operator!=(Shader const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Shader::operator<(Shader const&) const
{
    OSC_NOT_IMPLEMENTED
}

std::string const& osc::Shader::getName() const
{
    OSC_NOT_IMPLEMENTED
}

int osc::Shader::findPropertyIndex(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

int osc::Shader::findPropertyIndex(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

int osc::Shader::getPropertyCount() const
{
    OSC_NOT_IMPLEMENTED
}

std::string const& osc::Shader::getPropertyName(int propertyIndex) const
{
    OSC_NOT_IMPLEMENTED
}

size_t osc::Shader::getPropertyNameID(int propertyIndex) const
{
    OSC_NOT_IMPLEMENTED
}

osc::ShaderType osc::Shader::getPropertyType(int propertyIndex) const
{
    OSC_NOT_IMPLEMENTED
}

std::ostream& osc::operator<<(std::ostream&, Shader const&)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(Shader const&)
{
    OSC_NOT_IMPLEMENTED
}

size_t std::hash<osc::Shader>::operator()(osc::Shader const&) const
{
    OSC_NOT_IMPLEMENTED
}


// osc::Material

osc::Material::Material(Shader)
{
    OSC_NOT_IMPLEMENTED
}

osc::Material::Material(Material const&) = default;

osc::Material::Material(Material&&) noexcept = default;

osc::Material::~Material() noexcept = default;

osc::Material& osc::Material::operator=(Material const&) = default;

osc::Material& osc::Material::operator=(Material&&) noexcept = default;

bool osc::Material::operator==(Material const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Material::operator!=(Material const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Material::operator<(Material const&) const
{
    OSC_NOT_IMPLEMENTED
}

osc::Shader const& osc::Material::getShader() const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Material::hasProperty(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::Material::hasProperty(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

float osc::Material::getFloat(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

float osc::Material::getFloat(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setFloat(std::string_view propertyName, float)
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setFloat(size_t propertyNameID, float)
{
    OSC_NOT_IMPLEMENTED
}

int osc::Material::getInt(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

int osc::Material::getInt(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setInt(std::string_view propertyName, int)
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setInt(size_t propertyNameID, int)
{
    OSC_NOT_IMPLEMENTED
}

osc::Texture2D const& osc::Material::getTexture(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

osc::Texture2D const& osc::Material::getTexture(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setTexture(std::string_view propertyName, Texture2D)
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setTexture(size_t propertyNameID, Texture2D)
{
    OSC_NOT_IMPLEMENTED
}

glm::vec4 const& osc::Material::getVector(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

glm::vec4 const& osc::Material::getVector(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setVector(std::string_view propertyName, glm::vec4 const&)
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setVector(size_t propertyNameID, glm::vec4 const&)
{
    OSC_NOT_IMPLEMENTED
}

glm::mat4 const& osc::Material::getMatrix(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

glm::mat4 const& osc::Material::getMatrix(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setMatrix(std::string_view propertyName, glm::mat4 const&)
{
    OSC_NOT_IMPLEMENTED
}

void osc::Material::setMatrix(size_t propertyNameID, glm::mat4 const&)
{
    OSC_NOT_IMPLEMENTED
}

std::ostream& osc::operator<<(std::ostream&, Material const&)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(Material const&)
{
    OSC_NOT_IMPLEMENTED
}

size_t std::hash<osc::Material>::operator()(osc::Material const&) const
{
    OSC_NOT_IMPLEMENTED
}


// osc::MaterialPropertyBlock

osc::MaterialPropertyBlock::MaterialPropertyBlock()
{
    OSC_NOT_IMPLEMENTED
}

osc::MaterialPropertyBlock::MaterialPropertyBlock(MaterialPropertyBlock const&)
{
    OSC_NOT_IMPLEMENTED
}

osc::MaterialPropertyBlock::MaterialPropertyBlock(MaterialPropertyBlock&&) noexcept
{
    OSC_NOT_IMPLEMENTED
}

osc::MaterialPropertyBlock::~MaterialPropertyBlock() noexcept
{
    OSC_NOT_IMPLEMENTED
}

osc::MaterialPropertyBlock& osc::MaterialPropertyBlock::operator=(MaterialPropertyBlock const&)
{
    OSC_NOT_IMPLEMENTED
}

osc::MaterialPropertyBlock& osc::MaterialPropertyBlock::operator=(MaterialPropertyBlock&&) noexcept
{
    OSC_NOT_IMPLEMENTED
}

bool osc::MaterialPropertyBlock::operator==(MaterialPropertyBlock const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::MaterialPropertyBlock::operator!=(MaterialPropertyBlock const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::MaterialPropertyBlock::operator<(MaterialPropertyBlock const&) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::clear()
{
    OSC_NOT_IMPLEMENTED
}

bool osc::MaterialPropertyBlock::isEmpty() const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::MaterialPropertyBlock::hasProperty(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::MaterialPropertyBlock::hasProperty(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

float osc::MaterialPropertyBlock::getFloat(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

float osc::MaterialPropertyBlock::getFloat(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setFloat(std::string_view propertyName, float)
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setFloat(size_t propertyNameID, float)
{
    OSC_NOT_IMPLEMENTED
}

int osc::MaterialPropertyBlock::getInt(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

int osc::MaterialPropertyBlock::getInt(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setInt(std::string_view propertyName, int)
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setInt(size_t propertyNameID, int)
{
    OSC_NOT_IMPLEMENTED
}

osc::Texture2D const& osc::MaterialPropertyBlock::getTexture(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

osc::Texture2D const& osc::MaterialPropertyBlock::getTexture(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setTexture(std::string_view propertyName, Texture2D)
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setTexture(size_t propertyNameID, Texture2D)
{
    OSC_NOT_IMPLEMENTED
}

glm::vec4 const& osc::MaterialPropertyBlock::getVector(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

glm::vec4 const& osc::MaterialPropertyBlock::getVector(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setVector(std::string_view propertyName, glm::vec4 const&)
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setVector(size_t propertyNameID, glm::vec4 const&)
{
    OSC_NOT_IMPLEMENTED
}

glm::mat4 const& osc::MaterialPropertyBlock::getMatrix(std::string_view propertyName) const
{
    OSC_NOT_IMPLEMENTED
}

glm::mat4 const& osc::MaterialPropertyBlock::getMatrix(size_t propertyNameID) const
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setMatrix(std::string_view propertyName, glm::mat4 const&)
{
    OSC_NOT_IMPLEMENTED
}

void osc::MaterialPropertyBlock::setMatrix(size_t propertyNameID, glm::mat4 const&)
{
    OSC_NOT_IMPLEMENTED
}

std::ostream& osc::operator<<(std::ostream&, MaterialPropertyBlock const&)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(MaterialPropertyBlock const&)
{
    OSC_NOT_IMPLEMENTED
}

size_t std::hash<osc::MaterialPropertyBlock>::operator()(osc::MaterialPropertyBlock const&) const
{
    OSC_NOT_IMPLEMENTED
}


// osc::CameraProjection

std::ostream& osc::operator<<(std::ostream&, CameraProjection)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(CameraProjection)
{
    OSC_NOT_IMPLEMENTED
}


// osc::CameraNew

osc::CameraNew::CameraNew()
{
    OSC_NOT_IMPLEMENTED
}

osc::CameraNew::CameraNew(Texture2D)
{
    OSC_NOT_IMPLEMENTED
}

osc::CameraNew::CameraNew(CameraNew const&)
{
    OSC_NOT_IMPLEMENTED
}

osc::CameraNew::CameraNew(CameraNew&&) noexcept
{
    OSC_NOT_IMPLEMENTED
}

osc::CameraNew::~CameraNew() noexcept
{
    OSC_NOT_IMPLEMENTED
}

osc::CameraNew& osc::CameraNew::operator=(CameraNew const&)
{
    OSC_NOT_IMPLEMENTED
}

osc::CameraNew& osc::CameraNew::operator=(CameraNew&&) noexcept
{
    OSC_NOT_IMPLEMENTED
}

bool osc::CameraNew::operator==(CameraNew const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::CameraNew::operator!=(CameraNew const&) const
{
    OSC_NOT_IMPLEMENTED
}

bool osc::CameraNew::operator<(CameraNew const&) const
{
    OSC_NOT_IMPLEMENTED
}

glm::vec4 const& osc::CameraNew::getBackgroundColor() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setBackgroundColor(glm::vec4 const&)
{
    OSC_NOT_IMPLEMENTED
}

osc::CameraProjection osc::CameraNew::getCameraProjection() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setCameraProjection(CameraProjection)
{
    OSC_NOT_IMPLEMENTED
}

float osc::CameraNew::getOrthographicSize() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setOrthographicSize(glm::vec2)
{
    OSC_NOT_IMPLEMENTED
}

float osc::CameraNew::getCameraFOV() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setCameraFOV()
{
    OSC_NOT_IMPLEMENTED
}

float osc::CameraNew::getNearClippingPlane() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setNearClippingPlane(float)
{
    OSC_NOT_IMPLEMENTED
}

float osc::CameraNew::getFarClippingPlane() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setFarClippingPlane(float)
{
    OSC_NOT_IMPLEMENTED
}

std::optional<osc::Texture2D> osc::CameraNew::getTexture() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setTexture(Texture2D)
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setTexture()
{
    OSC_NOT_IMPLEMENTED
}

int osc::CameraNew::getPixelWidth() const
{
    OSC_NOT_IMPLEMENTED
}

int osc::CameraNew::getPixelHeight() const
{
    OSC_NOT_IMPLEMENTED
}

float osc::CameraNew::getAspectRatio() const
{
    OSC_NOT_IMPLEMENTED
}

std::optional<osc::Rect> osc::CameraNew::getScissorRect() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setScissorRect(Rect const&)
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::setScissorRect()
{
    OSC_NOT_IMPLEMENTED
}

glm::mat4 const& osc::CameraNew::getCameraToWorldMatrix() const
{
    OSC_NOT_IMPLEMENTED
}

void osc::CameraNew::render()
{
    OSC_NOT_IMPLEMENTED
}

std::ostream& osc::operator<<(std::ostream&, CameraNew const&)
{
    OSC_NOT_IMPLEMENTED
}

std::string osc::to_string(CameraNew const&)
{
    OSC_NOT_IMPLEMENTED
}

size_t std::hash<osc::CameraNew>::operator()(osc::CameraNew const&) const
{
    OSC_NOT_IMPLEMENTED
}


// osc::GraphicsBackend

void osc::GraphicsBackend::DrawMesh(Mesh&, Material&, Transform&, CameraNew&, MaterialPropertyBlock const*)
{
    OSC_NOT_IMPLEMENTED
}

void osc::GraphicsBackend::DrawMesh(Mesh&, Material&, glm::mat4 const&, CameraNew&, MaterialPropertyBlock const*)
{
    OSC_NOT_IMPLEMENTED
}

void osc::GraphicsBackend::DrawMesh(Mesh&, glm::vec3 const& pos, glm::quat const& rot, CameraNew&, MaterialPropertyBlock const*)
{
    OSC_NOT_IMPLEMENTED
}

void osc::GraphicsBackend::Blit(Texture2D&)
{
    OSC_NOT_IMPLEMENTED
}

void osc::GraphicsBackend::Blit(Texture2D&, Rect const& srcRect, Rect const& destRect)
{
    OSC_NOT_IMPLEMENTED
}

void osc::GraphicsBackend::Blit(Texture2D&, Texture2D&, Rect const& srcRect, Rect const& destRect)
{
    OSC_NOT_IMPLEMENTED
}
