#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragUV;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform samplerCube texSampler;

void main()
{
    outColour = texture(texSampler, fragUV);
}