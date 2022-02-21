#include "Renderer.hpp"

#include "src/3D/BVH.hpp"
#include "src/Utils/UID.hpp"
#include "src/Utils/DefaultConstructOnCopy.hpp"
#include "src/Assertions.hpp"

#include <nonstd/span.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <vector>


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

// globals
namespace
{
    static constexpr std::array<char const*, 2> const g_MeshTopographyStrings =
    {
        "MeshTopographyNew_Triangles",
        "MeshTopographyNew_Lines",
    };

    static constexpr std::array<char const*, 3> const g_TextureWrapModeStrings =
    {
        "TextureWrapMode_Repeat",
        "TextureWrapMode_Clamp",
        "TextureWrapMode_Mirror",
    };

    static constexpr std::array<char const*, 3> const g_TextureFilterModeStrings =
    {
        "TextureFilterMode_Nearest",
        "TextureFilterMode_Linear",
        "TextureFilterMode_Mipmap"
    };

    static constexpr std::array<char const*, osc::ShaderType_TOTAL> g_ShaderTypeStrings =
    {
        "ShaderType_Float",
        "ShaderType_Int",
        "ShaderType_Matrix",
        "ShaderType_Texture",
        "ShaderType_Vector",
    };

    static constexpr std::array<char const*, 2> g_CameraProjectionStrings =
    {
        "CameraProjection_Perspective",
        "CameraProjection_Orthographic",
    };
}


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

    template<typename T>
    std::string StreamName(T const& v)
    {
        std::stringstream ss;
        ss << v;
        return std::move(ss).str();
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

    std::vector<osc::Rgba32> PackAsRGBA32(nonstd::span<glm::vec4 const> pixels)
    {
        std::vector<osc::Rgba32> rv;
        rv.reserve(pixels.size());
        for (glm::vec4 const& pixel : pixels)
        {
            rv.push_back(osc::Rgba32FromVec4(pixel));
        }
        return rv;
    }

    std::vector<PackedIndex> PackIndexRangeAsU16(size_t n)
    {
        OSC_ASSERT(n <= std::numeric_limits<uint16_t>::max());

        size_t numEls = (n+1)/2;

        std::vector<PackedIndex> rv;
        rv.reserve(numEls);

        for (size_t i = 0, end = n-1; i < end; i += 2)
        {
            PackedIndex& pi = rv.emplace_back();
            pi.u16.a = static_cast<uint16_t>(i);
            pi.u16.b = static_cast<uint16_t>(i+1);
        }

        if (n % 2)
        {
            PackedIndex& pi = rv.emplace_back();
            pi.u16.a = static_cast<uint16_t>(n-1);
            pi.u16.b = {};
        }

        return rv;
    }

    std::vector<PackedIndex> PackIndexRangeAsU32(size_t n)
    {
        std::vector<PackedIndex> rv;
        rv.reserve(n);

        for (size_t i = 0; i < n; ++i)
        {
            PackedIndex& pi = rv.emplace_back();
            pi.u32 = static_cast<uint32_t>(i);
        }

        return rv;
    }

    template<typename T>
    static std::vector<T> Create0ToNIndices(size_t n)
    {
        static_assert(std::is_integral_v<T>);

        OSC_ASSERT(n >= 0);
        OSC_ASSERT(n <= std::numeric_limits<T>::max());

        std::vector<T> rv;
        rv.reserve(n);

        for (T i = 0; i < n; ++i)
        {
            rv.push_back(i);
        }

        return rv;
    }
}


// osc::Mesh::Impl

class osc::Mesh::Impl final {
public:
    Impl() = default;

    Impl(MeshTopographyNew t, nonstd::span<glm::vec3 const> verts) :
        m_Topography{std::move(t)},
        m_Verts(verts.begin(), verts.end()),
        m_IndexFormat{verts.size() <= std::numeric_limits<uint16_t>::max() ? IndexFormat::UInt16 : IndexFormat::UInt32},
        m_NumIndices{static_cast<int>(verts.size())},
        m_IndicesData{m_IndexFormat == IndexFormat::UInt16 ? PackIndexRangeAsU16(verts.size()) : PackIndexRangeAsU32(verts.size())}
    {
        recalculateBounds();
    }

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

        m_GpuBuffersUpToDate = false;
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

        m_GpuBuffersUpToDate = false;
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

        m_GpuBuffersUpToDate = false;
        m_VersionCounter++;
    }

    void scaleTexCoords(float factor)
    {
        for (glm::vec2& tc : m_TexCoords)
        {
            tc *= factor;
        }

        m_GpuBuffersUpToDate = false;
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
        m_GpuBuffersUpToDate = false;
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
        m_TriangleBVH.clear();
        // don't reset version counter (over-hashing is fine, under-hashing is not)
        m_GpuBuffersUpToDate = false;
    }

    void recalculateBounds()
    {
        m_AABB = AABBFromVerts(m_Verts.data(), m_Verts.size());

        if (m_Topography == MeshTopographyNew_Triangles)
        {
            BVH_BuildFromTriangles(m_TriangleBVH, m_Verts.data(), m_Verts.size());
        }
        else
        {
            m_TriangleBVH.clear();
        }
    }

    size_t getHash() const
    {
        size_t idHash = std::hash<UID>{}(*m_ID);
        size_t versionHash = std::hash<int>{}(m_VersionCounter);
        return CombineHashes(idHash, versionHash);
    }

    RayCollision getClosestRayTriangleCollisionModelspace(Line const& modelspaceLine) const
    {
        if (m_Topography != MeshTopographyNew_Triangles)
        {
            return RayCollision{false, 0.0f};
        }

        BVHCollision coll;
        bool collided = BVH_GetClosestRayTriangleCollision(m_TriangleBVH, m_Verts.data(), m_Verts.size(), modelspaceLine, &coll);

        if (collided)
        {
            return RayCollision{true, coll.distance};
        }
        else
        {
            return RayCollision{false, 0.0f};
        }
    }

    RayCollision getClosestRayTriangleCollisionWorldspace(Line const& worldspaceLine, glm::mat4 const& model2world) const
    {
        // do a fast ray-to-AABB collision test
        AABB modelspaceAABB = getBounds();
        AABB worldspaceAABB = AABBApplyXform(modelspaceAABB, model2world);

        RayCollision rayAABBCollision = GetRayCollisionAABB(worldspaceLine, worldspaceAABB);

        if (!rayAABBCollision.hit)
        {
            return rayAABBCollision;  // missed the AABB, so *definitely* missed the mesh
        }

        // it hit the AABB, so it *may* have hit a triangle in the mesh
        //
        // refine the hittest by doing a slower ray-to-triangle test
        glm::mat4 world2model = glm::inverse(model2world);
        Line modelspaceLine = LineApplyXform(worldspaceLine, world2model);

        return getClosestRayTriangleCollisionModelspace(modelspaceLine);
    }

private:
    DefaultConstructOnCopy<UID> m_ID;
    MeshTopographyNew m_Topography = MeshTopographyNew_Triangles;
    std::vector<glm::vec3> m_Verts;
    std::vector<glm::vec3> m_Normals;
    std::vector<glm::vec2> m_TexCoords;
    IndexFormat m_IndexFormat = IndexFormat::UInt16;
    int m_NumIndices = 0;
    std::vector<PackedIndex> m_IndicesData;
    AABB m_AABB = {};
    BVH m_TriangleBVH;
    int m_VersionCounter = 0;
    DefaultConstructOnCopy<bool> m_GpuBuffersUpToDate = false;

    // TODO: GPU data
};


// osc::Texture2D::Impl

class osc::Texture2D::Impl final {
public:
    Impl(int width, int height, nonstd::span<Rgba32 const> pixels) :
        m_Dims{width, height},
        m_PixelData{pixels.begin(), pixels.end()}
    {
        OSC_ASSERT(static_cast<int>(m_PixelData.size()) == width*height);
    }

    Impl(int width, int height, nonstd::span<glm::vec4 const> pixels) :
        m_Dims{width, height},
        m_PixelData{PackAsRGBA32(pixels)}
    {
        OSC_ASSERT(static_cast<int>(m_PixelData.size()) == width*height);
    }

    int getWidth() const
    {
        return m_Dims.x;
    }

    int getHeight() const
    {
        return m_Dims.y;
    }

    float getAspectRatio() const
    {
        return VecAspectRatio(m_Dims);
    }

    TextureWrapMode getWrapMode() const
    {
        return getWrapModeU();
    }

    void setWrapMode(TextureWrapMode wm)
    {
        setWrapModeU(wm);
    }

    TextureWrapMode getWrapModeU() const
    {
        return m_WrapModeU;
    }

    void setWrapModeU(TextureWrapMode wm)
    {
        m_WrapModeU = wm;
        m_VersionCounter++;
    }

    TextureWrapMode getWrapModeV() const
    {
        return m_WrapModeV;
    }

    void setWrapModeV(TextureWrapMode wm)
    {
        m_WrapModeV = wm;
        m_VersionCounter++;
    }

    TextureWrapMode getWrapModeW() const
    {
        return m_WrapModeW;
    }

    void setWrapModeW(TextureWrapMode wm)
    {
        m_WrapModeW = wm;
        m_VersionCounter++;
    }

    TextureFilterMode getFilterMode() const
    {
        return m_FilterMode;
    }

    void setFilterMode(TextureFilterMode fm)
    {
        m_FilterMode = fm;
        m_VersionCounter++;
    }

    size_t getHash() const
    {
        size_t idHash = std::hash<UID>{}(*m_ID);
        size_t versionHash = std::hash<int>{}(m_VersionCounter);
        return CombineHashes(idHash, versionHash);
    }

private:
    DefaultConstructOnCopy<UID> m_ID;
    glm::ivec2 m_Dims;
    std::vector<osc::Rgba32> m_PixelData;
    TextureWrapMode m_WrapModeU = TextureWrapMode_Repeat;
    TextureWrapMode m_WrapModeV = TextureWrapMode_Repeat;
    TextureWrapMode m_WrapModeW = TextureWrapMode_Repeat;
    TextureFilterMode m_FilterMode = TextureFilterMode_Linear;
    int m_VersionCounter = 0;
    DefaultConstructOnCopy<bool> m_GpuBuffersUpToDate = false;

    // TODO: GPU data
};


// osc::Shader::Impl

class osc::Shader::Impl final {
public:
    Impl(char const*)
    {
        OSC_ASSERT(false && "not yet implemented");
    }

    std::string const& getName() const;

    int findPropertyIndex(std::string_view propertyName) const;

    int findPropertyIndex(size_t propertyNameID) const;

    int getPropertyCount() const;

    std::string const& getPropertyName(int propertyIndex) const;

    size_t getPropertyNameID(int propertyIndex) const;

    ShaderType getPropertyType(int propertyIndex) const;

    std::ostream& operator<<(std::ostream o) const;

    std::string to_string() const;

    size_t getHash() const;

private:
    // TODO: needs gl::Program
    // TODO: needs definition of where the attrs are
};


// osc::Material::Impl

class osc::Material::Impl final {
public:
    Impl(Shader shader) : m_Shader{std::move(shader)}
    {
    }

    Shader const& getShader() const;

    bool hasProperty(std::string_view propertyName) const;
    bool hasProperty(size_t propertyNameID) const;

    float getFloat(std::string_view propertyName) const;
    float getFloat(size_t propertyNameID) const;
    void setFloat(std::string_view propertyName, float);
    void setFloat(size_t propertyNameID, float);

    int getInt(std::string_view propertyName) const;
    int getInt(size_t propertyNameID) const;
    void setInt(std::string_view propertyName, int);
    void setInt(size_t propertyNameID, int);

    Texture2D const& getTexture(std::string_view propertyName) const;
    Texture2D const& getTexture(size_t propertyNameID) const;
    void setTexture(std::string_view propertyName, Texture2D);
    void setTexture(size_t propertyNameID, Texture2D);

    glm::vec4 const& getVector(std::string_view propertyName) const;
    glm::vec4 const& getVector(size_t propertyNameID) const;
    void setVector(std::string_view propertyName, glm::vec4 const&);
    void setVector(size_t propertyNameID, glm::vec4 const&);

    glm::mat4 const& getMatrix(std::string_view propertyName) const;
    glm::mat4 const& getMatrix(size_t propertyNameID) const;
    void setMatrix(std::string_view propertyName, glm::mat4 const&);
    void setMatrix(size_t propertyNameID, glm::mat4 const&);

    size_t getHash() const;

private:
    Shader m_Shader;

};


// osc::MaterialPropertyBlock::Impl

class osc::MaterialPropertyBlock::Impl final {
public:
    void clear();
    bool isEmpty() const;

    bool hasProperty(std::string_view propertyName) const;
    bool hasProperty(size_t propertyNameID) const;

    float getFloat(std::string_view propertyName) const;
    float getFloat(size_t propertyNameID) const;
    void setFloat(std::string_view propertyName, float);
    void setFloat(size_t propertyNameID, float);

    int getInt(std::string_view propertyName) const;
    int getInt(size_t propertyNameID) const;
    void setInt(std::string_view propertyName, int);
    void setInt(size_t propertyNameID, int);

    Texture2D const& getTexture(std::string_view propertyName) const;
    Texture2D const& getTexture(size_t propertyNameID) const;
    void setTexture(std::string_view propertyName, Texture2D);
    void setTexture(size_t propertyNameID, Texture2D);

    glm::vec4 const& getVector(std::string_view propertyName) const;
    glm::vec4 const& getVector(size_t propertyNameID) const;
    void setVector(std::string_view propertyName, glm::vec4 const&);
    void setVector(size_t propertyNameID, glm::vec4 const&);

    glm::mat4 const& getMatrix(std::string_view propertyName) const;
    glm::mat4 const& getMatrix(size_t propertyNameID) const;
    void setMatrix(std::string_view propertyName, glm::mat4 const&);
    void setMatrix(size_t propertyNameID, glm::mat4 const&);

    size_t getHash() const;
private:
    // TODO

};


// osc::CameraNew::Impl

class osc::CameraNew::Impl final {
public:
    Impl(Texture2D);

    glm::vec4 getBackgroundColor() const;
    void setBackgroundColor(glm::vec4 const&);

    CameraProjection getCameraProjection() const;
    void setCameraProjection(CameraProjection);

    // only used if orthographic
    //
    // e.g. https://docs.unity3d.com/ScriptReference/Camera-orthographicSize.html
    float getOrthographicSize() const;
    void setOrthographicSize(float);

    // only used if perspective
    float getCameraFOV() const;
    void setCameraFOV(float);

    float getNearClippingPlane() const;
    void setNearClippingPlane(float);

    float getFarClippingPlane() const;
    void setFarClippingPlane(float);

    std::optional<Texture2D> getTexture() const;
    void setTexture(Texture2D);
    void setTexture();  // resets to drawing to screen

    int getPixelWidth() const;
    int getPixelHeight() const;
    float getAspectRatio() const;

    std::optional<Rect> getScissorRect() const;
    void setScissorRect(Rect const&);  // rect is in pixel space?
    void setScissorRect();  // resets to having no scissor

    glm::vec3 getPosition() const;
    void setPosition(glm::vec3 const&);

    glm::vec3 getDirection() const;
    void setDirection(glm::vec3 const&);

    glm::mat4 const& getCameraToWorldMatrix() const;

    // flushes any rendering commands that were queued against this camera
    //
    // after this call completes, callers can then use the output texture/screen
    void render();

    size_t getHash() const;

private:
    // TODO
};



// PUBLIC API


// osc::MeshTopographyNew

std::ostream& osc::operator<<(std::ostream& o, MeshTopographyNew t)
{
    return o << g_MeshTopographyStrings.at(static_cast<size_t>(t));
}

std::string osc::to_string(MeshTopographyNew t)
{
    return std::string{g_MeshTopographyStrings.at(static_cast<size_t>(t))};
}


// osc::Mesh impl

osc::Mesh::Mesh() : m_Impl{std::make_shared<Impl>()} {}

osc::Mesh::Mesh(MeshTopographyNew t, nonstd::span<glm::vec3 const> verts) :
    m_Impl{std::make_shared<Impl>(std::move(t), std::move(verts))}
{
}

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
    m_Impl->setTopography(std::move(mt));
}

nonstd::span<glm::vec3 const> osc::Mesh::getVerts() const
{
    return m_Impl->getVerts();
}

void osc::Mesh::setVerts(nonstd::span<const glm::vec3> vs)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVerts(std::move(vs));
}

nonstd::span<glm::vec3 const> osc::Mesh::getNormals() const
{
    return m_Impl->getNormals();
}

void osc::Mesh::setNormals(nonstd::span<const glm::vec3> vs)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setNormals(std::move(vs));
}

nonstd::span<glm::vec2 const> osc::Mesh::getTexCoords() const
{
    return m_Impl->getTexCoords();
}

void osc::Mesh::setTexCoords(nonstd::span<const glm::vec2> tc)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexCoords(std::move(tc));
}

void osc::Mesh::scaleTexCoords(float factor)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->scaleTexCoords(std::move(factor));
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
    m_Impl->setIndices(std::move(vs));
}

void osc::Mesh::setIndices(nonstd::span<const uint32_t> vs)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setIndices(std::move(vs));
}

osc::AABB const& osc::Mesh::getBounds() const
{
    return m_Impl->getBounds();
}

osc::RayCollision osc::Mesh::getClosestRayTriangleCollisionModelspace(Line const& modelspaceLine) const
{
    return m_Impl->getClosestRayTriangleCollisionModelspace(modelspaceLine);
}

osc::RayCollision osc::Mesh::getClosestRayTriangleCollisionWorldspace(Line const& worldspaceLine, glm::mat4 const& model2world) const
{
    return m_Impl->getClosestRayTriangleCollisionWorldspace(worldspaceLine, model2world);
}

void osc::Mesh::clear()
{
    DoCopyOnWrite(m_Impl);
    m_Impl->clear();
}

std::ostream& osc::operator<<(std::ostream& o, Mesh const& m)
{
    return o << "Mesh(nverts = " << m.m_Impl->getVerts().size() << ", nindices = " << m.m_Impl->getIndices().size() << ')';
}

std::string osc::to_string(Mesh const& m)
{
    return StreamName(m);
}

size_t std::hash<osc::Mesh>::operator()(osc::Mesh const& mesh) const
{
    return mesh.m_Impl->getHash();
}


// osc::TextureWrapMode

std::ostream& osc::operator<<(std::ostream& o, osc::TextureWrapMode wm)
{
    return o << g_TextureWrapModeStrings.at(static_cast<size_t>(wm));
}

std::string osc::to_string(TextureWrapMode wm)
{
    return std::string{g_TextureWrapModeStrings.at(static_cast<size_t>(wm))};
}


// osc::TextureFilterMode

std::ostream& osc::operator<<(std::ostream& o, osc::TextureFilterMode fm)
{
    return o << g_TextureFilterModeStrings.at(static_cast<size_t>(fm));
}

std::string osc::to_string(TextureFilterMode fm)
{
    return std::string{g_TextureFilterModeStrings.at(static_cast<size_t>(fm))};
}


// osc::Texture2D

osc::Texture2D::Texture2D(int width, int height, nonstd::span<Rgba32 const> pixels) :
    m_Impl{std::make_shared<Impl>(std::move(width), std::move(height), std::move(pixels))}
{
}

osc::Texture2D::Texture2D(int width, int height, nonstd::span<glm::vec4 const> pixels) :
    m_Impl{std::make_shared<Impl>(std::move(width), std::move(height), std::move(pixels))}
{
}

osc::Texture2D::Texture2D(Texture2D const&) = default;

osc::Texture2D::Texture2D(Texture2D&&) noexcept = default;

osc::Texture2D::~Texture2D() noexcept = default;

osc::Texture2D& osc::Texture2D::operator=(Texture2D const&) = default;

osc::Texture2D& osc::Texture2D::operator=(Texture2D&&) noexcept = default;

bool osc::Texture2D::operator==(Texture2D const& o) const
{
    return m_Impl == o.m_Impl;
}

bool osc::Texture2D::operator!=(Texture2D const& o) const
{
    return m_Impl != o.m_Impl;
}

bool osc::Texture2D::operator<(Texture2D const& o) const
{
    return m_Impl < o.m_Impl;
}

int osc::Texture2D::getWidth() const
{
    return m_Impl->getWidth();
}

int osc::Texture2D::getHeight() const
{
    return m_Impl->getHeight();
}

float osc::Texture2D::getAspectRatio() const
{
    return m_Impl->getAspectRatio();
}

osc::TextureWrapMode osc::Texture2D::getWrapMode() const
{
    return m_Impl->getWrapMode();
}

void osc::Texture2D::setWrapMode(TextureWrapMode wm)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setWrapMode(std::move(wm));
}

osc::TextureWrapMode osc::Texture2D::getWrapModeU() const
{
    return m_Impl->getWrapModeU();
}

void osc::Texture2D::setWrapModeU(TextureWrapMode wm)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setWrapModeU(std::move(wm));
}

osc::TextureWrapMode osc::Texture2D::getWrapModeV() const
{
    return m_Impl->getWrapModeV();
}

void osc::Texture2D::setWrapModeV(TextureWrapMode wm)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setWrapModeV(std::move(wm));
}

osc::TextureWrapMode osc::Texture2D::getWrapModeW() const
{
    return m_Impl->getWrapModeW();
}

void osc::Texture2D::setWrapModeW(TextureWrapMode wm)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setWrapModeW(std::move(wm));
}

osc::TextureFilterMode osc::Texture2D::getFilterMode() const
{
    return m_Impl->getFilterMode();
}

void osc::Texture2D::setFilterMode(TextureFilterMode fm)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setFilterMode(std::move(fm));
}

std::ostream& osc::operator<<(std::ostream& o, Texture2D const& t)
{
    return o << "Texture2D(width = " << t.getWidth() << ", height = " << t.getHeight() << ')';
}

std::string osc::to_string(Texture2D const& t)
{
    return StreamName(t);
}

size_t std::hash<osc::Texture2D>::operator()(osc::Texture2D const& t) const
{
    return t.m_Impl->getHash();
}


// osc::ShaderType

std::ostream& osc::operator<<(std::ostream& o, ShaderType st)
{
    return o << g_ShaderTypeStrings.at(static_cast<size_t>(st));
}

std::string osc::to_string(ShaderType st)
{
    return std::string{g_ShaderTypeStrings.at(static_cast<size_t>(st))};
}


// shader IDs

size_t osc::ConvertPropertyNameToNameID(std::string_view sv)
{
    static std::string g_Buffer;
    static std::mutex g_BufferMutex;

    std::lock_guard l{g_BufferMutex};
    g_Buffer = std::move(sv);

    return std::hash<std::string>{}(g_Buffer);
}


// osc::Shader

osc::Shader osc::Shader::compile(char const* src)
{
    return Shader{src};
}

osc::Shader::Shader(char const* src) : m_Impl{std::make_shared<Impl>(src)}
{
}

osc::Shader::Shader(Shader const&) = default;

osc::Shader::Shader(Shader&&) noexcept = default;

osc::Shader::~Shader() noexcept = default;

osc::Shader& osc::Shader::operator=(Shader const&) = default;

osc::Shader& osc::Shader::operator=(Shader&&) noexcept = default;

bool osc::Shader::operator==(Shader const& o) const
{
    return m_Impl == o.m_Impl;
}

bool osc::Shader::operator!=(Shader const& o) const
{
    return m_Impl != o.m_Impl;
}

bool osc::Shader::operator<(Shader const& o) const
{
    return m_Impl < o.m_Impl;
}

std::string const& osc::Shader::getName() const
{
    return m_Impl->getName();
}

int osc::Shader::findPropertyIndex(std::string_view propertyName) const
{
    return m_Impl->findPropertyIndex(std::move(propertyName));
}

int osc::Shader::findPropertyIndex(size_t propertyNameID) const
{
    return m_Impl->findPropertyIndex(std::move(propertyNameID));
}

int osc::Shader::getPropertyCount() const
{
    return m_Impl->getPropertyCount();
}

std::string const& osc::Shader::getPropertyName(int propertyIndex) const
{
    return m_Impl->getPropertyName(std::move(propertyIndex));
}

size_t osc::Shader::getPropertyNameID(int propertyIndex) const
{
    return m_Impl->getPropertyNameID(std::move(propertyIndex));
}

osc::ShaderType osc::Shader::getPropertyType(int propertyIndex) const
{
    return m_Impl->getPropertyType(std::move(propertyIndex));
}

std::ostream& osc::operator<<(std::ostream& o, Shader const& s)
{
    return o << "Shader(name = " << s.getName() << ')';
}

std::string osc::to_string(Shader const& s)
{
    return StreamName(s);
}

size_t std::hash<osc::Shader>::operator()(osc::Shader const& s) const
{
    return s.m_Impl->getHash();
}


// osc::Material

osc::Material::Material(Shader shader) : m_Impl{std::make_shared<Impl>(std::move(shader))}
{
}

osc::Material::Material(Material const&) = default;

osc::Material::Material(Material&&) noexcept = default;

osc::Material::~Material() noexcept = default;

osc::Material& osc::Material::operator=(Material const&) = default;

osc::Material& osc::Material::operator=(Material&&) noexcept = default;

bool osc::Material::operator==(Material const& o) const
{
    return m_Impl == o.m_Impl;
}

bool osc::Material::operator!=(Material const& o) const
{
    return m_Impl != o.m_Impl;
}

bool osc::Material::operator<(Material const& o) const
{
    return m_Impl < o.m_Impl;
}

osc::Shader const& osc::Material::getShader() const
{
    return m_Impl->getShader();
}

bool osc::Material::hasProperty(std::string_view propertyName) const
{
    return m_Impl->hasProperty(std::move(propertyName));
}

bool osc::Material::hasProperty(size_t propertyNameID) const
{
    return m_Impl->hasProperty(std::move(propertyNameID));
}

glm::vec4 const& osc::Material::getColor() const
{
    return m_Impl->getVector("Color");
}

void osc::Material::setColor(glm::vec4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVector("Color", v);
}

float osc::Material::getFloat(std::string_view propertyName) const
{
    return m_Impl->getFloat(std::move(propertyName));
}

float osc::Material::getFloat(size_t propertyNameID) const
{
    return m_Impl->getFloat(std::move(propertyNameID));
}

void osc::Material::setFloat(std::string_view propertyName, float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setFloat(std::move(propertyName), std::move(v));
}

void osc::Material::setFloat(size_t propertyNameID, float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setFloat(std::move(propertyNameID), std::move(v));
}

int osc::Material::getInt(std::string_view propertyName) const
{
    return m_Impl->getInt(std::move(propertyName));
}

int osc::Material::getInt(size_t propertyNameID) const
{
    return m_Impl->getInt(std::move(propertyNameID));
}

void osc::Material::setInt(std::string_view propertyName, int v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setInt(std::move(propertyName), std::move(v));
}

void osc::Material::setInt(size_t propertyNameID, int v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setInt(std::move(propertyNameID), std::move(v));
}

osc::Texture2D const& osc::Material::getTexture(std::string_view propertyName) const
{
    return m_Impl->getTexture(std::move(propertyName));
}

osc::Texture2D const& osc::Material::getTexture(size_t propertyNameID) const
{
    return m_Impl->getTexture(std::move(propertyNameID));
}

void osc::Material::setTexture(std::string_view propertyName, Texture2D t)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexture(std::move(propertyName), std::move(t));
}

void osc::Material::setTexture(size_t propertyNameID, Texture2D t)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexture(std::move(propertyNameID), std::move(t));
}

glm::vec4 const& osc::Material::getVector(std::string_view propertyName) const
{
    return m_Impl->getVector(std::move(propertyName));
}

glm::vec4 const& osc::Material::getVector(size_t propertyNameID) const
{
    return m_Impl->getVector(std::move(propertyNameID));
}

void osc::Material::setVector(std::string_view propertyName, glm::vec4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVector(std::move(propertyName), v);
}

void osc::Material::setVector(size_t propertyNameID, glm::vec4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVector(std::move(propertyNameID), v);
}

glm::mat4 const& osc::Material::getMatrix(std::string_view propertyName) const
{
    return m_Impl->getMatrix(std::move(propertyName));
}

glm::mat4 const& osc::Material::getMatrix(size_t propertyNameID) const
{
    return m_Impl->getMatrix(std::move(propertyNameID));
}

void osc::Material::setMatrix(std::string_view propertyName, glm::mat4 const& m)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setMatrix(std::move(propertyName), m);
}

void osc::Material::setMatrix(size_t propertyNameID, glm::mat4 const& m)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setMatrix(std::move(propertyNameID), m);
}

std::ostream& osc::operator<<(std::ostream& o, Material const&)
{
    return o << "Material()";
}

std::string osc::to_string(Material const& mat)
{
    return StreamName(mat);
}

size_t std::hash<osc::Material>::operator()(osc::Material const& mat) const
{
    return mat.m_Impl->getHash();
}


// osc::MaterialPropertyBlock

osc::MaterialPropertyBlock::MaterialPropertyBlock() : m_Impl{std::make_shared<Impl>()}
{
}

osc::MaterialPropertyBlock::MaterialPropertyBlock(MaterialPropertyBlock const&) = default;

osc::MaterialPropertyBlock::MaterialPropertyBlock(MaterialPropertyBlock&&) noexcept = default;

osc::MaterialPropertyBlock::~MaterialPropertyBlock() noexcept = default;

osc::MaterialPropertyBlock& osc::MaterialPropertyBlock::operator=(MaterialPropertyBlock const&) = default;

osc::MaterialPropertyBlock& osc::MaterialPropertyBlock::operator=(MaterialPropertyBlock&&) noexcept = default;

bool osc::MaterialPropertyBlock::operator==(MaterialPropertyBlock const& o) const
{
    return m_Impl == o.m_Impl;
}

bool osc::MaterialPropertyBlock::operator!=(MaterialPropertyBlock const& o) const
{
    return m_Impl != o.m_Impl;
}

bool osc::MaterialPropertyBlock::operator<(MaterialPropertyBlock const& o) const
{
    return m_Impl < o.m_Impl;
}

void osc::MaterialPropertyBlock::clear()
{
    DoCopyOnWrite(m_Impl);
    m_Impl->clear();
}

bool osc::MaterialPropertyBlock::isEmpty() const
{
    return m_Impl->isEmpty();
}

bool osc::MaterialPropertyBlock::hasProperty(std::string_view propertyName) const
{
    return m_Impl->hasProperty(std::move(propertyName));
}

bool osc::MaterialPropertyBlock::hasProperty(size_t propertyNameID) const
{
    return m_Impl->hasProperty(std::move(propertyNameID));
}

glm::vec4 const& osc::MaterialPropertyBlock::getColor() const
{
    return m_Impl->getVector("Color");
}

void osc::MaterialPropertyBlock::setColor(glm::vec4 const& c)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVector("Color", c);
}

float osc::MaterialPropertyBlock::getFloat(std::string_view propertyName) const
{
    return m_Impl->getFloat(std::move(propertyName));
}

float osc::MaterialPropertyBlock::getFloat(size_t propertyNameID) const
{
    return m_Impl->getFloat(std::move(propertyNameID));
}

void osc::MaterialPropertyBlock::setFloat(std::string_view propertyName, float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setFloat(std::move(propertyName), std::move(v));
}

void osc::MaterialPropertyBlock::setFloat(size_t propertyNameID, float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setFloat(std::move(propertyNameID), std::move(v));
}

int osc::MaterialPropertyBlock::getInt(std::string_view propertyName) const
{
    return m_Impl->getInt(std::move(propertyName));
}

int osc::MaterialPropertyBlock::getInt(size_t propertyNameID) const
{
    return m_Impl->getInt(std::move(propertyNameID));
}

void osc::MaterialPropertyBlock::setInt(std::string_view propertyName, int v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setInt(std::move(propertyName), std::move(v));
}

void osc::MaterialPropertyBlock::setInt(size_t propertyNameID, int v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setInt(std::move(propertyNameID), std::move(v));
}

osc::Texture2D const& osc::MaterialPropertyBlock::getTexture(std::string_view propertyName) const
{
    return m_Impl->getTexture(std::move(propertyName));
}

osc::Texture2D const& osc::MaterialPropertyBlock::getTexture(size_t propertyNameID) const
{
    return m_Impl->getTexture(std::move(propertyNameID));
}

void osc::MaterialPropertyBlock::setTexture(std::string_view propertyName, Texture2D t)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexture(std::move(propertyName), std::move(t));
}

void osc::MaterialPropertyBlock::setTexture(size_t propertyNameID, Texture2D t)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexture(std::move(propertyNameID), std::move(t));
}

glm::vec4 const& osc::MaterialPropertyBlock::getVector(std::string_view propertyName) const
{
    return m_Impl->getVector(std::move(propertyName));
}

glm::vec4 const& osc::MaterialPropertyBlock::getVector(size_t propertyNameID) const
{
    return m_Impl->getVector(std::move(propertyNameID));
}

void osc::MaterialPropertyBlock::setVector(std::string_view propertyName, glm::vec4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVector(propertyName, v);
}

void osc::MaterialPropertyBlock::setVector(size_t propertyNameID, glm::vec4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setVector(std::move(propertyNameID), v);
}

glm::mat4 const& osc::MaterialPropertyBlock::getMatrix(std::string_view propertyName) const
{
    return m_Impl->getMatrix(std::move(propertyName));
}

glm::mat4 const& osc::MaterialPropertyBlock::getMatrix(size_t propertyNameID) const
{
    return m_Impl->getMatrix(std::move(propertyNameID));
}

void osc::MaterialPropertyBlock::setMatrix(std::string_view propertyName, glm::mat4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setMatrix(std::move(propertyName), v);
}

void osc::MaterialPropertyBlock::setMatrix(size_t propertyNameID, glm::mat4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setMatrix(std::move(propertyNameID), v);
}

std::ostream& osc::operator<<(std::ostream& o, MaterialPropertyBlock const&)
{
    return o << "MaterialPropertyBlock()";
}

std::string osc::to_string(MaterialPropertyBlock const& m)
{
    return StreamName(m);
}

size_t std::hash<osc::MaterialPropertyBlock>::operator()(osc::MaterialPropertyBlock const& m) const
{
    return m.m_Impl->getHash();
}


// osc::CameraProjection

std::ostream& osc::operator<<(std::ostream& o, CameraProjection p)
{
    return o << g_CameraProjectionStrings.at(static_cast<size_t>(p));
}

std::string osc::to_string(CameraProjection p)
{
    return std::string{g_CameraProjectionStrings.at(static_cast<size_t>(p))};
}


// osc::CameraNew

osc::CameraNew::CameraNew(Texture2D t) :
    m_Impl{std::make_shared<Impl>(std::move(t))}
{
}

osc::CameraNew::CameraNew(CameraNew const&) = default;

osc::CameraNew::CameraNew(CameraNew&&) noexcept = default;

osc::CameraNew::~CameraNew() noexcept = default;

osc::CameraNew& osc::CameraNew::operator=(CameraNew const&) = default;

osc::CameraNew& osc::CameraNew::operator=(CameraNew&&) noexcept = default;

bool osc::CameraNew::operator==(CameraNew const& o) const
{
    return m_Impl == o.m_Impl;
}

bool osc::CameraNew::operator!=(CameraNew const& o) const
{
    return m_Impl != o.m_Impl;
}

bool osc::CameraNew::operator<(CameraNew const& o) const
{
    return m_Impl < o.m_Impl;
}

glm::vec4 osc::CameraNew::getBackgroundColor() const
{
    return m_Impl->getBackgroundColor();
}

void osc::CameraNew::setBackgroundColor(glm::vec4 const& v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setBackgroundColor(v);
}

osc::CameraProjection osc::CameraNew::getCameraProjection() const
{
    return m_Impl->getCameraProjection();
}

void osc::CameraNew::setCameraProjection(CameraProjection p)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setCameraProjection(std::move(p));
}

float osc::CameraNew::getOrthographicSize() const
{
    return m_Impl->getOrthographicSize();
}

void osc::CameraNew::setOrthographicSize(float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setOrthographicSize(std::move(v));
}

float osc::CameraNew::getCameraFOV() const
{
    return m_Impl->getCameraFOV();
}

void osc::CameraNew::setCameraFOV(float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setCameraFOV(std::move(v));
}

float osc::CameraNew::getNearClippingPlane() const
{
    return m_Impl->getNearClippingPlane();
}

void osc::CameraNew::setNearClippingPlane(float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setNearClippingPlane(std::move(v));
}

float osc::CameraNew::getFarClippingPlane() const
{
    return m_Impl->getFarClippingPlane();
}

void osc::CameraNew::setFarClippingPlane(float v)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setFarClippingPlane(std::move(v));
}

std::optional<osc::Texture2D> osc::CameraNew::getTexture() const
{
    return m_Impl->getTexture();
}

void osc::CameraNew::setTexture(Texture2D t)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexture(std::move(t));
}

void osc::CameraNew::setTexture()
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setTexture();
}

int osc::CameraNew::getPixelWidth() const
{
    return m_Impl->getPixelWidth();
}

int osc::CameraNew::getPixelHeight() const
{
    return m_Impl->getPixelHeight();
}

float osc::CameraNew::getAspectRatio() const
{
    return m_Impl->getAspectRatio();
}

std::optional<osc::Rect> osc::CameraNew::getScissorRect() const
{
    return m_Impl->getScissorRect();
}

void osc::CameraNew::setScissorRect(Rect const& r)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setScissorRect(r);
}

void osc::CameraNew::setScissorRect()
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setScissorRect();
}

glm::vec3 osc::CameraNew::getPosition() const
{
    return m_Impl->getPosition();
}

void osc::CameraNew::setPosition(glm::vec3 const& p)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setPosition(p);
}

glm::vec3 osc::CameraNew::getDirection() const
{
    return m_Impl->getDirection();
}

void osc::CameraNew::setDirection(glm::vec3 const& d)
{
    DoCopyOnWrite(m_Impl);
    m_Impl->setDirection(d);
}

glm::mat4 const& osc::CameraNew::getCameraToWorldMatrix() const
{
    return m_Impl->getCameraToWorldMatrix();
}

void osc::CameraNew::render()
{
    DoCopyOnWrite(m_Impl);
    m_Impl->render();
}

std::ostream& osc::operator<<(std::ostream& o, CameraNew const&)
{
    return o << "CameraNew()";
}

std::string osc::to_string(CameraNew const& c)
{
    return StreamName(c);
}

size_t std::hash<osc::CameraNew>::operator()(osc::CameraNew const& c) const
{
    return c.m_Impl->getHash();
}


// osc::GraphicsBackend

void osc::Graphics::DrawMesh(Mesh&, glm::vec3 const& pos, CameraNew&, MaterialPropertyBlock const*)
{
    // - copy the data onto the camera's command buffer list
    // - deal with any batch flushing etc.
    throw std::runtime_error{"nyi"};
}
