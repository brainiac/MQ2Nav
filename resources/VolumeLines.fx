//////////////////////
// Volume line shader. Based on the CG Volume Lines sample from NVidia, a lot of time
// and some math from https://forum.libcinder.org/topic/smooth-thick-lines-using-geometry-shader.

float4x4 mWVP : WorldViewProjection;
float4x4 mWV : WorldView;

// inputs
float4 lineColor;
float4 linkColor;
float opacity;
float lineWidth;

struct TInputVertex
{
	float4 pos          : POSITION;     // Position of this vertex
	float4 otherPos     : NORMAL;       // Position of the other vertex at the other end of the line.
	float  thickness    : TEXCOORD1;    // Thickness info.
	float4 adjPos       : TEXCOORD0;
	float  adjHint      : TEXCOORD2;
	float  type         : TEXCOORD3;
};

struct TOutputVertex
{
	float4 Pos : POSITION;
	float Type : TEXCOORD0;
};

float2 perp(float2 p0)
{
	return float2(p0.y, -p0.x);
}

TOutputVertex VolumeLineVS(TInputVertex IN)
{
	TOutputVertex OUT = (TOutputVertex)0;

	float thickness = IN.thickness.x * lineWidth * 4;

	// World*View transformation.
	float4 posStart = IN.pos;
	float4 posEnd = IN.otherPos;
	float4 posAdj = IN.adjPos;

	posStart = mul(posStart, mWVP);
	posEnd = mul(posEnd, mWVP);
	posAdj = mul(posAdj, mWVP);

	// Project these points in screen space.
	float2 startPos2D = posStart.xy / posStart.w;
	float2 endPos2D = posEnd.xy / posEnd.w;
	float2 adjPos2D = posAdj.xy / posAdj.w;

	// Calculate 2D line direction.
	float2 v0_ = normalize(adjPos2D - startPos2D);
	float2 v1_ = normalize(startPos2D - endPos2D);
	float2 v2_ = normalize(adjPos2D - startPos2D);

	bool isEnd = (IN.adjHint == 0);
	float2 adjustment;

	if (!isEnd)
	{
		// determine miter lines by averaging the normals of the segment
		float2 tangent;
		if (IN.adjHint > 0) {
			tangent = normalize(v1_ + v2_);
		} else {
			tangent = normalize(v0_ + v1_);
		}

		adjustment = perp(tangent) * thickness;
	} else {
		adjustment = perp(v1_) * thickness;
	}

	posStart.xy += adjustment;

	OUT.Pos = posStart;
	OUT.Type = IN.type;
	return OUT;
}

float4 VolumeLinePS(float type : TEXCOORD0) : COLOR
{
	if (type > 0.0f) {
		return float4(linkColor.rgb, opacity);
	}

	return float4(lineColor.rgb, opacity);
}

technique VolumeLine
{
	// HIDDEN render pass
	pass p0
	{
		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		CullMode = None;

		// write stencil
		StencilEnable = true;
		StencilFunc = Always;
		StencilPass = Replace;
		StencilWriteMask = 255;
		StencilRef = 1;

		ZWriteEnable = false;
		ZEnable = true;
		ZFunc = Greater;

		VertexShader = compile vs_2_0 VolumeLineVS();
		PixelShader = compile ps_2_0 VolumeLinePS();
	}

	// VISIBLE render pass
	pass p1
	{
		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		CullMode = None;

		// write stencil
		StencilEnable = true;
		StencilFunc = Always;
		StencilPass = Replace;
		StencilWriteMask = 255;
		StencilRef = 1;

		ZWriteEnable = false;
		ZEnable = true;
		ZFunc = LessEqual;

		VertexShader = compile vs_2_0 VolumeLineVS();
		PixelShader = compile ps_2_0 VolumeLinePS();
	}

	// BORDER HIDDEN render pass
	// uses stencil buffer to mask out parts written by previous
	pass p2
	{
		AlphaBlendEnable = true;
		SrcBlend = SRCALPHA;
		DestBlend = INVSRCALPHA;
		CullMode = None;

		StencilEnable = true;
		StencilFunc = Equal;
		StencilRef = 0;
		StencilPass = Zero;

		ZWriteEnable = false;
		ZEnable = true;
		ZFunc = Greater;

		VertexShader = compile vs_2_0 VolumeLineVS();
		PixelShader = compile ps_2_0 VolumeLinePS();
	}

	// BORDER VISIBLE render pass
	// uses stencil buffer to mask out parts written by previous
	pass p3
	{
		AlphaBlendEnable = true;
		SrcBlend = SRCALPHA;
		DestBlend = INVSRCALPHA;
		CullMode = None;

		StencilEnable = true;
		StencilFunc = Equal;
		StencilRef = 0;
		StencilPass = Zero;

		ZWriteEnable = false;
		ZEnable = true;
		ZFunc = LessEqual;

		VertexShader = compile vs_2_0 VolumeLineVS();
		PixelShader = compile ps_2_0 VolumeLinePS();
	}
}
