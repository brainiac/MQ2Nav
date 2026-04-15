$input a_position, a_normal, a_color0, a_color1, a_texcoord0
$output v_color0, v_texcoord0

#include "../common.sh"

// x: 1.0 = use vertex colors, 0.0 = white
// y: material alpha
// z: has vertex tint
uniform vec4 u_useVertexColors; 

uniform vec4 u_globalAmbient;
uniform vec4 u_directionalLightColor;
uniform vec4 u_directionalLightNormal;

void main()
{
	// Transform position by model-view-projection matrix
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

	// Transform normal by model matrix
	vec3 worldNormal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
	//vec3 worldNormal = 2.0 * a_normal - 1.0;

#if 0
	v_color0.rgb =
		  (u_globalAmbient.rgb * shadeFactor)
		+ (u_directionalLightColor.rgb * max(0.0, dot(worldNormal, -u_directionalLightNormal.xyz)))
		+ (a_color0.rgb * 0.5)
		;
	v_color0.rgb = clamp(v_color0.rgb, 0, 0.5);
	v_color0.a = 1;
#else
	// Simple directional lighting for flat shading
	vec3 lightDir = normalize(-u_directionalLightNormal.xyz);
	vec3 ambient = u_directionalLightColor.rgb;
	float ndotl = max(dot(worldNormal, lightDir), 0.0);
	vec3 lighting = ambient + (vec3_splat(1.0) - ambient) * ndotl;

	// Choose base color: vertex color or white based on uniform
	vec3 baseColor = mix(vec3_splat(1.0), a_color0.rgb + u_globalAmbient.rgb, u_useVertexColors.x);

	if (u_useVertexColors.z > 0.5f)
	{
		baseColor = baseColor * a_color1;
	}

	// Apply lighting to base color
	v_color0 = vec4(baseColor * lighting, u_useVertexColors.y);
#endif

	v_texcoord0 = a_texcoord0;
}
