
struct FCopyToCubeFaceVSOutput
{
	float2 UV : TEXCOORD0;
	float3 ScreenVector : TEXCOORD1;
	float4 Position : SV_POSITION;
};

struct FRouteToCubeFaceGSOut
{
	FCopyToCubeFaceVSOutput Vertex;
	uint LayerIndex : SV_RenderTargetArrayIndex;
};

int TargetCaptureIndex;
int CubeFace;

[maxvertexcount(3)]
void TestMain(triangle FCopyToCubeFaceVSOutput Input[3], inout TriangleStream<FRouteToCubeFaceGSOut> OutStream)
{
	FRouteToCubeFaceGSOut Vertex0;
	Vertex0.Vertex = Input[0];
	Vertex0.LayerIndex = CubeFace + 6 * TargetCaptureIndex;

	FRouteToCubeFaceGSOut Vertex1;
	Vertex1.Vertex = Input[1];
	Vertex1.LayerIndex = CubeFace + 6 * TargetCaptureIndex;

	FRouteToCubeFaceGSOut Vertex2;
	Vertex2.Vertex = Input[2];
	Vertex2.LayerIndex = CubeFace + 6 * TargetCaptureIndex;

	OutStream.Append(Vertex0);
	OutStream.Append(Vertex1);
	OutStream.Append(Vertex2);
}
