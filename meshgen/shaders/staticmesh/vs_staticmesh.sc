$input a_position, a_normal, a_color0, a_color1, a_texcoord0
$output v_color0, v_texcoord0, v_worldPos, v_worldNormal

#include "../common.sh"

// x: 1.0 = use vertex colors, 0.0 = white
// y: material alpha
// z: has vertex tint
uniform vec4 u_useVertexColors; 

uniform vec4 u_globalAmbient;

void main()
{
	// Transform position by model-view-projection matrix
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

	// Pass world-space position and normal for fragment shader lighting
	v_worldPos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
	v_worldNormal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);

	// Choose base color: vertex color or white based on uniform
	vec3 baseColor = mix(vec3_splat(1.0), a_color0.rgb + u_globalAmbient.rgb, u_useVertexColors.x);

	if (u_useVertexColors.z > 0.5f)
	{
		baseColor = baseColor * a_color1;
	}

	// Pass base color (no lighting applied - lighting is computed in fragment shader)
	v_color0 = vec4(baseColor, u_useVertexColors.y);

	v_texcoord0 = a_texcoord0;
}
