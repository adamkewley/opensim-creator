#include "Gl.hpp"

#include <exception>
#include <sstream>
#include <vector>
#include <type_traits>

#define GL_STRINGIFY(x) #x
#define GL_TOSTRING(x) GL_STRINGIFY(x)
#define GL_SOURCELOC __FILE__ ":" GL_TOSTRING(__LINE__)

GLuint gl::CreateShader(GLenum shaderType)
{
    return glCreateShader(shaderType);
}

void gl::DeleteShader(GLuint shaderHandle)
{
    glDeleteShader(shaderHandle);
}

void gl::ShaderSource(GLuint shader, GLsizei count, const GLchar **string, GLint const* length)
{
    glShaderSource(shader, count, string, length);
}

void gl::CompileShader(GLuint shader)
{
    glCompileShader(shader);
}

void gl::GetShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    glGetShaderiv(shader, pname, params);
}

void gl::GetShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei *length, GLchar* infoLog)
{
    glGetShaderInfoLog(shader, maxLength, length, infoLog);
}



GLuint gl::CreateProgram()
{
    return glCreateProgram();
}

void gl::DeleteProgram(GLuint program)
{
    glDeleteProgram(program);
}

void gl::UseProgram(GLuint program)
{
    glUseProgram(program);
}

void gl::AttachShader(GLuint program, GLuint shader)
{
    glAttachShader(program, shader);
}

void gl::LinkProgram(GLuint program)
{
    glLinkProgram(program);
}

void gl::GetProgramiv(GLuint program, GLenum pname, GLint* params)
{
    glGetProgramiv(program, pname, params);
}

void gl::GetProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
    glGetProgramInfoLog(program, maxLength, length, infoLog);
}

GLint gl::GetUniformLocation(GLuint program, GLchar const* name)
{
    return glGetUniformLocation(program, name);
}

void gl::Uniform1f(GLint location, GLfloat v0)
{
    glUniform1f(location, v0);
}

void gl::Uniform2f(GLint location, GLfloat v0, GLfloat v1)
{
    glUniform2f(location, v0, v1);
}

void gl::Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    glUniform3f(location, v0, v1, v2);
}

void gl::Uniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    glUniform4f(location, v0, v1, v2, v3);
}

void gl::Uniform1i(GLint location, GLint v0)
{
    glUniform1i(location, v0);
}

void gl::Uniform2i(GLint location, GLint v0, GLint v1)
{
    glUniform2i(location, v0, v1);
}

void gl::Uniform3i(GLint location, GLint v0, GLint v1, GLint v2)
{
    glUniform3i(location, v0, v1, v2);
}

void gl::Uniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    glUniform4i(location, v0, v1, v2, v3);
}

void gl::Uniform1ui(GLint location, GLuint v0)
{
    glUniform1ui(location, v0);
}

void gl::Uniform2ui(GLint location, GLuint v0, GLuint v1)
{
    glUniform2ui(location, v0, v1);
}

void gl::Uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    glUniform3ui(location, v0, v1, v2);
}

void gl::Uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    glUniform4ui(location, v0, v1, v2, v3);
}

void gl::Uniform1fv(GLint location, GLsizei count, GLfloat const* value)
{
    glUniform1fv(location, count, value);
}

void gl::Uniform2fv(GLint location, GLsizei count, GLfloat const* value)
{
    glUniform2fv(location, count, value);
}

void gl::Uniform3fv(GLint location, GLsizei count, GLfloat const* value)
{
    glUniform3fv(location, count, value);
}

void gl::Uniform4fv(GLint location, GLsizei count, GLfloat const* value)
{
    glUniform4fv(location, count, value);
}

void gl::Uniform1iv(GLint location, GLsizei count, GLint const* value)
{
    glUniform1iv(location, count, value);
}

void gl::Uniform2iv(GLint location, GLsizei count, GLint const* value)
{
    glUniform2iv(location, count, value);
}

void gl::Uniform3iv(GLint location, GLsizei count, GLint const* value)
{
    glUniform3iv(location, count, value);
}

void gl::Uniform4iv(GLint location, GLsizei count, GLint const* value)
{
    glUniform4iv(location, count, value);
}

void gl::Uniform1uiv(GLint location, GLsizei count, GLuint const* value)
{
    glUniform1uiv(location, count, value);
}

void gl::Uniform2uiv(GLint location, GLsizei count, GLuint const* value)
{
    glUniform2uiv(location, count, value);
}

void gl::Uniform3uiv(GLint location, GLsizei count, GLuint const* value)
{
    glUniform3uiv(location, count, value);
}

void gl::Uniform4uiv(GLint location, GLsizei count, GLuint const* value)
{
    glUniform4uiv(location, count, value);
}

void gl::UniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value)
{
    glUniformMatrix2fv(location, count, transpose, value);
}

void gl::UniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, GLfloat const* value)
{
    glUniformMatrix3fv(location, count, transpose, value);
}

void gl::UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix4fv(location, count, transpose, value);
}

void gl::UniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix2x3fv(location, count, transpose, value);
}

void gl::UniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix3x2fv(location, count, transpose, value);
}

void gl::UniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix2x4fv(location, count, transpose, value);
}

void gl::UniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix4x2fv(location, count, transpose, value);
}

void gl::UniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix3x4fv(location, count, transpose, value);
}

void gl::UniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    glUniformMatrix4x3fv(location, count, transpose, value);
}



GLint gl::GetAttribLocation(GLuint program, GLchar const* name)
{
    return glGetAttribLocation(program, name);
}

void gl::VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, size_t offset)
{
    glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<void*>(offset));
}

void gl::EnableVertexAttribArray(GLuint index)
{
    glEnableVertexAttribArray(index);
}

void gl::VertexAttribDivisor(GLuint index, GLuint divisor)
{
    glVertexAttribDivisor(index, divisor);
}



void gl::GenBuffers(GLsizei n, GLuint* buffers)
{
    glGenBuffers(n, buffers);
}

void gl::DeleteBuffers(GLsizei n, GLuint const* buffers)
{
    glDeleteBuffers(n, buffers);
}

void gl::BindBuffer(GLenum target, GLuint buffer)
{
    glBindBuffer(target, buffer);
}

void gl::BufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    glBufferData(target, size, data, usage);
}



void gl::GenVertexArrays(GLsizei n, GLuint *arrays)
{
    glGenVertexArrays(n, arrays);
}

void gl::DeleteVertexArrays(GLsizei n, GLuint const* arrays)
{
    glDeleteVertexArrays(n, arrays);
}

void gl::BindVertexArray(GLuint array)
{
    glBindVertexArray(array);
}



void gl::GenTextures(GLsizei n, GLuint* textures)
{
    glGenTextures(n, textures);
}

void gl::DeleteTextures(GLsizei n, GLuint* textures)
{
    glDeleteTextures(n, textures);
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glActiveTexture.xhtml
void gl::ActiveTexture(GLenum texture)
{
    glActiveTexture(texture);
}

void gl::BindTexture(GLenum target, GLuint handle)
{
    glBindTexture(target, handle);
}

void gl::TexParameteri(GLenum target, GLenum pname, GLint param)
{
    glTexParameteri(target, pname, param);
}

void gl::TexImage2D(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const void* pixels)
{
    glTexImage2D(target,
                 level,
                 internalformat,
                 width,
                 height,
                 border,
                 format,
                 type,
                 pixels);
}

void gl::GenerateMipmap(GLenum target)
{
    glGenerateMipmap(target);
}



void gl::GenFramebuffers(GLsizei n, GLuint* ids)
{
    glGenFramebuffers(n, ids);
}

void gl::DeleteFramebuffers(GLsizei n, GLuint* framebuffers)
{
    glDeleteFramebuffers(n, framebuffers);
}

void gl::BindFramebuffer(GLenum target, GLuint framebuffer)
{
    glBindFramebuffer(target, framebuffer);
}

void gl::DrawBuffer(GLenum mode)
{
    glDrawBuffer(mode);
}

void gl::DrawBuffers(GLsizei n, GLenum const* bufs)
{
    glDrawBuffers(n, bufs);
}

void gl::FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}



void gl::GenRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    glGenRenderbuffers(n, renderbuffers);
}

void gl::BindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    glBindRenderbuffer(target, renderbuffer);
}

void gl::DeleteRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    glDeleteRenderbuffers(n, renderbuffers);
}

void BindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    glBindRenderbuffer(target, renderbuffer);
}

void gl::FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void gl::RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorage(target, internalformat, width, height);
}

void gl::ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    glClearColor(red, green, blue, alpha);
}

void gl::Clear(GLbitfield mask)
{
    glClear(mask);
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawArrays.xhtml
void gl::DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    glDrawArrays(mode, first, count);
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawArraysInstanced.xhtml
void gl::DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
    glDrawArraysInstanced(mode, first, count, instancecount);
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawElements.xhtml
void gl::DrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    glDrawElements(mode, count, type, indices);
}

void gl::Viewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    glViewport(x, y, w, h);
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBlitFramebuffer.xhtml
void gl::BlitFramebuffer(
    GLint srcX0,
    GLint srcY0,
    GLint srcX1,
    GLint srcY1,
    GLint dstX0,
    GLint dstY0,
    GLint dstX1,
    GLint dstY1,
    GLbitfield mask,
    GLenum filter)
{
    glBlitFramebuffer(srcX0,
                      srcY0,
                      srcX1,
                      srcY1,
                      dstX0,
                      dstY0,
                      dstX1,
                      dstY1,
                      mask,
                      filter);
}

void gl::GetIntegerv(GLenum pname, GLint* out)
{
    glGetIntegerv(pname, out);
}

void gl::Enable(GLenum cap)
{
    glEnable(cap);
}

void gl::Disable(GLenum cap)
{
    glDisable(cap);
}


// "high-level" API

// an exception that specifically means something has gone wrong in
// the OpenGL API
class OpenGlException final : public std::exception {
    std::string m_Msg;

public:
    OpenGlException(std::string s) : m_Msg{std::move(s)} {
    }

    char const* what() const noexcept {
        return m_Msg.c_str();
    }
};


void gl::UseProgram()
{
    UseProgram(static_cast<GLuint>(0));
}

gl::Shader::Shader(GLenum shaderType) :
    m_ShaderHandle{CreateShader(shaderType)}
{
    if (m_ShaderHandle == Shader::EmptyHandle()) {
        throw OpenGlException{GL_SOURCELOC ": glCreateShader() failed: this could mean that your GPU/system is out of memory, or that your OpenGL driver is invalid in some way"};
    }
}

gl::Shader::Shader(GLenum shaderType, char const* src) :
    Shader{shaderType}
{
    CompileFromSource(*this, src);
}

gl::Shader::Shader(Shader&& tmp) noexcept :
    m_ShaderHandle{std::exchange(tmp.m_ShaderHandle, Shader::EmptyHandle())}
{
}

gl::Shader::~Shader() noexcept
{
    if (m_ShaderHandle != Shader::EmptyHandle()) {
        DeleteShader(m_ShaderHandle);
    }
}

gl::Shader& gl::Shader::Shader::operator=(Shader&& tmp) noexcept
{
    std::swap(m_ShaderHandle, tmp.m_ShaderHandle);
    return *this;
}

GLuint gl::Shader::release()
{
    return std::exchange(m_ShaderHandle, Shader::EmptyHandle());
}

std::ostream& gl::operator<<(std::ostream& o, Shader const& s)
{
    return o << "Shader(handle = " << s.get() << ')';
}

void gl::CompileFromSource(Shader const& s, const char* src)
{
    ShaderSource(s.get(), 1, &src, nullptr);
    CompileShader(s.get());

    // check for compile errors
    GLint params = GL_FALSE;
    GetShaderiv(s.get(), GL_COMPILE_STATUS, &params);

    if (params == GL_TRUE) {
        return;
    }

    // else: there were compile errors

    GLint logLen = 0;
    GetShaderiv(s.get(), GL_INFO_LOG_LENGTH, &logLen);

    std::vector<GLchar> errMessageBytes(logLen);
    GetShaderInfoLog(s.get(), logLen, &logLen, errMessageBytes.data());

    std::stringstream ss;
    ss << "gl::CompilesShader failed: " << errMessageBytes.data();
    throw std::runtime_error{std::move(ss).str()};
}

gl::Program::Program() :
    m_ProgramHandle{CreateProgram()}
{
    if (m_ProgramHandle == Program::EmptyHandle()) {
        throw OpenGlException{GL_SOURCELOC "CreateProgram() failed: this could mean that your GPU/system is out of memory, or that your OpenGL driver is invalid in some way"};
    }
}

gl::Program::Program(Shader const& a, Shader const& b) :
    Program{}
{
    AttachShader(*this, a);
    AttachShader(*this, b);
    LinkProgram(*this);
}

gl::Program::Program(Shader const& a, Shader const& b, Shader const& c) :
    Program{}
{
    AttachShader(*this, a);
    AttachShader(*this, b);
    AttachShader(*this, c);
    LinkProgram(*this);
}

gl::Program::Program(Program&& tmp) noexcept :
    m_ProgramHandle{std::exchange(tmp.m_ProgramHandle, Program::EmptyHandle())}
{
}

gl::Program::~Program() noexcept
{
    if (m_ProgramHandle != Program::EmptyHandle()) {
        DeleteProgram(m_ProgramHandle);
    }
}

gl::Program& gl::Program::operator=(Program&& tmp) noexcept
{
    std::swap(m_ProgramHandle, tmp.m_ProgramHandle);
    return *this;
}

GLuint gl::Program::release()
{
    return std::exchange(m_ProgramHandle, Program::EmptyHandle());
}

std::ostream& gl::operator<<(std::ostream& o, Program const& p)
{
    return o << "Program(handle = " << p.get() << ')';
}

void gl::UseProgram(Program const& p)
{
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glUseProgram.xhtml
    UseProgram(p.get());
}

void gl::AttachShader(Program& p, Shader const& sh)
{
    AttachShader(p.get(), sh.get());
}

void gl::LinkProgram(Program& p)
{
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glLinkProgram.xhtml

    LinkProgram(p.get());

    // check for link errors
    GLint linkStatus = GL_FALSE;
    GetProgramiv(p.get(), GL_LINK_STATUS, &linkStatus);

    if (linkStatus == GL_TRUE) {
        return;
    }

    // else: there were link errors
    GLint logLen = 0;
    GetProgramiv(p.get(), GL_INFO_LOG_LENGTH, &logLen);

    std::vector<GLchar> errMessageBytes(logLen);
    GetProgramInfoLog(p.get(), static_cast<GLsizei>(errMessageBytes.size()), nullptr, errMessageBytes.data());

    std::stringstream ss;
    ss << "OpenGL: glLinkProgram() failed: ";
    ss << errMessageBytes.data();
    throw std::runtime_error{ss.str()};
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGetUniformLocation.xhtml
//     *throws on error
GLint gl::GetUniformLocationOrThrow(Program const& p, GLchar const* name)
{
    GLint handle = GetUniformLocation(p.get(), name);
    if (handle == -1) {
        throw OpenGlException{std::string{"glGetUniformLocation() failed: cannot get "} + name};
    }
    return handle;
}

std::ostream& gl::operator<<(std::ostream& o, ShaderType s)
{
    switch (s) {
    case ShaderType::Float:
        return o << "Float";
    case ShaderType::Int:
        return o << "Int";
    case ShaderType::Sampler2D:
        return o << "Sampler2D";
    case ShaderType::Sampler2DMS:
        return o << "Sampler2DMS";
    case ShaderType::SamplerCube:
        return o << "SamplerCube";
    case ShaderType::Bool:
        return o << "Bool";
    case ShaderType::Vec2:
        return o << "Vec2";
    case ShaderType::Vec3:
        return o << "Vec3";
    case ShaderType::Vec4:
        return o << "Vec4";
    case ShaderType::Mat4:
        return o << "Mat4";
    case ShaderType::Mat3:
        return o << "Mat3";
    case ShaderType::Mat4x3:
        return o << "Mat4x3";
    default:
        throw std::logic_error{GL_SOURCELOC ": unsupported shader type passed in"};
    }
}

int gl::GetNumShaderLocationsTakenBy(ShaderType t)
{
    switch (t) {
    case ShaderType::Float:
    case ShaderType::Int:
    case ShaderType::Sampler2D:
    case ShaderType::Sampler2DMS:
    case ShaderType::SamplerCube:
    case ShaderType::Bool:
    case ShaderType::Vec2:
    case ShaderType::Vec3:
    case ShaderType::Vec4:
        return 1;
    case ShaderType::Mat4:
        return 4;
    case ShaderType::Mat3:
        return 3;
    case ShaderType::Mat4x3:
        return 4;
    default:
        throw std::logic_error{GL_SOURCELOC ": unsupported shader type passed to GetNumShaderLocationsTakenBy"};
    }
}

int gl::GetNumElementsPerLocation(ShaderType t)
{
    switch (t) {
    case ShaderType::Float:
    case ShaderType::Int:
    case ShaderType::Sampler2D:
    case ShaderType::Sampler2DMS:
    case ShaderType::SamplerCube:
    case ShaderType::Bool:
        return 1;
    case ShaderType::Vec2:
        return 2;
    case ShaderType::Vec3:
        return 3;
    case ShaderType::Vec4:
        return 4;
    case ShaderType::Mat4:
        return 4;
    case ShaderType::Mat3:
        return 3;
    case ShaderType::Mat4x3:
        return 3;
    default:
        throw std::logic_error{GL_SOURCELOC ": unsupported shader type passed to GetNumElementsPerLocation"};
    }
}

void gl::WriteUniformToStream(std::ostream& o, GLint location, ShaderType st)
{
    o << "Uniform<" << st << ">(loc = " << location << ')';
}

void gl::SetUniform(gl::Uniform<gl::ShaderType::Float>& u, GLfloat value)
{
    Uniform1f(u.geti(), value);
}

void gl::SetUniform(Uniform<ShaderType::Vec3>& u, float x, float y, float z)
{
    Uniform3f(u.geti(), x, y, z);
}


void gl::SetUniform(Uniform<ShaderType::Int>& u, GLint value)
{
    Uniform1i(u.geti(), value);
}

void gl::SetUniform(Uniform<ShaderType::Sampler2DMS>& u, GLint v)
{
    Uniform1i(u.geti(), v);
}

void gl::SetUniform(Uniform<ShaderType::Sampler2D>& u, GLint v)
{
    Uniform1i(u.geti(), v);
}

void gl::SetUniform(Uniform<ShaderType::Bool>& u, bool v)
{
    Uniform1i(u.geti(), v);
}

void gl::SetUniform(Uniform<ShaderType::Vec3>& u, float const vs[3])
{
    Uniform3fv(u.geti(), 1, vs);
}

void gl::SetUniform(Uniform<ShaderType::Int>& u, GLsizei n, GLint const* data)
{
    Uniform1iv(u.geti(), n, data);
}


GLint gl::GetAttribLocationOrThrow(Program const& p, GLchar const* name)
{
    GLint handle = GetAttribLocation(p.get(), name);
    if (handle == -1) {
        throw OpenGlException{std::string{"glGetAttribLocation() failed: cannot get "} + name};
    }
    return handle;
}

void gl::WriteAttributeToStream(std::ostream& o, GLint location, ShaderType type)
{
    o << "Attribute<" << type << ">(loc = " << location << ')';
}

size_t gl::TypeSize(GLenum type)
{
    // https://www.khronos.org/opengl/wiki/OpenGL_Type
    // https://www.khronos.org/opengl/wiki/Vertex_Specification#Component_type

    switch (type) {
    case GL_BYTE:
        return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE:
        return sizeof(GLubyte);
    case GL_SHORT:
        return sizeof(GLshort);
    case GL_UNSIGNED_SHORT:
        return sizeof(GLushort);
    case GL_INT:
        return sizeof(GLint);
    case GL_UNSIGNED_INT:
        return sizeof(GLuint);
    case GL_INT_2_10_10_10_REV:
        static_assert(sizeof(uint32_t) == 4);
        return sizeof(uint32_t);  // packed into a 32-bit unsinged integer. Two's complement signed integer but overall bitfield is unsigned. Bitdepth is 2,10,10,10, but in reverse order, so least-significant 10 bits are the first component.
    case GL_UNSIGNED_INT_2_10_10_10_REV:
        static_assert(sizeof(uint32_t) == 4);
        return sizeof(uint32_t);  // unsigned version of above
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
        // ONLY ON OpenGL >=4.4
        static_assert(sizeof(uint32_t) == 4);
        return sizeof(uint32_t);  // unsigned version of above
    case GL_FIXED:
        return sizeof(GLfixed);
    case GL_HALF_FLOAT:
        return sizeof(GLhalf);
    case GL_FLOAT:
        return sizeof(GLfloat);
    case GL_DOUBLE:
        return sizeof(GLdouble);
    default:
        throw OpenGlException{"unknown type supplied to GetTypeSize"};
    }
}

gl::BufferBindingDescription::BufferBindingDescription(
        GLint attributeLocation,
        gl::ShaderType shaderType,
        GLenum bufferDataFormat,
        bool isNormalized,
        size_t offset) :
    m_AttributeLocation{std::move(attributeLocation)},
    m_ShaderType{std::move(shaderType)},
    m_BufferDataFormat{std::move(bufferDataFormat)},
    m_IsNormalized{std::move(isNormalized)},
    m_Offset{std::move(offset)}
{
}

void gl::VertexAttribPointer(BufferBindingDescription const& d, size_t stride)
{
    int locs = gl::GetNumShaderLocationsTakenBy(d.getShaderType());
    int elsPerLoc = gl::GetNumElementsPerLocation(d.getShaderType());
    size_t typeSize = gl::TypeSize(d.getBufferDataFormat());
    size_t sizePerLoc = elsPerLoc*typeSize;

    for (int i = 0; i < locs; ++i)
    {
        int loc = d.getAttributeLocation() + i;
        size_t offset = d.getOffset() + i*sizePerLoc;
        gl::VertexAttribPointer(loc, elsPerLoc, d.getBufferDataFormat(), d.isNormalized(), static_cast<GLsizei>(stride), offset);
    }
}

void gl::EnableVertexAttribArray(GLuint index, ShaderType type)
{
    int numShaderSlots = GetNumShaderLocationsTakenBy(type);
    for (int slot = 0; slot < numShaderSlots; ++slot) {
        EnableVertexAttribArray(index++);
    }
}

void gl::VertexAttribDivisor(GLuint index, ShaderType type, GLuint divisor)
{
    int numShaderSlots = GetNumShaderLocationsTakenBy(type);
    for (int slot = 0; slot < numShaderSlots; ++slot) {
        VertexAttribDivisor(index++, divisor);
    }
}

gl::Buffer::Buffer()
{
    GenBuffers(1, &m_BufferHandle);
    if (m_BufferHandle == Buffer::EmptyHandle()) {
        throw OpenGlException{GL_SOURCELOC "glGenBuffers() failed: this could mean that your GPU/system is out of memory, or that your OpenGL driver is invalid in some way"};
    }
}

gl::Buffer::Buffer(Buffer&& tmp) noexcept :
    m_BufferHandle{std::exchange(tmp.m_BufferHandle, Buffer::EmptyHandle())}
{
}

gl::Buffer::~Buffer() noexcept
{
    if (m_BufferHandle != Buffer::EmptyHandle()) {
        DeleteBuffers(1, &m_BufferHandle);
    }
}

gl::Buffer& gl::Buffer::operator=(Buffer&& tmp) noexcept
{
    std::swap(m_BufferHandle, tmp.m_BufferHandle);
    return *this;
}

GLuint gl::Buffer::release()
{
    return std::exchange(m_BufferHandle, Buffer::EmptyHandle());
}

std::ostream& gl::operator<<(std::ostream& o, Buffer const& b)
{
    return o << "Buffer(handle = " << b.get() << ')';
}

void gl::BindBuffer(GLenum target, Buffer const& handle)
{
    BindBuffer(target, handle.get());
}

gl::Buffer gl::CreateBuffer(GLenum target, GLenum usage, const void* data, GLsizeiptr size)
{
    Buffer rv;
    BindBuffer(target, rv);
    BufferData(target, size, data, usage);
    return rv;
}

gl::SizedBuffer::SizedBuffer(Buffer b, int byteSize, int structSize) :
    Buffer{std::move(b)},
    m_ByteSize{std::move(byteSize)},
    m_StructSize{std::move(structSize)}
{
}

void gl::SizedBuffer::assign(
        GLenum target,
        GLenum usage,
        void const* ptr,
        int byteSize,
        int structSize)
{
    BufferData(target, byteSize, ptr, usage);
    m_ByteSize = static_cast<int>(byteSize);
    m_StructSize = structSize;
}

std::ostream& gl::operator<<(std::ostream& o, SizedBuffer const& sb)
{
    return o << "SizedBuffer(handle = " << sb.get() << ", bytes = " << sb.numBytes() << ", nels = " << sb.numEls() << ")";
}

gl::VertexArray::VertexArray()
{
    GenVertexArrays(1, &m_VaoHandle);
    if (m_VaoHandle == VertexArray::EmptyHandle()) {
        throw OpenGlException{GL_SOURCELOC "glGenVertexArrays() failed: this could mean that your GPU/system is out of memory, or that your OpenGL driver is invalid in some way"};
    }
}

gl::VertexArray::VertexArray(VertexArray&& tmp) noexcept :
    m_VaoHandle{std::exchange(tmp.m_VaoHandle, VertexArray::EmptyHandle())}
{
}

gl::VertexArray::~VertexArray() noexcept
{
    if (m_VaoHandle != VertexArray::EmptyHandle()) {
        DeleteVertexArrays(1, &m_VaoHandle);
    }
}

gl::VertexArray& gl::VertexArray::operator=(VertexArray&& tmp) noexcept
{
    std::swap(m_VaoHandle, tmp.m_VaoHandle);
    return *this;
}

GLuint gl::VertexArray::release()
{
    return std::exchange(m_VaoHandle, VertexArray::EmptyHandle());
}

std::ostream& gl::operator<<(std::ostream& o, VertexArray const& vao)
{
    return o << "VertexArray(handle = " << vao.get() << ')';
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBindVertexArray.xhtml
void gl::BindVertexArray(VertexArray const& vao)
{
    BindVertexArray(vao.get());
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBindVertexArray.xhtml
void gl::BindVertexArray()
{
    BindVertexArray(static_cast<GLuint>(0));
}

gl::Texture::Texture()
{
    GenTextures(1, &m_TextureHandle);
    if (m_TextureHandle == Texture::EmptyHandle()) {
        throw OpenGlException{GL_SOURCELOC "glGenTextures() failed: this could mean that your GPU/system is out of memory, or that your OpenGL driver is invalid in some way"};
    }
}

gl::Texture::Texture(Texture&& tmp) noexcept :
    m_TextureHandle{std::exchange(tmp.m_TextureHandle, Texture::EmptyHandle())}
{
}

gl::Texture::~Texture() noexcept
{
    if (m_TextureHandle != Texture::EmptyHandle()) {
        DeleteTextures(1, &m_TextureHandle);
    }
}

gl::Texture& gl::Texture::operator=(Texture&& tmp) noexcept
{
    std::swap(m_TextureHandle, tmp.m_TextureHandle);
    return *this;
}

GLuint gl::Texture::release()
{
    return std::exchange(m_TextureHandle, EmptyHandle());
}

std::ostream& gl::operator<<(std::ostream& o, Texture const& t)
{
    return o << "Texture(handle = " << t.get() << ')';
}

void gl::BindTexture(GLenum target, Texture const& texture)
{
    BindTexture(target, texture.get());
}

gl::FrameBuffer::FrameBuffer()
{
    GenFramebuffers(1, &m_FboHandle);
    if (m_FboHandle == EmptyHandle()) {
        throw OpenGlException{GL_SOURCELOC "glGenFramebuffers() failed: this could mean that your GPU/system is out of memory, or that your OpenGL driver is invalid in some way"};
    }
}

gl::FrameBuffer::FrameBuffer(FrameBuffer&& tmp) noexcept :
    m_FboHandle{std::exchange(tmp.m_FboHandle, EmptyHandle())}
{
}

gl::FrameBuffer::~FrameBuffer() noexcept
{
    if (m_FboHandle != EmptyHandle()) {
        DeleteFramebuffers(1, &m_FboHandle);
    }
}

gl::FrameBuffer& gl::FrameBuffer::operator=(FrameBuffer&& tmp) noexcept
{
    std::swap(m_FboHandle, tmp.m_FboHandle);
    return *this;
}

GLuint gl::FrameBuffer::release()
{
    return std::exchange(m_FboHandle, EmptyHandle());
}

std::ostream& gl::operator<<(std::ostream& o, FrameBuffer const& fbo)
{
    return o << "FrameBuffer(" << fbo.get() << ')';
}

// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glBindFramebuffer.xhtml
void gl::BindFramebuffer(GLenum target, FrameBuffer const& fb)
{
    BindFramebuffer(target, fb.get());
}

void gl::BindWindowFramebuffer(GLenum target)
{
    BindFramebuffer(target, 0);
}

void gl::FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, Texture const& tex, GLint level)
{
    FramebufferTexture2D(target, attachment, textarget, tex.get(), level);
}


gl::RenderBuffer::RenderBuffer()
{
    GenRenderbuffers(1, &m_RenderBuffer);
    if (m_RenderBuffer == EmptyHandle()) {
        throw OpenGlException{GL_SOURCELOC "glGenRenderBuffers() failed: this could mean that your GPU/system is out of memory, or that your OpenGL driver is invalid in some way"};
    }
}

gl::RenderBuffer::RenderBuffer(RenderBuffer&& tmp) noexcept :
    m_RenderBuffer{std::exchange(tmp.m_RenderBuffer, EmptyHandle())}
{
}

gl::RenderBuffer::~RenderBuffer() noexcept
{
    if (m_RenderBuffer != EmptyHandle()) {
        DeleteRenderbuffers(1, &m_RenderBuffer);
    }
}

gl::RenderBuffer& gl::RenderBuffer::operator=(RenderBuffer&& tmp) noexcept
{
    std::swap(m_RenderBuffer, tmp.m_RenderBuffer);
    return *this;
}

GLuint gl::RenderBuffer::release()
{
    return std::exchange(m_RenderBuffer, EmptyHandle());
}

// https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glBindRenderbuffer.xml
void gl::BindRenderBuffer(RenderBuffer& rb)
{
    BindRenderbuffer(GL_RENDERBUFFER, rb.get());
}

// https://www.khronos.org/registry/OpenGL-Refpages/es2.0/xhtml/glBindRenderbuffer.xml
void gl::BindRenderBuffer()
{
    BindRenderbuffer(GL_RENDERBUFFER, 0);
}

void gl::FramebufferRenderbuffer(GLenum target, GLenum attachment, RenderBuffer const& rb)
{
    FramebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, rb.get());
}

bool gl::IsCurrentFboComplete()
{
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

int gl::GetInteger(GLenum pname)
{
    GLint out;
    glGetIntegerv(pname, &out);
    return out;
}

GLenum gl::GetEnum(GLenum pname)
{
    return static_cast<GLenum>(GetInteger(pname));
}


