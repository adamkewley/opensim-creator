#include "Shader.hpp"

std::ostream& osc::operator<<(std::ostream&, ShaderQualifier const&)
{

}

osc::ShaderProperty::ShaderProperty(std::string name,
                                    gl::ShaderType type,
                                    ShaderQualifier qualifier,
                                    GLint location) :
    m_Name{std::move(name)},
    m_Type{std::move(type)},
    m_Qualifier{std::move(qualifier)},
    m_Location{std::move(location)}
{
}

std::ostream& osc::operator<<(std::ostream&, ShaderProperty const&)
{

}




/*

// describes some element of a buffer, used to handle glVertexAttribPointer calls
class BufferElementDescription final {
public:
    BufferElementDescription(GLint shaderAttributeLocation_,
                             ShaderType shaderType_,
                             GLenum bufferDataFormat_,
                             bool isNormalized_,
                             size_t offset_) :

        shaderAttributeLocation{shaderAttributeLocation_},
        shaderGlslType{shaderType_},
        bufferDataFormat{bufferDataFormat_},
        isNormalized{isNormalized_},
        offset{offset_}
    {
    }

    template<ShaderType ShaderType>
    BufferElementDescription(Attribute<ShaderType> attr_,
                             GLenum bufferDataFormat_,
                             bool isNormalized_,
                             size_t offset_) :
        shaderAttributeLocation{attr_.geti()},
        shaderGlslType{attr_.type()},
        bufferDataFormat{bufferDataFormat_},
        isNormalized{isNormalized_},
        offset{offset_}
    {
    }

    // returned by `GetAttribLocation` or specified in the shader itself
    GLint shaderAttributeLocation;

    // type of the data as declared in the shader (e.g. `vec2`, `float`)
    //
    // this dictates the number of components pointed to by the location. Matrices
    // have special treatment (the location is treated a `n` contiguous shader locations)
    ShaderType shaderGlslType;

    // type of the data as it was packed into the bound array buffer
    //
    // this doesn't *necessarily* match the datatype of the glsl type because
    // you might (e.g.) bind normalized integer *data* to shader `float`s
    GLenum bufferDataFormat;

    // if the data specified with the (above) format is normalized
    bool isNormalized;

    // offset of the element in the provided buffer pointer
    size_t offset;
};
void VertexAttribPointer(BufferElementDescription const&, size_t stride);

void gl::VertexAttribPointer(BufferElementDescription const& desc, size_t stride)
{
    int numShaderSlots = GetNumShaderLocationsTakenBy(desc.shaderGlslType);
    int elsPerSlot = GetNumElementsPerLocation(desc.shaderGlslType);

    GLboolean normGl = desc.isNormalized ? GL_TRUE : GL_FALSE;
    GLsizei strideGl = static_cast<GLsizei>(stride);

    size_t typeSize = TypeSize(desc.bufferDataFormat);

    for (int slot = 0; slot < numShaderSlots; ++slot) {
        size_t offset = desc.offset + (slot * elsPerSlot * typeSize);
        VertexAttribPointer(desc.shaderAttributeLocation + slot, elsPerSlot, desc.bufferDataFormat, normGl, strideGl, offset);
    }
}

*/
