#version 450

#extension GL_ARB_separate_shader_objects : enable

#define MAT_ID_HDR_PROBE 0
#define MAT_ID_FSCHLICK_DGGX_GSMITH 1
#define NUM_MATERIALS 2

layout (set = 0, binding = 1) uniform sampler2DMS gbuffer1;
layout (set = 0, binding = 2) uniform sampler2DMS gbuffer2;
layout (set = 0, binding = 3) uniform sampler2DMS gbuffer3;
layout (set = 0, binding = 4) uniform sampler2DMS depthImage;

layout (set = 0, binding = 5) uniform samplerCube specularIrradianceMap;
layout (set = 0, binding = 6) uniform sampler2D brdfLuts[1];

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (constant_id = 0) const int NUM_LIGHTS = 32;
layout (constant_id = 1) const int NUM_SAMPLES = 4;

struct Light
{
	vec4 position;
	vec3 color;
	float radius;
};

layout (std140, set = 0, binding = 0) uniform UBO 
{
	vec3 eyePos;
	float emissiveStrength;
	vec4 diffuseSHCoefficients[9];
	Light pointLights[NUM_LIGHTS];
};

layout (push_constant) uniform pushConstants
{
	uint totalMipLevels;
} pcs;


vec4 unpackRGBA(float packedColor)
{
	uint tmp = uint(packedColor);	
	return vec4((tmp >> 24) & 0xff, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff, tmp & 0xff) 
		* 0.0039215686274509803921568627451;
}

vec3 computeDiffuseIrradiance(vec3 nrm)
{
	// rotate cosine lobe's SH coefficients so that
	// nrm is the rotational symmetric axis
	float c0 = 0.886228; // l = m = 0
	float c1 = 1.023328 * nrm.y; // l = 1, m = -1
	float c2 = 1.023328 * nrm.z; // l = 1, m = 0
	float c3 = 1.023328 * nrm.x; // l = 1, m = 1
	float c4 = 0.858085 * nrm.x * nrm.y; // l = 2, m = -2
	float c5 = 0.858085 * nrm.y * nrm.z; // l = 2, m = -1
	float c6 = 0.247708 * (3.0 * nrm.z * nrm.z - 1.0); // l = 2, m = 0
	float c7 = 0.858085 * nrm.x * nrm.z; // l = 2, m = 1
	float c8 = 0.429043 * (nrm.x * nrm.x - nrm.y * nrm.y); // l = 2, m = 2
	
	return diffuseSHCoefficients[0].rgb * c0 +
		diffuseSHCoefficients[1].rgb * c1 +
		diffuseSHCoefficients[2].rgb * c2 +
		diffuseSHCoefficients[3].rgb * c3 +
		diffuseSHCoefficients[4].rgb * c4 +
		diffuseSHCoefficients[5].rgb * c5 +
		diffuseSHCoefficients[6].rgb * c6 +
		diffuseSHCoefficients[7].rgb * c7 +
		diffuseSHCoefficients[8].rgb * c8;
}


void main() 
{
	const ivec2 extent = textureSize(gbuffer1);
	const ivec2 pixelPos = ivec2(extent * inUV);
	
	vec3 finalColor = vec3(0.0);
	int matIdCounts[NUM_MATERIALS] = { 0, 0 };
	int matIdRep[NUM_MATERIALS]; // stores representative sample id for each material
	
	for (int i = 0; i < NUM_SAMPLES; ++i)
	{
		int matId = int(round(texelFetch(gbuffer3, pixelPos, i).w * 255.0));
		++matIdCounts[matId];
		matIdRep[matId] = i;
	}
	
	if (matIdCounts[MAT_ID_HDR_PROBE] > 0)
	{
		finalColor += texelFetch(gbuffer1, pixelPos, matIdRep[MAT_ID_HDR_PROBE]).rgb *
			float(matIdCounts[MAT_ID_HDR_PROBE]) / NUM_SAMPLES;
	}
	
	if (matIdCounts[MAT_ID_FSCHLICK_DGGX_GSMITH] > 0)
	{
		int i = matIdRep[MAT_ID_FSCHLICK_DGGX_GSMITH];
		vec4 gb1 = texelFetch(gbuffer1, pixelPos, i);
		vec4 gb2 = texelFetch(gbuffer2, pixelPos, i);
		vec4 RMAI = texelFetch(gbuffer3, pixelPos, i); // (roughness, metalness, AO, material id)
		
		vec3 pos = gb2.xyz;
		float emissiveness = gb2.w;
		vec3 nrm = gb1.xyz;
		vec3 albedo = unpackRGBA(gb1.w).rgb;
		albedo = pow(albedo, vec3(2.2)); // to linear space
		
		float roughness = clamp(RMAI.x, 0.0, 1.0);
		float metalness = clamp(RMAI.y, 0.0, 1.0);
		float aoVal = RMAI.z;
		
		vec3 v = normalize(eyePos.xyz - pos);
		vec3 r = normalize(reflect(-v, nrm));
		
		vec3 diffIr = computeDiffuseIrradiance(nrm) * 0.318310;
		
		float specMipLevel = roughness * float(pcs.totalMipLevels - 1);
		vec3 specIr = textureLod(specularIrradianceMap, r, specMipLevel).rgb;
		
		float NoV = clamp(dot(nrm, v), 0.0, 1.0);
		vec2 brdfTerm = textureLod(brdfLuts[0], vec2(NoV, clamp(roughness, 0.0, 1.0)), 0).rg;
		
		const vec3 dielectricF0 = vec3(0.04);
		vec3 diffColor = albedo * (1.0 - metalness); // if it is metal, no diffuse color
		vec3 specColor = mix(dielectricF0, albedo, metalness); // since metal has no albedo, we use the space to store its F0
		
		vec3 result = diffColor * diffIr + specIr * (specColor * brdfTerm.x + brdfTerm.y);
		result *= aoVal;
		result += albedo * emissiveness * emissiveStrength;
		finalColor += result * float(matIdCounts[MAT_ID_FSCHLICK_DGGX_GSMITH]) / NUM_SAMPLES;
	}
	
	outColor = vec4(finalColor, 1.0);
}