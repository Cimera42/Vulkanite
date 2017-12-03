#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragUV;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(push_constant) uniform PushConstantBufferObject {
    mat4 view;
    mat4 proj;
} pcbo;

void main()
{
    gl_Position = pcbo.proj * pcbo.view * vec4(inPosition, 1.0);
    fragUV = inPosition*vec3(1,1,-1);
}