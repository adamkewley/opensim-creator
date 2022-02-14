#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Shader.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

namespace osc {
    // A specialized edge-detection shader for rim highlighting
    struct EdgeDetectionShader final : public Shader {
        gl::Program program;

        static constexpr gl::Attribute<gl::ShaderType::Vec3> aPos = SHADER_LOC_VERTEX_POSITION;
        static constexpr gl::Attribute<gl::ShaderType::Vec2> aTexCoord = SHADER_LOC_VERTEX_TEXCOORD01;

        gl::Uniform<gl::ShaderType::Mat4> uMVP;
        gl::Uniform<gl::ShaderType::Sampler2D> uSampler0;
        gl::Uniform<gl::ShaderType::Vec4> uRimRgba;
        gl::Uniform<gl::ShaderType::Float> uRimThickness;

        EdgeDetectionShader();
    };
}
