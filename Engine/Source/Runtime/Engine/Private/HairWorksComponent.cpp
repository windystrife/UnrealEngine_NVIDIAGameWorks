// @third party code - BEGIN HairWorks
#include "Components/HairWorksComponent.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include <Nv/Common/NvCoMemoryReadStream.h>
#include "HairWorksSDK.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/HairWorksMaterial.h"
#include "Engine/HairWorksAsset.h"
#include "SkeletalRenderGPUSkin.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/HairWorksPinTransformComponent.h"
#include "HairWorksSceneProxy.h"

UHairWorksComponent::UHairWorksComponent(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Setup shadow
	CastShadow = true;
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;
	bCastInsetShadow = true;
	bCastStaticShadow = false;

	// Setup tick
	bAutoActivate = true;
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

FPrimitiveSceneProxy* UHairWorksComponent::CreateSceneProxy()
{
	return new FHairWorksSceneProxy(this, HairInstance.Hair->AssetId);
}

void UHairWorksComponent::OnAttachmentChanged()
{
	// Parent as skeleton
	ParentSkeleton = Cast<USkinnedMeshComponent>(GetAttachParent());

	// Setup mapping
	SetupBoneAndMorphMapping();

	// Refresh render data
	MarkRenderDynamicDataDirty();
}

FBoxSphereBounds UHairWorksComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_CalcHairWorksBounds);

	if (HairInstance.Hair == nullptr || HairInstance.Hair->AssetId == NvHair::ASSET_ID_NULL)
		return FBoxSphereBounds(EForceInit::ForceInit);

	UpdateBoneMatrices();

	checkSlow(BoneMatrices.Num() == 0 || BoneMatrices.Num() == ::HairWorks::GetSDK()->getNumBones(HairInstance.Hair->AssetId));

	NvHair::Vec3 HairBoundMin, HairBoundMax;
	::HairWorks::GetSDK()->getBounds(
		HairInstance.Hair->AssetId,
		BoneMatrices.Num() > 0 ? reinterpret_cast<const NvHair::Mat4x4*>(BoneMatrices.GetData()) : nullptr,
		HairBoundMin,
		HairBoundMax
	);

	FBoxSphereBounds HairBounds(FBox(reinterpret_cast<FVector&>(HairBoundMin), reinterpret_cast<FVector&>(HairBoundMax)));

	return HairBounds.TransformBy(LocalToWorld);
}

void UHairWorksComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();

	// Send data for rendering
	SendHairDynamicData();
}

void UHairWorksComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// Call super
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update pin transforms. Mainly for editor
	if(SceneProxy != nullptr && HairInstance.Hair->HairMaterial->Pins.Num() > 0)
	{
		// Get pin matrices
		auto& HairSceneProxy = static_cast<FHairWorksSceneProxy&>(*SceneProxy);
		TArray<FMatrix> PinMatrices = HairSceneProxy.GetPinMatrices();

		// Set pin component transform
		for(auto* ChildComponent : GetAttachChildren())
		{
			auto* PinComponent = Cast<UHairWorksPinTransformComponent>(ChildComponent);
			if(PinComponent == nullptr)
				continue;

			if(PinComponent->PinIndex < 0 || PinComponent->PinIndex >= PinMatrices.Num())
				continue;

			FTransform PinTransform(PinMatrices[PinComponent->PinIndex]);
			PinComponent->SetWorldLocationAndRotation(PinTransform.GetLocation(), PinTransform.GetRotation());
		}
	}

	// Mark to send dynamic data
	MarkRenderDynamicDataDirty();
}

FActorComponentInstanceData * UHairWorksComponent::GetComponentInstanceData() const
{
	/**
	 *  Component instance cached data class for HairWorks components.
	 *  Copies HairInstance and HairMaterial. Because HairInstance contains instanced reference, HairMaterial, so it's not automatically copied. And HairMaterial is a instance reference, so it's not automatically copied either.
	 */
	class FHairWorksComponentInstanceData: public FPrimitiveComponentInstanceData
	{
	public:
		FHairWorksComponentInstanceData(const UHairWorksComponent& SourceComponent)
			:FPrimitiveComponentInstanceData(&SourceComponent)
		{
			if(!SourceComponent.IsEditableWhenInherited())
				return;

			class FHairInstancePropertyWriter: public FObjectWriter
			{
			public:
				FHairInstancePropertyWriter(const UHairWorksComponent& HairComp, TArray<uint8>& InBytes)
					: FObjectWriter(InBytes)
				{
					auto* Archetype = CastChecked<UHairWorksComponent>(HairComp.GetArchetype());

					FHairWorksInstance::StaticStruct()->SerializeTaggedProperties(
						*this,
						(uint8*)&HairComp.HairInstance,
						FHairWorksInstance::StaticStruct(),
						(uint8*)&Archetype->HairInstance
					);

					UHairWorksMaterial::StaticClass()->SerializeTaggedProperties(
						*this,
						(uint8*)HairComp.HairInstance.HairMaterial,
						UHairWorksMaterial::StaticClass(),
						(uint8*)Archetype->HairInstance.HairMaterial
					);
				}

				virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
				{
					return (InProperty->HasAnyPropertyFlags(CPF_Transient | CPF_ContainsInstancedReference | CPF_InstancedReference)
						|| !InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
				}

			private:
			} HairInstancePropertyWriter(SourceComponent, SavedProperties);
		}

		virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
		{
			FPrimitiveComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);

			if(CacheApplyPhase != ECacheApplyPhase::PostUserConstructionScript || SavedProperties.Num() == 0)
				return;

			class FHairInstancePropertyReader : public FObjectReader
			{
			public:
				FHairInstancePropertyReader(UHairWorksComponent& InComponent, TArray<uint8>& InBytes)
					: FObjectReader(InBytes)
				{
					FHairWorksInstance::StaticStruct()->SerializeTaggedProperties(
						*this,
						(uint8*)&InComponent.HairInstance,
						FHairWorksInstance::StaticStruct(),
						nullptr
					);

					UHairWorksMaterial::StaticClass()->SerializeTaggedProperties(
						*this,
						(uint8*)InComponent.HairInstance.HairMaterial,
						UHairWorksMaterial::StaticClass(),
						nullptr
					);
				}
			} HairInstancePropertyReader(*CastChecked<UHairWorksComponent>(Component), SavedProperties);

			// If a property is instanced or contains instanced sub properties, it is treated as UCS modified, and it will become readonly. We fix it here. https://answers.unrealengine.com/questions/700589/struct-property-cant-be-edited-in-level-1.html. 
			TArray<UProperty*> Properties;
			Properties.Add(Component->GetClass()->FindPropertyByName("HairInstance"));
			Component->RemoveUCSModifiedProperties(Properties);
		}

	protected:
		TArray<uint8> SavedProperties;
	};

	return new FHairWorksComponentInstanceData(*this);
}

bool UHairWorksComponent::ShouldCreateRenderState() const
{
	return ::HairWorks::GetSDK() != nullptr && HairInstance.Hair != nullptr && HairInstance.Hair->AssetId != NvHair::ASSET_ID_NULL;
}

void UHairWorksComponent::CreateRenderState_Concurrent()
{
	// Call super
	Super::CreateRenderState_Concurrent();

	// Setup mapping
	SetupBoneAndMorphMapping();

	// Update bone matrices
	UpdateBoneMatrices();

	// Update proxy
	SendHairDynamicData(true);	// Ensure correct visual effect at first frame.
}

void UHairWorksComponent::SendHairDynamicData(bool bForceSkinning)const
{
	// Setup material
	if(SceneProxy == nullptr)
		return;

	TSharedRef<FHairWorksSceneProxy::FDynamicRenderData> DynamicData(new FHairWorksSceneProxy::FDynamicRenderData);

	DynamicData->Textures.SetNumZeroed(NvHair::ETextureType::COUNT_OF);
	::HairWorks::GetSDK()->getInstanceDescriptorFromAsset(HairInstance.Hair->AssetId, DynamicData->HairInstanceDesc);

	// Always load from asset to propagate visualization flags
	checkSlow(HairInstance.Hair->HairMaterial);

	auto* HairMaterial = HairInstance.Hair->HairMaterial;
	if(HairMaterial != nullptr)
	{
		HairInstance.Hair->HairMaterial->GetHairInstanceParameters(DynamicData->HairInstanceDesc, DynamicData->Textures);
	}

	// Load from component hair material
	checkSlow(HairInstance.HairMaterial->GetOuter() == this);
	if(HairInstance.HairMaterial != nullptr && HairInstance.bOverride)
	{
		HairMaterial = HairInstance.HairMaterial;

		NvHair::InstanceDescriptor OverideHairDesc;
		HairInstance.HairMaterial->GetHairInstanceParameters(OverideHairDesc, DynamicData->Textures);

		// Propagate visualization flags
#define HairWorksMergeVisFlag(FlagName) OverideHairDesc.m_visualize##FlagName |= DynamicData->HairInstanceDesc.m_visualize##FlagName

		HairWorksMergeVisFlag(Bones);
		HairWorksMergeVisFlag(BoundingBox);
		HairWorksMergeVisFlag(Capsules);
		HairWorksMergeVisFlag(ControlVertices);
		HairWorksMergeVisFlag(GrowthMesh);
		HairWorksMergeVisFlag(GuideHairs);
		HairWorksMergeVisFlag(HairInteractions);
		HairWorksMergeVisFlag(PinConstraints);
		HairWorksMergeVisFlag(ShadingNormals);
		HairWorksMergeVisFlag(ShadingNormalBone);
		HairWorksMergeVisFlag(SkinnedGuideHairs);
#undef HairWorksMergeVisFlag

		OverideHairDesc.m_drawRenderHairs &= DynamicData->HairInstanceDesc.m_drawRenderHairs;

		if(OverideHairDesc.m_colorizeMode == NvHair::ColorizeMode::NONE)
			OverideHairDesc.m_colorizeMode = DynamicData->HairInstanceDesc.m_colorizeMode;

		DynamicData->HairInstanceDesc = OverideHairDesc;
	}

	// Disable simulation
	if(bForceSkinning)
		DynamicData->HairInstanceDesc.m_simulate = false;

	if (HairMaterial)
	{
		// Hair normal center
		auto* BoneIdx = HairInstance.Hair->BoneNameToIdx.Find(HairMaterial->HairNormalCenter);
		if (BoneIdx != nullptr)
			DynamicData->HairInstanceDesc.m_hairNormalBoneIndex = *BoneIdx;
		else
			DynamicData->HairInstanceDesc.m_hairNormalWeight = 0;

		// Simulation flag
		DynamicData->bSimulateInWorldSpace = HairMaterial->bSimulateInWorldSpace;
	}

	// Set skinning data
	DynamicData->BoneMatrices = BoneMatrices;

	// Setup pins
	if(HairInstance.Hair->PinsUpdateFrameNumber != GFrameNumber && HairInstance.Hair->HairMaterial->Pins.Num() > 0)
	{
		HairInstance.Hair->PinsUpdateFrameNumber = GFrameNumber;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			HairUpdatePins,
			NvHair::AssetId, AssetId, HairInstance.Hair->AssetId,
			const TArray<FHairWorksPin>, EnginePins, HairInstance.Hair->HairMaterial->Pins,
			{
				TArray<NvHair::Pin> Pins;
				Pins.AddDefaulted(EnginePins.Num());
				::HairWorks::GetSDK()->getPins(AssetId, 0, Pins.Num(), Pins.GetData());

				for(auto PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
				{
					auto& Pin = Pins[PinIndex];

					const auto& SrcPin = EnginePins[PinIndex];

					Pin.m_useDynamicPin = SrcPin.bDynamicPin;
					Pin.m_doLra = SrcPin.bTetherPin;
					Pin.m_pinStiffness = SrcPin.Stiffness;
					Pin.m_influenceFallOff = SrcPin.InfluenceFallOff;
					Pin.m_influenceFallOffCurve = reinterpret_cast<const NvHair::Vec4&>(SrcPin.InfluenceFallOffCurve);
				}

				::HairWorks::GetSDK()->setPins(AssetId, 0, Pins.Num(), Pins.GetData());
			}
		);
	}

	// Add pin meshes
	DynamicData->PinMeshes.AddDefaulted(HairInstance.Hair->HairMaterial->Pins.Num());

	for(auto* ChildComponent : GetAttachChildren())
	{
		// Find pin transform component
		auto* PinComponent = Cast<UHairWorksPinTransformComponent>(ChildComponent);
		if(PinComponent == nullptr)
			continue;

		if(PinComponent->PinIndex < 0 || PinComponent->PinIndex >= DynamicData->PinMeshes.Num())
			continue;

		// Collect pin meshes
		auto& PinMeshes = DynamicData->PinMeshes[PinComponent->PinIndex];

		TFunction<bool(const USceneComponent*)> AddPinMesh;
		AddPinMesh = [&](const USceneComponent* Component)
		{
			// Add mesh
			if(Component->IsPendingKill())
				return false;

			auto* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);

			if(PrimitiveComponent != nullptr && PrimitiveComponent->SceneProxy != nullptr && !PrimitiveComponent->IsRenderStateDirty())
			{
				FHairWorksSceneProxy::FPinMesh PinMesh;
				PinMesh.Mesh = PrimitiveComponent->SceneProxy;
				PinMesh.LocalTransform = PrimitiveComponent->GetRelativeTransform().ToMatrixWithScale();

				PinMeshes.Add(PinMesh);
			}

			// Find in children
			Component->GetAttachChildren().FindByPredicate(AddPinMesh);

			return false;
		};

		AddPinMesh(PinComponent);
	}

	// Update morph data
	do
	{
		if(MorphIndices.Num() <= 0 || ParentSkeleton == nullptr || ParentSkeleton->MeshObject == nullptr)
			break;

		if(ParentSkeleton->MeshObject->IsCPUSkinned())
			break;

		DynamicData->ParentSkin = static_cast<FSkeletalMeshObjectGPUSkin*>(ParentSkeleton->MeshObject);
	} while(false);	

	// Send to proxy
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		HairUpdateDynamicData,
		FHairWorksSceneProxy&, ThisProxy, static_cast<FHairWorksSceneProxy&>(*SceneProxy),
		TSharedRef<FHairWorksSceneProxy::FDynamicRenderData>, DynamicData, DynamicData,
		{
			ThisProxy.UpdateDynamicData_RenderThread(*DynamicData);
		}
	);

	// Force to simulate for new created instance
	static const auto& CVarHairFrameRateIndepedentRendering = *IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairWorks.FrameRateIndependentRendering"));
	if(CVarHairFrameRateIndepedentRendering.GetInt() != 0 && bForceSkinning)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			HairForceSimulation,
			FHairWorksSceneProxy&, ThisProxy, static_cast<FHairWorksSceneProxy&>(*SceneProxy),
			{
				if(ThisProxy.GetHairInstanceId() != NvHair::INSTANCE_ID_NULL)
					::HairWorks::GetSDK()->stepInstanceSimulation(ThisProxy.GetHairInstanceId(), 0);
			}
		);
	}
}

void UHairWorksComponent::SetupBoneAndMorphMapping()
{
	// Setup bone mapping
	if (::HairWorks::GetSDK() == nullptr || HairInstance.Hair == nullptr || ParentSkeleton == nullptr || ParentSkeleton->SkeletalMesh == nullptr)
		return;

	auto& Bones = ParentSkeleton->SkeletalMesh->RefSkeleton.GetRefBoneInfo();
	BoneIndices.SetNumUninitialized(HairInstance.Hair->BoneNames.Num());

	for(auto Idx = 0; Idx < BoneIndices.Num(); ++Idx)
	{
		BoneIndices[Idx] = Bones.IndexOfByPredicate(
			[&](const FMeshBoneInfo& BoneInfo){return BoneInfo.Name == HairInstance.Hair->BoneNames[Idx]; }
		);
	}

	// Setup morph index mapping
	do
	{
		if(ParentSkeleton->SkeletalMesh->MorphTargets.Num() <= 0)
		{
			MorphIndices.Empty(true);
			break;
		}

		// Check if parent skeletal mesh has changed. 
		if(!bAutoRemapMorphTarget)
		{
			if(CachedSkeletalMeshForMorph == ParentSkeleton->SkeletalMesh)
				break;

			CachedSkeletalMeshForMorph = ParentSkeleton->SkeletalMesh;
		}

		// Get vertices of parent skeletal mesh
		const auto& ParentMeshVertexBuffer = ParentSkeleton->SkeletalMesh->GetResourceForRendering()->LODModels[0].VertexBufferGPUSkin;

		TArray<FVector> ParentMeshVertices;
		ParentMeshVertices.SetNumUninitialized(ParentMeshVertexBuffer.GetNumVertices());

		for(auto VertexIdx = 0; VertexIdx < ParentMeshVertices.Num(); ++VertexIdx)
		{
			ParentMeshVertices[VertexIdx] = ParentMeshVertexBuffer.GetVertexPositionSlow(VertexIdx);
		}

		// Get vertices of hair growth mesh
		const auto GuideNum = ::HairWorks::GetSDK()->getNumGuideHairs(HairInstance.Hair->AssetId);

		TArray<FVector> GuideRootVertices;
		GuideRootVertices.SetNumUninitialized(GuideNum);
		::HairWorks::GetSDK()->getRootVertices(HairInstance.Hair->AssetId, reinterpret_cast<NvHair::Vec3*>(GuideRootVertices.GetData()));

		// Find closest skeletal mesh vertex for each vertex of HairWorks growth mesh
		const auto Transform = GetRelativeTransform();

		MorphIndices.SetNumUninitialized(GuideNum, true);

		for(auto GuideIdx = 0; GuideIdx < GuideNum; ++GuideIdx)
		{
			const auto GuideRootVertex = Transform.TransformPosition(GuideRootVertices[GuideIdx]);

			float ClosestSqrDist = FLT_MAX;
			int32 ClosestVertexIdx = 0;

			for(auto VertexIdx = 0; VertexIdx < ParentMeshVertices.Num(); ++VertexIdx)
			{
				const float SqrDist = FVector::DistSquared(GuideRootVertex, ParentMeshVertices[VertexIdx]);
				if(SqrDist < ClosestSqrDist)
				{
					ClosestSqrDist = SqrDist;
					ClosestVertexIdx = VertexIdx;
				}
			}

			MorphIndices[GuideIdx] = ClosestVertexIdx;
		}

		// Propagate to all instances in editor
#if WITH_EDITOR
		do
		{
			if(bAutoRemapMorphTarget)
				break;

			auto* Archetype = GetArchetype();
			if(Archetype == nullptr || Archetype->HasAllFlags(RF_ClassDefaultObject))
				break;

			TArray<UObject*> Instances;
			Archetype->GetArchetypeInstances(Instances);

			Instances.Add(Archetype);

			for(auto* Instance : Instances)
			{
				auto* HairWorksComp = CastChecked<UHairWorksComponent>(Instance);
				HairWorksComp->CachedSkeletalMeshForMorph = CachedSkeletalMeshForMorph;
				HairWorksComp->MorphIndices = MorphIndices;
				HairWorksComp->Modify();
			}
		} while(false);
#endif
	} while(false);

	// Update morph indices to scene proxy
	if (SceneProxy)
	{
		const auto& LocalMorphIndices = MorphIndices;
		auto& HairWorksSceneProxy = static_cast<FHairWorksSceneProxy&>(*SceneProxy);
		auto UpdateMorphIndices = [&HairWorksSceneProxy, LocalMorphIndices]()
		{
			HairWorksSceneProxy.UpdateMorphIndices_RenderThread(LocalMorphIndices);
		};

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			HairUpdateMorphIndices,
			decltype(UpdateMorphIndices), UpdateMorphIndices, UpdateMorphIndices,
			{
				UpdateMorphIndices();
			}
		);
	}
}

void UHairWorksComponent::UpdateBoneMatrices()const
{
	if(ParentSkeleton == nullptr)
	{
		BoneMatrices.Empty();
		return;
	}

	BoneMatrices.Init(FMatrix::Identity, BoneIndices.Num());

	for(auto Idx = 0; Idx < BoneIndices.Num(); ++Idx)
	{
		const uint16& IdxInParent = BoneIndices[Idx];
		if(IdxInParent >= ParentSkeleton->GetComponentSpaceTransforms().Num())
			continue;

		const auto Matrix = ParentSkeleton->GetComponentSpaceTransforms()[IdxInParent].ToMatrixWithScale();

		BoneMatrices[Idx] = ParentSkeleton->SkeletalMesh->RefBasesInvMatrix[IdxInParent] * Matrix;
	}
}

void UHairWorksComponent::Serialize(FArchive & Ar)
{
	Super::Serialize(Ar);

	// When it's duplicated in Blueprints editor, instanced reference is shared, not duplicated. This should be a bug. So we have to duplicate hair material, which is a instanced reference, by ourselves.
	if(Ar.IsLoading() && HairInstance.HairMaterial->GetOuter() != this)
	{
		HairInstance.HairMaterial = CastChecked<UHairWorksMaterial>(StaticDuplicateObject(HairInstance.HairMaterial, this));
	}

	// Fix object flag for old assets
	HairInstance.HairMaterial->SetFlags(GetMaskedFlags(RF_PropagateToSubObjects));
}

void UHairWorksComponent::PostInitProperties()
{
	// Inherits parent flags. One purpose is to avoid "Graph is linked to private object(s) in an external package." error in UPackage::SavePackage(). Another purpose is to inherit archetype flag.
	if(!HasAnyFlags(RF_NeedLoad))
		HairInstance.HairMaterial = NewObject<UHairWorksMaterial>(this, NAME_None, GetMaskedFlags(RF_PropagateToSubObjects));

	Super::PostInitProperties();
}

// @third party code - END HairWorks
