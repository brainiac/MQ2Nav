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
	v_worldNormal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);

	// Choose base color: vertex color or white based on uniform
	vec3 baseColor = mix(vec3_splat(1.0), a_color0.rgb + u_globalAmbient.rgb, u_useVertexShading);

	if (u_useVertexTint)
	{
		baseColor = baseColor * a_color1;
	}

	// Pass base color (no lighting applied - lighting is computed in fragment shader)
	vec3 color = baseColor;

	// Directional light (always computed in vertex shader)
	vec3 lightDir = normalize(-u_directionalLightNormal.xyz);
	vec3 ambient = u_directionalLightColor.rgb;
	float ndotl = max(dot(v_worldNormal, lightDir), 0.0);
	vec3 lighting = ambient + (vec3_splat(1.0) - ambient) * ndotl;

	// Per-vertex point light shading (mode 1)
	float pointLightMode = u_pointLightMode;
	if (pointLightMode > 0.5 && pointLightMode < 1.5)
	{
		lighting = lighting
			+ CalculatePointLightShading(0, v_worldNormal, v_worldPos)
			+ CalculatePointLightShading(1, v_worldNormal, v_worldPos)
			+ CalculatePointLightShading(2, v_worldNormal, v_worldPos);
	}

	color = color * lighting;

	v_color0 = vec4(color, u_materialAlpha);

	v_texcoord0 = a_texcoord0;
}
