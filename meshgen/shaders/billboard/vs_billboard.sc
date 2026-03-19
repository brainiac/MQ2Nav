$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_color0, v_uv, v_atlasUV

#include "../common.sh"

void main()
{
	vec4 instancePosition = i_data0;
	vec4 instanceColor    = i_data1;
	float width           = i_data2.x;
	vec4 uvRegion         = i_data3;  // (u0, v0, u1, v1) in atlas
	float scale = 5.0;

	v_color0 = instanceColor;
	v_uv = a_position.xy + 0.5;  // -0.5..0.5 to 0..1

	// Remap quad 0..1 UV into the atlas sub-region
	v_atlasUV = mix(uvRegion.xy, uvRegion.zw, v_uv);

	gl_Position = mul(u_viewProj, instancePosition);
	if (gl_Position.w > 0) {
		gl_Position /= gl_Position.w;
		vec2 ndcScale = vec2(width * scale, width * scale) / u_viewRect.zw;
		gl_Position.xy += a_position.xy * ndcScale;
	} else {
		gl_Position = vec4(0, 0, 100, 1);
	}
}
