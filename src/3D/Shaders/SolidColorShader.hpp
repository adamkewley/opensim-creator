#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Shader.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

namespace osc {
    struct SolidColorShader final : public Shader {
        gl::Program program;

        static constexpr gl::Attribute<gl::ShaderType::Vec3> aPos = SHADER_LOC_VERTEX_POSITION;

        gl::Uniform<gl::ShaderType::Mat4> uModel;
        gl::Uniform<gl::ShaderType::Mat4> uView;
        gl::Uniform<gl::ShaderType::Mat4> uProjection;
        gl::Uniform<gl::ShaderType::Vec4> uColor;

        SolidColorShader();
    };
}
