$input v_color0, v_pointPos, v_pointData

#include "../common.sh"

void main()
{
	float radius = v_pointData.x;
	float cameraDist = v_pointData.y;
	float dist = distance(v_pointPos, gl_FragCoord.xy);
	if (dist > radius)
		discard;

	float near = 500;
	float far = 1500 + near;

	float scaleVal = max(0, (cameraDist - near) / far);
	float alpha = max(0.0, min(1.0, 1.0 - scaleVal));

	gl_FragColor = vec4(v_color0.xyz, v_color0.w * alpha);
}
