// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ConvexDecompTool.cpp: Utility for turning graphics mesh into convex hulls.
=============================================================================*/

#include "ConvexDecompTool.h"


#include "Misc/FeedbackContext.h"
#include "Settings/EditorExperimentalSettings.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"

#include "ThirdParty/VHACD/public/VHACD.h"

DEFINE_LOG_CATEGORY_STATIC(LogConvexDecompTool, Log, All);

using namespace VHACD;

class FVHACDProgressCallback : public IVHACD::IUserCallback
{
public:
	FVHACDProgressCallback(void) {}
	~FVHACDProgressCallback() {};

	void Update(const double overallProgress, const double stageProgress, const double operationProgress, const char * const stage,	const char * const    operation)
	{
		FString StatusString = FString::Printf(TEXT("Processing [%s]..."), ANSI_TO_TCHAR(stage));

		GWarn->StatusUpdate(stageProgress*10.f, 1000, FText::FromString(StatusString));
		//GWarn->StatusUpdate(overallProgress*10.f, 1000, FText::FromString(StatusString));
	};
};

//#define DEBUG_VHACD
#ifdef DEBUG_VHACD
class VHACDLogger : public IVHACD::IUserLogger
{
public:
	virtual ~VHACDLogger() {};
	virtual void Log(const char * const msg) override
	{
		UE_LOG(LogConvexDecompTool, Log, TEXT("VHACD: %s"), ANSI_TO_TCHAR(msg));
	}
};
#endif // DEBUG_VHACD

void DecomposeMeshToHulls(UBodySetup* InBodySetup, const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, float InAccuracy, int32 InMaxHullVerts)
{
	check(InBodySetup != NULL);

	bool bSuccess = false;

	// Validate input by checking bounding box
	FBox VertBox(ForceInit);
	for (FVector Vert : InVertices)
	{
		VertBox += Vert;
	}

	// If box is invalid, or the largest dimension is less than 1 unit, or smallest is less than 0.1, skip trying to generate collision (V-HACD often crashes...)
	if (VertBox.IsValid == 0 || VertBox.GetSize().GetMax() < 1.f || VertBox.GetSize().GetMin() < 0.1f)
	{
		return;
	}

	FVHACDProgressCallback VHACD_Callback;

	IVHACD::Parameters VHACD_Params;
	VHACD_Params.m_resolution = 1000000; // Maximum number of voxels generated during the voxelization stage (default=100,000, range=10,000-16,000,000)
	VHACD_Params.m_maxNumVerticesPerCH = InMaxHullVerts; // Controls the maximum number of triangles per convex-hull (default=64, range=4-1024)
	VHACD_Params.m_concavity = 0.3f * (1.f - FMath::Clamp(InAccuracy, 0.f, 1.f)); // Maximum allowed concavity (default=0.0025, range=0.0-1.0)
	VHACD_Params.m_callback = &VHACD_Callback;
	VHACD_Params.m_oclAcceleration = false;
	VHACD_Params.m_minVolumePerCH = 0.003f; // this should be around 1 / (3 * m_resolution ^ (1/3))

#ifdef DEBUG_VHACD
	VHACDLogger logger;
	VHACD_Params.m_logger = &logger;
#endif //DEBUG_VHACD

	GWarn->BeginSlowTask(NSLOCTEXT("ConvexDecompTool", "BeginCreatingCollisionTask", "Creating Collision"), true, false);

	IVHACD* InterfaceVHACD = CreateVHACD();
	
	const float* const Verts = (float*)InVertices.GetData();
	const unsigned int NumVerts = InVertices.Num();
	const int* const Tris = (int*)InIndices.GetData();
	const unsigned int NumTris = InIndices.Num() / 3;

	bSuccess = InterfaceVHACD->Compute(Verts, 3, NumVerts, Tris, 3, NumTris, VHACD_Params);
	
	GWarn->EndSlowTask();

	if(bSuccess)
	{
		// Clean out old hulls
		InBodySetup->RemoveSimpleCollision();

		// Iterate over each result hull
		int32 NumHulls = InterfaceVHACD->GetNConvexHulls();


		for(int32 HullIdx=0; HullIdx<NumHulls; HullIdx++)
		{
			// Create a new hull in the aggregate geometry
			FKConvexElem ConvexElem;

			IVHACD::ConvexHull Hull;
			InterfaceVHACD->GetConvexHull(HullIdx, Hull);
			for (uint32 VertIdx = 0; VertIdx < Hull.m_nPoints; VertIdx++)
			{
				FVector V;
				V.X = (float)(Hull.m_points[(VertIdx * 3) + 0]);
				V.Y = (float)(Hull.m_points[(VertIdx * 3) + 1]);
				V.Z = (float)(Hull.m_points[(VertIdx * 3) + 2]);

				ConvexElem.VertexData.Add(V);
			}			

			ConvexElem.UpdateElemBox();

			InBodySetup->AggGeom.ConvexElems.Add(ConvexElem);
		}

		InBodySetup->InvalidatePhysicsData(); // update GUID
	}
	

	InterfaceVHACD->Clean();
	InterfaceVHACD->Release();
}
