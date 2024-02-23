#pragma once

#include <memory>

namespace OpenSim { class Model; }
namespace osc::mow { class Document; }

namespace osc::mow
{
    class CachedModelWarper final {
    public:
        CachedModelWarper();
        CachedModelWarper(CachedModelWarper const&) = delete;
        CachedModelWarper(CachedModelWarper&&) noexcept;
        CachedModelWarper& operator=(CachedModelWarper const&) = delete;
        CachedModelWarper& operator=(CachedModelWarper&&) noexcept;
        ~CachedModelWarper() noexcept;

        std::unique_ptr<OpenSim::Model> warp(Document const&);
    private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}