#version 450

#extension GL_ARB_separate_shader_objects : enable

#define PI 								3.1415926535897932384626433832795
#define ONE_OVER_PI						0.31830988618379067153776752674503

#define MAT_ID_HDR_PROBE				0
#define MAT_ID_FSCHLICK_DGGX_GSMITH 	1
#define NUM_MATERIALS					2
#define CSM_MAX_SEG_COUNT				4
#define MAX_SHADOW_LIGHT_COUNT			2

layout (set = 0, binding = 1) uniform sampler2DMS gbuffer1;
layout (set = 0, binding = 2) uniform sampler2DMS gbuffer2;
layout (set = 0, binding = 3) uniform sampler2DMS gbuffer3;
layout (set = 0, binding = 4) uniform sampler2DMS depthImage;

layout (set = 0, binding = 5) uniform samplerCube specularIrradianceMap;
layout (set = 0, binding = 6) uniform sampler2D brdfLuts[1];
layout (set = 0, binding = 7) uniform sampler2DArrayShadow shadowMaps[1];

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (constant_id = 0) const int NUM_LIGHTS = 32;
layout (constant_id = 1) const int NUM_SAMPLES = 4;

struct DiracLight
{
	vec3 posOrDir;		// world position for point light, direction for directional light
	int idx;			// < 0 if doesn't cast shadow. Otherwise, (idx * CSM_MAX_SEG_COUNT) points to the first cascade scale/offset
	vec3 color;			// can be greater than one
	float radius;		// == 0.0 for directional light and > 0.0 for point light
};

layout (std140, set = 0, binding = 0) uniform UBO 
{
	vec3 eyeWorldPos;
	float emissiveStrength;
	vec4 diffuseSHCoefficients[9];
	vec4 normFarPlaneZs;
	mat4 cascadeVPs[CSM_MAX_SEG_COUNT * MAX_SHADOW_LIGHT_COUNT];
	DiracLight diracLights[NUM_LIGHTS];
};

layout (push_constant) uniform pushConstants
{
	uint totalMipLevels;
	int segmentCount;
	int pcfKernelSize;
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

vec3 computeShadowCoords(int idx, vec3 worldPos)
{
	vec4 clipPos = cascadeVPs[idx] * vec4(worldPos, 1.0);
	vec3 ndcPos = clipPos.xyz / clipPos.w;
	return vec3(ndcPos.xy * 0.5 + 0.5, ndcPos.z);
}

float pcfPercentLit(int lightIdx, vec4 shadowCoords, vec2 texelSize)
{
	int offset = pcs.pcfKernelSize >> 1;
	float result = 0.0;
	
	for (int du = -offset; du <= offset; ++du)
	{
		for (int dv = -offset; dv <= offset; ++dv)
		{
			result += texture(shadowMaps[lightIdx], shadowCoords + vec4(vec2(du, dv) * texelSize, 0.0, 0.0));
		}
	}
	
	return result / float(pcs.pcfKernelSize * pcs.pcfKernelSize);
}

float computeShadowCoef(int lightIdx, float sampleDepth, vec3 worldPos, vec2 texelSize)
{
	int seg = pcs.segmentCount - 1;
	if (sampleDepth < normFarPlaneZs.x) seg = 0;
	else if (sampleDepth < normFarPlaneZs.y) seg = 1;
	else if (sampleDepth < normFarPlaneZs.z) seg = 2;

	vec4 shadowCoords;
	shadowCoords.xyw = computeShadowCoords(CSM_MAX_SEG_COUNT * lightIdx + seg, worldPos);
	shadowCoords.w -= 0.002; // Bias to reduce shadow acne
	shadowCoords.z = float(seg);
	
	return pcfPercentLit(lightIdx, shadowCoords, texelSize);
}

void unpackGbuffers(
	in ivec2 pixelPos, in int sampleIdx, in vec4 RMAI, out vec3 worldPos, out vec3 worldNrm, out vec3 albedo,
	out float roughness, out float metalness, out float aoVal, out float emissiveness, out float depth)
{
	vec4 gb1 = texelFetch(gbuffer1, pixelPos, sampleIdx);
	vec4 gb2 = texelFetch(gbuffer2, pixelPos, sampleIdx);
	depth = texelFetch(depthImage, pixelPos, sampleIdx).r; // [0, 1] non-linear
	
	worldPos = gb2.xyz;
	worldNrm = gb1.xyz;
	albedo = pow(unpackRGBA(gb1.w).rgb, vec3(2.2)); // to linear space
	
	roughness = clamp(RMAI.x, 0.0, 1.0);
	metalness = clamp(RMAI.y, 0.0, 1.0);
	aoVal = RMAI.z;
	emissiveness = gb2.w;
}

vec3 computeDistantEnvironmentLighting(
	vec3 worldPos, vec3 worldNrm, vec3 albedo,
	float roughness, float metalness, float aoVal)
{
	vec3 v = normalize(eyeWorldPos.xyz - worldPos);
	vec3 r = normalize(reflect(-v, worldNrm));
	
	vec3 diffIr = computeDiffuseIrradiance(worldNrm) * ONE_OVER_PI;
	
	float specMipLevel = roughness * float(pcs.totalMipLevels - 1);
	vec3 specIr = textureLod(specularIrradianceMap, r, specMipLevel).rgb;
	
	float NoV = clamp(dot(worldNrm, v), 0.0, 1.0);
	vec2 brdfTerm = textureLod(brdfLuts[0], vec2(NoV, clamp(roughness, 0.0, 1.0)), 0).rg;
	
	const vec3 dielectricF0 = vec3(0.04);
	vec3 diffColor = albedo * (1.0 - metalness); // if it is metal, no diffuse color
	vec3 specColor = mix(dielectricF0, albedo, metalness); // since metal has no albedo, we use the space to store its F0
	
	const float distEnvLightStrength = diffuseSHCoefficients[0].w;
	vec3 distEnvLighting = diffColor * diffIr + specIr * (specColor * brdfTerm.x + brdfTerm.y);
	distEnvLighting *= aoVal * distEnvLightStrength;
	
	return distEnvLighting;
}

float D_GGX(float NoH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = NoH * NoH * (a2 - 1.0) + 1.0;
	denom = 1.0 / (denom * denom);
	return a2 * denom * ONE_OVER_PI;
}

float G1_Schlick(float NoX, float k)
{
	return NoX * (1.0 / (NoX * (1.0 - k) + k));
}

float G(float NoL, float NoV, float roughness)
{
	float k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
	return G1_Schlick(NoL, k) * G1_Schlick(NoV, k);
}

vec3 F_Schlick(vec3 F0, float LoH)
{
	return F0 + (vec3(1.0) - F0) * pow(1.0 - LoH, 5.0);
}

vec3 f_CookTorrance(float NoL, float NoV, float NoH, float LoH, float roughness, vec3 F0)
{
	float denom = 4.0 * NoL * NoV;
	return D_GGX(NoH, roughness) * F_Schlick(F0, LoH) * G(NoL, NoV, roughness) * (1.0 / denom);
}

vec3 computeDiracLighting(
	vec3 worldPos, vec3 worldNrm, vec3 albedo,
	float roughness, float metalness, float depth)
{
	const vec3 dielectricF0 = vec3(0.04);
	vec3 diffColor = albedo * (1.0 - metalness) * ONE_OVER_PI;
	vec3 specColor = mix(dielectricF0, albedo, metalness);
	vec3 result = vec3(0.0);
	vec3 L, V, H, Ir;
	float NoL, NoV, NoH, LoH;
	float visibility = 1.0;

	for (int i = 0; i < NUM_LIGHTS; ++i)
	{
		DiracLight light = diracLights[i];
		
		if (light.radius > 0.0) // point light
		{
			L = light.posOrDir - worldPos;
			float radius = light.radius;
			float scale = max(0.0, radius - length(L)) / radius;
			if (scale == 0.0) continue;
			Ir = light.color * (scale * scale);
			L = normalize(L);
		}
		else // directional light
		{
			L = light.posOrDir;
			Ir = light.color;
		}
		
		NoL = dot(worldNrm, L);
		if (NoL <= 0.0) continue;
		V = normalize(eyeWorldPos - worldPos);
		H = normalize(V + L);
		NoV = dot(worldNrm, V);
		NoH = dot(worldNrm, H);
		LoH = dot(L, H);
		
		if (light.idx >= 0) // this light casts shadow
		{
			vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMaps[light.idx], 0).xy);
			visibility = computeShadowCoef(light.idx, depth, worldPos, texelSize);
			if (visibility == 0.0) continue;
		}
		
		result += (diffColor + f_CookTorrance(NoL, NoV, NoH, LoH, roughness, specColor)) * Ir * NoL * visibility;
	}
	
	return result;
}


void main() 
{
	const ivec2 extent = textureSize(gbuffer1);
	const ivec2 pixelPos = ivec2(extent * inUV);
	vec3 finalColor = vec3(0.0);
	
	// Analyze normal complexity/discontinuity
	bool bComplexPixel = false;
	vec3 firstNrm = texelFetch(gbuffer1, pixelPos, 0).xyz;

	for (int i = 1; i < NUM_SAMPLES; ++i)
	{
		if (dot(abs(firstNrm - texelFetch(gbuffer1, pixelPos, i).xyz), vec3(1.0)) > 0.1)
		{
			bComplexPixel = true;
			break;
		}
	}
	
	// Lighting computation
	int numIters = bComplexPixel ? NUM_SAMPLES : 1;
	
	for (int i = 0; i < numIters; ++i)
	{
		vec4 RMAI = texelFetch(gbuffer3, pixelPos, i); // (roughness, metalness, AO, material id)
		
		if (round(RMAI.w * 255.0) == MAT_ID_HDR_PROBE)
		{
			finalColor += texelFetch(gbuffer1, pixelPos, i).rgb;
		}
		else // MAT_ID_FSCHLICK_DGGX_GSMITH
		{
			vec3 worldPos, worldNrm, albedo;
			float roughness, metalness, aoVal, emissiveness, depth;
			
			unpackGbuffers(pixelPos, i, RMAI, worldPos, worldNrm, albedo, roughness, metalness, aoVal, emissiveness, depth);
			
			finalColor += computeDistantEnvironmentLighting(worldPos, worldNrm, albedo, roughness, metalness, aoVal);
			finalColor += computeDiracLighting(worldPos, worldNrm, albedo, roughness, metalness, depth);
			finalColor += albedo * emissiveness * emissiveStrength;
		}
	}
	
	outColor = vec4(finalColor / float(numIters), 1.0);
}

