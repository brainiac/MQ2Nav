$input v_color0, v_texcoord0

#include "../common.sh"

SAMPLER2D(s_texColor, 0);

// x = texture bound
// y = show invisible
// z = is transparent texture
// w = alpha ref
uniform vec4 u_textureFlags;

#define u_hasTexture (u_textureFlags.x > 0.5)
#define u_isInvisibleTexture (u_textureFlags.z > 0.5)
#define u_showInvisibleTextures (u_textureFlags.y > 0.5)


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
	
	gl_FragColor = baseColor;
}
