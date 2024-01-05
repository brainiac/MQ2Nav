$input v_color0, v_texcoord0

#include "../common.sh"

uniform vec4 u_aa_radius;

void main()
{
	float v_u = v_texcoord0.x;
	float v_v = v_texcoord0.y;
	float v_line_width = v_texcoord0.z;
	float v_line_length = v_texcoord0.w;

	float au = 1.0 - smoothstep(1.0 - ((2.0 * u_aa_radius.x) / v_line_width), 1.0, abs(v_u / v_line_width));
	float av = 1.0 - smoothstep(1.0 - ((2.0 * u_aa_radius.y) / v_line_length), 1.0, abs(v_v / v_line_length));

	gl_FragColor = v_color0;
	gl_FragColor.a *= min(av, au);
}
