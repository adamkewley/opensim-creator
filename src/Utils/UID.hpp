#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>

namespace osc
{
    class UID {
    private:
        static std::atomic<int64_t> g_NextID;

        static inline int64_t GetNextID() noexcept
        {
            return g_NextID.fetch_add(1, std::memory_order_relaxed);
        }

        constexpr UID(int64_t value) : m_Value{std::move(value)}
        {
        }
    public:
        static constexpr UID invalid()
        {
            return UID{-2};
        }

        static constexpr UID empty()
        {
            return UID{-1};
        }

        UID() : m_Value{GetNextID()}
        {
        }

        void reset()
        {
            m_Value = GetNextID();
        }

        constexpr int64_t get() const noexcept
        {
            return m_Value;
        }

    private:
        int64_t m_Value;
    };

    // strongly-typed version of the above
    //
    // adds compile-time type checking to IDs
    template<typename T>
    class UIDT : public UID {
    private:
        UIDT(UID id) : UID{std::move(id)}
        {
        }
    public:
        UIDT() : UID{}
        {
        }

        template<typename U>
        UIDT<U> downcast() const
        {
            return UIDT<U>{*this};
        }
    };

    std::ostream& operator<<(std::ostream& o, UID const& id);

    constexpr bool operator==(UID const& lhs, UID const& rhs) noexcept
    {
        return lhs.get() == rhs.get();
    }

    constexpr bool operator!=(UID const& lhs, UID const& rhs) noexcept
    {
        return lhs.get() != rhs.get();
    }

    constexpr bool operator<(UID const& lhs, UID const& rhs) noexcept
    {
        return lhs.get() < rhs.get();
    }
}

// hashing support for LogicalIDs
//
// lets them be used as associative lookup keys, etc.
namespace std
{
    template<>
    struct hash<osc::UID> {
        size_t operator()(osc::UID const& id) const
        {
            return static_cast<size_t>(id.get());
        }
    };

    template<typename T>
    struct hash<osc::UIDT<T>> {
        size_t operator()(osc::UID const& id) const
        {
            return static_cast<size_t>(id.get());
        }
    };
}
