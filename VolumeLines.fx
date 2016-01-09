//////////////////////
// Volume line shader. Based on the CG Volume Lines sample from NVidia.

float4x4 mWVP : WorldViewProjection;
float4x4 mWV : WorldView;


texture lineTexture;

sampler textureSampler = sampler_state
{
	Texture = (lineTexture);
	MipFilter = LINEAR;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
};


struct TInputVertex
{
	float4 pos			: POSITION;		// Position of this vertex
	float4 otherPos		: NORMAL;		// Position of the other vertex at the other end of the line.
	float4 texOffset	: TEXCOORD0;	// Tex coord offset.
	float3 thickness	: TEXCOORD1;	// Thickness info.
};

struct TOutputVertex
{
	float4 hPos : POSITION;
	float blend : COLOR0;
	float4 tex0 : TEXCOORD0;
	float4 tex1 : TEXCOORD1;
};

TOutputVertex VolumeLineVS(TInputVertex IN)
{
	TOutputVertex OUT = (TOutputVertex)0;

	// World*View transformation.
	float4 posStart = mul(IN.pos, mWV);
	float4 posEnd = mul(IN.otherPos, mWV);

	// Unit vector between eye and center of the line.
	float3 middlePoint = normalize((posStart.xyz + posEnd.xyz) / 2.0);

	// Unit vector of the line direction.
	float3 lineOffset = posEnd.xyz - posStart.xyz;
	float3 lineDir = normalize(lineOffset);
	float lineLenSq = dot(lineOffset, lineOffset);

	// Dot product to compute texture coef
	float texCoef = abs(dot(lineDir, middlePoint));

	// Change texture coef depending on line length: y=(Sz/(l^2))(x-1)+1
	texCoef = max(((texCoef - 1) * (lineLenSq / IN.thickness.z)) + 1, 0);

	posStart = mul(IN.pos, mWVP);
	posEnd = mul(IN.otherPos, mWVP);

	// Project these points in screen space.
	float2 startPos2D = posStart.xy / posStart.w;
	float2 endPos2D = posEnd.xy / posEnd.w;

	// Calculate 2D line direction.
	float2 lineDir2D = normalize(startPos2D - endPos2D);

	// Shift vertices along 2D projected line
	posStart.xy += ((texCoef * IN.texOffset.x) * lineDir2D.xy);

	// Shift vertex for thickness perpendicular to line direction.
	lineDir2D *= IN.thickness.x;
	posStart.x += lineDir2D.y;
	posStart.y -= lineDir2D.x;

	OUT.hPos = posStart;

	// Compute tex coords depending on texCoef.
	float4 tex;
	tex.zw = float2(0, 1);
	tex.y = min(15.0 / 16.f, texCoef);
	tex.x = modf(tex.y * 4.0, tex.y);
	OUT.blend = modf(tex.x * 4.0, tex.x);
	tex.xy = (tex.xy / 4.0) + (IN.texOffset).zw;
	tex.y = 1 - tex.y;
	OUT.tex0 = tex;

	// Now get the second texture coord : increment.

	tex.y = min(texCoef + (1.0 / 16.f), 15.0 / 16.0);
	tex.x = modf(tex.y * 4.0, tex.y);
	tex.x = floor(tex.x * 4.0);
	tex.xy = (tex.xy / 4) + (IN.texOffset).zw;
	tex.y = 1 - tex.y;
	OUT.tex1 = tex;

	return OUT;
}


float4 VolumeLinePS(TOutputVertex IN) : COLOR
{
	float4 blendFactor = IN.blend;
	float4 c0 = tex2D(textureSampler, IN.tex0);
	float4 c1 = tex2D(textureSampler, IN.tex1);
	return lerp(c0, c1, blendFactor);
}


technique VolumeLine
{
	pass p0
	{
		CullMode = none;
		AlphaBlendEnable = true;
		SrcBlend = one;
		DestBlend = one;
		alpharef = 0.3;
		alphafunc = GreaterEqual;
		ZEnable = true;
		VertexShader = compile vs_2_0 VolumeLineVS();
		PixelShader = compile ps_2_0 VolumeLinePS();
	}
}
