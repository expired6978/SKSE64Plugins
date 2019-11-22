cbuffer MatrixBuffer : register(b0)
{
	matrix worldMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
	float4 viewport;
};

cbuffer TransformBuffer : register(b1)
{
	matrix transform;
};

struct VertexInputType
{
	float4 position : POSITION;
	float2 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3 color : COLOR;
};

struct GS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3 color : COLOR;
};

float4 modelToProj(in float4 pos)
{
	return mul(mul(mul(pos, worldMatrix), viewMatrix), projectionMatrix);
}

GS_INPUT WireframeVertexShader(VertexInputType input)
{
	GS_INPUT output;
	input.position.w = 1.0f;
	input.position = mul(input.position, transform);
	output.Pos = modelToProj(input.position);
	output.tex = input.tex;
	output.normal = mul(input.normal, (float3x3)worldMatrix);
	output.normal = normalize(output.normal);
	output.color = input.color;
	return output;
}