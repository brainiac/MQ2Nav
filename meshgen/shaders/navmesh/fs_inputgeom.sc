$input v_color0, v_texcoord0

#include "../common.sh"

SAMPLER2D(textureSampler, 0);

void main()
{
	vec4 textureColor = texture2D(textureSampler, v_texcoord0);
	gl_FragColor = textureColor * v_color0;
}
