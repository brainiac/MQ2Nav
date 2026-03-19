$input v_color0, v_uv, v_atlasUV

#include "../common.sh"

SAMPLER2D(s_texColor, 0);

void main()
{
	vec4 texel = texture2D(s_texColor, v_atlasUV);
	if (texel.a < 0.01)
		discard;

	gl_FragColor = texel * v_color0;
}
