#include "HelloTriangleScreen.hpp"

#include "src/App.hpp"
#include "src/3D/Gl.hpp"
#include "src/3D/GlGlm.hpp"
#include "src/Screens/Experimental/ExperimentsScreen.hpp"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

using namespace osc;

static constexpr char const g_VertexShader[] = R"(
    #version 330 core

    in vec3 aPos;

    void main()
    {
        gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
    }
)";

static constexpr char const g_FragmentShader[] = R"(
    #version 330 core

    out vec4 FragColor;
    uniform vec4 uColor;

    void main()
    {
        FragColor = uColor;
    }
)";

namespace {
    struct BasicShader final {
        gl::Program program{
            gl::Shader{GL_VERTEX_SHADER, g_VertexShader},
            gl::Shader{GL_FRAGMENT_SHADER, g_FragmentShader}
        };

        gl::Attribute<gl::ShaderType::Vec3> aPos{program, "aPos"};
        gl::Uniform<gl::ShaderType::Vec4> uColor{program, "uColor"};
    };
}

static gl::VertexArray CreateVAO(BasicShader& shader, gl::Buffer const& points)
{
    gl::BufferBindingDescription description
    {
        shader.aPos.geti(),
        gl::ShaderType::Vec3,
        GL_FLOAT,
        false,
        0
    };

    gl::VertexArray rv;
    gl::BindVertexArray(rv);
    gl::BindBuffer(GL_ARRAY_BUFFER, points);
    VertexAttribPointer(description, sizeof(glm::vec3));
    gl::EnableVertexAttribArray(shader.aPos.get(), shader.aPos.getType());
    gl::BindVertexArray();

    return rv;
}

static glm::vec3 g_TriangleData[] = {
    {-1.0f, -1.0f, 0.0f},
    {+1.0f, -1.0f, 0.0f},
    {+0.0f, +1.0f, 0.0f}
};

struct osc::HelloTriangleScreen::Impl final {
    BasicShader shader;
    gl::SizedBuffer points = gl::CreateSizedBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW, g_TriangleData);
    gl::VertexArray vao = CreateVAO(shader, points);

    float fadeSpeed = 1.0f;
    glm::vec4 color = {1.0f, 0.0f, 0.0f, 1.0f};
};

// public API

osc::HelloTriangleScreen::HelloTriangleScreen() :
    m_Impl{new Impl{}}
{
}

osc::HelloTriangleScreen::~HelloTriangleScreen() noexcept = default;

void osc::HelloTriangleScreen::onEvent(SDL_Event const& e)
{
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
    {
        //App::cur().requestTransition<ExperimentsScreen>();
    }
}

void osc::HelloTriangleScreen::tick(float dt) {

    if (m_Impl->color.r < 0.0f || m_Impl->color.r > 1.0f)
    {
        m_Impl->fadeSpeed = -m_Impl->fadeSpeed;
    }

    m_Impl->color.r -= dt * m_Impl->fadeSpeed;
}

void osc::HelloTriangleScreen::draw()
{
    gl::Viewport(0, 0, App::cur().idims().x, App::cur().idims().y);
    gl::ClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gl::UseProgram(m_Impl->shader.program);
    gl::Uniform4fv(m_Impl->shader.uColor, m_Impl->color);
    gl::BindVertexArray(m_Impl->vao);
    gl::DrawArrays(GL_TRIANGLES, 0, m_Impl->points.numEls());
    gl::BindVertexArray();
}
