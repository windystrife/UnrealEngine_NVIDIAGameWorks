void TestMain(
	float2 InPosition : ATTRIBUTE0,
	float2 InUV : ATTRIBUTE1,
	uint VertexID : SV_VertexID,
	out float2 OutUV : TEXCOORD0,
	out float4 OutPosition : SV_POSITION
	)
{
	OutPosition = float4( InPosition, VertexID, 1 );
    OutUV = InUV;
}
