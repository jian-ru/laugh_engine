#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec2 inTexcoord;

layout (std140, binding = 0) uniform UBO 
{
	mat4 MVP;
	mat4 MV;
	mat4 MV_invTrans;
} ubo;

layout (location = 0) out vec3 outEyePos;
layout (location = 1) out vec3 outEyeNormal;
layout (location = 2) out vec3 outEyeTangent;
layout (location = 3) out vec2 outTexcoord;

out gl_PerVertex
{
	vec4 gl_Position;
};


void main() 
{
	gl_Position = ubo.MVP * vec4(inPosition, 1.0);
	
	outEyePos = vec3(ubo.MV * vec4(inPosition, 1.0));
	outEyeNormal = vec3(ubo.MV_invTrans * vec4(inNormal, 0.0));
	outEyeTangent = vec3(ubo.MV * vec4(inTangent, 0.0));
	outTexcoord = inTexcoord;
}