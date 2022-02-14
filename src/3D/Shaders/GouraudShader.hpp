#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Shader.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

namespace osc {

    struct GouraudShader final : public Shader {
        gl::Program program;

        static constexpr gl::Attribute<gl::ShaderType::Vec3> aPos = SHADER_LOC_VERTEX_POSITION;
        static constexpr gl::Attribute<gl::ShaderType::Vec2> aTexCoord = SHADER_LOC_VERTEX_TEXCOORD01;
        static constexpr gl::Attribute<gl::ShaderType::Vec3> aNormal = SHADER_LOC_VERTEX_NORMAL;

        // uniforms
        gl::Uniform<gl::ShaderType::Mat4> uProjMat;
        gl::Uniform<gl::ShaderType::Mat4> uViewMat;
        gl::Uniform<gl::ShaderType::Mat4> uModelMat;
        gl::Uniform<gl::ShaderType::Mat3> uNormalMat;
        gl::Uniform<gl::ShaderType::Vec4> uDiffuseColor;
        gl::Uniform<gl::ShaderType::Vec3> uLightDir;
        gl::Uniform<gl::ShaderType::Vec3> uLightColor;
        gl::Uniform<gl::ShaderType::Vec3> uViewPos;
        gl::Uniform<gl::ShaderType::Bool> uIsTextured;
        gl::Uniform<gl::ShaderType::Sampler2D> uSampler0;

        GouraudShader();
    };
}
