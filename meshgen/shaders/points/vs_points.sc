$input a_position, i_data0, i_data1, i_data2
$output v_color0, v_pointPos, v_pointData

#include "../common.sh"

void main()
{
	vec4 instancePosition = i_data0;
	vec4 instanceColor = i_data1;
	float width = i_data2.x;
	float scale = 5;

	v_pointData = vec2(width, 0);
	v_color0 = instanceColor;

	gl_Position = mul(u_viewProj, instancePosition);
	if (gl_Position.w > 0) {
		gl_Position /= gl_Position.w;
		vec2 ndcScale = vec2(width * scale, width * scale) / u_viewRect.zw;
		gl_Position.xy += a_position * ndcScale;

		// Transform point vertex into NDC coordinates
		vec4 viewPos = mul(u_view, instancePosition);
		vec4 pointPos = mul(u_proj, viewPos);
		vec2 ndcPos = pointPos.xy / pointPos.w;
		ndcPos.y *= -1;
		v_pointPos = u_viewRect.zw * (ndcPos * 0.5 + 0.5);
		v_pointData.y = length(viewPos);
	} else {
		gl_Position = vec4(0, 0, 100, 1);
	}
}
