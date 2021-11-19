#pragma once

#include "src/3D/Gl.hpp"

#include <iosfwd>
#include <string>

namespace osc {

    enum class ShaderQualifier {
        Uniform,
        Attribute,
        InstancedAttribute,
    };

    std::ostream& operator<<(std::ostream&, ShaderQualifier const&);

    class ShaderProperty final {
    public:
        ShaderProperty(std::string name,
                       gl::ShaderType type,
                       ShaderQualifier qualifier,
                       GLint location);

        std::string const& getName() const { return m_Name; }
        gl::ShaderType getType() const { return m_Type; }
        ShaderQualifier getQualifier() const { return m_Qualifier; }
        GLint getLocation() const { return m_Location; }

    private:
        std::string m_Name;
        gl::ShaderType m_Type;
        ShaderQualifier m_Qualifier;
        GLint m_Location;
    };

    std::ostream& operator<<(std::ostream&, ShaderProperty const&);

    class Shader {
    public:
        virtual int FindPropertyIndex(std::string const&) const = 0;
        virtual int GetPropertyCount() const = 0;
        virtual std::string const& GetPropertyName(int) const = 0;
        virtual gl::ShaderType GetPropertyType(int) const = 0;
        virtual ShaderQualifier GetPropertyQualifier(int) const = 0;
        virtual GLint GetPropertyLocation() const = 0;

        virtual ~Shader() noexcept = default;
    };
}
