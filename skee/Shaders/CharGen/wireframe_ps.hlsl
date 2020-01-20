
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

struct PS_INPUT_WIRE
{
	float4 Pos : SV_POSITION;
	float2 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3 color : COLOR;
	noperspective float4 EdgeA: TEXCOORD1;
	noperspective float4 EdgeB: TEXCOORD2;
	uint Case : TEXCOORD3;
};

float LineWidth = 1.5;

float evalMinDistanceToEdges(in PS_INPUT_WIRE input)
{
	float dist;

	// The easy case, the 3 distances of the fragment to the 3 edges is already
	// computed, get the min.
	if (input.Case == 0)
	{
		dist = min(min(input.EdgeA.x, input.EdgeA.y), input.EdgeA.z);
	}
	// The tricky case, compute the distances and get the min from the 2D lines
	// given from the geometry shader.
	else
	{
		// Compute and compare the sqDist, do one sqrt in the end.

		float2 AF = input.Pos.xy - input.EdgeA.xy;
		float sqAF = dot(AF, AF);
		float AFcosA = dot(AF, input.EdgeA.zw);
		dist = abs(sqAF - AFcosA * AFcosA);

		float2 BF = input.Pos.xy - input.EdgeB.xy;
		float sqBF = dot(BF, BF);
		float BFcosB = dot(BF, input.EdgeB.zw);
		dist = min(dist, abs(sqBF - BFcosB * BFcosB));

		// Only need to care about the 3rd edge for some cases.
		if (input.Case == 1 || input.Case == 2 || input.Case == 4)
		{
			float AFcosA0 = dot(AF, normalize(input.EdgeB.xy - input.EdgeA.xy));
			dist = min(dist, abs(sqAF - AFcosA0 * AFcosA0));
		}

		dist = sqrt(dist);
	}

	return dist;
}

float4 WireframePixelShader(PS_INPUT_WIRE input) : SV_TARGET
{
	// Compute the shortest distance between the fragment and the edges.
	float dist = evalMinDistanceToEdges(input);

	// Cull fragments too far from the edge.
	if (dist > 0.5*LineWidth + 1) discard;

	// Map the computed distance to the [0,2] range on the border of the line.
	dist = clamp((dist - (0.5*LineWidth - 1)), 0, 2);

	// Alpha is computed from the function exp2(-2(x)^2).
	dist *= dist;
	float alpha = exp2(-0.75 * dist);

	// Standard wire color
	float4 color = wireColor;
	color.a *= alpha;

	return color;
}
