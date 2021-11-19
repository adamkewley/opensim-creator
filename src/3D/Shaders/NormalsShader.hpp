#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Shader.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

namespace osc {
    // uses a geometry shader to render normals as lines
    struct NormalsShader final : public Shader {
        gl::Program program;

        static constexpr gl::Attribute<gl::ShaderType::Vec3> aPos = SHADER_LOC_VERTEX_POSITION;
        static constexpr gl::Attribute<gl::ShaderType::Vec3> aNormal = SHADER_LOC_VERTEX_NORMAL;

        gl::Uniform<gl::ShaderType::Mat4> uModelMat;
        gl::Uniform<gl::ShaderType::Mat4> uViewMat;
        gl::Uniform<gl::ShaderType::Mat4> uProjMat;
        gl::Uniform<gl::ShaderType::Mat4> uNormalMat;

        NormalsShader();
    };
}
