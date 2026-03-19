$input v_color0, v_texcoord0

#include "../common.sh"

SAMPLER2D(s_texColor, 0);

// x = texture bound
// y = show invisible
// z = is transparent texture
// w = alpha ref
uniform vec4 u_textureFlags; 


void main()
{
	vec4 baseColor = v_color0;
	
	if (u_textureFlags.x > 0.5) // has texture
	{
		if (u_textureFlags.z > 0.5 && u_textureFlags.y < 1.0) // is transparent and !showInvisibile
		{
			discard;
		}

		vec4 texColor = texture2D(s_texColor, v_texcoord0);
		baseColor = texColor * v_color0;
	}
	else if (u_textureFlags.y < 1.0)
	{
		discard;
	}

	if (u_textureFlags.w > 0 && baseColor.a < u_textureFlags.w)
		discard;
	
	gl_FragColor = baseColor;
}
