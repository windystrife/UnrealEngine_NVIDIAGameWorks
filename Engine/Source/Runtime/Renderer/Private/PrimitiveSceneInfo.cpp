// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveSceneInfo.cpp: Primitive scene info implementation.
=============================================================================*/

#include "PrimitiveSceneInfo.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "SceneManagement.h"
#include "SceneCore.h"
#include "VelocityRendering.h"
#include "ScenePrivate.h"

/** An implementation of FStaticPrimitiveDrawInterface that stores the drawn elements for the rendering thread to use. */
class FBatchingSPDI : public FStaticPrimitiveDrawInterface
{
public:

	// Constructor.
	FBatchingSPDI(FPrimitiveSceneInfo* InPrimitiveSceneInfo):
		PrimitiveSceneInfo(InPrimitiveSceneInfo)
	{}

	// FStaticPrimitiveDrawInterface.
	virtual void SetHitProxy(HHitProxy* HitProxy) final override
	{
		CurrentHitProxy = HitProxy;

		if(HitProxy)
		{
			// Only use static scene primitive hit proxies in the editor.
			if(GIsEditor)
			{
				// Keep a reference to the hit proxy from the FPrimitiveSceneInfo, to ensure it isn't deleted while the static mesh still
				// uses its id.
				PrimitiveSceneInfo->HitProxies.Add(HitProxy);
			}
		}
	}

	virtual void DrawMesh(const FMeshBatch& Mesh, float ScreenSize) final override
	{
		if (Mesh.GetNumPrimitives() > 0)
		{
			check(Mesh.VertexFactory);
			check(Mesh.VertexFactory->IsInitialized());
#if DO_CHECK
			Mesh.CheckUniformBuffers();
#endif
			PrimitiveSceneInfo->Proxy->VerifyUsedMaterial(Mesh.MaterialRenderProxy);

			FStaticMesh* StaticMesh = new(PrimitiveSceneInfo->StaticMeshes) FStaticMesh(
				PrimitiveSceneInfo,
				Mesh,
				ScreenSize,
				CurrentHitProxy ? CurrentHitProxy->Id : FHitProxyId()
				);
		}
	}

private:
	FPrimitiveSceneInfo* PrimitiveSceneInfo;
	TRefCountPtr<HHitProxy> CurrentHitProxy;
};

FPrimitiveFlagsCompact::FPrimitiveFlagsCompact(const FPrimitiveSceneProxy* Proxy)
	: bCastDynamicShadow(Proxy->CastsDynamicShadow())
	, bStaticLighting(Proxy->HasStaticLighting())
	, bCastStaticShadow(Proxy->CastsStaticShadow())
{}

FPrimitiveSceneInfoCompact::FPrimitiveSceneInfoCompact(FPrimitiveSceneInfo* InPrimitiveSceneInfo) :
	PrimitiveFlagsCompact(InPrimitiveSceneInfo->Proxy)
{
	PrimitiveSceneInfo = InPrimitiveSceneInfo;
	Proxy = PrimitiveSceneInfo->Proxy;
	Bounds = PrimitiveSceneInfo->Proxy->GetBounds();
	MinDrawDistance = PrimitiveSceneInfo->Proxy->GetMinDrawDistance();
	MaxDrawDistance = PrimitiveSceneInfo->Proxy->GetMaxDrawDistance();

	VisibilityId = PrimitiveSceneInfo->Proxy->GetVisibilityId();
}

FPrimitiveSceneInfo::FPrimitiveSceneInfo(UPrimitiveComponent* InComponent,FScene* InScene):
	Proxy(InComponent->SceneProxy),
	PrimitiveComponentId(InComponent->ComponentId),
	ComponentLastRenderTime(&InComponent->LastRenderTime),
	ComponentLastRenderTimeOnScreen(&InComponent->LastRenderTimeOnScreen),
	IndirectLightingCacheAllocation(NULL),
	CachedPlanarReflectionProxy(NULL),
	CachedReflectionCaptureProxy(NULL),
	bNeedsCachedReflectionCaptureUpdate(true),
	DefaultDynamicHitProxy(NULL),
	LightList(NULL),
	LastRenderTime(-FLT_MAX),
	LastVisibilityChangeTime(0.0f),
	Scene(InScene),
	NumES2DynamicPointLights(0),
	PackedIndex(INDEX_NONE),
	ComponentForDebuggingOnly(InComponent),
	bNeedsStaticMeshUpdate(false),
	bNeedsUniformBufferUpdate(false),
	bPrecomputedLightingBufferDirty(false)
	// NVCHANGE_BEGIN: Add VXGI
	, VxgiLastVoxelizationPass(0)
	, VoxelizationOnlyMeshStartIdx(0)
	// NVCHANGE_END: Add VXGI
{
	check(ComponentForDebuggingOnly);
	check(PrimitiveComponentId.IsValid());
	check(Proxy);

	UPrimitiveComponent* SearchParentComponent = Cast<UPrimitiveComponent>(InComponent->GetAttachmentRoot());

	if (SearchParentComponent && SearchParentComponent != InComponent)
	{
		LightingAttachmentRoot = SearchParentComponent->ComponentId;
	}

	// Only create hit proxies in the Editor as that's where they are used.
	if (GIsEditor)
	{
		// Create a dynamic hit proxy for the primitive. 
		DefaultDynamicHitProxy = Proxy->CreateHitProxies(InComponent,HitProxies);
		if( DefaultDynamicHitProxy )
		{
			DefaultDynamicHitProxyId = DefaultDynamicHitProxy->Id;
		}
	}

	// set LOD parent info if exists
	UPrimitiveComponent* LODParent = InComponent->GetLODParentPrimitive();
	if (LODParent)
	{
		LODParentComponentId = LODParent->ComponentId;
	}

	FMemory::Memzero(CachedReflectionCaptureProxies);
}

FPrimitiveSceneInfo::~FPrimitiveSceneInfo()
{
	check(!OctreeId.IsValidId());
}

void FPrimitiveSceneInfo::AddStaticMeshes(FRHICommandListImmediate& RHICmdList)
{
	// Cache the primitive's static mesh elements.
	FBatchingSPDI BatchingSPDI(this);
	BatchingSPDI.SetHitProxy(DefaultDynamicHitProxy);
	Proxy->DrawStaticElements(&BatchingSPDI);

	// NVCHANGE_BEGIN: Add VXGI
	VoxelizationOnlyMeshStartIdx = StaticMeshes.Num();
#if WITH_GFSDK_VXGI
	if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5 && StaticMeshes.Num() > 0
		&& RHISupportsTessellation(GShaderPlatformForFeatureLevel[Scene->GetFeatureLevel()]))
	{
		bool bRequiresDifferentIndexBufferForVoxelization = false;
		for (int32 MeshIndex = 0; MeshIndex < VoxelizationOnlyMeshStartIdx && !bRequiresDifferentIndexBufferForVoxelization; MeshIndex++)
		{
			FStaticMesh& Mesh = StaticMeshes[MeshIndex];
			if (!Mesh.VertexFactory->GetType()->SupportsTessellationShaders())
			{
				continue;
			}
			const FMaterial* MaterialResource = Mesh.MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
			check(MaterialResource);
			
			// This partially duplicates RequiresAdjacencyInformation but the logic is simple
			auto TessellationMode = MaterialResource->GetTessellationMode();
			bool bEnableCrackFreeDisplacement = MaterialResource->IsCrackFreeDisplacementEnabled();
			bool bRequiresAdjacencyInformation = (TessellationMode == MTM_PNTriangles) || (TessellationMode == MTM_FlatTessellation && bEnableCrackFreeDisplacement);
			
			bool bUsedWithVxgiVoxelization = MaterialResource->GetVxgiMaterialProperties().bUsedWithVxgiVoxelization;
			bool bVxgiAllowTesselationDuringVoxelization = MaterialResource->GetVxgiMaterialProperties().bVxgiAllowTesselationDuringVoxelization;
			bool bIsTranslucent = IsTranslucentBlendMode(MaterialResource->GetBlendMode());

			bRequiresDifferentIndexBufferForVoxelization = bRequiresAdjacencyInformation
				&& bUsedWithVxgiVoxelization
				&& !bVxgiAllowTesselationDuringVoxelization
				&& !bIsTranslucent;
		}

		if (bRequiresDifferentIndexBufferForVoxelization)
		{
			//Set this flag so that inside here we will pick the right index buffer
			RHIPushVoxelizationFlag();
			//Add the meshes a second time to StaticMeshes making it larger than VoxelizationOnlyMeshStartIdx
			Proxy->DrawStaticElements(&BatchingSPDI);
			RHIPopVoxelizationFlag();
		}
		check(VoxelizationOnlyMeshStartIdx == StaticMeshes.Num() || bRequiresDifferentIndexBufferForVoxelization);
	}
#endif

	StaticMeshes.Shrink();

	for(int32 MeshIndex = 0;MeshIndex < StaticMeshes.Num();MeshIndex++)
	{
		FStaticMesh& Mesh = StaticMeshes[MeshIndex];

		// Add the static mesh to the scene's static mesh list.
		FSparseArrayAllocationInfo SceneArrayAllocation = Scene->StaticMeshes.AddUninitialized();
		Scene->StaticMeshes[SceneArrayAllocation.Index] = &Mesh;
		Mesh.Id = SceneArrayAllocation.Index;

		if (Mesh.bRequiresPerElementVisibility)
		{
			// Use a separate index into StaticMeshBatchVisibility, since most meshes don't use it
			Mesh.BatchVisibilityId = Scene->StaticMeshBatchVisibility.AddUninitialized().Index;
		}

		if (MeshIndex < VoxelizationOnlyMeshStartIdx)
		{
			// By this point, the index buffer render resource must be initialized
			// Add the static mesh to the appropriate draw lists.
			Mesh.AddToDrawLists(RHICmdList, Scene);
		}
#if WITH_GFSDK_VXGI
		//The meshes beginning at VoxelizationOnlyMeshStartIdx in the StaticMeshes array are the special ones for use with voxelization only.
		//However in the non-tessellated case or when tessellation is allowed in voxelization the same FStaticMesh can be used for both the voxelization and non-voxelization mesh
		//so VoxelizationOnlyMeshStartIdx == StaticMeshes.Num() since no more meshes were added. In this case all the meshes go into both AddToDrawLists and AddToVXGIDrawLists
		if (MeshIndex >= VoxelizationOnlyMeshStartIdx || VoxelizationOnlyMeshStartIdx == StaticMeshes.Num())
		{
			Mesh.AddToVXGIDrawLists(RHICmdList, Scene);
		}
#endif
	}
	// NVCHANGE_END: Add VXGI
}

void FPrimitiveSceneInfo::AddToScene(FRHICommandListImmediate& RHICmdList, bool bUpdateStaticDrawLists)
{
	check(IsInRenderingThread());
	
	// If we are attaching a primitive that should be statically lit but has unbuilt lighting,
	// Allocate space in the indirect lighting cache so that it can be used for previewing indirect lighting
	if (Proxy->HasStaticLighting() 
		&& Proxy->NeedsUnbuiltPreviewLighting() 
		&& IsIndirectLightingCacheAllowed(Scene->GetFeatureLevel()))
	{
		FIndirectLightingCacheAllocation* PrimitiveAllocation = Scene->IndirectLightingCache.FindPrimitiveAllocation(PrimitiveComponentId);

		if (PrimitiveAllocation)
		{
			IndirectLightingCacheAllocation = PrimitiveAllocation;
			PrimitiveAllocation->SetDirty();
		}
		else
		{
			PrimitiveAllocation = Scene->IndirectLightingCache.AllocatePrimitive(this, true);
			PrimitiveAllocation->SetDirty();
			IndirectLightingCacheAllocation = PrimitiveAllocation;
		}
	}
	MarkPrecomputedLightingBufferDirty();

	if (bUpdateStaticDrawLists)
	{
		AddStaticMeshes(RHICmdList);
	}

	// create potential storage for our compact info
	FPrimitiveSceneInfoCompact CompactPrimitiveSceneInfo(this);

	// Add the primitive to the octree.
	check(!OctreeId.IsValidId());
	Scene->PrimitiveOctree.AddElement(CompactPrimitiveSceneInfo);
	check(OctreeId.IsValidId());

	if (Proxy->CastsDynamicIndirectShadow())
	{
		Scene->DynamicIndirectCasterPrimitives.Add(this);
	}

	Scene->PrimitiveSceneProxies[PackedIndex] = Proxy;

	// Set bounds.
	FPrimitiveBounds& PrimitiveBounds = Scene->PrimitiveBounds[PackedIndex];
	FBoxSphereBounds BoxSphereBounds = Proxy->GetBounds();
	PrimitiveBounds.BoxSphereBounds = BoxSphereBounds;
	PrimitiveBounds.MinDrawDistanceSq = FMath::Square(Proxy->GetMinDrawDistance());
	PrimitiveBounds.MaxDrawDistance = Proxy->GetMaxDrawDistance();

	Scene->PrimitiveFlagsCompact[PackedIndex] = FPrimitiveFlagsCompact(Proxy);

	// Store precomputed visibility ID.
	int32 VisibilityBitIndex = Proxy->GetVisibilityId();
	FPrimitiveVisibilityId& VisibilityId = Scene->PrimitiveVisibilityIds[PackedIndex];
	VisibilityId.ByteIndex = VisibilityBitIndex / 8;
	VisibilityId.BitMask = (1 << (VisibilityBitIndex & 0x7));

	// Store occlusion flags.
	uint8 OcclusionFlags = EOcclusionFlags::None;
	if (Proxy->CanBeOccluded())
	{
		OcclusionFlags |= EOcclusionFlags::CanBeOccluded;
	}
	if (Proxy->HasSubprimitiveOcclusionQueries())
	{
		OcclusionFlags |= EOcclusionFlags::HasSubprimitiveQueries;
	}
	if (Proxy->AllowApproximateOcclusion()
		// Allow approximate occlusion if attached, even if the parent does not have bLightAttachmentsAsGroup enabled
		|| LightingAttachmentRoot.IsValid())
	{
		OcclusionFlags |= EOcclusionFlags::AllowApproximateOcclusion;
	}
	if (VisibilityBitIndex >= 0)
	{
		OcclusionFlags |= EOcclusionFlags::HasPrecomputedVisibility;
	}
	Scene->PrimitiveOcclusionFlags[PackedIndex] = OcclusionFlags;

	// Store occlusion bounds.
	FBoxSphereBounds OcclusionBounds = BoxSphereBounds;
	if (Proxy->HasCustomOcclusionBounds())
	{
		OcclusionBounds = Proxy->GetCustomOcclusionBounds();
	}
	OcclusionBounds.BoxExtent.X = OcclusionBounds.BoxExtent.X + OCCLUSION_SLOP;
	OcclusionBounds.BoxExtent.Y = OcclusionBounds.BoxExtent.Y + OCCLUSION_SLOP;
	OcclusionBounds.BoxExtent.Z = OcclusionBounds.BoxExtent.Z + OCCLUSION_SLOP;
	OcclusionBounds.SphereRadius = OcclusionBounds.SphereRadius + OCCLUSION_SLOP;
	Scene->PrimitiveOcclusionBounds[PackedIndex] = OcclusionBounds;

	// Store the component.
	Scene->PrimitiveComponentIds[PackedIndex] = PrimitiveComponentId;

	bNeedsCachedReflectionCaptureUpdate = true;

	{
		FMemMark MemStackMark(FMemStack::Get());

		// Find lights that affect the primitive in the light octree.
		for (FSceneLightOctree::TConstElementBoxIterator<SceneRenderingAllocator> LightIt(Scene->LightOctree, Proxy->GetBounds().GetBox());
			LightIt.HasPendingElements();
			LightIt.Advance())
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = LightIt.GetCurrentElement();
			if (LightSceneInfoCompact.AffectsPrimitive(CompactPrimitiveSceneInfo.Bounds, CompactPrimitiveSceneInfo.Proxy))
			{
				FLightPrimitiveInteraction::Create(LightSceneInfoCompact.LightSceneInfo,this);
			}
		}
	}

	INC_MEMORY_STAT_BY(STAT_PrimitiveInfoMemory, sizeof(*this) + StaticMeshes.GetAllocatedSize() + Proxy->GetMemoryFootprint());
}

void FPrimitiveSceneInfo::RemoveStaticMeshes()
{
	// Remove static meshes from the scene.
	StaticMeshes.Empty();
}

void FPrimitiveSceneInfo::RemoveFromScene(bool bUpdateStaticDrawLists)
{
	check(IsInRenderingThread());

	// implicit linked list. The destruction will update this "head" pointer to the next item in the list.
	while(LightList)
	{
		FLightPrimitiveInteraction::Destroy(LightList);
	}
	
	// Remove the primitive from the octree.
	check(OctreeId.IsValidId());
	check(Scene->PrimitiveOctree.GetElementById(OctreeId).PrimitiveSceneInfo == this);
	Scene->PrimitiveOctree.RemoveElement(OctreeId);
	OctreeId = FOctreeElementId();

	if (Proxy->CastsDynamicIndirectShadow())
	{
		Scene->DynamicIndirectCasterPrimitives.RemoveSingleSwap(this);
	}

	IndirectLightingCacheAllocation = NULL;
	ClearPrecomputedLightingBuffer(false);

	DEC_MEMORY_STAT_BY(STAT_PrimitiveInfoMemory, sizeof(*this) + StaticMeshes.GetAllocatedSize() + Proxy->GetMemoryFootprint());

	if (bUpdateStaticDrawLists)
	{
		RemoveStaticMeshes();
	}
}

void FPrimitiveSceneInfo::UpdateStaticMeshes(FRHICommandListImmediate& RHICmdList)
{
	checkSlow(bNeedsStaticMeshUpdate);
	bNeedsStaticMeshUpdate = false;

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FPrimitiveSceneInfo_UpdateStaticMeshes);

	// Remove the primitive's static meshes from the draw lists they're currently in, and re-add them to the appropriate draw lists.
	for (int32 MeshIndex = 0; MeshIndex < StaticMeshes.Num(); MeshIndex++)
	{
		StaticMeshes[MeshIndex].RemoveFromDrawLists();
		// NVCHANGE_BEGIN: Add VXGI
		if (MeshIndex < VoxelizationOnlyMeshStartIdx)
		{
			// By this point, the index buffer render resource must be initialized
			// Add the static mesh to the appropriate draw lists.
			StaticMeshes[MeshIndex].AddToDrawLists(RHICmdList, Scene);
		}
#if WITH_GFSDK_VXGI
		if (MeshIndex >= VoxelizationOnlyMeshStartIdx || VoxelizationOnlyMeshStartIdx == StaticMeshes.Num())
		{
			StaticMeshes[MeshIndex].AddToVXGIDrawLists(RHICmdList, Scene);
		}
#endif
		// NVCHANGE_END: Add VXGI
	}
}

void FPrimitiveSceneInfo::UpdateUniformBuffer(FRHICommandListImmediate& RHICmdList)
{
	checkSlow(bNeedsUniformBufferUpdate);
	bNeedsUniformBufferUpdate = false;
	Proxy->UpdateUniformBuffer();
}

void FPrimitiveSceneInfo::BeginDeferredUpdateStaticMeshes()
{
	// Set a flag which causes InitViews to update the static meshes the next time the primitive is visible.
	bNeedsStaticMeshUpdate = true;
}

void FPrimitiveSceneInfo::LinkLODParentComponent()
{
	if (LODParentComponentId.IsValid())
	{
		Scene->SceneLODHierarchy.AddChildNode(LODParentComponentId, this);
	}
}

void FPrimitiveSceneInfo::UnlinkLODParentComponent()
{
	if(LODParentComponentId.IsValid())
	{
		Scene->SceneLODHierarchy.RemoveChildNode(LODParentComponentId, this);
		// I don't think this will be reused but just in case
		LODParentComponentId = FPrimitiveComponentId();
	}
}

void FPrimitiveSceneInfo::LinkAttachmentGroup()
{
	// Add the primitive to its attachment group.
	if (LightingAttachmentRoot.IsValid())
	{
		FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(LightingAttachmentRoot);

		if (!AttachmentGroup)
		{
			// If this is the first primitive attached that uses this attachment parent, create a new attachment group.
			AttachmentGroup = &Scene->AttachmentGroups.Add(LightingAttachmentRoot, FAttachmentGroupSceneInfo());
		}

		AttachmentGroup->Primitives.Add(this);
	}
	else if (Proxy->LightAttachmentsAsGroup())
	{
		FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (!AttachmentGroup)
		{
			// Create an empty attachment group 
			AttachmentGroup = &Scene->AttachmentGroups.Add(PrimitiveComponentId, FAttachmentGroupSceneInfo());
		}

		AttachmentGroup->ParentSceneInfo = this;
	}
}

void FPrimitiveSceneInfo::UnlinkAttachmentGroup()
{
	// Remove the primitive from its attachment group.
	if (LightingAttachmentRoot.IsValid())
	{
		FAttachmentGroupSceneInfo& AttachmentGroup = Scene->AttachmentGroups.FindChecked(LightingAttachmentRoot);
		AttachmentGroup.Primitives.RemoveSwap(this);

		if (AttachmentGroup.Primitives.Num() == 0)
		{
			// If this was the last primitive attached that uses this attachment root, free the group.
			Scene->AttachmentGroups.Remove(LightingAttachmentRoot);
		}
	}
	else if (Proxy->LightAttachmentsAsGroup())
	{
		FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);
		
		if (AttachmentGroup)
		{
			AttachmentGroup->ParentSceneInfo = NULL;
		}
	}
}

void FPrimitiveSceneInfo::GatherLightingAttachmentGroupPrimitives(TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos)
{
#if ENABLE_NAN_DIAGNOSTIC
	// local function that returns full name of object
	auto GetObjectName = [](const UPrimitiveComponent* InPrimitive)->FString
	{
		return (InPrimitive) ? InPrimitive->GetFullName() : FString(TEXT("Unknown Object"));
	};

	// verify that the current object has a valid bbox before adding it
	const float& BoundsRadius = this->Proxy->GetBounds().SphereRadius;
	if (ensureMsgf(!FMath::IsNaN(BoundsRadius) && FMath::IsFinite(BoundsRadius),
		TEXT("%s had an ill-formed bbox and was skipped during shadow setup, contact DavidH."), *GetObjectName(this->ComponentForDebuggingOnly)))
	{
		OutChildSceneInfos.Add(this);
	}
	else
	{
		// return, leaving the TArray empty
		return;
	}

#else 
	// add self at the head of this queue
	OutChildSceneInfos.Add(this);
#endif

	if (!LightingAttachmentRoot.IsValid() && Proxy->LightAttachmentsAsGroup())
	{
		const FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (AttachmentGroup)
		{
			
			for (int32 ChildIndex = 0, ChildIndexMax = AttachmentGroup->Primitives.Num(); ChildIndex < ChildIndexMax; ChildIndex++)
			{
				FPrimitiveSceneInfo* ShadowChild = AttachmentGroup->Primitives[ChildIndex];
#if ENABLE_NAN_DIAGNOSTIC
				// Only enqueue objects with valid bounds using the normality of the SphereRaduis as criteria.

				const float& ShadowChildBoundsRadius = ShadowChild->Proxy->GetBounds().SphereRadius;

				if (ensureMsgf(!FMath::IsNaN(ShadowChildBoundsRadius) && FMath::IsFinite(ShadowChildBoundsRadius),
					TEXT("%s had an ill-formed bbox and was skipped during shadow setup, contact DavidH."), *GetObjectName(ShadowChild->ComponentForDebuggingOnly)))
				{
					checkSlow(!OutChildSceneInfos.Contains(ShadowChild))
				    OutChildSceneInfos.Add(ShadowChild);
				}
#else
				// enqueue all objects.
				checkSlow(!OutChildSceneInfos.Contains(ShadowChild))
			    OutChildSceneInfos.Add(ShadowChild);
#endif
			}
		}
	}
}

void FPrimitiveSceneInfo::GatherLightingAttachmentGroupPrimitives(TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos) const
{
	OutChildSceneInfos.Add(this);

	if (!LightingAttachmentRoot.IsValid() && Proxy->LightAttachmentsAsGroup())
	{
		const FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (AttachmentGroup)
		{
			for (int32 ChildIndex = 0, ChildIndexMax = AttachmentGroup->Primitives.Num(); ChildIndex < ChildIndexMax; ChildIndex++)
			{
				const FPrimitiveSceneInfo* ShadowChild = AttachmentGroup->Primitives[ChildIndex];

				checkSlow(!OutChildSceneInfos.Contains(ShadowChild))
			    OutChildSceneInfos.Add(ShadowChild);
			}
		}
	}
}

FBoxSphereBounds FPrimitiveSceneInfo::GetAttachmentGroupBounds() const
{
	FBoxSphereBounds Bounds = Proxy->GetBounds();

	if (!LightingAttachmentRoot.IsValid() && Proxy->LightAttachmentsAsGroup())
	{
		const FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (AttachmentGroup)
		{
			for (int32 ChildIndex = 0; ChildIndex < AttachmentGroup->Primitives.Num(); ChildIndex++)
			{
				FPrimitiveSceneInfo* AttachmentChild = AttachmentGroup->Primitives[ChildIndex];
				Bounds = Bounds + AttachmentChild->Proxy->GetBounds();
			}
		}
	}

	return Bounds;
}

uint32 FPrimitiveSceneInfo::GetMemoryFootprint()
{
	return( sizeof( *this ) + HitProxies.GetAllocatedSize() + StaticMeshes.GetAllocatedSize() );
}

bool FPrimitiveSceneInfo::ShouldRenderVelocity(const FViewInfo& View, bool bCheckVisibility) const
{
	int32 PrimitiveId = GetIndex();
	if (bCheckVisibility)
	{
		const bool bVisible = View.PrimitiveVisibilityMap[PrimitiveId];

		// Only render if visible.
		if (!bVisible)
		{
			return false;
		}
	}

	const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

	if (!Proxy->IsMovable())
	{
		return false;
	}

	// !Skip translucent objects as they don't support velocities and in the case of particles have a significant CPU overhead.
	if (!PrimitiveViewRelevance.bOpaqueRelevance || !PrimitiveViewRelevance.bRenderInMainPass)
	{
		return false;
	}

	const float LODFactorDistanceSquared = (Proxy->GetBounds().Origin - View.ViewMatrices.GetViewOrigin()).SizeSquared() * FMath::Square(View.LODDistanceFactor);

	// The minimum projected screen radius for a primitive to be drawn in the velocity pass, as a fraction of half the horizontal screen width (likely to be 0.08f)
	float MinScreenRadiusForVelocityPass = View.FinalPostProcessSettings.MotionBlurPerObjectSize * (2.0f / 100.0f);
	float MinScreenRadiusForVelocityPassSquared = FMath::Square(MinScreenRadiusForVelocityPass);

	// Skip primitives that only cover a small amount of screenspace, motion blur on them won't be noticeable.
	if (FMath::Square(Proxy->GetBounds().SphereRadius) <= MinScreenRadiusForVelocityPassSquared * LODFactorDistanceSquared)
	{
		return false;
	}

	// Only render primitives with velocity.
	if (!FVelocityDrawingPolicy::HasVelocity(View, this))
	{
		return false;
	}

	// If the base pass is allowed to render velocity in the GBuffer, only mesh with static lighting need the velocity pass.
	if (FVelocityRendering::OutputsToGBuffer() && (!UseSelectiveBasePassOutputs() || !Proxy->HasStaticLighting()))
	{
		return false;
	}

	return true;
}

void FPrimitiveSceneInfo::ApplyWorldOffset(FVector InOffset)
{
	Proxy->ApplyWorldOffset(InOffset);
}

void FPrimitiveSceneInfo::UpdatePrecomputedLightingBuffer()
{
	// The update is invalid if the lighting cache allocation was not in a functional state.
	if (bPrecomputedLightingBufferDirty && (!IndirectLightingCacheAllocation || (Scene->IndirectLightingCache.IsInitialized() && IndirectLightingCacheAllocation->bHasEverUpdatedSingleSample)))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdatePrecomputedLightingBuffer);

		EUniformBufferUsage BufferUsage = Proxy->IsOftenMoving() ? UniformBuffer_SingleFrame : UniformBuffer_MultiFrame;

		// If the PrimitiveInfo has no precomputed lighting buffer, it will fallback to the global Empty buffer.
		if (!RHISupportsVolumeTextures(Scene->GetFeatureLevel())
			&& Scene->VolumetricLightmapSceneData.HasData()
			&& (Proxy->IsMovable() || Proxy->NeedsUnbuiltPreviewLighting())
			&& Proxy->WillEverBeLit())
		{
			IndirectLightingCacheUniformBuffer = CreatePrecomputedLightingUniformBuffer(BufferUsage, Scene->GetFeatureLevel(), NULL, NULL, Proxy->GetBounds().Origin, Scene->GetFrameNumber(), &Scene->VolumetricLightmapSceneData, NULL);
		}
		else if (IndirectLightingCacheAllocation)
		{
			IndirectLightingCacheUniformBuffer = CreatePrecomputedLightingUniformBuffer(BufferUsage, Scene->GetFeatureLevel(), &Scene->IndirectLightingCache, IndirectLightingCacheAllocation, FVector(0, 0, 0), 0, NULL, NULL);
		}
		else
		{
			IndirectLightingCacheUniformBuffer.SafeRelease();
		}

		FPrimitiveSceneProxy::FLCIArray LCIs;
		Proxy->GetLCIs(LCIs);
		for (int32 i = 0; i < LCIs.Num(); ++i)
		{
			FLightCacheInterface* LCI = LCIs[i];
			if (!LCI) continue;

			// If the LCI has no precomputed lighting buffer, it will fallback to the PrimitiveInfo buffer.
			if (LCI->GetShadowMapInteraction().GetType() == SMIT_Texture || LCI->GetLightMapInteraction(Scene->GetFeatureLevel()).GetType() == LMIT_Texture)
			{
				LCI->SetPrecomputedLightingBuffer(CreatePrecomputedLightingUniformBuffer(BufferUsage, Scene->GetFeatureLevel(), NULL, NULL, FVector(0, 0, 0), 0, NULL, LCI));
			}
			else
			{
				LCI->SetPrecomputedLightingBuffer(FUniformBufferRHIRef());
			}
		}

		bPrecomputedLightingBufferDirty = false;
	}
}

void FPrimitiveSceneInfo::ClearPrecomputedLightingBuffer(bool bSingleFrameOnly)
{
	if (!bSingleFrameOnly || Proxy->IsOftenMoving())
	{
		IndirectLightingCacheUniformBuffer.SafeRelease();

		FPrimitiveSceneProxy::FLCIArray LCIs;
		Proxy->GetLCIs(LCIs);
		for (int32 i = 0; i < LCIs.Num(); ++i)
		{
			FLightCacheInterface* LCI = LCIs[i];
			if (LCI)
			{
				LCI->SetPrecomputedLightingBuffer(FUniformBufferRHIRef());
			}
		}
		MarkPrecomputedLightingBufferDirty();
	}
}

