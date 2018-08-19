#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(location = 3) in mat4 instanceMatrix;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 worldPos;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(push_constant) uniform ParticlePushConstantBufferObject {
    mat4 view;
    mat4 proj;
} pcbo;

void main()
{
    gl_Position = pcbo.proj * pcbo.view * instanceMatrix * vec4(inPosition, 1.0);
    fragUV = inUV;
    fragNormal = inNormal;
    worldPos = (instanceMatrix * vec4(inPosition, 1.0)).xyz;
}