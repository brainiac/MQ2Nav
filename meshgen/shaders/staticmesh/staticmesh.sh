
// x = texture bound
// y = show invisible
// z = is transparent texture
// w = alpha ref
uniform vec4 u_textureFlags;

// x: 1.0 = use vertex colors, 0.0 = white
// y: material alpha
// z: has vertex tint
uniform vec4 u_useVertexColors; 

uniform vec4 u_globalAmbient;


// Directional light
uniform vec4 u_directionalLightColor;
uniform vec4 u_directionalLightNormal;

// Point lights (max 3)
uniform vec4 u_pointLightPosRadius[3];       // xyz = world position, w = radius
uniform vec4 u_pointLightColorIntensity[3];  // rgb = color * intensity

#define u_hasTexture (u_textureFlags.x > 0.5)
#define u_isInvisibleTexture (u_textureFlags.z > 0.5)
#define u_showInvisibleTextures (u_textureFlags.y > 0.5)


vec3 CalculatePointLightShading(int lightNumber, vec3 normal, vec3 worldPos)
{
	float radius = u_pointLightPosRadius[lightNumber].w;
	if (radius <= 0.0)
		return vec3_splat(0.0);

	vec3 lightVec = u_pointLightPosRadius[lightNumber].xyz - worldPos;
	float attenuation = 1.0f - saturate(dot(lightVec / radius, lightVec / radius));

	vec3 lightColor = u_pointLightColorIntensity[lightNumber].rgb;

	return lightColor * saturate(dot(normal, normalize(lightVec))) * attenuation;
}