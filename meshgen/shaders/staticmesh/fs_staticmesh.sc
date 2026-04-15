$input v_color0, v_texcoord0, v_worldPos, v_worldNormal

#include "../common.sh"

SAMPLER2D(s_texColor, 0);

// x = texture bound
// y = show invisible
// z = is transparent texture
// w = alpha ref
uniform vec4 u_textureFlags;

// Directional light
uniform vec4 u_directionalLightColor;
uniform vec4 u_directionalLightNormal;

// Point lights (max 3)
uniform vec4 u_pointLightPosRadius[3];       // xyz = world position, w = radius
uniform vec4 u_pointLightColorIntensity[3];  // rgb = color * intensity
uniform vec4 u_pointLightParams;             // x = active light count (0-3)

#define u_hasTexture (u_textureFlags.x > 0.5)
#define u_isInvisibleTexture (u_textureFlags.z > 0.5)
#define u_showInvisibleTextures (u_textureFlags.y > 0.5)


vec3 CalculatePointLightShading(int lightNumber, vec3 normal, vec3 worldPos)
{
	float radius = u_pointLightPosRadius[lightNumber].w;
	if (radius <= 0.0)
		return vec3_splat(0.0);

	vec3 lightPos = u_pointLightPosRadius[lightNumber].xyz;

	vec3 toLight = lightPos - worldPos;
	
	float dist = length(toLight);

	if (dist < radius)
	{
		vec3 toLightDir = toLight / dist;
		float pointNdotL = max(dot(normal, toLightDir), 0.0);

		// Smooth quadratic attenuation
		float ratio = dist / radius;
		float attenuation = saturate(1.0 - ratio * ratio);

		return u_pointLightColorIntensity[lightNumber].rgb * pointNdotL * attenuation;
	}

	return vec3_splat(0.0);
}

void main()
{
	vec4 baseColor = v_color0;

	if (u_hasTexture)
	{
		if (u_isInvisibleTexture && !u_showInvisibleTextures)
		{
			discard;
		}

		vec4 texColor = texture2D(s_texColor, v_texcoord0);

		if (u_textureFlags.w > 0 && texColor.a < u_textureFlags.w)
		{
			discard;
		}

		baseColor = vec4(texColor.xyz * v_color0.xyz, v_color0.w);
	}
	else if (u_textureFlags.y < 1.0)
	{
		discard;
	}
	else if (u_alphaRef > 0 && baseColor.a < u_alphaRef)
	{
		discard;
	}

	// Per-fragment lighting
	vec3 normal = normalize(v_worldNormal);

	// Directional light (Lambert)
	vec3 lightDir = normalize(-u_directionalLightNormal.xyz);
	vec3 ambient = u_directionalLightColor.rgb;
	float ndotl = max(dot(normal, lightDir), 0.0);
	vec3 lighting = ambient + (vec3_splat(1.0) - ambient) * ndotl;

	// Point lights
	lighting = lighting
		+ CalculatePointLightShading(0, normal, v_worldPos);
		+ CalculatePointLightShading(1, normal, v_worldPos);
		+ CalculatePointLightShading(2, normal, v_worldPos);

	gl_FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
