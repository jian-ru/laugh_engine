#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexcoord;


out gl_PerVertex
{
	vec4 gl_Position;
};


void main() 
{
	gl_Position = vec4(inPosition, 1.0);
}