#include "PlainTextureShader.hpp"

static char const g_VertexShader[] = R"(
    #version 330 core

    uniform mat4 uMVP;
    uniform float uTextureScaler = 1.0;

    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 texCoord;

    void main(void) {
        gl_Position = uMVP * vec4(aPos, 1.0);
        texCoord = uTextureScaler * aTexCoord;
    }
)";

static char const g_FragmentShader[] = R"(
    #version 330 core

    in vec2 texCoord;

    layout (location = 0) out vec4 fragColor;

    uniform sampler2D uSampler0;

    void main(void) {
        fragColor = texture(uSampler0, texCoord);
    }
)";

osc::PlainTextureShader::PlainTextureShader() :
    program{
        gl::Shader{GL_VERTEX_SHADER, g_VertexShader},
        gl::Shader{GL_FRAGMENT_SHADER, g_FragmentShader}
    },
    uMVP{program, "uMVP"},
    uTextureScaler{program, "uTextureScaler"},
    uSampler0{program, "uSampler0"}
{
}
