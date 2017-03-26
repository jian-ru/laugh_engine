#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inPosition;

layout (std140, set = 0, binding = 0) uniform UBO 
{
	mat4 cascadeVP;
};

layout (std140, set = 1, binding = 1) uniform UBO_per_model
{
	mat4 M;
	mat4 M_invTrans;
};

out gl_PerVertex
{
	vec4 gl_Position;
};


void main() 
{
	gl_Position = cascadeVP * (M * vec4(inPosition, 1.0));
}