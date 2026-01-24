$input v_color0, v_texcoord0

#include "../common.sh"

SAMPLER2D(s_texColor, 0);
uniform vec4 u_hasTexture; // x = 1.0 if texture bound, 0.0 otherwise

void main()
{
	vec4 baseColor = v_color0;
	
	if (u_hasTexture.x > 0.5)
	{
		vec4 texColor = texture2D(s_texColor, v_texcoord0);
		baseColor = texColor * v_color0;
	}
	
	gl_FragColor = baseColor;
}
