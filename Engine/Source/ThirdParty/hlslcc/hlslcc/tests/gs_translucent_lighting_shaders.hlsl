
struct FScreenVertexOutput
{
	float2 UV : TEXCOORD0;
	float4 Position : SV_POSITION;
};

struct FWriteToSliceVertexOutput
{
	FScreenVertexOutput Vertex;
	uint LayerIndex : TEXCOORD1;
};

struct FWriteToSliceGeometryOutput
{
	FScreenVertexOutput Vertex;
	uint LayerIndex : SV_RenderTargetArrayIndex;
};

/** Z index of the minimum slice in the range. */
int MinZ;

/** Geometry shader that writes to a range of slices of a volume texture. */
[maxvertexcount(3)]
void TestMain(triangle FWriteToSliceVertexOutput Input[3], inout TriangleStream<FWriteToSliceGeometryOutput> OutStream)
{
	FWriteToSliceGeometryOutput Vertex0;
	Vertex0.Vertex = Input[0].Vertex;
	Vertex0.LayerIndex = Input[0].LayerIndex + MinZ;

	FWriteToSliceGeometryOutput Vertex1;
	Vertex1.Vertex = Input[1].Vertex;
	Vertex1.LayerIndex = Input[1].LayerIndex + MinZ;

	FWriteToSliceGeometryOutput Vertex2;
	Vertex2.Vertex = Input[2].Vertex;
	Vertex2.LayerIndex = Input[2].LayerIndex + MinZ;

	OutStream.Append(Vertex0);
	OutStream.Append(Vertex1);
	OutStream.Append(Vertex2);
}
