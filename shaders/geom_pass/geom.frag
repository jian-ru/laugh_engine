#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerNormal;
layout (binding = 4) uniform sampler2D samplerRoughness;
layout (binding = 5) uniform sampler2D samplerMetalness;
layout (binding = 6) uniform sampler2D samplerAO;

layout (push_constant) uniform pushConstants
{
	uint materialId;
	uint hasAoMap;
} pcs;

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inWorldNormal;
layout (location = 2) in vec2 inTexcoord;

layout (location = 0) out vec4 outGbuffer1;
layout (location = 1) out vec4 outGbuffer2;
layout (location = 2) out vec4 outGbuffer3;


float packRGBA(vec4 color)
{
	uvec4 rgba = uvec4(color * 255.0 + 0.49);
	return float((rgba.r << 24) | (rgba.g << 16) | (rgba.b << 8) | rgba.a);
}

// Compute a matrix that transform a vector from tangent space
// to world space
mat3 computeTBN(vec3 n)
{
	vec3 e1 = dFdx(inWorldPos);
	vec3 e2 = dFdy(inWorldPos);
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
	vec4 albedo = vec4(texture(samplerAlbedo, inTexcoord).rgb, 0.0);
	vec3 nrmmap = texture(samplerNormal, inTexcoord).rgb;
	float roughness = texture(samplerRoughness, inTexcoord).r;
	float metalness = texture(samplerMetalness, inTexcoord).r;
	float aoVal = 1.0;
	if (pcs.hasAoMap > 0)
	{
		aoVal = texture(samplerAO, inTexcoord).r;
	}

	vec3 surfnrm = normalize(inWorldNormal);
	mat3 tbn = computeTBN(surfnrm);
	
	float packedAlbedo = packRGBA(albedo);
	vec3 nrm = normalize(tbn * (2.0 * nrmmap - 1.0));
	vec4 RMAI = vec4(roughness, metalness, aoVal, float(pcs.materialId) / 255.0);
	
	outGbuffer1 = vec4(nrm, packedAlbedo);
	outGbuffer2 = vec4(inWorldPos, 0.0);
	outGbuffer3 = RMAI;
}