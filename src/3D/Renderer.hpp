#pragma once

#include "src/3D/Model.hpp"

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/quaternion.hpp>
#include <nonstd/span.hpp>

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace osc
{
    enum MeshTopographyNew {
        MeshTopographyNew_Triangles,
        MeshTopographyNew_Lines,
    };

    std::ostream& operator<<(std::ostream&, MeshTopographyNew);
    std::string to_string(MeshTopographyNew);

    class Mesh final {
    public:
        Mesh();
        Mesh(Mesh const&);
        Mesh(Mesh&&) noexcept;
        ~Mesh() noexcept;

        Mesh& operator=(Mesh const&);
        Mesh& operator=(Mesh&&) noexcept;

        bool operator==(Mesh const&) const;
        bool operator!=(Mesh const&) const;
        bool operator<(Mesh const&) const;

        MeshTopographyNew getTopography() const;
        void setTopography(MeshTopographyNew);

        nonstd::span<glm::vec3 const> getVerts() const;
        void setVerts(nonstd::span<glm::vec3 const>);

        nonstd::span<glm::vec3 const> getNormals() const;
        void setNormals(nonstd::span<glm::vec3 const>);

        nonstd::span<glm::vec2 const> getTexCoords() const;
        void setTexCoords(nonstd::span<glm::vec2 const>);
        void scaleTexCoords(float);

        int getNumIndices() const;
        std::vector<uint32_t> getIndices() const;  // careful: copies
        void setIndices(nonstd::span<uint16_t const>);
        void setIndices(nonstd::span<uint32_t const>);

        AABB const& getBounds() const;  // local-space
        RayCollision getClosestRayTriangleCollisionModelspace(Line const& modelspaceLine) const;
        RayCollision getClosestRayTriangleCollisionWorldspace(Line const& worldspaceLine, glm::mat4 const& model2world) const;

        void clear();

        class Impl;
    private:
        friend class GraphicsBackend;
        friend struct std::hash<Mesh>;
        friend std::ostream& operator<<(std::ostream&, Mesh const&);
        friend std::string to_string(Mesh const&);

        std::shared_ptr<Impl> m_Impl;
    };

    std::ostream& operator<<(std::ostream&, Mesh const&);
    std::string to_string(Mesh const&);
}

namespace std
{
    template<>
    struct hash<osc::Mesh> {
        size_t operator()(osc::Mesh const&) const;
    };
}

namespace osc
{
    enum TextureWrapMode {
        TextureWrapMode_Repeat,
        TextureWrapMode_Clamp,
        TextureWrapMode_Mirror,
    };

    std::ostream& operator<<(std::ostream&, TextureWrapMode);
    std::string to_string(TextureWrapMode);

    enum TextureFilterMode {
        TextureFilterMode_Nearest,
        TextureFilterMode_Linear,
        TextureFilterMode_Mipmap
    };

    std::ostream& operator<<(std::ostream&, TextureFilterMode);
    std::string to_string(TextureFilterMode);

    // a handle to a 2D texture that can be rendered by the graphics backend
    class Texture2D final {
    public:
        // RGBA32, SRGB
        Texture2D(int width, int height, nonstd::span<Rgba32 const>);
        Texture2D(int width, int height, nonstd::span<glm::vec4 const>);
        Texture2D(Texture2D const&);
        Texture2D(Texture2D&&) noexcept;
        ~Texture2D() noexcept;

        Texture2D& operator=(Texture2D const&);
        Texture2D& operator=(Texture2D&&) noexcept;

        bool operator==(Texture2D const&) const;
        bool operator!=(Texture2D const&) const;
        bool operator<(Texture2D const&) const;

        int getWidth() const;
        int getHeight() const;
        float getAspectRatio() const;

        TextureWrapMode getWrapMode() const;  // same as getWrapModeU
        void setWrapMode(TextureWrapMode);
        TextureWrapMode getWrapModeU() const;
        void setWrapModeU(TextureWrapMode);
        TextureWrapMode getWrapModeV() const;
        void setWrapModeV(TextureWrapMode);
        TextureWrapMode getWrapModeW() const;
        void setWrapModeW(TextureWrapMode);

        TextureFilterMode getFilterMode() const;
        void setFilterMode(TextureFilterMode);

        class Impl;
    private:
        friend class GraphicsBackend;
        friend struct std::hash<Texture2D>;
        friend std::ostream& operator<<(std::ostream&, Texture2D const&);
        friend std::string to_string(Texture2D const&);

        std::shared_ptr<Impl> m_Impl;
    };

    std::ostream& operator<<(std::ostream&, Texture2D const&);
    std::string to_string(Texture2D const&);
}

namespace std
{
    template<>
    struct hash<osc::Texture2D> {
        size_t operator()(osc::Texture2D const&) const;
    };
}

namespace osc
{
    // data type of a property in a shader (e.g. vec3)
    enum ShaderType {
        ShaderType_Float = 0,
        ShaderType_Int,
        ShaderType_Matrix,
        ShaderType_Texture,
        ShaderType_Vector,
        ShaderType_TOTAL,
    };

    std::ostream& operator<<(std::ostream&, ShaderType);
    std::string to_string(ShaderType);

    // returns a globally unique lookup ID for a shader property name
    //
    // This ID is guaranteed to not change during the application lifetime. It can
    // be used to accelerate runtime property lookups. However, don't save it anywhere
    // because the underlying algorithm may change between versions/installations of
    // OSC
    size_t ConvertPropertyNameToNameID(std::string_view propertyName);

    // a handle to a shader
    class Shader final {
    public:
        static Shader compile(char const* src);
        explicit Shader(char const* src);  // throws on compile error
        Shader(Shader const&);
        Shader(Shader&&) noexcept;
        ~Shader() noexcept;

        Shader& operator=(Shader const&);
        Shader& operator=(Shader&&) noexcept;

        bool operator==(Shader const&) const;
        bool operator!=(Shader const&) const;
        bool operator<(Shader const&) const;

        std::string const& getName() const;

        int findPropertyIndex(std::string_view propertyName) const;
        int findPropertyIndex(size_t propertyNameID) const;

        int getPropertyCount() const;
        std::string const& getPropertyName(int propertyIndex) const;
        size_t getPropertyNameID(int propertyIndex) const;
        ShaderType getPropertyType(int propertyIndex) const;

        class Impl;
    private:
        friend class GraphicsBackend;
        friend struct std::hash<Shader>;
        friend std::ostream& operator<<(std::ostream&, Shader const&);
        friend std::string to_string(Shader const&);

        std::shared_ptr<Impl> m_Impl;
    };

    std::ostream& operator<<(std::ostream&, Shader const&);
    std::string to_string(Shader const&);
}

namespace std
{
    template<>
    struct hash<osc::Shader> {
        size_t operator()(osc::Shader const&) const;
    };
}

namespace osc
{
    // a material is a shader + the shader's property values (state)
    class Material final {
    public:
        explicit Material(Shader);
        Material(Material const&);
        Material(Material&&) noexcept;
        ~Material() noexcept;

        Material& operator=(Material const&);
        Material& operator=(Material&&) noexcept;

        bool operator==(Material const&) const;
        bool operator!=(Material const&) const;
        bool operator<(Material const&) const;

        Shader const& getShader() const;

        bool hasProperty(std::string_view propertyName) const;
        bool hasProperty(size_t propertyNameID) const;

        // equivalent to `setVector("Color", ...) etc.
        glm::vec4 const& getColor() const;
        void setColor(glm::vec4 const&);

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

        class Impl;
    private:
        friend class GraphicsBackend;
        friend struct std::hash<Material>;
        friend std::ostream& operator<<(std::ostream&, Material const&);
        friend std::string to_string(Material const&);

        std::shared_ptr<Impl> m_Impl;
    };

    std::ostream& operator<<(std::ostream&, Material const&);
    std::string to_string(Material const&);
}

namespace std
{
    template<>
    struct hash<osc::Material> {
        size_t operator()(osc::Material const&) const;
    };
}

namespace osc
{
    // a "block" of properties. Used to "override" properties of a material on a
    // per-instance basis
    //
    // the reason this is useful is because the graphics backend may optimize drawing
    // meshes that have the same material (e.g. via instanced rendering)
    class MaterialPropertyBlock final {
    public:
        MaterialPropertyBlock();
        MaterialPropertyBlock(MaterialPropertyBlock const&);
        MaterialPropertyBlock(MaterialPropertyBlock&&) noexcept;
        ~MaterialPropertyBlock() noexcept;

        MaterialPropertyBlock& operator=(MaterialPropertyBlock const&);
        MaterialPropertyBlock& operator=(MaterialPropertyBlock&&) noexcept;

        bool operator==(MaterialPropertyBlock const&) const;
        bool operator!=(MaterialPropertyBlock const&) const;
        bool operator<(MaterialPropertyBlock const&) const;

        void clear();
        bool isEmpty() const;

        bool hasProperty(std::string_view propertyName) const;
        bool hasProperty(size_t propertyNameID) const;

        // equivalent to `setVector("Color", ...) etc.
        glm::vec4 const& getColor() const;
        void setColor(glm::vec4 const&);

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

        class Impl;
    private:
        friend class GraphicsBackend;
        friend struct std::hash<MaterialPropertyBlock>;
        friend std::ostream& operator<<(std::ostream&, MaterialPropertyBlock const&);
        friend std::string to_string(MaterialPropertyBlock const&);

        std::shared_ptr<Impl> m_Impl;
    };

    std::ostream& operator<<(std::ostream&, MaterialPropertyBlock const&);
    std::string to_string(MaterialPropertyBlock const&);
}

namespace std
{
    template<>
    struct hash<osc::MaterialPropertyBlock> {
        size_t operator()(osc::MaterialPropertyBlock const&) const;
    };
}

namespace osc
{
    enum CameraProjection {
        CameraProjection_Perspective,
        CameraProjection_Orthographic,
    };

    std::ostream& operator<<(std::ostream&, CameraProjection);
    std::string to_string(CameraProjection);

    class CameraNew final {
    public:
        CameraNew();  // draws to screen
        explicit CameraNew(Texture2D);  // draws to texture
        CameraNew(CameraNew const&);
        CameraNew(CameraNew&&) noexcept;
        ~CameraNew() noexcept;

        CameraNew& operator=(CameraNew const&);
        CameraNew& operator=(CameraNew&&) noexcept;

        bool operator==(CameraNew const&) const;
        bool operator!=(CameraNew const&) const;
        bool operator<(CameraNew const&) const;

        glm::vec4 const& getBackgroundColor() const;
        void setBackgroundColor(glm::vec4 const&);

        CameraProjection getCameraProjection() const;
        void setCameraProjection(CameraProjection);

        // only used if orthographic
        //
        // e.g. https://docs.unity3d.com/ScriptReference/Camera-orthographicSize.html
        float getOrthographicSize() const;
        void setOrthographicSize(glm::vec2);

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

        glm::mat4 const& getCameraToWorldMatrix() const;

        // flushes any rendering commands that were queued against this camera
        //
        // after this call completes, callers can then use the output texture/screen
        void render();

        class Impl;
    private:
        friend class GraphicsBackend;
        friend struct std::hash<CameraNew>;
        friend std::ostream& operator<<(std::ostream&, CameraNew const&);
        friend std::string to_string(CameraNew const&);

        std::shared_ptr<Impl> m_Impl;
    };

    std::ostream& operator<<(std::ostream&, CameraNew const&);
    std::string to_string(CameraNew const&);
}

namespace std
{
    template<>
    struct hash<osc::CameraNew> {
        size_t operator()(osc::CameraNew const&) const;
    };
}

namespace osc
{
    class Graphics final {
    public:
        static void DrawMesh(Mesh&, glm::vec3 const& pos, CameraNew&, MaterialPropertyBlock const* = nullptr);
    };
}
