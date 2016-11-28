#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (input_attachment_index = 0, binding = 1) uniform subpassInput siGbuffer1;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (constant_id = 0) const int NUM_LIGHTS = 32;

struct Light
{
	vec4 position;
	vec3 color;
	float radius;
};

layout (binding = 0) uniform UBO 
{
	// vec4 eyePos;
	Light lights[NUM_LIGHTS];
} ubo;


vec4 unpackRGBA(float packedColor)
{
	uint tmp = uint(packedColor);	
	return vec4((tmp >> 24) & 0xff, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff, tmp & 0xff) 
		* 0.0039215686274509803921568627451;
}

vec3 recoverEyePos(float depth)
{
	float oneOverHeight = 1.0 / 720.0;
	float aspect = 1280.0 / 720.0;
	float ph = 2.0 * 0.414213562 * oneOverHeight;
	float pw = ph * aspect;
	vec3 viewVec = vec3(vec2(pw, ph) * (vec2(inUV.x, 1.0 - inUV.y) * vec2(1280.0, 720.0) - vec2(1280.0, 720.0) * 0.5), -1.0);
	return viewVec * -depth;
}

vec3 recoverEyeNrm(vec2 xy)
{
	return vec3(xy, sqrt(1.0 - max(0.0, dot(xy, xy))));
}


void main() 
{
	vec4 gb1 = subpassLoad(siGbuffer1);
	
	vec3 pos = recoverEyePos(gb1.w);
	vec3 nrm = recoverEyeNrm(gb1.xy);
	vec3 albedo = unpackRGBA(gb1.z).rgb;
	
	// float depth = (100.0 / -99.9 * gb1.w - 10.0 / 99.9) / -gb1.w;
	// outColor = vec4(depth, depth, depth, 1.0);
	// return;
	
	if (-gb1.w > 0.1 && -gb1.w < 100.0)
	{
		vec3 finalColor = vec3(0.0);
	
		for (int i = 0; i < NUM_LIGHTS; ++i)
		{
			vec3 lightPos = ubo.lights[i].position.xyz;
			
			float dist = distance(lightPos, pos);
			
			if (dist < ubo.lights[i].radius)
			{
				float scale = (ubo.lights[i].radius - dist) / ubo.lights[i].radius;
				vec3 v = -pos;
				vec3 l = lightPos - pos;
				vec3 h = normalize(v + l);
				
				finalColor +=
					0.5 * albedo * clamp(dot(nrm, l), 0.0, 1.0) +
					0.5 * ubo.lights[i].color * pow(clamp(dot(nrm, h), 0.0, 1.0), 20.0);
			}
		}
		
		outColor = vec4(finalColor, 1.0);
		// outColor = vec4(nrm, 1.0);
		// outColor = vec4(albedo, 1.0);
		// outColor = gb1;
		// outColor = vec4(abs(pos), 1.0);
		// outColor = vec4(ubo.eyePos.xyz / 3.0, 1.0);
	}
}