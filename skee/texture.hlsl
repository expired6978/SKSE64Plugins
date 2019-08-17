SamplerState SampleType;

struct LayerData
{
	uint blendMode;
	uint type;
	float4 color;
};

cbuffer PerFrameBuffer : register(b0)
{
	matrix worldMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
};

Texture2D sourceTexture : register(t0);
Texture2D blendTexture : register(t1);
StructuredBuffer<LayerData> layerData : register(t2);

struct VertexInputType
{
    float4 position : POSITION;
    float2 tex : TEXCOORD0;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 Overlay(float4 a, float4 b)
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

float4 LinearLight(float4 a, float4 b)
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

float4 LinearDodge(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	r.rgb = a.rgb + b.rgb;
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

float4 LinearBurn(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	r.rgb = clamp(a.rgb + b.rgb - 1, 0, 1);
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

float4 Lighten(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	if (a.r < b.r) r.r = b.r; else r.r = a.r;
	if (a.g < b.g) r.g = b.g; else r.g = a.g;
	if (a.b < b.b) r.b = b.b; else r.b = a.b;
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

float4 Darken(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	if (a.r > b.r) r.r = b.r; else r.r = a.r;
	if (a.g > b.g) r.g = b.g; else r.g = a.g;
	if (a.b > b.b) r.b = b.b; else r.b = a.b;
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

#define BlendColorDodgef(base, blend)	((blend == 1.0) ? blend : min(base / (1.0 - blend), 1.0))

float4 ColorDodge(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	r.r = BlendColorDodgef(a.r, b.r);
	r.g = BlendColorDodgef(a.g, b.g);
	r.b = BlendColorDodgef(a.b, b.b);
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

float4 ColorBurn(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	r.rgb = clamp(1 - (1 - a.rgb) / b.rgb, 0, 1);
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}


#define BlendSoftLightf(base, blend)	((blend < 0.5) ? (2.0 * base * blend + base * base * (1.0 - 2.0 * blend)) : (sqrt(base) * (2.0 * blend - 1.0) + 2.0 * base * (1.0 - blend)))

float4 SoftLight(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	r.r = BlendSoftLightf(a.r, b.r);
	r.g = BlendSoftLightf(a.g, b.g);
	r.b = BlendSoftLightf(a.b, b.b);
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

float BlendTintf(float a, float b)
{
    float r;
    if(a < 0.5) r = 2.0 * a * b; else r = 1.0 - 2.0 * (1.0 - b) * (1.0 - a);
    return r;
}

float4 Tint(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	r.r = BlendTintf(a.r, b.r);
	r.g = BlendTintf(a.g, b.g);
	r.b = BlendTintf(a.b, b.b);
	r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
	return r;
}

float4 RMN(float4 a, float4 b)
{
	float4 r;
	r.a = a.a;
	b.rgb = clamp(b.rgb / b.a, 0, 1);
	float3 n1 = a.xyz * float3( 2,  2, 2) + float3(-1, -1,  0);
    float3 n2 = b.xyz * float3(-2, -2, 2) + float3( 1,  1, -1);
    r.rgb = n1 * dot(n1, n2) / n1.z - n2;
    r.rgb = (1 - b.a) * a.rgb + r.rgb * b.a;
    return r;
}

float4 TexturePixelShader(PixelInputType input) : SV_TARGET
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
	}

	switch(layerData[0].blendMode)
	{
		case 0:
		source.rgb = layer.rgb * layer.a + (source.rgb * (1 - layer.a));
		break;
		case 1:
		source = Overlay(source, layer);
		break;
		case 2:
		source = LinearLight(source, layer);
		break;
		case 3:
		source = LinearDodge(source, layer);
		break;
		case 4:
		source = LinearBurn(source, layer);
		break;
		case 5:
		source = Lighten(source, layer);
		break;
		case 6:
		source = Darken(source, layer);
		break;
		case 7:
		source = ColorDodge(source, layer);
		break;
		case 8:
		source = ColorBurn(source, layer);
		break;
		case 9:
		source = SoftLight(source, layer);
		break;
		case 10:
		source = Tint(source, layer);
		break;
		case 11:
		source = RMN(source, layer);
		break;
	}

	return source;
}

PixelInputType TextureVertexShader(VertexInputType input)
{
	PixelInputType output;

	// Change the position vector to be 4 units for proper matrix calculations.
	input.position.w = 1.0f;

	// Calculate the position of the vertex against the world, view, and projection matrices.
	output.position = mul(input.position, worldMatrix);
	output.position = mul(output.position, viewMatrix);
	output.position = mul(output.position, projectionMatrix);
    
	// Store the texture coordinates for the pixel shader.
	output.tex = input.tex;
    
	return output;
}