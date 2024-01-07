$input a_position, i_data0, i_data1, i_data2
$output v_color0, v_pointPos, v_pointData

#include "../common.sh"

uniform vec4 CameraRight_worldspace;
uniform vec4 CameraUp_worldspace;

#define BILLBOARD 1

void main()
{
	vec4 instancePosition = i_data0;
	float scale = 5;
	vec4 instanceColor = i_data1;
	float width = i_data2.x;

	v_pointData = vec2(width, 0);
	v_color0 = instanceColor;

#if !BILLBOARD
	// Transform cube vertex into clip space
	mat4 model = mtxFromCols(
		vec4(scale, 0, 0, 0),
		vec4(0, scale, 0, 0),
		vec4(0, 0, scale, 0),
		instancePosition
	);
	vec4 worldPos = mul(model, vec4(a_position, 1.0));
#else
	vec3 particleCenter_worldspace = instancePosition.xyz;
	vec3 vertexPosition_worldspace = particleCenter_worldspace
		+ CameraRight_worldspace * a_position.x * 1
		+ CameraUp_worldspace * a_position.y * 1;

	//vec4 worldPos = vec4(vertexPosition_worldspace, 1.0);

	vec4 worldPos = instancePosition;
#endif

	gl_Position = mul(u_viewProj, worldPos);
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
}
