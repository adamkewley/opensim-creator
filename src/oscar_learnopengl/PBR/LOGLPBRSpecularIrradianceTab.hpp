#pragma once

#include <oscar/UI/Tabs/Tab.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <oscar/Utils/UID.hpp>
#include <SDL_events.h>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { class TabHost; }

namespace osc
{
    class LOGLPBRSpecularIrradianceTab final : public Tab {
    public:
        static CStringView id();

        explicit LOGLPBRSpecularIrradianceTab(ParentPtr<TabHost> const&);
        LOGLPBRSpecularIrradianceTab(LOGLPBRSpecularIrradianceTab const&) = delete;
        LOGLPBRSpecularIrradianceTab(LOGLPBRSpecularIrradianceTab&&) noexcept;
        LOGLPBRSpecularIrradianceTab& operator=(LOGLPBRSpecularIrradianceTab const&) = delete;
        LOGLPBRSpecularIrradianceTab& operator=(LOGLPBRSpecularIrradianceTab&&) noexcept;
        ~LOGLPBRSpecularIrradianceTab() noexcept override;

    private:
        UID implGetID() const final;
        CStringView implGetName() const final;
        void implOnMount() final;
        void implOnUnmount() final;
        bool implOnEvent(SDL_Event const&) final;
        void implOnTick() final;
        void implOnDrawMainMenu() final;
        void implOnDraw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
