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

float4 TestMain(in FVertexInput In) : SV_POSITION
{
	return float4( In.Position, In.VertexID, 1 );
}
