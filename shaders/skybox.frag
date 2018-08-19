#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragUV;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outColour;
layout(location = 2) out vec4 outNormal;

layout(binding = 0) uniform samplerCube texSampler;

void main()
{
    outColour = texture(texSampler, fragUV);

    outPosition = vec4(0);
    outNormal = vec4(0);
}