#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Model.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nonstd/span.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace osc {

    // numeric format of the vert indices
    enum class IndexFormat {
        UInt16,
        UInt32,
    };

    // high-level class for holding/rendering meshes
    //
    // uses copy-on-write semantics to make the mesh have value-like behavior
    class Mesh {
    public:
        Mesh();  // empty mesh with triangle topography
        Mesh(MeshData);
        Mesh(Mesh const&) noexcept;
        Mesh(Mesh&&) noexcept;
        ~Mesh() noexcept;
        Mesh& operator=(Mesh const&) noexcept;
        Mesh& operator=(Mesh&&) noexcept;

        bool operator==(Mesh const&) const noexcept;
        bool operator!=(Mesh const&) const noexcept;
        bool operator<(Mesh const&) const noexcept;
        bool operator>(Mesh const&) const noexcept;
        bool operator<=(Mesh const&) const noexcept;
        bool operator>=(Mesh const&) const noexcept;

        void SetMeshData(MeshData);

        MeshTopography GetTopography() const noexcept;
        GLenum GetTopographyOpenGL() const;
        void SetTopography(MeshTopography);

        nonstd::span<glm::vec3 const> GetVerts() const;
        void SetVerts(nonstd::span<glm::vec3 const>);

        nonstd::span<glm::vec3 const> GetNormals() const;
        void SetNormals(nonstd::span<glm::vec3 const>);

        nonstd::span<glm::vec2 const> GetTexCoords() const;
        void SetTexCoords(nonstd::span<glm::vec2 const>);
        void ScaleTexCoords(float);

        IndexFormat GetIndexFormat() const;
        GLenum GetIndexFormatOpenGL() const;
        void SetIndexFormat(IndexFormat);

        int GetNumIndices() const;
        std::vector<uint32_t> GetIndices() const;  // note: copies them, because IndexFormat may be U16 internally
        void SetIndicesU16(nonstd::span<uint16_t const>);
        void SetIndicesU32(nonstd::span<uint32_t const>);  // note: format trumps this, value will be truncated

        // returns AABB in model-space
        AABB const& GetAABB() const;

        // returns bounding sphere in model-space
        Sphere const& GetBoundingSphere() const;

        // returns !hit if the line doesn't intersect it *or* the topography is not triangular
        RayCollision GetClosestRayTriangleCollisionModelspace(Ray const& modelspaceLine) const;

        // as above, but works in worldspace (requires a model matrix to map the worldspace line into modelspace)
        RayCollision GetRayMeshCollisionInWorldspace(glm::mat4 const& model2world, Ray const& worldspaceLine) const;

        void clear();
        void UploadToGPU();  // must be called from GPU thread

        gl::VertexArray& GetVertexArray();
        void Draw() const;
        void DrawInstanced(size_t n) const;

    public:
        struct Impl;
    private:
        std::unique_ptr<Impl> m_Impl;
    };
}
