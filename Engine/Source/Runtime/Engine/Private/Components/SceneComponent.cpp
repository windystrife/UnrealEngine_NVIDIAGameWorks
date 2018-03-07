// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneComponent.cpp
=============================================================================*/


#include "Components/SceneComponent.h"
#include "EngineStats.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/MapBuildDataRegistry.h"
#include "GameFramework/PhysicsVolume.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"
#include "ComponentReregisterContext.h"
#include "UnrealEngine.h"
#include "PhysicsPublic.h"
#include "Logging/MessageLog.h"
#include "Net/UnrealNetwork.h"
#include "ComponentUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Components/ChildActorComponent.h"
#include "UObject/UObjectThreadContext.h"
#include "Engine/SCS_Node.h"
#include "EngineGlobals.h"

#define LOCTEXT_NAMESPACE "SceneComponent"


namespace SceneComponentStatics
{
	static const FName DefaultSceneRootVariableName(TEXT("DefaultSceneRoot"));
	static const FName MobilityName(TEXT("Mobility"));
	static const FText MobilityWarnText = LOCTEXT("InvalidMove", "move");
	static const FName PhysicsVolumeTraceName(TEXT("PhysicsVolumeTrace"));
}

DEFINE_LOG_CATEGORY_STATIC(LogSceneComponent, Log, All);

DECLARE_CYCLE_STAT(TEXT("UpdateComponentToWorld"), STAT_UpdateComponentToWorld, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("UpdateChildTransforms"), STAT_UpdateChildTransforms, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("Component UpdateBounds"), STAT_ComponentUpdateBounds, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("Component UpdateNavData"), STAT_ComponentUpdateNavData, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("Component PostUpdateNavData"), STAT_ComponentPostUpdateNavData, STATGROUP_Component);


FOverlapInfo::FOverlapInfo(UPrimitiveComponent* InComponent, int32 InBodyIndex)
	: bFromSweep(false)
{
	OverlapInfo.Actor = InComponent ? InComponent->GetOwner() : nullptr;
	OverlapInfo.Component = InComponent;
	OverlapInfo.Item = InBodyIndex;
}

FName USceneComponent::GetDefaultSceneRootVariableName()
{
	return SceneComponentStatics::DefaultSceneRootVariableName;
}

USceneComponent::USceneComponent(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
	, CachedLevelCollection(nullptr)
{
	Mobility = EComponentMobility::Movable;
	RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);
	// default behavior is visible
	bVisible = true;
	bAutoActivate = false;
}

#if WITH_EDITORONLY_DATA
void USceneComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USceneComponent* This = CastChecked<USceneComponent>(InThis);
	Collector.AddReferencedObject(This->SpriteComponent);

	if (GComponentsWithLegacyLightmaps.GetAnnotationMap().Num() > 0)
	{
		FMeshMapBuildLegacyData LegacyMeshData = GComponentsWithLegacyLightmaps.GetAnnotation(This);

		for (int32 EntryIndex = 0; EntryIndex < LegacyMeshData.Data.Num(); EntryIndex++)
		{
			LegacyMeshData.Data[EntryIndex].Value->AddReferencedObjects(Collector);
		}
	}

	Super::AddReferencedObjects(InThis, Collector);
}
#endif

#if WITH_EDITOR

/**
 * A static helper function used meant as the default for determining if a 
 * component's Mobility should be overridden.
 * 
 * @param  CurrentMobility	The component's current Mobility setting
 * @param  NewMobility		The proposed new Mobility for the component in question
 * @return True if the two mobilities are not equal (false if they are equal)
 */
static bool AreMobilitiesDifferent(EComponentMobility::Type CurrentMobility, EComponentMobility::Type NewMobility)
{
	return CurrentMobility != NewMobility;
}
DECLARE_DELEGATE_RetVal_OneParam(bool, FMobilityQueryDelegate, EComponentMobility::Type);

/**
 * A static helper function that recursively alters the Mobility property for all 
 * sub-components (descending from the specified USceneComponent)
 * 
 * @param  SceneComponentObject		The component whose sub-components you want to alter
 * @param  NewMobilityType			The Mobility type you want to switch sub-components over to
 * @param  ShouldOverrideMobility	A delegate used to determine if a sub-component's Mobility should be overridden
 *									(if left unset it will default to the AreMobilitiesDifferent() function)
 * @return The number of descendants that had their mobility altered.
 */
static int32 SetDescendantMobility(USceneComponent const* SceneComponentObject, EComponentMobility::Type NewMobilityType, FMobilityQueryDelegate ShouldOverrideMobility = FMobilityQueryDelegate())
{
	if (!ensure(SceneComponentObject != nullptr))
	{
		return 0;
	}

	TArray<USceneComponent*> AttachedChildren = SceneComponentObject->GetAttachChildren();
	// gather children for component templates
	USCS_Node* SCSNode = ComponentUtils::FindCorrespondingSCSNode(SceneComponentObject);
	if (SCSNode != nullptr)
	{
		// gather children from the SCSNode
		for (USCS_Node* SCSChild : SCSNode->GetChildNodes())
		{
			USceneComponent* ChildSceneComponent = Cast<USceneComponent>(SCSChild->ComponentTemplate);
			if (ChildSceneComponent != nullptr)
			{
				AttachedChildren.Add(ChildSceneComponent);
			}
		}
	}

	if (!ShouldOverrideMobility.IsBound())
	{
		ShouldOverrideMobility = FMobilityQueryDelegate::CreateStatic(&AreMobilitiesDifferent, NewMobilityType);
	}

	int32 NumDescendantsChanged = 0;
	// recursively alter the mobility for children and deeper descendants 
	for (USceneComponent* ChildSceneComponent : AttachedChildren)
	{
		if (ChildSceneComponent)
		{
			if (ShouldOverrideMobility.Execute(ChildSceneComponent->Mobility))
			{
				ChildSceneComponent->Modify();

				// USceneComponents shouldn't be set Stationary 
				if ((NewMobilityType == EComponentMobility::Stationary) && ChildSceneComponent->IsA(UStaticMeshComponent::StaticClass()))
				{
					// make it Movable (because it is acceptable for Stationary parents to have Movable children)
					ChildSceneComponent->Mobility = EComponentMobility::Movable;
				}
				else
				{
					ChildSceneComponent->Mobility = NewMobilityType;
				}

				ChildSceneComponent->RecreatePhysicsState();

				++NumDescendantsChanged;
			}
			NumDescendantsChanged += SetDescendantMobility(ChildSceneComponent, NewMobilityType, ShouldOverrideMobility);
		}
	}

	return NumDescendantsChanged;
}

/**
 * A static helper function that alters the Mobility property for all ancestor
 * components (ancestors of the specified USceneComponent).
 * 
 * @param  SceneComponentObject		The component whose attached ancestors you want to alter
 * @param  NewMobilityType			The Mobility type you want to switch ancestor components over to
 * @param  ShouldOverrideMobility	A delegate used to determine if a ancestor's Mobility should be overridden
 *									(if left unset it will default to the AreMobilitiesDifferent() function)
 * @return The number of ancestors that had their mobility altered.
 */
static int32 SetAncestorMobility(USceneComponent const* SceneComponentObject, EComponentMobility::Type NewMobilityType, FMobilityQueryDelegate ShouldOverrideMobility = FMobilityQueryDelegate())
{
	if (!ensure(SceneComponentObject != nullptr))
	{
		return 0;
	}

	if (!ShouldOverrideMobility.IsBound())
	{
		ShouldOverrideMobility = FMobilityQueryDelegate::CreateStatic(&AreMobilitiesDifferent, NewMobilityType);
	}

	int32 MobilityAlteredCount = 0;
	while(USceneComponent* AttachedParent = ComponentUtils::GetAttachedParent(SceneComponentObject))
	{
		if (ShouldOverrideMobility.Execute(AttachedParent->Mobility))
		{
			// USceneComponents shouldn't be set Stationary 
			switch(NewMobilityType)
			{
			case EComponentMobility::Stationary:

				if (UStaticMeshComponent* StaticMeshParent = Cast<UStaticMeshComponent>(AttachedParent))
				{
					StaticMeshParent->Modify();

					// make it Static (because it is acceptable for Stationary children to have Static parents)
					StaticMeshParent->Mobility = EComponentMobility::Static;
					StaticMeshParent->SetSimulatePhysics(false);
				}
				break;

			case EComponentMobility::Static:

				AttachedParent->Modify();
				if (UPrimitiveComponent* PrimitiveComponentParent = Cast<UPrimitiveComponent>(AttachedParent))
				{
					PrimitiveComponentParent->SetSimulatePhysics(false);
				}
				AttachedParent->Mobility = NewMobilityType;
				break;

			default:
				AttachedParent->Modify();
				AttachedParent->Mobility = NewMobilityType;
			}

			AttachedParent->RecreatePhysicsState();
			++MobilityAlteredCount;
		}
		SceneComponentObject = AttachedParent;
	}

	return MobilityAlteredCount;
}

/**
 * When a scene component's Mobility is altered, we need to make sure the scene hierarchy is
 * updated. Parents can't be more mobile than their children. This means that certain
 * mobility hierarchy structures are disallowed, like:
 *
 *    Movable
 *   |-Stationary   <-- NOT allowed
 *   Movable
 *   |-Static       <-- NOT allowed
 *   Stationary
 *   |-Static       <-- NOT allowed
 *
 * This method walks the hierarchy and alters parent/child component's Mobility as a result of
 * this property change.
 */
static void UpdateAttachedMobility(USceneComponent* ComponentThatChanged)
{
	// Attached parent components can't be more mobile than their children. This means that 
	// certain mobility hierarchy structures are disallowed. So we have to walk the hierarchy 
	// and alter parent/child components as a result of this property change.

	// track how many other components we had to change
	int32 NumMobilityChanges = 0;

	// Movable components can only have movable sub-components
	if(ComponentThatChanged->Mobility == EComponentMobility::Movable)
	{
		NumMobilityChanges += SetDescendantMobility(ComponentThatChanged, EComponentMobility::Movable);
	}
	else if(ComponentThatChanged->Mobility == EComponentMobility::Stationary)
	{
		// a functor for checking if we should change a component's Mobility
		struct FMobilityEqualityFunctor
		{
			bool operator()(EComponentMobility::Type CurrentMobility, EComponentMobility::Type CheckValue)
			{
				return CurrentMobility == CheckValue;
			}
		};
		FMobilityEqualityFunctor EquivalenceFunctor;

		// a delegate for checking if components are Static
		FMobilityQueryDelegate IsStaticDelegate  = FMobilityQueryDelegate::CreateRaw(&EquivalenceFunctor, &FMobilityEqualityFunctor::operator(), EComponentMobility::Static);
		// a delegate for checking if components are Movable
		FMobilityQueryDelegate IsMovableDelegate = FMobilityQueryDelegate::CreateRaw(&EquivalenceFunctor, &FMobilityEqualityFunctor::operator(), EComponentMobility::Movable);

		// if any descendants are static, change them to stationary (or movable for static meshes)
		NumMobilityChanges += SetDescendantMobility(ComponentThatChanged, EComponentMobility::Stationary, IsStaticDelegate);

		// if any ancestors are movable, change them to stationary (or static for static meshes)
		NumMobilityChanges += SetAncestorMobility(ComponentThatChanged, EComponentMobility::Stationary, IsMovableDelegate);
	}
	else // if MobilityValue == Static
	{
		// ensure we have the mobility we expected (in case someone adds a new one)
		ensure(ComponentThatChanged->Mobility == EComponentMobility::Static);

		if (USceneComponent* ParentComponent = ComponentUtils::GetAttachedParent(ComponentThatChanged))
		{
			// Cannot set mobility on skeletal mesh component to static, so detach instead, this is prevented in the editor when trying to attach a static component to a skeletal mesh component
			if (ParentComponent->CanHaveStaticMobility())
			{
				NumMobilityChanges += SetAncestorMobility(ComponentThatChanged, EComponentMobility::Static);
			}
			else
			{
				ComponentThatChanged->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

				// Fire off a notification
				FText NotificationText = FText::Format(LOCTEXT("ComponentDetachedFromParentDueToMobility", "Caused {0} to be detached from its parent {1} because it does not allow to be static"), FText::FromName(ComponentThatChanged->GetFName()), FText::FromName(ParentComponent->GetFName()));
				FNotificationInfo Info(NotificationText);
				Info.bFireAndForget = true;
				Info.bUseThrobber = true;
				Info.ExpireDuration = 2.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}		
	}

	// if we altered any components (other than the ones selected), then notify the user
	if(NumMobilityChanges > 0)
	{
		FText NotificationText = LOCTEXT("MobilityAlteredSingularNotification", "Caused 1 component to also change Mobility");
		if(NumMobilityChanges > 1)
		{
			NotificationText = FText::Format(LOCTEXT("MobilityAlteredPluralNotification", "Caused {0} other components to also change Mobility"), FText::AsNumber(NumMobilityChanges));
		}
		FNotificationInfo Info(NotificationText);
		Info.bFireAndForget = true;
		Info.bUseThrobber   = true;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

/**
 * A static helper function that recursively alters the bIsEditorOnly property for all
 * sub-components (descending from the specified USceneComponent)
 *
 * @param  SceneComponentObject		The component whose sub-components you want to alter
 *
 * @return The number of descendants that had their bIsEditorOnly flag altered.
 */
static int32 SetDescendantIsEditorOnly(USceneComponent const* SceneComponentObject)
{
	if (!ensure(SceneComponentObject != nullptr))
	{
		return 0;
	}

	TArray<USceneComponent*> AttachedChildren = SceneComponentObject->GetAttachChildren();

	// Do we need the templates too?
	// gather children for component templates
	USCS_Node* SCSNode = ComponentUtils::FindCorrespondingSCSNode(SceneComponentObject);
	if (SCSNode != nullptr)
	{
		// gather children from the SCSNode
		for (USCS_Node* SCSChild : SCSNode->GetChildNodes())
		{
			USceneComponent* ChildSceneComponent = Cast<USceneComponent>(SCSChild->ComponentTemplate);
			if (ChildSceneComponent != nullptr)
			{
				AttachedChildren.Add(ChildSceneComponent);
			}
		}
	}

	int32 NumDescendantsChanged = 0;
	// recursively alter the bIsEditorOnly flag for children and deeper descendants 
	for (USceneComponent* ChildSceneComponent : AttachedChildren)
	{
		if (!ChildSceneComponent->bIsEditorOnly)
		{
			ChildSceneComponent->Modify();
			ChildSceneComponent->bIsEditorOnly = true;
			++NumDescendantsChanged;
		}
		NumDescendantsChanged += SetDescendantIsEditorOnly(ChildSceneComponent);
	}

	return NumDescendantsChanged;
}

/**
 * When a scene component's bIsEditorOnly behavior is altered, we need to make sure the scene hierarchy is
 * updated. All children of an editor only component must be editor only too.
 *
 * This method walks the hierarchy and alters parent/child component's bIsEditorOnly flag as a result of
 * this property change.
 */
static void UpdateAttachedIsEditorOnly(USceneComponent* ComponentThatChanged)
{
	const int32 NumComponentsChanged = SetDescendantIsEditorOnly(ComponentThatChanged);

	// if we altered any components (other than the ones selected), then notify the user
	if (NumComponentsChanged > 0 && !ComponentThatChanged->HasAllFlags(RF_ArchetypeObject))
	{
		FText NotificationText = LOCTEXT("IsEditorOnlyAlteredSingularNotification", "Caused 1 component to also change its IsEditorOnly behaviour");
		if (NumComponentsChanged > 1)
		{
			NotificationText = FText::Format(LOCTEXT("IsEditorOnlyAlteredPluralNotification", "Caused {0} other components to also change their IsEditorOnly behaviour"), FText::AsNumber(NumComponentsChanged));
		}
		FNotificationInfo Info(NotificationText);
		Info.bFireAndForget = true;
		Info.bUseThrobber = true;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void USceneComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const static FName LocationName("RelativeLocation");
	const static FName RotationName("RelativeRotation");
	const static FName ScaleName("RelativeScale3D");

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : FName();

	// Note: This must be called before UActorComponent::PostEditChangeChainProperty is called because this component will be reset when UActorComponent reruns construction scripts 
	if (PropertyName == SceneComponentStatics::MobilityName)
	{
		UpdateAttachedMobility(this);
	}
	if (bIsEditorOnly && PropertyName == GET_MEMBER_NAME_CHECKED(UActorComponent, bIsEditorOnly))
	{
		UpdateAttachedIsEditorOnly(this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyName == LocationName || PropertyName == RotationName || PropertyName == ScaleName)
	{
		UNavigationSystem::UpdateComponentInNavOctree(*this);
	}
}

void USceneComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// Note: This must be called before UActorComponent::PostEditChangeChainProperty is called because this component will be reset when UActorComponent reruns construction scripts 
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == SceneComponentStatics::MobilityName)
	{
		UpdateAttachedMobility(this);
	}
	if (bIsEditorOnly && PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UActorComponent, bIsEditorOnly))
	{
		UpdateAttachedIsEditorOnly(this);
	}

	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

#endif

FTransform USceneComponent::CalcNewComponentToWorld_GeneralCase(const FTransform& NewRelativeTransform, const USceneComponent* Parent, FName SocketName) const
{
	if (Parent != nullptr)
	{
		const FTransform ParentToWorld = Parent->GetSocketTransform(SocketName);
		FTransform NewCompToWorld = NewRelativeTransform * ParentToWorld;
		if(bAbsoluteLocation)
		{
			NewCompToWorld.CopyTranslation(NewRelativeTransform);
		}

		if(bAbsoluteRotation)
		{
			NewCompToWorld.CopyRotation(NewRelativeTransform);
		}

		if(bAbsoluteScale)
		{
			NewCompToWorld.CopyScale3D(NewRelativeTransform);
		}

		return NewCompToWorld;
	}
	else
	{
		return NewRelativeTransform;
	}
}

void USceneComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
}

void USceneComponent::UpdateComponentToWorldWithParent(USceneComponent* Parent,FName SocketName, EUpdateTransformFlags UpdateTransformFlags, const FQuat& RelativeRotationQuat, ETeleportType Teleport)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateComponentToWorld);
	FScopeCycleCounterUObject ComponentScope(this);

#if ENABLE_NAN_DIAGNOSTIC
	if (RelativeRotationQuat.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("USceneComponent:UpdateComponentToWorldWithParent found NaN in parameter RelativeRotationQuat: %s"), *RelativeRotationQuat.ToString());
	}
#endif

	// If our parent hasn't been updated before, we'll need walk up our parent attach hierarchy
	if (Parent && !Parent->bComponentToWorldUpdated)
	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_UpdateComponentToWorldWithParent_Parent);
		Parent->UpdateComponentToWorld();

		// Updating the parent may (depending on if we were already attached to parent) result in our being updated, so just return
		if (bComponentToWorldUpdated)
		{
			return;
		}
	}

	bComponentToWorldUpdated = true;

	FTransform NewTransform(NoInit);

	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_UpdateComponentToWorldWithParent_XForm);
		// Calculate the new ComponentToWorld transform
		const FTransform RelativeTransform(RelativeRotationQuat, RelativeLocation, RelativeScale3D);
#if ENABLE_NAN_DIAGNOSTIC
		if (!RelativeTransform.IsValid())
		{
			logOrEnsureNanError(TEXT("USceneComponent:UpdateComponentToWorldWithParent found NaN/INF in new RelativeTransform: %s"), *RelativeTransform.ToString());
		}
#endif
		NewTransform = CalcNewComponentToWorld(RelativeTransform, Parent, SocketName);
	}

#if DO_CHECK
	ensure(NewTransform.IsValid());
#endif

	// If transform has changed..
	bool bHasChanged;
	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_UpdateComponentToWorldWithParent_HasChanged);
		bHasChanged = !GetComponentTransform().Equals(NewTransform, SMALL_NUMBER);
	}
	if (bHasChanged)
	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_UpdateComponentToWorldWithParent_Changed);
		// Update transform
		ComponentToWorld = NewTransform;
		PropagateTransformUpdate(true, UpdateTransformFlags, Teleport);
	}
	else
	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_UpdateComponentToWorldWithParent_NotChanged);
		PropagateTransformUpdate(false);
	}
}

void USceneComponent::OnRegister()
{
	// If we need to perform a call to AttachTo, do that now
	// At this point scene component still has no any state (rendering, physics),
	// so this call will just add this component to an AttachChildren array of a the Parent component
	if (GetAttachParent())
	{
		if (AttachToComponent(GetAttachParent(), FAttachmentTransformRules::KeepRelativeTransform, GetAttachSocketName()) == false)
		{
			// Failed to attach, we need to clear AttachParent so we don't think we're actually attached when we're not.
			AttachParent = nullptr;
			AttachSocketName = NAME_None;
		}
	}
	
	// Cache the level collection that contains the level in which this component is registered for fast access in IsVisible().
	const UWorld* const World = GetWorld();
	if (World)
	{
		const ULevel* const CachedLevel = GetComponentLevel();
		CachedLevelCollection = CachedLevel ? CachedLevel->GetCachedLevelCollection() : nullptr;
	}

	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	if (bVisualizeComponent && SpriteComponent == nullptr && GetOwner() && !GetWorld()->IsGameWorld() )
	{
		// Create a new billboard component to serve as a visualization of the actor until there is another primitive component
		SpriteComponent = NewObject<UBillboardComponent>(GetOwner(), NAME_None, RF_Transactional | RF_Transient | RF_TextExportTransient);

		SpriteComponent->Sprite = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/EmptyActor.EmptyActor"));
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->Mobility = EComponentMobility::Movable;
		SpriteComponent->AlwaysLoadOnClient = false;
		SpriteComponent->bIsEditorOnly = true;
		SpriteComponent->SpriteInfo.Category = TEXT("Misc");
		SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT( "SpriteCategory", "Misc", "Misc" );
		SpriteComponent->CreationMethod = CreationMethod;
		SpriteComponent->bIsScreenSizeScaled = true;
		SpriteComponent->bUseInEditorScaling = true;

		SpriteComponent->SetupAttachment(this);
		SpriteComponent->RegisterComponent();
	}
#endif
}

void USceneComponent::OnUnregister()
{
	CachedLevelCollection = nullptr;

	Super::OnUnregister();
}

void USceneComponent::PropagateTransformUpdate(bool bTransformChanged, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate);
	if (IsDeferringMovementUpdates())
	{
		FScopedMovementUpdate* CurrentUpdate = GetCurrentScopedMovement();

		if (CurrentUpdate && Teleport == ETeleportType::TeleportPhysics)
		{
			// Remember this was a teleport
			CurrentUpdate->SetHasTeleported();
		}

		// We are deferring these updates until later.
		return;
	}
	const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
	FPlatformMisc::Prefetch(AttachedChildren.GetData());
	if (bTransformChanged)
	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_TransformChanged);
		{
			// Then update bounds
			//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_UpdateBounds);
			UpdateBounds();
		}

		// If registered, tell subsystems about the change in transform
		if(bRegistered)
		{
			// Call OnUpdateTransform if this components wants it
			if(bWantsOnUpdateTransform)
			{
				//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_OnUpdateTransform);
				OnUpdateTransform(UpdateTransformFlags, Teleport);
			}
			TransformUpdated.Broadcast(this, UpdateTransformFlags, Teleport);

			// Flag render transform as dirty
			MarkRenderTransformDirty();
		}
		
		{
			//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_UpdateChildTransforms);
			// Now go and update children
			//Do not pass skip physics to children. This is only used when physics updates us, but in that case we really do need to update the attached children since they are kinematic
			if (AttachedChildren.Num() > 0)
			{
				EUpdateTransformFlags ChildrenFlagNoPhysics = ~EUpdateTransformFlags::SkipPhysicsUpdate & UpdateTransformFlags;
				UpdateChildTransforms(ChildrenFlagNoPhysics, Teleport);
			}
		}

#if WITH_EDITOR
		// Notify the editor of transformation update
		if (!IsTemplate())
		{
			GEngine->BroadcastOnComponentTransformChanged(this, Teleport);
		}
#endif // WITH_EDITOR

		// Refresh navigation
		if (bNavigationRelevant && bRegistered)
		{
			UpdateNavigationData();
		}
	}
	else
	{
		//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_NOT_TransformChanged);
		{
			//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_UpdateBounds);
			// We update bounds even if transform doesn't change, as shape/mesh etc might have done
			UpdateBounds();
		}

		{
			//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_UpdateChildTransforms);
			// Now go and update children
			if (AttachedChildren.Num() > 0)
			{
				UpdateChildTransforms();
			}
		}

		// If registered, tell subsystems about the change in transform
		if (bRegistered)
		{
			//QUICK_SCOPE_CYCLE_COUNTER(STAT_USceneComponent_PropagateTransformUpdate_MarkRenderTransformDirty);
			// Need to flag as dirty so new bounds are sent to render thread
			MarkRenderTransformDirty();
		}
	}
}


void USceneComponent::EndScopedMovementUpdate(class FScopedMovementUpdate& CompletedScope)
{
	SCOPE_CYCLE_COUNTER(STAT_EndScopedMovementUpdate);
	checkSlow(IsInGameThread());

	// Special case when shutting down
	if (ScopedMovementStack.Num() == 0)
	{
		return;
	}

	// Process top of the stack
	FScopedMovementUpdate* CurrentScopedUpdate = ScopedMovementStack.Pop(false);
	checkSlow(CurrentScopedUpdate == &CompletedScope);
	{
		checkSlow(CurrentScopedUpdate->IsDeferringUpdates());
		if (ScopedMovementStack.Num() == 0)
		{
			// This was the last item on the stack, time to apply the updates if necessary
			const bool bTransformChanged = CurrentScopedUpdate->IsTransformDirty();
			if (bTransformChanged)
			{
				// Pass teleport flag if set
				PropagateTransformUpdate(true, EUpdateTransformFlags::None, CurrentScopedUpdate->bHasTeleported ? ETeleportType::TeleportPhysics : ETeleportType::None);
			}

			// We may have moved somewhere and then moved back to the start, we still need to update overlaps if we touched things along the way.
			// If no movement and no change in transform, nothing changed.
			if (bTransformChanged || CurrentScopedUpdate->bHasMoved)
			{
				UPrimitiveComponent* PrimitiveThis = Cast<UPrimitiveComponent>(this);
				if (PrimitiveThis)
				{
					// NOTE: UpdateOverlaps filters events to only consider overlaps where bGenerateOverlapEvents is true for both components, so it's ok if we queued up other overlaps.
					TArray<FOverlapInfo> EndOverlaps;
					const TArray<FOverlapInfo>* EndOverlapsPtr = CurrentScopedUpdate->GetOverlapsAtEnd(*PrimitiveThis, EndOverlaps, bTransformChanged);
					UpdateOverlaps(&CurrentScopedUpdate->GetPendingOverlaps(), true, EndOverlapsPtr);
				}
				else
				{
					UpdateOverlaps(nullptr, true, nullptr);
				}
			}

			// Dispatch all deferred blocking hits
			if (CurrentScopedUpdate->BlockingHits.Num() > 0)
			{
				AActor* const Owner = GetOwner();
				if (Owner)
				{
					// If we have blocking hits, we must be a primitive component.
					UPrimitiveComponent* PrimitiveThis = CastChecked<UPrimitiveComponent>(this);
					for (const FHitResult& Hit : CurrentScopedUpdate->BlockingHits)
					{
						// Overlaps may have caused us to be destroyed, as could other queued blocking hits.
						if (PrimitiveThis->IsPendingKill())
						{
							break;
						}

						// Collision response may change (due to overlaps or multiple blocking hits), make sure it's still considered blocking.
						if (PrimitiveThis->GetCollisionResponseToComponent(Hit.GetComponent()) == ECR_Block)
						{
							PrimitiveThis->DispatchBlockingHit(*Owner, Hit);
						}						
					}
				}
			}
		}
		else
		{
			// Combine with next item on the stack
			FScopedMovementUpdate* OuterScopedUpdate = ScopedMovementStack.Last();
			OuterScopedUpdate->OnInnerScopeComplete(*CurrentScopedUpdate);
		}
	}
}


void USceneComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	if (bPromoteChildren)
	{
		AActor* Owner = GetOwner();
		if (Owner != nullptr)
		{
			Owner->Modify();
			USceneComponent* ChildToPromote = nullptr;

			const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
			// Handle removal of the root node
			if (this == Owner->GetRootComponent())
			{
				// Always choose non editor-only child nodes over editor-only child nodes (since we don't want editor-only nodes to end up with non editor-only child nodes)
				// Exclude scene components owned by attached child actors
				USceneComponent* const * FindResult =
					AttachedChildren.FindByPredicate([Owner](USceneComponent* Child){ return Child != nullptr && !Child->IsEditorOnly() && Child->GetOwner() == Owner; });

				if (FindResult != nullptr)
				{
					ChildToPromote = *FindResult;
				}
				else
				{
					// Didn't find a suitable component to promote so create a new default component

					Rename(nullptr, GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors);

					// Construct a new default root component
					USceneComponent* NewRootComponent = NewObject<USceneComponent>(Owner, USceneComponent::GetDefaultSceneRootVariableName(), RF_Transactional);
					NewRootComponent->Mobility = Mobility;
					NewRootComponent->SetWorldLocationAndRotation(GetComponentLocation(), GetComponentRotation());
#if WITH_EDITORONLY_DATA
					NewRootComponent->bVisualizeComponent = true;
#endif
					Owner->AddInstanceComponent(NewRootComponent);
					NewRootComponent->RegisterComponent();

					// Designate the new default root as the child we're promoting
					ChildToPromote = NewRootComponent;
				}

				Owner->Modify();

				// Set the selected child node as the new root
				check(ChildToPromote != nullptr);
				Owner->SetRootComponent(ChildToPromote);
			}
			else    // ...not the root node, so we'll promote the selected child node to this position in its AttachParent's child array.
			{
				// Cache our AttachParent
				USceneComponent* CachedAttachParent = GetAttachParent();
				check(CachedAttachParent != nullptr);

				// Find the our position in its AttachParent's child array
				const TArray<USceneComponent*>& AttachSiblings = CachedAttachParent->GetAttachChildren();
				int32 Index = AttachSiblings.Find(this);
				check(Index != INDEX_NONE);

				// Detach from parent
				DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

				// Find an appropriate child node to promote to this node's position in the hierarchy
				if (AttachedChildren.Num() > 0)
				{
					// Always choose non editor-only child nodes over editor-only child nodes (since we don't want editor-only nodes to end up with non editor-only child nodes)
					USceneComponent* const * FindResult =
						AttachedChildren.FindByPredicate([Owner](USceneComponent* Child){ return Child != nullptr && !Child->IsEditorOnly(); });

					if (FindResult != nullptr)
					{
						ChildToPromote = *FindResult;
					}
					else
					{
						// Default to first child node
						check(AttachedChildren[0] != nullptr);
						ChildToPromote = AttachedChildren[0];
					}
				}

				if (ChildToPromote != nullptr)
				{
					// Attach the child node that we're promoting to the parent and move it to the same position as the old node was in the array
					ChildToPromote->AttachToComponent(CachedAttachParent, FAttachmentTransformRules::KeepWorldTransform);
					CachedAttachParent->AttachChildren.Remove(ChildToPromote);

					Index = FMath::Clamp<int32>(Index, 0, AttachSiblings.Num());
					CachedAttachParent->AttachChildren.Insert(ChildToPromote, Index);
				}
			}

			// Detach child nodes from the node that's being removed and re-attach them to the child that's being promoted
			TArray<USceneComponent*> AttachChildrenLocalCopy(AttachedChildren);
			for (USceneComponent* Child : AttachChildrenLocalCopy)
			{
				check(Child != nullptr);

				// Note: This will internally call Modify(), so we don't need to call it here
				Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
				if (Child != ChildToPromote)
				{
					Child->AttachToComponent(ChildToPromote, FAttachmentTransformRules::KeepWorldTransform);
				}
			}
		}
	}
	Super::DestroyComponent(bPromoteChildren);
}

void USceneComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

#if WITH_EDITORONLY_DATA
	if (SpriteComponent)
	{
		SpriteComponent->DestroyComponent();
	}
#endif

	ScopedMovementStack.Reset();

	// If we're just destroying for the exit purge don't bother with any of this
	if (!GExitPurge)
	{
		// If we're destroying the hierarchy we only have to make sure that we detach children from other Actor's
		AActor* MyOwner = GetOwner();

		if (bDestroyingHierarchy)
		{
			// We'll lazily determine if we were attached in any way to a component from another Actor so that any of our children
			// can be attached to this anyways as that would have been what ultimately occurred after the entire hierarchy was torn down
			bool bExternalAttachParentDetermined = false;
			USceneComponent* ExternalAttachParent = nullptr;

			int32 ChildCount = AttachChildren.Num();

			// We cache the actual children to put back after the detach process
			TArray<USceneComponent*> CachedChildren;
			CachedChildren.Reserve(ChildCount);

			while (ChildCount > 0)
			{
				USceneComponent* Child = AttachChildren.Last();
				if (Child && Child->GetOwner() != MyOwner)
				{
					if (Child->GetAttachParent())
					{
						if (Child->GetAttachParent() == this)
						{
							if (!bExternalAttachParentDetermined)
							{
								ExternalAttachParent = GetAttachParent();
								while (ExternalAttachParent)
								{
									if (ExternalAttachParent->GetOwner() != MyOwner)
									{
										break;
									}
									ExternalAttachParent = ExternalAttachParent->GetAttachParent();
								}
								bExternalAttachParentDetermined = true;
							}

							bool bNeedsDetach = true;
							if (ExternalAttachParent)
							{
								bNeedsDetach = (Child->AttachToComponent(ExternalAttachParent, FAttachmentTransformRules::KeepWorldTransform) == false);
							}
							if (bNeedsDetach)
							{
								Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
							}
						}
						else
						{
#if WITH_EDITORONLY_DATA
							// If we are in the middle of a transaction it isn't entirely unexpected that an AttachParent/AttachChildren pairing is wrong
							if (!ensure(GIsTransacting))
#endif
							{
								// We've gotten in to a bad state where the Child's AttachParent doesn't jive with the AttachChildren array
								// so instead of crashing, output an error and gracefully handle
								UE_LOG(LogSceneComponent, Error, TEXT("Component '%s' has '%s' in its AttachChildren array, however, '%s' believes it is attached to '%s'"), *GetFullName(), *Child->GetFullName(), *Child->GetFullName(), *Child->GetAttachParent()->GetFullName());
							}
							AttachChildren.Pop(false);
						}
					}
					else 
					{
						// We've gotten in to a bad state where the Child's AttachParent doesn't jive with the AttachChildren array
						// so instead of crashing, gracefully handle and output an error. 
						// We skip outputting the error if something is pending kill because this is likely a undo/redo situation that is not concerning.
						if (!IsPendingKill() && !Child->IsPendingKill())
						{
							UE_LOG(LogSceneComponent, Error, TEXT("Component '%s' has '%s' in its AttachChildren array, however, '%s' believes it is not attached to anything"), *GetFullName(), *Child->GetFullName(), *Child->GetFullName());
						}
						AttachChildren.Pop(false);
					}
					checkf(ChildCount > AttachChildren.Num(), TEXT("AttachChildren count increased while detaching '%s', likely caused by OnAttachmentChanged introducing new children, which could lead to an infinite loop."), *Child->GetName());
				}
				else
				{
					AttachChildren.Pop(false);
					if (Child)
					{
						CachedChildren.Add(Child);
					}
				}
				ChildCount = AttachChildren.Num();
			}
			AttachChildren = MoveTemp(CachedChildren);
		}
		else
		{
			int32 ChildCount = AttachChildren.Num();
			while (ChildCount > 0)
			{
				if (USceneComponent* Child = AttachChildren.Last())
				{
					if (Child->GetAttachParent())
					{
						if (Child->GetAttachParent() == this)
						{
							bool bNeedsDetach = true;
							if (GetAttachParent())
							{
								bNeedsDetach = (Child->AttachToComponent(GetAttachParent(), FAttachmentTransformRules::KeepWorldTransform) == false);
							}
							if (bNeedsDetach)
							{
								Child->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
							}
						}
						else
						{
#if WITH_EDITORONLY_DATA
							// If we are in the middle of a transaction it isn't entirely unexpected that an AttachParent/AttachChildren pairing is wrong
							if (!ensure(GIsTransacting))
#endif
							{
								// We've gotten in to a bad state where the Child's AttachParent doesn't jive with the AttachChildren array
								// so instead of crashing, output an error and gracefully handle
								UE_LOG(LogSceneComponent, Error, TEXT("Component '%s' has '%s' in its AttachChildren array, however, '%s' believes it is attached to '%s'"), *GetFullName(), *Child->GetFullName(), *Child->GetFullName(), *Child->GetAttachParent()->GetFullName());
							}
							AttachChildren.Pop(false);
						}
					}
					else 
					{
						// We've gotten in to a bad state where the Child's AttachParent doesn't jive with the AttachChildren array
						// so instead of crashing, gracefully handle and output an error. 
						// We skip outputting the error if something is pending kill because this is likely a undo/redo situation that is not concerning.
						if (!IsPendingKill() && !Child->IsPendingKill())
						{
							UE_LOG(LogSceneComponent, Error, TEXT("Component '%s' has '%s' in its AttachChildren array, however, '%s' believes it is not attached to anything"), *GetFullName(), *Child->GetFullName(), *Child->GetFullName());
						}
						AttachChildren.Pop(false);
					}
					checkf(ChildCount > AttachChildren.Num(), TEXT("AttachChildren count increased while detaching '%s', likely caused by OnAttachmentChanged introducing new children, which could lead to an infinite loop."), *Child->GetName());
				}
				else
				{
					AttachChildren.Pop(false);
				}
				ChildCount = AttachChildren.Num();
			}
		}

		// Don't bother detaching from our parent if we're destroying the hierarchy, unless we're attached to
		// another Actor's component
		if (GetAttachParent() && (!bDestroyingHierarchy || GetAttachParent()->GetOwner() != MyOwner))
		{
			// Ensure we are detached before destroying
			DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
}

FBoxSphereBounds USceneComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = LocalToWorld.GetLocation();
	NewBounds.BoxExtent = FVector::ZeroVector;
	NewBounds.SphereRadius = 0.f;
	return NewBounds;
}

void USceneComponent::CalcBoundingCylinder(float& CylinderRadius, float& CylinderHalfHeight) const
{
	CylinderRadius = FMath::Sqrt( FMath::Square(Bounds.BoxExtent.X) + FMath::Square(Bounds.BoxExtent.Y) );
	CylinderHalfHeight = Bounds.BoxExtent.Z;
}

void USceneComponent::UpdateBounds()
{
	SCOPE_CYCLE_COUNTER(STAT_ComponentUpdateBounds);

#if WITH_EDITOR
	FBoxSphereBounds OriginalBounds = Bounds; // Save old bounds
#endif

	// if use parent bound if attach parent exists, and the flag is set
	// since parents tick first before child, this should work correctly
	if ( bUseAttachParentBound && GetAttachParent() != nullptr )
	{
		Bounds = GetAttachParent()->Bounds;
	}
	else
	{
		// Calculate new bounds
		Bounds = CalcBounds(GetComponentTransform());
	}


#if ENABLE_NAN_DIAGNOSTIC
	if (Bounds.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("Bounds contains NaN for %s"), *GetPathName());
		Bounds.DiagnosticCheckNaN();
	}
#endif

#if WITH_EDITOR
	// If bounds have changed (in editor), trigger data rebuild
	if ( IsRegistered() && (GetWorld() != nullptr) && !GetWorld()->IsGameWorld() &&
		(OriginalBounds.Origin.Equals(Bounds.Origin) == false || OriginalBounds.BoxExtent.Equals(Bounds.BoxExtent) == false) )
	{
		GEngine->TriggerStreamingDataRebuild();
	}
#endif
}


void USceneComponent::SetRelativeLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	ConditionalUpdateComponentToWorld();
	
#if ENABLE_NAN_DIAGNOSTIC
	const bool bNaN = NewRotation.ContainsNaN();
	if (bNaN)
	{
		logOrEnsureNanError(TEXT("USceneComponent::SetRelativeLocationAndRotation contains NaN is NewRotation. %s "), *GetNameSafe(GetOwner()));
	}
	if (GEnsureOnNANDiagnostic)
	{
		const bool bIsNormalized = NewRotation.IsNormalized();
		if (!bIsNormalized)
		{
			UE_LOG(LogSceneComponent, Warning, TEXT("USceneComponent::SetRelativeLocationAndRotation has unnormalized NewRotation (%s). %s"), *NewRotation.ToString(), *GetNameSafe(GetOwner()));
		}
	}
#else
	const bool bNaN = false;
#endif

	const FTransform DesiredRelTransform((bNaN ? FQuat::Identity : NewRotation), NewLocation);
	const FTransform DesiredWorldTransform = CalcNewComponentToWorld(DesiredRelTransform);
	const FVector DesiredDelta = FTransform::SubtractTranslations(DesiredWorldTransform, GetComponentTransform());

	MoveComponent(DesiredDelta, DesiredWorldTransform.GetRotation(), bSweep, OutSweepHitResult, MOVECOMP_NoFlags, Teleport);
}

// The FRotator version. It could be a simple wrapper to the FQuat version but it tries to avoid FQuat conversion if possible because:
// (a) conversions affect rotation equality tests so that SetRotation() calls with the same FRotator can cause unnecessary updates because we think they are different rotations after normalization.
// (b) conversions are expensive.
void USceneComponent::SetRelativeLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (NewLocation != RelativeLocation)
	{
		// It's possible that NewRotation == RelativeRotation, so check the cache for a Rotator->Quat conversion.
		SetRelativeLocationAndRotation(NewLocation, RelativeRotationCache.RotatorToQuat_ReadOnly(NewRotation), bSweep, OutSweepHitResult, Teleport);
	}
	else if (!NewRotation.Equals(RelativeRotation, SCENECOMPONENT_ROTATOR_TOLERANCE))
	{
		// We know the rotations are different, don't bother with the cache.
		SetRelativeLocationAndRotation(NewLocation, NewRotation.Quaternion(), bSweep, OutSweepHitResult, Teleport);
	}
}

void USceneComponent::SetRelativeRotation(FRotator NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (!NewRotation.Equals(RelativeRotation, SCENECOMPONENT_ROTATOR_TOLERANCE))
	{
		// We know the rotations are different, don't bother with the cache.
		SetRelativeLocationAndRotation(RelativeLocation, NewRotation.Quaternion(), bSweep, OutSweepHitResult, Teleport);
	}
}

void USceneComponent::AddRelativeRotation(const FQuat& DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FQuat CurRelRotQuat = RelativeRotationCache.RotatorToQuat(RelativeRotation);
	const FQuat NewRelRotQuat = DeltaRotation * CurRelRotQuat;
	SetRelativeLocationAndRotation(RelativeLocation, NewRelRotQuat, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddLocalOffset(FVector DeltaLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FQuat CurRelRotQuat = RelativeRotationCache.RotatorToQuat(RelativeRotation);
	const FVector LocalOffset = CurRelRotQuat.RotateVector(DeltaLocation);
	SetRelativeLocationAndRotation(RelativeLocation + LocalOffset, CurRelRotQuat, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddLocalRotation(FRotator DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FQuat CurRelRotQuat = RelativeRotationCache.RotatorToQuat(RelativeRotation);
	const FQuat NewRelRotQuat = CurRelRotQuat * DeltaRotation.Quaternion();
	SetRelativeLocationAndRotation(RelativeLocation, NewRelRotQuat, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddLocalRotation(const FQuat& DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FQuat CurRelRotQuat = RelativeRotationCache.RotatorToQuat(RelativeRotation);
	const FQuat NewRelRotQuat = CurRelRotQuat * DeltaRotation;
	SetRelativeLocationAndRotation(RelativeLocation, NewRelRotQuat, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddLocalTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FTransform RelativeTransform( RelativeRotationCache.RotatorToQuat(RelativeRotation), RelativeLocation, FVector(1,1,1) ); // don't use scaling, so it matches how AddLocalRotation/Offset work
	const FTransform NewRelTransform = DeltaTransform * RelativeTransform;
	SetRelativeTransform(NewRelTransform, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddWorldOffset(FVector DeltaLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FVector NewWorldLocation = DeltaLocation + GetComponentTransform().GetTranslation();
	SetWorldLocation(NewWorldLocation, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddWorldRotation(FRotator DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FQuat NewWorldRotation = DeltaRotation.Quaternion() * GetComponentTransform().GetRotation();
	SetWorldRotation(NewWorldRotation, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddWorldRotation(const FQuat& DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FQuat NewWorldRotation = DeltaRotation * GetComponentTransform().GetRotation();
	SetWorldRotation(NewWorldRotation, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::AddWorldTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	const FTransform& LocalComponentTransform = GetComponentTransform();
	const FQuat NewWorldRotation = DeltaTransform.GetRotation() * LocalComponentTransform.GetRotation();
	const FVector NewWorldLocation = FTransform::AddTranslations(DeltaTransform, LocalComponentTransform);
	SetWorldTransform(FTransform(NewWorldRotation, NewWorldLocation, FVector(1,1,1)),bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::SetRelativeScale3D(FVector NewScale3D)
{
	if( NewScale3D != RelativeScale3D )
	{
		if (NewScale3D.ContainsNaN())
		{
			UE_LOG(LogBlueprint, Warning, TEXT("SetRelativeScale3D : Invalid Scale entered (%s). Resetting to 1.f."), *NewScale3D.ToString());
			NewScale3D = FVector(1.f);
		}
		RelativeScale3D = NewScale3D;		
		UpdateComponentToWorld();
		
		if (IsRegistered())
		{
			if (!IsDeferringMovementUpdates())
			{
				UpdateOverlaps();
			}
			else
			{
				// Invalidate cached overlap state at this location.
				TArray<FOverlapInfo> EmptyOverlaps;
				GetCurrentScopedMovement()->AppendOverlapsAfterMove(EmptyOverlaps, false, false);
			}
		}
	}
}

void USceneComponent::ResetRelativeTransform()
{
	SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	SetRelativeScale3D(FVector(1.f));
}

void USceneComponent::SetRelativeTransform(const FTransform& NewTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	SetRelativeLocationAndRotation(NewTransform.GetTranslation(), NewTransform.GetRotation(), bSweep, OutSweepHitResult, Teleport);
	SetRelativeScale3D(NewTransform.GetScale3D());
}

FTransform USceneComponent::GetRelativeTransform() const
{
	const FTransform RelativeTransform( RelativeRotationCache.RotatorToQuat(RelativeRotation), RelativeLocation, RelativeScale3D );
	return RelativeTransform;
}

void USceneComponent::SetWorldLocation(FVector NewLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	FVector NewRelLocation = NewLocation;

	// If attached to something, transform into local space
	if (GetAttachParent() != nullptr && !bAbsoluteLocation)
	{
		FTransform ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
		NewRelLocation = ParentToWorld.InverseTransformPosition(NewLocation);
	}

	SetRelativeLocation(NewRelLocation, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::SetWorldRotation(const FQuat& NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	FQuat NewRelRotation = NewRotation;

	// If already attached to something, transform into local space
	if (GetAttachParent() != nullptr && !bAbsoluteRotation)
	{
		const FTransform  ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
		// in order to support mirroring, you'll have to use FTransform.GetRelativeTransform
		// because negative SCALE should flip the rotation
		if (FTransform::AnyHasNegativeScale(RelativeScale3D, ParentToWorld.GetScale3D()))
		{	
			FTransform NewTransform = GetComponentTransform();
			// set new desired rotation
			NewTransform.SetRotation(NewRotation);
			// Get relative transform from ParentToWorld
			const FQuat NewRelQuat = NewTransform.GetRelativeTransform(ParentToWorld).GetRotation();
			NewRelRotation = NewRelQuat;
		}
		else
		{
			const FQuat ParentToWorldQuat = ParentToWorld.GetRotation();
			// Quat multiplication works reverse way, make sure you do Parent(-1) * World = Local, not World*Parent(-) = Local (the way matrix does)
			const FQuat NewRelQuat = ParentToWorldQuat.Inverse() * NewRotation;
			NewRelRotation = NewRelQuat;		
		}
	}

	SetRelativeRotation(NewRelRotation, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::SetWorldRotation(FRotator NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (GetAttachParent() == nullptr)
	{
		// No parent, relative == world. Use FRotator version because it can check for rotation change without conversion issues.
		SetRelativeRotation(NewRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else
	{
		SetWorldRotation(NewRotation.Quaternion(), bSweep, OutSweepHitResult, Teleport);
	}
}


void USceneComponent::SetWorldScale3D(FVector NewScale)
{
	FVector NewRelScale = NewScale;

	// If attached to something, transform into local space
	if(GetAttachParent() != nullptr && !bAbsoluteScale)
	{
		FTransform ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
		NewRelScale = NewScale * ParentToWorld.GetSafeScaleReciprocal(ParentToWorld.GetScale3D());
	}

	SetRelativeScale3D(NewRelScale);
}

void USceneComponent::SetWorldTransform(const FTransform& NewTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	// If attached to something, transform into local space
	if (GetAttachParent() != nullptr)
	{
		const FTransform ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
		FTransform RelativeTM = NewTransform.GetRelativeTransform(ParentToWorld);

		// Absolute location, rotation, and scale use the world transform directly.
		if (bAbsoluteLocation)
		{
			RelativeTM.CopyTranslation(NewTransform);
		}

		if (bAbsoluteRotation)
		{
			RelativeTM.CopyRotation(NewTransform);
		}

		if (bAbsoluteScale)
		{
			RelativeTM.CopyScale3D(NewTransform);
		}

		SetRelativeTransform(RelativeTM, bSweep, OutSweepHitResult, Teleport);
	}
	else
	{
		SetRelativeTransform(NewTransform, bSweep, OutSweepHitResult, Teleport);
	}
}

void USceneComponent::SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (GetAttachParent() == nullptr)
	{
		// No parent, relative == world. Use FRotator version because it can check for rotation change without conversion issues.
		SetRelativeLocationAndRotation(NewLocation, NewRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else
	{
		SetWorldLocationAndRotation(NewLocation, NewRotation.Quaternion(), bSweep, OutSweepHitResult, Teleport);
	}
}

void USceneComponent::SetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	// If attached to something, transform into local space
	FQuat NewFinalRotation = NewRotation;
	if (GetAttachParent() != nullptr)
	{
		FTransform ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());

		if (!bAbsoluteLocation)
		{
			NewLocation = ParentToWorld.InverseTransformPosition(NewLocation);
		}

		if (!bAbsoluteRotation)
		{
			// Quat multiplication works reverse way, make sure you do Parent(-1) * World = Local, not World*Parent(-) = Local (the way matrix does)
			FQuat NewRelQuat = ParentToWorld.GetRotation().Inverse() * NewRotation;
			NewFinalRotation = NewRelQuat;
		}
	}

	SetRelativeLocationAndRotation(NewLocation, NewFinalRotation, bSweep, OutSweepHitResult, Teleport);
}

void USceneComponent::SetWorldLocationAndRotationNoPhysics(const FVector& NewLocation, const FRotator& NewRotation)
{
	// If attached to something, transform into local space
	if (GetAttachParent() != nullptr)
	{
		const FTransform ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());

		if(bAbsoluteLocation)
		{
			RelativeLocation = NewLocation;
		}
		else
		{
			RelativeLocation = ParentToWorld.InverseTransformPosition(NewLocation);
		}

		if(bAbsoluteRotation)
		{
			RelativeRotation = NewRotation;
		}
		else
		{
			// Quat multiplication works reverse way, make sure you do Parent(-1) * World = Local, not World*Parent(-) = Local (the way matrix does)
			const FQuat NewRelQuat = ParentToWorld.GetRotation().Inverse() * NewRotation.Quaternion();
			RelativeRotation = RelativeRotationCache.QuatToRotator(NewRelQuat);
		}
	}
	else
	{
		RelativeLocation = NewLocation;
		RelativeRotation = NewRotation;
	}

	UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate);
}

void USceneComponent::SetAbsolute(bool bNewAbsoluteLocation, bool bNewAbsoluteRotation, bool bNewAbsoluteScale)
{
	bAbsoluteLocation = bNewAbsoluteLocation;
	bAbsoluteRotation = bNewAbsoluteRotation;
	bAbsoluteScale = bNewAbsoluteScale;

	UpdateComponentToWorld();
}

FTransform USceneComponent::K2_GetComponentToWorld() const
{
	return GetComponentToWorld();
}

FVector USceneComponent::GetForwardVector() const
{
	return GetComponentTransform().GetUnitAxis( EAxis::X );
}

FVector USceneComponent::GetRightVector() const
{
	return GetComponentTransform().GetUnitAxis( EAxis::Y );
}

FVector USceneComponent::GetUpVector() const
{
	return GetComponentTransform().GetUnitAxis( EAxis::Z );
}

FVector USceneComponent::K2_GetComponentLocation() const
{
	return GetComponentLocation();
}

FRotator USceneComponent::K2_GetComponentRotation() const
{
	return GetComponentRotation();
}

FVector USceneComponent::K2_GetComponentScale() const
{
	return GetComponentScale();
}

void USceneComponent::GetParentComponents(TArray<class USceneComponent*>& Parents) const
{
	Parents.Reset();

	USceneComponent* ParentIterator = GetAttachParent();
	while (ParentIterator != nullptr)
	{
		Parents.Add(ParentIterator);
		ParentIterator = ParentIterator->GetAttachParent();
	}
}

int32 USceneComponent::GetNumChildrenComponents() const
{
	return GetAttachChildren().Num();
}

USceneComponent* USceneComponent::GetChildComponent(int32 ChildIndex) const
{
	if (ChildIndex < 0)
	{
		UE_LOG(LogBlueprint, Log, TEXT("SceneComponent::GetChild called with a negative ChildIndex: %d"), ChildIndex);
		return nullptr;
	}

	const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
	if (ChildIndex >= AttachedChildren.Num())
	{
		UE_LOG(LogBlueprint, Log, TEXT("SceneComponent::GetChild called with an out of range ChildIndex: %d; Number of children is %d."), ChildIndex, AttachedChildren.Num());
		return nullptr;
	}

	return AttachedChildren[ChildIndex];
}

void USceneComponent::GetChildrenComponents(bool bIncludeAllDescendants, TArray<USceneComponent*>& Children) const
{
	Children.Reset();

	if (bIncludeAllDescendants)
	{
		AppendDescendants(Children);
	}
	else
	{
		Children.Append(GetAttachChildren());
	}
}

void USceneComponent::AppendDescendants(TArray<USceneComponent*>& Children) const
{
	const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
	Children.Append(AttachedChildren);

	for (USceneComponent* Child : AttachedChildren)
	{
		if (Child)
		{
			Child->AppendDescendants(Children);
		}
	}
}

void USceneComponent::SetRelativeRotationCache(const FRotationConversionCache& InCache)
{
	if (RelativeRotationCache.GetCachedRotator() != InCache.GetCachedRotator())
	{
		// Before overwriting the rotator cache, ensure there is no pending update on the transform.
		// Otherwise future calls to SetWorldTransform() will first update the cache and invalidate this change.
		ConditionalUpdateComponentToWorld();

		// The use case for setting the RelativeRotationCache is to control which Rotator ends up
		// being assigned to the component when updating its transform from a Quaternion (see InternalSetWorldLocationAndRotation()).
		// Most of the time, ToQuaternion(ToRotator(Quaternion)) == Quaternion but this is not always the case
		// because of floating point precision. When not equal, rerunning a blueprint script generates another
		// rotator, which ends up generating another transform at map load (since it's the rotator that gets serialized).
		// The transform at map load being different than the one after blueprint rescript, the engine invalidates the
		// precomputed lighting (as it is position dependent) when calling ApplyComponentInstanceData()
		RelativeRotationCache = InCache;
	}
}

void USceneComponent::SetupAttachment(class USceneComponent* InParent, FName InSocketName)
{
	if (ensureMsgf(!bRegistered, TEXT("SetupAttachment should only be used to initialize AttachParent and AttachSocketName for a future AttachTo. Once a component is registered you must use AttachTo.")))
	{
		if (ensureMsgf(InParent != this, TEXT("Cannot attach a component to itself.")))
		{
			if (ensureMsgf(InParent == nullptr || !InParent->IsAttachedTo(this), TEXT("Setting up attachment would create a cycle.")))
			{
				if (ensureMsgf(AttachParent == nullptr || !AttachParent->AttachChildren.Contains(this), TEXT("SetupAttachment cannot be used once a component has already had AttachTo used to connect it to a parent.")))
				{
					AttachParent = InParent;
					AttachSocketName = InSocketName;
				}
			}
		}
	}
}

//This function is used for giving AttachTo different bWeldSimulatedBodies default, but only when called from BP
bool USceneComponent::K2_AttachTo(class USceneComponent* InParent, FName InSocketName, EAttachLocation::Type AttachLocationType, bool bWeldSimulatedBodies /*= true*/)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return AttachTo(InParent, InSocketName, AttachLocationType, bWeldSimulatedBodies);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

//This function is used for giving AttachToComponent different bWeldSimulatedBodies default, but only when called from BP
bool USceneComponent::K2_AttachToComponent(USceneComponent* Parent, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule, bool bWeldSimulatedBodies)
{
	return AttachToComponent(Parent, FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies), SocketName);
}

void USceneComponent::ConvertAttachLocation(EAttachLocation::Type InAttachLocation, EAttachmentRule& InOutLocationRule, EAttachmentRule& InOutRotationRule, EAttachmentRule& InOutScaleRule)
{
	switch (InAttachLocation)
	{
	case EAttachLocation::KeepRelativeOffset:
		InOutLocationRule = EAttachmentRule::KeepRelative;
		InOutRotationRule = EAttachmentRule::KeepRelative;
		InOutScaleRule = EAttachmentRule::KeepRelative;
		break;

	case EAttachLocation::KeepWorldPosition:
		InOutLocationRule = EAttachmentRule::KeepWorld;
		InOutRotationRule = EAttachmentRule::KeepWorld;
		InOutScaleRule = EAttachmentRule::KeepWorld;
		break;

	case EAttachLocation::SnapToTarget:
		InOutLocationRule = EAttachmentRule::SnapToTarget;
		InOutRotationRule = EAttachmentRule::SnapToTarget;
		InOutScaleRule = EAttachmentRule::KeepWorld;
		break;

	case EAttachLocation::SnapToTargetIncludingScale:
		InOutLocationRule = EAttachmentRule::SnapToTarget;
		InOutRotationRule = EAttachmentRule::SnapToTarget;
		InOutScaleRule = EAttachmentRule::SnapToTarget;
		break;
	}
}

bool USceneComponent::AttachTo(class USceneComponent* Parent, FName InSocketName, EAttachLocation::Type AttachType /*= EAttachLocation::KeepRelativeOffset */, bool bWeldSimulatedBodies /*= false*/)
{
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepRelative, bWeldSimulatedBodies);
	ConvertAttachLocation(AttachType, AttachmentRules.LocationRule, AttachmentRules.RotationRule, AttachmentRules.ScaleRule);

	return AttachToComponent(Parent, AttachmentRules, InSocketName);
}

bool USceneComponent::AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName SocketName)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (ThreadContext.IsInConstructor > 0)
	{
		// Validate that the use of AttachTo in the constructor is just setting up the attachment and not expecting to be able to do anything else
		ensureMsgf(!AttachmentRules.bWeldSimulatedBodies, TEXT("AttachToComponent when called from a constructor cannot weld simulated bodies. Consider calling SetupAttachment directly instead."));
		ensureMsgf(AttachmentRules.LocationRule == EAttachmentRule::KeepRelative && AttachmentRules.RotationRule == EAttachmentRule::KeepRelative && AttachmentRules.ScaleRule == EAttachmentRule::KeepRelative, TEXT("AttachToComponent when called from a constructor is only setting up attachment and will always be treated as KeepRelative. Consider calling SetupAttachment directly instead."));
		SetupAttachment(Parent, SocketName);
		return true;
	}

	if(Parent != nullptr)
	{
		const bool bSameAttachParentAndSocket = (Parent == GetAttachParent() && SocketName == GetAttachSocketName());
		if (bSameAttachParentAndSocket && Parent->GetAttachChildren().Contains(this))
		{
			// already attached!
			return true;
		}

		if(Parent == this)
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("AttachToSelfWarning", "AttachTo: '{0}' cannot be attached to itself. Aborting."), 
				FText::FromString(GetPathName())));
			return false;
		}

		AActor* MyActor = GetOwner();
		AActor* TheirActor = Parent->GetOwner();

		if (MyActor == TheirActor && MyActor && MyActor->GetRootComponent() == this)
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("AttachToSelfRootWarning", "AttachTo: '{0}' root component cannot be attached to other components in the same actor. Aborting."),
				FText::FromString(GetPathName())));
			return false;
		}

		if(Parent->IsAttachedTo(this))
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("AttachCycleWarning", "AttachTo: '{0}' already attached to '{1}', would form cycle. Aborting."), 
				FText::FromString(Parent->GetPathName()), 
				FText::FromString(GetPathName())));
			return false;
		}

		if(!Parent->CanAttachAsChild(this, SocketName))
		{
			UE_LOG(LogSceneComponent, Warning, TEXT("AttachTo: '%s' will not allow '%s' to be attached as a child."), *Parent->GetPathName(), *GetPathName());
			return false;
		}

		// Don't allow components with static mobility to be attached to non-static parents (except during UCS)
		if(!IsOwnerRunningUserConstructionScript() && Mobility == EComponentMobility::Static && Parent->Mobility != EComponentMobility::Static)
		{
			FString ExtraBlueprintInfo;
#if WITH_EDITORONLY_DATA
			UClass* ParentClass = Parent->GetOuter()->GetClass();
			if (ParentClass->ClassGeneratedBy && ParentClass->ClassGeneratedBy->IsA(UBlueprint::StaticClass())) 
			{ 
				ExtraBlueprintInfo = FString::Printf(TEXT(" (in blueprint \"%s\")"), *ParentClass->ClassGeneratedBy->GetName());  
			}
#endif //WITH_EDITORONLY_DATA
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("NoStaticToDynamicWarning", "AttachTo: '{0}' is not static {1}, cannot attach '{2}' which is static to it. Aborting."), 
				FText::FromString(Parent->GetPathName()), 
				FText::FromString(ExtraBlueprintInfo), 
				FText::FromString(GetPathName())));
			return false;
		}

		// if our template type doesn't match
		if (Parent->IsTemplate() != IsTemplate())
		{
			if (Parent->IsTemplate())
			{
				ensureMsgf(false, TEXT("Template Mismatch during attachment. Attaching instanced component to template component. Parent '%s' Self '%s'"), *Parent->GetName(), *GetName());
			}
			else
			{
				ensureMsgf(false, TEXT("Template Mismatch during attachment. Attaching template component to instanced component. Parent '%s' Self '%s'"), *Parent->GetName(), *GetName());
			}
			return false;
		}

		// Don't call UpdateOverlaps() when detaching, since we are going to do it anyway after we reattach below.
		// Aside from a perf benefit this also maintains correct behavior when we don't have KeepWorldPosition set.
		const bool bSavedDisableDetachmentUpdateOverlaps = bDisableDetachmentUpdateOverlaps;
		bDisableDetachmentUpdateOverlaps = true;

		// Find out if we're already attached, and save off our position in the array if we are
		int32 LastAttachIndex = INDEX_NONE;
		Parent->GetAttachChildren().Find(this, LastAttachIndex);

		FDetachmentTransformRules DetachmentRules(AttachmentRules, true);

		// Make sure we are detached
		if (bSameAttachParentAndSocket && !IsRegistered() && AttachmentRules.LocationRule == EAttachmentRule::KeepRelative && AttachmentRules.RotationRule == EAttachmentRule::KeepRelative && AttachmentRules.ScaleRule == EAttachmentRule::KeepRelative && LastAttachIndex == INDEX_NONE)
		{
			// No sense detaching from what we are about to attach to during registration, as long as relative position is being maintained.
			//UE_LOG(LogSceneComponent, Verbose, TEXT("[%s] skipping DetachFromParent() for same pending parent [%s] during registration."),
			//	   *GetPathName(GetOwner() ? GetOwner()->GetOuter() : nullptr),
			//	   *GetAttachParent()->GetPathName(GetAttachParent()->GetOwner() ? GetAttachParent()->GetOwner()->GetOuter() : nullptr));
		}
		else
		{
			DetachFromComponent(DetachmentRules);
		}
		
		// Restore detachment update overlaps flag.
		bDisableDetachmentUpdateOverlaps = bSavedDisableDetachmentUpdateOverlaps;

		{
			//This code requires some explaining. Inside the editor we allow user to attach physically simulated objects to other objects. This is done for convenience so that users can group things together in hierarchy.
			//At runtime we must not attach physically simulated objects as it will cause double transform updates, and you should just use a physical constraint if attachment is the desired behavior.
			//Note if bWeldSimulatedBodies = true then they actually want to keep these objects simulating together
			//We must fixup the relative location,rotation,scale as the attachment is no longer valid. Blueprint uses simple construction to try and attach before ComponentToWorld has ever been updated, so we cannot rely on it.
			//As such we must calculate the proper Relative information
			//Also physics state may not be created yet so we use bSimulatePhysics to determine if the object has any intention of being physically simulated
			UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(this);
			FBodyInstance* BI = PrimitiveComponent ? PrimitiveComponent->GetBodyInstance() : nullptr;

			if (BI && BI->bSimulatePhysics && !AttachmentRules.bWeldSimulatedBodies)
			{
				UWorld* MyWorld = GetWorld();
				if (MyWorld && MyWorld->IsGameWorld())
				{
					if (!MyWorld->bIsRunningConstructionScript && (GetOwner()->HasActorBegunPlay() || GetOwner()->IsActorBeginningPlay()))
					{
						//Since the object is physically simulated it can't be the case that it's a child of object A and being attached to object B (at runtime)
						bDisableDetachmentUpdateOverlaps = true;
						DetachFromComponent(DetachmentRules);
						bDisableDetachmentUpdateOverlaps = bSavedDisableDetachmentUpdateOverlaps;

						//User tried to attach but physically based so detach. However, if they provided relative coordinates we should still get the correct position
						if (AttachmentRules.LocationRule == EAttachmentRule::KeepRelative || AttachmentRules.RotationRule == EAttachmentRule::KeepRelative || AttachmentRules.ScaleRule == EAttachmentRule::KeepRelative)
						{
							UpdateComponentToWorldWithParent(Parent, SocketName, EUpdateTransformFlags::None, RelativeRotationCache.RotatorToQuat(RelativeRotation));
							if (AttachmentRules.LocationRule == EAttachmentRule::KeepRelative)
							{
								RelativeLocation = GetComponentLocation();
							}
							if (AttachmentRules.RotationRule == EAttachmentRule::KeepRelative)
							{
								RelativeRotation = GetComponentRotation();
							}
							if (AttachmentRules.ScaleRule == EAttachmentRule::KeepRelative)
							{
								RelativeScale3D = GetComponentScale();
							}
							if (IsRegistered())
							{
								UpdateOverlaps();
							}
						}

						return false;
					}
				}
			}
		}

		// Detach removes all Prerequisite, so will need to add after Detach happens
		PrimaryComponentTick.AddPrerequisite(Parent, Parent->PrimaryComponentTick); // force us to tick after the parent does

		// Save pointer from child to parent
		AttachParent = Parent;
		AttachSocketName = SocketName;

		OnAttachmentChanged();

		// Preserve order of previous attachment if valid (in case we're doing a reattach operation inside a loop that might assume the AttachChildren order won't change)
		if(LastAttachIndex != INDEX_NONE)
		{
			Parent->AttachChildren.Insert(this, LastAttachIndex);
		}
		else
		{
			Parent->AttachChildren.Add(this);
		}
		AddToCluster(Parent, true);

		if (Parent->IsNetSimulating() && !IsNetSimulating())
		{
			Parent->ClientAttachedChildren.AddUnique(this);
		}

		// Now apply attachment rules
		FTransform SocketTransform = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
#if ENABLE_NAN_DIAGNOSTIC
		if (SocketTransform.ContainsNaN())
		{
			logOrEnsureNanError(TEXT("Attaching particle to SocketTransform that contains NaN, earlying out"));
			return false;
		}
#endif
		FTransform RelativeTM = GetComponentTransform().GetRelativeTransform(SocketTransform);
#if ENABLE_NAN_DIAGNOSTIC
		if (RelativeTM.ContainsNaN())
		{
			logOrEnsureNanError(TEXT("Attaching particle to RelativeTM that contains NaN, earlying out"));
			return false;
		}
#endif

		switch (AttachmentRules.LocationRule)
		{
		case EAttachmentRule::KeepRelative:
			// dont do anything, keep relative position the same
			break;
		case EAttachmentRule::KeepWorld:
			if (bAbsoluteLocation)
			{
				RelativeLocation = GetComponentTransform().GetTranslation();
			}
			else
			{
				RelativeLocation = RelativeTM.GetTranslation();
			}
			break;
		case EAttachmentRule::SnapToTarget:
			RelativeLocation = FVector::ZeroVector;
			break;
		}

		switch (AttachmentRules.RotationRule)
		{
		case EAttachmentRule::KeepRelative:
			// dont do anything, keep relative rotation the same
			break;
		case EAttachmentRule::KeepWorld:
			if (bAbsoluteRotation)
			{
				RelativeRotation = GetComponentRotation();
			}
			else
			{
				RelativeRotation = RelativeRotationCache.QuatToRotator(RelativeTM.GetRotation());
			}
			break;
		case EAttachmentRule::SnapToTarget:
			RelativeRotation = FRotator::ZeroRotator;
			break;
		}

		switch (AttachmentRules.ScaleRule)
		{
		case EAttachmentRule::KeepRelative:
			// dont do anything, keep relative scale the same
			break;
		case EAttachmentRule::KeepWorld:
			if (bAbsoluteScale)
			{
				RelativeScale3D = GetComponentTransform().GetScale3D();
			}
			else
			{
				RelativeScale3D = RelativeTM.GetScale3D();
			}
			break;
		case EAttachmentRule::SnapToTarget:
			RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);
			break;
		}

#if WITH_EDITOR
		if(GEngine)
		{
			if(GetOwner() && this == GetOwner()->GetRootComponent())
			{
				GEngine->BroadcastLevelActorAttached(GetOwner(), GetAttachParent()->GetOwner());
			}
		}
#endif

		GetAttachParent()->OnChildAttached(this);

		UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);

		if (AttachmentRules.bWeldSimulatedBodies)
		{
			if (UPrimitiveComponent * PrimitiveComponent = Cast<UPrimitiveComponent>(this))
			{
				if (FBodyInstance* BI = PrimitiveComponent->GetBodyInstance())
				{
					PrimitiveComponent->WeldToImplementation(GetAttachParent(), GetAttachSocketName(), AttachmentRules.bWeldSimulatedBodies);
				}
			}
		}

		// Update overlaps, in case location changed or overlap state depends on attachment.
		if (IsRegistered())
		{
			UpdateOverlaps();
		}

		return true;
	}

	return false;
}

bool USceneComponent::SnapTo(class USceneComponent* Parent, FName InSocketName)
{
	return AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, InSocketName);
}

void USceneComponent::DetachFromParent(bool bMaintainWorldPosition, bool bCallModify)
{
	FDetachmentTransformRules DetachmentRules(EDetachmentRule::KeepRelative, bCallModify);
	if (bMaintainWorldPosition)
	{
		DetachmentRules.LocationRule = EDetachmentRule::KeepWorld;

		// force maintain world rotation and scale for backwards compatibility
		DetachmentRules.RotationRule = EDetachmentRule::KeepWorld;
		DetachmentRules.ScaleRule = EDetachmentRule::KeepWorld;
	}

	DetachFromComponent(DetachmentRules);
}

void USceneComponent::K2_DetachFromComponent(EDetachmentRule LocationRule /*= EDetachmentRule::KeepRelative*/, EDetachmentRule RotationRule /*= EDetachmentRule::KeepRelative*/, EDetachmentRule ScaleRule /*= EDetachmentRule::KeepRelative*/, bool bCallModify /*= true*/)
{
	DetachFromComponent(FDetachmentTransformRules(LocationRule, RotationRule, ScaleRule, bCallModify));
}

void USceneComponent::DetachFromComponent(const FDetachmentTransformRules& DetachmentRules)
{
	if (GetAttachParent() != nullptr)
	{
		AActor* Owner = GetOwner();

		if (UPrimitiveComponent * PrimComp = Cast<UPrimitiveComponent>(this))
		{
			PrimComp->UnWeldFromParent();
		}

		// Make sure parent points to us if we're registered
		ensureMsgf(!bRegistered || GetAttachParent()->GetAttachChildren().Contains(this), TEXT("Attempt to detach SceneComponent '%s' owned by '%s' from AttachParent '%s' while not attached."), *GetName(), (Owner ? *Owner->GetName() : TEXT("Unowned")), *GetAttachParent()->GetName());

		if (DetachmentRules.bCallModify && !HasAnyFlags(RF_Transient))
		{
			Modify();
			GetAttachParent()->Modify();
		}

		PrimaryComponentTick.RemovePrerequisite(GetAttachParent(), GetAttachParent()->PrimaryComponentTick); // no longer required to tick after the attachment

		GetAttachParent()->AttachChildren.Remove(this);
		GetAttachParent()->ClientAttachedChildren.Remove(this);
		GetAttachParent()->OnChildDetached(this);

#if WITH_EDITOR
		if(GEngine)
		{
			if(Owner && this == Owner->GetRootComponent())
			{
				GEngine->BroadcastLevelActorDetached(Owner, GetAttachParent()->GetOwner());
			}
		}
#endif
		AttachParent = nullptr;
		AttachSocketName = NAME_None;

		OnAttachmentChanged();

		// If desired, update RelativeLocation and RelativeRotation to maintain current world position after detachment
		switch (DetachmentRules.LocationRule)
		{
		case EDetachmentRule::KeepRelative:
			break;
		case EDetachmentRule::KeepWorld:
			RelativeLocation = GetComponentTransform().GetTranslation(); // or GetComponentLocation, but worried about custom location...
			break;
		}

		switch (DetachmentRules.RotationRule)
		{
		case EDetachmentRule::KeepRelative:
			break;
		case EDetachmentRule::KeepWorld:
			RelativeRotation = GetComponentRotation();
			break;
		}

		switch (DetachmentRules.ScaleRule)
		{
		case EDetachmentRule::KeepRelative:
			break;
		case EDetachmentRule::KeepWorld:
			RelativeScale3D = GetComponentScale();
			break;
		}

		// calculate transform with new attachment condition
		UpdateComponentToWorld();

		// Update overlaps, in case location changed or overlap state depends on attachment.
		if (IsRegistered() && !bDisableDetachmentUpdateOverlaps)
		{
			UpdateOverlaps();
		}
	}
}

USceneComponent* USceneComponent::GetAttachmentRoot() const
{
	const USceneComponent* Top;
	for( Top=this; Top && Top->GetAttachParent(); Top=Top->GetAttachParent() );
	return const_cast<USceneComponent*>(Top);
}

AActor* USceneComponent::GetAttachmentRootActor() const
{
	const USceneComponent* const AttachmentRootComponent = GetAttachmentRoot();
	return AttachmentRootComponent ? AttachmentRootComponent->GetOwner() : nullptr;
}

bool USceneComponent::IsAttachedTo(class USceneComponent* TestComp) const
{
	if(TestComp != nullptr)
	{
		for( const USceneComponent* Comp=this->GetAttachParent(); Comp!=nullptr; Comp=Comp->GetAttachParent() )
		{
			if( TestComp == Comp )
			{
				return true;
			}
		}
	}
	return false;
}

FSceneComponentInstanceData::FSceneComponentInstanceData(const USceneComponent* SourceComponent)
	: FActorComponentInstanceData(SourceComponent)
{
	AActor* SourceOwner = SourceComponent->GetOwner();
	const TArray<USceneComponent*>& AttachedChildren = SourceComponent->GetAttachChildren();
	for (int32 i = AttachedChildren.Num()-1; i >= 0; --i)
	{
		USceneComponent* SceneComponent = AttachedChildren[i];
		if (SceneComponent && SceneComponent->GetOwner() == SourceOwner && !SceneComponent->IsCreatedByConstructionScript() && !SceneComponent->HasAnyFlags(RF_DefaultSubObject))
		{
			AttachedInstanceComponents.Emplace(SceneComponent, FTransform(SceneComponent->RelativeRotation, SceneComponent->RelativeLocation, SceneComponent->RelativeScale3D));
		}
	}
}

void FSceneComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase)
{
	FActorComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);

	USceneComponent* SceneComponent = CastChecked<USceneComponent>(Component);

	if (ContainsSavedProperties())
	{
		SceneComponent->UpdateComponentToWorld();
	}

	for (const TPair<USceneComponent*, FTransform>& ChildComponentPair : AttachedInstanceComponents)
	{
		USceneComponent* ChildComponent = ChildComponentPair.Key;
		// If the ChildComponent now has a "good" attach parent it was set by the transaction and it means we are undoing/redoing attachment
		// and so the rebuilt component should not take back attachment ownership
		if (ChildComponent && (ChildComponent->GetAttachParent() == nullptr || ChildComponent->GetAttachParent()->IsPendingKill()))
		{
			ChildComponent->RelativeLocation = ChildComponentPair.Value.GetLocation();
			ChildComponent->RelativeRotation = ChildComponentPair.Value.GetRotation().Rotator();
			ChildComponent->RelativeScale3D = ChildComponentPair.Value.GetScale3D();
			ChildComponent->AttachToComponent(SceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}

void FSceneComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	FActorComponentInstanceData::AddReferencedObjects(Collector);
	for (TPair<USceneComponent*, FTransform>& ChildComponentPair : AttachedInstanceComponents)
	{
		Collector.AddReferencedObject(ChildComponentPair.Key);
	}
}

void FSceneComponentInstanceData::FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	for (TPair<USceneComponent*, FTransform>& ChildComponentPair : AttachedInstanceComponents)
	{
		if (UObject* const* NewChildComponent = OldToNewInstanceMap.Find(ChildComponentPair.Key))
		{
			ChildComponentPair.Key = CastChecked<USceneComponent>(*NewChildComponent, ECastCheckedType::NullAllowed);
		}
	}
}

FActorComponentInstanceData* USceneComponent::GetComponentInstanceData() const
{
	FActorComponentInstanceData* InstanceData = nullptr;

	for (USceneComponent* Child : GetAttachChildren())
	{
		if (Child && !Child->IsCreatedByConstructionScript() && !Child->HasAnyFlags(RF_DefaultSubObject))
		{
			InstanceData = new FSceneComponentInstanceData(this);
			break;
		}
	}

	if (InstanceData == nullptr)
	{
		InstanceData = Super::GetComponentInstanceData();
	}

	return InstanceData;
}

void USceneComponent::UpdateChildTransforms(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateChildTransforms);

#if ENABLE_NAN_DIAGNOSTIC
	if (!GetComponentTransform().IsValid())
	{
		logOrEnsureNanError(TEXT("USceneComponent::UpdateChildTransforms found NaN/INF in ComponentToWorld: %s"), *GetComponentTransform().ToString());
	}
#endif

	EUpdateTransformFlags UpdateTransformFlagsFromParent = UpdateTransformFlags | EUpdateTransformFlags::PropagateFromParent;

	for (USceneComponent* ChildComp : GetAttachChildren())
	{
		if (ChildComp != nullptr)
		{
			// Don't update the child if it uses a completely absolute (world-relative) scheme, unless it has never been updated.
			if (!(ChildComp->bAbsoluteLocation && ChildComp->bAbsoluteRotation && ChildComp->bAbsoluteScale) || !ChildComp->bComponentToWorldUpdated)
			{
				ChildComp->UpdateComponentToWorld(UpdateTransformFlagsFromParent, Teleport);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
void USceneComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Copy from deprecated properties
	if(Ar.UE4Ver() < VER_UE4_SCENECOMP_TRANSLATION_TO_LOCATION)
	{
		RelativeLocation = RelativeTranslation_DEPRECATED;
		bAbsoluteLocation = bAbsoluteTranslation_DEPRECATED;
	}
}
#endif


void USceneComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	Super::PostInterpChange(PropertyThatChanged);

	static FName NAME_RelativeScale3D(GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D));

	if (PropertyThatChanged->GetFName() == NAME_RelativeScale3D)
	{
		UpdateComponentToWorld();
	}
}

TArray<FName> USceneComponent::GetAllSocketNames() const
{
	TArray<FComponentSocketDescription> SocketList;
	QuerySupportedSockets(/*out*/ SocketList);

	TArray<FName> ResultList;
	ResultList.Reserve(SocketList.Num());

	for (const FComponentSocketDescription& SocketDesc : SocketList)
	{
		ResultList.Add(SocketDesc.Name);
	}

	return ResultList;
}

FTransform USceneComponent::GetSocketTransform(FName SocketName, ERelativeTransformSpace TransformSpace) const
{
	switch(TransformSpace)
	{
		case RTS_Actor:
		{
			return GetComponentTransform().GetRelativeTransform( GetOwner()->GetTransform() );
			break;
		}
		case RTS_Component:
		case RTS_ParentBoneSpace:
		{
			return FTransform::Identity;
		}
		default:
		{
			return GetComponentTransform();
		}
	}
}

FVector USceneComponent::GetSocketLocation(FName SocketName) const
{
	return GetSocketTransform(SocketName, RTS_World).GetTranslation();
}

FRotator USceneComponent::GetSocketRotation(FName SocketName) const
{
	return GetSocketTransform(SocketName, RTS_World).GetRotation().Rotator();
}

FQuat USceneComponent::GetSocketQuaternion(FName SocketName) const
{
	return GetSocketTransform(SocketName, RTS_World).GetRotation();
}

bool USceneComponent::DoesSocketExist(FName InSocketName) const
{
	return false;
}

bool USceneComponent::HasAnySockets() const
{
	return false;
}

void USceneComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
}

FVector USceneComponent::GetComponentVelocity() const
{
	return ComponentVelocity;
}

void USceneComponent::GetSocketWorldLocationAndRotation(FName InSocketName, FVector& OutLocation, FRotator& OutRotation) const
{
	FTransform const SocketWorldTransform = GetSocketTransform(InSocketName);

	// assemble output
	OutLocation = SocketWorldTransform.GetLocation();
	OutRotation = SocketWorldTransform.Rotator();
}

void USceneComponent::GetSocketWorldLocationAndRotation(FName InSocketName, FVector& OutLocation, FQuat& OutRotation) const
{
	FTransform const SocketWorldTransform = GetSocketTransform(InSocketName);

	// assemble output
	OutLocation = SocketWorldTransform.GetLocation();
	OutRotation = SocketWorldTransform.GetRotation();
}

bool USceneComponent::IsWorldGeometry() const
{
	return false;
}

ECollisionEnabled::Type USceneComponent::GetCollisionEnabled() const
{
	return ECollisionEnabled::NoCollision;
}

const FCollisionResponseContainer& USceneComponent::GetCollisionResponseToChannels() const
{
	return FCollisionResponseContainer::GetDefaultResponseContainer();
}

ECollisionResponse USceneComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	return ECR_Ignore;
}

ECollisionChannel USceneComponent::GetCollisionObjectType() const
{
	return ECC_WorldDynamic;
}

ECollisionResponse USceneComponent::GetCollisionResponseToComponent(class USceneComponent* OtherComponent) const
{
	// Ignore if no component, or either component has no collision
	if(OtherComponent == nullptr || GetCollisionEnabled() == ECollisionEnabled::NoCollision || OtherComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return ECR_Ignore;
	}

	ECollisionResponse OutResponse;
	ECollisionChannel MyCollisionObjectType = GetCollisionObjectType();
	ECollisionChannel OtherCollisionObjectType = OtherComponent->GetCollisionObjectType();

	/**
	 * We decide minimum of behavior of both will decide the resulting response
	 * If A wants to block B, but B wants to touch A, touch will be the result of this collision
	 * However if A is static, then we don't care about B's response to A (static) but A's response to B overrides the verdict
	 * Vice versa, if B is static, we don't care about A's response to static but B's response to A will override the verdict
	 * To make this work, if MyCollisionObjectType is static, set OtherResponse to be ECR_Block, so to be ignored at the end
	 **/
	ECollisionResponse MyResponse = GetCollisionResponseToChannel(OtherCollisionObjectType);
	ECollisionResponse OtherResponse = OtherComponent->GetCollisionResponseToChannel(MyCollisionObjectType);

	OutResponse = FMath::Min<ECollisionResponse>(MyResponse, OtherResponse);


	return OutResponse;
}

void USceneComponent::SetMobility(EComponentMobility::Type NewMobility)
{
	if (NewMobility != Mobility)
	{
		FComponentReregisterContext ReregisterContext(this);
		Mobility = NewMobility;

		if (Mobility == EComponentMobility::Movable)	//if we're now movable all children should be updated as having static children is invalid
		{
			for (USceneComponent* ChildComponent : GetAttachChildren())
			{
				if (ChildComponent)
				{
					ChildComponent->SetMobility(NewMobility);
				}
			}
		}
	}
}

bool USceneComponent::IsSimulatingPhysics(FName BoneName) const
{
	return false;
}

bool USceneComponent::IsAnySimulatingPhysics() const
{
	return IsSimulatingPhysics();
}

APhysicsVolume* USceneComponent::GetPhysicsVolume() const
{
	if (PhysicsVolume.IsValid())
	{
		return PhysicsVolume.Get();
	}
	else if (const UWorld* MyWorld = GetWorld())
	{
		return MyWorld->GetDefaultPhysicsVolume();
	}

	return nullptr;
}

void USceneComponent::UpdatePhysicsVolume( bool bTriggerNotifiers )
{
	if ( bShouldUpdatePhysicsVolume && !IsPendingKill() )
	{
		if (UWorld* MyWorld = GetWorld())
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdatePhysicsVolume);

			APhysicsVolume* NewVolume = MyWorld->GetDefaultPhysicsVolume();
			// Avoid doing anything if there are no other physics volumes in the world.
			if (MyWorld->GetNonDefaultPhysicsVolumeCount() > 0)
			{
				// Avoid a full overlap query if we can do some quick bounds tests against the volumes.
				static uint32 MaxVolumesToCheck = 100;
				uint32 VolumeIndex = 0;
				bool bAnyPotentialOverlap = false;
				for (auto VolumeIter = MyWorld->GetNonDefaultPhysicsVolumeIterator(); VolumeIter && !bAnyPotentialOverlap; ++VolumeIter, ++VolumeIndex)
				{
					const APhysicsVolume* Volume = VolumeIter->Get();
					if (Volume != nullptr)
					{
						const USceneComponent* VolumeRoot = Volume->GetRootComponent();
						if (VolumeRoot)
						{
							if (FBoxSphereBounds::SpheresIntersect(VolumeRoot->Bounds, Bounds))
							{
								if (FBoxSphereBounds::BoxesIntersect(VolumeRoot->Bounds, Bounds))
								{
									bAnyPotentialOverlap = true;
								}
							}
						}
					}

					// Bail if too many volumes. Later we'll probably convert to using an octree so this wouldn't be a concern.
					if (VolumeIndex >= MaxVolumesToCheck)
					{
						bAnyPotentialOverlap = true;
						break;
					}
				}

				if (bAnyPotentialOverlap)
				{
					// check for all volumes that overlap the component
					TArray<FOverlapResult> Hits;
					FComponentQueryParams Params(SCENE_QUERY_STAT(UpdatePhysicsVolume),  GetOwner());

					bool bOverlappedOrigin = false;
					const UPrimitiveComponent* SelfAsPrimitive = Cast<UPrimitiveComponent>(this);
					if (SelfAsPrimitive)
					{
						MyWorld->ComponentOverlapMultiByChannel(Hits, SelfAsPrimitive, GetComponentLocation(), GetComponentQuat(), GetCollisionObjectType(), Params);
					}
					else
					{
						bOverlappedOrigin = true;
						MyWorld->OverlapMultiByChannel(Hits, GetComponentLocation(), FQuat::Identity, GetCollisionObjectType(), FCollisionShape::MakeSphere(0.f), Params);
					}

					for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
					{
						const FOverlapResult& Link = Hits[HitIdx];
						APhysicsVolume* const V = Cast<APhysicsVolume>(Link.GetActor());
						if (V && (V->Priority > NewVolume->Priority))
						{
							if (bOverlappedOrigin || V->IsOverlapInVolume(*this))
							{
								NewVolume = V;
							}
						}
					}
				}
			}

			if (PhysicsVolume != NewVolume)
			{
				SetPhysicsVolume(NewVolume, bTriggerNotifiers);
			}
		}
	}
}

void USceneComponent::SetPhysicsVolume( APhysicsVolume * NewVolume,  bool bTriggerNotifiers )
{
	// Owner can be NULL
	// The Notifier can be triggered with NULL Actor
	// Still the delegate should be still called
	if( bTriggerNotifiers )
	{
		if( NewVolume != PhysicsVolume )
		{
			AActor *A = GetOwner();
			if( PhysicsVolume.IsValid() )
			{
				PhysicsVolume->ActorLeavingVolume(A);
			}
			PhysicsVolumeChangedDelegate.Broadcast(NewVolume);
			PhysicsVolume = NewVolume;
			if( PhysicsVolume.IsValid() )
			{
				PhysicsVolume->ActorEnteredVolume(A);
			}
		}
	}
	else
	{
		PhysicsVolume = NewVolume;
	}
}

void USceneComponent::BeginDestroy()
{
	PhysicsVolumeChangedDelegate.Clear();

	Super::BeginDestroy();
}

bool USceneComponent::InternalSetWorldLocationAndRotation(FVector NewLocation, const FQuat& RotationQuat, bool bNoPhysics, ETeleportType Teleport)
{
	checkSlow(bComponentToWorldUpdated);
	FQuat NewRotationQuat(RotationQuat);

#if ENABLE_NAN_DIAGNOSTIC
	if (NewRotationQuat.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("USceneComponent:InternalSetWorldLocationAndRotation found NaN in NewRotationQuat: %s"), *NewRotationQuat.ToString());
		NewRotationQuat = FQuat::Identity;
	}
#endif

	// If attached to something, transform into local space
	if (GetAttachParent() != nullptr)
	{
		FTransform const ParentToWorld = GetAttachParent()->GetSocketTransform(GetAttachSocketName());
		// in order to support mirroring, you'll have to use FTransform.GetrelativeTransform
		// because negative scale should flip the rotation
		if (FTransform::AnyHasNegativeScale(RelativeScale3D, ParentToWorld.GetScale3D()))
		{
			FTransform const WorldTransform = FTransform(RotationQuat, NewLocation, RelativeScale3D*ParentToWorld.GetScale3D());
			FTransform const RelativeTransform = WorldTransform.GetRelativeTransform(ParentToWorld);

			if (!bAbsoluteLocation)
			{
				NewLocation = RelativeTransform.GetLocation();
			}

			if (!bAbsoluteRotation)
			{
				NewRotationQuat = RelativeTransform.GetRotation();
			}
		}
		else
		{
			if (!bAbsoluteLocation)
			{
				NewLocation = ParentToWorld.InverseTransformPosition(NewLocation);
			}

			if (!bAbsoluteRotation)
			{
				// Quat multiplication works reverse way, make sure you do Parent(-1) * World = Local, not World*Parent(-) = Local (the way matrix does)
				NewRotationQuat = ParentToWorld.GetRotation().Inverse() * NewRotationQuat;
			}
		}
	}

	const FRotator NewRelativeRotation = RelativeRotationCache.QuatToRotator_ReadOnly(NewRotationQuat);
	if (!NewLocation.Equals(RelativeLocation) || !NewRelativeRotation.Equals(RelativeRotation))
	{
		RelativeLocation = NewLocation;

		// Here it is important to compute the quaternion from the rotator and not the opposite.
		// In some cases, similar quaternions generate the same rotator, which create issues.
		// When the component is loaded, the rotator is used to generate the quaternion, which
		// is then used to compute the ComponentToWorld matrix. When running a blueprint script,  
		// it is required to generate that same ComponentToWorld otherwise the FComponentInstanceDataCache
		// might fail to apply to the relevant component. In order to have the exact same transform
		// we must enforce the quaternion to come from the rotator (as in load)
		RelativeRotation = NewRelativeRotation;
		RelativeRotationCache.RotatorToQuat(NewRelativeRotation);

#if ENABLE_NAN_DIAGNOSTIC
		if (RelativeRotation.ContainsNaN())
		{
			logOrEnsureNanError(TEXT("USceneComponent:InternalSetWorldLocationAndRotation found NaN in RelativeRotation: %s"), *RelativeRotation.ToString());
			RelativeRotation = FRotator::ZeroRotator;
		}
#endif
		UpdateComponentToWorldWithParent(GetAttachParent(),GetAttachSocketName(), SkipPhysicsToEnum(bNoPhysics), RelativeRotationCache.GetCachedQuat(), Teleport);

		// we need to call this even if this component itself is not navigation relevant
		// checking ShouldUpdateNavOctreeOnComponentChange here is an optimization for static navigation users
		if (UNavigationSystem::ShouldUpdateNavOctreeOnComponentChange())
		{
			PostUpdateNavigationData();
		}

		return true;
	}

	return false;
}

void USceneComponent::UpdateOverlaps(TArray<FOverlapInfo> const* PendingOverlaps, bool bDoNotifies, const TArray<FOverlapInfo>* OverlapsAtEndLocation)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateOverlaps); 

	if (IsDeferringMovementUpdates())
	{
		GetCurrentScopedMovement()->ForceOverlapUpdate();
		return;
	}
	
	// SceneComponent has no physical representation, so no overlaps to test for/
	// But, we need to test down the attachment chain since there might be PrimitiveComponents below.
	TInlineComponentArray<USceneComponent*> AttachedChildren;
	AttachedChildren.Append(GetAttachChildren());
	for (USceneComponent* ChildComponent : AttachedChildren)
	{
		if (ChildComponent)
		{
			// Do not pass on OverlapsAtEndLocation, it only applied to this component.
			ChildComponent->UpdateOverlaps(nullptr, bDoNotifies);
		}
	}

	if (bShouldUpdatePhysicsVolume)
	{
		UpdatePhysicsVolume(bDoNotifies);
	}
}

bool USceneComponent::CheckStaticMobilityAndWarn(const FText& ActionText) const
{
	// make sure mobility is movable, otherwise you shouldn't try to move
	if (Mobility != EComponentMobility::Movable && IsRegistered())
	{
		if (UWorld * World = GetWorld())
		{
			if (World->IsGameWorld() && World->bIsWorldInitialized && !IsOwnerRunningUserConstructionScript())
			{
				AActor* MyOwner = GetOwner();
				if (MyOwner && MyOwner->IsActorInitialized())
				{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					FMessageLog("PIE").Warning(FText::Format(LOCTEXT("InvalidMustBeMovable", "Mobility of {0} : {1} has to be 'Movable' if you'd like to {2}. "),
						FText::FromString(GetPathNameSafe(GetOwner())), FText::FromString(GetName()), ActionText));
#endif
					return true;
				}
			}
		}
	}

	return false;
}


// FRotator version. This could be a simple wrapper to the FQuat version, but in the case of no significant change in location or rotation (as FRotator),
// we avoid passing through to the FQuat version because conversion can generate a false negative for the rotation equality comparison done using a strict tolerance.
bool USceneComponent::MoveComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult* Hit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	if (GetAttachParent() == nullptr)
	{
		if (Delta.IsZero() && NewRotation.Equals(RelativeRotation, SCENECOMPONENT_ROTATOR_TOLERANCE))
		{
			if (Hit)
			{
				Hit->Init();
			}
			return true;
		}

		return MoveComponentImpl(Delta, RelativeRotationCache.RotatorToQuat_ReadOnly(NewRotation), bSweep, Hit, MoveFlags, Teleport);
	}

	return MoveComponentImpl(Delta, NewRotation.Quaternion(), bSweep, Hit, MoveFlags, Teleport);
}


bool USceneComponent::MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveComponentSceneComponentTime);

	// static things can move before they are registered (e.g. immediately after streaming), but not after.
	if (IsPendingKill() || CheckStaticMobilityAndWarn(SceneComponentStatics::MobilityWarnText))
	{
		if (OutHit)
		{
			*OutHit = FHitResult();
		}
		return false;
	}

	// Fill in optional output param. SceneComponent doesn't sweep, so this is just an empty result.
	if (OutHit)
	{
		*OutHit = FHitResult(1.f);
	}

	ConditionalUpdateComponentToWorld();

	// early out for zero case
	if( Delta.IsZero() )
	{
		// Skip if no vector or rotation.
		if (NewRotation.Equals(GetComponentTransform().GetRotation(), SCENECOMPONENT_QUAT_TOLERANCE))
		{
			return true;
		}
	}

	// just teleport, sweep is supported for PrimitiveComponents. This will update child components as well.
	const bool bMoved = InternalSetWorldLocationAndRotation(GetComponentLocation() + Delta, NewRotation, false, Teleport);

	// Only update overlaps if not deferring updates within a scope
	if (bMoved && !IsDeferringMovementUpdates())
	{
		// need to update overlap detection in case PrimitiveComponents are attached.
		UpdateOverlaps();
	}

	return true;
}

bool USceneComponent::IsVisibleInEditor() const
{
	// in editor, we only check bVisible
	return bVisible;
}

bool USceneComponent::ShouldRender() const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

#if !UE_BUILD_SHIPPING
	// If we want to create render state even for hidden components, return true here
	if (World && World->bCreateRenderStateForHiddenComponents)
	{
		return true;
	}
#endif // !UE_BUILD_SHIPPING

	if (Owner)
	{
		if (UChildActorComponent* ParentComponent = Owner->GetParentComponent())
		{
			if (!ParentComponent->ShouldRender())
			{
				return false;
			}
		}
	}
	
	const bool bShowInEditor = 
#if WITH_EDITOR
		GIsEditor ? (!Owner || !Owner->IsHiddenEd()) : false;
#else
		false;
#endif
	const bool bInGameWorld = World && World->UsesGameHiddenFlags();

	const bool bShowInGame = IsVisible() && (!Owner || !Owner->bHidden);
	return ((bInGameWorld && bShowInGame) || (!bInGameWorld && bShowInEditor)) && bVisible == true;
}

bool USceneComponent::CanEverRender() const
{
	AActor* Owner = GetOwner();

	if (Owner)
	{
		if (UChildActorComponent* ParentComponent = Owner->GetParentComponent())
		{
			if (!ParentComponent->CanEverRender())
			{
				return false;
			}
		}
	}

	const bool bShowInEditor =
#if WITH_EDITOR
		GIsEditor ? (!Owner || !Owner->IsHiddenEd()) : false;
#else
		false;
#endif
	UWorld *World = GetWorld();
	const bool bInGameWorld = World && World->UsesGameHiddenFlags();

	const bool bShowInGame = (!Owner || !Owner->bHidden);
	return ((bInGameWorld && bShowInGame) || (!bInGameWorld && bShowInEditor));
}

bool USceneComponent::ShouldComponentAddToScene() const
{
	// If the detail mode setting allows it, add it to the scene.
	return DetailMode <= GetCachedScalabilityCVars().DetailMode;
}

bool USceneComponent::IsVisible() const
{
	// if hidden in game, nothing to do
	if (bHiddenInGame)
	{
		return false;
	}
	
	return ( bVisible && (!CachedLevelCollection || CachedLevelCollection->IsVisible()) ); 
}

void USceneComponent::OnVisibilityChanged()
{
	MarkRenderStateDirty();
}

void USceneComponent::SetVisibility(const bool bNewVisibility, const USceneComponent::EVisibilityPropagation PropagateToChildren)
{
	bool bRecurseChildren = (PropagateToChildren == EVisibilityPropagation::Propagate);
	if ( bNewVisibility != bVisible )
	{
		bRecurseChildren = bRecurseChildren || (PropagateToChildren == EVisibilityPropagation::DirtyOnly);
		bVisible = bNewVisibility;
		OnVisibilityChanged();
	}

	const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
	if (bRecurseChildren && AttachedChildren.Num() > 0)
	{
		// fully traverse down the attachment tree
		// we do it entirely inline here instead of recursing in case a primitivecomponent is a child of a non-primitivecomponent
		TInlineComponentArray<USceneComponent*, NumInlinedActorComponents> ComponentStack;

		// prime the pump
		ComponentStack.Append(AttachedChildren);

		while (ComponentStack.Num() > 0)
		{
			USceneComponent* const CurrentComp = ComponentStack.Pop(/*bAllowShrinking=*/ false);
			if (CurrentComp)
			{
				ComponentStack.Append(CurrentComp->GetAttachChildren());

				if (PropagateToChildren == EVisibilityPropagation::Propagate)
				{
					CurrentComp->SetVisibility(bNewVisibility, EVisibilityPropagation::NoPropagation);
				}

				// Render state must be dirtied if any parent component's visibility has changed. Since we can't easily track whether 
				// any parent in the hierarchy was dirtied, we have to mark dirty always.
				CurrentComp->MarkRenderStateDirty();
			}
		}
	}
}

void USceneComponent::OnHiddenInGameChanged()
{
	MarkRenderStateDirty();
}

void USceneComponent::SetHiddenInGame(const bool bNewHiddenGame, const USceneComponent::EVisibilityPropagation PropagateToChildren)
{
	bool bRecurseChildren = (PropagateToChildren == EVisibilityPropagation::Propagate);
	if ( bNewHiddenGame != bHiddenInGame )
	{
		bRecurseChildren = bRecurseChildren || (PropagateToChildren == EVisibilityPropagation::DirtyOnly);
		bHiddenInGame = bNewHiddenGame;
		OnHiddenInGameChanged();
	}

	const TArray<USceneComponent*>& AttachedChildren = GetAttachChildren();
	if (bRecurseChildren && AttachedChildren.Num() > 0)
	{
		// fully traverse down the attachment tree
		// we do it entirely inline here instead of recursing in case a primitivecomponent is a child of a non-primitivecomponent
		TInlineComponentArray<USceneComponent*, NumInlinedActorComponents> ComponentStack;

		// prime the pump
		ComponentStack.Append(AttachedChildren);

		while (ComponentStack.Num() > 0)
		{
			USceneComponent* const CurrentComp = ComponentStack.Pop(/*bAllowShrinking=*/ false);
			if (CurrentComp)
			{
				ComponentStack.Append(CurrentComp->GetAttachChildren());

				if (PropagateToChildren == EVisibilityPropagation::Propagate)
				{
					CurrentComp->SetHiddenInGame(bNewHiddenGame, EVisibilityPropagation::NoPropagation);
				}

				// Render state must be dirtied if any parent component's visibility has changed. Since we can't easily track whether 
				// any parent in the hierarchy was dirtied, we have to mark dirty always.
				CurrentComp->MarkRenderStateDirty();
			}
		}
	}
}

void USceneComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	
	// Calculate current ComponentToWorld transform
	// We do this because at level load/duplication ComponentToWorld is uninitialized
	{
		const FTransform RelativeTransform(RelativeRotationCache.RotatorToQuat(RelativeRotation), RelativeLocation, RelativeScale3D);
		ComponentToWorld = CalcNewComponentToWorld(RelativeTransform);
	}

	// Update bounds
	Bounds.Origin+= InOffset;

	// Update component location
	if (GetAttachParent() == nullptr || bAbsoluteLocation)
	{
		RelativeLocation = GetComponentLocation() + InOffset;
		
		// Calculate the new ComponentToWorld transform
		const FTransform RelativeTransform(RelativeRotationCache.RotatorToQuat(RelativeRotation), RelativeLocation, RelativeScale3D);
		ComponentToWorld = CalcNewComponentToWorld(RelativeTransform);
	}

	// Physics move is skipped if physics state is not created or physics scene supports origin shifting
	// We still need to send transform to physics scene to "transform back" actors which should ignore origin shifting
	// (such actors receive Zero offset)
	const bool bSkipPhysicsTransform = (!bPhysicsStateCreated || (bWorldShift && FPhysScene::SupportsOriginShifting() && !InOffset.IsZero()));
	OnUpdateTransform(SkipPhysicsToEnum(bSkipPhysicsTransform));
	
	// We still need to send transform to RT to "transform back" primitives which should ignore origin shifting
	// (such primitives receive Zero offset)
	if (!bWorldShift || InOffset.IsZero())
	{
		MarkRenderTransformDirty();
	}

	// Update physics volume if desired	
	if (bShouldUpdatePhysicsVolume && !bWorldShift)
	{
		UpdatePhysicsVolume(true);
	}

	// Update children
	for (USceneComponent* ChildComp : GetAttachChildren())
	{
		if(ChildComp != nullptr)
		{
			ChildComp->ApplyWorldOffset(InOffset, bWorldShift);
		}
	}
}

FBoxSphereBounds USceneComponent::GetPlacementExtent() const
{
	return CalcBounds( FTransform::Identity );
}

void USceneComponent::OnRep_Transform()
{
	bNetUpdateTransform = true;
}

void USceneComponent::OnRep_AttachParent()
{
	bNetUpdateAttachment = true;
}

void USceneComponent::OnRep_AttachSocketName()
{
	bNetUpdateAttachment = true;
}

void USceneComponent::OnRep_AttachChildren()
{
	// Because replication of AttachChildren is not atomic with AttachParent of the corresponding component it is
	// entirely possible to get duplicates in the AttachChildren array.  So we have to extract them and the later entry
	// is always the duplicate
	for (int32 SearchIndex = AttachChildren.Num()-1; SearchIndex >= 1; --SearchIndex)
	{
		if (USceneComponent* PossibleDuplicate = AttachChildren[SearchIndex])
		{
			for (int32 DuplicateCheckIndex = SearchIndex - 1; DuplicateCheckIndex >= 0; --DuplicateCheckIndex)
			{
				if (PossibleDuplicate == AttachChildren[DuplicateCheckIndex])
				{
					AttachChildren.RemoveAt(SearchIndex, 1, false);
					break;
				}
			}
		}
	}

	if (ClientAttachedChildren.Num())
	{
		for (USceneComponent* AttachChild : AttachChildren)
		{
			if (AttachChild)
			{
				// Clear out any initially attached components from the client attached list that end up becoming replicated
				ClientAttachedChildren.Remove(AttachChild);
			}
		}

		// When the server replicates the attach children array to the client it will wipe out any client only attachments
		// so we need to fill back in the client attached children
		for (USceneComponent* ClientAttachChild : ClientAttachedChildren)
		{
			if (ClientAttachChild)
			{
				AttachChildren.AddUnique(ClientAttachChild);
			}
		}
	}
}

void USceneComponent::OnRep_Visibility(bool OldValue)
{
	bool ReppedValue = bVisible;
	bVisible = OldValue;
	SetVisibility(ReppedValue);
}

void USceneComponent::PreNetReceive()
{
	Super::PreNetReceive();

	bNetUpdateTransform = false;
	bNetUpdateAttachment = false;
	NetOldAttachSocketName = GetAttachSocketName();
	NetOldAttachParent = GetAttachParent();
}

void USceneComponent::PostNetReceive()
{
	Super::PostNetReceive();

	// If we have no attach parent, attach to parent's root component.
	if (GetAttachParent() == nullptr)
	{
		USceneComponent * ParentRoot = GetOwner()->GetRootComponent();
		if (ParentRoot != this)
		{
			bNetUpdateAttachment = true;
			AttachParent = ParentRoot;
		}
	}
}
	
void USceneComponent::PostRepNotifies()
{
	if (bNetUpdateAttachment)
	{
		Exchange(NetOldAttachParent, AttachParent);
		Exchange(NetOldAttachSocketName, AttachSocketName);
		AttachToComponent(NetOldAttachParent, FAttachmentTransformRules::KeepRelativeTransform, NetOldAttachSocketName);
		bNetUpdateAttachment = false;
	}

	if (bNetUpdateTransform)
	{
		UpdateComponentToWorld(EUpdateTransformFlags::SkipPhysicsUpdate);
		bNetUpdateTransform = false;
	}
}

void USceneComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(USceneComponent, bAbsoluteLocation);
	DOREPLIFETIME(USceneComponent, bAbsoluteRotation);
	DOREPLIFETIME(USceneComponent, bAbsoluteScale);
	DOREPLIFETIME(USceneComponent, bVisible);
	DOREPLIFETIME(USceneComponent, AttachParent);
	DOREPLIFETIME(USceneComponent, AttachChildren);
	DOREPLIFETIME(USceneComponent, AttachSocketName);
	DOREPLIFETIME(USceneComponent, RelativeLocation);
	DOREPLIFETIME(USceneComponent, RelativeRotation);
	DOREPLIFETIME(USceneComponent, RelativeScale3D);
}

#if WITH_EDITOR
bool USceneComponent::CanEditChange( const UProperty* Property ) const
{
	bool bIsEditable = Super::CanEditChange( Property );
	if( bIsEditable && Property != nullptr )
	{
		AActor* Owner = GetOwner();
		if(Owner != nullptr)
		{
			if(Property->GetFName() == TEXT( "RelativeLocation" ) ||
			   Property->GetFName() == TEXT( "RelativeRotation" ) ||
			   Property->GetFName() == TEXT( "RelativeScale3D" ))
			{
				bIsEditable = !Owner->bLockLocation;
			}
		}

		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UActorComponent, bIsEditorOnly))
		{
			const USceneComponent* SceneComponentObject = this;
			while (USceneComponent* AttachedParent = ComponentUtils::GetAttachedParent(SceneComponentObject))
			{
				if (AttachedParent->IsEditorOnly())
				{
					bIsEditable = false;
					break;
				}
				SceneComponentObject = AttachedParent;
			}
		}
	}

	return bIsEditable;
}
#endif


/**
 * FScopedPreventAttachedComponentMove implementation
 */

FScopedPreventAttachedComponentMove::~FScopedPreventAttachedComponentMove()
{
	if (Owner)
	{
		Owner->bAbsoluteLocation = bSavedAbsoluteLocation;
		Owner->bAbsoluteRotation = bSavedAbsoluteRotation;
		Owner->bAbsoluteScale = bSavedAbsoluteScale;

		if (bSavedNonAbsoluteComponent && Owner->GetAttachParent())
		{
			// Need to keep RelativeLocation/Rotation/Scale in sync. ComponentToWorld() will remain correct because child isn't moving.
			const FTransform ParentToWorld = Owner->GetAttachParent()->GetSocketTransform(Owner->GetAttachSocketName());
			const FTransform ChildRelativeTM = Owner->GetComponentTransform().GetRelativeTransform(ParentToWorld);

			if (!bSavedAbsoluteLocation)
			{
				Owner->RelativeLocation = ChildRelativeTM.GetTranslation();
			}
			if (!bSavedAbsoluteRotation)
			{
				Owner->RelativeRotation = ChildRelativeTM.GetRotation().Rotator();
			}
			if (!bSavedAbsoluteScale)
			{
				Owner->RelativeScale3D = ChildRelativeTM.GetScale3D();
			}
		}
	}
}


/**
 * FScopedMovementUpdate implementation
 */

static uint32 s_ScopedWarningCount = 0;

FScopedMovementUpdate::FScopedMovementUpdate( class USceneComponent* Component, EScopedUpdate::Type ScopeBehavior, bool bRequireOverlapsEventFlagToQueueOverlaps )
: Owner(Component)
, OuterDeferredScope(nullptr)
, bDeferUpdates(ScopeBehavior == EScopedUpdate::DeferredUpdates)
, bHasMoved(false)
, bHasTeleported(false)
, bRequireOverlapsEventFlag(bRequireOverlapsEventFlagToQueueOverlaps)
, CurrentOverlapState(EOverlapState::eUseParent)
, FinalOverlapCandidatesIndex(INDEX_NONE)
{
	if (IsValid(Component))
	{
		OuterDeferredScope = Component->GetCurrentScopedMovement();
		InitialTransform = Component->GetComponentToWorld();
		InitialRelativeLocation = Component->RelativeLocation;
		InitialRelativeRotation = Component->RelativeRotation;
		InitialRelativeScale = Component->RelativeScale3D;

		if (ScopeBehavior == EScopedUpdate::ImmediateUpdates)
		{
			// We only allow ScopeUpdateImmediately if there is no outer scope, or if the outer scope is also ScopeUpdateImmediately.
			if (OuterDeferredScope && OuterDeferredScope->bDeferUpdates)
			{
				if (s_ScopedWarningCount < 100 || (GFrameCounter & 31) == 0)
				{
					s_ScopedWarningCount++;
					UE_LOG(LogSceneComponent, Error, TEXT("FScopedMovementUpdate attempting to use immediate updates within deferred scope, will use deferred updates instead."));
				}

				bDeferUpdates = true;
			}
		}			

		if (bDeferUpdates)
		{
			Component->BeginScopedMovementUpdate(*this);
		}
	}
	else
	{
		Owner = nullptr;
	}
}


FScopedMovementUpdate::~FScopedMovementUpdate()
{
	if (bDeferUpdates && IsValid(Owner))
	{
		Owner->EndScopedMovementUpdate(*this);
	}
	Owner = nullptr;
}


bool FScopedMovementUpdate::IsTransformDirty() const
{
	if (IsValid(Owner))
	{
		return !InitialTransform.Equals(Owner->GetComponentToWorld());
	}

	return false;
}


void FScopedMovementUpdate::RevertMove()
{
	USceneComponent* Component = Owner;
	if (IsValid(Component))
	{
		FinalOverlapCandidatesIndex = INDEX_NONE;
		PendingOverlaps.Reset();
		BlockingHits.Reset();
		
		if (IsTransformDirty())
		{
			// Teleport to start
			Component->ComponentToWorld = InitialTransform;
			Component->RelativeLocation = InitialRelativeLocation;
			Component->RelativeRotation = InitialRelativeRotation;
			Component->RelativeScale3D = InitialRelativeScale;

			if (!IsDeferringUpdates())
			{
				Component->PropagateTransformUpdate(true);
				Component->UpdateOverlaps();
			}
		}
	}
	bHasMoved = false;
	CurrentOverlapState = EOverlapState::eUseParent;
}

void FScopedMovementUpdate::AppendOverlapsAfterMove(const TArray<FOverlapInfo>& NewPendingOverlaps, bool bSweep, bool bIncludesOverlapsAtEnd)
{
	bHasMoved = true;
	const bool bWasForcing = (CurrentOverlapState == EOverlapState::eForceUpdate);

	if (bIncludesOverlapsAtEnd)
	{
		CurrentOverlapState = EOverlapState::eIncludesOverlaps;
		if (NewPendingOverlaps.Num())
		{
			FinalOverlapCandidatesIndex = PendingOverlaps.Num();
			PendingOverlaps.Append(NewPendingOverlaps);
		}
		else
		{
			// No new pending overlaps means we're not overlapping anything at the end location.
			FinalOverlapCandidatesIndex = INDEX_NONE;
		}
	}
	else
	{
		// We don't know about the final overlaps in the case of a teleport.
		CurrentOverlapState = EOverlapState::eUnknown;
		FinalOverlapCandidatesIndex = INDEX_NONE;
		PendingOverlaps.Append(NewPendingOverlaps);
	}

	if (bWasForcing)
	{
		CurrentOverlapState = EOverlapState::eForceUpdate;
	}
}


void FScopedMovementUpdate::OnInnerScopeComplete(const FScopedMovementUpdate& InnerScope)
{
	if (IsValid(Owner))
	{
		checkSlow(IsDeferringUpdates());
		checkSlow(InnerScope.IsDeferringUpdates());
		checkSlow(InnerScope.OuterDeferredScope == this);

		// Combine with the next item on the stack.
		if (InnerScope.HasMoved(EHasMovedTransformOption::eTestTransform))
		{
			bHasMoved = true;
			
			if (InnerScope.CurrentOverlapState == EOverlapState::eUseParent)
			{
				// Unchanged, use our own
			}
			else
			{
				// Bubble up from inner scope.
				CurrentOverlapState = InnerScope.CurrentOverlapState;
				if (InnerScope.FinalOverlapCandidatesIndex == INDEX_NONE)
				{
					FinalOverlapCandidatesIndex = INDEX_NONE;
				}
				else
				{
					checkSlow(InnerScope.GetPendingOverlaps().Num() > 0);
					FinalOverlapCandidatesIndex = PendingOverlaps.Num() + InnerScope.FinalOverlapCandidatesIndex;
				}
				PendingOverlaps.Append(InnerScope.GetPendingOverlaps());
				checkSlow(FinalOverlapCandidatesIndex < PendingOverlaps.Num());
			}
		}
		else
		{
			// Don't want to invalidate a parent scope when nothing changed in the child.
			checkSlow(InnerScope.CurrentOverlapState == EOverlapState::eUseParent);
		}

		BlockingHits.Append(InnerScope.GetPendingBlockingHits());
	}	
}

const TArray<FOverlapInfo>* FScopedMovementUpdate::GetOverlapsAtEnd(class UPrimitiveComponent& PrimComponent, TArray<FOverlapInfo>& EndOverlaps, bool bTransformChanged) const
{
	const TArray<FOverlapInfo>* EndOverlapsPtr = nullptr;
	switch (CurrentOverlapState)
	{
		case FScopedMovementUpdate::EOverlapState::eUseParent:
		{
			// Only rotation could have possibly changed
			if (bTransformChanged && PrimComponent.AreSymmetricRotations(InitialTransform.GetRotation(), PrimComponent.GetComponentQuat(), PrimComponent.GetComponentScale()))
			{
				EndOverlapsPtr = PrimComponent.ConvertRotationOverlapsToCurrentOverlaps(EndOverlaps, PrimComponent.GetOverlapInfos());
			}
			else
			{
				// Use current overlaps (unchanged)
				EndOverlapsPtr = &PrimComponent.GetOverlapInfos();
			}
			break;
		}
		case FScopedMovementUpdate::EOverlapState::eUnknown:
		case FScopedMovementUpdate::EOverlapState::eForceUpdate:
		{
			EndOverlapsPtr = nullptr;
			break;
		}
		case FScopedMovementUpdate::EOverlapState::eIncludesOverlaps:
		{
			if (FinalOverlapCandidatesIndex == INDEX_NONE)
			{
				// Overlapping nothing
				EndOverlapsPtr = &EndOverlaps;
			}
			else
			{
				// Fill in EndOverlaps with overlaps valid at the end location.
				const bool bMatchingScale = FTransform::AreScale3DsEqual(InitialTransform, PrimComponent.GetComponentTransform());
				if (bMatchingScale)
				{
					EndOverlapsPtr = PrimComponent.ConvertSweptOverlapsToCurrentOverlaps(
						EndOverlaps, GetPendingOverlaps(), FinalOverlapCandidatesIndex,
						PrimComponent.GetComponentLocation(), PrimComponent.GetComponentQuat());
				}
			}
			break;
		}
		default:
		{
			checkf(false, TEXT("Unknown FScopedMovementUpdate::EOverlapState value"));
			break;
		}
	}

	return EndOverlapsPtr;
}


bool FScopedMovementUpdate::SetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewQuat, bool bNoPhysics /*= false*/, ETeleportType Teleport /*= ETeleportType::None*/)
{
	if (Owner)
	{
		return Owner->InternalSetWorldLocationAndRotation(NewLocation, NewQuat, bNoPhysics, Teleport);
	}
	return false;
}


#if WITH_EDITOR
const int32 USceneComponent::GetNumUncachedStaticLightingInteractions() const
{
	int32 NumUncachedStaticLighting = 0;
	for (USceneComponent* ChildComponent : GetAttachChildren())
	{
		if (ChildComponent)
		{
			NumUncachedStaticLighting += ChildComponent->GetNumUncachedStaticLightingInteractions();
		}
	}
	return NumUncachedStaticLighting;
}
#endif

void USceneComponent::UpdateNavigationData()
{
	SCOPE_CYCLE_COUNTER(STAT_ComponentUpdateNavData);

	if (UNavigationSystem::ShouldUpdateNavOctreeOnComponentChange() && IsRegistered())
	{
		UWorld* MyWorld = GetWorld();
		if ((MyWorld == nullptr) || !MyWorld->IsGameWorld() || !MyWorld->IsNetMode(ENetMode::NM_Client))
		{
			// use propagated component's transform update in editor OR server game with additional navsys check
			UNavigationSystem::UpdateComponentInNavOctree(*this);
		}
	}
}

void USceneComponent::PostUpdateNavigationData()
{
	SCOPE_CYCLE_COUNTER(STAT_ComponentPostUpdateNavData);

	if (UNavigationSystem::ShouldUpdateNavOctreeOnComponentChange() && IsRegistered())
	{
		UWorld* MyWorld = GetWorld();
		if (MyWorld != nullptr && MyWorld->GetNavigationSystem() != nullptr 
			&& (MyWorld->GetNavigationSystem()->ShouldAllowClientSideNavigation() || !MyWorld->IsNetMode(ENetMode::NM_Client)))
		{
			// use propagated component's transform update in editor OR server game with additional navsys check
			UNavigationSystem::UpdateNavOctreeAfterMove(this);
		}
	}
}


// K2 versions of various transform changing operations.
// Note: we pass null for the hit result if not sweeping, for better perf.
// This assumes this K2 function is only used by blueprints, which initializes the param for each function call.

void USceneComponent::K2_SetRelativeLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetRelativeLocationAndRotation(NewLocation, NewRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_SetWorldLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetWorldLocationAndRotation(NewLocation, NewRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_SetRelativeLocation(FVector NewLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetRelativeLocation(NewLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_SetRelativeRotation(FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetRelativeRotation(NewRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_SetRelativeTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetRelativeTransform(NewTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddRelativeLocation(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddRelativeLocation(DeltaLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddRelativeRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddRelativeRotation(DeltaRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddLocalOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddLocalOffset(DeltaLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddLocalRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddLocalRotation(DeltaRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddLocalTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddLocalTransform(DeltaTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_SetWorldLocation(FVector NewLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetWorldLocation(NewLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_SetWorldRotation(FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetWorldRotation(NewRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_SetWorldTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetWorldTransform(NewTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddWorldOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddWorldOffset(DeltaLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddWorldRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddWorldRotation(DeltaRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void USceneComponent::K2_AddWorldTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddWorldTransform(DeltaTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

#undef LOCTEXT_NAMESPACE
