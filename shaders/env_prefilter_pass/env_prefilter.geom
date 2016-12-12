#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

layout (std140, set = 0, binding = 0) uniform UBO 
{
	mat4 V[6];
	mat4 P;
};

layout (location = 0) out vec3 outUVW;


void main()
{
	vec4 pos1 = gl_in[0].gl_Position;
	vec4 pos2 = gl_in[1].gl_Position;
	vec4 pos3 = gl_in[2].gl_Position;

	for (int face = 0; face < 6; ++face)
	{
		mat4 VP = P * V[face];
		
		gl_Layer = face;
		gl_Position = VP * pos1;
		outUVW = pos1.xyz;
		EmitVertex();
		
		gl_Layer = face;
		gl_Position = VP * pos2;
		outUVW = pos2.xyz;
		EmitVertex();
		
		gl_Layer = face;
		gl_Position = VP * pos3;
		outUVW = pos3.xyz;
		EmitVertex();
		
		EndPrimitive();
	}
}