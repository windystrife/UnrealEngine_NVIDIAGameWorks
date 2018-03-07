
[numthreads(16,16,1)]
void TestMain(
	int3 InUV : SV_GroupThreadID
	)
{
	float4 OutColor = InUV.xyyx;
}