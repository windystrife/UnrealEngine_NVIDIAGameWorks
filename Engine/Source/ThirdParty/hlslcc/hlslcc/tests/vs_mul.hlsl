float4x4 LocalToWorld;
float4 MatrixRows[8];

void TestMain(
	in float4 InPosition : POSITION,
	out float4 OutPosition: POSITION,
	out float4 Misc[9] : MISC0
	)
{
	float4x4 A = float4x4(MatrixRows[0], MatrixRows[1], MatrixRows[2], MatrixRows[3]);
	float4x4 B = float4x4(MatrixRows[4], MatrixRows[5], MatrixRows[6], MatrixRows[7]);

	Misc[4] = mul(MatrixRows[0].x, MatrixRows[1]);
	Misc[5] = mul(MatrixRows[0], MatrixRows[1]).xxxx;
	Misc[6] = mul(A, InPosition);
	Misc[7] = mul(InPosition, A);
	Misc[8] = mul(LocalToWorld, InPosition);
	OutPosition = mul(InPosition, LocalToWorld);

	//float4x4 C = mul(A,B);
	float3x4 C = mul((float3x4)A, (float3x4)B);
	Misc[0] = C[0];
	Misc[1] = C[1];
	Misc[2] = C[2];
	Misc[3] = C[2];// C[3];
}
