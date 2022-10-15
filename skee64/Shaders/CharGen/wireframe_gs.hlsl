cbuffer MatrixBuffer : register(b0)
{
	matrix worldMatrix;
	matrix viewMatrix;
	matrix projectionMatrix;
	float4 viewport;
};

uint infoA[] = { 0, 0, 0, 0, 1, 1, 2 };
uint infoB[] = { 1, 1, 2, 0, 2, 1, 2 };
uint infoAd[] = { 2, 2, 1, 1, 0, 0, 0 };
uint infoBd[] = { 2, 2, 1, 2, 0, 2, 1 };
uint infoEdge0[] = { 0, 2, 0, 0, 0, 0, 2 };

struct GS_INPUT
{
	float4 Pos : SV_POSITION;
	float2 tex : TEXCOORD0;
	float3 normal : NORMAL;
	float3 color : COLOR;
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

// From window pixel pos to projection frame at the specified z (view frame). 
float2 projToWindow(in float4 pos)
{
	return float2(viewport.x*0.5*((pos.x / pos.w) + 1) + viewport.z,
		viewport.y*0.5*(1 - (pos.y / pos.w)) + viewport.w);
}

[maxvertexcount(3)]
void WireframeGeometryShader(triangle GS_INPUT input[3],
	inout TriangleStream<PS_INPUT_WIRE> outStream)
{
	PS_INPUT_WIRE output;

	// Compute the case from the positions of point in space.
	output.Case = (input[0].Pos.z < 0) * 4 + (input[1].Pos.z < 0) * 2 + (input[2].Pos.z < 0);

	// If case is all vertices behind viewpoint (case = 7) then cull.
	if (output.Case == 7) return;

	output.tex = input[0].tex;
	output.normal = input[0].normal;
	output.color = input[0].color;

	// Transform position to window space
	float2 points[3];
	points[0] = projToWindow(input[0].Pos);
	points[1] = projToWindow(input[1].Pos);
	points[2] = projToWindow(input[2].Pos);

	// If Case is 0, all projected points are defined, do the
	// general case computation
	if (output.Case == 0)
	{
		output.EdgeA = float4(0, 0, 0, 0);
		output.EdgeB = float4(0, 0, 0, 0);

		// Compute the edges vectors of the transformed triangle
		float2 edges[3];
		edges[0] = points[1] - points[0];
		edges[1] = points[2] - points[1];
		edges[2] = points[0] - points[2];

		// Store the length of the edges
		float lengths[3];
		lengths[0] = length(edges[0]);
		lengths[1] = length(edges[1]);
		lengths[2] = length(edges[2]);

		// Compute the cos angle of each vertices
		float cosAngles[3];
		cosAngles[0] = dot(-edges[2], edges[0]) / (lengths[2] * lengths[0]);
		cosAngles[1] = dot(-edges[0], edges[1]) / (lengths[0] * lengths[1]);
		cosAngles[2] = dot(-edges[1], edges[2]) / (lengths[1] * lengths[2]);

		// The height for each vertices of the triangle
		float heights[3];
		heights[1] = lengths[0] * sqrt(1 - cosAngles[0] * cosAngles[0]);
		heights[2] = lengths[1] * sqrt(1 - cosAngles[1] * cosAngles[1]);
		heights[0] = lengths[2] * sqrt(1 - cosAngles[2] * cosAngles[2]);

		float edgeSigns[3];
		edgeSigns[0] = (edges[0].x > 0 ? 1 : -1);
		edgeSigns[1] = (edges[1].x > 0 ? 1 : -1);
		edgeSigns[2] = (edges[2].x > 0 ? 1 : -1);

		float edgeOffsets[3];
		edgeOffsets[0] = lengths[0] * (0.5 - 0.5*edgeSigns[0]);
		edgeOffsets[1] = lengths[1] * (0.5 - 0.5*edgeSigns[1]);
		edgeOffsets[2] = lengths[2] * (0.5 - 0.5*edgeSigns[2]);

		output.Pos = (input[0].Pos);
		output.EdgeA[0] = 0;
		output.EdgeA[1] = heights[0];
		output.EdgeA[2] = 0;
		output.EdgeB[0] = edgeOffsets[0];
		output.EdgeB[1] = edgeOffsets[1] + edgeSigns[1] * cosAngles[1] * lengths[0];
		output.EdgeB[2] = edgeOffsets[2] + edgeSigns[2] * lengths[2];
		outStream.Append(output);

		output.Pos = (input[1].Pos);
		output.EdgeA[0] = 0;
		output.EdgeA[1] = 0;
		output.EdgeA[2] = heights[1];
		output.EdgeB[0] = edgeOffsets[0] + edgeSigns[0] * lengths[0];
		output.EdgeB[1] = edgeOffsets[1];
		output.EdgeB[2] = edgeOffsets[2] + edgeSigns[2] * cosAngles[2] * lengths[1];
		outStream.Append(output);

		output.Pos = (input[2].Pos);
		output.EdgeA[0] = heights[2];
		output.EdgeA[1] = 0;
		output.EdgeA[2] = 0;
		output.EdgeB[0] = edgeOffsets[0] + edgeSigns[0] * cosAngles[0] * lengths[2];
		output.EdgeB[1] = edgeOffsets[1] + edgeSigns[1] * lengths[1];
		output.EdgeB[2] = edgeOffsets[2];
		outStream.Append(output);

		outStream.RestartStrip();
	}
	// Else need some tricky computations
	else
	{
		// Then compute and pass the edge definitions from the case
		output.EdgeA.xy = points[infoA[output.Case]];
		output.EdgeB.xy = points[infoB[output.Case]];

		output.EdgeA.zw = normalize(output.EdgeA.xy - points[infoAd[output.Case]]);
		output.EdgeB.zw = normalize(output.EdgeB.xy - points[infoBd[output.Case]]);

		// Generate vertices
		output.Pos = (input[0].Pos);
		outStream.Append(output);

		output.Pos = (input[1].Pos);
		outStream.Append(output);

		output.Pos = (input[2].Pos);
		outStream.Append(output);

		outStream.RestartStrip();
	}
}