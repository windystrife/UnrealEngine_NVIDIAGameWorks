// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShadowSetupMobile.cpp: Shadow setup implementation for mobile specific features.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "EngineDefines.h"
#include "ConvexVolume.h"
#include "RendererInterface.h"
#include "GenericOctree.h"
#include "LightSceneInfo.h"
#include "SceneRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"

static TAutoConsoleVariable<int32> CVarCsmShaderCullingDebugGfx(
	TEXT("r.Mobile.Shadow.CSMShaderCullingDebugGfx"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarsCsmShaderCullingDisableCasterTest(
	TEXT("r.Mobile.Shadow.CSMShaderCullingDisableCasterTest"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarsCsmShaderCullingCombineCasters(
	TEXT("r.Mobile.Shadow.CSMShaderCullingCombineCasters"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarsCsmShaderCullingTestBox(
	TEXT("r.Mobile.Shadow.CSMShaderCullingTestBox"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe);

static bool CouldStaticMeshEverReceiveCSMFromStationaryLight(ERHIFeatureLevel::Type FeatureLevel, const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FStaticMesh& StaticMesh)
{
	// test if static shadows are allowed in the first place:
	static auto* CVarMobileAllowDistanceFieldShadows = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowDistanceFieldShadows"));
	const bool bMobileAllowDistanceFieldShadows = CVarMobileAllowDistanceFieldShadows->GetValueOnRenderThread() == 1;

	bool bHasCSMApplicableLightInteraction = bMobileAllowDistanceFieldShadows && StaticMesh.LCI && StaticMesh.LCI->GetLightMapInteraction(FeatureLevel).GetType() == LMIT_Texture;
	bool bHasCSMApplicableShadowInteraction = bHasCSMApplicableLightInteraction && StaticMesh.LCI && StaticMesh.LCI->GetShadowMapInteraction().GetType() == SMIT_Texture;

	return (bHasCSMApplicableLightInteraction && bHasCSMApplicableShadowInteraction) ||
		(!bHasCSMApplicableLightInteraction && PrimitiveSceneInfo->Proxy->IsMovable());
}

static bool EnableStaticMeshCombinedStaticAndCSMVisibilityState(const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMobileCSMVisibilityInfo& MobileCSMVisibilityInfo, FViewInfo& View)
{
	bool bFoundReceiver = false;
	if (MobileCSMVisibilityInfo.MobilePrimitiveCSMReceiverVisibilityMap[PrimitiveSceneInfo->GetIndex()])
	{
		return bFoundReceiver;
	}

	MobileCSMVisibilityInfo.MobilePrimitiveCSMReceiverVisibilityMap[PrimitiveSceneInfo->GetIndex()] = true;
	INC_DWORD_STAT_BY(STAT_CSMStaticPrimitiveReceivers, 1);
	for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
	{
		const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];

		bool bHasCSMApplicableShadowInteraction = View.StaticMeshVisibilityMap[StaticMesh.Id] && StaticMesh.LCI;
		bHasCSMApplicableShadowInteraction = bHasCSMApplicableShadowInteraction && StaticMesh.LCI->GetShadowMapInteraction().GetType() == SMIT_Texture;

		if (CouldStaticMeshEverReceiveCSMFromStationaryLight(View.GetFeatureLevel(), PrimitiveSceneInfo, StaticMesh))
		{
			const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh.MaterialRenderProxy;
			const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
			const EMaterialShadingModel ShadingModel = Material->GetShadingModel();
			const bool bIsLitMaterial = ShadingModel != MSM_Unlit;
			if (bIsLitMaterial)
			{
				// CSM enabled list
				MobileCSMVisibilityInfo.MobileCSMStaticMeshVisibilityMap[StaticMesh.Id] = MobileCSMVisibilityInfo.MobileNonCSMStaticMeshVisibilityMap[StaticMesh.Id];
				// CSM excluded list
				MobileCSMVisibilityInfo.MobileNonCSMStaticMeshVisibilityMap[StaticMesh.Id] = false;

				if (StaticMesh.bRequiresPerElementVisibility)
				{
					// CSM enabled list
					MobileCSMVisibilityInfo.MobileCSMStaticBatchVisibility[StaticMesh.BatchVisibilityId] = MobileCSMVisibilityInfo.MobileNonCSMStaticBatchVisibility[StaticMesh.BatchVisibilityId];
					// CSM excluded list
					MobileCSMVisibilityInfo.MobileNonCSMStaticBatchVisibility[StaticMesh.BatchVisibilityId] = 0;
				}
				
				INC_DWORD_STAT_BY(STAT_CSMStaticMeshReceivers, 1);
				bFoundReceiver = true;
			}
		}
	}
	return bFoundReceiver;
}

static bool ShouldPrimitiveReceiveCombinedCSMAndStaticShadowsFromStationaryLights(FPrimitiveSceneProxy* PrimitiveProxy)
{
	static auto* ConsoleVarAllReceiveDynamicCSM = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllReceiveDynamicCSM"));
	return PrimitiveProxy->ShouldReceiveCombinedCSMAndStaticShadowsFromStationaryLights() || ConsoleVarAllReceiveDynamicCSM->GetValueOnRenderThread() != 0;
}

template<typename TReceiverFunc>
static bool MobileDetermineStaticMeshesCSMVisibilityStateInner(
	FScene* Scene,
	FViewInfo& View,
	const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact,
	FProjectedShadowInfo* WholeSceneShadow,
	TReceiverFunc IsReceiverFunc
	)
{
	FConvexVolume ViewVolume;
	FVector LightDir = WholeSceneShadow->GetLightSceneInfo().Proxy->GetDirection();
	const float ShadowCastLength = WORLD_MAX;

	FPrimitiveSceneInfo* RESTRICT	PrimitiveSceneInfo = PrimitiveSceneInfoCompact.PrimitiveSceneInfo;
	FPrimitiveSceneProxy* RESTRICT	PrimitiveProxy = PrimitiveSceneInfoCompact.Proxy;
	const FBoxSphereBounds&			PrimitiveBounds = PrimitiveSceneInfoCompact.Bounds;
	bool bFoundCSMReceiver = false;
	if (PrimitiveProxy->WillEverBeLit() && ShouldPrimitiveReceiveCombinedCSMAndStaticShadowsFromStationaryLights(PrimitiveProxy)
		&& (PrimitiveProxy->GetLightingChannelMask() & WholeSceneShadow->GetLightSceneInfo().Proxy->GetLightingChannelMask()) != 0)
	{
		FProjectedShadowInfo* RESTRICT ProjectedShadowInfo = WholeSceneShadow;

		if (ProjectedShadowInfo->bReflectiveShadowmap && !PrimitiveProxy->AffectsDynamicIndirectLighting())
		{
			return bFoundCSMReceiver;
		}

		FLightSceneProxy* RESTRICT LightProxy = ProjectedShadowInfo->GetLightSceneInfo().Proxy;
		check(LightProxy->UseCSMForDynamicObjects());
		const FVector LightDirection = LightProxy->GetDirection();
		const FVector PrimitiveToShadowCenter = ProjectedShadowInfo->ShadowBounds.Center - PrimitiveBounds.Origin;
		// Project the primitive's bounds origin onto the light vector
		const float ProjectedDistanceFromShadowOriginAlongLightDir = PrimitiveToShadowCenter | LightDirection;
		// Calculate the primitive's squared distance to the cylinder's axis
		const float PrimitiveDistanceFromCylinderAxisSq = (-LightDirection * ProjectedDistanceFromShadowOriginAlongLightDir + PrimitiveToShadowCenter).SizeSquared();
		const float CombinedRadiusSq = FMath::Square(ProjectedShadowInfo->ShadowBounds.W + PrimitiveBounds.SphereRadius);

		// Include all primitives for movable lights, but only statically shadowed primitives from a light with static shadowing,
		// Since lights with static shadowing still create per-object shadows for primitives without static shadowing.
		if ((!LightProxy->HasStaticLighting() || (!ProjectedShadowInfo->GetLightSceneInfo().IsPrecomputedLightingValid() || LightProxy->UseCSMForDynamicObjects()))
			// Check if this primitive is in the shadow's cylinder
			&& PrimitiveDistanceFromCylinderAxisSq < CombinedRadiusSq
			// Check if the primitive is closer than the cylinder cap toward the light
			// next line is commented as it breaks large world shadows, if this was meant to be an optimization we should think about a better solution
			//// && ProjectedDistanceFromShadowOriginAlongLightDir - PrimitiveBounds.SphereRadius < -ProjectedShadowInfo->MinPreSubjectZ
			// If the primitive is further along the cone axis than the shadow bounds origin, 
			// Check if the primitive is inside the spherical cap of the cascade's bounds
			&& !(ProjectedDistanceFromShadowOriginAlongLightDir < 0
				&& PrimitiveToShadowCenter.SizeSquared() > CombinedRadiusSq))
		{
			FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[WholeSceneShadow->GetLightSceneInfo().Id];

			const FPrimitiveViewRelevance& Relevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneInfo->GetIndex()];
			const bool bLit = (Relevance.ShadingModelMaskRelevance & (1 << MSM_Unlit)) == 0;
			bool bCanReceiveDynamicShadow =
				bLit
				&& (Relevance.bOpaqueRelevance || Relevance.bMaskedRelevance)
				&& IsReceiverFunc(PrimitiveBounds.Origin, PrimitiveBounds.BoxExtent, PrimitiveBounds.SphereRadius);

			if (bCanReceiveDynamicShadow)
			{
				bFoundCSMReceiver = EnableStaticMeshCombinedStaticAndCSMVisibilityState(PrimitiveSceneInfo, View.MobileCSMVisibilityInfo, View);
			}
		}
	}
	return bFoundCSMReceiver;
}

template<typename TReceiverFunc>
static bool MobileDetermineStaticMeshesCSMVisibilityState(FScene* Scene, FViewInfo& View, FProjectedShadowInfo* WholeSceneShadow, TReceiverFunc IsReceiverFunc)
{
	bool bFoundReceiver = false;
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ShadowOctreeTraversal);
		// Find primitives that are in a shadow frustum in the octree.
		for (FScenePrimitiveOctree::TConstIterator<SceneRenderingAllocator> PrimitiveOctreeIt(Scene->PrimitiveOctree);
		PrimitiveOctreeIt.HasPendingNodes();
			PrimitiveOctreeIt.Advance())
		{
			const FScenePrimitiveOctree::FNode& PrimitiveOctreeNode = PrimitiveOctreeIt.GetCurrentNode();
			const FOctreeNodeContext& PrimitiveOctreeNodeContext = PrimitiveOctreeIt.GetCurrentContext();

			// Find children of this octree node that may contain relevant primitives.
			FOREACH_OCTREE_CHILD_NODE(ChildRef)
			{
				if (PrimitiveOctreeNode.HasChild(ChildRef))
				{
					// Check that the child node is in the frustum for at least one shadow.
					const FOctreeNodeContext ChildContext = PrimitiveOctreeNodeContext.GetChildContext(ChildRef);
					bool bCanReceiveDynamicShadow = IsReceiverFunc(FVector(ChildContext.Bounds.Center), FVector(ChildContext.Bounds.Extent), ChildContext.Bounds.Extent.Size3());

					if (bCanReceiveDynamicShadow)
					{
						PrimitiveOctreeIt.PushChild(ChildRef);
					}
				}
			}

			// Check all the primitives in this octree node.
			for (FScenePrimitiveOctree::ElementConstIt NodePrimitiveIt(PrimitiveOctreeNode.GetElementIt()); NodePrimitiveIt; ++NodePrimitiveIt)
			{
				// gather the shadows for this one primitive
				bFoundReceiver = MobileDetermineStaticMeshesCSMVisibilityStateInner(Scene, View, *NodePrimitiveIt, WholeSceneShadow, IsReceiverFunc) || bFoundReceiver;
			}
		}
	}
	return bFoundReceiver;
}

static void VisualizeMobileDynamicCSMSubjectCapsules(FViewInfo& View, FLightSceneInfo* LightSceneInfo)
{
	auto DrawDebugCapsule = [](FViewInfo& InView, const FLightSceneInfo* InLightSceneInfo, const FVector& Start, float CastLength, float CapsuleRadius)
	{
		const FMatrix& LightToWorld = InLightSceneInfo->Proxy->GetLightToWorld();
		FViewElementPDI ShadowFrustumPDI(&InView, NULL);
		FVector Dir = LightToWorld.GetUnitAxis(EAxis::X);
		FVector End = Start + (Dir*CastLength);
		DrawWireSphere(&ShadowFrustumPDI, FTransform(Start), FColor::White, CapsuleRadius, 40, 0);
		DrawWireCapsule(&ShadowFrustumPDI, Start + Dir*0.5f*CastLength, LightToWorld.GetUnitAxis(EAxis::Z), LightToWorld.GetUnitAxis(EAxis::Y), Dir,
			FColor(231, 0, 0, 255), CapsuleRadius, 0.5f * CastLength + CapsuleRadius, 25, SDPG_World);
		ShadowFrustumPDI.DrawLine(Start, End, FColor::Black, 0);
	};

	FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
	FMobileCSMSubjectPrimitives& MobileCSMSubjectPrimitives = VisibleLightViewInfo.MobileCSMSubjectPrimitives;
	FVector LightDir = LightSceneInfo->Proxy->GetDirection();
	const float ShadowCastLength = WORLD_MAX;
	if (CVarsCsmShaderCullingCombineCasters.GetValueOnRenderThread())
	{
		// Combined bounds
		FVector CombinedCasterStart;
		FVector CombinedCasterEnd;
		FBoxSphereBounds CombinedBounds(EForceInit::ForceInitToZero);
		for (auto& Caster : MobileCSMSubjectPrimitives.GetShadowSubjectPrimitives())
		{
			CombinedBounds = (CombinedBounds.SphereRadius > 0.0f) ? CombinedBounds + Caster->Proxy->GetBounds() : Caster->Proxy->GetBounds();
		}
		CombinedCasterStart = CombinedBounds.Origin;
		CombinedCasterEnd = CombinedBounds.Origin + (LightDir * ShadowCastLength);

		DrawDebugCapsule(View, LightSceneInfo, CombinedCasterStart, ShadowCastLength, CombinedBounds.SphereRadius);
	}
	else
	{
		for (auto& Caster : MobileCSMSubjectPrimitives.GetShadowSubjectPrimitives())
		{
			const FBoxSphereBounds& CasterBounds = Caster->Proxy->GetBounds();
			const FVector& CasterStart = CasterBounds.Origin;
			const FVector CasterEnd = CasterStart + (LightDir * ShadowCastLength);
			DrawDebugCapsule(View, LightSceneInfo, CasterStart, ShadowCastLength, CasterBounds.SphereRadius);
		}
	}
}
/** Finds the visible dynamic shadows for each view. */
void FMobileSceneRenderer::InitDynamicShadows(FRHICommandListImmediate& RHICmdList)
{
	static auto* MyCVarMobileEnableStaticAndCSMShadowReceivers = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
	static auto* MyCVarEnableCsmShaderCulling = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.Shadow.CSMShaderCulling"));
	const bool bMobileEnableStaticAndCSMShadowReceivers = MyCVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnRenderThread() == 1;

	const bool bCombinedStaticAndCSMEnabled = MyCVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnRenderThread()
		&& MyCVarEnableCsmShaderCulling->GetValueOnRenderThread();

	// initialize CSMVisibilityInfo for each eligible light.
	for (FLightSceneInfo* MobileDirectionalLightSceneInfo : Scene->MobileDirectionalLights)
	{
		const FLightSceneProxy* LightSceneProxy = MobileDirectionalLightSceneInfo ? MobileDirectionalLightSceneInfo->Proxy : nullptr;
		if (LightSceneProxy)
		{
			bool bLightHasCombinedStaticAndCSMEnabled = bCombinedStaticAndCSMEnabled && LightSceneProxy->UseCSMForDynamicObjects();

			if (bLightHasCombinedStaticAndCSMEnabled)
			{
				int32 PrimitiveCount = Scene->Primitives.Num();
				for (auto& View : Views)
				{
					FMobileCSMSubjectPrimitives& MobileCSMSubjectPrimitives = View.VisibleLightInfos[MobileDirectionalLightSceneInfo->Id].MobileCSMSubjectPrimitives;
					MobileCSMSubjectPrimitives.InitShadowSubjectPrimitives(PrimitiveCount);
				}
			}
		}
	}

	FSceneRenderer::InitDynamicShadows(RHICmdList);

	// Prepare view's visibility lists.
	// TODO: only do this when CSM + static is required.
	for (auto& View : Views)
	{
		FMobileCSMVisibilityInfo& MobileCSMVisibilityInfo = View.MobileCSMVisibilityInfo;
		// Init list of primitives that can receive Dynamic CSM.
		MobileCSMVisibilityInfo.MobilePrimitiveCSMReceiverVisibilityMap.Init(false, View.PrimitiveVisibilityMap.Num());

		// Init static mesh visibility info for CSM drawlist
		MobileCSMVisibilityInfo.MobileCSMStaticMeshVisibilityMap.Init(false, View.StaticMeshVisibilityMap.Num());
		MobileCSMVisibilityInfo.MobileCSMStaticBatchVisibility.AddZeroed(View.StaticMeshBatchVisibility.Num());

		// Init static mesh visibility info for default drawlist that excludes meshes in CSM only drawlist.
		MobileCSMVisibilityInfo.MobileNonCSMStaticMeshVisibilityMap = View.StaticMeshVisibilityMap;
		MobileCSMVisibilityInfo.MobileNonCSMStaticBatchVisibility = View.StaticMeshBatchVisibility;
	}

	for (FLightSceneInfo* MobileDirectionalLightSceneInfo : Scene->MobileDirectionalLights)
	{
		const FLightSceneProxy* LightSceneProxy = MobileDirectionalLightSceneInfo ? MobileDirectionalLightSceneInfo->Proxy : nullptr;
		if (LightSceneProxy)
		{
			bool bLightHasCombinedStaticAndCSMEnabled = bCombinedStaticAndCSMEnabled && LightSceneProxy->UseCSMForDynamicObjects();

			if (bLightHasCombinedStaticAndCSMEnabled)
			{
				BuildCombinedStaticAndCSMVisibilityState(MobileDirectionalLightSceneInfo);
			}
		}
	}

	{
		// Check for modulated shadows. 
		bModulatedShadowsInUse = false;
		for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt && bModulatedShadowsInUse == false; ++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
			FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
			// Mobile renderer only projects modulated shadows.
			bModulatedShadowsInUse = VisibleLightInfo.ShadowsToProject.Num() > 0;
		}
	}
}

// Build visibility lists of CSM receivers and non-csm receivers.
void FMobileSceneRenderer::BuildCombinedStaticAndCSMVisibilityState(FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_BuildCombinedStaticAndCSMVisibilityState);

	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	
	// Determine largest split, find shadow frustum to perform culling tests.
	int LargestSplitIndex = INDEX_NONE;
	FProjectedShadowInfo* ProjectedShadowInfo = nullptr;
	for (auto& ProjectedShadowInfoIt : VisibleLightInfo.AllProjectedShadows)
	{
		if(ProjectedShadowInfoIt->IsWholeSceneDirectionalShadow() )
		{
			FProjectedShadowInfo* WholeSceneShadow = ProjectedShadowInfoIt;
			if (ProjectedShadowInfoIt->CascadeSettings.ShadowSplitIndex > LargestSplitIndex)
			{
				LargestSplitIndex = ProjectedShadowInfoIt->CascadeSettings.ShadowSplitIndex;
				ProjectedShadowInfo = ProjectedShadowInfoIt;
			}
		}
	}

	if (ProjectedShadowInfo == nullptr)
	{
		return;
	}

	if (LightSceneInfo->Proxy->CastsDynamicShadow()
		&& (LightSceneInfo->Proxy->HasStaticShadowing() && LightSceneInfo->Proxy->UseCSMForDynamicObjects())
		)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			bool bStaticCSMReceiversFound = false;
			FViewInfo& View = Views[ViewIndex];

			FViewInfo* ShadowSubjectView = ProjectedShadowInfo->DependentView ? ProjectedShadowInfo->DependentView : &View;
			FVisibleLightViewInfo& VisibleLightViewInfo = ShadowSubjectView->VisibleLightInfos[LightSceneInfo->Id];
			FMobileCSMSubjectPrimitives& MobileCSMSubjectPrimitives = VisibleLightViewInfo.MobileCSMSubjectPrimitives;
			FMobileCSMVisibilityInfo& MobileCSMVisibilityInfo = View.MobileCSMVisibilityInfo;
			const auto& ShadowSubjectPrimitives = MobileCSMSubjectPrimitives.GetShadowSubjectPrimitives();
			if (ShadowSubjectPrimitives.Num() != 0)
			{
				if (CVarsCsmShaderCullingDisableCasterTest.GetValueOnRenderThread())
				{
					for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
					{
						EnableStaticMeshCombinedStaticAndCSMVisibilityState(Scene->Primitives[BitIt.GetIndex()], MobileCSMVisibilityInfo, View);
					}
					MobileCSMVisibilityInfo.bMobileDynamicCSMInUse = true;
					break;
				}
				
				if (CVarCsmShaderCullingDebugGfx.GetValueOnRenderThread())
				{
					VisualizeMobileDynamicCSMSubjectCapsules(View, LightSceneInfo);
				}

				const bool bCombineCasters = CVarsCsmShaderCullingCombineCasters.GetValueOnRenderThread() != 0;
				FVector LightDir = LightSceneInfo->Proxy->GetDirection();
				const float ShadowCastLength = WORLD_MAX;
				FVector CombinedCasterStart;
				FVector CombinedCasterEnd;
				FBoxSphereBounds CombinedBounds(EForceInit::ForceInitToZero);

				// Calculate combined bounds if needed.
				if (bCombineCasters)
				{
					for (auto& Caster : ShadowSubjectPrimitives)
					{
						CombinedBounds = (CombinedBounds.SphereRadius > 0.0f) ? CombinedBounds + Caster->Proxy->GetBounds() : Caster->Proxy->GetBounds();
					}
					CombinedCasterStart = CombinedBounds.Origin;
					CombinedCasterEnd = CombinedBounds.Origin + (LightDir * ShadowCastLength);
				}

				FConvexVolume ViewFrustum;
				GetViewFrustumBounds(ViewFrustum, View.ViewMatrices.GetViewProjectionMatrix(), true);
				FConvexVolume& ShadowReceiverFrustum = ProjectedShadowInfo->ReceiverFrustum;
				FVector& PreShadowTranslation = ProjectedShadowInfo->PreShadowTranslation;

				// Common receiver test functions.
				// Test receiver bounding box against view+shadow frusta only
				auto IsShadowReceiver = [&ViewFrustum, &ShadowReceiverFrustum, &PreShadowTranslation](const FVector& PrimOrigin, const FVector& PrimExtent)
				{
					return ViewFrustum.IntersectBox(PrimOrigin, PrimExtent)
						&& ShadowReceiverFrustum.IntersectBox(PrimOrigin + PreShadowTranslation, PrimExtent);
				};
				// Test receiver bounding box against shadow frustum only. 
				auto IsShadowReceiverNoView = [&ShadowReceiverFrustum, &PreShadowTranslation](const FVector& PrimOrigin, const FVector& PrimExtent)
				{
					return ShadowReceiverFrustum.IntersectBox(PrimOrigin + PreShadowTranslation, PrimExtent);
				};

				INC_DWORD_STAT_BY(STAT_CSMSubjects, ShadowSubjectPrimitives.Num());
				const bool bPerformBoxTests = CVarsCsmShaderCullingTestBox.GetValueOnRenderThread() != 0;
				if (bPerformBoxTests)
				{
					// Test receiver against single caster capsule vs bounding box
					auto IsShadowReceiverCasterVsBox = [](const FVector& PrimOrigin, const FVector& PrimExtent, const FVector& CasterStart, const FVector& CasterEnd, float CasterRadius)
					{
						FBox PrimBox(PrimOrigin - (PrimExtent + CasterRadius), PrimOrigin + (PrimExtent + CasterRadius));
						FVector Direction = CasterEnd - CasterStart;

						return FMath::LineBoxIntersection(PrimBox, CasterStart, CasterEnd, Direction);
					};

					// Test against all caster capsules vs bounding box
					auto IsShadowReceiverAllCastersVsBox = [&ShadowSubjectPrimitives, &IsShadowReceiverCasterVsBox, &LightDir, &ShadowCastLength](const FVector& PrimOrigin, const FVector& PrimExtent)
					{
						for (auto& Caster : ShadowSubjectPrimitives)
						{
							const FBoxSphereBounds& CasterBounds = Caster->Proxy->GetBounds();
							const FVector& CasterStart = CasterBounds.Origin;
							const FVector CasterEnd = CasterStart + (LightDir * ShadowCastLength);
							float CasterRadius = CasterBounds.SphereRadius;

							if (IsShadowReceiverCasterVsBox(PrimOrigin, PrimExtent, CasterStart, CasterEnd, CasterRadius))
							{
								return true;
							}
						}
						return false;
					};

					if (bCombineCasters)
					{
						// Test against view+shadow frustums and caster capsule vs bounding box
						auto IsShadowReceiverCombinedBox = [&IsShadowReceiver, &IsShadowReceiverCasterVsBox, &CombinedBounds, &CombinedCasterStart, &CombinedCasterEnd](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiver(PrimOrigin, PrimExtent)	&& IsShadowReceiverCasterVsBox(PrimOrigin, PrimExtent, CombinedCasterStart, CombinedCasterEnd, CombinedBounds.SphereRadius);
						};
						// Test against shadow frustum and caster capsule vs bounding box
						auto IsShadowReceiverCombinedBoxNoView = [&IsShadowReceiverNoView, &IsShadowReceiverCasterVsBox, &CombinedBounds, &CombinedCasterStart, &CombinedCasterEnd](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiverNoView(PrimOrigin, PrimExtent) && IsShadowReceiverCasterVsBox(PrimOrigin, PrimExtent, CombinedCasterStart, CombinedCasterEnd, CombinedBounds.SphereRadius);
						};
						bStaticCSMReceiversFound = MobileDetermineStaticMeshesCSMVisibilityState(Scene, View, ProjectedShadowInfo, IsShadowReceiverCombinedBox);
					}
					else
					{
						// Test against view+shadow frustums and all caster capsules vs bounding box
						auto IsShadowReceiverBoxAllCasters = [&IsShadowReceiver, &IsShadowReceiverAllCastersVsBox](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiver(PrimOrigin, PrimExtent)	&& IsShadowReceiverAllCastersVsBox(PrimOrigin, PrimExtent);
						};
						// Test against shadow frustum and all caster capsules vs bounding box
						auto IsShadowReceiverBoxPerCasterNoView = [&IsShadowReceiverNoView, &IsShadowReceiverAllCastersVsBox](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiverNoView(PrimOrigin, PrimExtent) && IsShadowReceiverAllCastersVsBox(PrimOrigin, PrimExtent);
						};
						bStaticCSMReceiversFound = MobileDetermineStaticMeshesCSMVisibilityState(Scene, View, ProjectedShadowInfo, IsShadowReceiverBoxAllCasters);
					}
				}
				else
				{
					//Test against caster capsule vs bounds sphere
					auto IsShadowReceiverCasterVsSphere = [](const FVector& PrimOrigin, float PrimRadius, const FVector& CasterStart, const FVector& CasterEnd, float CasterRadius)
					{
						return FMath::PointDistToSegmentSquared(PrimOrigin, CasterStart, CasterEnd) < FMath::Square(PrimRadius + CasterRadius);
					};

					// Test against all caster capsules vs bounding box
					auto IsShadowReceiverAllCastersVsSphere = [&ShadowSubjectPrimitives, &IsShadowReceiverCasterVsSphere, &LightDir, &ShadowCastLength](const FVector& PrimOrigin, float PrimRadius)
					{
						for (auto& Caster : ShadowSubjectPrimitives)
						{
							const FBoxSphereBounds& CasterBounds = Caster->Proxy->GetBounds();
							const FVector& CasterStart = CasterBounds.Origin;
							float CasterRadius = CasterBounds.SphereRadius;
							const FVector CasterEnd = CasterStart + (LightDir * ShadowCastLength);

							if (IsShadowReceiverCasterVsSphere(PrimOrigin, PrimRadius, CasterStart, CasterEnd, CasterRadius))
							{
								return true;
							}
						}
						return false;
					};

					if (bCombineCasters)
					{
						// Test against view+shadow frustums and caster capsule vs bounding sphere
						auto IsShadowReceiverCombined = [&IsShadowReceiver, &IsShadowReceiverCasterVsSphere, &CombinedBounds, &CombinedCasterStart, &CombinedCasterEnd](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiver(PrimOrigin, PrimExtent)	&& IsShadowReceiverCasterVsSphere(PrimOrigin, PrimRadius, CombinedCasterStart, CombinedCasterEnd, CombinedBounds.SphereRadius);
						};
						// Test against shadow frustum and caster capsule vs bounding sphere
						auto IsShadowReceiverCombinedNoView = [&IsShadowReceiverNoView, &IsShadowReceiverCasterVsSphere, &CombinedBounds, &CombinedCasterStart, &CombinedCasterEnd](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiverNoView(PrimOrigin, PrimExtent) && IsShadowReceiverCasterVsSphere(PrimOrigin, PrimRadius, CombinedCasterStart, CombinedCasterEnd, CombinedBounds.SphereRadius);
						};
						bStaticCSMReceiversFound = MobileDetermineStaticMeshesCSMVisibilityState(Scene, View, ProjectedShadowInfo, IsShadowReceiverCombined);
					}
					else // all casters test
					{
						// Test against view+shadow frustums and all caster capsules vs bounding sphere
						auto IsShadowReceiverSphereAllCasters = [&IsShadowReceiver, &IsShadowReceiverAllCastersVsSphere](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiver(PrimOrigin, PrimExtent)	&& IsShadowReceiverAllCastersVsSphere(PrimOrigin, PrimRadius);
						};

						// Test against shadow frustum and all caster capsules vs bounding sphere
						auto IsShadowReceiverSpherePerCasterNoView = [&IsShadowReceiverNoView, &IsShadowReceiverAllCastersVsSphere](const FVector& PrimOrigin, const FVector& PrimExtent, float PrimRadius)
						{
							return IsShadowReceiverNoView(PrimOrigin, PrimExtent) && IsShadowReceiverAllCastersVsSphere(PrimOrigin, PrimRadius);
						};

						bStaticCSMReceiversFound = MobileDetermineStaticMeshesCSMVisibilityState(Scene, View, ProjectedShadowInfo, IsShadowReceiverSphereAllCasters);
					}
				}
			}
			MobileCSMVisibilityInfo.bMobileDynamicCSMInUse = bStaticCSMReceiversFound;
		}
	}
}
