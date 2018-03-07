int AtmosphereLayer;

Texture2DMS<float,4> TestFloatTex;

float4x4 Matrixes[2];

struct FAtmosphereVSOutput
{
	float2 OutTexCoord[2] : TEXCOORD0;
	float4 OutPosition : SV_POSITION;
};

struct FAtmosphereGSOut
{
	FAtmosphereVSOutput Vertex;
	uint LayerIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)]
void TestMain(triangle FAtmosphereVSOutput Input[3], inout TriangleStream<FAtmosphereGSOut> OutStream)
{
	uint Width; 
	uint Height;
	uint Dummy;
	TestFloatTex.GetDimensions(Width, Height, Dummy);

	FAtmosphereGSOut Vertex0;
	Vertex0.Vertex = Input[0];
	Vertex0.Vertex.OutPosition = Matrixes[0][3];
	Vertex0.LayerIndex = Width;

	FAtmosphereGSOut Vertex1;
	Vertex1.Vertex = Input[1];
	Vertex1.Vertex.OutPosition = Matrixes[1][3];
	Vertex1.LayerIndex = Height;

	FAtmosphereGSOut Vertex2;
	Vertex2.Vertex = Input[2];
	Vertex2.Vertex.OutPosition = Matrixes[1][2];
	Vertex2.LayerIndex = Dummy;

	OutStream.Append(Vertex0);
	OutStream.Append(Vertex1);
	OutStream.Append(Vertex2);
}
