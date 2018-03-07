#include "BlastExtendedSupport.h"
#include "BlastGlueVolume.h"

#include "UObject/ConstructorHelpers.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysXPublic.h"
#include "PhysicsPublic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ShowFlags.h"
#include "SceneManagement.h"

UBlastExtendedSupportMeshComponent::UBlastExtendedSupportMeshComponent(const FObjectInitializer& ObjectInitializer)
	: UBlastMeshComponent(ObjectInitializer)
{
	//The bonds are added there also so no need to do glue ourselves
	bSupportedByWorld = false;
	//You can't edit the combined mesh so assume we provide these settings
	bOverride_BlastMaterial = true;
	bOverride_ImpactDamageProperties = true;
	bOverride_StressProperties = true;
	bOverride_DebrisProperties = true;
}

int32 UBlastExtendedSupportMeshComponent::GetCombinedChunkIndex(int32 ComponentIndex, int32 ComponentChunkIndex) const
{
	if (SavedComponents.IsValidIndex(ComponentIndex))
	{
		const auto& PerComponent = SavedComponents[ComponentIndex].ChunkIDs;
		if (PerComponent.IsValidIndex(ComponentChunkIndex))
		{
			return PerComponent[ComponentChunkIndex];
		}
	}
	return INDEX_NONE;
}

int32 UBlastExtendedSupportMeshComponent::GetComponentChunkIndex(int32 CombinedIndex, int32* OutComponentIndex) const
{
	if (ChunkToOriginalChunkMap.IsValidIndex(CombinedIndex))
	{
		*OutComponentIndex = ChunkToOriginalChunkMap[CombinedIndex].X;
		return ChunkToOriginalChunkMap[CombinedIndex].Y;
	}
	*OutComponentIndex = INDEX_NONE;
	return INDEX_NONE;
}

void UBlastExtendedSupportMeshComponent::SetChunkVisible(int32 ChunkIndex, bool bInVisible)
{
	int32 ComponentIndex, RelChunkIndex;
	RelChunkIndex = GetComponentChunkIndex(ChunkIndex, &ComponentIndex);
	if (SavedComponents.IsValidIndex(ComponentIndex))
	{
		SavedComponents[ComponentIndex].MeshComponent->SetChunkVisible(RelChunkIndex, bInVisible);
	}
}

bool UBlastExtendedSupportMeshComponent::PopulateComponentBoneTransforms(TArray<FTransform>& Transforms, TBitArray<>& BonesTouched, int32 ComponentIndex)
{
	auto PScene = GetPXScene();
	if (!PScene || !SavedComponents.IsValidIndex(ComponentIndex))
	{
		//During cooking there is no PhysX scene, so nothing to sync
		return false;
	}
	bool bAnyBodiesChanged = false;

	FBlastExtendedStructureComponent& Component = SavedComponents[ComponentIndex];

	//We could get here during initial setup where this is not populated yet
	if (Component.LastActorTransforms.Num() != BlastActors.Num())
	{
		Component.LastActorTransforms.SetNum(BlastActors.Num());
	}

	UBlastMesh* ComponentBlastMesh = Component.MeshComponent->GetBlastMesh();
	const int32 ComponentChunkCount = ComponentBlastMesh->GetChunkCount();

	SCENE_LOCK_READ(PScene);
	for (int32 ActorIndex = BlastActorsBeginLive; ActorIndex < BlastActorsEndLive; ActorIndex++)
	{
		FActorData& ActorData = BlastActors[ActorIndex];
		FBodyInstance* BodyInst = ActorData.BodyInstance;
		if (!BodyInst)
		{
			continue;
		}

		//We need to track this per-component so we don't use ActorData.PreviousBodyWorldTransform
		FTransform& PreviousBodyWorldTransform = Component.LastActorTransforms[ActorIndex];

		FTransform BodyWT = BodyInst->GetUnrealWorldTransform_AssumesLocked();
		BodyWT.SetScale3D(BodyInst->Scale3D);

		if (!BodyWT.Equals(PreviousBodyWorldTransform))
		{
			PreviousBodyWorldTransform = BodyWT;
			BodyWT = Component.TransformAtMerge * BodyWT;
			FTransform BodyCST = BodyWT.GetRelativeTransform(Component.MeshComponent->GetComponentTransform());

			for (const FActorChunkData& ChunkData : ActorData.Chunks)
			{
				int32 CombinedIndex = ChunkData.ChunkIndex;
				int32 ThisComponentIndex;
				int32 ComponentChunkIndex = GetComponentChunkIndex(CombinedIndex, &ThisComponentIndex);
				if (ThisComponentIndex == ComponentIndex)
				{
					int32 BoneIndex = ComponentBlastMesh->ChunkIndexToBoneIndex[ComponentChunkIndex];
					Transforms[BoneIndex] = ComponentBlastMesh->GetComponentSpaceInitialBoneTransform(BoneIndex) * BodyCST;
					BonesTouched[BoneIndex] = true;
					bAnyBodiesChanged = true;
				}
			}
		}
	}
	SCENE_UNLOCK_READ(PScene);

	return bAnyBodiesChanged;
}

FBox UBlastExtendedSupportMeshComponent::GetWorldBoundsOfComponentChunks(int32 ComponentIndex) const
{
	FBox NewBox(ForceInit);
	if (SavedComponents.IsValidIndex(ComponentIndex))
	{
		SCOPED_SCENE_READ_LOCK(GetPXScene());
		for (int32 ActorIndex = BlastActorsBeginLive; ActorIndex < BlastActorsEndLive; ActorIndex++)
		{
			UBodySetup* BodySetup = ActorBodySetups[ActorIndex];
			const FActorData& BlastActor = BlastActors[ActorIndex];
			if (BodySetup && BlastActor.BodyInstance)
			{
				//Todo: Cache components the actor is par tof?
				for (FActorChunkData CD : BlastActor.Chunks)
				{
					int32 ChunkComponentIdx;
					if (GetComponentChunkIndex(CD.ChunkIndex, &ChunkComponentIdx) != INDEX_NONE && ChunkComponentIdx == ComponentIndex)
					{
						//Include this actor
						FTransform BodyWorldTransform = BlastActor.BodyInstance->GetUnrealWorldTransform_AssumesLocked();
						BodyWorldTransform.SetScale3D(BlastActor.BodyInstance->Scale3D);
						FBox AABB = BodySetup->AggGeom.CalcAABB(BodyWorldTransform);
						NewBox += AABB;
						break;
					}
				}
				
			}
		}
	}
	return NewBox;
}

namespace
{
	class FBlastMeshSceneProxyNoRender : public FBlastMeshSceneProxyBase, public FPrimitiveSceneProxy
	{
	public:
		FBlastMeshSceneProxyNoRender(const UBlastMeshComponent* Component)
			: FBlastMeshSceneProxyBase(Component)
			, FPrimitiveSceneProxy(Component)
		{
			//This assert fires when rendering SelectedComponentProxies
			bVerifyUsedMaterials = false;
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FMatrix ProxyLocalToWorld;
		/** component space bone transforms*/
		TArray<FTransform> MeshComponentSpaceTransforms;
		TArray<FPrimitiveSceneProxy*> SelectedComponentProxies;
#endif

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance VR;
			VR.bDrawRelevance = IsShown(View);
			VR.bDynamicRelevance = true;
			VR.bRenderCustomDepth = ShouldRenderCustomDepth();
			VR.bRenderInMainPass = ShouldRenderInMainPass();
			return VR;
		}

		virtual bool CanBeOccluded() const
		{
			return false;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
#if WITH_EDITOR
			RenderDebugLines(Views, ViewFamily, VisibilityMap, Collector);

			if (IsSelected())
			{
				//Also render our components in the selection outline so you can see them
				for (FPrimitiveSceneProxy* PSP : SelectedComponentProxies)
				{
					PSP->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
				}
			}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					RenderPhysicsAsset(ViewIndex, Collector, ViewFamily.EngineShowFlags, ProxyLocalToWorld, &MeshComponentSpaceTransforms);

					if (EngineShowFlags.MassProperties && DebugMassData.Num() > 0)
					{
						FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
						for (const FDebugMassData& DebugMass : DebugMassData)
						{
							if (MeshComponentSpaceTransforms.IsValidIndex(DebugMass.BoneIndex))
							{
								const FTransform BoneToWorld = MeshComponentSpaceTransforms[DebugMass.BoneIndex] * FTransform(ProxyLocalToWorld);
								DebugMass.DrawDebugMass(PDI, BoneToWorld);
							}
						}
					}
				}
			}
#endif
		}

		virtual uint32 GetMemoryFootprint(void) const override
		{
			return sizeof(*this) + GetAllocatedSize();
		}

		uint32 GetAllocatedSize(void) const {
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			return FPrimitiveSceneProxy::GetAllocatedSize()
				+ sizeof(FTransform) * MeshComponentSpaceTransforms.Num()
				+ sizeof(FPrimitiveSceneProxy*) * SelectedComponentProxies.Num();
#else			
			return FPrimitiveSceneProxy::GetAllocatedSize();
#endif
		}
	};
}

FPrimitiveSceneProxy* UBlastExtendedSupportMeshComponent::CreateSceneProxy()
{
	//Always create it but we don't need to pass an actual mesh
	FBlastMeshSceneProxyNoRender* Result = nullptr;

	if (ShouldRender())
	{
		Result = ::new FBlastMeshSceneProxyNoRender(this);
	}

	BlastProxy = Result;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SendRenderDebugPhysics(Result);
#endif

	return Result;
}

void UBlastExtendedSupportMeshComponent::CreateRenderState_Concurrent()
{
	//Don't need to create the MeshObject
	UPrimitiveComponent::CreateRenderState_Concurrent();
}

void UBlastExtendedSupportMeshComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	//Need to check SceneProxy since we don't know when to set BlastProxy to null
	if (BlastProxy && SceneProxy)
	{
		TArray<FPrimitiveSceneProxy*> SelectedComponentProxies;

		if (IsSelected() || IsOwnerSelected())
		{
			ABlastExtendedSupportStructure* Owner = CastChecked<ABlastExtendedSupportStructure>(GetOwner());
			TArray<UBlastMeshComponent*> StructureComponents;
			Owner->GetStructureComponents(StructureComponents);

			SelectedComponentProxies.Reserve(StructureComponents.Num());

			for (UBlastMeshComponent* SC : StructureComponents)
			{
				if (SC && SC->SceneProxy)
				{
					SelectedComponentProxies.Add(SC->SceneProxy);
				}
			}
		}

		ENQUEUE_RENDER_COMMAND(DebugRenderData)([BlastProxy{ static_cast<FBlastMeshSceneProxyNoRender*>(BlastProxy) }, ProxyLocalToWorld{ GetComponentTransform().ToMatrixWithScale() }, MeshComponentSpaceTransforms{ GetComponentSpaceTransforms() }, SelectedComponentProxies{ MoveTemp(SelectedComponentProxies) }](FRHICommandList& RHICmdList) mutable
		{
			BlastProxy->ProxyLocalToWorld = ProxyLocalToWorld;
			BlastProxy->MeshComponentSpaceTransforms = MoveTemp(MeshComponentSpaceTransforms);
			BlastProxy->SelectedComponentProxies = SelectedComponentProxies;
		});
	}
#endif
}

bool UBlastExtendedSupportMeshComponent::ShouldUpdateTransform(bool bLODHasChanged) const
{
#if WITH_EDITOR
	//If we are rendering debug mode make sure the bounds are up to date since we will get culled since our mesh is tiny
	if (BlastDebugRenderMode != EBlastDebugRenderMode::None)
	{
		return true;
	}
#endif
	//Debris is done in RefreshBoneTransforms
	return Super::ShouldUpdateTransform(bLODHasChanged) && GetUsedDebrisProperties().DebrisFilters.Num() > 0;
}

void UBlastExtendedSupportMeshComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if WITH_EDITOR
	if (IsSelected() || IsOwnerSelected())
	{ 
		MarkRenderDynamicDataDirty();
	}
#endif
}

void UBlastExtendedSupportMeshComponent::OnRegister()
{
	//Make sure our child components register first or else visibility could not propagate correctly
	for (const auto& SC : SavedComponents)
	{
		if (!SC.MeshComponent->IsRegistered())
		{
			SC.MeshComponent->RegisterComponent();
		}

		// set up tick dependency between master & slave components
		SC.MeshComponent->AddTickPrerequisiteComponent(this);
	}

	Super::OnRegister();
}

void UBlastExtendedSupportMeshComponent::BroadcastOnDamaged(FName ActorName, const FVector& DamageOrigin, const FRotator& DamageRot, FName DamageType)
{
	Super::BroadcastOnDamaged(ActorName, DamageOrigin, DamageRot, DamageType);

	int32 ActorIndex = ActorNameToActorIndex(ActorName);
	if (BlastActors.IsValidIndex(ActorIndex))
	{
		TBitArray<> ComponentsGettingTheEvent(false, SavedComponents.Num());
		for (const FActorChunkData& Chunk : BlastActors[ActorIndex].Chunks)
		{
			int32 ComponentIndex, RelChunkIndex;
			RelChunkIndex = GetComponentChunkIndex(Chunk.ChunkIndex, &ComponentIndex);
			if (ComponentsGettingTheEvent.IsValidIndex(ComponentIndex))
			{
				ComponentsGettingTheEvent[ComponentIndex] = true;
			}
		}

		for (TConstSetBitIterator<> It(ComponentsGettingTheEvent, 0); It; ++It)
		{
			SavedComponents[It.GetIndex()].MeshComponent->BroadcastOnDamaged(ActorName, DamageOrigin, DamageRot, DamageType);
		}
	}
}

void UBlastExtendedSupportMeshComponent::BroadcastOnActorCreated(FName ActorName)
{
	Super::BroadcastOnActorCreated(ActorName);

	int32 ActorIndex = ActorNameToActorIndex(ActorName);
	if (BlastActors.IsValidIndex(ActorIndex))
	{
		TBitArray<> ComponentsGettingTheEvent(false, SavedComponents.Num());
		for (const FActorChunkData& Chunk : BlastActors[ActorIndex].Chunks)
		{
			int32 ComponentIndex, RelChunkIndex;
			RelChunkIndex = GetComponentChunkIndex(Chunk.ChunkIndex, &ComponentIndex);
			if (ComponentsGettingTheEvent.IsValidIndex(ComponentIndex))
			{
				ComponentsGettingTheEvent[ComponentIndex] = true;
			}
		}

		for (TConstSetBitIterator<> It(ComponentsGettingTheEvent, 0); It; ++It)
		{
			SavedComponents[It.GetIndex()].MeshComponent->BroadcastOnActorCreated(ActorName);
		}
	}
}

void UBlastExtendedSupportMeshComponent::BroadcastOnActorDestroyed(FName ActorName)
{
	Super::BroadcastOnActorDestroyed(ActorName);

	int32 ActorIndex = ActorNameToActorIndex(ActorName);
	if (BlastActors.IsValidIndex(ActorIndex))
	{
		TBitArray<> ComponentsGettingTheEvent(false, SavedComponents.Num());
		for (const FActorChunkData& Chunk : BlastActors[ActorIndex].Chunks)
		{
			int32 ComponentIndex, RelChunkIndex;
			RelChunkIndex = GetComponentChunkIndex(Chunk.ChunkIndex, &ComponentIndex);
			if (ComponentsGettingTheEvent.IsValidIndex(ComponentIndex))
			{
				ComponentsGettingTheEvent[ComponentIndex] = true;
			}
		}

		for (TConstSetBitIterator<> It(ComponentsGettingTheEvent, 0); It; ++It)
		{
			SavedComponents[It.GetIndex()].MeshComponent->BroadcastOnActorDestroyed(ActorName);
		}
	}
}

void UBlastExtendedSupportMeshComponent::BroadcastOnActorCreatedFromDamage(FName ActorName, const FVector& DamageOrigin, const FRotator& DamageRot, FName DamageType)
{
	Super::BroadcastOnActorCreatedFromDamage(ActorName, DamageOrigin, DamageRot, DamageType);

	int32 ActorIndex = ActorNameToActorIndex(ActorName);
	if (BlastActors.IsValidIndex(ActorIndex))
	{
		TBitArray<> ComponentsGettingTheEvent(false, SavedComponents.Num());
		for (const FActorChunkData& Chunk : BlastActors[ActorIndex].Chunks)
		{
			int32 ComponentIndex, RelChunkIndex;
			RelChunkIndex = GetComponentChunkIndex(Chunk.ChunkIndex, &ComponentIndex);
			if (ComponentsGettingTheEvent.IsValidIndex(ComponentIndex))
			{
				ComponentsGettingTheEvent[ComponentIndex] = true;
			}
		}

		for (TConstSetBitIterator<> It(ComponentsGettingTheEvent, 0); It; ++It)
		{
			SavedComponents[It.GetIndex()].MeshComponent->BroadcastOnActorCreatedFromDamage(ActorName, DamageOrigin, DamageRot, DamageType);
		}
	}
}

void UBlastExtendedSupportMeshComponent::BroadcastOnBondsDamaged(FName ActorName, bool bIsSplit, FName DamageType, const TArray<FBondDamageEvent>& Events)
{
	Super::BroadcastOnBondsDamaged(ActorName, bIsSplit, DamageType, Events);

	int32 ActorIndex = ActorNameToActorIndex(ActorName);
	if (BlastActors.IsValidIndex(ActorIndex))
	{
		TArray<TArray<FBondDamageEvent>> PerComponentArrays;
		PerComponentArrays.SetNum(SavedComponents.Num());
		for (const FBondDamageEvent& Event : Events)
		{
			int32 ComponentIndex[2], RelChunkIndex[2];
			RelChunkIndex[0] = GetComponentChunkIndex(Event.ChunkIndex, &ComponentIndex[0]);
			RelChunkIndex[1] = GetComponentChunkIndex(Event.OtherChunkIndex, &ComponentIndex[1]);
			if (PerComponentArrays.IsValidIndex(ComponentIndex[0]))
			{
				//Same sub-component at both ends, just remap the idx
				if (ComponentIndex[0] == ComponentIndex[1])
				{
					FBondDamageEvent NewEvent = Event;
					NewEvent.ChunkIndex = FMath::Min(RelChunkIndex[0], RelChunkIndex[1]);
					NewEvent.OtherChunkIndex = FMath::Max(RelChunkIndex[0], RelChunkIndex[1]);
					PerComponentArrays[ComponentIndex[0]].Add(NewEvent);
				}
				else
				{
					//Treat is a world bond
					FBondDamageEvent NewEvent = Event;
					NewEvent.ChunkIndex = RelChunkIndex[0];
					NewEvent.OtherChunkIndex = INDEX_NONE;
					PerComponentArrays[ComponentIndex[0]].Add(NewEvent);
				}
			}

			if (ComponentIndex[0] != ComponentIndex[1] && PerComponentArrays.IsValidIndex(ComponentIndex[1]))
			{
				//Treat is a world bond
				FBondDamageEvent NewEvent = Event;
				NewEvent.ChunkIndex = RelChunkIndex[1];
				NewEvent.OtherChunkIndex = INDEX_NONE;
				PerComponentArrays[ComponentIndex[1]].Add(NewEvent);
			}
		}

		for (int32 I = 0; I < PerComponentArrays.Num(); I++)
		{
			if (PerComponentArrays[I].Num() > 0)
			{
				SavedComponents[I].MeshComponent->BroadcastOnBondsDamaged(ActorName, bIsSplit, DamageType, PerComponentArrays[I]);
			}
		}
	}
}

void UBlastExtendedSupportMeshComponent::BroadcastOnChunksDamaged(FName ActorName, bool bIsSplit, FName DamageType, const TArray<FChunkDamageEvent>& Events)
{
	Super::BroadcastOnChunksDamaged(ActorName, bIsSplit, DamageType, Events);

	int32 ActorIndex = ActorNameToActorIndex(ActorName);
	if (BlastActors.IsValidIndex(ActorIndex))
	{
		TArray<TArray<FChunkDamageEvent>> PerComponentArrays;
		PerComponentArrays.SetNum(SavedComponents.Num());
		for (const FChunkDamageEvent& Event : Events)
		{
			int32 ComponentIndex, RelChunkIndex;
			RelChunkIndex = GetComponentChunkIndex(Event.ChunkIndex, &ComponentIndex);
			if (PerComponentArrays.IsValidIndex(ComponentIndex))
			{
				FChunkDamageEvent NewEvent = Event;
				NewEvent.ChunkIndex = RelChunkIndex;
				PerComponentArrays[ComponentIndex].Add(NewEvent);
			}
		}

		for (int32 I = 0; I < PerComponentArrays.Num(); I++)
		{
			if (PerComponentArrays[I].Num() > 0)
			{
				SavedComponents[I].MeshComponent->BroadcastOnChunksDamaged(ActorName, bIsSplit, DamageType, PerComponentArrays[I]);
			}
		}
	}
}

bool UBlastExtendedSupportMeshComponent::OnBondsDamagedBound() const
{
	if (Super::OnBondsDamagedBound())
	{
		return true;
	}
	for (const auto& SC : SavedComponents)
	{
		if (SC.MeshComponent->OnBondsDamagedBound())
		{
			return true;
		}
	}
	return false;
}

bool UBlastExtendedSupportMeshComponent::OnChunksDamagedBound() const
{
	if (Super::OnChunksDamagedBound())
	{
		return true;
	}
	for (const auto& SC : SavedComponents)
	{
		if (SC.MeshComponent->OnChunksDamagedBound())
		{
			return true;
		}
	}
	return false;
}

void UBlastExtendedSupportMeshComponent::ShowActorsVisibleChunks(uint32 actorIndex)
{
	Super::ShowActorsVisibleChunks(actorIndex);
	RefreshBoundsForActor(actorIndex);
}

void UBlastExtendedSupportMeshComponent::HideActorsVisibleChunks(uint32 actorIndex)
{
	RefreshBoundsForActor(actorIndex);
	Super::HideActorsVisibleChunks(actorIndex);
}

void UBlastExtendedSupportMeshComponent::RefreshBoundsForActor(uint32 actorIndex)
{
	TBitArray<> ComponentsToUpdate(false, SavedComponents.Num());
	for (const FActorChunkData& Chunk : BlastActors[actorIndex].Chunks)
	{
		int32 ComponentIndex, RelChunkIndex;
		RelChunkIndex = GetComponentChunkIndex(Chunk.ChunkIndex, &ComponentIndex);
		if (ComponentsToUpdate.IsValidIndex(ComponentIndex))
		{
			ComponentsToUpdate[ComponentIndex] = true;
		}
	}

	for (TConstSetBitIterator<> It(ComponentsToUpdate, 0); It; ++It)
	{
		SavedComponents[It.GetIndex()].MeshComponent->RefreshBoneTransforms();
	}
}

#if WITH_EDITOR

void UBlastExtendedSupportMeshComponent::InvalidateSupportData()
{
	static bool bInsideInvalidateSupportData = false;
	if (bInsideInvalidateSupportData)
	{
		//Prevent recursion since SetOwningSuppportStructure calls us
		return;
	}
	TGuardValue<bool> InsideInvalidateSupportData(bInsideInvalidateSupportData, true);

	UBlastGlueWorldTag::SetExtendedSupportDirty(GetWorld());
	if (SavedComponents.Num() > 0)
	{
		for (auto& SC : SavedComponents)
		{
			SC.MeshComponent->MarkDirtyOwningSuppportStructure();
		}
		SetBlastMesh(nullptr);
		SavedComponents.Reset();
		UBlastGlueWorldTag::SetExtendedSupportDirty(GetWorld());
	}
}
#endif

ABlastExtendedSupportStructure::ABlastExtendedSupportStructure()
	: bEnabled(true), bondGenerationDistance(0.0f)
{
	//Want to be able to see the debug view in game so hide the UBillboardComponent
	bHidden = false;

	//WE can't use the UBillboardComponent since it only exists in the editor
	USceneComponent* SceneRootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	RootComponent = SceneRootComponent;
#if WITH_EDITORONLY_DATA
	UBillboardComponent* Sprite = GetSpriteComponent();
	if (Sprite)
	{
		Sprite->SetHiddenInGame(true);
		Sprite->SetupAttachment(SceneRootComponent);
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture;
			FName ID_Info;
			FText NAME_Info;
			FConstructorStatics()
				: SpriteTexture(TEXT("/Blast/EditorResources/S_BlastExtendedSupport"))
				, ID_Info(TEXT("BlastExtendedSupport"))
				, NAME_Info(NSLOCTEXT("SpriteCategory", "BlastExtendedSupport", "BlastExtendedSupport"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		Sprite->Sprite = ConstructorStatics.SpriteTexture.Get();
		Sprite->SpriteInfo.Category = ConstructorStatics.ID_Info;
		Sprite->SpriteInfo.DisplayName = ConstructorStatics.NAME_Info;
	}
#endif // WITH_EDITORONLY_DATA

	ExtendedSupportMesh = CreateDefaultSubobject<UBlastExtendedSupportMeshComponent>(GET_MEMBER_NAME_CHECKED(ABlastExtendedSupportStructure, ExtendedSupportMesh));
	ExtendedSupportMesh->SetupAttachment(SceneRootComponent);
}

void ABlastExtendedSupportStructure::GetStructureComponents(TArray<UBlastMeshComponent*>& OutComponents)
{
	GetStructureComponents(StructureActors, OutComponents);
}


void ABlastExtendedSupportStructure::GetStructureComponents(const TArray<AActor*>& StructureActors, TArray<UBlastMeshComponent*>& OutComponents)
{
	TInlineComponentArray<UBlastMeshComponent*> Temp;
	OutComponents.Reset();
	for (AActor* Actor : StructureActors)
	{
		if (Actor)
		{
			Actor->GetComponents(Temp);
			for (UBlastMeshComponent* Mesh : Temp)
			{
				//Skip these if somehow the user added them
				if (!Mesh->IsA<UBlastExtendedSupportMeshComponent>() && Mesh->GetBlastMesh())
				{
					OutComponents.Add(Mesh);
				}
			}
		}
	}
}

void ABlastExtendedSupportStructure::PostActorCreated()
{
	Super::PostActorCreated();
#if WITH_EDITOR
	UBlastGlueWorldTag* WorldTag = UBlastGlueWorldTag::GetForWorld(GetWorld());
	if (WorldTag)
	{
		WorldTag->SupportStructures.AddUnique(this);
	}
	//Creating a new one invalidates the data
	ExtendedSupportMesh->InvalidateSupportData();
#endif
}

void ABlastExtendedSupportStructure::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	UBlastGlueWorldTag* WorldTag = UBlastGlueWorldTag::GetForWorld(GetWorld());
	if (WorldTag)
	{
		WorldTag->SupportStructures.AddUnique(this);
	}
#endif
}

void ABlastExtendedSupportStructure::Destroyed()
{
	Super::Destroyed();
#if WITH_EDITOR
	UBlastGlueWorldTag* WorldTag = UBlastGlueWorldTag::GetForWorld(GetWorld());
	if (WorldTag)
	{
		WorldTag->SupportStructures.RemoveSwap(this);
		if (WorldTag->SupportStructures.Num() == 0 && WorldTag->GlueVolumes.Num() == 0)
		{
			WorldTag->bIsDirty = false;
		}
	}
#endif
}

#if WITH_EDITOR

void ABlastExtendedSupportStructure::AddStructureActor(AActor* NewActor)
{
	StructureActors.AddUnique(NewActor);
	ExtendedSupportMesh->InvalidateSupportData();
}

void ABlastExtendedSupportStructure::RemoveStructureActor(AActor* NewActor)
{
	StructureActors.Remove(NewActor);
	ExtendedSupportMesh->InvalidateSupportData();
}

void ABlastExtendedSupportStructure::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ABlastExtendedSupportStructure, bEnabled) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ABlastExtendedSupportStructure, StructureActors) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ABlastExtendedSupportStructure, bondGenerationDistance))
	{
		ExtendedSupportMesh->InvalidateSupportData();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ABlastExtendedSupportStructure::PostEditMove(bool bFinished)
{
	if (bFinished)
	{
		ExtendedSupportMesh->InvalidateSupportData();
	}

	Super::PostEditMove(bFinished);
}


void ABlastExtendedSupportStructure::StoreSavedComponents(const TArray<FBlastExtendedStructureComponent>& SavedData, const TArray<FIntPoint>& ChunkMap, UBlastMesh* CombinedAsset)
{
	ExtendedSupportMesh->SavedComponents = SavedData;
	ExtendedSupportMesh->ChunkToOriginalChunkMap = ChunkMap;
	//Make sure our component is at the origin since we work in worldspace
	ExtendedSupportMesh->SetRelativeTransform(RootComponent->GetComponentTransform().Inverse(), false, nullptr, ETeleportType::TeleportPhysics);

	ExtendedSupportMesh->SetBlastMesh(CombinedAsset);
	ExtendedSupportMesh->SetModifiedAsset(CombinedAsset);
	MarkPackageDirty();
}

void ABlastExtendedSupportStructure::ResetActorAssociations()
{
	TArray<UBlastMeshComponent*> TempComponents;
	GetStructureComponents(TempComponents);

	for (UBlastMeshComponent* C : TempComponents)
	{
		//Clear the stored data index
		C->SetOwningSuppportStructure(this, INDEX_NONE);
	}
}

#endif

UBlastMeshExtendedSupport::UBlastMeshExtendedSupport(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}