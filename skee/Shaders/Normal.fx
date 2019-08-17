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

// Technique name must be lowercase
float4 normal(PixelInputType input) : SV_TARGET
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

	source.rgb = layer.rgb * layer.a + (source.rgb * (1 - layer.a));
	return source;
}