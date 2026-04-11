$input a_position, a_normal, a_color0, a_color1, a_texcoord0
$output v_color0, v_texcoord0

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

	// Transform normal by model matrix (use u_model for world space normal)
	vec3 worldNormal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);

	// Simple directional lighting for flat shading
	vec3 lightDir = normalize(vec3(0.3, 1.0, 0.5));
	float ndotl = max(dot(worldNormal, lightDir), 0.0);
	float ambient = 0.3;
	float lighting = ambient + (1.0 - ambient) * ndotl;

	// Choose base color: vertex color or white based on uniform
	vec3 baseColor = mix(vec3_splat(1.0), a_color0.rgb + u_globalAmbient.rgb, u_useVertexColors.x);

	if (u_useVertexColors.z > 0.5f)
	{
		baseColor = baseColor * a_color1;
	}

	// Apply lighting to base color
	v_color0 = vec4(baseColor * lighting, u_useVertexColors.y);
	v_texcoord0 = a_texcoord0;
}
