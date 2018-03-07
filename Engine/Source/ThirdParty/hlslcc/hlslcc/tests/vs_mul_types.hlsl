int atype(float a) { return 0; }
int atype(float2 a) { return 1; }
int atype(float3 a) { return 2; }
int atype(float4 a) { return 3; }
int atype(float3x3 a) { return 5; }
int atype(float3x4 a) { return 6; }
int atype(float4x3 a) { return 7; }
int atype(float4x4 a) { return 8; }

void TestMain(
	float2 InPosition : ATTRIBUTE0,
	float2 InUV : ATTRIBUTE1,
	out float2 OutUV : TEXCOORD0,
	out float4 OutPosition : SV_POSITION,
	out int TypeIndex[13] : TYPEINDEX0
	)
{
	OutPosition = float4( InPosition, 0, 0 );
    OutUV = InUV;

	float4 fVector = InPosition.xyxy;
	float4x4 fMatrix = float4x4(fVector,fVector,fVector,fVector);
	float4x3 fMatrix43 = float4x3(fVector.xyz, fVector.xyz, fVector.xyz, fVector.xyz);
	float3x4 fMatrix34 = float3x4(fVector, fVector, fVector);

	TypeIndex[0] = atype(fMatrix * fMatrix);           // 8 float4x4
	TypeIndex[1] = atype(fMatrix * fVector.z);         // 8 float4x4
	TypeIndex[2] = atype(fMatrix43 * fVector.z);       // 7 float4x3
	TypeIndex[3] = atype(fMatrix * fMatrix34);         // 6 float3x4
	TypeIndex[4] = atype(fMatrix * fMatrix43);         // 7 float4x3
	TypeIndex[5] = atype(mul(fVector.x, fVector.y));   // 0 float
	TypeIndex[6] = atype(mul(fVector.x, fVector.xyz)); // 2 float3
	TypeIndex[7] = atype(mul(fVector.xyz, fVector));   // 0 float
	TypeIndex[8] = atype(mul(fMatrix,fVector));        // 3 float4
	TypeIndex[9] = atype(mul(fVector.xyz,fMatrix));    // 3 float4
	TypeIndex[10] = atype(mul(fMatrix43,fMatrix34));    // 8 float4x4
	TypeIndex[11] = atype(mul(fMatrix34,fMatrix43));    // 5 float3x3
	TypeIndex[12] = atype(mul(fMatrix34,fMatrix34));    // 6 float3x4
}
