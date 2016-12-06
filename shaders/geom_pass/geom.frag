#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (binding = 1) uniform sampler2D samplerAlbedo;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform sampler2D samplerRoughness;
layout (binding = 4) uniform sampler2D samplerMetalness;

layout (push_constant) uniform pushConstants
{
	uint materialId;
} pcs;

layout (location = 0) in vec3 inEyePos;
layout (location = 1) in vec3 inEyeNormal;
layout (location = 2) in vec2 inTexcoord;

layout (location = 0) out vec4 outGbuffer1;


float packRGBA(vec4 color)
{
	uvec4 rgba = uvec4(color * 255.0 + 0.49);
	return float((rgba.r << 24) | (rgba.g << 16) | (rgba.b << 8) | rgba.a);
}

// Compute a matrix that transform a vector from tangent space
// to eye space
mat3 computeTBN(vec3 n)
{
	vec3 e1 = dFdx(inEyePos);
	vec3 e2 = dFdy(inEyePos);
	vec2 duv1 = dFdx(inTexcoord);
	vec2 duv2 = dFdy(inTexcoord);
	
	vec3 t = (duv2.t * e1 - duv1.t * e2) /
		(duv1.s * duv2.t - duv2.s * duv1.t);
	t = normalize(t - n * dot(n, t));
	vec3 b = normalize(cross(n, t));
	return mat3(t, b, n);
}


void main() 
{
	vec4 albedo = texture(samplerAlbedo, inTexcoord);
	vec3 nrmmap = texture(samplerNormal, inTexcoord).rgb;
	float roughness = texture(samplerRoughness, inTexcoord).r;
	float metalness = texture(samplerMetalness, inTexcoord).r;

	vec3 surfnrm = normalize(inEyeNormal);
	mat3 tbn = computeTBN(surfnrm);
	
	float packedAlbedo = packRGBA(albedo);
	vec3 nrm = normalize(tbn * (2.0 * nrmmap - 1.0));
	float RMI = packRGBA(vec4(roughness, metalness, float(pcs.materialId) / 255.0, 0.0));
	
	outGbuffer1 = vec4(nrm.xy, packedAlbedo, RMI);
}