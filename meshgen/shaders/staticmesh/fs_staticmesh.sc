$input v_color0, v_texcoord0, v_worldPos, v_worldNormal

#include "../common.sh"
#include "staticmesh.sh"

SAMPLER2D(s_texColor, 0);


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

	// Per-fragment lighting mode (mode 2): compute all lighting here
	if (u_pointLightMode > 1.5)
	{
		// Directional light (Lambert)
		vec3 lightDir = normalize(-u_directionalLightNormal.xyz);
		vec3 ambient = u_directionalLightColor.rgb;
		float ndotl = max(dot(normal, lightDir), 0.0);
		vec3 lighting = ambient + (vec3_splat(1.0) - ambient) * ndotl;

		// Point lights
		lighting = lighting
			+ CalculatePointLightShading(0, normal, v_worldPos)
			+ CalculatePointLightShading(1, normal, v_worldPos)
			+ CalculatePointLightShading(2, normal, v_worldPos);

		baseColor = vec4(baseColor.rgb * lighting, baseColor.a);
	}

	gl_FragColor = baseColor;
}
