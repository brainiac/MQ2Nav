$input v_color0, v_texcoord0, v_worldPos, v_worldNormal

#include "../common.sh"
#include "staticmesh.sh"

SAMPLER2D(s_texColor, 0);


void main()
{
	vec4 baseColor = v_color0;

	if (u_hasTexture)
	{
		// Some invisible walls have actual textures.
		if (u_isInvisibleTexture && !u_showInvisibleTextures)
		{
			discard;
		}

		vec4 texColor = texture2D(s_texColor, v_texcoord0);

		// Handle alpha test
		if (u_alphaRef > 0 && texColor.a < u_alphaRef)
		{
			discard;
		}

		baseColor = vec4(texColor.rgb * baseColor.rgb, baseColor.a);
	}
	else if (!u_showInvisibleTextures)
	{
		// Discard invisible walls without textures unless we're showing invisible textures.
		discard;
	}
	else if (u_alphaRef > 0 && baseColor.a < u_alphaRef)
	{
		// Handle alpha test for non-textured surfaces
		discard;
	}

	gl_FragColor = baseColor;
}
