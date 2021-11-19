#pragma once

#include "src/3D/Gl.hpp"

#include <cstddef>
#include <utility>

namespace osc {
    class BufferBindingDescription final {
    public:
        BufferBindingDescription(GLint attributeLocation,
                                 gl::ShaderType shaderType,
                                 GLenum bufferDataFormat,
                                 bool isNormalized,
                                 size_t offset) :
            m_AttributeLocation{std::move(attributeLocation)},
            m_ShaderType{std::move(shaderType)},
            m_BufferDataFormat{std::move(bufferDataFormat)},
            m_IsNormalized{std::move(isNormalized)},
            m_Offset{std::move(offset)}
        {
        }

        GLint getAttributeLocation() const { return m_AttributeLocation; }
        gl::ShaderType getShaderType() const { return m_ShaderType; }
        GLenum getBufferDataFormat() const { return m_BufferDataFormat; }
        bool isNormalized() const { return m_IsNormalized; }
        size_t getOffset() const { return m_Offset; }

    private:
        GLint m_AttributeLocation;
        gl::ShaderType m_ShaderType;
        GLenum m_BufferDataFormat;
        bool m_IsNormalized;
        size_t m_Offset;
    };

    void VertexAttribPointer(BufferBindingDescription const&, size_t stride);
}
