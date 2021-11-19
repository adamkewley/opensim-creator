#pragma once

#include "src/3D/Gl.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <type_traits>

// gl_glm: extensions for using glm types in OpenGL
namespace gl {

    inline void UniformMatrix3fv(Uniform<ShaderType::Mat3>u, glm::mat3 const& mat) noexcept
    {
        gl::UniformMatrix3fv(u.geti(), 1, false, glm::value_ptr(mat));
    }

    inline void Uniform4fv(Uniform<ShaderType::Vec4>& u, glm::vec4 const& v) noexcept
    {
        gl::Uniform4fv(u.geti(), 1, glm::value_ptr(v));
    }

    inline void Uniform3fv(Uniform<ShaderType::Vec3>& u, glm::vec3 const& v) noexcept
    {
        gl::Uniform3fv(u.geti(), 1, glm::value_ptr(v));
    }

    inline void Uniform3fv(Uniform<ShaderType::Vec3>u, GLsizei n, glm::vec3 const* vs) noexcept
    {
        static_assert(sizeof(glm::vec3) == 3 * sizeof(GLfloat));
        gl::Uniform3fv(u.geti(), n, glm::value_ptr(*vs));
    }

    inline void UniformMatrix4fv(Uniform<ShaderType::Mat4>& u, glm::mat4 const& mat) noexcept
    {
        gl::UniformMatrix4fv(u.geti(), 1, false, glm::value_ptr(mat));
    }

    inline void UniformMatrix4fv(Uniform<ShaderType::Mat4>& u, GLsizei n, glm::mat4 const* first) noexcept
    {
        static_assert(sizeof(glm::mat4) == 16 * sizeof(GLfloat));
        gl::UniformMatrix4fv(u.geti(), n, false, glm::value_ptr(*first));
    }

    inline void Uniform2fv(Uniform<ShaderType::Vec2>& u, glm::vec2 const& v) noexcept
    {
        gl::Uniform2fv(u.geti(), 1, glm::value_ptr(v));
    }

    inline void Uniform2fv(Uniform<ShaderType::Vec2>& u, GLsizei n, glm::vec2 const* vs) noexcept
    {
        static_assert(sizeof(glm::vec2) == 2 * sizeof(GLfloat));
        gl::Uniform2fv(u.geti(), n, glm::value_ptr(*vs));
    }

    inline void ClearColor(glm::vec4 const& v) noexcept
    {
        ClearColor(v[0], v[1], v[2], v[3]);
    }
}
