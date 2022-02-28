#pragma once

#include <GL/glew.h>

#include <cstdint>
#include <iosfwd>

// gl: convenience C++ bindings to OpenGL
namespace gl
{
    // low-level API: maps 1:1 with OpenGL

    GLuint CreateShader(GLenum shaderType);
    void DeleteShader(GLuint shaderHandle);
    void ShaderSource(GLuint shader, GLsizei count, const GLchar **string, GLint const* length);
    void CompileShader(GLuint shader);
    void GetShaderiv(GLuint shader, GLenum pname, GLint* params);
    void GetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar* infoLog);

    GLuint CreateProgram();
    void DeleteProgram(GLuint program);
    void UseProgram(GLuint program);
    void AttachShader(GLuint program, GLuint shader);
    void LinkProgram(GLuint program);
    void GetProgramiv(GLuint program, GLenum pname, GLint* params);
    void GetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog);

    GLint GetUniformLocation(GLuint program, GLchar const* name);
    void Uniform1f(GLint location, GLfloat v0);
    void Uniform2f(GLint location, GLfloat v0, GLfloat v1);
    void Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void Uniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void Uniform1i(GLint location, GLint v0);
    void Uniform2i(GLint location, GLint v0, GLint v1);
    void Uniform3i(GLint location, GLint v0, GLint v1, GLint v2);
    void Uniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
    void Uniform1ui(GLint location, GLuint v0);
    void Uniform2ui(GLint location, GLuint v0, GLuint v1);
    void Uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
    void Uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
    void Uniform1fv(GLint location, GLsizei count, GLfloat const* value);
    void Uniform2fv(GLint location, GLsizei count, GLfloat const* value);
    void Uniform3fv(GLint location, GLsizei count, GLfloat const* value);
    void Uniform4fv(GLint location, GLsizei count, GLfloat const* value);
    void Uniform1iv(GLint location, GLsizei count, GLint const* value);
    void Uniform2iv(GLint location, GLsizei count, GLint const* value);
    void Uniform3iv(GLint location, GLsizei count, GLint const* value);
    void Uniform4iv(GLint location, GLsizei count, GLint const* value);
    void Uniform1uiv(GLint location, GLsizei count, GLuint const* value);
    void Uniform2uiv(GLint location, GLsizei count, GLuint const* value);
    void Uniform3uiv(GLint location, GLsizei count, GLuint const* value);
    void Uniform4uiv(GLint location, GLsizei count, GLuint const* value);
    void UniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);
    void UniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value);

    GLint GetAttribLocation(GLuint program, GLchar const* name);
    void VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, size_t offset);
    void EnableVertexAttribArray(GLuint index);
    void VertexAttribDivisor(GLuint index, GLuint divisor);

    void GenBuffers(GLsizei n, GLuint* buffers);
    void DeleteBuffers(GLsizei n, GLuint const* buffers);
    void BindBuffer(GLenum target, GLuint buffer);
    void BufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage);

    void GenVertexArrays(GLsizei n, GLuint *arrays);
    void DeleteVertexArrays(GLsizei n, const GLuint *arrays);
    void BindVertexArray(GLuint array);

    void GenTextures(GLsizei n, GLuint* textures);
    void DeleteTextures(GLsizei n, GLuint* textures);
    void ActiveTexture(GLenum texture);
    void BindTexture(GLenum target, GLuint handle);
    void TexParameteri(GLenum target, GLenum pname, GLint param);
    void TexImage2D(
        GLenum target,
        GLint level,
        GLint internalformat,
        GLsizei width,
        GLsizei height,
        GLint border,
        GLenum format,
        GLenum type,
        const void* pixels);
    void GenerateMipmap(GLenum target);

    void GenFramebuffers(GLsizei n, GLuint* ids);
    void DeleteFramebuffers(GLsizei n, GLuint* framebuffers);
    void BindFramebuffer(GLenum target, GLuint framebuffer);
    void DrawBuffer(GLenum mode);
    void DrawBuffers(GLsizei n, GLenum const* bufs);
    void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

    void GenRenderbuffers(GLsizei n, GLuint* renderbuffers);
    void DeleteRenderbuffers(GLsizei n, GLuint* renderbuffers);
    void BindRenderbuffer(GLenum target, GLuint renderbuffer);
    void FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    void RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

    void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    void Clear(GLbitfield mask);

    void DrawArrays(GLenum mode, GLint first, GLsizei count);
    void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
    void DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);

    void Viewport(GLint x, GLint y, GLsizei w, GLsizei h);

    void BlitFramebuffer(
        GLint srcX0,
        GLint srcY0,
        GLint srcX1,
        GLint srcY1,
        GLint dstX0,
        GLint dstY0,
        GLint dstX1,
        GLint dstY1,
        GLbitfield mask,
        GLenum filter);

    void GetIntegerv(GLenum pname, GLint* out);

    void Enable(GLenum cap);
    void Disable(GLenum cap);


    // "high-level" API
    //
    // these don't quite map 1:1 with OpenGL, but make using the OpenGL API easier

    void UseProgram();

    // type-safe lifetime wrapper over `CreateShader`, `DeleteShader`
    class Shader {
    public:
        static constexpr GLuint EmptyHandle() { return 0; }

        explicit Shader(GLenum shaderType);
        Shader(GLenum shaderType, char const* src);
        Shader(Shader&&) noexcept;
        Shader(Shader const&) = delete;
        ~Shader() noexcept;
        Shader& operator=(Shader&&) noexcept;
        Shader& operator=(Shader const&) = delete;

        GLuint get() const { return m_ShaderHandle; }
        GLuint release();

    private:
        GLuint m_ShaderHandle;
    };

    std::ostream& operator<<(std::ostream&, Shader const&);
    void CompileFromSource(Shader const&, const char* src);  // throws on error

    // type-safe lifetime wrapper over `CreateProgram`, `DestroyProgram`
    class Program final {
    public:
        static constexpr GLuint EmptyHandle() { return 0; }

        Program();
        Program(Shader const&, Shader const&);
        Program(Shader const&, Shader const&, Shader const&);
        Program(Program const&) = delete;
        Program(Program&&) noexcept;
        ~Program() noexcept;
        Program& operator=(Program const&) = delete;
        Program& operator=(Program&&) noexcept;

        GLuint get() const { return m_ProgramHandle; }
        GLuint release();

    private:
        GLuint m_ProgramHandle;
    };

    std::ostream& operator<<(std::ostream&, Program const&);
    void UseProgram(Program const&);
    void AttachShader(Program&, Shader const&);
    void LinkProgram(Program&);

    GLint GetUniformLocationOrThrow(Program const& p, GLchar const* name);

    enum class ShaderType {
        Float = 0,
        Int,
        Sampler2D,
        Sampler2DMS,
        SamplerCube,
        Bool,
        Vec2,
        Vec3,
        Vec4,
        Mat4,
        Mat3,
        Mat4x3,
        TOTAL,
    };
    #define OSC_GLSL_SHADERTYPE_NAMES { \
        "Float" , \
        "Int", \
        "Sampler2D", \
        "Sampler2DMS", \
        "SamplerCube", \
        "Bool", \
        "Vec2", \
        "Vec3", \
        "Vec4", \
        "Mat4", \
        "Mat3", \
        "Mat4x3", \
    }
    std::ostream& operator<<(std::ostream&, ShaderType);
    int GetNumShaderLocationsTakenBy(ShaderType);
    int GetNumElementsPerLocation(ShaderType);

    // strongly-typed uniform location
    template<ShaderType ShaderTypeV>
    class Uniform {
    public:
        constexpr Uniform(GLint location) : m_Location{location} {}
        Uniform(Program const& p, GLchar const* name) : m_Location{GetUniformLocationOrThrow(p, name)} {}


        GLuint get() const { return static_cast<GLuint>(m_Location); }
        GLint geti() const { return static_cast<GLint>(m_Location); }

    private:
        GLint m_Location;
    };

    using UniformFloat = Uniform<ShaderType::Float>;
    using UniformInt = Uniform<ShaderType::Int>;
    using UniformSampler2D = Uniform<ShaderType::Sampler2D>;
    using UniformSampler2DMS = Uniform<ShaderType::Sampler2DMS>;
    using UniformSamplerCube = Uniform<ShaderType::SamplerCube>;
    using UniformBool = Uniform<ShaderType::Bool>;
    using UniformVec2 = Uniform<ShaderType::Vec2>;
    using UniformVec3 = Uniform<ShaderType::Vec3>;
    using UniformVec4 = Uniform<ShaderType::Vec4>;
    using UniformMat4 = Uniform<ShaderType::Mat4>;
    using UniformMat3 = Uniform<ShaderType::Mat3>;
    using UniformMat4x3 = Uniform<ShaderType::Mat4x3>;

    void WriteUniformToStream(std::ostream&, GLint location, ShaderType type);
    template<ShaderType ShaderTypeV>
    inline std::ostream& operator<<(std::ostream& o, Uniform<ShaderTypeV> const& u)
    {
        WriteUniformToStream(o, u.geti(), ShaderTypeV);
        return o;
    }

    void SetUniform(Uniform<ShaderType::Float>& u, GLfloat value);
    void SetUniform(Uniform<ShaderType::Vec3>& u, float x, float y, float z);
    void SetUniform(Uniform<ShaderType::Int>& u, GLint value);
    void SetUniform(Uniform<ShaderType::Sampler2D>& u, GLint v);
    void SetUniform(Uniform<ShaderType::Sampler2DMS>& u, GLint v);
    void SetUniform(Uniform<ShaderType::Bool>& u, bool v);
    void SetUniform(Uniform<ShaderType::Vec3>& u, float const vs[3]);
    void SetUniform(Uniform<ShaderType::Int>& u, GLsizei n, GLint const* data);

    GLint GetAttribLocationOrThrow(Program const& p, GLchar const* name);

    // strongly-typed attribute location
    template<ShaderType ShaderTypeV>
    class Attribute {
    public:
        constexpr Attribute(GLint location) : m_Location{location} {}
        Attribute(Program const& p, GLchar const* name) : m_Location{GetAttribLocationOrThrow(p, name)} {}

        GLuint get() const { return static_cast<GLuint>(m_Location); }
        GLint geti() const { return static_cast<GLint>(m_Location); }
        ShaderType getType() const { return ShaderTypeV; }

    private:
        GLint m_Location;
    };

    using AttributeFloat = Attribute<ShaderType::Float>;
    using AttributeInt = Attribute<ShaderType::Int>;
    using AttributeSampler2D = Attribute<ShaderType::Sampler2D>;
    using AttributeSampler2DMD = Attribute<ShaderType::Sampler2DMS>;
    using AttributeSamplerCube = Attribute<ShaderType::SamplerCube>;
    using AttributeBool = Attribute<ShaderType::Bool>;
    using AttributeVec2 = Attribute<ShaderType::Vec2>;
    using AttributeVec3 = Attribute<ShaderType::Vec3>;
    using AttributeVec4 = Attribute<ShaderType::Vec4>;
    using AttributeMat4 = Attribute<ShaderType::Mat4>;
    using AttributeMat3 = Attribute<ShaderType::Mat3>;
    using AttributeMat4x3 = Attribute<ShaderType::Mat4x3>;

    void WriteAttributeToStream(std::ostream&, GLint location, ShaderType type);

    template<ShaderType ShaderTypeV>
    inline std::ostream& operator<<(std::ostream& o, Attribute<ShaderTypeV> const& a)
    {
        WriteAttributeToStream(o, a.geti(), a.getType());
        return o;
    }

    size_t TypeSize(GLenum);  // e.g. GL_BYTE -> sizeof(GLbyte), GL_FLOAT -> sizeof(GLfloat)

    template<ShaderType ShaderTypeV>
    inline void VertexAttribPointer(Attribute<ShaderTypeV> const& attr, GLint size, GLenum type, GLboolean normalized, GLsizei stride, size_t offset)
    {
        VertexAttribPointer(attr.get(), size, type, normalized, stride, offset);
    }

    // high-level description of a attribute-buffer binding
    class BufferBindingDescription final {
    public:
        BufferBindingDescription(GLint attributeLocation,
                                 gl::ShaderType shaderType,
                                 GLenum bufferDataFormat,
                                 bool isNormalized,
                                 size_t offset);

        template<ShaderType ShaderTypeV>
        BufferBindingDescription(gl::Attribute<ShaderTypeV> const& attr,
                                 GLenum bufferDataFormat,
                                 bool isNormalized,
                                 size_t offset) :
            BufferBindingDescription{attr.get(),
                                     ShaderTypeV,
                                     bufferDataFormat,
                                     isNormalized,
                                     offset}
        {
        }

        GLint getAttributeLocation() const { return m_AttributeLocation; }
        gl::ShaderType getShaderType() const { return m_ShaderType; }
        GLenum getBufferDataFormat() const { return m_BufferDataFormat; }
        bool isNormalized() const { return m_IsNormalized; }
        size_t getOffset() const { return m_Offset; }

    private:
        GLint m_AttributeLocation;
        gl::ShaderType m_ShaderType;
        GLenum m_BufferDataFormat;
        bool m_IsNormalized;
        size_t m_Offset;
    };

    // VertexAttribPointer that takes a high-level description - handy for annoying bindings, like matrices
    void VertexAttribPointer(BufferBindingDescription const&, size_t stride);

    void EnableVertexAttribArray(GLuint index, ShaderType type);  // enables multiple slots, if the shader type requires it

    template<ShaderType ShaderTypeV>
    inline void EnableVertexAttribArray(Attribute<ShaderTypeV> const& attr)
    {
        EnableVertexAttribArray(attr.get(), attr.getType());
    }

    void VertexAttribDivisor(GLuint index, ShaderType type, GLuint divisor);

    template<ShaderType ShaderTypeV>
    inline void VertexAttribDivisor(Attribute<ShaderTypeV> const& attr, GLuint divisor)
    {
        VertexAttribDivisor(attr.get(), attr.getType(), divisor);
    }

    // type-safe lifetime wrapper over `GenBuffers`, `DeleteBuffers`
    class Buffer {
    public:
        static constexpr GLuint EmptyHandle() { return -1; }

        Buffer();  // effectively, glGenBuffers
        Buffer(Buffer const&) = delete;
        Buffer(Buffer&&) noexcept;
        ~Buffer() noexcept;
        Buffer& operator=(Buffer const&) = delete;
        Buffer& operator=(Buffer&&) noexcept;

        GLuint get() const { return m_BufferHandle; }
        GLuint release();

    private:
        GLuint m_BufferHandle;
    };

    std::ostream& operator<<(std::ostream&, Buffer const&);
    void BindBuffer(GLenum target, Buffer const&);

    // effectively, make it, bind it, uplaod data
    Buffer CreateBuffer(GLenum target,  GLenum usage, const void* data, GLsizeiptr size);

    template<typename Container>
    Buffer CreateBuffer(GLenum target, GLenum usage, Container const& c)
    {
        using V = typename Container::value_type;

        void const* ptr = c.data();
        GLsizeiptr byteSize = static_cast<GLsizeiptr>(c.size() * sizeof(V));

        return CreateBuffer(target, usage, ptr, byteSize);
    }

    template<typename T, size_t N>
    Buffer CreateBuffer(GLenum target, GLenum usage, T const (&arr)[N])
    {
        GLsizeiptr byteSize = static_cast<GLsizeiptr>(N * sizeof(T));
        return CreateBuffer(target, usage, arr, byteSize);
    }

    // helper class that effectively holds a type-erased raw buffer + extra information
    class SizedBuffer final : public Buffer {
    public:
        SizedBuffer(Buffer, int byteSize, int structSize);

        GLuint get() const { return m_Buffer.get(); }
        GLuint release() { return m_Buffer.release(); }
        int numBytes() const noexcept { return m_ByteSize; }
        int numEls() const noexcept { return m_ByteSize/m_StructSize; }

        void assign(GLenum target, GLenum usage, void const* ptr, int byteSize, int structSize);

        template<typename Container>
        void assign(GLenum target, GLenum usage, Container const& c)
        {
            int structSize = static_cast<int>(sizeof(typename Container::value_type));
            GLsizeiptr byteSize = static_cast<GLsizeiptr>(c.size() * structSize);
            assign(target, usage, c.data(), byteSize, structSize);
        }

        template<typename T, size_t N>
        void assign(GLenum target, GLenum usage, T const (&arr)[N])
        {
            int structSize = static_cast<int>(sizeof(T));
            GLsizeiptr byteSize = static_cast<GLsizeiptr>(N * structSize);
            assign(target, usage, arr, byteSize, structSize);
        }

    private:
        Buffer m_Buffer;
        int m_ByteSize;
        int m_StructSize;
    };

    std::ostream& operator<<(std::ostream&, SizedBuffer const&);

    template<typename Container>
    SizedBuffer CreateSizedBuffer(GLenum target, GLenum usage, Container const& c)
    {
        int elSize = static_cast<int>(sizeof(typename Container::value_type));
        int nEls = static_cast<int>(c.size());
        int nBytes = nEls * elSize;
        return SizedBuffer{CreateBuffer(target, usage, c), nBytes, elSize};
    }

    template<typename T, size_t N>
    SizedBuffer CreateSizedBuffer(GLenum target, GLenum usage, T const (&arr)[N])
    {
        int elSize = static_cast<int>(sizeof(T));
        int nEls = static_cast<int>(N);
        int nBytes = nEls * elSize;
        return SizedBuffer{CreateBuffer(target, usage, arr), nBytes, elSize};
    }

    // type-safe lifetime wrapper over GenVertexArrays, DeleteVertexArrays
    class VertexArray final {
    public:
        static constexpr GLuint EmptyHandle() { return -1; }

        VertexArray();
        VertexArray(VertexArray const&) = delete;
        VertexArray(VertexArray&&) noexcept;
        ~VertexArray() noexcept;
        VertexArray& operator=(VertexArray const&) = delete;
        VertexArray& operator=(VertexArray&&) noexcept;

        GLuint get() const { return m_VaoHandle; }
        GLuint release();

    private:
        GLuint m_VaoHandle;
    };

    std::ostream& operator<<(std::ostream&, VertexArray const&);
    void BindVertexArray(VertexArray const& vao);
    void BindVertexArray();

    // type-safe lifetime wrapper over GenTextures, DeleteTextures
    class Texture final {
    public:
        static constexpr GLuint EmptyHandle() { return -1; }

        Texture();
        Texture(Texture const&) = delete;
        Texture(Texture&&) noexcept;
        ~Texture() noexcept;
        Texture& operator=(Texture const&) = delete;
        Texture& operator=(Texture&&) noexcept;

        GLuint get() const { return m_TextureHandle; }
        void* getVoidHandle() const { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_TextureHandle)); }

        GLuint release();

    private:
        GLuint m_TextureHandle;
    };

    std::ostream& operator<<(std::ostream&, Texture const&);
    void BindTexture(GLenum target, Texture const& texture);

    // type-safe lifetime wrapper over GenFrameBuffers, DeleteFrameBuffers
    class FrameBuffer final {
    public:
        static constexpr GLuint EmptyHandle() { return -1; }

        FrameBuffer();
        FrameBuffer(FrameBuffer const&);
        FrameBuffer(FrameBuffer&&) noexcept;
        ~FrameBuffer() noexcept;
        FrameBuffer& operator=(FrameBuffer const&) = delete;
        FrameBuffer& operator=(FrameBuffer&&) noexcept;

        GLuint get() const { return m_FboHandle; }
        GLuint release();

    private:
        GLuint m_FboHandle;
    };

    std::ostream& operator<<(std::ostream&, FrameBuffer const&);
    void BindFramebuffer(GLenum target, FrameBuffer const& fb);
    void BindWindowFramebuffer(GLenum target);

    template<typename... T>
    inline void DrawBuffers(T... vs) noexcept {
        GLenum attachments[sizeof...(vs)] = {static_cast<GLenum>(vs)...};
        DrawBuffers(sizeof...(vs), attachments);
    }

    void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, Texture const&, GLint level);

    // type-safe lifetime wrapper over GenRenderBuffers, DeleteRenderBuffers
    class RenderBuffer final {
    public:
        static constexpr GLuint EmptyHandle() { return 0; }

        RenderBuffer();
        RenderBuffer(RenderBuffer const&) = delete;
        RenderBuffer(RenderBuffer&&) noexcept;
        ~RenderBuffer() noexcept;
        RenderBuffer& operator=(RenderBuffer const&) = delete;
        RenderBuffer& operator=(RenderBuffer&&) noexcept;

        GLuint get() const { return m_RenderBuffer; }
        GLuint release();

    private:
        GLuint m_RenderBuffer;
    };

    std::ostream& operator<<(std::ostream&, RenderBuffer const&);
    void BindRenderBuffer(RenderBuffer& rb);
    void BindRenderBuffer();
    void FramebufferRenderbuffer(GLenum target, GLenum attachment, RenderBuffer const& rb);

    bool IsCurrentFboComplete();

    int GetInteger(GLenum pname);
    GLenum GetEnum(GLenum pname);
}
