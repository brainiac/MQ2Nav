$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_color0, v_texcoord0

#include "../common.sh"

uniform vec2 u_aa_radius;

void main()
{
	vec3 quad_pos = a_position;

	vec3 line_pos_a = i_data0.xyz;
	float line_width_a = i_data0.w;
	vec4 line_col_a = i_data1;
	vec3 line_pos_b = i_data2.xyz;
	float line_width_b = i_data2.w;
	vec4 line_col_b = i_data3;

	float u_width = u_viewRect.z;
	float u_height = u_viewRect.w;
	float u_aspect_ratio = u_height / u_width;

	vec4 colors[2] = {line_col_a, line_col_b};
	colors[0].a *= min(1.0, line_width_a);
	colors[1].a *= min(1.0, line_width_b);
	v_color0 = colors[int(quad_pos.x)];

	vec4 clip_pos_a = mul(u_modelViewProj, vec4(line_pos_a, 1.0));
	vec4 clip_pos_b = mul(u_modelViewProj, vec4(line_pos_b, 1.0));

	vec2 ndc_pos_0 = clip_pos_a.xy / clip_pos_a.w;
	vec2 ndc_pos_1 = clip_pos_b.xy / clip_pos_b.w;

	vec2 line_vector = ndc_pos_1 - ndc_pos_0;
	vec2 dir = normalize(vec2(line_vector.x, line_vector.y * u_aspect_ratio));

	float extension_length = u_aa_radius.y;
	line_width_a = max(1.0, line_width_a) + u_aa_radius.x;
	line_width_b = max(1.0, line_width_b) + u_aa_radius.x;

	vec2 normal = vec2(-dir.y, dir.x);
	vec2 normal_a = vec2(line_width_a / u_width, line_width_a / u_height) * normal;
	vec2 normal_b = vec2(line_width_b / u_width, line_width_b / u_height) * normal;
	vec2 extension = vec2(extension_length / u_width, extension_length / u_height) * dir;

	vec2 zw_part = (1.0 - quad_pos.x) * clip_pos_a.zw + quad_pos.x * clip_pos_b.zw;
	vec2 dir_y = quad_pos.y * ((1.0 - quad_pos.x) * normal_a + quad_pos.x * normal_b);
	vec2 dir_x = quad_pos.x * line_vector + (2.0 * quad_pos.x - 1.0) * extension;

	gl_Position = vec4((ndc_pos_0 + dir_x + dir_y) * zw_part.y, zw_part);
}
