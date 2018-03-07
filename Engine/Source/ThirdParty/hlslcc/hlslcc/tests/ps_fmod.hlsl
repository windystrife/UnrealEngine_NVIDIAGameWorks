void TestMain(
	out float4 OutColor0 : SV_Target0,
	out float4 OutColor1 : SV_Target1
	)
{
	OutColor0.x = fmod( 2.5f,  1.0f); // +0.5
	OutColor0.y = fmod(-2.5f,  1.0f); // -0.5
	OutColor0.z = fmod( 2.5f, -1.0f); // +0.5
	OutColor0.w = fmod(-2.5f, -1.0f); // -0.5

	OutColor1.x = floor( 2.5f); // +2.0
	OutColor1.y = floor(-2.5f); // -3.0
	OutColor1.z = trunc( 2.5f); // +2.0
	OutColor1.w = trunc(-2.5f); // -2.0
}
