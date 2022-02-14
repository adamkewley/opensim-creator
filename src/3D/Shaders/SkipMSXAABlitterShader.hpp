#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Shader.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

namespace osc {
    // designed to blit the first sample from a multisampled texture source
    struct SkipMSXAABlitterShader final : public Shader {
        gl::Program program;

        static constexpr gl::Attribute<gl::ShaderType::Vec3> aPos = SHADER_LOC_VERTEX_POSITION;
        static constexpr gl::Attribute<gl::ShaderType::Vec2> aTexCoord = SHADER_LOC_VERTEX_TEXCOORD01;

        gl::Uniform<gl::ShaderType::Mat4> uModelMat;
        gl::Uniform<gl::ShaderType::Mat4> uViewMat;
        gl::Uniform<gl::ShaderType::Mat4> uProjMat;
        gl::Uniform<gl::ShaderType::Sampler2DMS> uSampler0;

        SkipMSXAABlitterShader();
    };
}
