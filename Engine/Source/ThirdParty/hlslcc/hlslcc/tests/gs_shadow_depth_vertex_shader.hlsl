
/** View projection matrix for each cube map face. */
row_major float4x4 ShadowViewProjectionMatrices[6];

/** Flag indicating which faces of the cube map the object is visible to.  This just needs 6 bits, but there's no int parameter type yet. */
float4 MeshVisibleToFace[6];

#define UNROLL
#define BRANCH

#define TANGENTTOWORLD0					TEXCOORD0
#define TANGENTTOWORLD2					TEXCOORD1
#define NUM_MATERIAL_TEXCOORDS 1

#define TANGENTTOWORLD_INTERPOLATOR_BLOCK	float4 TangentToWorld0 : TANGENTTOWORLD0; float4	TangentToWorld2	: TANGENTTOWORLD2;

struct FVertexFactoryInterpolantsVSToPS
{
	TANGENTTOWORLD_INTERPOLATOR_BLOCK

	float4	Color					: COLOR0;
	float2  PerInstanceParams			: COLOR1;
	float2	LightMapCoordinate			: TEXCOORD2;
	float4	TexCoords[(NUM_MATERIAL_TEXCOORDS+1)/2]	: TEXCOORD3;
};

/** Data passed from the vertex shader to the pixel shader for the shadow depth pass. */
struct FShadowDepthVSToPS 
{
	FVertexFactoryInterpolantsVSToPS FactoryInterpolants;

	float4 GSPosition : TEXCOORD6;
	float4 PixelPosition	: TEXCOORD7;
};

struct FShadowDepthGSToPS
{
	FShadowDepthVSToPS PSInputs;

	/** Controls which of the cube map faces to rasterize the primitive to, only the value from the first vertex is used. */
	uint RTIndex : SV_RenderTargetArrayIndex;
	float4 OutPosition : SV_POSITION;
};
 
[maxvertexcount(18)]
void TestMain(triangle FShadowDepthVSToPS Input[3], inout TriangleStream<FShadowDepthGSToPS> OutStream)
{
	UNROLL
	// Clone the triangle to each face
    for (int CubeFaceIndex = 0; CubeFaceIndex < 6; CubeFaceIndex++)
    {
		BRANCH
		// Skip this cube face if the object is not visible to it
		if (MeshVisibleToFace[CubeFaceIndex].x > 0)
		{
			float4 ClipSpacePositions[3];
			UNROLL
			for (int VertexIndex = 0; VertexIndex < 3; VertexIndex++)
			{
				// Calculate the clip space position for each cube face
				ClipSpacePositions[VertexIndex] = mul(Input[VertexIndex].GSPosition, ShadowViewProjectionMatrices[CubeFaceIndex]);
			}

			float4 FrustumTests0 = saturate(ClipSpacePositions[0].xyxy * float4(-1, -1, 1, 1) - ClipSpacePositions[0].w);		
			float4 FrustumTests1 = saturate(ClipSpacePositions[1].xyxy * float4(-1, -1, 1, 1) - ClipSpacePositions[1].w);		
			float4 FrustumTests2 = saturate(ClipSpacePositions[2].xyxy * float4(-1, -1, 1, 1) - ClipSpacePositions[2].w);		
			float4 FrustumTests = FrustumTests0 * FrustumTests1 * FrustumTests2;		

			BRANCH		
			// Frustum culling, saves GPU time with high poly meshes
			if (!any(FrustumTests != 0))		
			{				
				FShadowDepthGSToPS Output;
				Output.RTIndex = CubeFaceIndex;

				UNROLL
				for (int VertexIndex = 0; VertexIndex < 3; VertexIndex++)
				{
					Output.OutPosition = ClipSpacePositions[VertexIndex];
					Output.PSInputs.FactoryInterpolants = Input[VertexIndex].FactoryInterpolants;
					Output.PSInputs.GSPosition = Output.OutPosition;

					Output.PSInputs.PixelPosition = Input[VertexIndex].GSPosition;
					OutStream.Append(Output);
				}
				OutStream.RestartStrip();
			}
		}
	}
}
