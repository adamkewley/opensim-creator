#pragma once

#include "src/screen.hpp"

#include <memory>

namespace osc {
    class Mesh_hittest_with_bvh_screen final : public Screen {
    public:
        struct Impl;
    private:
        std::unique_ptr<Impl> m_Impl;

    public:
        Mesh_hittest_with_bvh_screen();
        ~Mesh_hittest_with_bvh_screen() noexcept override;

        void on_mount() override;
        void on_unmount() override;
        void on_event(SDL_Event const&) override;
        void tick(float) override;
        void draw() override;
    };
}
