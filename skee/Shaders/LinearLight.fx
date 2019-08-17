SamplerState SampleType;

struct LayerData
{
	uint blendMode;
	uint type;
	float4 color;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

Texture2D sourceTexture : register(t0);
Texture2D blendTexture : register(t1);
StructuredBuffer<LayerData> layerData : register(t2);

float4 Blend(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	if (b.r > 0.5) r.r = a.r + 2 * (b.r - 0.5); else r.r = a.r + 2 * b.r - 1;
	if (b.g > 0.5) r.g = a.g + 2 * (b.g - 0.5); else r.g = a.g + 2 * b.g - 1;
	if (b.b > 0.5) r.b = a.b + 2 * (b.b - 0.5); else r.b = a.b + 2 * b.b - 1;
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

// Technique name must be lowercase
float4 linearlight(PixelInputType input) : SV_TARGET
{
	float4 source = sourceTexture.Sample(SampleType, input.tex);
	float4 layer = blendTexture.Sample(SampleType, input.tex);
	
	switch(layerData[0].type)
	{
		case 0:
		layer = layer * layerData[0].color;
		break;
		case 1:
		layer = float4(layerData[0].color.rgb, layer.r * layerData[0].color.a);
		break;
		case 2:
		layer = layerData[0].color;
		break;
	}

	return Blend(source, layer);
}