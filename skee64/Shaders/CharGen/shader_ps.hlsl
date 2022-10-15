Texture2D diffuse : register(t0);
Texture2D normal : register(t1);
Texture2D specular : register(t2);
Texture2D detail : register(t3);
Texture2D tintMask : register(t4);

SamplerState Sampler;

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

float overlay( float base, float blend )
{
    float result;
    if ( base < 0.5 ) {
        result = 2.0 * base * blend;
    } else {
        result = 1.0 - 2.0 * (1.0 - blend) * (1.0 - base);
    }
    return result;
}

float4 tint(float4 base, float4 blend)
{
	return float4(overlay(base.r, blend.r), overlay(base.g, blend.g), overlay(base.b, blend.b), overlay(base.a, blend.a));
}

float4 LightPixelShader(PixelInputType input) : SV_TARGET
{
	float4 color = diffuse.Sample(Sampler, input.tex);
	if(color.a < alphaThreshold)
		discard;

	if(hasTintMask)
	{
		color = tint(color, tintMask.Sample(Sampler, input.tex));
	}
	
	if(tintColor.a > 0)
	{
		color = tint(color, tintColor);
	}

	color.rgb *= input.color;

	return color;
}
