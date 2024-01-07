$input a_position, i_data0, i_data1, i_data2
$output v_color0, v_pointPos, v_pointSize, v_debugPos

#include "../common.sh"

void main()
{
	vec4 instancePosition = i_data0;
	float instanceWidth = i_data2.x;
	vec4 instanceColor = i_data1;

	v_pointSize = vec3(instanceWidth, u_viewRect.z, u_viewRect.w);
	v_color0 = instanceColor;

	// Transform cube vertex into clip space
	float modelScale = 5;
	mat4 model = mtxFromCols(
		vec4(modelScale, 0, 0, 0),
		vec4(0, modelScale, 0, 0),
		vec4(0, 0, modelScale, 0),
		instancePosition
	);
	vec4 worldPos = mul(model, vec4(a_position, 1.0));
	gl_Position = mul(u_viewProj, worldPos);
	gl_Position = vec4(gl_Position.xyz / gl_Position.w, 1.0);

	// Transform point vertex into NDC coordinates
	vec4 pointPos = mul(u_viewProj, instancePosition);
	vec2 ndcPos = pointPos.xy / pointPos.w;
	ndcPos.y *= -1;
	//v_pointPos.xy = u_viewRect.zw * ndcPos;
	v_pointPos.zw = ndcPos;


	v_pointPos.xy = u_viewRect.zw * (ndcPos * 0.5 + 0.5);
	v_debugPos = pointPos;
	//v_pointPos.y = (u_viewRect.w / 2) - v_pointPos.y;
	//v_pointPos = pointPos.xyz / pointPos.w;
}
