#pragma once

#include "src/Graphics/ShaderType.hpp"
#include "src/Utils/Cow.hpp"
#include "src/Utils/CStringView.hpp"

#include <iosfwd>
#include <optional>
#include <string>

// note: implementation is in `GraphicsImplementation.cpp`
namespace osc
{
    // a handle to a shader
    class Shader final {
    public:
        Shader(CStringView vertexShader, CStringView fragmentShader);  // throws on compile error
        Shader(CStringView vertexShader, CStringView geometryShader, CStringView fragmmentShader);  // throws on compile error
        Shader(Shader const&);
        Shader(Shader&&) noexcept;
        Shader& operator=(Shader const&);
        Shader& operator=(Shader&&) noexcept;
        ~Shader() noexcept;

        std::optional<int> findPropertyIndex(std::string const& propertyName) const;

        int getPropertyCount() const;
        std::string const& getPropertyName(int propertyIndex) const;
        ShaderType getPropertyType(int propertyIndex) const;

        friend void swap(Shader& a, Shader& b) noexcept
        {
            swap(a.m_Impl, b.m_Impl);
        }

    private:
        friend class GraphicsBackend;
        friend bool operator==(Shader const&, Shader const&) noexcept;
        friend bool operator!=(Shader const&, Shader const&) noexcept;
        friend bool operator<(Shader const&, Shader const&) noexcept;
        friend std::ostream& operator<<(std::ostream&, Shader const&);

        class Impl;
        Cow<Impl> m_Impl;
    };

    inline bool operator==(Shader const& a, Shader const& b) noexcept
    {
        return a.m_Impl == b.m_Impl;
    }

    inline bool operator!=(Shader const& a, Shader const& b) noexcept
    {
        return a.m_Impl != b.m_Impl;
    }

    inline bool operator<(Shader const& a, Shader const& b) noexcept
    {
        return a.m_Impl < b.m_Impl;
    }

    std::ostream& operator<<(std::ostream&, Shader const&);
}