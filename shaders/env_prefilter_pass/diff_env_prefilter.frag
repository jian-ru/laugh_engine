#version 450

#extension GL_ARB_separate_shader_objects : enable


#define PI 3.14159265358979323
#define INV_PI 0.31830988618379067239521257108191


layout (set = 0, binding = 1) uniform samplerCube hdrProbe; // unfiltered map with mips

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;


vec2 Hammersley(uint i, uint N)
{
	float vdc = bitfieldReverse(i) * 2.3283064365386963e-10; // Van der Corput
	return vec2(float(i) / float(N), vdc);
}

vec3 ImportanceSampleCosine(vec2 Xi, vec3 N)
{
	float cosTheta = sqrt(max(1.0 - Xi.y, 0.0));
	float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
	float phi = 2.0 * PI * Xi.x;
	
	vec3 L = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
	
	vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
	vec3 tangent = normalize(cross(N, up));
	vec3 bitangent = cross(N, tangent);
	
	return tangent * L.x + bitangent * L.y + N * L.z;
}

vec3 ImportanceSample(vec3 N)
{
	vec4 result = vec4(0.0);
	
	float cubeWidth = float(textureSize(hdrProbe, 0).x);
	
	const uint numSamples = 1024;
	for (uint i = 0; i < numSamples; ++i)
	{
		vec2 Xi = Hammersley(i, numSamples);
		vec3 L = ImportanceSampleCosine(Xi, N);
		
		float NoL = max(dot(N, L), 0.0);
		
		if (NoL > 0.0)
		{
			// Compute Lod using inverse solid angle and pdf.
            // From Chapter 20.4 Mipmap filtered samples in GPU Gems 3.
            // http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html
			float pdf = NoL * INV_PI;
			float solidAngleTexel = 4.0 * PI / (6.0 * cubeWidth * cubeWidth);
			float solidAngleSample = 1.0 / (numSamples * pdf);
			float lod = 0.5 * log2(solidAngleSample / solidAngleTexel);
			
			vec3 hdrRadiance = textureLod(hdrProbe, L, lod).rgb;
			result += vec4(hdrRadiance / pdf, 1.0);
		}
	}
	
	return result.rgb / result.w;
}


void main() 
{
	vec3 N = normalize(inUVW);
	
	// Convolve the environment map with a cosine lobe along N
	outColor = vec4(ImportanceSample(N), 1.0);
}