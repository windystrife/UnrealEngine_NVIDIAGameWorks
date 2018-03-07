// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveComponent.cpp: Primitive component implementation.
=============================================================================*/

#include "Components/PrimitiveComponent.h"
#include "EngineStats.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "WorldCollision.h"
#include "AI/Navigation/NavigationSystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/WorldSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "ContentStreaming.h"
#include "DrawDebugHelpers.h"
#include "UnrealEngine.h"
#include "PhysicsPublic.h"
#include "PhysicsEngine/BodySetup.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "CollisionDebugDrawingPublic.h"
#include "GameFramework/CheatManager.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "PrimitiveSceneProxy.h"

#define LOCTEXT_NAMESPACE "PrimitiveComponent"

//////////////////////////////////////////////////////////////////////////
// Globals

namespace PrimitiveComponentStatics
{
	static const FText MobilityWarnText = LOCTEXT("InvalidMove", "move");
}

typedef TArray<FOverlapInfo, TInlineAllocator<3>> TInlineOverlapInfoArray;

DEFINE_LOG_CATEGORY_STATIC(LogPrimitiveComponent, Log, All);

static int32 bAllowCachedOverlapsCVar = 1;
static FAutoConsoleVariableRef CVarAllowCachedOverlaps(
	TEXT("p.AllowCachedOverlaps"), 
	bAllowCachedOverlapsCVar,
	TEXT("Primitive Component physics\n")
	TEXT("0: disable cached overlaps, 1: enable (default)"),
	ECVF_Default);

static float InitialOverlapToleranceCVar = 0.0f;
static FAutoConsoleVariableRef CVarInitialOverlapTolerance(
	TEXT("p.InitialOverlapTolerance"),
	InitialOverlapToleranceCVar,
	TEXT("Tolerance for initial overlapping test in PrimitiveComponent movement.\n")
	TEXT("Normals within this tolerance are ignored if moving out of the object.\n")
	TEXT("Dot product of movement direction and surface normal."),
	ECVF_Default);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 CVarShowInitialOverlaps = 0;
FAutoConsoleVariableRef CVarRefShowInitialOverlaps(
	TEXT("p.ShowInitialOverlaps"),
	CVarShowInitialOverlaps,
	TEXT("Show initial overlaps when moving a component, including estimated 'exit' direction.\n")
	TEXT(" 0:off, otherwise on"),
	ECVF_Cheat);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static int32 bEnableFastOverlapCheck = 1;
static FAutoConsoleVariableRef CVarEnableFastOverlapCheck(TEXT("p.EnableFastOverlapCheck"), bEnableFastOverlapCheck, TEXT("Enable fast overlap check against sweep hits, avoiding UpdateOverlaps (for the swept component)."));
DECLARE_CYCLE_STAT(TEXT("MoveComponent FastOverlap"), STAT_MoveComponent_FastOverlap, STATGROUP_Game);

// Predicate to determine if an overlap is with a certain AActor.
struct FPredicateOverlapHasSameActor
{
	FPredicateOverlapHasSameActor(const AActor& Owner)
	: MyOwner(Owner)
	{
	}

	bool operator() (const FOverlapInfo& Info)
	{
		return Info.OverlapInfo.Actor == &MyOwner;
	}

private:
	const AActor& MyOwner;
};

// Predicate to determine if an overlap is *NOT* with a certain AActor.
struct FPredicateOverlapHasDifferentActor
{
	FPredicateOverlapHasDifferentActor(const AActor& Owner)
	: MyOwner(Owner)
	{
	}

	bool operator() (const FOverlapInfo& Info)
	{
		return Info.OverlapInfo.Actor != &MyOwner;
	}

private:
	const AActor& MyOwner;
};


FORCEINLINE_DEBUGGABLE static bool CanComponentsGenerateOverlap(const UPrimitiveComponent* MyComponent, /*const*/ UPrimitiveComponent* OtherComp)
{
	return OtherComp
		&& OtherComp->bGenerateOverlapEvents
		&& MyComponent
		&& MyComponent->bGenerateOverlapEvents
		&& MyComponent->GetCollisionResponseToComponent(OtherComp) == ECR_Overlap;
}

// Predicate to remove components from overlaps array that can no longer overlap
struct FPredicateFilterCannotOverlap
{
	FPredicateFilterCannotOverlap(const UPrimitiveComponent& OwningComponent)
	: MyComponent(OwningComponent)
	{
	}

	bool operator() (const FOverlapInfo& Info) const
	{
		return !CanComponentsGenerateOverlap(&MyComponent, Info.OverlapInfo.GetComponent());
	}

private:
	const UPrimitiveComponent& MyComponent;
};


///////////////////////////////////////////////////////////////////////////////
// PRIMITIVE COMPONENT
///////////////////////////////////////////////////////////////////////////////

int32 UPrimitiveComponent::CurrentTag = 2147483647 / 4;

// 0 is reserved to mean invalid
FThreadSafeCounter UPrimitiveComponent::NextComponentId;

UPrimitiveComponent::UPrimitiveComponent(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	PostPhysicsComponentTick.bCanEverTick = false;
	PostPhysicsComponentTick.bStartWithTickEnabled = true;
	PostPhysicsComponentTick.TickGroup = TG_PostPhysics;

	LastRenderTime = -1000.0f;
	LastRenderTimeOnScreen = -1000.0f;
	BoundsScale = 1.0f;
	MinDrawDistance = 0.0f;
	DepthPriorityGroup = SDPG_World;
	bAllowCullDistanceVolume = true;
	bUseAsOccluder = false;
	bReceivesDecals = true;
	CastShadow = false;
	bCastDynamicShadow = true;
	bAffectDynamicIndirectLighting = true;
	bAffectDistanceFieldLighting = true;
	LpvBiasMultiplier = 1.0f;
	bCastStaticShadow = true;
	bCastVolumetricTranslucentShadow = false;
	IndirectLightingCacheQuality = ILCQ_Point;
	bSelectable = true;
	AlwaysLoadOnClient = true;
	AlwaysLoadOnServer = true;
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	bAlwaysCreatePhysicsState = false;
	bVisibleInReflectionCaptures = true;
	bRenderInMainPass = true;
	VisibilityId = INDEX_NONE;
	CanBeCharacterBase_DEPRECATED = ECB_Yes;
	CanCharacterStepUpOn = ECB_Yes;
	ComponentId.PrimIDValue = NextComponentId.Increment();
	CustomDepthStencilValue = 0;
	CustomDepthStencilWriteMask = ERendererStencilMask::ERSM_Default;

	bUseEditorCompositing = false;

	bGenerateOverlapEvents = true;
	bMultiBodyOverlap = false;
	bCheckAsyncSceneOnMove = false;
	bReturnMaterialOnMove = false;
	bCanEverAffectNavigation = false;
	bNavigationRelevant = false;

	bWantsOnUpdateTransform = true;

	bCachedAllCollideableDescendantsRelative = false;
	bAttachedToStreamingManagerAsStatic = false;
	bAttachedToStreamingManagerAsDynamic = false;
	bHandledByStreamingManagerAsDynamic = false;
	LastCheckedAllCollideableDescendantsTime = 0.f;
	
	bApplyImpulseOnDamage = true;

#if WITH_EDITORONLY_DATA
	bEnableAutoLODGeneration = true;
#endif // WITH_EDITORONLY_DATA

#if WITH_FLEX
	FlexParticleCount = 0;

	bIsFlexParent = 0;
	bFlexParticleDrain = 0;
	bFlexEnableParticleCounter = 0;
#endif
}

bool UPrimitiveComponent::UsesOnlyUnlitMaterials() const
{
	return false;
}


bool UPrimitiveComponent::GetLightMapResolution( int32& Width, int32& Height ) const
{
	Width	= 0;
	Height	= 0;
	return false;
}


void UPrimitiveComponent::GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const
{
	LightMapMemoryUsage		= 0;
	ShadowMapMemoryUsage	= 0;
	return;
}

void UPrimitiveComponent::InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) 
{
	// If a static lighting build has been enqueued for this primitive, don't stomp on its visibility ID.
	if (bInvalidateBuildEnqueuedLighting)
	{
		VisibilityId = INDEX_NONE;
	}

	Super::InvalidateLightingCacheDetailed(bInvalidateBuildEnqueuedLighting, bTranslationOnly);
}

bool UPrimitiveComponent::IsEditorOnly() const
{
	return Super::IsEditorOnly() || ((AlwaysLoadOnClient == false) && (AlwaysLoadOnServer == false));
}

bool UPrimitiveComponent::HasStaticLighting() const
{
	return ((Mobility == EComponentMobility::Static) || bLightAsIfStatic) && SupportsStaticLighting();
}

void UPrimitiveComponent::GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	if (CVarStreamingUseNewMetrics.GetValueOnGameThread() != 0)
	{
		LevelContext.BindBuildData(nullptr);

		TArray<UMaterialInterface*> UsedMaterials;
		GetUsedMaterials(UsedMaterials);

		if (UsedMaterials.Num())
		{
			// As we have no idea what this component is doing, we assume something very conservative
			// by specifying that the texture is stretched across the bounds. To do this, we use a density of 1
			// while also specifying the component scale as the bound radius. 
			// Note that material UV scaling will  still apply.
			static FMeshUVChannelInfo UVChannelData;
			if (!UVChannelData.bInitialized)
			{
				UVChannelData.bInitialized = true;
				for (float& Density : UVChannelData.LocalUVDensities)
				{
					Density = 1.f;
				}
			}

			FPrimitiveMaterialInfo MaterialData;
			MaterialData.PackedRelativeBox = PackedRelativeBox_Identity;
			MaterialData.UVChannelData = &UVChannelData;

			TArray<UTexture*> UsedTextures;
			for (UMaterialInterface* MaterialInterface : UsedMaterials)
			{
				if (MaterialInterface)
				{
					MaterialData.Material = MaterialInterface;
					LevelContext.ProcessMaterial(Bounds, MaterialData, Bounds.SphereRadius, OutStreamingTextures);
				}
			}
		}
	}
}


void UPrimitiveComponent::GetStreamingTextureInfoWithNULLRemoval(FStreamingTextureLevelContext& LevelContext, TArray<struct FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	if (!IsRegistered() || SceneProxy) // If registered but without a scene proxy, then this is not visible.
	{
		GetStreamingTextureInfo(LevelContext, OutStreamingTextures);
		for (int32 Index = 0; Index < OutStreamingTextures.Num(); Index++)
		{
			const FStreamingTexturePrimitiveInfo& Info = OutStreamingTextures[Index];
			if (!IsStreamingTexture(Info.Texture))
			{
				OutStreamingTextures.RemoveAtSwap(Index--);
			}
			else
			{
				// Other wise check that everything is setup right. If the component is not yet registered, then the bound data is irrelevant.
				const bool bCanBeStreamedByDistance = Info.TexelFactor > SMALL_NUMBER && (Info.Bounds.SphereRadius > SMALL_NUMBER || !IsRegistered()) && ensure(FMath::IsFinite(Info.TexelFactor));
				if (!bForceMipStreaming && !bCanBeStreamedByDistance && !(Info.TexelFactor < 0 && Info.Texture->LODGroup == TEXTUREGROUP_Terrain_Heightmap))
				{
					OutStreamingTextures.RemoveAtSwap(Index--);
				}
			}
		}
	}
}

void UPrimitiveComponent::GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel)
{
	// Get the used materials so we can get their textures
	TArray<UMaterialInterface*> UsedMaterials;
	GetUsedMaterials( UsedMaterials );

	TArray<UTexture*> UsedTextures;
	for( int32 MatIndex = 0; MatIndex < UsedMaterials.Num(); ++MatIndex )
	{
		// Ensure we don't have any NULL elements.
		if( UsedMaterials[ MatIndex ] )
		{
			auto World = GetWorld();

			UsedTextures.Reset();
			UsedMaterials[MatIndex]->GetUsedTextures(UsedTextures, QualityLevel, false, World ? World->FeatureLevel : GMaxRHIFeatureLevel, false);

			for( int32 TextureIndex=0; TextureIndex<UsedTextures.Num(); TextureIndex++ )
			{
				OutTextures.AddUnique( UsedTextures[TextureIndex] );
			}
		}
	}
}

/** 
* Abstract function actually execute the tick. 
* @param DeltaTime - frame time to advance, in seconds
* @param TickType - kind of tick for this frame
* @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
* @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completetion of this task until certain child tasks are complete.
**/
void FPrimitiveComponentPostPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime){ Target->PostPhysicsTick(*this); });
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/** Abstract function to describe this tick. Used to print messages about illegal cycles in the dependency graph **/
FString FPrimitiveComponentPostPhysicsTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[UPrimitiveComponent::PostPhysicsTick]");
}

void UPrimitiveComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	if (bRegister)
	{
		if (SetupActorComponentTickFunction(&PostPhysicsComponentTick))
		{
			PostPhysicsComponentTick.Target = this;

			// If primary tick is registered, add a prerequisate to it
			if(PrimaryComponentTick.bCanEverTick)
			{
				PostPhysicsComponentTick.AddPrerequisite(this,PrimaryComponentTick); 
			}

			// Set a prereq for the post physics tick to happen after physics is finished
			UWorld* World = GetWorld();
			if (World != nullptr)
			{
				PostPhysicsComponentTick.AddPrerequisite(World, World->EndPhysicsTickFunction);
			}
		}
	}
	else
	{
		if(PostPhysicsComponentTick.IsTickFunctionRegistered())
		{
			PostPhysicsComponentTick.UnRegisterTickFunction();
		}
	}
}

void UPrimitiveComponent::SetPostPhysicsComponentTickEnabled(bool bEnable)
{
	// @todo ticking, James, turn this off when not needed
	if (!IsTemplate() && PostPhysicsComponentTick.bCanEverTick)
	{
		PostPhysicsComponentTick.SetTickFunctionEnable(bEnable);
	}
}

bool UPrimitiveComponent::IsPostPhysicsComponentTickEnabled() const
{
	return PostPhysicsComponentTick.IsTickFunctionEnabled();
}


//////////////////////////////////////////////////////////////////////////
// Render

void UPrimitiveComponent::CreateRenderState_Concurrent()
{
	// Make sure cached cull distance is up-to-date if its zero and we have an LD cull distance
	if( CachedMaxDrawDistance == 0.f && LDMaxDrawDistance > 0.f )
	{
		CachedMaxDrawDistance = LDMaxDrawDistance;
	}

	Super::CreateRenderState_Concurrent();

	UpdateBounds();

	// If the primitive isn't hidden and the detail mode setting allows it, add it to the scene.
	if (ShouldComponentAddToScene())
	{
		GetWorld()->Scene->AddPrimitive(this);
	}

	// To prevent processing components twice (since they are also processed in the FLevelTextureManager when the level becomes visible)
	// here we only handles component that are already dynamic and that need an updates.
	if (bHandledByStreamingManagerAsDynamic)
	{
		FStreamingManagerCollection* Collection = IStreamingManager::Get_Concurrent();
		if (Collection)
		{
			Collection->NotifyPrimitiveUpdated_Concurrent(this);
		}
	}
}

void UPrimitiveComponent::SendRenderTransform_Concurrent()
{
	UpdateBounds();

	// If the primitive isn't hidden update its transform.
	const bool bDetailModeAllowsRendering	= DetailMode <= GetCachedScalabilityCVars().DetailMode;
	if( bDetailModeAllowsRendering && (ShouldRender() || bCastHiddenShadow))
	{
		// Update the scene info's transform for this primitive.
		GetWorld()->Scene->UpdatePrimitiveTransform(this);
	}

	Super::SendRenderTransform_Concurrent();
}

void UPrimitiveComponent::OnRegister()
{
	Super::OnRegister();

	if (bCanEverAffectNavigation)
	{
		const bool bNavRelevant = bNavigationRelevant = IsNavigationRelevant();
		if (bNavRelevant)
		{
			UNavigationSystem::OnComponentRegistered(this);
		}
	}
	else
	{
		bNavigationRelevant = false;
	}
}


void UPrimitiveComponent::OnUnregister()
{
	// If this is being garbage collected we don't really need to worry about clearing this
	if (!HasAnyFlags(RF_BeginDestroyed) && !IsUnreachable())
	{
		UWorld* World = GetWorld();
		if (World && World->Scene)
		{
			World->Scene->ReleasePrimitive(this);
		}
	}

	Super::OnUnregister();

	// Unregister only has effect on dynamic primitives (as static ones are handled when the level visibility changes).
	if (bAttachedToStreamingManagerAsDynamic)
	{
		IStreamingManager::Get().NotifyPrimitiveDetached(this);
	}

	if (bCanEverAffectNavigation)
	{
		UNavigationSystem::OnComponentUnregistered(this);
	}
}

FPrimitiveComponentInstanceData::FPrimitiveComponentInstanceData(const UPrimitiveComponent* SourceComponent)
	: FSceneComponentInstanceData(SourceComponent)
	, VisibilityId(SourceComponent->VisibilityId)
	, LODParent(SourceComponent->GetLODParentPrimitive())
{
	const_cast<UPrimitiveComponent*>(SourceComponent)->ConditionalUpdateComponentToWorld(); // sadness
	ComponentTransform = SourceComponent->GetComponentTransform();
}

void FPrimitiveComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);

	UPrimitiveComponent* PrimitiveComponent = CastChecked<UPrimitiveComponent>(Component);

#if WITH_EDITOR
	// This is needed to restore transient collision profile data.
	PrimitiveComponent->UpdateCollisionProfile();
#endif // #if WITH_EDITOR
	PrimitiveComponent->SetLODParentPrimitive(LODParent);

	if (VisibilityId != INDEX_NONE && GetComponentTransform().Equals(PrimitiveComponent->GetComponentTransform(), 1.e-3f))
	{
		PrimitiveComponent->VisibilityId = VisibilityId;
	}

	if (Component->IsRegistered() && ((VisibilityId != INDEX_NONE) || ContainsSavedProperties()))
	{
		Component->MarkRenderStateDirty();
	}
}

bool FPrimitiveComponentInstanceData::ContainsData() const
{
	return (ContainsSavedProperties() || AttachedInstanceComponents.Num() > 0 || LODParent || (VisibilityId != INDEX_NONE));
}

void FPrimitiveComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	FSceneComponentInstanceData::AddReferencedObjects(Collector);

	// if LOD Parent
	if (LODParent)
	{
		Collector.AddReferencedObject(LODParent);
	}
}

void FPrimitiveComponentInstanceData::FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	FSceneComponentInstanceData::FindAndReplaceInstances(OldToNewInstanceMap);

	// if LOD Parent 
	if (LODParent)
	{
		if (UObject* const* NewLODParent = OldToNewInstanceMap.Find(LODParent))
		{
			LODParent = CastChecked<UPrimitiveComponent>(*NewLODParent, ECastCheckedType::NullAllowed);
		}
	}
}

FActorComponentInstanceData* UPrimitiveComponent::GetComponentInstanceData() const
{
	FPrimitiveComponentInstanceData* InstanceData = new FPrimitiveComponentInstanceData(this);

	if (!InstanceData->ContainsData())
	{
		delete InstanceData;
		InstanceData = nullptr;
	}

	return InstanceData;
}

void UPrimitiveComponent::OnAttachmentChanged()
{
	UWorld* World = GetWorld();
	if (World && World->Scene)
	{
		World->Scene->UpdatePrimitiveAttachment(this);
	}
}

void UPrimitiveComponent::DestroyRenderState_Concurrent()
{
	// Remove the primitive from the scene.
	UWorld* World = GetWorld();
	if(World && World->Scene)
	{
		World->Scene->RemovePrimitive(this);
	}

	Super::DestroyRenderState_Concurrent();
}


//////////////////////////////////////////////////////////////////////////
// Physics

void UPrimitiveComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	// if we have a scene, we don't want to disable all physics and we have no bodyinstance already
	if(!BodyInstance.IsValidBodyInstance())
	{
		//UE_LOG(LogPrimitiveComponent, Warning, TEXT("Creating Physics State (%s : %s)"), *GetNameSafe(GetOuter()),  *GetName());

		UBodySetup* BodySetup = GetBodySetup();
		if(BodySetup)
		{
			// Create new BodyInstance at given location.
			FTransform BodyTransform = GetComponentTransform();

			// Here we make sure we don't have zero scale. This still results in a body being made and placed in
			// world (very small) but is consistent with a body scaled to zero.
			const FVector BodyScale = BodyTransform.GetScale3D();
			if(BodyScale.IsNearlyZero())
			{
				BodyTransform.SetScale3D(FVector(KINDA_SMALL_NUMBER));
			}

#if UE_WITH_PHYSICS
			// Create the body.
			BodyInstance.InitBody(BodySetup, BodyTransform, this, GetWorld()->GetPhysicsScene());		
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			SendRenderDebugPhysics();
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif //UE_WITH_PHYSICS

#if WITH_EDITOR
			// Make sure we have a valid body instance here. As we do not keep BIs with no collision shapes at all,
			// we don't want to create cloth collision in these cases
			if (BodyInstance.IsValidBodyInstance())
			{
				const float RealMass = BodyInstance.GetBodyMass();
				const float CalcedMass = BodySetup->CalculateMass(this);
				float MassDifference =  RealMass - CalcedMass;
				if (RealMass > 1.0f && FMath::Abs(MassDifference) > 0.1f)
				{
					UE_LOG(LogPhysics, Log, TEXT("Calculated mass differs from real mass for %s:%s. Mass: %f  CalculatedMass: %f"),
						GetOwner() != NULL ? *GetOwner()->GetName() : TEXT("NoActor"),
						*GetName(), RealMass, CalcedMass);
				}
			}
#endif // WITH_EDITOR
		}
	}
}	

void UPrimitiveComponent::EnsurePhysicsStateCreated()
{
	// if physics is created when it shouldn't OR if physics isn't created when it should
	// we should fix it up
	if ( IsPhysicsStateCreated() != ShouldCreatePhysicsState() )
	{
		RecreatePhysicsState();
	}
}

bool UPrimitiveComponent::IsWelded() const
{
	return BodyInstance.WeldParent != nullptr;
}

void UPrimitiveComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	// Always send new transform to physics
	if(bPhysicsStateCreated && !(UpdateTransformFlags & EUpdateTransformFlags::SkipPhysicsUpdate))
	{
		//If we update transform of welded bodies directly (i.e. on the actual component) we need to update the shape transforms of the parent.
		//If the parent is updated, any welded shapes are automatically updated so we don't need to do this physx update.
		//If the parent is updated and we are NOT welded, the child still needs to update physx
		const bool bTransformSetDirectly = !(UpdateTransformFlags & EUpdateTransformFlags::PropagateFromParent);
		if( bTransformSetDirectly || !IsWelded())
		{
			SendPhysicsTransform(Teleport);
		}
	}
}

void UPrimitiveComponent::SendPhysicsTransform(ETeleportType Teleport)
{
	BodyInstance.SetBodyTransform(GetComponentTransform(), Teleport);
	BodyInstance.UpdateBodyScale(GetComponentTransform().GetScale3D());
}

void UPrimitiveComponent::OnDestroyPhysicsState()
{
	// we remove welding related to this component
	UnWeldFromParent();
	UnWeldChildren();

	// clean up physics engine representation
	if(BodyInstance.IsValidBodyInstance())
	{
		// We tell the BodyInstance to shut down the physics-engine data.
		BodyInstance.TermBody();
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SendRenderDebugPhysics();
#endif

	Super::OnDestroyPhysicsState();
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UPrimitiveComponent::SendRenderDebugPhysics(FPrimitiveSceneProxy* OverrideSceneProxy)
{
	FPrimitiveSceneProxy* UseSceneProxy = OverrideSceneProxy ? OverrideSceneProxy : SceneProxy;
	if (UseSceneProxy)
	{
		TArray<FPrimitiveSceneProxy::FDebugMassData> DebugMassData;
		if (!IsWelded() && Mobility != EComponentMobility::Static)
		{
			if (FBodyInstance* BI = GetBodyInstance())
			{
				if (BI->IsValidBodyInstance())
				{
					DebugMassData.AddDefaulted();
					FPrimitiveSceneProxy::FDebugMassData& RootMassData = DebugMassData[0];
					const FTransform MassToWorld = BI->GetMassSpaceToWorldSpace();

					RootMassData.LocalCenterOfMass = GetComponentTransform().InverseTransformPosition(MassToWorld.GetLocation());
					RootMassData.LocalTensorOrientation = MassToWorld.GetRotation() * GetComponentTransform().GetRotation().Inverse();
					RootMassData.MassSpaceInertiaTensor = BI->GetBodyInertiaTensor();
					RootMassData.BoneIndex = INDEX_NONE;
				}
			}
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			PrimitiveComponent_SendRenderDebugPhysics, FPrimitiveSceneProxy*, PassedSceneProxy, UseSceneProxy, TArray<FPrimitiveSceneProxy::FDebugMassData>, UseDebugMassData, DebugMassData,
		{
				PassedSceneProxy->SetDebugMassData(UseDebugMassData);
		});
	}
}
#endif

FMatrix UPrimitiveComponent::GetRenderMatrix() const
{
	return GetComponentTransform().ToMatrixWithScale();
}

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// as temporary fix for the bug TTP 299926
	// permanent fix is coming
	if (IsTemplate())
	{
		BodyInstance.FixupData(this);
	}
}

#if WITH_EDITOR
void UPrimitiveComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Keep track of old cached cull distance to see whether we need to re-attach component.
	const float OldCachedMaxDrawDistance = CachedMaxDrawDistance;

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if(PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// CachedMaxDrawDistance needs to be set as if you have no cull distance volumes affecting this primitive component the cached value wouldn't get updated
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, LDMaxDrawDistance) || PropertyName == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bAllowCullDistanceVolume))
		{
			CachedMaxDrawDistance = LDMaxDrawDistance;
		}

		// we need to reregister the primitive if the min draw distance changed to propagate the change to the rendering thread
		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, MinDrawDistance))
		{
			MarkRenderStateDirty();
		}
	}

	if (bLightAsIfStatic && GetStaticLightingType() == LMIT_None)
	{
		bLightAsIfStatic = false;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Make sure cached cull distance is up-to-date.
	if( LDMaxDrawDistance > 0 )
	{
		CachedMaxDrawDistance = FMath::Min( LDMaxDrawDistance, CachedMaxDrawDistance );
	}
	// Directly use LD cull distance if cull distance volumes are disabled.
	if( !bAllowCullDistanceVolume )
	{
		CachedMaxDrawDistance = LDMaxDrawDistance;
	}
	else if (UWorld* World = GetWorld())
	{
		World->UpdateCullDistanceVolumes(nullptr, this);
	}

	// Reattach to propagate cull distance change.
	if( CachedMaxDrawDistance != OldCachedMaxDrawDistance )
	{
		MarkRenderStateDirty();
	}

	// update component, ActorComponent's property update locks navigation system 
	// so it needs to be called directly here
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bCanEverAffectNavigation))
	{
		HandleCanEverAffectNavigationChange();
	}
}

bool UPrimitiveComponent::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange( InProperty );
	if (bIsEditable && InProperty)
	{
		const FName PropertyName = InProperty->GetFName();

		static FName LightAsIfStaticName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bLightAsIfStatic);
		static FName LightmassSettingsName = TEXT("LightmassSettings");
		static FName LightingChannelsName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, LightingChannels);
		static FName SingleSampleShadowFromStationaryLightsName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bSingleSampleShadowFromStationaryLights);
		static FName IndirectLightingCacheQualityName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, IndirectLightingCacheQuality);
		static FName CastCinematicShadowName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bCastCinematicShadow);
		static FName CastInsetShadowName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, bCastInsetShadow);
		static FName CastShadowName = GET_MEMBER_NAME_CHECKED(UPrimitiveComponent, CastShadow);

		if (PropertyName == LightAsIfStaticName)
		{
			// Disable editing bLightAsIfStatic on static components, since it has no effect
			return Mobility != EComponentMobility::Static;
		}

		if (PropertyName == LightmassSettingsName)
		{
			return Mobility != EComponentMobility::Movable || bLightAsIfStatic;
		}

		if (PropertyName == SingleSampleShadowFromStationaryLightsName)
		{
			return Mobility != EComponentMobility::Static;
		}

		if (PropertyName == CastCinematicShadowName)
		{
			return Mobility == EComponentMobility::Movable;
		}

		if (PropertyName == IndirectLightingCacheQualityName)
		{
			UWorld* World = GetWorld();
			AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : NULL;
			const bool bILCRelevant = WorldSettings ? (WorldSettings->LightmassSettings.VolumeLightingMethod == VLM_SparseVolumeLightingSamples) : true;
			return bILCRelevant && Mobility == EComponentMobility::Movable;
		}

		if (PropertyName == CastInsetShadowName)
		{
			return !bSelfShadowOnly;
		}

		if (PropertyName == CastShadowName)
		{
			// Look for any lit materials
			bool bHasAnyLitMaterials = false;
			const int32 NumMaterials = GetNumMaterials();
			for (int32 MaterialIndex = 0; (MaterialIndex < NumMaterials) && !bHasAnyLitMaterials; ++MaterialIndex)
			{
				UMaterialInterface* Material = GetMaterial(MaterialIndex);

				if (Material)
				{
					if (Material->GetShadingModel() != MSM_Unlit)
					{
						bHasAnyLitMaterials = true;
					}
				}
				else
				{
					// Default material is lit
					bHasAnyLitMaterials = true;
				}
			}

			// If there's at least one lit section it could cast shadows, so let the property be edited.
			// The 0 materials catch is in case any components aren't properly implementing the GetMaterial API, they might or might not work with shadows.
			return (NumMaterials == 0) || bHasAnyLitMaterials;
		}
	}

	return bIsEditable;
}

void UPrimitiveComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	const FName NAME_Scale3D(TEXT("Scale3D"));
	const FName NAME_Scale(TEXT("Scale"));
	const FName NAME_Translation(TEXT("Translation"));

	for( FEditPropertyChain::TIterator It(PropertyChangedEvent.PropertyChain.GetHead()); It; ++It )
	{
		if( It->GetFName() == NAME_Scale3D		||
			It->GetFName() == NAME_Scale		||
			It->GetFName() == NAME_Translation	||
			It->GetFName() == NAME_Rotation)
		{
			UpdateComponentToWorld();
			break;
		}
	}

	Super::PostEditChangeChainProperty( PropertyChangedEvent );
}

void UPrimitiveComponent::CheckForErrors()
{
	AActor* Owner = GetOwner();

	if (CastShadow && bCastDynamicShadow && BoundsScale > 1.0f)
	{
		FMessageLog("MapCheck").PerformanceWarning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_ShadowCasterUsingBoundsScale", "Actor casts dynamic shadows and has a BoundsScale greater than 1! This will have a large performance hit" )))
			->AddToken(FMapErrorToken::Create(FMapErrors::ShadowCasterUsingBoundsScale));
	}

	if (HasStaticLighting() && !HasValidSettingsForStaticLighting(true) && (!Owner || !Owner->IsA(AWorldSettings::StaticClass())))	// Ignore worldsettings
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_InvalidLightmapSettings", "Component is a static type but has invalid lightmap settings!  Indirect lighting will be black.  Common causes are lightmap resolution of 0, LightmapCoordinateIndex out of bounds." )))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticComponentHasInvalidLightmapSettings));
	}
}

void UPrimitiveComponent::UpdateCollisionProfile()
{
	BodyInstance.LoadProfileData(false);
}
#endif // WITH_EDITOR

void UPrimitiveComponent::ReceiveComponentDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bApplyImpulseOnDamage)
	{
		UDamageType const* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*)&DamageEvent;
			if ((DamageTypeCDO->DamageImpulse > 0.f) && !PointDamageEvent->ShotDirection.IsNearlyZero())
			{
				if (IsSimulatingPhysics(PointDamageEvent->HitInfo.BoneName))
				{
					FVector const ImpulseToApply = PointDamageEvent->ShotDirection.GetSafeNormal() * DamageTypeCDO->DamageImpulse;
					AddImpulseAtLocation(ImpulseToApply, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.BoneName);
				}
			}
		}
		else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			FRadialDamageEvent* const RadialDamageEvent = (FRadialDamageEvent*)&DamageEvent;
			if (DamageTypeCDO->DamageImpulse > 0.f)
			{
				AddRadialImpulse(RadialDamageEvent->Origin, RadialDamageEvent->Params.OuterRadius, DamageTypeCDO->DamageImpulse, RIF_Linear, DamageTypeCDO->bRadialDamageVelChange);
			}
		}
	}
}

void UPrimitiveComponent::PostLoad()
{
	Super::PostLoad();
	
	int32 const UE4Version = GetLinkerUE4Version();

	// as temporary fix for the bug TTP 299926
	// permanent fix is coming
	if (!IsTemplate())
	{
		BodyInstance.FixupData(this);
	}

	if (UE4Version < VER_UE4_RENAME_CANBECHARACTERBASE)
	{
		CanCharacterStepUpOn = CanBeCharacterBase_DEPRECATED;
	}

	// Make sure cached cull distance is up-to-date.
	if( LDMaxDrawDistance > 0.f )
	{
		// Directly use LD cull distance if cached one is not set.
		if( CachedMaxDrawDistance == 0.f )
		{
			CachedMaxDrawDistance = LDMaxDrawDistance;
		}
		// Use min of both if neither is 0. Need to check as 0 has special meaning.
		else
		{
			CachedMaxDrawDistance = FMath::Min( LDMaxDrawDistance, CachedMaxDrawDistance );
		}
	} 

	if (bLightAsIfStatic && GetStaticLightingType() == LMIT_None)
	{
		bLightAsIfStatic = false;
	}
}

void UPrimitiveComponent::PostDuplicate(bool bDuplicateForPIE)
{
	if (!bDuplicateForPIE)
	{
		VisibilityId = INDEX_NONE;
	}

	Super::PostDuplicate(bDuplicateForPIE);
}

#if WITH_EDITOR
/**
 * Called after importing property values for this object (paste, duplicate or .t3d import)
 * Allow the object to perform any cleanup for properties which shouldn't be duplicated or
 * are unsupported by the script serialization
 */
void UPrimitiveComponent::PostEditImport()
{
	Super::PostEditImport();

	VisibilityId = INDEX_NONE;

	// as temporary fix for the bug TTP 299926
	// permanent fix is coming
	if (IsTemplate()==false)
	{
		BodyInstance.FixupData(this);
	}
}
#endif

void UPrimitiveComponent::BeginDestroy()
{
	// Whether static or dynamic, all references need to be freed
	if (IsAttachedToStreamingManager())
	{
		IStreamingManager::Get().NotifyPrimitiveDetached(this);
	}

	Super::BeginDestroy();

	// Use a fence to keep track of when the rendering thread executes this scene detachment.
	DetachFence.BeginFence();
	AActor* Owner = GetOwner();
	if(Owner)
	{
		Owner->DetachFence.BeginFence();
	}
}

void UPrimitiveComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Prevent future overlap events. Any later calls to UpdateOverlaps will only allow this to end overlaps.
	bGenerateOverlapEvents = false;

	// End all current overlaps
	if (OverlappingComponents.Num() > 0)
	{
		const bool bDoNotifies = true;
		const bool bSkipNotifySelf = false;
		ClearComponentOverlaps(bDoNotifies, bSkipNotifySelf);
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UPrimitiveComponent::IsReadyForFinishDestroy()
{
	// Don't allow the primitive component to the purged until its pending scene detachments have completed.
	return Super::IsReadyForFinishDestroy() && DetachFence.IsFenceComplete();
}

void UPrimitiveComponent::FinishDestroy()
{
	// The detach fence has cleared so we better not be attached to the scene.
	check(AttachmentCounter.GetValue() == 0);
	Super::FinishDestroy();
}

bool UPrimitiveComponent::NeedsLoadForClient() const
{
	if (!IsVisible() && !IsCollisionEnabled() && !AlwaysLoadOnClient)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForClient();
	}
}


bool UPrimitiveComponent::NeedsLoadForServer() const
{
	if(!IsCollisionEnabled() && !AlwaysLoadOnServer)
	{
		return 0;
	}
	else
	{
		return Super::NeedsLoadForServer();
	}
}

void UPrimitiveComponent::SetOwnerNoSee(bool bNewOwnerNoSee)
{
	if(bOwnerNoSee != bNewOwnerNoSee)
	{
		bOwnerNoSee = bNewOwnerNoSee;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetOnlyOwnerSee(bool bNewOnlyOwnerSee)
{
	if(bOnlyOwnerSee != bNewOnlyOwnerSee)
	{
		bOnlyOwnerSee = bNewOnlyOwnerSee;
		MarkRenderStateDirty();
	}
}

bool UPrimitiveComponent::ShouldComponentAddToScene() const
{
	bool bSceneAdd = USceneComponent::ShouldComponentAddToScene();
	return bSceneAdd && (ShouldRender() || bCastHiddenShadow);
}

bool UPrimitiveComponent::ShouldCreatePhysicsState() const
{
	if (IsBeingDestroyed())
	{
		return false;
	}

	bool bShouldCreatePhysicsState = IsRegistered() && (bAlwaysCreatePhysicsState || BodyInstance.GetCollisionEnabled() != ECollisionEnabled::NoCollision);

#if WITH_EDITOR
	if (BodyInstance.bSimulatePhysics)
	{
		if(UWorld* World = GetWorld())
		{
			if(World->IsGameWorld())
			{
				const ECollisionEnabled::Type CollisionEnabled = GetCollisionEnabled();
				if (CollisionEnabled == ECollisionEnabled::NoCollision || CollisionEnabled == ECollisionEnabled::QueryOnly)
				{
					FMessageLog("PIE").Warning(FText::Format(LOCTEXT("InvalidSimulateOptions", "Invalid Simulate Options: Body ({0}) is set to simulate physics but Collision Enabled is incompatible"),
						FText::FromString(GetReadableName())));
				}
			}
		}
	}

	// if it shouldn't create physics state, but if world wants to enable trace collision for components, allow it
	if (!bShouldCreatePhysicsState)
	{
		UWorld* World = GetWorld();
		if (World && World->bEnableTraceCollision)
		{
			bShouldCreatePhysicsState = true;
		}
	}
#endif
	return bShouldCreatePhysicsState;
}

bool UPrimitiveComponent::HasValidPhysicsState() const
{
	return BodyInstance.IsValidBodyInstance();
}

bool UPrimitiveComponent::IsComponentIndividuallySelected() const
{
	bool bIndividuallySelected = false;
#if WITH_EDITOR
	if(SelectionOverrideDelegate.IsBound())
	{
		bIndividuallySelected = SelectionOverrideDelegate.Execute(this);
	}
#endif
	return bIndividuallySelected;
}

bool UPrimitiveComponent::ShouldRenderSelected() const
{
	if (bSelectable)
	{
		if (const AActor* Owner = GetOwner())
		{
			if (Owner->IsSelected())
			{
				return true;
			}
			else if (Owner->IsChildActor())
			{
				AActor* ParentActor = Owner->GetParentActor();
				while (ParentActor->IsChildActor())
				{
					ParentActor = ParentActor->GetParentActor();
				}
				return ParentActor->IsSelected();
			}
		}
	}
	return false;
}

void UPrimitiveComponent::SetCastShadow(bool NewCastShadow)
{
	if(NewCastShadow != CastShadow)
	{
		CastShadow = NewCastShadow;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetSingleSampleShadowFromStationaryLights(bool bNewSingleSampleShadowFromStationaryLights)
{
	if (bNewSingleSampleShadowFromStationaryLights != bSingleSampleShadowFromStationaryLights)
	{
		bSingleSampleShadowFromStationaryLights = bNewSingleSampleShadowFromStationaryLights;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetTranslucentSortPriority(int32 NewTranslucentSortPriority)
{
	if (NewTranslucentSortPriority != TranslucencySortPriority)
	{
		TranslucencySortPriority = NewTranslucentSortPriority;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetReceivesDecals(bool bNewReceivesDecals)
{
	if (bNewReceivesDecals != bReceivesDecals)
	{
		bReceivesDecals = bNewReceivesDecals;
		MarkRenderStateDirty();
	}
}


void UPrimitiveComponent::PushSelectionToProxy()
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetSelection_GameThread(ShouldRenderSelected(),IsComponentIndividuallySelected());
	}
}


void UPrimitiveComponent::PushEditorVisibilityToProxy( uint64 InVisibility )
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetHiddenEdViews_GameThread( InVisibility );
	}
}

#if WITH_EDITOR
uint64 UPrimitiveComponent::GetHiddenEditorViews() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->HiddenEditorViews : 0;
}
#endif// WITH_EDITOR

void UPrimitiveComponent::PushHoveredToProxy(const bool bInHovered)
{
	//although this should only be called for attached components, some billboard components can get in without valid proxies
	if (SceneProxy)
	{
		SceneProxy->SetHovered_GameThread(bInHovered);
	}
}

void UPrimitiveComponent::SetCullDistance(const float NewCullDistance)
{
	if (NewCullDistance >= 0.f && NewCullDistance != LDMaxDrawDistance)
	{
		const float OldLDMaxDrawDistance = LDMaxDrawDistance;

		LDMaxDrawDistance = NewCullDistance;
	
		if (CachedMaxDrawDistance == 0.f || LDMaxDrawDistance < CachedMaxDrawDistance)
		{
			SetCachedMaxDrawDistance(LDMaxDrawDistance);
		}
		else if (OldLDMaxDrawDistance == CachedMaxDrawDistance)
		{
			if (UWorld* World = GetWorld())
			{
				World->UpdateCullDistanceVolumes(nullptr, this);
			}
			else
			{
				SetCachedMaxDrawDistance(LDMaxDrawDistance);
			}
		}
	}
}

void UPrimitiveComponent::SetCachedMaxDrawDistance(const float NewCachedMaxDrawDistance)
{
	if( !FMath::IsNearlyEqual(CachedMaxDrawDistance, NewCachedMaxDrawDistance) )
	{
		CachedMaxDrawDistance = NewCachedMaxDrawDistance;
		MarkRenderStateDirty();
	}
}


void UPrimitiveComponent::SetDepthPriorityGroup(ESceneDepthPriorityGroup NewDepthPriorityGroup)
{
	if (DepthPriorityGroup != NewDepthPriorityGroup)
	{
		DepthPriorityGroup = NewDepthPriorityGroup;
		MarkRenderStateDirty();
	}
}



void UPrimitiveComponent::SetViewOwnerDepthPriorityGroup(
	bool bNewUseViewOwnerDepthPriorityGroup,
	ESceneDepthPriorityGroup NewViewOwnerDepthPriorityGroup
	)
{
	bUseViewOwnerDepthPriorityGroup = bNewUseViewOwnerDepthPriorityGroup;
	ViewOwnerDepthPriorityGroup = NewViewOwnerDepthPriorityGroup;
	MarkRenderStateDirty();
}

bool UPrimitiveComponent::IsWorldGeometry() const
{
	// if modify flag doesn't change, and 
	// it's saying it's movement is static, we considered to be world geom
	// @Todo collision: we could check if bEnableCollision==true here as well
	// but then if we disable collision, they just become non world geometry. 
	// not sure if that would be best way to do this yet
	return Mobility != EComponentMobility::Movable && GetCollisionObjectType()==ECC_WorldStatic;
}

ECollisionChannel UPrimitiveComponent::GetCollisionObjectType() const
{
	return ECollisionChannel(BodyInstance.GetObjectType());
}

void UPrimitiveComponent::SetBoundsScale(float NewBoundsScale)
{
	BoundsScale = NewBoundsScale;
	UpdateBounds();
	MarkRenderTransformDirty();
}

UMaterialInterface* UPrimitiveComponent::GetMaterial(int32 Index) const
{
	return NULL;
}

void UPrimitiveComponent::SetMaterial(int32 Index, UMaterialInterface* InMaterial)
{
}

void UPrimitiveComponent::SetMaterialByName(FName MaterialSlotName, class UMaterialInterface* Material)
{
}

int32 UPrimitiveComponent::GetNumMaterials() const
{
	return 0;
}

UMaterialInstanceDynamic* UPrimitiveComponent::CreateAndSetMaterialInstanceDynamic(int32 ElementIndex)
{
	UMaterialInterface* MaterialInstance = GetMaterial(ElementIndex);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInstance);

	if (MaterialInstance && !MID)
	{
		// Create and set the dynamic material instance.
		MID = UMaterialInstanceDynamic::Create(MaterialInstance, this);
		SetMaterial(ElementIndex, MID);
	}
	else if (!MaterialInstance)
	{
		UE_LOG(LogPrimitiveComponent, Warning, TEXT("CreateAndSetMaterialInstanceDynamic on %s: Material index %d is invalid."), *GetPathName(), ElementIndex);
	}

	return MID;
}

UMaterialInstanceDynamic* UPrimitiveComponent::CreateAndSetMaterialInstanceDynamicFromMaterial(int32 ElementIndex, class UMaterialInterface* Parent)
{
	if (Parent)
	{
		SetMaterial(ElementIndex, Parent);
		return CreateAndSetMaterialInstanceDynamic(ElementIndex);
	}

	return NULL;
}

UMaterialInstanceDynamic* UPrimitiveComponent::CreateDynamicMaterialInstance(int32 ElementIndex, class UMaterialInterface* SourceMaterial)
{
	if (SourceMaterial)
	{
		SetMaterial(ElementIndex, SourceMaterial);
	}

	UMaterialInterface* MaterialInstance = GetMaterial(ElementIndex);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInstance);

	if (MaterialInstance && !MID)
	{
		// Create and set the dynamic material instance.
		MID = UMaterialInstanceDynamic::Create(MaterialInstance, this);
		SetMaterial(ElementIndex, MID);
	}
	else if (!MaterialInstance)
	{
		UE_LOG(LogPrimitiveComponent, Warning, TEXT("CreateDynamicMaterialInstance on %s: Material index %d is invalid."), *GetPathName(), ElementIndex);
	}

	return MID;
}

UMaterialInterface* UPrimitiveComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
	//This function should be overriden
	SectionIndex = 0;
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// MOVECOMPONENT PROFILING CODE

// LOOKING_FOR_PERF_ISSUES
#define PERF_MOVECOMPONENT_STATS 0

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS
extern bool GShouldLogOutAFrameOfMoveComponent;

/** 
 *	Class to start/stop timer when it goes outside MoveComponent scope.
 *	We keep all results from different MoveComponent calls until we reach the top level, and then print them all out.
 *	That way we can show totals before breakdown, and not pollute timings with log time.
 */
class FScopedMoveCompTimer
{
	/** Struct to contain one temporary MoveActor timing, until we finish entire move and can print it out. */
	struct FMoveTimer
	{
		AActor* Actor;
		FVector Delta;
		double Time;
		int32 Depth;
		bool bDidLineCheck;
		bool bDidEncroachCheck;
	};

	/** Array of all moves within one 'outer' move. */
	static TArray<FMoveTimer> Moves;
	/** Current depth in movement hierarchy */
	static int32 Depth;

	/** Time that this scoped move started. */
	double	StartTime;
	/** Index into Moves array to put results of this MoveActor timing. */
	int32		MoveIndex;
public:

	/** If we did a line check during this MoveActor. */
	bool	bDidLineCheck;
	/** If we did an encroach check during this MoveActor. */
	bool	bDidEncroachCheck;

	FScopedMoveCompTimer(AActor* Actor, const FVector& Delta)
		: StartTime(0.0)
	    , MoveIndex(-1)
	    , bDidLineCheck(false)
		, bDidEncroachCheck(false)
	{
		if(GShouldLogOutAFrameOfMoveComponent)
		{
			// Add new entry to temp results array, and save actor and current stack depth
			MoveIndex = Moves.AddZeroed(1);
			Moves(MoveIndex).Actor = Actor;
			Moves(MoveIndex).Depth = Depth;
			Moves(MoveIndex).Delta = Delta;
			Depth++;

			StartTime = FPlatformTime::Seconds(); // Start timer.
		}
	}

	~FScopedMoveCompTimer()
	{
		if(GShouldLogOutAFrameOfMoveComponent)
		{
			// Record total time MoveActor took
			const double TakeTime = FPlatformTime::Seconds() - StartTime;

			check(Depth > 0);
			check(MoveIndex < Moves.Num());

			// Update entry with timing results
			Moves(MoveIndex).Time = TakeTime;
			Moves(MoveIndex).bDidLineCheck = bDidLineCheck;
			Moves(MoveIndex).bDidEncroachCheck = bDidEncroachCheck;

			Depth--;

			// Reached the top of the move stack again - output what we accumulated!
			if(Depth == 0)
			{
				for(int32 MoveIdx=0; MoveIdx<Moves.Num(); MoveIdx++)
				{
					const FMoveTimer& Move = Moves(MoveIdx);

					// Build indentation
					FString Indent;
					for(int32 i=0; i<Move.Depth; i++)
					{
						Indent += TEXT("  ");
					}

					UE_LOG(LogPrimitiveComponent, Log, TEXT("MOVE%s - %s %5.2fms (%f %f %f) %d %d %s"), *Indent, *Move.Actor->GetName(), Move.Time * 1000.f, Move.Delta.X, Move.Delta.Y, Move.Delta.Z, Move.bDidLineCheck, Move.bDidEncroachCheck, *Move.Actor->GetDetailedInfo());
				}

				// Clear moves array
				Moves.Reset();
			}
		}
	}

};

// Init statics
TArray<FScopedMoveCompTimer::FMoveTimer>	FScopedMoveCompTimer::Moves;
int32											FScopedMoveCompTimer::Depth = 0;

#endif //  !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS


static void PullBackHit(FHitResult& Hit, const FVector& Start, const FVector& End, const float Dist)
{
	const float DesiredTimeBack = FMath::Clamp(0.1f, 0.1f/Dist, 1.f/Dist) + 0.001f;
	Hit.Time = FMath::Clamp( Hit.Time - DesiredTimeBack, 0.f, 1.f );
}

/**
 * PERF_ISSUE_FINDER
 *
 * MoveComponent should not take a long time to execute.  If it is then then there is probably something wrong.
 *
 * Turn this on to have the engine log out when a specific actor is taking longer than 
 * PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME_AMOUNT to move.  This is a great way to catch cases where
 * collision has been enabled but it should not have been.  Or if a specific actor is doing something evil
 *
 **/
//#define PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME 1
const static float PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME_AMOUNT = 2.0f; // modify this value to look at larger or smaller sets of "bad" actors


static bool ShouldIgnoreHitResult(const UWorld* InWorld, FHitResult const& TestHit, FVector const& MovementDirDenormalized, const AActor* MovingActor, EMoveComponentFlags MoveFlags)
{
	if (TestHit.bBlockingHit)
	{
		// check "ignore bases" functionality
		if ( (MoveFlags & MOVECOMP_IgnoreBases) && MovingActor )	//we let overlap components go through because their overlap is still needed and will cause beginOverlap/endOverlap events
		{
			// ignore if there's a base relationship between moving actor and hit actor
			AActor const* const HitActor = TestHit.GetActor();
			if (HitActor)
			{
				if (MovingActor->IsBasedOnActor(HitActor) || HitActor->IsBasedOnActor(MovingActor))
				{
					return true;
				}
			}
		}
	
		// If we started penetrating, we may want to ignore it if we are moving out of penetration.
		// This helps prevent getting stuck in walls.
		if ( TestHit.bStartPenetrating && !(MoveFlags & MOVECOMP_NeverIgnoreBlockingOverlaps) )
		{
 			const float DotTolerance = InitialOverlapToleranceCVar;

			// Dot product of movement direction against 'exit' direction
			const FVector MovementDir = MovementDirDenormalized.GetSafeNormal();
			const float MoveDot = (TestHit.ImpactNormal | MovementDir);

			const bool bMovingOut = MoveDot > DotTolerance;

	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			{
				if (CVarShowInitialOverlaps != 0)
				{
					UE_LOG(LogTemp, Log, TEXT("Overlapping %s Dir %s Dot %f Normal %s Depth %f"), *GetNameSafe(TestHit.Component.Get()), *MovementDir.ToString(), MoveDot, *TestHit.ImpactNormal.ToString(), TestHit.PenetrationDepth);
					DrawDebugDirectionalArrow(InWorld, TestHit.TraceStart, TestHit.TraceStart + 30.f * TestHit.ImpactNormal, 5.f, bMovingOut ? FColor(64,128,255) : FColor(255,64,64), true, 4.f);
					if (TestHit.PenetrationDepth > KINDA_SMALL_NUMBER)
					{
						DrawDebugDirectionalArrow(InWorld, TestHit.TraceStart, TestHit.TraceStart + TestHit.PenetrationDepth * TestHit.Normal, 5.f, FColor(64,255,64), true, 4.f);
					}
				}
			}
	#endif

			// If we are moving out, ignore this result!
			if (bMovingOut)
			{
				return true;
			}
		}
	}

	return false;
}


// Returns true if we should check the bGenerateOverlapEvents flag when gathering overlaps, otherwise we'll always just do it.
static bool ShouldCheckOverlapFlagToQueueOverlaps(const UPrimitiveComponent& ThisComponent)
{
	const FScopedMovementUpdate* CurrentUpdate = ThisComponent.GetCurrentScopedMovement();
	if (CurrentUpdate)
	{
		return CurrentUpdate->RequiresOverlapsEventFlag();
	}
	// By default we require the bGenerateOverlapEvents to queue up overlaps, since we require it to trigger events.
	return true;
}


static bool ShouldIgnoreOverlapResult(const UWorld* World, const AActor* ThisActor, const UPrimitiveComponent& ThisComponent, const AActor* OtherActor, const UPrimitiveComponent& OtherComponent, bool bCheckOverlapFlags)
{
	// Don't overlap with self
	if (&ThisComponent == &OtherComponent)
	{
		return true;
	}

	if (bCheckOverlapFlags)
	{
		// Both components must set bGenerateOverlapEvents
		if (!ThisComponent.bGenerateOverlapEvents || !OtherComponent.bGenerateOverlapEvents)
		{
			return true;
		}
	}

	if (!ThisActor || !OtherActor)
	{
		return true;
	}

	if (!World || OtherActor == World->GetWorldSettings() || !OtherActor->IsActorInitialized())
	{
		return true;
	}

	return false;
}


void UPrimitiveComponent::InitSweepCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const
{
	OutResponseParam.CollisionResponse = BodyInstance.GetResponseToChannels();
	OutParams.AddIgnoredActors(MoveIgnoreActors);
	OutParams.AddIgnoredComponents(MoveIgnoreComponents);
	OutParams.bTraceAsyncScene = bCheckAsyncSceneOnMove;
	OutParams.bTraceComplex = bTraceComplexOnMove;
	OutParams.bReturnPhysicalMaterial = bReturnMaterialOnMove;
	OutParams.IgnoreMask = GetMoveIgnoreMask();
}

void UPrimitiveComponent::SetMoveIgnoreMask(FMaskFilter InMoveIgnoreMask)
{
	if (ensure(InMoveIgnoreMask < (1 << NumExtraFilterBits))) // We only have a limited nubmer of bits for the mask. TODO: don't assert, and make this a nicer exposed value.
	{
		MoveIgnoreMask = InMoveIgnoreMask;
	}
}

FCollisionShape UPrimitiveComponent::GetCollisionShape(float Inflation) const
{
	// This is intended to be overridden by shape classes, so this is a simple, large bounding shape.
	FVector Extent = Bounds.BoxExtent + Inflation;
	if (Inflation < 0.f)
	{
		// Don't shrink below zero size.
		Extent = Extent.ComponentMax(FVector::ZeroVector);
	}

	return FCollisionShape::MakeBox(Extent);
}


bool UPrimitiveComponent::MoveComponentImpl( const FVector& Delta, const FQuat& NewRotationQuat, bool bSweep, FHitResult* OutHit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveComponentTime);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS
	FScopedMoveCompTimer MoveTimer(this, Delta);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS

#if defined(PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME) || LOOKING_FOR_PERF_ISSUES
	uint32 MoveCompTakingLongTime=0;
	CLOCK_CYCLES(MoveCompTakingLongTime);
#endif

	// static things can move before they are registered (e.g. immediately after streaming), but not after.
	if (IsPendingKill() || CheckStaticMobilityAndWarn(PrimitiveComponentStatics::MobilityWarnText))
	{
		if (OutHit)
		{
			OutHit->Init();
		}
		return false;
	}

	ConditionalUpdateComponentToWorld();

	// Set up
	const FVector TraceStart = GetComponentLocation();
	const FVector TraceEnd = TraceStart + Delta;
	float DeltaSizeSq = (TraceEnd - TraceStart).SizeSquared();				// Recalc here to account for precision loss of float addition
	const FQuat InitialRotationQuat = GetComponentTransform().GetRotation();

	// ComponentSweepMulti does nothing if moving < KINDA_SMALL_NUMBER in distance, so it's important to not try to sweep distances smaller than that. 
	const float MinMovementDistSq = (bSweep ? FMath::Square(4.f*KINDA_SMALL_NUMBER) : 0.f);
	if (DeltaSizeSq <= MinMovementDistSq)
	{
		// Skip if no vector or rotation.
		if (NewRotationQuat.Equals(InitialRotationQuat, SCENECOMPONENT_QUAT_TOLERANCE))
		{
			// copy to optional output param
			if (OutHit)
			{
				OutHit->Init(TraceStart, TraceEnd);
			}
			return true;
		}
		DeltaSizeSq = 0.f;
	}

	const bool bSkipPhysicsMove = ((MoveFlags & MOVECOMP_SkipPhysicsMove) != MOVECOMP_NoFlags);

	// WARNING: HitResult is only partially initialized in some paths. All data is valid only if bFilledHitResult is true.
	FHitResult BlockingHit(NoInit);
	BlockingHit.bBlockingHit = false;
	BlockingHit.Time = 1.f;
	bool bFilledHitResult = false;
	bool bMoved = false;
	bool bIncludesOverlapsAtEnd = false;
	bool bRotationOnly = false;
	TArray<FOverlapInfo> PendingOverlaps;
	AActor* const Actor = GetOwner();

	if ( !bSweep )
	{
		// not sweeping, just go directly to the new transform
		bMoved = InternalSetWorldLocationAndRotation(TraceEnd, NewRotationQuat, bSkipPhysicsMove, Teleport);
		bRotationOnly = (DeltaSizeSq == 0);
		bIncludesOverlapsAtEnd = bRotationOnly && (AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, GetComponentScale())) && IsQueryCollisionEnabled();
	}
	else
	{
		TArray<FHitResult> Hits;
		FVector NewLocation = TraceStart;

		// Perform movement collision checking if needed for this actor.
		const bool bCollisionEnabled = IsQueryCollisionEnabled();
		if( bCollisionEnabled && (DeltaSizeSq > 0.f))
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if( !IsRegistered() )
			{
				if (Actor)
				{
					ensureMsgf(IsRegistered(), TEXT("%s MovedComponent %s not initialized deleteme %d"),*Actor->GetName(), *GetName(), Actor->IsPendingKill());
				}
				else
				{ //-V523
					ensureMsgf(IsRegistered(), TEXT("MovedComponent %s not initialized"), *GetFullName());
				}
			}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS
			MoveTimer.bDidLineCheck = true;
#endif 
			UWorld* const MyWorld = GetWorld();

			const bool bForceGatherOverlaps = !ShouldCheckOverlapFlagToQueueOverlaps(*this);
			FComponentQueryParams Params(SCENE_QUERY_STAT(MoveComponent), Actor);
			FCollisionResponseParams ResponseParam;
			InitSweepCollisionParams(Params, ResponseParam);
			Params.bIgnoreTouches |= !(bGenerateOverlapEvents || bForceGatherOverlaps);
			bool const bHadBlockingHit = MyWorld->ComponentSweepMulti(Hits, this, TraceStart, TraceEnd, InitialRotationQuat, Params);

			if (Hits.Num() > 0)
			{
				const float DeltaSize = FMath::Sqrt(DeltaSizeSq);
				for(int32 HitIdx=0; HitIdx<Hits.Num(); HitIdx++)
				{
					PullBackHit(Hits[HitIdx], TraceStart, TraceEnd, DeltaSize);
				}
			}

			// If we had a valid blocking hit, store it.
			// If we are looking for overlaps, store those as well.
			int32 FirstNonInitialOverlapIdx = INDEX_NONE;
			if (bHadBlockingHit || (bGenerateOverlapEvents || bForceGatherOverlaps))
			{
				int32 BlockingHitIndex = INDEX_NONE;
				float BlockingHitNormalDotDelta = BIG_NUMBER;
				for( int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++ )
				{
					const FHitResult& TestHit = Hits[HitIdx];

					if (TestHit.bBlockingHit)
					{
						if (!ShouldIgnoreHitResult(MyWorld, TestHit, Delta, Actor, MoveFlags))
						{
							if (TestHit.Time == 0.f)
							{
								// We may have multiple initial hits, and want to choose the one with the normal most opposed to our movement.
								const float NormalDotDelta = (TestHit.ImpactNormal | Delta);
								if (NormalDotDelta < BlockingHitNormalDotDelta)
								{
									BlockingHitNormalDotDelta = NormalDotDelta;
									BlockingHitIndex = HitIdx;
								}
							}
							else if (BlockingHitIndex == INDEX_NONE)
							{
								// First non-overlapping blocking hit should be used, if an overlapping hit was not.
								// This should be the only non-overlapping blocking hit, and last in the results.
								BlockingHitIndex = HitIdx;
								break;
							}
						}
					}
					else if (bGenerateOverlapEvents || bForceGatherOverlaps)
					{
						UPrimitiveComponent* OverlapComponent = TestHit.Component.Get();
						if (OverlapComponent && (OverlapComponent->bGenerateOverlapEvents || bForceGatherOverlaps))
						{
							if (!ShouldIgnoreOverlapResult(MyWorld, Actor, *this, TestHit.GetActor(), *OverlapComponent, /*bCheckOverlapFlags=*/ !bForceGatherOverlaps))
							{
								// don't process touch events after initial blocking hits
								if (BlockingHitIndex >= 0 && TestHit.Time > Hits[BlockingHitIndex].Time)
								{
									break;
								}

								if (FirstNonInitialOverlapIdx == INDEX_NONE && TestHit.Time > 0.f)
								{
									// We are about to add the first non-initial overlap.
									FirstNonInitialOverlapIdx = PendingOverlaps.Num();
								}

								// cache touches
								PendingOverlaps.AddUnique(FOverlapInfo(TestHit));
							}
						}
					}
				}

				// Update blocking hit, if there was a valid one.
				if (BlockingHitIndex >= 0)
				{
					BlockingHit = Hits[BlockingHitIndex];
					bFilledHitResult = true;
				}
			}
		
			// Update NewLocation based on the hit result
			if (!BlockingHit.bBlockingHit)
			{
				NewLocation = TraceEnd;
			}
			else
			{
				check(bFilledHitResult);
				NewLocation = TraceStart + (BlockingHit.Time * (TraceEnd - TraceStart));

				// Sanity check
				const FVector ToNewLocation = (NewLocation - TraceStart);
				if (ToNewLocation.SizeSquared() <= MinMovementDistSq)
				{
					// We don't want really small movements to put us on or inside a surface.
					NewLocation = TraceStart;
					BlockingHit.Time = 0.f;

					// Remove any pending overlaps after this point, we are not going as far as we swept.
					if (FirstNonInitialOverlapIdx != INDEX_NONE)
					{
						const bool bAllowShrinking = false;
						PendingOverlaps.SetNum(FirstNonInitialOverlapIdx, bAllowShrinking);
					}
				}
			}

			bIncludesOverlapsAtEnd = AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, GetComponentScale());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (UCheatManager::IsDebugCapsuleSweepPawnEnabled() && BlockingHit.bBlockingHit && !IsZeroExtent())
			{
				// this is sole debug purpose to find how capsule trace information was when hit 
				// to resolve stuck or improve our movement system - To turn this on, use DebugCapsuleSweepPawn
				APawn const* const ActorPawn = (Actor ? Cast<APawn>(Actor) : NULL);
				if (ActorPawn && ActorPawn->Controller && ActorPawn->Controller->IsLocalPlayerController())
				{
					APlayerController const* const PC = CastChecked<APlayerController>(ActorPawn->Controller);
					if (PC->CheatManager)
					{
						FVector CylExtent = ActorPawn->GetSimpleCollisionCylinderExtent()*FVector(1.001f,1.001f,1.0f);							
						FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CylExtent);
						PC->CheatManager->AddCapsuleSweepDebugInfo(TraceStart, TraceEnd, BlockingHit.ImpactPoint, BlockingHit.Normal, BlockingHit.ImpactNormal, BlockingHit.Location, CapsuleShape.GetCapsuleHalfHeight(), CapsuleShape.GetCapsuleRadius(), true, (BlockingHit.bStartPenetrating && BlockingHit.bBlockingHit) ? true: false);
					}
				}
			}
#endif
		}
		else if (DeltaSizeSq > 0.f)
		{
			// apply move delta even if components has collisions disabled
			NewLocation += Delta;
			bIncludesOverlapsAtEnd = false;
		}
		else if (DeltaSizeSq == 0.f && bCollisionEnabled)
		{
			bIncludesOverlapsAtEnd = AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, GetComponentScale());
			bRotationOnly = true;
		}

		// Update the location.  This will teleport any child components as well (not sweep).
		bMoved = InternalSetWorldLocationAndRotation(NewLocation, NewRotationQuat, bSkipPhysicsMove, Teleport);
	}

	// Handle overlap notifications.
	if (bMoved)
	{
		if (IsDeferringMovementUpdates())
		{
			// Defer UpdateOverlaps until the scoped move ends.
			FScopedMovementUpdate* ScopedUpdate = GetCurrentScopedMovement();
			if (bRotationOnly && bIncludesOverlapsAtEnd)
			{
				ScopedUpdate->KeepCurrentOverlapsAfterRotation(bSweep);
			}
			else
			{
				ScopedUpdate->AppendOverlapsAfterMove(PendingOverlaps, bSweep, bIncludesOverlapsAtEnd);
			}
		}
		else
		{
			if (bIncludesOverlapsAtEnd)
			{
				TArray<FOverlapInfo> OverlapsAtEndLocation;
				const TArray<FOverlapInfo>* OverlapsAtEndLocationPtr = nullptr; // When non-null, used as optimization to avoid work in UpdateOverlaps.
				if (bRotationOnly)
				{
					OverlapsAtEndLocationPtr = ConvertRotationOverlapsToCurrentOverlaps(OverlapsAtEndLocation, GetOverlapInfos());
				}
				else
				{
					OverlapsAtEndLocationPtr = ConvertSweptOverlapsToCurrentOverlaps(OverlapsAtEndLocation, PendingOverlaps, 0, GetComponentLocation(), GetComponentQuat());
				}
				UpdateOverlaps(&PendingOverlaps, true, OverlapsAtEndLocationPtr);
			}
			else
			{
				UpdateOverlaps(&PendingOverlaps, true, nullptr);
			}
		}
	}

	// Handle blocking hit notifications. Avoid if pending kill (which could happen after overlaps).
	const bool bAllowHitDispatch = !BlockingHit.bStartPenetrating || !(MoveFlags & MOVECOMP_DisableBlockingOverlapDispatch);
	if (BlockingHit.bBlockingHit && bAllowHitDispatch && !IsPendingKill())
	{
		check(bFilledHitResult);
		if (IsDeferringMovementUpdates())
		{
			FScopedMovementUpdate* ScopedUpdate = GetCurrentScopedMovement();
			ScopedUpdate->AppendBlockingHitAfterMove(BlockingHit);
		}
		else
		{
			DispatchBlockingHit(*Actor, BlockingHit);
		}
	}

#if defined(PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME) || LOOKING_FOR_PERF_ISSUES
	UNCLOCK_CYCLES(MoveCompTakingLongTime);
	const float MSec = FPlatformTime::ToMilliseconds(MoveCompTakingLongTime);
	if( MSec > PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME_AMOUNT )
	{
		if (GetOwner())
		{
			UE_LOG(LogPrimitiveComponent, Log, TEXT("%10f executing MoveComponent for %s owned by %s"), MSec, *GetName(), *GetOwner()->GetFullName() );
		}
		else
		{
			UE_LOG(LogPrimitiveComponent, Log, TEXT("%10f executing MoveComponent for %s"), MSec, *GetFullName() );
		}
	}
#endif

	// copy to optional output param
	if (OutHit)
	{
		if (bFilledHitResult)
		{
			*OutHit = BlockingHit;
		}
		else
		{
			OutHit->Init(TraceStart, TraceEnd);
		}
	}

	// Return whether we moved at all.
	return bMoved;
}


void UPrimitiveComponent::DispatchBlockingHit(AActor& Owner, FHitResult const& BlockingHit)
{
	UPrimitiveComponent* const BlockingHitComponent = BlockingHit.Component.Get();
	if (BlockingHitComponent)
	{
		Owner.DispatchBlockingHit(this, BlockingHitComponent, true, BlockingHit);

		// Dispatch above could kill the component, so we need to check that.
		if (!BlockingHitComponent->IsPendingKill())
		{
			// BlockingHit.GetActor() could be marked for deletion in DispatchBlockingHit(), which would make the weak pointer return NULL.
			if (AActor* const BlockingHitActor = BlockingHit.GetActor())
			{
				BlockingHitActor->DispatchBlockingHit(BlockingHitComponent, this, false, BlockingHit);
			}
		}
	}
}

void UPrimitiveComponent::DispatchWakeEvents(int32 WakeEvent, FName BoneName)
{
	FBodyInstance* RootBI = GetBodyInstance(BoneName, false);
	if(RootBI)
	{
		if(RootBI->bGenerateWakeEvents)
		{
			if (WakeEvent == SleepEvent::SET_Wakeup)
			{
				OnComponentWake.Broadcast(this, BoneName);
			}else
			{
				OnComponentSleep.Broadcast(this, BoneName);
			}
		}
	}
	
	//now update children that are welded
	for(USceneComponent* SceneComp : GetAttachChildren())
	{
		if(UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(SceneComp))
		{
			if(FBodyInstance* BI = PrimComp->GetBodyInstance(BoneName, false))
			{
				if(BI->WeldParent == RootBI)
				{
					PrimComp->DispatchWakeEvents(WakeEvent, BoneName);	
				}
			}
		}
	}
}


bool UPrimitiveComponent::IsNavigationRelevant() const 
{ 
	if (!CanEverAffectNavigation())
	{
		return false;
	}

	if (HasCustomNavigableGeometry() >= EHasCustomNavigableGeometry::EvenIfNotCollidable)
	{
		return true;
	}

	const FCollisionResponseContainer& ResponseToChannels = GetCollisionResponseToChannels();
	return IsQueryCollisionEnabled() &&
		(ResponseToChannels.GetResponse(ECC_Pawn) == ECR_Block || ResponseToChannels.GetResponse(ECC_Vehicle) == ECR_Block);
}

FBox UPrimitiveComponent::GetNavigationBounds() const
{
	return Bounds.GetBox();
}

//////////////////////////////////////////////////////////////////////////
// COLLISION

extern float DebugLineLifetime;


bool UPrimitiveComponent::LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params)
{
	bool bHaveHit = BodyInstance.LineTrace(OutHit, Start, End, Params.bTraceComplex, Params.bReturnPhysicalMaterial); 

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetWorld()->DebugDrawSceneQueries(Params.TraceTag))
	{
		TArray<FHitResult> Hits;
		if (bHaveHit)
		{
			Hits.Add(OutHit);
		}

		DrawLineTraces(GetWorld(), Start, End, Hits, DebugLineLifetime);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return bHaveHit;
}

bool UPrimitiveComponent::SweepComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FCollisionShape &CollisionShape, bool bTraceComplex)
{
	return BodyInstance.Sweep(OutHit, Start, End, ShapeWorldRotation, CollisionShape, bTraceComplex);
}

bool UPrimitiveComponent::ComponentOverlapComponentImpl(class UPrimitiveComponent* PrimComp, const FVector Pos, const FQuat& Quat, const struct FCollisionQueryParams& Params)
{
	// if target is skeletalmeshcomponent and do not support singlebody physics
	USkeletalMeshComponent * OtherComp = Cast<USkeletalMeshComponent>(PrimComp);
	if (OtherComp)
	{
		UE_LOG(LogCollision, Warning, TEXT("ComponentOverlapMulti : (%s) Does not support skeletalmesh with Physics Asset"), *PrimComp->GetPathName());
		return false;
	}

	if(FBodyInstance* BI = PrimComp->GetBodyInstance())
	{
		return BI->OverlapTestForBody(Pos, Quat, GetBodyInstance());
	}

	return false;
}


bool UPrimitiveComponent::OverlapComponent(const FVector& Pos, const FQuat& Rot, const struct FCollisionShape& CollisionShape)
{
	return BodyInstance.OverlapTest(Pos, Rot, CollisionShape);
}

bool UPrimitiveComponent::ComputePenetration(FMTDResult& OutMTD, const FCollisionShape & CollisionShape, const FVector& Pos, const FQuat& Rot)
{
	if(FBodyInstance* ComponentBodyInstance = GetBodyInstance())
	{
		return ComponentBodyInstance->OverlapTest(Pos, Rot, CollisionShape, &OutMTD);
	}

	return false;
}

bool UPrimitiveComponent::IsOverlappingComponent(const UPrimitiveComponent* OtherComp) const
{
	for (int32 i=0; i < OverlappingComponents.Num(); ++i)
	{
		if (OverlappingComponents[i].OverlapInfo.Component.Get() == OtherComp)
		{
			return true;
		}
	}
	return false;
}

bool UPrimitiveComponent::IsOverlappingComponent(const FOverlapInfo& Overlap) const
{
	return OverlappingComponents.Find(Overlap) != INDEX_NONE;
}

bool UPrimitiveComponent::IsOverlappingActor(const AActor* Other) const
{
	if (Other)
	{
		for (int32 OverlapIdx=0; OverlapIdx<OverlappingComponents.Num(); ++OverlapIdx)
		{
			UPrimitiveComponent const* const PrimComp = OverlappingComponents[OverlapIdx].OverlapInfo.Component.Get();
			if ( PrimComp && (PrimComp->GetOwner() == Other) )
			{
				return true;
			}
		}
	}

	return false;
}

bool UPrimitiveComponent::GetOverlapsWithActor(const AActor* Actor, TArray<FOverlapInfo>& OutOverlaps) const
{
	const int32 InitialCount = OutOverlaps.Num();
	if (Actor)
	{
		for (int32 OverlapIdx=0; OverlapIdx<OverlappingComponents.Num(); ++OverlapIdx)
		{
			UPrimitiveComponent const* const PrimComp = OverlappingComponents[OverlapIdx].OverlapInfo.Component.Get();
			if ( PrimComp && (PrimComp->GetOwner() == Actor) )
			{
				OutOverlaps.Add(OverlappingComponents[OverlapIdx]);
			}
		}
	}

	return InitialCount != OutOverlaps.Num();
}

#if WITH_EDITOR
bool UPrimitiveComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP)
	{
		const FBox& ComponentBounds = Bounds.GetBox();

		// Check the component bounds versus the selection box
		// If the selection box must encompass the entire component, then both the min and max vector of the bounds must be inside in the selection
		// box to be valid. If the selection box only has to touch the component, then it is sufficient to check if it intersects with the bounds.
		if ((!bMustEncompassEntireComponent && InSelBBox.Intersect(ComponentBounds))
			|| (bMustEncompassEntireComponent && InSelBBox.IsInside(ComponentBounds)))
		{
			return true;
		}
	}

	return false;
}

bool UPrimitiveComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP)
	{
		bool bIsFullyContained;
		if (InFrustum.IntersectBox(Bounds.Origin, Bounds.BoxExtent, bIsFullyContained))
		{
			return !bMustEncompassEntireComponent || bIsFullyContained;
		}
	}

	return false;
}
#endif



/** Used to determine if it is ok to call a notification on this object */
extern bool IsActorValidToNotify(AActor* Actor);

// @fixme, duplicated, make an inline member?
bool IsPrimCompValidAndAlive(UPrimitiveComponent* PrimComp)
{
	return (PrimComp != NULL) && !PrimComp->IsPendingKill();
}

// @todo, don't need to pass in Other actor?
void UPrimitiveComponent::BeginComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies)
{
	// If pending kill, we should not generate any new overlaps
	if (IsPendingKill())
	{
		return;
	}

	//	UE_LOG(LogActor, Log, TEXT("BEGIN OVERLAP! Self=%s SelfComp=%s, Other=%s, OtherComp=%s"), *GetNameSafe(this), *GetNameSafe(MyComp), *GetNameSafe(OtherComp->GetOwner()), *GetNameSafe(OtherComp));

	UPrimitiveComponent* OtherComp = OtherOverlap.OverlapInfo.Component.Get();

	if (OtherComp)
	{
		bool const bComponentsAlreadyTouching = IsOverlappingComponent(OtherOverlap);
		if (!bComponentsAlreadyTouching && CanComponentsGenerateOverlap(this, OtherComp))
		{
			AActor* const OtherActor = OtherComp->GetOwner();
			AActor* const MyActor = GetOwner();

			const bool bNotifyActorTouch = bDoNotifies && !MyActor->IsOverlappingActor(OtherActor);

			// Perform reflexive touch.
			OverlappingComponents.Add(OtherOverlap);										// already verified uniqueness above
			OtherComp->OverlappingComponents.AddUnique(FOverlapInfo(this, INDEX_NONE));		// uniqueness unverified, so addunique
			
			if (bDoNotifies)
			{
				// first execute component delegates
				if (!IsPendingKill())
				{
					OnComponentBeginOverlap.Broadcast(this, OtherActor, OtherComp, OtherOverlap.GetBodyIndex(), OtherOverlap.bFromSweep, OtherOverlap.OverlapInfo);
				}

				if (!OtherComp->IsPendingKill())
				{
					// Reverse normals for other component. When it's a sweep, we are the one that moved.
					OtherComp->OnComponentBeginOverlap.Broadcast(OtherComp, MyActor, this, INDEX_NONE, OtherOverlap.bFromSweep, OtherOverlap.bFromSweep ? FHitResult::GetReversedHit(OtherOverlap.OverlapInfo) : OtherOverlap.OverlapInfo);
				}

				// then execute actor notification if this is a new actor touch
				if (bNotifyActorTouch)
				{
					// First actor virtuals
					if (IsActorValidToNotify(MyActor))
					{
						MyActor->NotifyActorBeginOverlap(OtherActor);
					}

					if (IsActorValidToNotify(OtherActor))
					{
						OtherActor->NotifyActorBeginOverlap(MyActor);
					}

					// Then level-script delegates
					if (IsActorValidToNotify(MyActor))
					{
						MyActor->OnActorBeginOverlap.Broadcast(MyActor, OtherActor);
					}

					if (IsActorValidToNotify(OtherActor))
					{
						OtherActor->OnActorBeginOverlap.Broadcast(OtherActor, MyActor);
					}
				}
			}
		}
	}
}


void UPrimitiveComponent::EndComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies, bool bSkipNotifySelf)
{
	UPrimitiveComponent* OtherComp = OtherOverlap.OverlapInfo.Component.Get();
	if (OtherComp == nullptr)
	{
		return;
	}

	//	UE_LOG(LogActor, Log, TEXT("END OVERLAP! Self=%s SelfComp=%s, Other=%s, OtherComp=%s"), *GetNameSafe(this), *GetNameSafe(MyComp), *GetNameSafe(OtherActor), *GetNameSafe(OtherComp));

	const int32 OtherOverlapIdx = OtherComp->OverlappingComponents.Find(FOverlapInfo(this, INDEX_NONE));
	if (OtherOverlapIdx != INDEX_NONE)
	{
		OtherComp->OverlappingComponents.RemoveAtSwap(OtherOverlapIdx, 1, false);
	}

	const int32 OverlapIdx = OverlappingComponents.Find(OtherOverlap);
	if (OverlapIdx != INDEX_NONE)
	{
		OverlappingComponents.RemoveAtSwap(OverlapIdx, 1, false);

		if (bDoNotifies)
		{
			AActor* const OtherActor = OtherComp->GetOwner();
			AActor* const MyActor = GetOwner();
			if (OtherActor)
			{
				if (!bSkipNotifySelf && IsPrimCompValidAndAlive(this))
				{
					OnComponentEndOverlap.Broadcast(this, OtherActor, OtherComp, OtherOverlap.GetBodyIndex());
				}

				if (IsPrimCompValidAndAlive(OtherComp))
				{
					OtherComp->OnComponentEndOverlap.Broadcast(OtherComp, MyActor, this, INDEX_NONE);
				}
	
				// if this was the last touch on the other actor by this actor, notify that we've untouched the actor as well
				if (MyActor && !MyActor->IsOverlappingActor(OtherActor) )
				{			
					if (IsActorValidToNotify(MyActor))
					{
						MyActor->NotifyActorEndOverlap(OtherActor);
						MyActor->OnActorEndOverlap.Broadcast(MyActor, OtherActor);
					}

					if (IsActorValidToNotify(OtherActor))
					{
						OtherActor->NotifyActorEndOverlap(MyActor);
						OtherActor->OnActorEndOverlap.Broadcast(OtherActor, MyActor);
					}
				}
			}
		}
	}
}

void UPrimitiveComponent::GetOverlappingActors(TArray<AActor*>& OutOverlappingActors, TSubclassOf<AActor> ClassFilter) const
{
	TSet<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, ClassFilter);

	OutOverlappingActors.Reset(OverlappingActors.Num());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		OutOverlappingActors.Add(OverlappingActor);
	}

}

void UPrimitiveComponent::GetOverlappingActors(TSet<AActor*>& OutOverlappingActors, TSubclassOf<AActor> ClassFilter) const
{
	OutOverlappingActors.Reset();
	OutOverlappingActors.Reserve(OverlappingComponents.Num());

	for (const FOverlapInfo& OtherOverlap : OverlappingComponents)
	{
		if (UPrimitiveComponent* OtherComponent = OtherOverlap.OverlapInfo.Component.Get())
		{
			AActor* OtherActor = OtherComponent->GetOwner();
			if (OtherActor)
			{
				if ( (*ClassFilter) == nullptr || OtherActor->IsA(ClassFilter) )
				{
					OutOverlappingActors.Add(OtherActor);
				}
			}
		}
	}
}

void UPrimitiveComponent::GetOverlappingComponents(TArray<UPrimitiveComponent*>& OutOverlappingComponents) const
{
	OutOverlappingComponents.Reset( OverlappingComponents.Num() );

	for (const FOverlapInfo& OtherOverlap : OverlappingComponents)
	{
		UPrimitiveComponent* const OtherComp = OtherOverlap.OverlapInfo.Component.Get();
		if (OtherComp)
		{
			OutOverlappingComponents.Add(OtherComp);
		}
	}
}

const TArray<FOverlapInfo>* UPrimitiveComponent::ConvertSweptOverlapsToCurrentOverlaps(
	TArray<FOverlapInfo>& OverlapsAtEndLocation, const TArray<FOverlapInfo>& SweptOverlaps, int32 SweptOverlapsIndex,
	const FVector& EndLocation, const FQuat& EndRotationQuat)
{
	checkSlow(SweptOverlapsIndex >= 0);

	const TArray<FOverlapInfo>* Result = nullptr;
	const bool bForceGatherOverlaps = !ShouldCheckOverlapFlagToQueueOverlaps(*this);
	if ((bGenerateOverlapEvents || bForceGatherOverlaps) && bAllowCachedOverlapsCVar)
	{
		const AActor* Actor = GetOwner();
		if (Actor && Actor->GetRootComponent() == this)
		{
			// We know we are not overlapping any new components at the end location. Children are ignored here (see note below).
			if (bEnableFastOverlapCheck)
			{
				SCOPE_CYCLE_COUNTER(STAT_MoveComponent_FastOverlap);

				// Check components we hit during the sweep, keep only those still overlapping
				const FCollisionQueryParams UnusedQueryParams(NAME_None, FCollisionQueryParams::GetUnknownStatId());
				for (int32 Index = SweptOverlapsIndex; Index < SweptOverlaps.Num(); ++Index)
				{
					const FOverlapInfo& OtherOverlap = SweptOverlaps[Index];
					UPrimitiveComponent* OtherPrimitive = OtherOverlap.OverlapInfo.GetComponent();
					if (OtherPrimitive && (OtherPrimitive->bGenerateOverlapEvents || bForceGatherOverlaps))
					{
						if (OtherPrimitive->bMultiBodyOverlap)
						{
							// Not handled yet. We could do it by checking every body explicitly and track each body index in the overlap test, but this seems like a rare need.
							return nullptr;
						}
						else if (Cast<USkeletalMeshComponent>(OtherPrimitive) || Cast<USkeletalMeshComponent>(this))
						{
							// SkeletalMeshComponent does not support this operation, and would return false in the test when an actual query could return true.
							return nullptr;
						}
						else if (OtherPrimitive->ComponentOverlapComponent(this, EndLocation, EndRotationQuat, UnusedQueryParams))
						{
							OverlapsAtEndLocation.Add(OtherOverlap);
						}
					}
				}

				// Note: we don't worry about adding any child components here, because they are not included in the sweep results.
				// Children test for their own overlaps after we update our own, and we ignore children in our own update.
				checkfSlow(OverlapsAtEndLocation.FindByPredicate(FPredicateOverlapHasSameActor(*Actor)) == nullptr,
					TEXT("Child overlaps should not be included in the SweptOverlaps() array in UPrimitiveComponent::ConvertSweptOverlapsToCurrentOverlaps()."));

				Result = &OverlapsAtEndLocation;
			}
			else
			{
				if (SweptOverlaps.Num() == 0 && AreAllCollideableDescendantsRelative())
				{
					// Add overlaps with components in this actor.
					GetOverlapsWithActor(Actor, OverlapsAtEndLocation);
					Result = &OverlapsAtEndLocation;
				}
			}
		}
	}

	return Result;
}


const TArray<FOverlapInfo>* UPrimitiveComponent::ConvertRotationOverlapsToCurrentOverlaps(TArray<FOverlapInfo>& OverlapsAtEndLocation, const TArray<FOverlapInfo>& CurrentOverlaps)
{
	const TArray<FOverlapInfo>* Result = nullptr;
	const bool bForceGatherOverlaps = !ShouldCheckOverlapFlagToQueueOverlaps(*this);
	if ((bGenerateOverlapEvents || bForceGatherOverlaps) && bAllowCachedOverlapsCVar)
	{
		const AActor* Actor = GetOwner();
		if (Actor && Actor->GetRootComponent() == this)
		{
			if (bEnableFastOverlapCheck)
			{
				// Add all current overlaps that are not children. Children test for their own overlaps after we update our own, and we ignore children in our own update.
				OverlapsAtEndLocation = CurrentOverlaps.FilterByPredicate(FPredicateOverlapHasDifferentActor(*Actor));
				Result = &OverlapsAtEndLocation;
			}
		}
	}

	return Result;
}


bool UPrimitiveComponent::AreAllCollideableDescendantsRelative(bool bAllowCachedValue) const
{
	UPrimitiveComponent* MutableThis = const_cast<UPrimitiveComponent*>(this);
	if (GetAttachChildren().Num() > 0)
	{
		UWorld* MyWorld = GetWorld();
		check(MyWorld);

		// Throttle this test when it has been false in the past, since it rarely changes afterwards.
		if (bAllowCachedValue && !bCachedAllCollideableDescendantsRelative && MyWorld->TimeSince(LastCheckedAllCollideableDescendantsTime) < 1.f)
		{
			return false;
		}

		// Check all descendant PrimitiveComponents
		TInlineComponentArray<USceneComponent*> ComponentStack;
		const bool bForceGatherOverlaps = !ShouldCheckOverlapFlagToQueueOverlaps(*this);

		ComponentStack.Append(GetAttachChildren());
		while (ComponentStack.Num() > 0)
		{
			USceneComponent* const CurrentComp = ComponentStack.Pop(false);
			if (CurrentComp)
			{
				// Is the component not using relative position?
				if (CurrentComp->bAbsoluteLocation || CurrentComp->bAbsoluteRotation)
				{
					// Can we possibly collide with the component?
					UPrimitiveComponent* const CurrentPrimitive = Cast<UPrimitiveComponent>(CurrentComp);
					if (CurrentPrimitive && (CurrentPrimitive->bGenerateOverlapEvents || bForceGatherOverlaps) && CurrentPrimitive->IsQueryCollisionEnabled() && CurrentPrimitive->GetCollisionResponseToChannel(GetCollisionObjectType()) != ECR_Ignore)
					{
						MutableThis->bCachedAllCollideableDescendantsRelative = false;
						MutableThis->LastCheckedAllCollideableDescendantsTime = MyWorld->GetTimeSeconds();
						return false;
					}
				}

				ComponentStack.Append(CurrentComp->GetAttachChildren());
			}
		}
	}

	MutableThis->bCachedAllCollideableDescendantsRelative = true;
	return true;
}

void UPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
	if(FBodyInstance* BI = GetBodyInstance(NAME_None, /*bGetWelded=*/ false))
	{
		if (BI->bSimulatePhysics && !BI->WeldParent)
		{
			//Since the object is physically simulated it can't be attached
			const bool bSavedDisableDetachmentUpdateOverlaps = bDisableDetachmentUpdateOverlaps;
			bDisableDetachmentUpdateOverlaps = true;
			DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			bDisableDetachmentUpdateOverlaps = bSavedDisableDetachmentUpdateOverlaps;
		}
	}
	
}

void UPrimitiveComponent::IgnoreActorWhenMoving(AActor* Actor, bool bShouldIgnore)
{
	// Clean up stale references
	MoveIgnoreActors.RemoveSwap(nullptr);

	// Add/Remove the actor from the list
	if (Actor)
	{
		if (bShouldIgnore)
		{
			MoveIgnoreActors.AddUnique(Actor);
		}
		else
		{
			MoveIgnoreActors.RemoveSingleSwap(Actor);
		}
	}
}

TArray<AActor*> UPrimitiveComponent::CopyArrayOfMoveIgnoreActors()
{
	for (int32 Index = MoveIgnoreActors.Num() - 1; Index >=0; --Index)
	{
		const AActor* const MoveIgnoreActor = MoveIgnoreActors[Index];
		if (MoveIgnoreActor == nullptr || MoveIgnoreActor->IsPendingKill())
		{
			MoveIgnoreActors.RemoveAtSwap(Index,1,false);
		}
	}
	return MoveIgnoreActors;
}

void UPrimitiveComponent::ClearMoveIgnoreActors()
{
	MoveIgnoreActors.Empty();
}

void UPrimitiveComponent::IgnoreComponentWhenMoving(UPrimitiveComponent* Component, bool bShouldIgnore)
{
	// Clean up stale references
	MoveIgnoreComponents.RemoveSwap(nullptr);

	// Add/Remove the component from the list
	if (Component)
	{
		if (bShouldIgnore)
		{
			MoveIgnoreComponents.AddUnique(Component);
		}
		else
		{
			MoveIgnoreComponents.RemoveSingleSwap(Component);
		}
	}
}

TArray<UPrimitiveComponent*> UPrimitiveComponent::CopyArrayOfMoveIgnoreComponents()
{
	for (int32 Index = MoveIgnoreComponents.Num() - 1; Index >= 0; --Index)
	{
		const UPrimitiveComponent* const MoveIgnoreComponent = MoveIgnoreComponents[Index];
		if (MoveIgnoreComponent == nullptr || MoveIgnoreComponent->IsPendingKill())
		{
			MoveIgnoreComponents.RemoveAtSwap(Index, 1, false);
		}
	}
	return MoveIgnoreComponents;
}

void UPrimitiveComponent::UpdateOverlaps(const TArray<FOverlapInfo>* NewPendingOverlaps, bool bDoNotifies, const TArray<FOverlapInfo>* OverlapsAtEndLocation)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateOverlaps); 

	if (IsDeferringMovementUpdates())
	{
		// Someone tried to call UpdateOverlaps() explicitly during a deferred update, this means they really have a good reason to force it.
		GetCurrentScopedMovement()->ForceOverlapUpdate();
		return;
	}

	// first, dispatch any pending overlaps
	if (bGenerateOverlapEvents && IsQueryCollisionEnabled())
	{
		// if we haven't begun play, we're still setting things up (e.g. we might be inside one of the construction scripts)
		// so we don't want to generate overlaps yet.
		AActor* const MyActor = GetOwner();
		if ( MyActor && MyActor->IsActorInitialized() )
		{
			const FTransform PrevTransform = GetComponentTransform();
			// If we are the root component we ignore child components. Those children will update their overlaps when we descend into the child tree.
			// This aids an optimization in MoveComponent.
			const bool bIgnoreChildren = (MyActor->GetRootComponent() == this);

			if (NewPendingOverlaps)
			{
				// Note: BeginComponentOverlap() only triggers overlaps where bGenerateOverlapEvents is true on both components.
				for (int32 Idx=0; Idx<NewPendingOverlaps->Num(); ++Idx)
				{
					BeginComponentOverlap( (*NewPendingOverlaps)[Idx], bDoNotifies );
				}
			}

			// now generate full list of new touches, so we can compare to existing list and
			// determine what changed
			TInlineOverlapInfoArray NewOverlappingComponents;

			// If pending kill, we should not generate any new overlaps
			if (!IsPendingKill())
			{
				// Might be able to avoid testing for new overlaps at the end location.
				if (OverlapsAtEndLocation != NULL && bAllowCachedOverlapsCVar && PrevTransform.Equals(GetComponentTransform()))
				{
					UE_LOG(LogPrimitiveComponent, VeryVerbose, TEXT("%s->%s Skipping overlap test!"), *GetNameSafe(GetOwner()), *GetName());
					NewOverlappingComponents = *OverlapsAtEndLocation;

					// BeginComponentOverlap may have disabled what we thought were valid overlaps at the end (collision response or overlap flags could change).
					// Or we have overlaps from a scoped update that didn't require overlap events, but we need to remove those now.
					if (NewPendingOverlaps && NewPendingOverlaps->Num() > 0)
					{
						NewOverlappingComponents.RemoveAllSwap(FPredicateFilterCannotOverlap(*this), /*bAllowShrinking*/ false);
					}
				}
				else
				{
					UE_LOG(LogPrimitiveComponent, VeryVerbose, TEXT("%s->%s Performing overlaps!"), *GetNameSafe(GetOwner()), *GetName());
					UWorld* const MyWorld = MyActor->GetWorld();
					TArray<FOverlapResult> Overlaps;
					// note this will optionally include overlaps with components in the same actor (depending on bIgnoreChildren). 
					FComponentQueryParams Params(SCENE_QUERY_STAT(UpdateOverlaps), bIgnoreChildren ? MyActor : nullptr);
					Params.bIgnoreBlocks = true;	//We don't care about blockers since we only route overlap events to real overlaps
					FCollisionResponseParams ResponseParam;
					InitSweepCollisionParams(Params, ResponseParam);
					ComponentOverlapMulti(Overlaps, MyWorld, GetComponentLocation(), GetComponentQuat(), GetCollisionObjectType(), Params);

					for( int32 ResultIdx=0; ResultIdx<Overlaps.Num(); ResultIdx++ )
					{
						const FOverlapResult& Result = Overlaps[ResultIdx];

						UPrimitiveComponent* const HitComp = Result.Component.Get();
						if (HitComp && (HitComp != this) && HitComp->bGenerateOverlapEvents)
						{
							if (!ShouldIgnoreOverlapResult(MyWorld, MyActor, *this, Result.GetActor(), *HitComp, /*bCheckOverlapFlags=*/ true))
							{
								NewOverlappingComponents.Add(FOverlapInfo(HitComp, Result.ItemIndex));		// don't need to add unique unless the overlap check can return dupes
							}
						}
					}
				}
			}

			if (OverlappingComponents.Num() > 0)
			{
				// make a copy of the old that we can manipulate to avoid n^2 searching later
				TInlineOverlapInfoArray OldOverlappingComponents;
				if (bIgnoreChildren)
				{
					OldOverlappingComponents = OverlappingComponents.FilterByPredicate(FPredicateOverlapHasDifferentActor(*MyActor));
				}
				else
				{
					OldOverlappingComponents = OverlappingComponents;
				}

				// Now we want to compare the old and new overlap lists to determine 
				// what overlaps are in old and not in new (need end overlap notifies), and 
				// what overlaps are in new and not in old (need begin overlap notifies).
				// We do this by removing common entries from both lists, since overlapping status has not changed for them.
				// What is left over will be what has changed.
				for (int32 CompIdx=0; CompIdx < OldOverlappingComponents.Num() && NewOverlappingComponents.Num() > 0; ++CompIdx)
				{
					// RemoveSingleSwap is ok, since it is not necessary to maintain order
					const bool bAllowShrinking = false;
					if (NewOverlappingComponents.RemoveSingleSwap(OldOverlappingComponents[CompIdx], bAllowShrinking) > 0)
					{
						OldOverlappingComponents.RemoveAtSwap(CompIdx, 1, bAllowShrinking);
						--CompIdx;
					}
				}

				// OldOverlappingComponents now contains only previous overlaps that are confirmed to no longer be valid.
				for (auto CompIt = OldOverlappingComponents.CreateIterator(); CompIt; ++CompIt)
				{
					const FOverlapInfo& OtherOverlap = *CompIt;
					if (OtherOverlap.OverlapInfo.Component.IsValid())
					{
						EndComponentOverlap(OtherOverlap, bDoNotifies, false);
					}
					else
					{
						// Remove stale item. Reclaim memory only if it's getting large, to try to avoid churn but avoid bloating component's memory usage.
						const bool bAllowShrinking = (OverlappingComponents.Max() >= 24);
						OverlappingComponents.RemoveSingleSwap(OtherOverlap, bAllowShrinking);
					}
				}
			}

			// NewOverlappingComponents now contains only new overlaps that didn't exist previously.
			for (auto CompIt = NewOverlappingComponents.CreateIterator(); CompIt; ++CompIt)
			{
				const FOverlapInfo& OtherOverlap = *CompIt;
				BeginComponentOverlap(OtherOverlap, bDoNotifies);
			}
		}
	}
	else
	{
		// bGenerateOverlapEvents is false or collision is disabled
		// End all overlaps that exist, in case bGenerateOverlapEvents was true last tick (i.e. was just turned off)
		if (OverlappingComponents.Num() > 0)
		{
			const bool bSkipNotifySelf = false;
			ClearComponentOverlaps(bDoNotifies, bSkipNotifySelf);
		}
	}

	// now update any children down the chain.
	// since on overlap events could manipulate the child array we need to take a copy
	// of it to avoid missing any children if one is removed from the middle
	TInlineComponentArray<USceneComponent*> AttachedChildren;
	AttachedChildren.Append(GetAttachChildren());

	for (USceneComponent* const ChildComp : AttachedChildren)
	{
		if (ChildComp)
		{
			// Do not pass on OverlapsAtEndLocation, it only applied to this component.
			ChildComp->UpdateOverlaps(nullptr, bDoNotifies, nullptr);
		}
	}

	// Update physics volume using most current overlaps
	if (bShouldUpdatePhysicsVolume)
	{
		UpdatePhysicsVolume(bDoNotifies);
	}
}

void UPrimitiveComponent::ClearComponentOverlaps(bool bDoNotifies, bool bSkipNotifySelf)
{
	if (OverlappingComponents.Num() > 0)
	{
		// Make a copy since EndComponentOverlap will remove items from OverlappingComponents.
		const TInlineOverlapInfoArray OverlapsCopy(OverlappingComponents);
		for (const FOverlapInfo& OtherOverlap : OverlapsCopy)
		{
			EndComponentOverlap(OtherOverlap, bDoNotifies, bSkipNotifySelf);
		}
	}
}

bool UPrimitiveComponent::ComponentOverlapMultiImpl(TArray<struct FOverlapResult>& OutOverlaps, const UWorld* World, const FVector& Pos, const FQuat& Quat, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	FComponentQueryParams ParamsWithSelf = Params;
	ParamsWithSelf.AddIgnoredComponent_LikelyDuplicatedRoot(this);
	OutOverlaps.Reset();
	return BodyInstance.OverlapMulti(OutOverlaps, World, /*pWorldToComponent=*/ nullptr, Pos, Quat, TestChannel, ParamsWithSelf, FCollisionResponseParams(GetCollisionResponseToChannels()), ObjectQueryParams);
}

void UPrimitiveComponent::UpdatePhysicsVolume( bool bTriggerNotifiers )
{
	if (bShouldUpdatePhysicsVolume && !IsPendingKill())
	{
		SCOPE_CYCLE_COUNTER(STAT_UpdatePhysicsVolume);
		if (UWorld* MyWorld = GetWorld())
		{
			if (MyWorld->GetNonDefaultPhysicsVolumeCount() == 0)
			{
				SetPhysicsVolume(MyWorld->GetDefaultPhysicsVolume(), bTriggerNotifiers);
			}
			else if (bGenerateOverlapEvents && IsQueryCollisionEnabled())
			{
				APhysicsVolume* BestVolume = MyWorld->GetDefaultPhysicsVolume();
				int32 BestPriority = BestVolume->Priority;

				for (auto CompIt = OverlappingComponents.CreateIterator(); CompIt; ++CompIt)
				{
					const FOverlapInfo& Overlap = *CompIt;
					UPrimitiveComponent* OtherComponent = Overlap.OverlapInfo.Component.Get();
					if (OtherComponent && OtherComponent->bGenerateOverlapEvents)
					{
						APhysicsVolume* V = Cast<APhysicsVolume>(OtherComponent->GetOwner());
						if (V && V->Priority > BestPriority)
						{
							if (V->IsOverlapInVolume(*this))
							{
								BestPriority = V->Priority;
								BestVolume = V;
							}
						}
					}
				}

				SetPhysicsVolume(BestVolume, bTriggerNotifiers);
			}
			else
			{
				Super::UpdatePhysicsVolume(bTriggerNotifiers);
			}
		}
	}
}


void UPrimitiveComponent::DispatchMouseOverEvents(UPrimitiveComponent* CurrentComponent, UPrimitiveComponent* NewComponent)
{
	if (NewComponent)
	{
		AActor* NewOwner = NewComponent->GetOwner();

		bool bBroadcastComponentBegin = true;
		bool bBroadcastActorBegin = true;
		if (CurrentComponent)
		{
			AActor* CurrentOwner = CurrentComponent->GetOwner();

			if (NewComponent == CurrentComponent)
			{
				bBroadcastComponentBegin = false;
			}
			else
			{
				bBroadcastActorBegin = (NewOwner != CurrentOwner);

				if (!CurrentComponent->IsPendingKill())
				{
					CurrentComponent->OnEndCursorOver.Broadcast(CurrentComponent);
				}
				if (bBroadcastActorBegin && IsActorValidToNotify(CurrentOwner))
				{
					CurrentOwner->NotifyActorEndCursorOver();
					if (IsActorValidToNotify(CurrentOwner))
					{
						CurrentOwner->OnEndCursorOver.Broadcast(CurrentOwner);
					}
				}
			}
		}

		if (bBroadcastComponentBegin)
		{
			if (bBroadcastActorBegin && IsActorValidToNotify(NewOwner))
			{
				NewOwner->NotifyActorBeginCursorOver();
				if (IsActorValidToNotify(NewOwner))
				{
					NewOwner->OnBeginCursorOver.Broadcast(NewOwner);
				}
			}
			if (!NewComponent->IsPendingKill())
			{
				NewComponent->OnBeginCursorOver.Broadcast(NewComponent);
			}
		}
	}
	else if (CurrentComponent)
	{
		AActor* CurrentOwner = CurrentComponent->GetOwner();

		if (!CurrentComponent->IsPendingKill())
		{
			CurrentComponent->OnEndCursorOver.Broadcast(CurrentComponent);
		}

		if (IsActorValidToNotify(CurrentOwner))
		{
			CurrentOwner->NotifyActorEndCursorOver();
			if (IsActorValidToNotify(CurrentOwner))
			{
				CurrentOwner->OnEndCursorOver.Broadcast(CurrentOwner);
			}
		}
	}
}

void UPrimitiveComponent::DispatchTouchOverEvents(ETouchIndex::Type FingerIndex, UPrimitiveComponent* CurrentComponent, UPrimitiveComponent* NewComponent)
{
	if (NewComponent)
	{
		AActor* NewOwner = NewComponent->GetOwner();

		bool bBroadcastComponentBegin = true;
		bool bBroadcastActorBegin = true;
		if (CurrentComponent)
		{
			AActor* CurrentOwner = CurrentComponent->GetOwner();

			if (NewComponent == CurrentComponent)
			{
				bBroadcastComponentBegin = false;
			}
			else
			{
				bBroadcastActorBegin = (NewOwner != CurrentOwner);

				if (!CurrentComponent->IsPendingKill())
				{
					CurrentComponent->OnInputTouchLeave.Broadcast(FingerIndex, CurrentComponent);
				}
				if (bBroadcastActorBegin && IsActorValidToNotify(CurrentOwner))
				{
					CurrentOwner->NotifyActorOnInputTouchLeave(FingerIndex);
					if (IsActorValidToNotify(CurrentOwner))
					{
						CurrentOwner->OnInputTouchLeave.Broadcast(FingerIndex, CurrentOwner);
					}
				}
			}
		}

		if (bBroadcastComponentBegin)
		{
			if (bBroadcastActorBegin && IsActorValidToNotify(NewOwner))
			{
				NewOwner->NotifyActorOnInputTouchEnter(FingerIndex);
				if (IsActorValidToNotify(NewOwner))
				{
					NewOwner->OnInputTouchEnter.Broadcast(FingerIndex, NewOwner);
				}
			}
			if (!NewComponent->IsPendingKill())
			{
				NewComponent->OnInputTouchEnter.Broadcast(FingerIndex, NewComponent);
			}
		}
	}
	else if (CurrentComponent)
	{
		AActor* CurrentOwner = CurrentComponent->GetOwner();

		if (!CurrentComponent->IsPendingKill())
		{
			CurrentComponent->OnInputTouchLeave.Broadcast(FingerIndex, CurrentComponent);
		}

		if (IsActorValidToNotify(CurrentOwner))
		{
			CurrentOwner->NotifyActorOnInputTouchLeave(FingerIndex);
			if (IsActorValidToNotify(CurrentOwner))
			{
				CurrentOwner->OnInputTouchLeave.Broadcast(FingerIndex, CurrentOwner);
			}
		}
	}
}

void UPrimitiveComponent::DispatchOnClicked(FKey ButtonPressed)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnClicked(ButtonPressed);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnClicked.Broadcast(GetOwner(), ButtonPressed);
		}
	}

	if (!IsPendingKill())
	{
		OnClicked.Broadcast(this, ButtonPressed);
	}
}

void UPrimitiveComponent::DispatchOnReleased(FKey ButtonReleased)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnReleased(ButtonReleased);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnReleased.Broadcast(GetOwner(), ButtonReleased);
		}
	}

	if (!IsPendingKill())
	{
		OnReleased.Broadcast(this, ButtonReleased);
	}
}

void UPrimitiveComponent::DispatchOnInputTouchBegin(const ETouchIndex::Type FingerIndex)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnInputTouchBegin(FingerIndex);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnInputTouchBegin.Broadcast(FingerIndex, GetOwner());
		}
	}

	if (!IsPendingKill())
	{
		OnInputTouchBegin.Broadcast(FingerIndex, this);
	}
}

void UPrimitiveComponent::DispatchOnInputTouchEnd(const ETouchIndex::Type FingerIndex)
{
	if (IsActorValidToNotify(GetOwner()))
	{
		GetOwner()->NotifyActorOnInputTouchEnd(FingerIndex);
		if (IsActorValidToNotify(GetOwner()))
		{
			GetOwner()->OnInputTouchEnd.Broadcast(FingerIndex, GetOwner());
		}
	}

	if (!IsPendingKill())
	{
		OnInputTouchEnd.Broadcast(FingerIndex, this);
	}
}

void UPrimitiveComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (BodyInstance.IsValidBodyInstance())
	{
		BodyInstance.GetBodyInstanceResourceSizeEx(CumulativeResourceSize);
	}
	if (SceneProxy)
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SceneProxy->GetMemoryFootprint());
	}

}

void UPrimitiveComponent::SetRenderCustomDepth(bool bValue)
{
	if( bRenderCustomDepth != bValue )
	{
		bRenderCustomDepth = bValue;
		if ( SceneProxy )
		{
			SceneProxy->SetCustomDepthEnabled_GameThread(bRenderCustomDepth);
		}
		else
		{
			MarkRenderStateDirty();
		}
	}
}

void UPrimitiveComponent::SetCustomDepthStencilValue(int32 Value)
{
	// Clamping to currently usable stencil range (as specified in property UI and tooltips)
	int32 ClampedValue = FMath::Clamp(Value, 0, 255);

	if (CustomDepthStencilValue != ClampedValue)
	{
		CustomDepthStencilValue = ClampedValue;
		if ( SceneProxy )
		{
			SceneProxy->SetCustomDepthStencilValue_GameThread(CustomDepthStencilValue);
		}
		else
		{
			MarkRenderStateDirty();
		}
	}
}

void UPrimitiveComponent::SetCustomDepthStencilWriteMask(ERendererStencilMask WriteMask)
{
	if (CustomDepthStencilWriteMask != WriteMask)
	{
		CustomDepthStencilWriteMask = WriteMask;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetRenderInMainPass(bool bValue)
{
	if (bRenderInMainPass != bValue)
	{
		bRenderInMainPass = bValue;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetRenderInMono(bool bValue)
{
	if (bRenderInMono != bValue)
	{
		bRenderInMono = bValue;
		MarkRenderStateDirty();
	}
}

void UPrimitiveComponent::SetLODParentPrimitive(UPrimitiveComponent * InLODParentPrimitive)
{
#if WITH_EDITOR
	if (ShouldGenerateAutoLOD())
#endif
	{
		// @todo, what do we do with old parent. We can't just reset undo parent because the parent migh be used by other primitive
		LODParentPrimitive = InLODParentPrimitive;
		MarkRenderStateDirty();
	}
}

UPrimitiveComponent* UPrimitiveComponent::GetLODParentPrimitive() const
{
	return LODParentPrimitive;
}
#if WITH_EDITOR
const int32 UPrimitiveComponent::GetNumUncachedStaticLightingInteractions() const
{
	int32 NumUncachedStaticLighting = 0;

	NumUncachedStaticLighting += Super::GetNumUncachedStaticLightingInteractions();
	if (SceneProxy)
	{
		NumUncachedStaticLighting += SceneProxy->GetNumUncachedStaticLightingInteractions();
	}
	return NumUncachedStaticLighting;
}
#endif


bool UPrimitiveComponent::CanCharacterStepUp(APawn* Pawn) const
{
	if ( CanCharacterStepUpOn != ECB_Owner )
	{
		return CanCharacterStepUpOn == ECB_Yes;
	}
	else
	{	
		const AActor* Owner = GetOwner();
		return Owner && Owner->CanBeBaseForCharacter(Pawn);
	}
}

bool UPrimitiveComponent::CanEditSimulatePhysics()
{
	//Even if there's no collision but there is a body setup, we still let them simulate physics.
	// The object falls through the world - this behavior is debatable but what we decided on for now
	return GetBodySetup() != nullptr;
}

void UPrimitiveComponent::SetCustomNavigableGeometry(const EHasCustomNavigableGeometry::Type InType)
{
	bHasCustomNavigableGeometry = InType;
}

#if WITH_EDITOR
const bool UPrimitiveComponent::ShouldGenerateAutoLOD() const
{
	return (Mobility != EComponentMobility::Movable) && bEnableAutoLODGeneration;
}
#endif 

#undef LOCTEXT_NAMESPACE
