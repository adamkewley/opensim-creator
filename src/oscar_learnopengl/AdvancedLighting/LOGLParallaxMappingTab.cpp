#include "LOGLParallaxMappingTab.hpp"

#include <oscar_learnopengl/LearnOpenGLHelpers.hpp>

#include <oscar/Bindings/ImGuiHelpers.hpp>
#include <oscar/Graphics/Camera.hpp>
#include <oscar/Graphics/Color.hpp>
#include <oscar/Graphics/ColorSpace.hpp>
#include <oscar/Graphics/Graphics.hpp>
#include <oscar/Graphics/GraphicsHelpers.hpp>
#include <oscar/Graphics/Material.hpp>
#include <oscar/Graphics/Mesh.hpp>
#include <oscar/Graphics/MeshGenerators.hpp>
#include <oscar/Graphics/MeshIndicesView.hpp>
#include <oscar/Graphics/MeshTopology.hpp>
#include <oscar/Graphics/Shader.hpp>
#include <oscar/Graphics/Texture2D.hpp>
#include <oscar/Maths/MathHelpers.hpp>
#include <oscar/Maths/Transform.hpp>
#include <oscar/Maths/Vec2.hpp>
#include <oscar/Maths/Vec3.hpp>
#include <oscar/Maths/Vec4.hpp>
#include <oscar/Platform/App.hpp>
#include <oscar/UI/Tabs/StandardTabBase.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <SDL_events.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using osc::App;
using osc::Camera;
using osc::ColorSpace;
using osc::CStringView;
using osc::Material;
using osc::Mesh;
using osc::MeshIndicesView;
using osc::MeshTopology;
using osc::Shader;
using osc::Texture2D;
using osc::Vec2;
using osc::Vec3;
using osc::Vec4;

namespace
{
    constexpr CStringView c_TabStringID = "LearnOpenGL/ParallaxMapping";

    // matches the quad used in LearnOpenGL's parallax mapping tutorial
    Mesh GenerateQuad()
    {
        std::vector<Vec3> const verts =
        {
            {-1.0f,  1.0f, 0.0f},
            {-1.0f, -1.0f, 0.0f},
            { 1.0f, -1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f},
        };

        std::vector<Vec3> const normals =
        {
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
        };

        std::vector<Vec2> const texCoords =
        {
            {0.0f, 1.0f},
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
        };

        std::vector<uint16_t> const indices =
        {
            0, 1, 2,
            0, 2, 3,
        };

        std::vector<Vec4> const tangents = osc::CalcTangentVectors(
            MeshTopology::Triangles,
            verts,
            normals,
            texCoords,
            MeshIndicesView{indices}
        );
        OSC_ASSERT_ALWAYS(tangents.size() == verts.size());

        Mesh rv;
        rv.setVerts(verts);
        rv.setNormals(normals);
        rv.setTexCoords(texCoords);
        rv.setTangents(tangents);
        rv.setIndices(indices);
        return rv;
    }

    Camera CreateCamera()
    {
        Camera rv;
        rv.setPosition({0.0f, 0.0f, 3.0f});
        rv.setCameraFOV(osc::Deg2Rad(45.0f));
        rv.setNearClippingPlane(0.1f);
        rv.setFarClippingPlane(100.0f);
        return rv;
    }

    Material CreateParallaxMappingMaterial()
    {
        Texture2D diffuseMap = osc::LoadTexture2DFromImage(
            App::resource("oscar_learnopengl/textures/bricks2.jpg"),
            ColorSpace::sRGB
        );
        Texture2D normalMap = osc::LoadTexture2DFromImage(
            App::resource("oscar_learnopengl/textures/bricks2_normal.jpg"),
            ColorSpace::Linear
        );
        Texture2D displacementMap = osc::LoadTexture2DFromImage(
            App::resource("oscar_learnopengl/textures/bricks2_disp.jpg"),
            ColorSpace::Linear
        );

        Material rv
        {
            Shader
            {
                App::slurp("oscar_learnopengl/shaders/AdvancedLighting/ParallaxMapping.vert"),
                App::slurp("oscar_learnopengl/shaders/AdvancedLighting/ParallaxMapping.frag"),
            },
        };
        rv.setTexture("uDiffuseMap", diffuseMap);
        rv.setTexture("uNormalMap", normalMap);
        rv.setTexture("uDisplacementMap", displacementMap);
        rv.setFloat("uHeightScale", 0.1f);
        return rv;
    }

    Material CreateLightCubeMaterial()
    {
        return Material
        {
            Shader
            {
                App::slurp("oscar_learnopengl/shaders/LightCube.vert"),
                App::slurp("oscar_learnopengl/shaders/LightCube.frag"),
            },
        };
    }
}

class osc::LOGLParallaxMappingTab::Impl final : public osc::StandardTabBase {
public:
    Impl() : StandardTabBase{c_TabStringID}
    {
        m_LightTransform.position = {0.5f, 1.0f, 0.3f};
        m_LightTransform.scale *= 0.2f;
    }

private:
    void implOnMount() final
    {
        m_IsMouseCaptured = true;
    }

    void implOnUnmount() final
    {
        m_IsMouseCaptured = false;
        App::upd().setShowCursor(true);
    }

    bool implOnEvent(SDL_Event const& e) final
    {
        // handle mouse capturing
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        {
            m_IsMouseCaptured = false;
            return true;
        }
        else if (e.type == SDL_MOUSEBUTTONDOWN && osc::IsMouseInMainViewportWorkspaceScreenRect())
        {
            m_IsMouseCaptured = true;
            return true;
        }
        return false;
    }

    void implOnDraw() final
    {
        // handle mouse capturing and update camera
        if (m_IsMouseCaptured)
        {
            UpdateEulerCameraFromImGuiUserInput(m_Camera, m_CameraEulers);
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            App::upd().setShowCursor(false);
        }
        else
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            App::upd().setShowCursor(true);
        }

        // clear screen and ensure camera has correct pixel rect
        App::upd().clearScreen({0.1f, 0.1f, 0.1f, 1.0f});

        // draw normal-mapped quad
        {
            m_ParallaxMappingMaterial.setVec3("uLightWorldPos", m_LightTransform.position);
            m_ParallaxMappingMaterial.setVec3("uViewWorldPos", m_Camera.getPosition());
            m_ParallaxMappingMaterial.setBool("uEnableMapping", m_IsMappingEnabled);
            Graphics::DrawMesh(m_QuadMesh, m_QuadTransform, m_ParallaxMappingMaterial, m_Camera);
        }

        // draw light source cube
        {
            m_LightCubeMaterial.setColor("uLightColor", Color::white());
            Graphics::DrawMesh(m_CubeMesh, m_LightTransform, m_LightCubeMaterial, m_Camera);
        }

        m_Camera.setPixelRect(osc::GetMainViewportWorkspaceScreenRect());
        m_Camera.renderToScreen();

        ImGui::Begin("controls");
        ImGui::Checkbox("normal mapping", &m_IsMappingEnabled);
        ImGui::End();
    }

    // rendering state
    Material m_ParallaxMappingMaterial = CreateParallaxMappingMaterial();
    Material m_LightCubeMaterial = CreateLightCubeMaterial();
    Mesh m_CubeMesh = GenLearnOpenGLCube();
    Mesh m_QuadMesh = GenerateQuad();

    // scene state
    Camera m_Camera = CreateCamera();
    Vec3 m_CameraEulers = {};
    Transform m_QuadTransform;
    Transform m_LightTransform;
    bool m_IsMappingEnabled = true;
    bool m_IsMouseCaptured = false;
};


// public API

CStringView osc::LOGLParallaxMappingTab::id()
{
    return c_TabStringID;
}

osc::LOGLParallaxMappingTab::LOGLParallaxMappingTab(ParentPtr<TabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::LOGLParallaxMappingTab::LOGLParallaxMappingTab(LOGLParallaxMappingTab&&) noexcept = default;
osc::LOGLParallaxMappingTab& osc::LOGLParallaxMappingTab::operator=(LOGLParallaxMappingTab&&) noexcept = default;
osc::LOGLParallaxMappingTab::~LOGLParallaxMappingTab() noexcept = default;

osc::UID osc::LOGLParallaxMappingTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::LOGLParallaxMappingTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::LOGLParallaxMappingTab::implOnMount()
{
    m_Impl->onMount();
}

void osc::LOGLParallaxMappingTab::implOnUnmount()
{
    m_Impl->onUnmount();
}

bool osc::LOGLParallaxMappingTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::LOGLParallaxMappingTab::implOnDraw()
{
    m_Impl->onDraw();
}
