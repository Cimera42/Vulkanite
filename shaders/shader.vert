#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 fragUV;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
} ubo;

layout(push_constant) uniform PushConstantBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} pcbo;

void main()
{
    gl_Position = pcbo.proj * pcbo.view * pcbo.model * vec4(inPosition, 1.0);
    fragUV = inUV;
}