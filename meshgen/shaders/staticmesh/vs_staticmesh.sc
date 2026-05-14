$input a_position, a_normal, a_color0, a_color1, a_texcoord0
$output v_color0, v_texcoord0, v_worldPos, v_worldNormal

#include "../common.sh"
#include "staticmesh.sh"


void main()
{
	// Transform position by model-view-projection matrix
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

	// Pass world-space position and normal for fragment shader lighting
	v_worldPos = mul(u_model[0], vec4(a_position, 1.0)).xyz;
	v_worldNormal = normalize(mul(u_normalMatrix, vec4(a_normal, 1.0)).xyz);

	vec3 baseColor =
		(u_useVertexShading) ? CalculateShading(a_color0.rgb, a_color0.a, v_worldNormal, v_worldPos)
		: vec3_splat(1.0f);

	if (u_useVertexTint)
	{
		baseColor = baseColor * a_color1;
	}

	v_color0 = vec4(baseColor, u_materialAlpha);

	v_texcoord0 = a_texcoord0;
}
