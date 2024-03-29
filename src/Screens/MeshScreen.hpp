#pragma once

#include "src/Platform/Screen.hpp"

#include <SDL_events.h>

#include <memory>

namespace osc
{
    // screen that displays a mesh (to confirm the `Mesh` abstraction works)
    class MeshScreen final : public Screen {
    public:
        MeshScreen();
        ~MeshScreen() noexcept override;

        void onMount() override;
        void onUnmount() override;
        void onEvent(SDL_Event const&) override;
        void tick(float) override;
        void draw() override;

        struct Impl;
    private:
        std::unique_ptr<Impl> m_Impl;
    };
}
