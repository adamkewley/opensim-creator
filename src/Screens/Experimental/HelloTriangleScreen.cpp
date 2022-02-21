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

template<typename T>
static std::vector<T> Create0ToNIndices(size_t n)
{
    static_assert(std::is_integral_v<T>);

    OSC_ASSERT(n >= 0);
    OSC_ASSERT(n <= std::numeric_limits<T>::max());

    std::vector<T> rv;
    rv.reserve(n);

    for (size_t i = 0; i < n; ++i)
    {
        rv.push_back(i);
    }

    return rv;
}

static osc::Mesh CreateBasicTriangleMesh(nonstd::span<glm::vec3 const> verts)
{
    osc::Mesh m;
    m.setVerts(verts);
    m.setIndices(Create0ToNIndices<uint16_t>(verts.size()));
    return m;
}

class osc::HelloTriangleScreen::Impl final {
public:
    Impl()
    {
        m_Camera.setCameraProjection(CameraProjection_Orthographic);
        m_Camera.setBackgroundColor({1.0f, 1.0f, 1.0f, 1.0f});
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
        // TODO: setting camera position
        // TODO: how does orthographic projection work here?
        // TODO: DrawMesh without rotation?

        m_MaterialProps.setColor(m_Color);
        GraphicsBackend::DrawMesh(m_Mesh, {0.0f, 0.0f, 0.0f}, m_Camera, &m_MaterialProps);
    }

private:
    Material m_Material = Material{Shader::compile(g_ShaderSrc)};
    Mesh m_Mesh = CreateBasicTriangleMesh(g_TriangleData);
    CameraNew m_Camera;
    MaterialPropertyBlock m_MaterialProps;
    float m_FadeSpeed = 1.0f;
    glm::vec4 m_Color = {1.0f, 0.0f, 0.0f, 1.0f};
};

// public API

osc::HelloTriangleScreen::HelloTriangleScreen() : m_Impl{new Impl{}}
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
