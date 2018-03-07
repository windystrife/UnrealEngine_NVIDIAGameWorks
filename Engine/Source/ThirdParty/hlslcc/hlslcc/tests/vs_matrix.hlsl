void TestMain(
	float2 InPosition : ATTRIBUTE0,
	float2 InUV : ATTRIBUTE1,
	out float2 OutUV : TEXCOORD0,
	out float4 OutPosition : SV_POSITION,
	out float4 OutMisc[4] : MISC0
	)
{
	float4x4 Matrix4 = 1;
	float3x3 Matrix3 = 2;
	float4x3 Matrix43 = 3;
	float3x4 Matrix34 = 4;

	float Det1 = determinant(Matrix4);
	float Det2 = determinant(Matrix3);
	
	float3x3 A = transpose(Matrix3);
	float3x4 B = transpose(Matrix43);
	float4x3 C = transpose(Matrix34);
	float2x2 D = transpose(Matrix34);

	OutMisc[0] = A[0].xyzz;
	OutMisc[1] = B[1];
	OutMisc[2] = C[2].xyzx;
	OutMisc[3] = D;

	OutPosition = float4( InPosition, 0, Det1 + Det2 );
    OutUV = InUV;
}
