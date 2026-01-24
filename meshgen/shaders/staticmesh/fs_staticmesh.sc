$input v_color0, v_texcoord0

#include "../common.sh"

// Placeholder for future texture sampling
// SAMPLER2D(textureSampler, 0);

void main()
{
	// Output vertex color directly
	// Future: multiply by texture sample
	// vec4 textureColor = texture2D(textureSampler, v_texcoord0);
	// gl_FragColor = textureColor * v_color0;
	
	gl_FragColor = v_color0;
}
