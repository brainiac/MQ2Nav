
// x = texture bound
// y = show invisible
// z = is transparent texture
uniform vec4 u_textureFlags;

#define u_hasTexture (u_textureFlags.x > 0.5)
#define u_isInvisibleTexture (u_textureFlags.z > 0.5)
#define u_showInvisibleTextures (u_textureFlags.y > 0.5)

// x: 1.0 = use vertex colors, 0.0 = white
// y: material alpha
// z: has vertex tint
// w: point light mode
uniform vec4 u_shadingMode;

#define u_useVertexShading (u_shadingMode.x)
#define u_materialAlpha (u_shadingMode.y)
#define u_useVertexTint (u_shadingMode.z > 0.5)
#define u_pointLightShading (u_shadingMode.w)    // 0: off, 1: on

uniform vec4 u_globalAmbient;
uniform vec4 u_specialAmbient;

// Directional light
uniform vec4 u_directionalLightColor;
uniform vec4 u_directionalLightBounceColor;
uniform vec4 u_directionalLightNormal;

// Normal matrix (world inverse transpose) for correct normal transformation
uniform mat4 u_normalMatrix;

// Point lights
uniform vec4 u_pointLightPosRadius[3];       // xyz = world position, w = radius
uniform vec4 u_pointLightColor[3];           // rgb = color

vec3 CalculatePointLightShading(int lightNumber, vec3 normal, vec3 worldPos)
{
	float radius = u_pointLightPosRadius[lightNumber].w;
	if (radius <= 0.0)
		return vec3_splat(0.0);

	vec3 lightVec = u_pointLightPosRadius[lightNumber].xyz - worldPos;
	float attenuation = 1.0f - saturate(dot(lightVec / radius, lightVec / radius));

	vec3 lightColor = u_pointLightColor[lightNumber].rgb;
	return lightColor * saturate(dot(normal, normalize(lightVec))) * attenuation;
}

vec3 CalculateShading(float3 diffuseColor, float blendAmount, vec3 normal, vec3 worldPos)
{
	vec3 result = diffuseColor
		+ u_specialAmbient.rgb
		+ (u_directionalLightColor.rgb
			* max(0.0, dot(normal, -u_directionalLightNormal.xyz))
			* blendAmount)
		+ (u_directionalLightBounceColor.rgb
			* max(0.0, dot(normal, u_directionalLightNormal.xyz))
			* blendAmount)
		+ (u_globalAmbient.rgb * blendAmount)
			;

	// Handle point lighting if its enabled
	if (u_pointLightShading > 0.5)
	{
		// Point lights
		result = result
			+ CalculatePointLightShading(0, normal, worldPos)
			+ CalculatePointLightShading(1, normal, worldPos)
			+ CalculatePointLightShading(2, normal, worldPos);
	}

	return result;
}