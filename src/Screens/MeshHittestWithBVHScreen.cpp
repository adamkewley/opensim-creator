#include "MeshHittestWithBVHScreen.hpp"

#include "src/Bindings/SimTKHelpers.hpp"
#include "src/Bindings/ImGuiHelpers.hpp"
#include "src/Graphics/Shaders/SolidColorShader.hpp"
#include "src/Graphics/Gl.hpp"
#include "src/Graphics/GlGlm.hpp"
#include "src/Graphics/Mesh.hpp"
#include "src/Graphics/MeshData.hpp"
#include "src/Graphics/MeshGen.hpp"
#include "src/Maths/BVH.hpp"
#include "src/Maths/Geometry.hpp"
#include "src/Maths/PolarPerspectiveCamera.hpp"
#include "src/Platform/App.hpp"
#include "src/Screens/ExperimentsScreen.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <imgui.h>

#include <cstdint>
#include <chrono>
#include <vector>

static gl::VertexArray makeVAO(osc::SolidColorShader& shader,
                               gl::ArrayBuffer<glm::vec3>& vbo,
                               gl::ElementArrayBuffer<uint32_t>& ebo)
{
    gl::VertexArray rv;
    gl::BindVertexArray(rv);
    gl::BindBuffer(vbo);
    gl::BindBuffer(ebo);
    gl::VertexAttribPointer(shader.aPos, false, sizeof(glm::vec3), 0);
    gl::EnableVertexAttribArray(shader.aPos);
    gl::BindVertexArray();
    return rv;
}

// assumes vertex array is set. Only sets uModel and draws each frame
static void drawBVHRecursive(osc::BVH const& bvh, osc::SolidColorShader& shader, int pos)
{
    osc::BVHNode const& n = bvh.nodes[pos];

    glm::vec3 halfWidths = Dimensions(n.bounds) / 2.0f;
    glm::vec3 center = Midpoint(n.bounds);

    glm::mat4 scaler = glm::scale(glm::mat4{1.0f}, halfWidths);
    glm::mat4 mover = glm::translate(glm::mat4{1.0f}, center);
    glm::mat4 mmtx = mover * scaler;

    gl::Uniform(shader.uModel, mmtx);
    gl::DrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);

    if (n.nlhs >= 0)
    {
        // if it's an internal node
        drawBVHRecursive(bvh, shader, pos+1);
        drawBVHRecursive(bvh, shader, pos+n.nlhs+1);
    }
}

struct osc::MeshHittestWithBVHScreen::Impl final {
    SolidColorShader shader;

    Mesh mesh = LoadMeshViaSimTK(App::resource("geometry/hat_ribs.vtp"));

    // triangle (debug)
    glm::vec3 tris[3];
    gl::ArrayBuffer<glm::vec3> triangleVBO;
    gl::ElementArrayBuffer<uint32_t> triangleEBO = {0, 1, 2};
    gl::VertexArray triangleVAO = makeVAO(shader, triangleVBO, triangleEBO);

    // AABB wireframe
    MeshData cubeWireframe = GenCubeLines();
    gl::ArrayBuffer<glm::vec3> cubeWireframeVBO{cubeWireframe.verts};
    gl::ElementArrayBuffer<uint32_t> cubeWireframeEBO{cubeWireframe.indices};
    gl::VertexArray cubeVAO = makeVAO(shader, cubeWireframeVBO, cubeWireframeEBO);

    std::chrono::microseconds raycastDuration{0};
    PolarPerspectiveCamera camera;
    bool isMousedOver = false;
    bool useBVH = true;
};

// public Impl

osc::MeshHittestWithBVHScreen::MeshHittestWithBVHScreen() :
    m_Impl{new Impl{}}
{
}

osc::MeshHittestWithBVHScreen::~MeshHittestWithBVHScreen() noexcept = default;

void osc::MeshHittestWithBVHScreen::onMount()
{
    osc::ImGuiInit();
    App::cur().disableVsync();
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void osc::MeshHittestWithBVHScreen::onUnmount()
{
    osc::ImGuiShutdown();
}

void osc::MeshHittestWithBVHScreen::onEvent(SDL_Event const& e)
{
    if (e.type == SDL_QUIT)
    {
        App::cur().requestQuit();
        return;
    }
    else if (osc::ImGuiOnEvent(e))
    {
        return;
    }
    else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
    {
        App::cur().requestTransition<ExperimentsScreen>();
        return;
    }
}

void osc::MeshHittestWithBVHScreen::tick(float)
{
    Impl& impl = *m_Impl;

    UpdatePolarCameraFromImGuiUserInput(App::cur().dims(), impl.camera);

    impl.camera.radius *= 1.0f - ImGui::GetIO().MouseWheel/10.0f;

    // handle hittest
    auto raycastStart = std::chrono::high_resolution_clock::now();
    {
        Line cameraRayWorldspace = impl.camera.unprojectTopLeftPosToWorldRay(ImGui::GetMousePos(), App::cur().dims());
        // camera ray in worldspace == camera ray in model space because the model matrix is an identity matrix

        impl.isMousedOver = false;

        if (impl.useBVH)
        {
            BVHCollision res;
            if (BVH_GetClosestRayTriangleCollision(impl.mesh.getTriangleBVH(),
                                                   impl.mesh.getVerts().data(),
                                                   impl.mesh.getVerts().size(),
                                                   cameraRayWorldspace, &res))
            {
                glm::vec3 const* v = impl.mesh.getVerts().data() + res.primId;
                impl.isMousedOver = true;
                impl.tris[0] = v[0];
                impl.tris[1] = v[1];
                impl.tris[2] = v[2];
                impl.triangleVBO.assign(impl.tris, 3);
            }
        }
        else
        {
            nonstd::span<glm::vec3 const> verts = impl.mesh.getVerts();
            for (size_t i = 0; i < verts.size(); i += 3)
            {
                glm::vec3 tri[3] = {verts[i], verts[i+1], verts[i+2]};
                RayCollision res = GetRayCollisionTriangle(cameraRayWorldspace, tri);
                if (res.hit)
                {
                    impl.isMousedOver = true;

                    // draw triangle for hit
                    impl.tris[0] = tri[0];
                    impl.tris[1] = tri[1];
                    impl.tris[2] = tri[2];
                    impl.triangleVBO.assign(impl.tris, 3);
                    break;
                }
            }
        }

    }
    auto raycastEnd = std::chrono::high_resolution_clock::now();
    auto raycastDt = raycastEnd - raycastStart;
    impl.raycastDuration = std::chrono::duration_cast<std::chrono::microseconds>(raycastDt);
}

void osc::MeshHittestWithBVHScreen::draw()
{
    auto dims = App::cur().idims();
    gl::Viewport(0, 0, dims.x, dims.y);

    osc::ImGuiNewFrame();

    Impl& impl = *m_Impl;
    SolidColorShader& shader = impl.shader;

    gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // printout stats
    {
        ImGui::Begin("controls");
        ImGui::Text("raycast duration = %lld micros", impl.raycastDuration.count());
        ImGui::Checkbox("use BVH", &impl.useBVH);
        ImGui::End();
    }

    gl::ClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl::UseProgram(shader.program);
    gl::Uniform(shader.uModel, gl::identity);
    gl::Uniform(shader.uView, impl.camera.getViewMtx());
    gl::Uniform(shader.uProjection, impl.camera.getProjMtx(App::cur().aspectRatio()));
    gl::Uniform(shader.uColor, impl.isMousedOver ? glm::vec4{0.0f, 1.0f, 0.0f, 1.0f} : glm::vec4{1.0f, 0.0f, 0.0f, 1.0f});

    // draw scene
    if (true)
    {
        gl::BindVertexArray(impl.mesh.GetVertexArray());
        impl.mesh.Draw();
        gl::BindVertexArray();
    }

    // draw hittest triangle debug
    if (impl.isMousedOver)
    {
        gl::Disable(GL_DEPTH_TEST);

        // draw triangle
        gl::Uniform(shader.uModel, gl::identity);
        gl::Uniform(shader.uColor, {0.0f, 0.0f, 0.0f, 1.0f});
        gl::BindVertexArray(impl.triangleVAO);
        gl::DrawElements(GL_TRIANGLES, impl.triangleEBO.sizei(), gl::indexType(impl.triangleEBO), nullptr);
        gl::BindVertexArray();

        gl::Enable(GL_DEPTH_TEST);
    }

    // draw BVH
    if (impl.useBVH && !impl.mesh.getTriangleBVH().nodes.empty())
    {
        // uModel is set by the recursive call
        gl::Uniform(shader.uColor, {0.0f, 0.0f, 0.0f, 1.0f});
        gl::BindVertexArray(impl.cubeVAO);
        drawBVHRecursive(impl.mesh.getTriangleBVH(), impl.shader, 0);
        gl::BindVertexArray();
    }

    osc::ImGuiRender();
}
