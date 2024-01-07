$input v_color0, v_pointPos, v_pointSize, v_debugPos

#include "../common.sh"

void main()
{
	float radius = v_pointSize.x;
	vec2 viewSize = v_pointSize.yz;

	//vec2 ndc = -1 + 2.0 * (gl_FragCoord.xy / viewSize);
	vec2 screenCoord = gl_FragCoord.xy;

	float dist = distance(v_pointPos.xy, screenCoord);
	if (dist > radius)
		discard;

	float threshold = 0.3;

	float d = dist / radius;
	float alpha = mix(v_color0.w, 0, step(1.0 - threshold, d));
	
	gl_FragColor = vec4(v_color0.xyz, alpha);
}
