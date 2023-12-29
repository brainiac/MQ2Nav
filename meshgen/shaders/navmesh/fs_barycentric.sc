$input v_color0, v_texcoord0, v_barycentric

#include "../common.sh"

SAMPLER2D(textureSampler, 0);

float fwidthEquivalent(float value) {
	return abs(ddx(value)) + abs(ddy(value));
}

float edgeFactor(float value) {
	float f = fwidthEquivalent(value);
	vec3 d = vec3(f, f, f);
	vec3 a3 = smoothstep(vec3(0, 0, 0), d * 1.5, value);
	return min(min(a3.x, a3.y), a3.z);
}

void main()
{
	vec4 textureColor = texture2D(textureSampler, v_texcoord0);
	float e = edgeFactor(v_barycentric);
	gl_FragColor.rgb = mix(textureColor * v_color0, vec3(0, 0, 0), 1.0 - e);
	gl_FragColor.a = 1.0;

}
