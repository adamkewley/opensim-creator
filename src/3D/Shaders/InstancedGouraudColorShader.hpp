#pragma once

#include "src/3D/Gl.hpp"
#include "src/3D/Shader.hpp"
#include "src/3D/ShaderLocationIndex.hpp"

namespace osc {
    struct InstancedGouraudColorShader final : public Shader {
        gl::Program program;

        // vertex attrs - the thing being instanced
        static constexpr gl::Attribute<gl::ShaderType::Vec3> aPos = SHADER_LOC_VERTEX_POSITION;
        static constexpr gl::Attribute<gl::ShaderType::Vec3> aNormal = SHADER_LOC_VERTEX_NORMAL;

        // instancing attrs - the instances - should be set with relevant divisor etc.
        static constexpr gl::Attribute<gl::ShaderType::Mat4x3> aModelMat = SHADER_LOC_MATRIX_MODEL;
        static constexpr gl::Attribute<gl::ShaderType::Mat3> aNormalMat = SHADER_LOC_MATRIX_NORMAL;
        static constexpr gl::Attribute<gl::ShaderType::Vec4> aDiffuseColor = SHADER_LOC_COLOR_DIFFUSE;

        // uniforms
        gl::Uniform<gl::ShaderType::Mat4> uProjMat;
        gl::Uniform<gl::ShaderType::Mat4> uViewMat;
        gl::Uniform<gl::ShaderType::Vec3> uLightDir;
        gl::Uniform<gl::ShaderType::Vec3> uLightColor;
        gl::Uniform<gl::ShaderType::Vec3> uViewPos;

        InstancedGouraudColorShader();
    };
}
