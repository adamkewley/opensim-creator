#pragma once

#include "src/Screen.hpp"

#include <SDL_events.h>

#include <memory>

namespace osc
{
    // EXPERIMENT: get new backend renderer working
    class RendererScreen final : public Screen {
    public:
        RendererScreen();
        ~RendererScreen() noexcept override;

        void onEvent(SDL_Event const&) override;
        void tick(float) override;
        void draw() override;

        class Impl;
    private:
        std::unique_ptr<Impl> m_Impl;
    };
}
