#include "SolidColorShader.hpp"

static char const g_VertexShader[] = R"(
    #version 330 core

    uniform mat4 uProjMat;
    uniform mat4 uViewMat;
    uniform mat4 uModelMat;

    layout (location = 0) in vec3 aPos;

    void main() {
        gl_Position = uProjMat * uViewMat * uModelMat * vec4(aPos, 1.0);
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

osc::SolidColorShader::SolidColorShader() :
    program{
        gl::Shader{GL_VERTEX_SHADER, g_VertexShader},
        gl::Shader{GL_FRAGMENT_SHADER, g_FragmentShader}
    },
    uModel{program, "uModelMat"},
    uView{program, "uViewMat"},
    uProjection{program, "uProjMat"},
    uColor{program, "uColor"}
{
}
