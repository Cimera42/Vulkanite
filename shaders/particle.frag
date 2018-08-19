#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 worldPos;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outColour;
layout(location = 2) out vec4 outNormal;

layout(binding = 0) uniform sampler2D texSampler;

void main()
{
    outColour = texture(texSampler, fragUV);

    outPosition = vec4(worldPos, 1);
    outNormal = vec4(fragNormal, 1);
}