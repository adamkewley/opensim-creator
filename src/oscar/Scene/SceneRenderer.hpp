#pragma once

#include <oscar/Graphics/AntiAliasingLevel.hpp>
#include <oscar/Maths/Vec2.hpp>

#include <memory>
#include <span>

namespace osc { class AppConfig; }
namespace osc { class ShaderCache; }
namespace osc { struct SceneDecoration; }
namespace osc { class SceneCache; }
namespace osc { struct SceneRendererParams; }
namespace osc { class RenderTexture; }

namespace osc
{
    class SceneRenderer final {
    public:
        SceneRenderer(AppConfig const&, SceneCache&, ShaderCache&);
        SceneRenderer(SceneRenderer const&);
        SceneRenderer(SceneRenderer&&) noexcept;
        SceneRenderer& operator=(SceneRenderer const&) = delete;
        SceneRenderer& operator=(SceneRenderer&&) noexcept;
        ~SceneRenderer() noexcept;

        Vec2i getDimensions() const;
        AntiAliasingLevel getAntiAliasingLevel() const;
        void render(std::span<SceneDecoration const>, SceneRendererParams const&);
        RenderTexture& updRenderTexture();

    private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
