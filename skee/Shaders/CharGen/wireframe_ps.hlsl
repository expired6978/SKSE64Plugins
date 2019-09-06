
cbuffer MaterialBuffer : register(b0)
{
	float4	wireColor;
	float4	tintColor;
	bool	hasNormal;
	bool	hasSpecular;
	bool	hasDetail;
	bool	hasTintMask;
	float	alphaThreshold;
	float	padding1;
	float	padding2;
	float	padding3;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3 color : COLOR;
};

float4 WireframePixelShader(PixelInputType input) : SV_TARGET
{
	return wireColor;
}
