#pragma once

#include <oscar/UI/Tabs/ITab.h>
#include <oscar/Utils/CStringView.h>
#include <oscar/Utils/UID.h>

#include <memory>

namespace osc { template<typename T> class ParentPtr; }
namespace osc { class ITabHost; }

namespace osc
{
    class MeshHittestTab final : public ITab {
    public:
        static CStringView id();

        explicit MeshHittestTab(ParentPtr<ITabHost> const&);
        MeshHittestTab(MeshHittestTab const&) = delete;
        MeshHittestTab(MeshHittestTab&&) noexcept;
        MeshHittestTab& operator=(MeshHittestTab const&) = delete;
        MeshHittestTab& operator=(MeshHittestTab&&) noexcept;
        ~MeshHittestTab() noexcept override;

    private:
        UID impl_get_id() const final;
        CStringView impl_get_name() const final;
        void impl_on_tick() final;
        void impl_on_draw() final;

        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
