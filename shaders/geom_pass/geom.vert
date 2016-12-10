#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexcoord;

layout (std140, set = 0, binding = 0) uniform UBO 
{
	mat4 VP;
};

layout (std140, set = 0, binding = 1) uniform UBO_per_model
{
	mat4 M;
	mat4 M_invTrans;
};

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outWorldNormal;
layout (location = 2) out vec2 outTexcoord;

out gl_PerVertex
{
	vec4 gl_Position;
};


void main() 
{
	gl_Position = VP * M * vec4(inPosition, 1.0);
	
	outWorldPos = vec3(M * vec4(inPosition, 1.0));
	outWorldNormal = normalize(vec3(M_invTrans * vec4(inNormal, 0.0)));
	outTexcoord = inTexcoord;
}