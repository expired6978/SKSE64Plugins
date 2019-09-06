struct PixelInputType
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

float4 BrushPShader(PixelInputType input) : SV_TARGET
{
	return input.color;
}
