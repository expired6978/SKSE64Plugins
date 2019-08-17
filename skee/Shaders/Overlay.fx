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
	if (a.r > 0.5) r.r = 1 - (1 - 2 * (a.r - 0.5)) * (1 - b.r); else r.r = (2 * a.r) * b.r;
	if (a.g > 0.5) r.g = 1 - (1 - 2 * (a.g - 0.5)) * (1 - b.g); else r.g = (2 * a.g) * b.g;
	if (a.b > 0.5) r.b = 1 - (1 - 2 * (a.b - 0.5)) * (1 - b.b); else r.b = (2 * a.b) * b.b;
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

// Technique name must be lowercase
float4 overlay(PixelInputType input) : SV_TARGET
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