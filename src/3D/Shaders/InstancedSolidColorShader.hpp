#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Shader.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

namespace osc {
    struct InstancedSolidColorShader final : public Shader {
        gl::Program program;

        static constexpr gl::Attribute<gl::ShaderType::Vec3> aPos = SHADER_LOC_VERTEX_POSITION;
        static constexpr gl::Attribute<gl::ShaderType::Mat4x3> aModelMat = SHADER_LOC_MATRIX_MODEL;

        gl::Uniform<gl::ShaderType::Mat4> uVP;
        gl::Uniform<gl::ShaderType::Vec4> uColor;

        InstancedSolidColorShader();
    };
}
