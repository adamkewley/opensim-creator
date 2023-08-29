#pragma once

#include "oscar/Graphics/ShaderType.hpp"
#include "oscar/Utils/CopyOnUpdPtr.hpp"
#include "oscar/Utils/CStringView.hpp"

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string_view>

// note: implementation is in `GraphicsImplementation.cpp`
namespace osc
{
    // a handle to a shader
    class Shader final {
    public:
        // throws on compile error
        Shader(
            CStringView vertexShader,
            CStringView fragmentShader
        );

        // throws on compile error
        Shader(
            CStringView vertexShader,
            CStringView geometryShader,
            CStringView fragmentShader
        );
        Shader(Shader const&);
        Shader(Shader&&) noexcept;
        Shader& operator=(Shader const&);
        Shader& operator=(Shader&&) noexcept;
        ~Shader() noexcept;

        size_t getPropertyCount() const;
        std::optional<ptrdiff_t> findPropertyIndex(std::string_view propertyName) const;
        std::string_view getPropertyName(ptrdiff_t) const;
        ShaderType getPropertyType(ptrdiff_t) const;

        friend void swap(Shader& a, Shader& b) noexcept
        {
            swap(a.m_Impl, b.m_Impl);
        }

    private:
        friend class GraphicsBackend;
        friend bool operator==(Shader const&, Shader const&) noexcept;
        friend bool operator!=(Shader const&, Shader const&) noexcept;
        friend std::ostream& operator<<(std::ostream&, Shader const&);

        class Impl;
        CopyOnUpdPtr<Impl> m_Impl;
    };

    inline bool operator==(Shader const& a, Shader const& b) noexcept
    {
        return a.m_Impl == b.m_Impl;
    }

    inline bool operator!=(Shader const& a, Shader const& b) noexcept
    {
        return a.m_Impl != b.m_Impl;
    }

    std::ostream& operator<<(std::ostream&, Shader const&);
}
