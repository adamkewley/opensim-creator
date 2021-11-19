#include "InstancedSolidColorShader.hpp"

static char const g_VertexShader[] = R"(
    #version 330 core

    uniform mat4 uVP;

    layout (location = 0) in vec3 aPos;
    layout (location = 6) in mat4x3 aModelMat;

    void main() {
        mat4 modelMat = mat4(vec4(aModelMat[0], 0), vec4(aModelMat[1], 0), vec4(aModelMat[2], 0), vec4(aModelMat[3], 1));
        gl_Position = uVP * modelMat * vec4(aPos, 1.0);
    }
)";

static char const g_FragmentShader[] = R"(
    #version 330 core

    uniform vec4 uColor;

    out vec4 FragColor;

    void main() {
        FragColor = uColor;
    }
)";

osc::InstancedSolidColorShader::InstancedSolidColorShader() :
    program{
        gl::Shader{GL_VERTEX_SHADER, g_VertexShader},
        gl::Shader{GL_FRAGMENT_SHADER, g_FragmentShader}
    },
    uVP{program, "uVP"},
    uColor{program, "uColor"}
{
}
