struct VS_OUTPUT
{
    float3 cpoint : CPOINT;
};

struct HS_OUTPUT
{
    float3 hs_out_cpoint : CPOINT;
};



struct HS_CONSTANT_OUTPUT
{
    float edges[2] : SV_TessFactor;
};

HS_CONSTANT_OUTPUT HSConst()
{
    HS_CONSTANT_OUTPUT result;

    result.edges[0] = 1.0f; // Detail factor (see below for explanation)
    result.edges[1] = 8.0f; // Density factor

    return result;
}


[domain("isoline")]
[partitioning("integer")]
[outputtopology("line")]
[outputcontrolpoints(4)]
[patchconstantfunc("HSConst")]

HS_OUTPUT TestMain
(
	InputPatch
	<
		VS_OUTPUT
		,
		4
	> 
	ip : Bar,
	uint 
	id 
	:
SV_OutputControlPointID
)
{
    HS_OUTPUT result;
	
		
	VS_OUTPUT patch = ip[2];

	
    result
		.
		hs_out_cpoint 
		= 
		ip
		[
			id
		]
		.
		cpoint;

		
    return result;
}
