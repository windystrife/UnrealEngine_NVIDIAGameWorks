void TestMain(
	float2 InUV : TEXCOORD0,
	out float4 OutColor : SV_Target0
	)
{
	OutColor = InUV.xyyx;
}