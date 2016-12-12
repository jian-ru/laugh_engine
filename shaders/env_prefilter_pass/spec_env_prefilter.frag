#version 450

#extension GL_ARB_separate_shader_objects : enable


#define PI 3.14159265358979323


layout (set = 0, binding = 1) uniform samplerCube hdrProbe; // unfiltered map with mips

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConst
{
	float convRoughness;
} pcs;


vec2 Hammersley(uint i, uint N)
{
	float vdc = bitfieldReverse(i) * 2.3283064365386963e-10; // Van der Corput
	return vec2(float(i) / float(N), vdc);
}

vec3 ImportanceSampleGGX(vec2 Xi, float roughness, vec3 N)
{
	float a = roughness * roughness;
	
	float phi = 2.0 * PI * Xi.x;
	float cosTheta = sqrt(clamp((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y), 0.0, 1.0));
	float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));
	
	vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	
	vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	
	return tangent * H.x + bitangent * H.y + N * H.z;
}

float D_GGX(float roughness, float NoH)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = NoH * NoH * (a2 - 1.0) + 1.0;
	denom = PI * denom * denom;
	return a2 / denom;
}

vec3 ImportanceSample(vec3 R)
{
	// Approximation: assume V == R
	// We lose enlongated reflection by doing this but we also get rid
	// of one variable. So we are able to bake irradiance into a single
	// mipmapped cube map. Otherwise, a cube map array is required.
	vec3 N = R;
	vec3 V = R;
	vec4 result = vec4(0.0);
	
	float cubeWidth = float(textureSize(hdrProbe, 0).x);
	
	const uint numSamples = 1024;
	for (uint i = 0; i < numSamples; ++i)
	{
		vec2 Xi = Hammersley(i, numSamples);
		vec3 H = ImportanceSampleGGX(Xi, pcs.convRoughness, N);
		vec3 L = 2.0 * dot(V, H) * H - V;
		
		float NoL = max(dot(N, L), 0.0);
		float NoH = max(dot(N, H), 0.0);
		float VoH = max(dot(V, H), 0.0);
		
		if (NoL > 0.0)
		{
			float D = D_GGX(pcs.convRoughness, NoH);
			float pdf = D * NoH / (4.0 * VoH);
			float solidAngleTexel = 4 * PI / (6.0 * cubeWidth * cubeWidth);
			float solidAngleSample = 1.0 / (numSamples * pdf);
			float lod = pcs.convRoughness == 0.0 ? 0.0 : 0.5 * log2(solidAngleSample / solidAngleTexel);
			
			vec3 hdrRadiance = textureLod(hdrProbe, L, lod).rgb;
			result += vec4(hdrRadiance * NoL, NoL);
		}
	}
	
	if (result.w == 0.0)
	{
		return result.rgb;
	}
	else
	{
		return result.rgb / result.w;
	}
}


void main() 
{
	vec3 R = normalize(inUVW);
	
	// Convolve the environment map with a GGX lobe along R
	outColor = vec4(ImportanceSample(R), 1.0);
}