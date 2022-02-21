#include "HelloTriangleScreen.hpp"

#include "src/3D/Renderer.hpp"
#include "src/Assertions.hpp"
#include "src/App.hpp"

#include <glm/vec3.hpp>

#include <vector>
#include <numeric>
#include <type_traits>


static constexpr char const g_ShaderSrc[] = R"(
    #version 330 core

    ---vertex shader---

    in vec3 aPos;

    void main()
    {
        gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
    }

    ---fragment shader---

    out vec4 FragColor;
    uniform vec4 uColor;

    void main()
    {
        FragColor = uColor;
    }
)";

static glm::vec3 const g_TriangleData[] =
{
    {-1.0f, -1.0f, 0.0f},
    {+1.0f, -1.0f, 0.0f},
    {+0.0f, +1.0f, 0.0f},
};

class osc::HelloTriangleScreen::Impl final {
public:
    Impl()
    {
        m_Camera.setCameraProjection(CameraProjection_Orthographic);
        m_Camera.setBackgroundColor({1.0f, 1.0f, 1.0f, 1.0f});
        m_Camera.setPosition({ 0.0f, 0.0f, 1.0f });
        m_Camera.setDirection({ 0.0f, 0.0f, -1.0f});  // points down
        m_Camera.setOrthographicSize(2.0f);  // triangle height
    }

    void onEvent(SDL_Event const& e)
    {
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        {
            // TODO: transition back to experiments screen
        }
    }

    void tick(float dt)
    {
        if (m_Color.r < 0.0f || m_Color.r > 1.0f)
        {
            m_FadeSpeed = -m_FadeSpeed;
        }

        m_Color.r -= dt * m_FadeSpeed;
    }

    void draw()
    {
        m_MaterialProps.setColor(m_Color);
        Graphics::DrawMesh(m_Mesh, {0.0f, 0.0f, 0.0f}, m_Camera, &m_MaterialProps);
    }

private:
    Material m_Material{Shader::compile(g_ShaderSrc)};
    Mesh m_Mesh{MeshTopographyNew_Triangles, g_TriangleData};
    CameraNew m_Camera;
    MaterialPropertyBlock m_MaterialProps;
    float m_FadeSpeed = 1.0f;
    glm::vec4 m_Color = {1.0f, 0.0f, 0.0f, 1.0f};
};

// public API

osc::HelloTriangleScreen::HelloTriangleScreen() :
    m_Impl{new Impl{}}
{
}

osc::HelloTriangleScreen::~HelloTriangleScreen() noexcept = default;

void osc::HelloTriangleScreen::onEvent(SDL_Event const& e)
{
    m_Impl->onEvent(e);
}

void osc::HelloTriangleScreen::tick(float dt)
{
    m_Impl->tick(dt);
}

void osc::HelloTriangleScreen::draw()
{
    m_Impl->draw();
}
