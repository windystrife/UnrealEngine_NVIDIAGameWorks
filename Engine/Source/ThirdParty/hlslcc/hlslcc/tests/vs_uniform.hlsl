struct FVertexInput
{
	float2	Position	: ATTRIBUTE0;
	float2	UV			: ATTRIBUTE1;
	uint	VertexID	: SV_VertexID;
};

struct FVertexOutput
{
	float4	Position	: SV_POSITION;
	float2	UV			: TEXCOORD0;
};

struct FUnused
{
	float4 Member1;
	float2 Member2;
};

float4 ScaleBias;

void TestMain(
	in FVertexInput In,
	out FVertexOutput Out
	)
{
	float2 TargetPos = In.Position * ScaleBias.xy + ScaleBias.zw;
	Out.Position = float4(
		TargetPos,
		In.VertexID,
		1);
    Out.UV = In.UV;
}
