#include "ColormappedPlainTextureShader.hpp"

static char const g_VertexShader[] = R"(
    #version 330 core

    uniform mat4 uMVP;

    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 texCoord;

    void main(void) {
        gl_Position = uMVP * vec4(aPos, 1.0f);
        texCoord = aTexCoord;
    }
)";

static char const g_FragmentShader[] = R"(
    #version 330 core

    uniform sampler2D uSamplerAlbedo;
    uniform mat4 uSamplerMultiplier = mat4(1.0);

    in vec2 texCoord;

    out vec4 fragColor;

    void main(void) {
        fragColor = uSamplerMultiplier * texture(uSamplerAlbedo, texCoord);
    }
)";

osc::ColormappedPlainTextureShader::ColormappedPlainTextureShader() :
    program{
        gl::Shader{GL_VERTEX_SHADER, g_VertexShader},
        gl::Shader{GL_FRAGMENT_SHADER, g_FragmentShader}
    },
    uMVP{program, "uMVP"},
    uSamplerAlbedo{program, "uSamplerAlbedo"},
    uSamplerMultiplier{program, "uSamplerMultiplier"}
{
}
