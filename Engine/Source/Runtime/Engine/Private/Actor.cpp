// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/Actor.h"
#include "Serialization/AsyncLoading.h"
#include "EngineDefines.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "UObject/UObjectHash.h"
#include "UObject/PropertyPortFlags.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/LocalPlayer.h"
#include "ContentStreaming.h"
#include "DrawDebugHelpers.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/LevelStreamingPersistent.h"
#include "PhysicsPublic.h"
#include "Logging/MessageLog.h"
#include "Net/UnrealNetwork.h"
#include "Net/RepLayout.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupInst.h"
#include "Engine/Canvas.h"
#include "DisplayDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Engine/DemoNetDriver.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Components/ChildActorComponent.h"
#include "Camera/CameraComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/NetworkObjectList.h"
#include "HAL/LowLevelMemTracker.h"

DEFINE_LOG_CATEGORY(LogActor);

DEFINE_STAT(STAT_GetComponentsTime);
DECLARE_CYCLE_STAT(TEXT("PostActorConstruction"), STAT_PostActorConstruction, STATGROUP_Engine);

#if UE_BUILD_SHIPPING
#define DEBUG_CALLSPACE(Format, ...)
#else
#define DEBUG_CALLSPACE(Format, ...) UE_LOG(LogNet, VeryVerbose, Format, __VA_ARGS__);
#endif

#define LOCTEXT_NAMESPACE "Actor"

FMakeNoiseDelegate AActor::MakeNoiseDelegate = FMakeNoiseDelegate::CreateStatic(&AActor::MakeNoiseImpl);

#if WITH_EDITOR
FUObjectAnnotationSparseBool GSelectedActorAnnotation;
#endif

#if !UE_BUILD_SHIPPING
FOnProcessEvent AActor::ProcessEventDelegate;
#endif

uint32 AActor::BeginPlayCallDepth = 0;

AActor::AActor()
{
	InitializeDefaults();
}


AActor::AActor(const FObjectInitializer& ObjectInitializer)
{
	// Forward to default constructor (we don't use ObjectInitializer for anything, this is for compatibility with inherited classes that call Super( ObjectInitializer )
	InitializeDefaults();
}

void AActor::InitializeDefaults()
{
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	// Default to no tick function, but if we set 'never ticks' to false (so there is a tick function) it is enabled by default
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.SetTickFunctionEnable(false); 

	CustomTimeDilation = 1.0f;

	Role = ROLE_Authority;
	RemoteRole = ROLE_None;
	bReplicates = false;
	NetPriority = 1.0f;
	NetUpdateFrequency = 100.0f;
	MinNetUpdateFrequency = 2.0f;
	bNetLoadOnClient = true;
#if WITH_EDITORONLY_DATA
	bEditable = true;
	bListedInSceneOutliner = true;
	bIsEditorPreviewActor = false;
	bHiddenEdLayer = false;
	bHiddenEdTemporary = false;
	bHiddenEdLevel = false;
	bActorLabelEditable = true;
	SpriteScale = 1.0f;
	bEnableAutoLODGeneration = true;	
	InputConsumeOption_DEPRECATED = ICO_ConsumeBoundKeys;
#endif // WITH_EDITORONLY_DATA
	NetCullDistanceSquared = 225000000.0f;
	NetDriverName = NAME_GameNetDriver;
	NetDormancy = DORM_Awake;
	// will be updated in PostInitProperties
	bActorEnableCollision = true;
	bActorSeamlessTraveled = false;
	bBlockInput = false;
	bCanBeDamaged = true;
	bFindCameraComponentWhenViewTarget = true;
	bAllowReceiveTickEventOnDedicatedServer = true;
	bRelevantForNetworkReplays = true;
	bGenerateOverlapEventsDuringLevelStreaming = false;
	bHasDeferredComponentRegistration = false;
#if WITH_EDITORONLY_DATA
	PivotOffset = FVector::ZeroVector;
#endif
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
}

void FActorTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Target && !Target->IsPendingKillOrUnreachable())
	{
		FScopeCycleCounterUObject ActorScope(Target);
		Target->TickActor(DeltaTime*Target->CustomTimeDilation, TickType, *this);	
	}
}

FString FActorTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[TickActor]");
}

bool AActor::CheckDefaultSubobjectsInternal()
{
	bool Result = Super::CheckDefaultSubobjectsInternal();
	if (Result)
	{
		Result = CheckActorComponents();
	}
	return Result;
}

bool AActor::CheckActorComponents()
{
	DEFINE_LOG_CATEGORY_STATIC(LogCheckComponents, Warning, All);

	bool bResult = true;

	for (UActorComponent* Inner : GetComponents())
	{
		if (!Inner)
		{
			continue;
		}
		if (Inner->IsPendingKill())
		{
			UE_LOG(LogCheckComponents, Warning, TEXT("Component is pending kill. Me = %s, Component = %s"), *this->GetFullName(), *Inner->GetFullName());
		}
		if (Inner->IsTemplate() && !IsTemplate())
		{
			UE_LOG(LogCheckComponents, Error, TEXT("Component is a template but I am not. Me = %s, Component = %s"), *this->GetFullName(), *Inner->GetFullName());
			bResult = false;
		}
		UObject* Archetype = Inner->GetArchetype();
		if (Archetype != Inner->GetClass()->GetDefaultObject())
		{
			if (Archetype != GetClass()->GetDefaultSubobjectByName(Inner->GetFName()))
			{
				UE_LOG(LogCheckComponents, Error, TEXT("Component archetype is not the CDO nor a default subobject of my class. Me = %s, Component = %s, Archetype = %s"), *this->GetFullName(), *Inner->GetFullName(), *Archetype->GetFullName());
				bResult = false;
			}
		}
	}
	for (int32 Index = 0; Index < BlueprintCreatedComponents.Num(); Index++)
	{
		UActorComponent* Inner = BlueprintCreatedComponents[Index];
		if (!Inner)
		{
			continue;
		}
		if (Inner->GetOuter() != this)
		{
			UE_LOG(LogCheckComponents, Error, TEXT("SerializedComponent does not have me as an outer. Me = %s, Component = %s"), *this->GetFullName(), *Inner->GetFullName());
			bResult = false;
		}
		if (Inner->IsPendingKill())
		{
			UE_LOG(LogCheckComponents, Warning, TEXT("SerializedComponent is pending kill. Me = %s, Component = %s"), *this->GetFullName(), *Inner->GetFullName());
		}
		if (Inner->IsTemplate() && !IsTemplate())
		{
			UE_LOG(LogCheckComponents, Error, TEXT("SerializedComponent is a template but I am not. Me = %s, Component = %s"), *this->GetFullName(), *Inner->GetFullName());
			bResult = false;
		}
		UObject* Archetype = Inner->GetArchetype();
		if (Archetype != Inner->GetClass()->GetDefaultObject())
		{
			if (Archetype != GetClass()->GetDefaultSubobjectByName(Inner->GetFName()))
			{
				UE_LOG(LogCheckComponents, Error, TEXT("SerializedComponent archetype is not the CDO nor a default subobject of my class. Me = %s, Component = %s, Archetype = %s"), *this->GetFullName(), *Inner->GetFullName(), *Archetype->GetFullName());
				bResult = false;
			}
		}
	}
	return bResult;
}

void AActor::ResetOwnedComponents()
{
#if WITH_EDITOR
	// Identify any natively-constructed components referenced by properties that either failed to serialize or came in as NULL.
	if(HasAnyFlags(RF_WasLoaded) && NativeConstructedComponentToPropertyMap.Num() > 0)
	{
		for (UActorComponent* Component : OwnedComponents)
		{
			// Only consider native components
			if (Component && Component->CreationMethod == EComponentCreationMethod::Native)
			{
				// Find the property or properties that previously referenced the natively-constructed component.
				TArray<UObjectProperty*> Properties;
				NativeConstructedComponentToPropertyMap.MultiFind(Component->GetFName(), Properties);

				// Determine if the property or properties are no longer valid references (either it got serialized out that way or something failed during load)
				for (UObjectProperty* ObjProp : Properties)
				{
					check(ObjProp != nullptr);
					UActorComponent* ActorComponent = Cast<UActorComponent>(ObjProp->GetObjectPropertyValue_InContainer(this));
					if (ActorComponent == nullptr)
					{
						// Restore the natively-constructed component instance
						ObjProp->SetObjectPropertyValue_InContainer(this, Component);
					}
				}
			}
		}

		// Clear out the mapping as we don't need it anymore
		NativeConstructedComponentToPropertyMap.Empty();
	}
#endif

	OwnedComponents.Reset();
	ReplicatedComponents.Reset();

	ForEachObjectWithOuter(this, [this](UObject* Child)
	{
		UActorComponent* Component = Cast<UActorComponent>(Child);
		if (Component && Component->GetOwner() == this)
		{
			OwnedComponents.Add(Component);

			if (Component->GetIsReplicated())
			{
				ReplicatedComponents.Add(Component);
			}
		}
	}, true, RF_NoFlags, EInternalObjectFlags::PendingKill);
}

void AActor::PostInitProperties()
{
	Super::PostInitProperties();
	RemoteRole = (bReplicates ? ROLE_SimulatedProxy : ROLE_None);

	// Make sure the OwnedComponents list correct.  
	// Under some circumstances sub-object instancing can result in bogus/duplicate entries.
	// This is not necessary for CDOs
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ResetOwnedComponents();
	}
}

bool AActor::CanBeInCluster() const
{
	return bCanBeInCluster;
}

void AActor::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	AActor* This = CastChecked<AActor>(InThis);
	Collector.AddReferencedObjects(This->OwnedComponents);
#if WITH_EDITOR
	if (This->CurrentTransactionAnnotation.IsValid())
	{
		This->CurrentTransactionAnnotation->AddReferencedObjects(Collector);
	}
#endif
	Super::AddReferencedObjects(InThis, Collector);
}

UWorld* AActor::GetWorld() const
{
	// CDO objects do not belong to a world
	// If the actors outer is destroyed or unreachable we are shutting down and the world should be NULL
	if (!HasAnyFlags(RF_ClassDefaultObject) && !GetOuter()->HasAnyFlags(RF_BeginDestroyed) && !GetOuter()->IsUnreachable())
	{
		if (ULevel* Level = GetLevel())
		{
			return Level->OwningWorld;
		}
	}
	return nullptr;
}

FTimerManager& AActor::GetWorldTimerManager() const
{
	return GetWorld()->GetTimerManager();
}

UGameInstance* AActor::GetGameInstance() const
{
	return GetWorld()->GetGameInstance();
}

bool AActor::IsNetStartupActor() const
{
	return bNetStartup;
}

FVector AActor::GetVelocity() const
{
	if ( RootComponent )
	{
		return RootComponent->GetComponentVelocity();
	}

	return FVector::ZeroVector;
}

void AActor::ClearCrossLevelReferences()
{
	// TODO: Change GetOutermost to GetLevel?
	if(RootComponent && GetRootComponent()->GetAttachParent() && (GetOutermost() != GetRootComponent()->GetAttachParent()->GetOutermost()))
	{
		GetRootComponent()->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}
}

bool AActor::TeleportTo( const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck )
{
	SCOPE_CYCLE_COUNTER(STAT_TeleportToTime);

	if(RootComponent == NULL)
	{
		return false;
	}

	UWorld* MyWorld = GetWorld();

	// Can't move non-movable actors during play
	if( (RootComponent->Mobility == EComponentMobility::Static) && MyWorld->AreActorsInitialized() )
	{
		return false;
	}

	if ( bIsATest && (GetActorLocation() == DestLocation) )
	{
		return true;
	}

	FVector const PrevLocation = GetActorLocation();
	FVector NewLocation = DestLocation;
	bool bTeleportSucceeded = true;
	UPrimitiveComponent* ActorPrimComp = Cast<UPrimitiveComponent>(RootComponent);
	if ( ActorPrimComp )
	{
		if (!bNoCheck && (ActorPrimComp->IsQueryCollisionEnabled() || (GetNetMode() != NM_Client)) )
		{
			// Apply the pivot offset to the desired location
			FVector Offset = GetRootComponent()->Bounds.Origin - PrevLocation;
			NewLocation = NewLocation + Offset;

			// check if able to find an acceptable destination for this actor that doesn't embed it in world geometry
			bTeleportSucceeded = MyWorld->FindTeleportSpot(this, NewLocation, DestRotation);
			NewLocation = NewLocation - Offset;
		}

		if (NewLocation.ContainsNaN() || PrevLocation.ContainsNaN())
		{
			bTeleportSucceeded = false;
			UE_LOG(LogActor, Log,  TEXT("Attempted to teleport to NaN"));
		}

		if ( bTeleportSucceeded )
		{
			// check whether this actor unacceptably encroaches on any other actors.
			if ( bIsATest || bNoCheck )
			{
				ActorPrimComp->SetWorldLocationAndRotation(NewLocation, DestRotation);
			}
			else
			{
				FVector const Delta = NewLocation - PrevLocation;
				bTeleportSucceeded = ActorPrimComp->MoveComponent(Delta, DestRotation, false, nullptr, MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
			}
			if( bTeleportSucceeded )
			{
				TeleportSucceeded(bIsATest);
			}
		}
	}
	else if (RootComponent)
	{
		// not a primitivecomponent, just set directly
		GetRootComponent()->SetWorldLocationAndRotation(NewLocation, DestRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	return bTeleportSucceeded; 
}


bool AActor::K2_TeleportTo( FVector DestLocation, FRotator DestRotation )
{
	return TeleportTo(DestLocation, DestRotation, false, false);
}

void AActor::AddTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrimaryActorTick.bCanEverTick && PrerequisiteActor && PrerequisiteActor->PrimaryActorTick.bCanEverTick)
	{
		PrimaryActorTick.AddPrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void AActor::AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrimaryActorTick.bCanEverTick && PrerequisiteComponent && PrerequisiteComponent->PrimaryComponentTick.bCanEverTick)
	{
		PrimaryActorTick.AddPrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

void AActor::RemoveTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrerequisiteActor)
	{
		PrimaryActorTick.RemovePrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void AActor::RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrerequisiteComponent)
	{
		PrimaryActorTick.RemovePrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

bool AActor::GetTickableWhenPaused()
{
	return PrimaryActorTick.bTickEvenWhenPaused;
}

void AActor::SetTickableWhenPaused(bool bTickableWhenPaused)
{
	PrimaryActorTick.bTickEvenWhenPaused = bTickableWhenPaused;
}

void AActor::AddControllingMatineeActor( AMatineeActor& InMatineeActor )
{
	if (RootComponent)
	{
		RootComponent->PrimaryComponentTick.AddPrerequisite(&InMatineeActor, InMatineeActor.PrimaryActorTick);
	}

	PrimaryActorTick.AddPrerequisite(&InMatineeActor, InMatineeActor.PrimaryActorTick);
	ControllingMatineeActors.AddUnique(&InMatineeActor);
}

void AActor::RemoveControllingMatineeActor( AMatineeActor& InMatineeActor )
{
	if (RootComponent)
	{
		RootComponent->PrimaryComponentTick.RemovePrerequisite(&InMatineeActor, InMatineeActor.PrimaryActorTick);
	}

	PrimaryActorTick.RemovePrerequisite(&InMatineeActor, InMatineeActor.PrimaryActorTick);
	ControllingMatineeActors.RemoveSwap(&InMatineeActor);
}

void AActor::BeginDestroy()
{
	ULevel* OwnerLevel = GetLevel();
	UnregisterAllComponents();
	if (OwnerLevel && !OwnerLevel->HasAnyInternalFlags(EInternalObjectFlags::Unreachable))
	{
		OwnerLevel->Actors.RemoveSingleSwap(this, false);
	}
	Super::BeginDestroy();
}

bool AActor::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && DetachFence.IsFenceComplete();
}

void AActor::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	// Prior to load, map natively-constructed component instances for Blueprint-generated class types to any serialized properties that might reference them.
	// We'll use this information post-load to determine if any owned components may not have been serialized through the reference property (i.e. in case the serialized property value ends up being NULL).
	if (Ar.IsLoading()
		&& OwnedComponents.Num() > 0
		&& !(Ar.GetPortFlags() & PPF_Duplicate)
		&& HasAllFlags(RF_WasLoaded|RF_NeedPostLoad))
	{
		if (const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(GetClass()))
		{
			NativeConstructedComponentToPropertyMap.Reset();
			NativeConstructedComponentToPropertyMap.Reserve(OwnedComponents.Num());
			for(TFieldIterator<UObjectProperty> PropertyIt(BPGC, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				UObjectProperty* ObjProp = *PropertyIt;

				// Ignore transient properties since they won't be serialized
				if(!ObjProp->HasAnyPropertyFlags(CPF_Transient))
				{
					UActorComponent* ActorComponent = Cast<UActorComponent>(ObjProp->GetObjectPropertyValue_InContainer(this));
					if(ActorComponent != nullptr && ActorComponent->CreationMethod == EComponentCreationMethod::Native)
					{
						NativeConstructedComponentToPropertyMap.Add(ActorComponent->GetFName(), ObjProp);
					}
				}
			}
		}
	}

	// When duplicating for PIE all components need to be gathered up and duplicated even if there are no other property references to them
	// otherwise we can end up with Attach Parents that do not get redirected to the correct component. However, if there is a transient component
	// we'll let that drop
	if (Ar.GetPortFlags() & PPF_DuplicateForPIE)
	{
		TInlineComponentArray<UActorComponent*> DuplicatingComponents;
		if (Ar.IsSaving())
		{
			DuplicatingComponents.Reserve(OwnedComponents.Num());
			for (UActorComponent* OwnedComponent : OwnedComponents)
			{
				if (OwnedComponent && !OwnedComponent->HasAnyFlags(RF_Transient))
				{
					DuplicatingComponents.Add(OwnedComponent);
				}
			}
		}
		Ar << DuplicatingComponents;
	}
#endif

	Super::Serialize(Ar);
}

void AActor::PostLoad()
{
	Super::PostLoad();

	// add ourselves to our Owner's Children array
	if (Owner != NULL)
	{
		checkSlow(!Owner->Children.Contains(this));
		Owner->Children.Add(this);
	}

	if (GetLinkerUE4Version() < VER_UE4_PRIVATE_REMOTE_ROLE)
	{
		bReplicates = (RemoteRole != ROLE_None);
	}

	// Ensure that this is not set for CDO (there was a case where this might have occurred in an older version when converting actor instances to BPs - see UE-18490)
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		bExchangedRoles = false;
	}

#if WITH_EDITORONLY_DATA
	if (GetLinkerUE4Version() < VER_UE4_CONSUME_INPUT_PER_BIND)
	{
		bBlockInput = (InputConsumeOption_DEPRECATED == ICO_ConsumeAll);
	}

	if (AActor* ParentActor = ParentComponentActor_DEPRECATED.Get())
	{
		TInlineComponentArray<UChildActorComponent*> ParentChildActorComponents(ParentActor);
		for (UChildActorComponent* ChildActorComponent : ParentChildActorComponents)
		{
			if (ChildActorComponent->GetChildActor() == this)
			{
				ParentComponent = ChildActorComponent;
				break;
			}
		}
	}

	if ( GIsEditor )
	{
		// Propagate the hidden at editor startup flag to the transient hidden flag
		bHiddenEdTemporary = bHiddenEd;

		// Check/warning when loading actors in editor. Should never load IsPendingKill() Actors!
		if ( IsPendingKill() )
		{
			UE_LOG(LogActor, Log,  TEXT("Loaded Actor (%s) with IsPendingKill() == true"), *GetName() );
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void AActor::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	USceneComponent* OldRoot = RootComponent;
	USceneComponent* OldRootParent = (OldRoot ? OldRoot->GetAttachParent() : nullptr);
	bool bHadRoot = !!OldRoot;
	FRotator OldRotation;
	FVector OldTranslation;
	FVector OldScale;
	if (bHadRoot)
	{
		OldRotation = OldRoot->RelativeRotation;
		OldTranslation = OldRoot->RelativeLocation;
		OldScale = OldRoot->RelativeScale3D;
	}

	Super::PostLoadSubobjects(OuterInstanceGraph);

	// If this is a Blueprint class, we may need to manually apply default value overrides to some inherited components in a cooked
	// build scenario. This can occur, for example, if we have a nativized Blueprint class somewhere in the class inheritance hierarchy.
	if (FPlatformProperties::RequiresCookedData() && !IsTemplate())
	{
		const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(GetClass());
		if (BPGC != nullptr && BPGC->bHasNativizedParent)
		{
			UBlueprintGeneratedClass::CheckAndApplyComponentTemplateOverrides(this);
		}
	}

	ResetOwnedComponents();

	if (RootComponent && bHadRoot && OldRoot != RootComponent)
	{
		UE_LOG(LogActor, Log, TEXT("Root component has changed, relocating new root component to old position %s->%s"), *OldRoot->GetFullName(), *GetRootComponent()->GetFullName());
		GetRootComponent()->RelativeRotation = OldRotation;
		GetRootComponent()->RelativeLocation = OldTranslation;
		GetRootComponent()->RelativeScale3D = OldScale;
		
		// Migrate any attachment to the new root
		if (OldRoot->GetAttachParent())
		{
			// Users may try to fixup attachment to the root on their own, avoid creating a cycle.
			if (OldRoot->GetAttachParent() != RootComponent)
			{
				RootComponent->SetupAttachment(OldRoot->GetAttachParent());
			}
		}

		// Attach old root to new root, if the user did not do something on their own during construction that differs from the serialized value.
		if (OldRoot->GetAttachParent() == OldRootParent && OldRoot->GetAttachParent() != RootComponent)
		{
			UE_LOG(LogActor, Log, TEXT("--- Attaching old root to new root %s->%s"), *OldRoot->GetFullName(), *GetRootComponent()->GetFullName());
			OldRoot->SetupAttachment(RootComponent);
		}

		// Reset the transform on the old component
		OldRoot->RelativeRotation = FRotator::ZeroRotator;
		OldRoot->RelativeLocation = FVector::ZeroVector;
		OldRoot->RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);
	}
}

void AActor::ProcessEvent(UFunction* Function, void* Parameters)
{
	LLM_SCOPE(ELLMTag::EngineMisc);

	#if WITH_EDITOR
	static const FName CallInEditorMeta(TEXT("CallInEditor"));
	const bool bAllowScriptExecution = GAllowActorScriptExecutionInEditor || Function->GetBoolMetaData(CallInEditorMeta);
	#else
	const bool bAllowScriptExecution = GAllowActorScriptExecutionInEditor;
	#endif
	UWorld* MyWorld = GetWorld();
	if( ((MyWorld && (MyWorld->AreActorsInitialized() || bAllowScriptExecution)) || HasAnyFlags(RF_ClassDefaultObject)) && !IsGarbageCollecting() )
	{
#if !UE_BUILD_SHIPPING
		if (!ProcessEventDelegate.IsBound() || !ProcessEventDelegate.Execute(this, Function, Parameters))
		{
			Super::ProcessEvent(Function, Parameters);
		}
#else
		Super::ProcessEvent(Function, Parameters);
#endif
	}
}

void AActor::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	// Attached components will be shifted by parents, will shift only USceneComponents derived components
	if (RootComponent != nullptr && RootComponent->GetAttachParent() == nullptr)
	{
		RootComponent->ApplyWorldOffset(InOffset, bWorldShift);
	}

	// Shift UActorComponent derived components, but not USceneComponents
 	for (UActorComponent* ActorComponent : GetComponents())
 	{
 		if (IsValid(ActorComponent) && !ActorComponent->IsA<USceneComponent>())
 		{
 			ActorComponent->ApplyWorldOffset(InOffset, bWorldShift);
 		}
 	}
 	
	// Navigation receives update during component registration. World shift needs a separate path to shift all navigation data
	// So this normally should happen only in the editor when user moves visible sub-levels
	if (!bWorldShift && !InOffset.IsZero())
	{
		if (RootComponent != nullptr && RootComponent->IsRegistered())
		{
			UNavigationSystem::UpdateNavOctreeBounds(this);
			UNavigationSystem::UpdateActorAndComponentsInNavOctree(*this);
		}
	}
}

/** Thread safe container for actor related global variables */
class FActorThreadContext : public TThreadSingleton<FActorThreadContext>
{
	friend TThreadSingleton<FActorThreadContext>;

	FActorThreadContext()
		: TestRegisterTickFunctions(nullptr)
	{
	}
public:
	/** Tests tick function registration */
	AActor* TestRegisterTickFunctions;
};

void AActor::RegisterActorTickFunctions(bool bRegister)
{
	check(!IsTemplate());

	if(bRegister)
	{
		if(PrimaryActorTick.bCanEverTick)
		{
			PrimaryActorTick.Target = this;
			PrimaryActorTick.SetTickFunctionEnable(PrimaryActorTick.bStartWithTickEnabled || PrimaryActorTick.IsTickFunctionEnabled());
			PrimaryActorTick.RegisterTickFunction(GetLevel());
		}
	}
	else
	{
		if(PrimaryActorTick.IsTickFunctionRegistered())
		{
			PrimaryActorTick.UnRegisterTickFunction();			
		}
	}

	FActorThreadContext::Get().TestRegisterTickFunctions = this; // we will verify the super call chain is intact. Don't copy and paste this to another actor class!
}

void AActor::RegisterAllActorTickFunctions(bool bRegister, bool bDoComponents)
{
	if(!IsTemplate())
	{
		// Prevent repeated redundant attempts
		if (bTickFunctionsRegistered != bRegister)
		{
			FActorThreadContext& ThreadContext = FActorThreadContext::Get();
			check(ThreadContext.TestRegisterTickFunctions == nullptr);
			RegisterActorTickFunctions(bRegister);
			bTickFunctionsRegistered = bRegister;
			checkf(ThreadContext.TestRegisterTickFunctions == this, TEXT("Failed to route Actor RegisterTickFunctions (%s)"), *GetFullName());
			ThreadContext.TestRegisterTickFunctions = nullptr;
		}

		if (bDoComponents)
		{
			for (UActorComponent* Component : GetComponents())
			{
				if (Component)
				{
					Component->RegisterAllComponentTickFunctions(bRegister);
				}
			}
		}
	}
}

void AActor::SetActorTickEnabled(bool bEnabled)
{
	if (!IsTemplate() && PrimaryActorTick.bCanEverTick)
	{
		PrimaryActorTick.SetTickFunctionEnable(bEnabled);
	}
}

bool AActor::IsActorTickEnabled() const
{
	return PrimaryActorTick.IsTickFunctionEnabled();
}

void AActor::SetActorTickInterval(float TickInterval)
{
	PrimaryActorTick.TickInterval = TickInterval;
}

float AActor::GetActorTickInterval() const
{
	return PrimaryActorTick.TickInterval;
}

bool AActor::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	if (NewOuter)
	{
		RegisterAllActorTickFunctions(false, true); // unregister all tick functions
		UnregisterAllComponents();
	}

	bool bSuccess = Super::Rename( InName, NewOuter, Flags );

	if (NewOuter && NewOuter->IsA<ULevel>())
	{
		UWorld* World = NewOuter->GetWorld();
		if (World && World->bIsWorldInitialized)
		{
			RegisterAllComponents();
		}
		RegisterAllActorTickFunctions(true, true); // register all tick functions
	}
	return bSuccess;
}

UNetConnection* AActor::GetNetConnection() const
{
	return Owner ? Owner->GetNetConnection() : NULL;
}

UPlayer* AActor::GetNetOwningPlayer()
{
	// We can only replicate RPCs to the owning player
	if (Role == ROLE_Authority)
	{
		if (Owner)
		{
			return Owner->GetNetOwningPlayer();
		}
	}
	return NULL;
}

bool AActor::DestroyNetworkActorHandled()
{
	return false;
}

void AActor::TickActor( float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction )
{
	//root of tick hierarchy

	// Non-player update.
	const bool bShouldTick = ((TickType!=LEVELTICK_ViewportsOnly) || ShouldTickIfViewportsOnly());
	if(bShouldTick)
	{
		// If an Actor has been Destroyed or its level has been unloaded don't execute any queued ticks
		if (!IsPendingKill() && GetWorld())
		{
			Tick(DeltaSeconds);	// perform any tick functions unique to an actor subclass
		}
	}
}

void AActor::Tick( float DeltaSeconds )
{
	// Blueprint code outside of the construction script should not run in the editor
	// Allow tick if we are not a dedicated server, or we allow this tick on dedicated servers
	if (GetWorldSettings() != nullptr && (bAllowReceiveTickEventOnDedicatedServer || !IsRunningDedicatedServer()))
	{
		ReceiveTick(DeltaSeconds);
	}


	// Update any latent actions we have for this actor

	// If this tick is skipped on a frame because we've got a TickInterval, our latent actions will be ticked
	// anyway by UWorld::Tick(). Given that, our latent actions don't need to be passed a larger
	// DeltaSeconds to make up the frames that they missed (because they wouldn't have missed any).
	// So pass in the world's DeltaSeconds value rather than our specific DeltaSeconds value.
	UWorld* MyWorld = GetWorld();
	MyWorld->GetLatentActionManager().ProcessLatentActions(this, MyWorld->GetDeltaSeconds());

	if (bAutoDestroyWhenFinished)
	{
		bool bOKToDestroy = true;

		for (UActorComponent* const Comp : GetComponents())
		{
			if (Comp && !Comp->IsReadyForOwnerToAutoDestroy())
			{
				bOKToDestroy = false;
				break;
			}
		}

		// die!
		if (bOKToDestroy)
		{
			Destroy();
		}
	}
}


/** If true, actor is ticked even if TickType==LEVELTICK_ViewportsOnly */
bool AActor::ShouldTickIfViewportsOnly() const
{
	return false;
}

void AActor::PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker )
{
	// Attachment replication gets filled in by GatherCurrentMovement(), but in the case of a detached root we need to trigger remote detachment.
	AttachmentReplication.AttachParent = nullptr;

	if ( bReplicateMovement || (RootComponent && RootComponent->GetAttachParent()) )
	{
		GatherCurrentMovement();
	}

	DOREPLIFETIME_ACTIVE_OVERRIDE( AActor, ReplicatedMovement, bReplicateMovement );

	// Don't need to replicate AttachmentReplication if the root component replicates, because it already handles it.
	DOREPLIFETIME_ACTIVE_OVERRIDE( AActor, AttachmentReplication, RootComponent && !RootComponent->GetIsReplicated() );

	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if (BPClass != NULL)
	{
		BPClass->InstancePreReplication(this, ChangedPropertyTracker);
	}
}

void AActor::CallPreReplication(UNetDriver* NetDriver)
{
	if (NetDriver == nullptr)
	{
		return;
	}

	IRepChangedPropertyTracker* const ActorChangedPropertyTracker = NetDriver->FindOrCreateRepChangedPropertyTracker(this).Get();

	// PreReplication is only called on the server, except when we're recording a Client Replay.
	// In that case we call PreReplication on the locally controlled Character as well.
	if ((Role == ROLE_Authority) || ((Role == ROLE_AutonomousProxy) && GetWorld()->IsRecordingClientReplay()))
	{
		PreReplication(*ActorChangedPropertyTracker);
	}

	// If we're recording a replay, call this for everyone (includes SimulatedProxies).
	if (ActorChangedPropertyTracker->IsReplay())
	{
		PreReplicationForReplay(*ActorChangedPropertyTracker);
	}

	// Call PreReplication on all owned components that are replicated
	for (UActorComponent* Component : ReplicatedComponents)
	{
		// Only call on components that aren't pending kill
		if (Component && !Component->IsPendingKill())
		{
			Component->PreReplication(*NetDriver->FindOrCreateRepChangedPropertyTracker(Component).Get());
		}
	}
}

void AActor::PreReplicationForReplay(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
}

void AActor::PostActorCreated()
{
	// nothing at the moment
}

void AActor::GetComponentsBoundingCylinder(float& OutCollisionRadius, float& OutCollisionHalfHeight, bool bNonColliding) const
{
	bool bIgnoreRegistration = false;

#if WITH_EDITOR
	if(IsTemplate())
	{
		// Editor code calls this function on default objects when placing them in the viewport, so no components will be registered in those cases.
		UWorld* MyWorld = GetWorld();
		if (!MyWorld || !MyWorld->IsGameWorld())
		{
			bIgnoreRegistration = true;
		}
		else
		{
			UE_LOG(LogActor, Log, TEXT("WARNING AActor::GetComponentsBoundingCylinder : Called on default object '%s'. Will likely return zero size."), *this->GetPathName());
		}
	}
#elif !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (IsTemplate())
	{
		UE_LOG(LogActor, Log, TEXT("WARNING AActor::GetComponentsBoundingCylinder : Called on default object '%s'. Will likely return zero size."), *this->GetPathName());
	}
#endif

	float Radius = 0.f;
	float HalfHeight = 0.f;

	for (const UActorComponent* ActorComponent : GetComponents())
	{
		const UPrimitiveComponent* PrimComp = Cast<const UPrimitiveComponent>(ActorComponent);
		if (PrimComp)
		{
			// Only use collidable components to find collision bounding box.
			if ((bIgnoreRegistration || PrimComp->IsRegistered()) && (bNonColliding || PrimComp->IsCollisionEnabled()))
			{
				float TestRadius, TestHalfHeight;
				PrimComp->CalcBoundingCylinder(TestRadius, TestHalfHeight);
				Radius = FMath::Max(Radius, TestRadius);
				HalfHeight = FMath::Max(HalfHeight, TestHalfHeight);
			}
		}
	}

	OutCollisionRadius = Radius;
	OutCollisionHalfHeight = HalfHeight;
}


void AActor::GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const
{
	if (IsRootComponentCollisionRegistered())
	{
		RootComponent->CalcBoundingCylinder(CollisionRadius, CollisionHalfHeight);
	}
	else
	{
		GetComponentsBoundingCylinder(CollisionRadius, CollisionHalfHeight, false);
	}
}

bool AActor::IsRootComponentCollisionRegistered() const
{
	return RootComponent != NULL && RootComponent->IsRegistered() && RootComponent->IsCollisionEnabled();
}

bool AActor::IsAttachedTo(const AActor* Other) const
{
	return (RootComponent && Other && Other->RootComponent) ? RootComponent->IsAttachedTo(Other->RootComponent) : false;
}

bool AActor::IsBasedOnActor(const AActor* Other) const
{
	return IsAttachedTo(Other);
}

bool AActor::Modify( bool bAlwaysMarkDirty/*=true*/ )
{
	if (!CanModify())
	{
		return false;
	}

	// Any properties that reference a blueprint constructed component needs to avoid creating a reference to the component from the transaction
	// buffer, so we temporarily switch the property to non-transactional while the modify occurs
	TArray<UObjectProperty*> TemporarilyNonTransactionalProperties;
	if (GUndo)
	{
		for (TFieldIterator<UObjectProperty> PropertyIt(GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UObjectProperty* ObjProp = *PropertyIt;
			if (!ObjProp->HasAllPropertyFlags(CPF_NonTransactional))
			{
				UActorComponent* ActorComponent = Cast<UActorComponent>(ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(this)));
				if (ActorComponent && ActorComponent->IsCreatedByConstructionScript())
				{
					ObjProp->SetPropertyFlags(CPF_NonTransactional);
					TemporarilyNonTransactionalProperties.Add(ObjProp);
				}
			}
		}
	}

	bool bSavedToTransactionBuffer = UObject::Modify( bAlwaysMarkDirty );

	for (UObjectProperty* ObjProp : TemporarilyNonTransactionalProperties)
	{
		ObjProp->ClearPropertyFlags(CPF_NonTransactional);
	}

	// If the root component is blueprint constructed we don't save it to the transaction buffer
	if( RootComponent && !RootComponent->IsCreatedByConstructionScript())
	{
		bSavedToTransactionBuffer = RootComponent->Modify( bAlwaysMarkDirty ) || bSavedToTransactionBuffer;
	}

	return bSavedToTransactionBuffer;
}

FBox AActor::GetComponentsBoundingBox(bool bNonColliding) const
{
	FBox Box(ForceInit);

	for (const UActorComponent* ActorComponent : GetComponents())
	{
		const UPrimitiveComponent* PrimComp = Cast<const UPrimitiveComponent>(ActorComponent);
		if (PrimComp)
		{
			// Only use collidable components to find collision bounding box.
			if (PrimComp->IsRegistered() && (bNonColliding || PrimComp->IsCollisionEnabled()))
			{
				Box += PrimComp->Bounds.GetBox();
			}
		}
	}

	return Box;
}

FBox AActor::CalculateComponentsBoundingBoxInLocalSpace( bool bNonColliding ) const
{
	FBox Box(ForceInit);

	const FTransform& ActorToWorld = GetTransform();
	const FTransform WorldToActor = ActorToWorld.Inverse();

	for( const UActorComponent* ActorComponent : GetComponents() )
	{
		const UPrimitiveComponent* PrimComp = Cast<const UPrimitiveComponent>( ActorComponent );
		if( PrimComp )
		{
			// Only use collidable components to find collision bounding box.
			if( PrimComp->IsRegistered() && ( bNonColliding || PrimComp->IsCollisionEnabled() ) )
			{
				const FTransform ComponentToActor = PrimComp->GetComponentTransform() * WorldToActor;
				FBoxSphereBounds ActorSpaceComponentBounds = PrimComp->CalcBounds( ComponentToActor );

				Box += ActorSpaceComponentBounds.GetBox();
			}
		}
	}

	return Box;
}

bool AActor::CheckStillInWorld()
{
	if (IsPendingKill())
	{
		return false;
	}
	UWorld* MyWorld = GetWorld();
	if (!MyWorld)
	{
		return false;
	}

	// check the variations of KillZ
	AWorldSettings* WorldSettings = MyWorld->GetWorldSettings( true );

	if (!WorldSettings->bEnableWorldBoundsChecks)
	{
		return true;
	}

	if( GetActorLocation().Z < WorldSettings->KillZ )
	{
		UDamageType const* const DmgType = WorldSettings->KillZDamageType ? WorldSettings->KillZDamageType->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
		FellOutOfWorld(*DmgType);
		return false;
	}
	// Check if box has poked outside the world
	else if( ( RootComponent != NULL ) && ( GetRootComponent()->IsRegistered() == true ) )
	{
		const FBox&	Box = GetRootComponent()->Bounds.GetBox();
		if(	Box.Min.X < -HALF_WORLD_MAX || Box.Max.X > HALF_WORLD_MAX ||
			Box.Min.Y < -HALF_WORLD_MAX || Box.Max.Y > HALF_WORLD_MAX ||
			Box.Min.Z < -HALF_WORLD_MAX || Box.Max.Z > HALF_WORLD_MAX )
		{
			UE_LOG(LogActor, Warning, TEXT("%s is outside the world bounds!"), *GetName());
			OutsideWorldBounds();
			// not safe to use physics or collision at this point
			SetActorEnableCollision(false);
			DisableComponentsSimulatePhysics();
			return false;
		}
	}

	return true;
}

void AActor::SetTickGroup(ETickingGroup NewTickGroup)
{
	PrimaryActorTick.TickGroup = NewTickGroup;
}

void AActor::ClearComponentOverlaps()
{
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents(PrimitiveComponents);

	// Remove owned components from overlap tracking
	// We don't traverse the RootComponent attachment tree since that might contain
	// components owned by other actors.
	TArray<FOverlapInfo, TInlineAllocator<3>> OverlapsForCurrentComponent;
	for (UPrimitiveComponent* const PrimComp : PrimitiveComponents)
	{
		OverlapsForCurrentComponent.Reset();
		OverlapsForCurrentComponent.Append(PrimComp->GetOverlapInfos());
		for (const FOverlapInfo& CurrentOverlap : OverlapsForCurrentComponent)
		{
			const bool bDoNotifies = true;
			const bool bSkipNotifySelf = false;
			PrimComp->EndComponentOverlap(CurrentOverlap, bDoNotifies, bSkipNotifySelf);
		}
	}
}

void AActor::UpdateOverlaps(bool bDoNotifies)
{
	// just update the root component, which will cascade down to the children
	USceneComponent* const RootComp = GetRootComponent();
	if (RootComp)
	{
		RootComp->UpdateOverlaps(NULL, bDoNotifies);
	}
}

bool AActor::IsOverlappingActor(const AActor* Other) const
{
	for (UActorComponent* OwnedComp : OwnedComponents)
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp))
		{
			if (PrimComp->IsOverlappingActor(Other))
			{
				// found one, finished
				return true;
			}
		}
	}
	return false;
}

void AActor::GetOverlappingActors(TArray<AActor*>& OutOverlappingActors, TSubclassOf<AActor> ClassFilter) const
{
	// prepare output
	TSet<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, ClassFilter);

	OutOverlappingActors.Reset(OverlappingActors.Num());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		OutOverlappingActors.Add(OverlappingActor);
	}
}

void AActor::GetOverlappingActors(TSet<AActor*>& OutOverlappingActors, TSubclassOf<AActor> ClassFilter) const
{
	// prepare output
	OutOverlappingActors.Reset();
	TSet<AActor*> OverlappingActorsForCurrentComponent;

	for(UActorComponent* OwnedComp : OwnedComponents)
	{
		if(UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp))
		{
			PrimComp->GetOverlappingActors(OverlappingActorsForCurrentComponent, ClassFilter);

			OutOverlappingActors.Reserve(OutOverlappingActors.Num() + OverlappingActorsForCurrentComponent.Num());

			// then merge it into our final list
			for (AActor* OverlappingActor : OverlappingActorsForCurrentComponent)
			{
				if (OverlappingActor != this)
				{
					OutOverlappingActors.Add(OverlappingActor);
				}
			}
		}
	}
}

void AActor::GetOverlappingComponents(TArray<UPrimitiveComponent*>& OutOverlappingComponents) const
{
	TSet<UPrimitiveComponent*> OverlappingComponents;
	GetOverlappingComponents(OverlappingComponents);

	OutOverlappingComponents.Reset(OverlappingComponents.Num());

	for (UPrimitiveComponent* OverlappingComponent : OverlappingComponents)
	{
		OutOverlappingComponents.Add(OverlappingComponent);
	}
}

void AActor::GetOverlappingComponents(TSet<UPrimitiveComponent*>& OutOverlappingComponents) const
{
	OutOverlappingComponents.Reset();
	TArray<UPrimitiveComponent*> OverlappingComponentsForCurrentComponent;

	for (UActorComponent* OwnedComp : OwnedComponents)
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp))
		{
			// get list of components from the component
			PrimComp->GetOverlappingComponents(OverlappingComponentsForCurrentComponent);

			OutOverlappingComponents.Reserve(OutOverlappingComponents.Num() + OverlappingComponentsForCurrentComponent.Num());

			// then merge it into our final list
			for (UPrimitiveComponent* OverlappingComponent : OverlappingComponentsForCurrentComponent)
			{
				OutOverlappingComponents.Add(OverlappingComponent);
			}
		}
	}
}

void AActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	// call BP handler
	ReceiveActorBeginOverlap(OtherActor);
}

void AActor::NotifyActorEndOverlap(AActor* OtherActor)
{
	// call BP handler
	ReceiveActorEndOverlap(OtherActor);
}

void AActor::NotifyActorBeginCursorOver()
{
	// call BP handler
	ReceiveActorBeginCursorOver();
}

void AActor::NotifyActorEndCursorOver()
{
	// call BP handler
	ReceiveActorEndCursorOver();
}

void AActor::NotifyActorOnClicked(FKey ButtonPressed)
{
	// call BP handler
	ReceiveActorOnClicked(ButtonPressed);
}

void AActor::NotifyActorOnReleased(FKey ButtonReleased)
{
	// call BP handler
	ReceiveActorOnReleased(ButtonReleased);
}

void AActor::NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex)
{
	// call BP handler
	ReceiveActorOnInputTouchBegin(FingerIndex);
}

void AActor::NotifyActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex)
{
	// call BP handler
	ReceiveActorOnInputTouchEnd(FingerIndex);
}

void AActor::NotifyActorOnInputTouchEnter(const ETouchIndex::Type FingerIndex)
{
	// call BP handler
	ReceiveActorOnInputTouchEnter(FingerIndex);
}

void AActor::NotifyActorOnInputTouchLeave(const ETouchIndex::Type FingerIndex)
{
	// call BP handler
	ReceiveActorOnInputTouchLeave(FingerIndex);
}


void AActor::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	// call BP handler
	ReceiveHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
}

/** marks all PrimitiveComponents for which their Owner is relevant for visibility as dirty because the Owner of some Actor in the chain has changed
 * @param TheActor the actor to mark components dirty for
 */
static void MarkOwnerRelevantComponentsDirty(AActor* TheActor)
{
	TInlineComponentArray<UPrimitiveComponent*> Components;
	TheActor->GetComponents(Components);

	for (int32 i = 0; i < Components.Num(); i++)
	{
		UPrimitiveComponent* Primitive = Components[i];
		if (Primitive->IsRegistered() && (Primitive->bOnlyOwnerSee || Primitive->bOwnerNoSee))
		{
			Primitive->MarkRenderStateDirty();
		}
	}

	// recurse over children of this Actor
	for (int32 i = 0; i < TheActor->Children.Num(); i++)
	{
		AActor* Child = TheActor->Children[i];
		if (Child != NULL && !Child->IsPendingKill())
		{
			MarkOwnerRelevantComponentsDirty(Child);
		}
	}
}

bool AActor::WasRecentlyRendered(float Tolerance) const
{
	UWorld* World = GetWorld();
	return (World) ? (World->GetTimeSeconds() - GetLastRenderTime() <= Tolerance) : false;
}

float AActor::GetLastRenderTime() const
{
	// return most recent of Components' LastRenderTime
	// @todo UE4 maybe check base component and components attached to it instead?
	float LastRenderTime = -1000.f;
	for (const UActorComponent* ActorComponent : GetComponents())
	{
		const UPrimitiveComponent* PrimComp = Cast<const UPrimitiveComponent>(ActorComponent);
		if (PrimComp && PrimComp->IsRegistered())
		{
			LastRenderTime = FMath::Max(LastRenderTime, PrimComp->LastRenderTime);
		}
	}
	return LastRenderTime;
}

void AActor::SetOwner( AActor *NewOwner )
{
	if (Owner != NewOwner && !IsPendingKill())
	{
		if (NewOwner != NULL && NewOwner->IsOwnedBy(this))
		{
			UE_LOG(LogActor, Error, TEXT("SetOwner(): Failed to set '%s' owner of '%s' because it would cause an Owner loop"), *NewOwner->GetName(), *GetName());
			return;
		}

		// Sets this actor's parent to the specified actor.
		AActor* OldOwner = Owner;
		if( Owner != NULL )
		{
			// remove from old owner's Children array
			verifySlow(Owner->Children.Remove(this) == 1);
		}

		Owner = NewOwner;

		if( Owner != NULL )
		{
			// add to new owner's Children array
			checkSlow(!Owner->Children.Contains(this));
			Owner->Children.Add(this);
		}

		// mark all components for which Owner is relevant for visibility to be updated
		MarkOwnerRelevantComponentsDirty(this);
	}
}

bool AActor::HasNetOwner() const
{
	if (Owner == NULL)
	{
		// all basic AActors are unable to call RPCs without special AActors as their owners (ie APlayerController)
		return false;
	}

	// Find the topmost actor in this owner chain
	AActor* TopOwner = NULL;
	for (TopOwner = Owner; TopOwner->Owner; TopOwner = TopOwner->Owner)
	{
	}

	return TopOwner->HasNetOwner();
}

void AActor::K2_AttachRootComponentTo(USceneComponent* InParent, FName InSocketName, EAttachLocation::Type AttachLocationType /*= EAttachLocation::KeepRelativeOffset */, bool bWeldSimulatedBodies /*=true*/)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AttachRootComponentTo(InParent, InSocketName, AttachLocationType, bWeldSimulatedBodies);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void AActor::AttachRootComponentTo(USceneComponent* InParent, FName InSocketName, EAttachLocation::Type AttachLocationType /*= EAttachLocation::KeepRelativeOffset */, bool bWeldSimulatedBodies /*=false*/)
{
	if(RootComponent && InParent)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RootComponent->AttachTo(InParent, InSocketName, AttachLocationType, bWeldSimulatedBodies);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void AActor::K2_AttachToComponent(USceneComponent* Parent, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule, bool bWeldSimulatedBodies)
{
	AttachToComponent(Parent, FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies), SocketName);
}

void AActor::AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName SocketName)
{
	if (RootComponent && Parent)
	{
		RootComponent->AttachToComponent(Parent, AttachmentRules, SocketName);
	}
}

void AActor::OnRep_AttachmentReplication()
{
	if (AttachmentReplication.AttachParent)
	{
		if (RootComponent)
		{
			USceneComponent* AttachParentComponent = (AttachmentReplication.AttachComponent ? AttachmentReplication.AttachComponent : AttachmentReplication.AttachParent->GetRootComponent());

			if (AttachParentComponent)
			{
				RootComponent->RelativeLocation = AttachmentReplication.LocationOffset;
				RootComponent->RelativeRotation = AttachmentReplication.RotationOffset;
				RootComponent->RelativeScale3D = AttachmentReplication.RelativeScale3D;
				RootComponent->AttachToComponent(AttachParentComponent, FAttachmentTransformRules::KeepRelativeTransform,  AttachmentReplication.AttachSocket);
			}
		}
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		// Handle the case where an object was both detached and moved on the server in the same frame.
		// Calling this extraneously does not hurt but will properly fire events if the movement state changed while attached.
		// This is needed because client side movement is ignored when attached
		OnRep_ReplicatedMovement();
	}
}

void AActor::K2_AttachRootComponentToActor(AActor* InParentActor, FName InSocketName /*= NAME_None*/, EAttachLocation::Type AttachLocationType /*= EAttachLocation::KeepRelativeOffset */, bool bWeldSimulatedBodies /*=true*/)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AttachRootComponentToActor(InParentActor, InSocketName, AttachLocationType, bWeldSimulatedBodies);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void AActor::AttachRootComponentToActor(AActor* InParentActor, FName InSocketName /*= NAME_None*/, EAttachLocation::Type AttachLocationType /*= EAttachLocation::KeepRelativeOffset */, bool bWeldSimulatedBodies /*=false*/)
{
	if (RootComponent && InParentActor)
	{
		USceneComponent* ParentDefaultAttachComponent = InParentActor->GetDefaultAttachComponent();
		if (ParentDefaultAttachComponent)
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			RootComponent->AttachTo(ParentDefaultAttachComponent, InSocketName, AttachLocationType, bWeldSimulatedBodies );
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
}

void AActor::K2_AttachToActor(AActor* ParentActor, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule, bool bWeldSimulatedBodies)
{
	AttachToActor(ParentActor, FAttachmentTransformRules(LocationRule, RotationRule, ScaleRule, bWeldSimulatedBodies), SocketName);
}

void AActor::AttachToActor(AActor* ParentActor, const FAttachmentTransformRules& AttachmentRules, FName SocketName)
{
	if (RootComponent && ParentActor)
	{
		USceneComponent* ParentDefaultAttachComponent = ParentActor->GetDefaultAttachComponent();
		if (ParentDefaultAttachComponent)
		{
			RootComponent->AttachToComponent(ParentDefaultAttachComponent, AttachmentRules, SocketName);
		}
	}
}

void AActor::SnapRootComponentTo(AActor* InParentActor, FName InSocketName/* = NAME_None*/)
{
	if (RootComponent && InParentActor)
	{
		USceneComponent* ParentDefaultAttachComponent = InParentActor->GetDefaultAttachComponent();
		if (ParentDefaultAttachComponent)
		{
			RootComponent->AttachToComponent(ParentDefaultAttachComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, InSocketName);
		}
	}
}

void AActor::DetachRootComponentFromParent(bool bMaintainWorldPosition)
{
	if(RootComponent)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RootComponent->DetachFromParent(bMaintainWorldPosition);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Clear AttachmentReplication struct
		AttachmentReplication = FRepAttachment();
	}
}

void AActor::K2_DetachFromActor(EDetachmentRule LocationRule /*= EDetachmentRule::KeepRelative*/, EDetachmentRule RotationRule /*= EDetachmentRule::KeepRelative*/, EDetachmentRule ScaleRule /*= EDetachmentRule::KeepRelative*/)
{
	DetachFromActor(FDetachmentTransformRules(LocationRule, RotationRule, ScaleRule, true));
}

void AActor::DetachFromActor(const FDetachmentTransformRules& DetachmentRules)
{
	if (RootComponent)
	{
		RootComponent->DetachFromComponent(DetachmentRules);
	}
}

void AActor::DetachSceneComponentsFromParent(USceneComponent* InParentComponent, bool bMaintainWorldPosition)
{
	DetachAllSceneComponents(InParentComponent, bMaintainWorldPosition ? FDetachmentTransformRules::KeepWorldTransform : FDetachmentTransformRules::KeepRelativeTransform);
}

void AActor::DetachAllSceneComponents(USceneComponent* InParentComponent, const FDetachmentTransformRules& DetachmentRules)
{
	if (InParentComponent)
	{
		TInlineComponentArray<USceneComponent*> Components;
		GetComponents(Components);

		for (USceneComponent* SceneComp : Components)
		{
			if (SceneComp->GetAttachParent() == InParentComponent)
			{
				SceneComp->DetachFromComponent(DetachmentRules);
			}
		}
	}
}

AActor* AActor::GetAttachParentActor() const
{
	if (GetRootComponent() && GetRootComponent()->GetAttachParent())
	{
		return GetRootComponent()->GetAttachParent()->GetOwner();
	}

	return nullptr;
}

FName AActor::GetAttachParentSocketName() const
{
	if (GetRootComponent() && GetRootComponent()->GetAttachParent())
	{
		return GetRootComponent()->GetAttachSocketName();
	}

	return NAME_None;
}

void AActor::GetAttachedActors(TArray<class AActor*>& OutActors) const
{
	OutActors.Reset();
	if (RootComponent != NULL)
	{
		// Current set of components to check
		TInlineComponentArray<USceneComponent*> CompsToCheck;

		// Set of all components we have checked
		TInlineComponentArray<USceneComponent*> CheckedComps;

		CompsToCheck.Push(RootComponent);

		// While still work left to do
		while(CompsToCheck.Num() > 0)
		{
			// Get the next off the queue
			const bool bAllowShrinking = false;
			USceneComponent* SceneComp = CompsToCheck.Pop(bAllowShrinking);

			// Add it to the 'checked' set, should not already be there!
			if (!CheckedComps.Contains(SceneComp))
			{
				CheckedComps.Add(SceneComp);

				AActor* CompOwner = SceneComp->GetOwner();
				if (CompOwner != NULL)
				{
					if (CompOwner != this)
					{
						// If this component has a different owner, add that owner to our output set and do nothing more
						OutActors.AddUnique(CompOwner);
					}
					else
					{
						// This component is owned by us, we need to add its children
						for (USceneComponent* ChildComp : SceneComp->GetAttachChildren())
						{
							// Add any we have not explored yet to the set to check
							if ((ChildComp != NULL) && !CheckedComps.Contains(ChildComp))
							{
								CompsToCheck.Push(ChildComp);
							}
						}
					}
				}
			}
		}
	}
}

bool AActor::ActorHasTag(FName Tag) const
{
	return (Tag != NAME_None) && Tags.Contains(Tag);
}

bool AActor::IsInLevel(const ULevel *TestLevel) const
{
	return (GetLevel() == TestLevel);
}

ULevel* AActor::GetLevel() const
{
	for (UObject* Outer = GetOuter(); Outer != nullptr; Outer = Outer->GetOuter())
	{
		if (ULevel* Level = Cast<ULevel>(Outer))
		{
			return Level;
		}
	}

	return nullptr;
}

bool AActor::IsInPersistentLevel(bool bIncludeLevelStreamingPersistent) const
{
	ULevel* MyLevel = GetLevel();
	UWorld* World = GetWorld();
	return ( (MyLevel == World->PersistentLevel) || ( bIncludeLevelStreamingPersistent && World->StreamingLevels.Num() > 0 &&
														Cast<ULevelStreamingPersistent>(World->StreamingLevels[0]) != NULL &&
														World->StreamingLevels[0]->GetLoadedLevel() == MyLevel ) );
}


bool AActor::IsMatineeControlled() const 
{
	bool bMovedByMatinee = false;
	for(auto It(ControllingMatineeActors.CreateConstIterator()); It; It++)
	{
		AMatineeActor* ControllingMatineeActor = *It;
		if(ControllingMatineeActor != NULL)
		{
			UInterpGroupInst* GroupInst = ControllingMatineeActor->FindGroupInst(this);
			if(GroupInst != NULL)
			{
				if(GroupInst->Group && GroupInst->Group->HasMoveTrack())
				{
					bMovedByMatinee = true;
					break;
				}
			}
			else
			{
				UE_LOG(LogActor, Log, TEXT("IsMatineeControlled: ControllingMatineeActor is set but no GroupInstance (%s)"), *GetPathName());
			}
		}
	}
	return bMovedByMatinee;
}

bool AActor::IsRootComponentStatic() const
{
	return(RootComponent != NULL && RootComponent->Mobility == EComponentMobility::Static);
}

bool AActor::IsRootComponentStationary() const
{
	return(RootComponent != NULL && RootComponent->Mobility == EComponentMobility::Stationary);
}

bool AActor::IsRootComponentMovable() const
{
	return(RootComponent != NULL && RootComponent->Mobility == EComponentMobility::Movable);
}

FVector AActor::GetTargetLocation(AActor* RequestedBy) const
{
	return GetActorLocation();
}


bool AActor::IsRelevancyOwnerFor(const AActor* ReplicatedActor, const AActor* ActorOwner, const AActor* ConnectionActor) const
{
	return (ActorOwner == this);
}

void AActor::ForceNetUpdate()
{
	if (NetDormancy > DORM_Awake)
	{
		FlushNetDormancy(); 
	}

	SetNetUpdateTime(GetWorld()->TimeSeconds - 0.01f);
}

bool AActor::IsReplicationPausedForConnection(const FNetViewer& ConnectionOwnerNetViewer)
{
	return false;
}

void AActor::OnReplicationPausedChanged(bool bIsReplicationPaused)
{
}

void AActor::SetNetDormancy(ENetDormancy NewDormancy)
{
	if (IsNetMode(NM_Client))
	{
		return;
	}
	
	UWorld* MyWorld = GetWorld();
	UNetDriver* NetDriver = GEngine->FindNamedNetDriver(MyWorld, NetDriverName);
	if (NetDriver)
	{
		NetDormancy = NewDormancy;

		// If not dormant, flush actor from NetDriver's dormant list
		if (NewDormancy <= DORM_Awake)
		{
			// Since we are coming out of dormancy, make sure we are on the network actor list
			MyWorld->AddNetworkActor( this );

			NetDriver->FlushActorDormancy(this);

			if (MyWorld->DemoNetDriver && MyWorld->DemoNetDriver != NetDriver)
			{
				MyWorld->DemoNetDriver->FlushActorDormancy(this);
			}
		}
	}
}

/** Removes the actor from the NetDriver's dormancy list: forcing at least one more update. */
void AActor::FlushNetDormancy()
{
	if (IsNetMode(NM_Client) || NetDormancy <= DORM_Awake)
	{
		return;
	}

	if (NetDormancy == DORM_Initial)
	{
		// No longer initially dormant
		NetDormancy = DORM_DormantAll;
	}

	// Don't proceed with network operations if not actually set to replicate
	if (!bReplicates)
	{
		return;
	}

	UWorld* MyWorld = GetWorld();

	// Add to network actors list if needed
	MyWorld->AddNetworkActor( this );
	
	UNetDriver* NetDriver = GetNetDriver();
	if (NetDriver)
	{
		NetDriver->FlushActorDormancy(this);

		if (MyWorld->DemoNetDriver && MyWorld->DemoNetDriver!= NetDriver)
		{
			MyWorld->DemoNetDriver->FlushActorDormancy(this);
		}
	}
}

void AActor::ForcePropertyCompare()
{
	if ( IsNetMode( NM_Client ) )
	{
		return;
	}

	if ( !bReplicates )
	{
		return;
	}

	const UWorld* MyWorld = GetWorld();

	UNetDriver* NetDriver = GetNetDriver();

	if ( NetDriver )
	{
		NetDriver->ForcePropertyCompare( this );

		if ( MyWorld->DemoNetDriver && MyWorld->DemoNetDriver != NetDriver )
		{
			MyWorld->DemoNetDriver->ForcePropertyCompare( this );
		}
	}
}

void AActor::PostRenderFor(APlayerController *PC, UCanvas *Canvas, FVector CameraPosition, FVector CameraDir) {}

void AActor::PrestreamTextures( float Seconds, bool bEnableStreaming, int32 CinematicTextureGroups )
{
	// This only handles non-location-based streaming. Location-based streaming is handled by SeqAct_StreamInTextures::UpdateOp.
	float Duration = 0.0;
	if ( bEnableStreaming )
	{
		// A Seconds==0.0f, it means infinite (e.g. 30 days)
		Duration = FMath::IsNearlyZero(Seconds) ? (60.0f*60.0f*24.0f*30.0f) : Seconds;
	}

	// Iterate over all components of that actor
	TInlineComponentArray<UMeshComponent*> Components;
	GetComponents(Components);

	for (int32 ComponentIndex=0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		// If its a static mesh component, with a static mesh
		UMeshComponent* MeshComponent = Components[ComponentIndex];
		if ( MeshComponent->IsRegistered() )
		{
			MeshComponent->PrestreamTextures( Duration, false, CinematicTextureGroups );
		}
	}
}

void AActor::OnRep_Instigator() {}

void AActor::OnRep_ReplicateMovement() {}

void AActor::RouteEndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bActorInitialized)
	{
		UWorld* World = GetWorld();
		if (World && World->HasBegunPlay())
		{
			EndPlay(EndPlayReason);
		}

		// Behaviors specific to an actor being unloaded due to a streaming level removal
		if (EndPlayReason == EEndPlayReason::RemovedFromWorld)
		{
			ClearComponentOverlaps();

			bActorInitialized = false;
			if (World)
			{
				World->RemoveNetworkActor(this);
			}
		}

		// Clear any ticking lifespan timers
		if (TimerHandle_LifeSpanExpired.IsValid())
		{
			SetLifeSpan(0.f);
		}

		UNavigationSystem::OnActorUnregistered(this);
	}

	UninitializeComponents();
}

void AActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActorHasBegunPlay == EActorBeginPlayState::HasBegunPlay)
	{
		ActorHasBegunPlay = EActorBeginPlayState::HasNotBegunPlay;

		// Dispatch the blueprint events
		ReceiveEndPlay(EndPlayReason);
		OnEndPlay.Broadcast(this, EndPlayReason);

		TInlineComponentArray<UActorComponent*> Components;
		GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component->HasBegunPlay())
			{
				Component->EndPlay(EndPlayReason);
			}
		}
	}
}

FVector AActor::GetPlacementExtent() const
{
	FVector Extent(0.f);
	if( (RootComponent && GetRootComponent()->ShouldCollideWhenPlacing()) && bCollideWhenPlacing) 
	{
		TInlineComponentArray<USceneComponent*> Components;
		GetComponents(Components);

		FBox ActorBox(ForceInit);
		for (int32 ComponentID=0; ComponentID<Components.Num(); ++ComponentID)
		{
			USceneComponent* SceneComp = Components[ComponentID];
			if (SceneComp->ShouldCollideWhenPlacing() )
			{
				ActorBox += SceneComp->GetPlacementExtent().GetBox();
			}
		}

		// Get box extent, adjusting for any difference between the center of the box and the actor pivot
		FVector AdjustedBoxExtent = ActorBox.GetExtent() - ActorBox.GetCenter();
		float CollisionRadius = FMath::Sqrt((AdjustedBoxExtent.X * AdjustedBoxExtent.X) + (AdjustedBoxExtent.Y * AdjustedBoxExtent.Y));
		Extent = FVector(CollisionRadius, CollisionRadius, AdjustedBoxExtent.Z);
	}
	return Extent;
}

void AActor::Destroyed()
{
	RouteEndPlay(EEndPlayReason::Destroyed);

	ReceiveDestroyed();
	OnDestroyed.Broadcast(this);
}

void AActor::TearOff()
{
	const ENetMode NetMode = GetNetMode();

	if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
	{
		bTearOff = true;
	}
}

void AActor::TornOff() {}

void AActor::Reset()
{
	K2_OnReset();
}

void AActor::FellOutOfWorld(const UDamageType& dmgType)
{
	DisableComponentsSimulatePhysics();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	Destroy();
}

void AActor::MakeNoise(float Loudness, APawn* NoiseInstigator, FVector NoiseLocation, float MaxRange, FName Tag)
{
	NoiseInstigator = NoiseInstigator ? NoiseInstigator : Instigator;
	if ((GetNetMode() != NM_Client) && NoiseInstigator)
	{
		AActor::MakeNoiseDelegate.Execute(this, Loudness, NoiseInstigator
			, NoiseLocation.IsZero() ? GetActorLocation() : NoiseLocation
			, MaxRange
			, Tag);
	}
}

void AActor::MakeNoiseImpl(AActor* NoiseMaker, float Loudness, APawn* NoiseInstigator, const FVector& NoiseLocation, float MaxRange, FName Tag)
{
	check(NoiseMaker);

	UPawnNoiseEmitterComponent* NoiseEmitterComponent = NoiseInstigator->GetPawnNoiseEmitterComponent();
	if (NoiseEmitterComponent)
	{
		// Note: MaxRange and Tag are not supported for this legacy component. Use AISense_Hearing instead.
		NoiseEmitterComponent->MakeNoise( NoiseMaker, Loudness, NoiseLocation );
	}
}

void AActor::SetMakeNoiseDelegate(const FMakeNoiseDelegate& NewDelegate)
{
	if (NewDelegate.IsBound())
	{
		MakeNoiseDelegate = NewDelegate;
	}
}

float AActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = DamageAmount;

	UDamageType const* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		// point damage event, pass off to helper function
		FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*) &DamageEvent;
		ActualDamage = InternalTakePointDamage(ActualDamage, *PointDamageEvent, EventInstigator, DamageCauser);

		// K2 notification for this actor
		if (ActualDamage != 0.f)
		{
			ReceivePointDamage(ActualDamage, DamageTypeCDO, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.ImpactNormal, PointDamageEvent->HitInfo.Component.Get(), PointDamageEvent->HitInfo.BoneName, PointDamageEvent->ShotDirection, EventInstigator, DamageCauser, PointDamageEvent->HitInfo);
			OnTakePointDamage.Broadcast(this, ActualDamage, EventInstigator, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.Component.Get(), PointDamageEvent->HitInfo.BoneName, PointDamageEvent->ShotDirection, DamageTypeCDO, DamageCauser);

			// Notify the component
			UPrimitiveComponent* const PrimComp = PointDamageEvent->HitInfo.Component.Get();
			if (PrimComp)
			{
				PrimComp->ReceiveComponentDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
			}
		}
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		// radial damage event, pass off to helper function
		FRadialDamageEvent* const RadialDamageEvent = (FRadialDamageEvent*) &DamageEvent;
		ActualDamage = InternalTakeRadialDamage(ActualDamage, *RadialDamageEvent, EventInstigator, DamageCauser);

		// K2 notification for this actor
		if (ActualDamage != 0.f)
		{
			FHitResult const& Hit = (RadialDamageEvent->ComponentHits.Num() > 0) ? RadialDamageEvent->ComponentHits[0] : FHitResult();
			ReceiveRadialDamage(ActualDamage, DamageTypeCDO, RadialDamageEvent->Origin, Hit, EventInstigator, DamageCauser);

			// add any desired physics impulses to our components
			for (int HitIdx = 0; HitIdx < RadialDamageEvent->ComponentHits.Num(); ++HitIdx)
			{
				FHitResult const& CompHit = RadialDamageEvent->ComponentHits[HitIdx];
				UPrimitiveComponent* const PrimComp = CompHit.Component.Get();
				if (PrimComp && PrimComp->GetOwner() == this)
				{
					PrimComp->ReceiveComponentDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
				}
			}
		}
	}

	// generic damage notifications sent for any damage
	// note we will broadcast these for negative damage as well
	if (ActualDamage != 0.f)
	{
		ReceiveAnyDamage(ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
		OnTakeAnyDamage.Broadcast(this, ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
		if (EventInstigator != NULL)
		{
			EventInstigator->InstigatedAnyDamage(ActualDamage, DamageTypeCDO, this, DamageCauser);
		}
	}

	return ActualDamage;
}

float AActor::InternalTakeRadialDamage(float Damage, FRadialDamageEvent const& RadialDamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	float ActualDamage = Damage;

	FVector ClosestHitLoc(0);

	// find closest component
	// @todo, something more accurate here to account for size of components, e.g. closest point on the component bbox?
	// @todo, sum up damage contribution to each component?
	float ClosestHitDistSq = MAX_FLT;
	for (int32 HitIdx=0; HitIdx<RadialDamageEvent.ComponentHits.Num(); ++HitIdx)
	{
		FHitResult const& Hit = RadialDamageEvent.ComponentHits[HitIdx];
		float const DistSq = (Hit.ImpactPoint - RadialDamageEvent.Origin).SizeSquared();
		if (DistSq < ClosestHitDistSq)
		{
			ClosestHitDistSq = DistSq;
			ClosestHitLoc = Hit.ImpactPoint;
		}
	}

	float const RadialDamageScale = RadialDamageEvent.Params.GetDamageScale( FMath::Sqrt(ClosestHitDistSq) );

	ActualDamage = FMath::Lerp(RadialDamageEvent.Params.MinimumDamage, ActualDamage, FMath::Max(0.f, RadialDamageScale));

	return ActualDamage;
}

float AActor::InternalTakePointDamage(float Damage, FPointDamageEvent const& PointDamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	return Damage;
}

// deprecated
void AActor::ReceivePointDamage(float Damage, const class UDamageType* DamageType, FVector HitLocation, FVector HitNormal, class UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, class AController* InstigatedBy, AActor* DamageCauser)
{
	// Call proper version with a default FHitResult.
	ReceivePointDamage(Damage, DamageType, HitLocation, HitNormal, HitComponent, BoneName, ShotFromDirection, InstigatedBy, DamageCauser, FHitResult());
}

/** Util to check if prim comp pointer is valid and still alive */
extern bool IsPrimCompValidAndAlive(UPrimitiveComponent* PrimComp);

/** Used to determine if it is ok to call a notification on this object */
bool IsActorValidToNotify(AActor* Actor)
{
	return (Actor != NULL) && !Actor->IsPendingKill() && !Actor->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists);
}

void AActor::InternalDispatchBlockingHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, bool bSelfMoved, FHitResult const& Hit)
{
	check(MyComp);

	if (OtherComp != nullptr)
	{
		AActor* OtherActor = OtherComp->GetOwner();

		// Call virtual
		if(IsActorValidToNotify(this))
		{
			NotifyHit(MyComp, OtherActor, OtherComp, bSelfMoved, Hit.ImpactPoint, Hit.ImpactNormal, FVector(0,0,0), Hit);
		}

		// If we are still ok, call delegate on actor
		if(IsActorValidToNotify(this))
		{
			OnActorHit.Broadcast(this, OtherActor, FVector(0,0,0), Hit);
		}

		// If component is still alive, call delegate on component
		if(!MyComp->IsPendingKill())
		{
			MyComp->OnComponentHit.Broadcast(MyComp, OtherActor, OtherComp, FVector(0,0,0), Hit);
		}
	}
}

void AActor::DispatchBlockingHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, bool bSelfMoved, FHitResult const& Hit)
{
	InternalDispatchBlockingHit(MyComp, OtherComp, bSelfMoved, bSelfMoved ? Hit : FHitResult::GetReversedHit(Hit));
}


FString AActor::GetHumanReadableName() const
{
	return GetName();
}

void AActor::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	// Draw box around Actor being debugged.
#if ENABLE_DRAW_DEBUG
	{
		FVector BoundsOrigin, BoundsExtent;
		GetActorBounds(true, BoundsOrigin, BoundsExtent);

		// Expand extent a little bit
		BoundsExtent *= 1.1f;
		DrawDebugBox(GetWorld(), BoundsOrigin, BoundsExtent, FColor::Green, false, -1.f, 0, 2.f);
	}
#endif

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor(255, 0, 0));

	FString T = GetHumanReadableName();
	if( IsPendingKill() )
	{
		T = FString::Printf(TEXT("%s DELETED (IsPendingKill() == true)"), *T);
	}
	if( T != "" )
	{
		DisplayDebugManager.DrawString(T);
	}

	DisplayDebugManager.SetDrawColor(FColor(255, 255, 255));

	if( DebugDisplay.IsDisplayOn(TEXT("net")) )
	{
		if( GetNetMode() != NM_Standalone )
		{
			// networking attributes
			T = FString::Printf(TEXT("ROLE: %i RemoteRole: %i NetNode: %i"), (int32)Role, (int32)RemoteRole, (int32)GetNetMode());

			if( bTearOff )
			{
				T = T + FString(TEXT(" Tear Off"));
			}
			DisplayDebugManager.DrawString(T);
		}
	}

	DisplayDebugManager.DrawString(FString::Printf(TEXT("Location: %s Rotation: %s"), *GetActorLocation().ToCompactString(), *GetActorRotation().ToCompactString()));

	if( DebugDisplay.IsDisplayOn(TEXT("physics")) )
	{
		DisplayDebugManager.DrawString(FString::Printf(TEXT("Velocity: %s Speed: %f Speed2D: %f"), *GetVelocity().ToCompactString(), GetVelocity().Size(), GetVelocity().Size2D()));
	}

	if( DebugDisplay.IsDisplayOn(TEXT("collision")) )
	{
		Canvas->DrawColor.B = 0;
		float MyRadius, MyHeight;
		GetComponentsBoundingCylinder(MyRadius, MyHeight);
		DisplayDebugManager.DrawString(FString::Printf(TEXT("Collision Radius: %f Height: %f"), MyRadius, MyHeight));

		if ( RootComponent == NULL )
		{
			DisplayDebugManager.DrawString(FString(TEXT("No RootComponent")));
		}

		T = FString(TEXT("Overlapping "));

		TSet<AActor*> TouchingActors;
		GetOverlappingActors(TouchingActors);
		bool bFoundAnyOverlaps = false;
		for (AActor* const TestActor : TouchingActors)
		{
			if (TestActor &&
				!TestActor->IsPendingKill())
			{
				T = T + TestActor->GetName() + " ";
				bFoundAnyOverlaps = true;
			}
		}

		if (!bFoundAnyOverlaps)
		{
			T = TEXT("Overlapping nothing");
		}
		DisplayDebugManager.DrawString(T);
	}
	DisplayDebugManager.DrawString(FString::Printf(TEXT(" Instigator: %s Owner: %s"), 
		(Instigator ? *Instigator->GetName() : TEXT("None")), (Owner ? *Owner->GetName() : TEXT("None"))));

	static FName NAME_Animation(TEXT("Animation"));
	static FName NAME_Bones = FName(TEXT("Bones"));
	if (DebugDisplay.IsDisplayOn(NAME_Animation) || DebugDisplay.IsDisplayOn(NAME_Bones))
	{
		TInlineComponentArray<USkeletalMeshComponent*> Components;
		GetComponents(Components);

		if (DebugDisplay.IsDisplayOn(NAME_Animation))
		{
			for (USkeletalMeshComponent* Comp : Components)
			{
				UAnimInstance* AnimInstance = Comp->GetAnimInstance();
				if (AnimInstance)
				{
					AnimInstance->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
				}
			}
		}
	}
}

void AActor::OutsideWorldBounds()
{
	Destroy();
}

bool AActor::CanBeBaseForCharacter(APawn* APawn) const
{
	return true;
}

void AActor::BecomeViewTarget( APlayerController* PC )
{
	K2_OnBecomeViewTarget(PC);
}

void AActor::EndViewTarget( APlayerController* PC )
{
	K2_OnEndViewTarget(PC);
}

APawn* AActor::GetInstigator() const
{
	return Instigator;
}

AController* AActor::GetInstigatorController() const
{
	return Instigator ? Instigator->Controller : NULL;
}

void AActor::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bFindCameraComponentWhenViewTarget)
	{
		// Look for the first active camera component and use that for the view
		TInlineComponentArray<UCameraComponent*> Cameras;
		GetComponents<UCameraComponent>(/*out*/ Cameras);

		for (UCameraComponent* CameraComponent : Cameras)
		{
			if (CameraComponent->bIsActive)
			{
				CameraComponent->GetCameraView(DeltaTime, OutResult);
				return;
			}
		}
	}

	GetActorEyesViewPoint(OutResult.Location, OutResult.Rotation);
}

bool AActor::HasActiveCameraComponent() const
{
	if (bFindCameraComponentWhenViewTarget)
	{
		// Look for the first active camera component and use that for the view
		for (const UActorComponent* Component : OwnedComponents)
		{
			const UCameraComponent* CameraComponent = Cast<const UCameraComponent>(Component);
			if (CameraComponent)
			{
				if (CameraComponent->bIsActive)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool AActor::HasActivePawnControlCameraComponent() const
{
	if (bFindCameraComponentWhenViewTarget)
	{
		// Look for the first active camera component and use that for the view
		for (const UActorComponent* Component : OwnedComponents)
		{
			const UCameraComponent* CameraComponent = Cast<const UCameraComponent>(Component);
			if (CameraComponent)
			{
				if (CameraComponent->bIsActive && CameraComponent->bUsePawnControlRotation)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void AActor::ForceNetRelevant()
{
	if ( !NeedsLoadForClient() )
	{
		UE_LOG(LogSpawn, Warning, TEXT("ForceNetRelevant called for actor that doesn't load on client: %s" ), *GetFullName() );
		return;
	}

	if (RemoteRole == ROLE_None)
	{
		SetReplicates(true);
		bAlwaysRelevant = true;
		if (NetUpdateFrequency == 0.f)
		{
			NetUpdateFrequency = 0.1f;
		}
	}
	ForceNetUpdate();
}

void AActor::GetActorEyesViewPoint( FVector& OutLocation, FRotator& OutRotation ) const
{
	OutLocation = GetActorLocation();
	OutRotation = GetActorRotation();
}

enum ECollisionResponse AActor::GetComponentsCollisionResponseToChannel(enum ECollisionChannel Channel) const
{
	ECollisionResponse OutResponse = ECR_Ignore;

	TInlineComponentArray<UPrimitiveComponent*> Components;
	GetComponents(Components);

	for (int32 i = 0; i < Components.Num(); i++)
	{
		UPrimitiveComponent* Primitive = Components[i];
		if ( Primitive->IsCollisionEnabled() )
		{
			// find Max of the response, blocking > overlapping > ignore
			OutResponse = FMath::Max(Primitive->GetCollisionResponseToChannel(Channel), OutResponse);
		}
	}

	return OutResponse;
};

void AActor::AddOwnedComponent(UActorComponent* Component)
{
	check(Component->GetOwner() == this);

	// Note: we do not mark dirty here because this can be called when in editor when modifying transient components
	// if a component is added during this time it should not dirty.  Higher level code in the editor should always dirty the package anyway
	const bool bMarkDirty = false;
	Modify(bMarkDirty);

	bool bAlreadyInSet = false;
	OwnedComponents.Add(Component, &bAlreadyInSet);

	if (!bAlreadyInSet)
	{
		if (Component->GetIsReplicated())
		{
			ReplicatedComponents.Add(Component);
		}

		if (Component->IsCreatedByConstructionScript())
		{
			BlueprintCreatedComponents.Add(Component);
		}
		else if (Component->CreationMethod == EComponentCreationMethod::Instance)
		{
			InstanceComponents.Add(Component);
		}
	}
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	// Note: we do not mark dirty here because this can be called as part of component duplication when reinstancing components during blueprint compilation
	// if a component is removed during this time it should not dirty.  Higher level code in the editor should always dirty the package anyway.
	const bool bMarkDirty = false;
	Modify(bMarkDirty);

	if (OwnedComponents.Remove(Component) > 0)
	{
		ReplicatedComponents.Remove(Component);
		if (Component->IsCreatedByConstructionScript())
		{
			BlueprintCreatedComponents.RemoveSingleSwap(Component);
		}
		else if (Component->CreationMethod == EComponentCreationMethod::Instance)
		{
			InstanceComponents.RemoveSingleSwap(Component);
		}
	}
}

#if DO_CHECK
bool AActor::OwnsComponent(UActorComponent* Component) const
{
	return OwnedComponents.Contains(Component);
}
#endif

void AActor::UpdateReplicatedComponent(UActorComponent* Component)
{
	checkf(Component->GetOwner() == this, TEXT("UE-9568: Component %s being updated for Actor %s"), *Component->GetPathName(), *GetPathName() );
	if (Component->GetIsReplicated())
	{
		ReplicatedComponents.Add(Component);
	}
	else
	{
		ReplicatedComponents.Remove(Component);
	}
}

void AActor::UpdateAllReplicatedComponents()
{
	ReplicatedComponents.Reset();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (Component != NULL)
		{
			UpdateReplicatedComponent(Component);
		}
	}
}

const TArray<UActorComponent*>& AActor::GetInstanceComponents() const
{
	return InstanceComponents;
}

void AActor::AddInstanceComponent(UActorComponent* Component)
{
	Component->CreationMethod = EComponentCreationMethod::Instance;
	InstanceComponents.AddUnique(Component);
}

void AActor::RemoveInstanceComponent(UActorComponent* Component)
{
	InstanceComponents.Remove(Component);
}

void AActor::ClearInstanceComponents(const bool bDestroyComponents)
{
	if (bDestroyComponents)
	{
		// Need to cache because calling destroy will remove them from InstanceComponents
		TArray<UActorComponent*> CachedComponents(InstanceComponents);

		// Run in reverse to reduce memory churn when the components are removed from InstanceComponents
		for (int32 Index=CachedComponents.Num()-1; Index >= 0; --Index)
		{
			CachedComponents[Index]->DestroyComponent();
		}
	}
	else
	{
		InstanceComponents.Reset();
	}
}

UActorComponent* AActor::FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
{
	UActorComponent* FoundComponent = nullptr;

	if (UClass* TargetClass = ComponentClass.Get())
	{
		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component && Component->IsA(TargetClass))
			{
				FoundComponent = Component;
				break;
			}
		}
	}

	return FoundComponent;
}

UActorComponent* AActor::GetComponentByClass(TSubclassOf<UActorComponent> ComponentClass) const
{
	return FindComponentByClass(ComponentClass);
}

TArray<UActorComponent*> AActor::GetComponentsByClass(TSubclassOf<UActorComponent> ComponentClass) const
{
	TArray<UActorComponent*> ValidComponents;

	// In the UActorComponent case we can skip the IsA checks for a slight performance benefit
	if (ComponentClass == UActorComponent::StaticClass())
	{
		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component)
			{
				ValidComponents.Add(Component);
			}
		}
	}
	else if (*ComponentClass)
	{
		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component && Component->IsA(ComponentClass))
			{
				ValidComponents.Add(Component);
			}
		}
	}
	
	return ValidComponents;
}

TArray<UActorComponent*> AActor::GetComponentsByTag(TSubclassOf<UActorComponent> ComponentClass, FName Tag) const
{
	TArray<UActorComponent*> ComponentsByClass = GetComponentsByClass(ComponentClass);

	TArray<UActorComponent*> ComponentsByTag;
	ComponentsByTag.Reserve(ComponentsByClass.Num());
	for (int i = 0; i < ComponentsByClass.Num(); ++i)
	{
		if (ComponentsByClass[i]->ComponentHasTag(Tag))
		{
			ComponentsByTag.Push(ComponentsByClass[i]);
		}
	}

	return ComponentsByTag;
}

void AActor::DisableComponentsSimulatePhysics()
{
	TInlineComponentArray<UPrimitiveComponent*> Components;
	GetComponents(Components);

	for (UPrimitiveComponent* Component : Components)
	{
		Component->SetSimulatePhysics(false);
	}
}

void AActor::PostRegisterAllComponents() 
{
}

/** Util to call OnComponentCreated on components */
static void DispatchOnComponentsCreated(AActor* NewActor)
{
	TInlineComponentArray<UActorComponent*> Components;
	NewActor->GetComponents(Components);

	for (UActorComponent* ActorComp : Components)
	{
		if (ActorComp && !ActorComp->HasBeenCreated())
		{
			ActorComp->OnComponentCreated();
		}
	}
}

#if WITH_EDITOR
void AActor::PostEditImport()
{
	Super::PostEditImport();

	DispatchOnComponentsCreated(this);
}

bool AActor::IsSelectedInEditor() const
{
	return !IsPendingKill() && GSelectedActorAnnotation.Get(this);
}

#endif

/** Util that sets up the actor's component hierarchy (when users forget to do so, in their native ctor) */
static USceneComponent* FixupNativeActorComponents(AActor* Actor)
{
	USceneComponent* SceneRootComponent = Actor->GetRootComponent();
	if (SceneRootComponent == nullptr)
	{
		TInlineComponentArray<USceneComponent*> SceneComponents;
		Actor->GetComponents(SceneComponents);
		if (SceneComponents.Num() > 0)
		{
			UE_LOG(LogActor, Warning, TEXT("%s has natively added scene component(s), but none of them were set as the actor's RootComponent - picking one arbitrarily"), *Actor->GetFullName());
	
			// if the user forgot to set one of their native components as the root, 
			// we arbitrarily pick one for them (otherwise the SCS could attempt to 
			// create its own root, and nest native components under it)
			for (USceneComponent* Component : SceneComponents)
			{
				if ((Component == nullptr) ||
					(Component->GetAttachParent() != nullptr) ||
					(Component->CreationMethod != EComponentCreationMethod::Native))
				{
					continue;
				}

				SceneRootComponent = Component;
				Actor->SetRootComponent(Component);
				break;
			}
		}
	}

	return SceneRootComponent;
}

// Simple and short-lived cache for storing transforms between beginning and finishing spawning.
static TMap< TWeakObjectPtr<AActor>, FTransform > GSpawnActorDeferredTransformCache;

static void ValidateDeferredTransformCache()
{
	// clean out any entries where the actor is no longer valid
	// could happen if an actor is destroyed before FinishSpawning is called
	for (auto It = GSpawnActorDeferredTransformCache.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<AActor>& ActorRef = It.Key();
		if (ActorRef.IsValid() == false)
		{
			It.RemoveCurrent();
		}
	}
}

void AActor::PostSpawnInitialize(FTransform const& UserSpawnTransform, AActor* InOwner, APawn* InInstigator, bool bRemoteOwned, bool bNoFail, bool bDeferConstruction)
{
	// General flow here is like so
	// - Actor sets up the basics.
	// - Actor gets PreInitializeComponents()
	// - Actor constructs itself, after which its components should be fully assembled
	// - Actor components get OnComponentCreated
	// - Actor components get InitializeComponent
	// - Actor gets PostInitializeComponents() once everything is set up
	//
	// This should be the same sequence for deferred or nondeferred spawning.

	// It's not safe to call UWorld accessor functions till the world info has been spawned.
	UWorld* const World = GetWorld();
	bool const bActorsInitialized = World && World->AreActorsInitialized();

	CreationTime = (World ? World->GetTimeSeconds() : 0.f);

	// Set network role.
	check(Role == ROLE_Authority);
	ExchangeNetRoles(bRemoteOwned);

	USceneComponent* const SceneRootComponent = FixupNativeActorComponents(this);
	if (SceneRootComponent != nullptr)
	{
		// Set the actor's location and rotation since it has a native rootcomponent
		// Note that we respect any initial transformation the root component may have from the CDO, so the final transform
		// might necessarily be exactly the passed-in UserSpawnTransform.
 		const FTransform RootTransform(SceneRootComponent->RelativeRotation, SceneRootComponent->RelativeLocation, SceneRootComponent->RelativeScale3D);
 		const FTransform FinalRootComponentTransform = RootTransform * UserSpawnTransform;
		SceneRootComponent->SetWorldTransform(FinalRootComponentTransform);
	}

	// Call OnComponentCreated on all default (native) components
	DispatchOnComponentsCreated(this);

	// If this is a Blueprint class, we may need to manually apply default value overrides to some inherited components in a
	// cooked build scenario. This can occur, for example, if we have a nativized Blueprint class in the inheritance hierarchy.
	// Note: This should be done prior to executing the construction script, in case there are any dependencies on default values.
	if (FPlatformProperties::RequiresCookedData())
	{
		const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(GetClass());
		if (BPGC != nullptr && BPGC->bHasNativizedParent)
		{
			UBlueprintGeneratedClass::CheckAndApplyComponentTemplateOverrides(this);
		}
	}

	// Register the actor's default (native) components, but only if we have a native scene root. If we don't, it implies that there could be only non-scene components
	// at the native class level. In that case, if this is a Blueprint instance, we need to defer native registration until after SCS execution can establish a scene root.
	// Note: This API will also call PostRegisterAllComponents() on the actor instance. If deferred, PostRegisterAllComponents() won't be called until the root is set by SCS.
	bHasDeferredComponentRegistration = (SceneRootComponent == nullptr && Cast<UBlueprintGeneratedClass>(GetClass()) != nullptr);
	if (!bHasDeferredComponentRegistration)
	{
		RegisterAllComponents();
	}

	// Set owner.
	SetOwner(InOwner);

	// Set instigator
	Instigator = InInstigator;

#if WITH_EDITOR
	// When placing actors in the editor, init any random streams 
	if (!bActorsInitialized)
	{
		SeedAllRandomStreams();
	}
#endif

	// See if anything has deleted us
	if( IsPendingKill() && !bNoFail )
	{
		return;
	}

	// Send messages. We've fully spawned
	PostActorCreated();

	// Executes native and BP construction scripts.
	// After this, we can assume all components are created and assembled.
	if (!bDeferConstruction)
	{
		FinishSpawning(UserSpawnTransform, true);
	}
	else if (SceneRootComponent != nullptr)
	{
		// we have a native root component and are deferring construction, store our original UserSpawnTransform
		// so we can do the proper thing if the user passes in a different transform during FinishSpawning
		GSpawnActorDeferredTransformCache.Emplace(this, UserSpawnTransform);
	}
}

#include "GameFramework/SpawnActorTimer.h"

void AActor::FinishSpawning(const FTransform& UserTransform, bool bIsDefaultTransform, const FComponentInstanceDataCache* InstanceDataCache)
{
#if ENABLE_SPAWNACTORTIMER
	FScopedSpawnActorTimer SpawnTimer(GetClass()->GetFName(), ESpawnActorTimingType::FinishSpawning);
	SpawnTimer.SetActorName(GetFName());
#endif

	if (ensure(!bHasFinishedSpawning))
	{
		bHasFinishedSpawning = true;

		FTransform FinalRootComponentTransform = (RootComponent ? RootComponent->GetComponentTransform() : UserTransform);

		// see if we need to adjust the transform (i.e. in deferred cases where the caller passes in a different transform here 
		// than was passed in during the original SpawnActor call)
		if (RootComponent && !bIsDefaultTransform)
		{
			FTransform const* const OriginalSpawnTransform = GSpawnActorDeferredTransformCache.Find(this);
			if (OriginalSpawnTransform)
			{
				GSpawnActorDeferredTransformCache.Remove(this);

				if (OriginalSpawnTransform->Equals(UserTransform) == false)
				{
					UserTransform.GetLocation().DiagnosticCheckNaN(TEXT("AActor::FinishSpawning: UserTransform.GetLocation()"));
					UserTransform.GetRotation().DiagnosticCheckNaN(TEXT("AActor::FinishSpawning: UserTransform.GetRotation()"));

					// caller passed a different transform!
					// undo the original spawn transform to get back to the template transform, so we can recompute a good
					// final transform that takes into account the template's transform
					FTransform const TemplateTransform = RootComponent->GetComponentTransform() * OriginalSpawnTransform->Inverse();
					FinalRootComponentTransform = TemplateTransform * UserTransform;
				}
			}

			// should be fast and relatively rare
			ValidateDeferredTransformCache();
		}

		FinalRootComponentTransform.GetLocation().DiagnosticCheckNaN(TEXT("AActor::FinishSpawning: FinalRootComponentTransform.GetLocation()"));
		FinalRootComponentTransform.GetRotation().DiagnosticCheckNaN(TEXT("AActor::FinishSpawning: FinalRootComponentTransform.GetRotation()"));

		ExecuteConstruction(FinalRootComponentTransform, nullptr, InstanceDataCache, bIsDefaultTransform);

		{
			SCOPE_CYCLE_COUNTER(STAT_PostActorConstruction);
			PostActorConstruction();
		}
	}
}

void AActor::PostActorConstruction()
{
	UWorld* const World = GetWorld();
	bool const bActorsInitialized = World && World->AreActorsInitialized();

	if (bActorsInitialized)
	{
		PreInitializeComponents();
	}

	// If this is dynamically spawned replicated actor, defer calls to BeginPlay and UpdateOverlaps until replicated properties are deserialized
	const bool bDeferBeginPlayAndUpdateOverlaps = (bExchangedRoles && RemoteRole == ROLE_Authority);

	if (bActorsInitialized)
	{
		// Call InitializeComponent on components
		InitializeComponents();

		// actor should have all of its components created and registered now, do any collision checking and handling that we need to do
		if (World)
		{
			switch (SpawnCollisionHandlingMethod)
			{
			case ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn:
			{
				// Try to find a spawn position
				FVector AdjustedLocation = GetActorLocation();
				FRotator AdjustedRotation = GetActorRotation();
				if (World->FindTeleportSpot(this, AdjustedLocation, AdjustedRotation))
				{
					SetActorLocationAndRotation(AdjustedLocation, AdjustedRotation, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
			break;
			case ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding:
			{
				// Try to find a spawn position			
				FVector AdjustedLocation = GetActorLocation();
				FRotator AdjustedRotation = GetActorRotation();
				if (World->FindTeleportSpot(this, AdjustedLocation, AdjustedRotation))
				{
					SetActorLocationAndRotation(AdjustedLocation, AdjustedRotation, false, nullptr, ETeleportType::TeleportPhysics);
				}
				else
				{
					UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because of collision at the spawn location [%s] for [%s]"), *AdjustedLocation.ToString(), *GetClass()->GetName());
					Destroy();
				}
			}
			break;
			case ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding:
				if (World->EncroachingBlockingGeometry(this, GetActorLocation(), GetActorRotation()))
				{
					UE_LOG(LogSpawn, Warning, TEXT("SpawnActor failed because of collision at the spawn location [%s] for [%s]"), *GetActorLocation().ToString(), *GetClass()->GetName());
					Destroy();
				}
				break;
			case ESpawnActorCollisionHandlingMethod::Undefined:
			case ESpawnActorCollisionHandlingMethod::AlwaysSpawn:
			default:
				// note we use "always spawn" as default, so treat undefined as that
				// nothing to do here, just proceed as normal
				break;
			}
		}

		if (!IsPendingKill())
		{
			PostInitializeComponents();
			if (!IsPendingKill())
			{
				if (!bActorInitialized)
				{
					UE_LOG(LogActor, Fatal, TEXT("%s failed to route PostInitializeComponents.  Please call Super::PostInitializeComponents() in your <className>::PostInitializeComponents() function. "), *GetFullName());
				}

				bool bRunBeginPlay = !bDeferBeginPlayAndUpdateOverlaps && (BeginPlayCallDepth > 0 || World->HasBegunPlay());
				if (bRunBeginPlay)
				{
					if (AActor* ParentActor = GetParentActor())
					{
						// Child Actors cannot run begin play until their parent has run
						bRunBeginPlay = (ParentActor->HasActorBegunPlay() || ParentActor->IsActorBeginningPlay());
					}
				}

#if WITH_EDITOR
				if (bRunBeginPlay && bIsEditorPreviewActor)
				{
					bRunBeginPlay = false;
				}
#endif

				if (bRunBeginPlay)
				{
					SCOPE_CYCLE_COUNTER(STAT_ActorBeginPlay);
					DispatchBeginPlay();
				}
			}
		}
	}
	else
	{
		// Set IsPendingKill() to true so that when the initial undo record is made,
		// the actor will be treated as destroyed, in that undo an add will
		// actually work
		MarkPendingKill();
		Modify(false);
		ClearPendingKill();
	}

	if (!IsPendingKill())
	{
		// Components are all there and we've begun play, init overlapping state
		if (!bDeferBeginPlayAndUpdateOverlaps)
		{
			UpdateOverlaps();
		}

		// Notify the texture streaming manager about the new actor.
		IStreamingManager::Get().NotifyActorSpawned(this);
	}
}

void AActor::SetReplicates(bool bInReplicates)
{ 
	if (Role == ROLE_Authority)
	{
		if (bReplicates == false && bInReplicates == true)
		{
			if (UWorld* MyWorld = GetWorld())		// GetWorld will return NULL on CDO, FYI
			{
				MyWorld->AddNetworkActor(this);
			}
		}

		RemoteRole = (bInReplicates ? ROLE_SimulatedProxy : ROLE_None);
		bReplicates = bInReplicates;
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("SetReplicates called on actor '%s' that is not valid for having its role modified."), *GetName());
	}
}

void AActor::SetReplicateMovement(bool bInReplicateMovement)
{
	bReplicateMovement = bInReplicateMovement;
}

void AActor::SetAutonomousProxy(const bool bInAutonomousProxy, const bool bAllowForcePropertyCompare)
{
	if (bReplicates)
	{
		const TEnumAsByte<enum ENetRole> OldRemoteRole = RemoteRole;

		RemoteRole = (bInAutonomousProxy ? ROLE_AutonomousProxy : ROLE_SimulatedProxy);

		if (bAllowForcePropertyCompare && RemoteRole != OldRemoteRole)
		{ 
			// We have to do this so the role change above will replicate (turn off shadow state sharing for a frame)
			// This is because RemoteRole is special since it will change between connections, so we have to special case
			ForcePropertyCompare();
		}
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("SetAutonomousProxy called on a unreplicated actor '%s"), *GetName());
	}
}

void AActor::CopyRemoteRoleFrom(const AActor* CopyFromActor)
{
	RemoteRole = CopyFromActor->GetRemoteRole();
	if (RemoteRole != ROLE_None)
	{
		GetWorld()->AddNetworkActor(this);
	}
}

void AActor::PostNetInit()
{
	if(RemoteRole != ROLE_Authority)
	{
		UE_LOG(LogActor, Warning, TEXT("AActor::PostNetInit %s Remoterole: %d"), *GetName(), (int)RemoteRole);
	}
	check(RemoteRole == ROLE_Authority);

	if (!HasActorBegunPlay())
	{
		const UWorld* MyWorld = GetWorld();
		if (MyWorld && MyWorld->HasBegunPlay())
		{
			SCOPE_CYCLE_COUNTER(STAT_ActorBeginPlay);
			DispatchBeginPlay();
		}
	}

	UpdateOverlaps();
}

void AActor::ExchangeNetRoles(bool bRemoteOwned)
{
	checkf(!HasAnyFlags(RF_ClassDefaultObject), TEXT("ExchangeNetRoles should never be called on a CDO as it causes issues when replicating actors over the network due to mutated transient data!"));

	if (!bExchangedRoles)
	{
		if (bRemoteOwned)
		{
			Exchange( Role, RemoteRole );
		}
		bExchangedRoles = true;
	}
}

void AActor::SwapRolesForReplay()
{
	Swap(Role, RemoteRole);
}

void AActor::DispatchBeginPlay()
{
	UWorld* World = (!HasActorBegunPlay() && !IsPendingKill() ? GetWorld() : nullptr);

	if (World)
	{
		const uint32 CurrentCallDepth = BeginPlayCallDepth++;

		BeginPlay();

		ensure(BeginPlayCallDepth - 1 == CurrentCallDepth);
		BeginPlayCallDepth = CurrentCallDepth;
	}
}

void AActor::BeginPlay()
{
	ensureMsgf(ActorHasBegunPlay == EActorBeginPlayState::HasNotBegunPlay, TEXT("BeginPlay was called on actor %s which was in state %d"), *GetPathName(), ActorHasBegunPlay);
	SetLifeSpan( InitialLifeSpan );
	RegisterAllActorTickFunctions(true, false); // Components are done below.

	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);

	ActorHasBegunPlay = EActorBeginPlayState::BeginningPlay;
	for (UActorComponent* Component : Components)
	{
		// bHasBegunPlay will be true for the component if the component was renamed and moved to a new outer during initialization
		if (Component->IsRegistered() && !Component->HasBegunPlay())
		{
			Component->RegisterAllComponentTickFunctions(true);
			Component->BeginPlay();
		}
		else
		{
			// When an Actor begins play we expect only the not bAutoRegister false components to not be registered
			//check(!Component->bAutoRegister);
		}
	}

	ReceiveBeginPlay();

	ActorHasBegunPlay = EActorBeginPlayState::HasBegunPlay;
}

void AActor::EnableInput(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		// If it doesn't exist create it and bind delegates
		if (!InputComponent)
		{
			InputComponent = NewObject<UInputComponent>(this);
			InputComponent->RegisterComponent();
			InputComponent->bBlockInput = bBlockInput;
			InputComponent->Priority = InputPriority;

			if (UInputDelegateBinding::SupportsInputDelegate(GetClass()))
			{
				UInputDelegateBinding::BindInputDelegates(GetClass(), InputComponent);
			}
		}
		else
		{
			// Make sure we only have one instance of the InputComponent on the stack
			PlayerController->PopInputComponent(InputComponent);
		}

		PlayerController->PushInputComponent(InputComponent);
	}
}

void AActor::DisableInput(APlayerController* PlayerController)
{
	if (InputComponent)
	{
		if (PlayerController)
		{
			PlayerController->PopInputComponent(InputComponent);
		}
		else
		{
			for (FConstPlayerControllerIterator PCIt = GetWorld()->GetPlayerControllerIterator(); PCIt; ++PCIt)
			{
				(*PCIt)->PopInputComponent(InputComponent);
			}
		}
	}
}

float AActor::GetInputAxisValue(const FName InputAxisName) const
{
	float Value = 0.f;

	if (InputComponent)
	{
		Value = InputComponent->GetAxisValue(InputAxisName);
	}

	return Value;
}

float AActor::GetInputAxisKeyValue(const FKey InputAxisKey) const
{
	float Value = 0.f;

	if (InputComponent)
	{
		Value = InputComponent->GetAxisKeyValue(InputAxisKey);
	}

	return Value;
}

FVector AActor::GetInputVectorAxisValue(const FKey InputAxisKey) const
{
	FVector Value;

	if (InputComponent)
	{
		Value = InputComponent->GetVectorAxisValue(InputAxisKey);
	}

	return Value;
}

bool AActor::SetActorLocation(const FVector& NewLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		const FVector Delta = NewLocation - GetActorLocation();
		return RootComponent->MoveComponent(Delta, GetActorQuat(), bSweep, OutSweepHitResult, MOVECOMP_NoFlags, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}

	return false;
}

bool AActor::SetActorRotation(FRotator NewRotation, ETeleportType Teleport)
{
#if ENABLE_NAN_DIAGNOSTIC
	if (NewRotation.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("AActor::SetActorRotation found NaN in FRotator NewRotation"));
		NewRotation = FRotator::ZeroRotator;
	}
#endif
	if (RootComponent)
	{
		return RootComponent->MoveComponent(FVector::ZeroVector, NewRotation, true, nullptr, MOVECOMP_NoFlags, Teleport);
	}

	return false;
}

bool AActor::SetActorRotation(const FQuat& NewRotation, ETeleportType Teleport)
{
#if ENABLE_NAN_DIAGNOSTIC
	if (NewRotation.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("AActor::SetActorRotation found NaN in FQuat NewRotation"));
	}
#endif
	if (RootComponent)
	{
		return RootComponent->MoveComponent(FVector::ZeroVector, NewRotation, true, nullptr, MOVECOMP_NoFlags, Teleport);
	}

	return false;
}

bool AActor::SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
#if ENABLE_NAN_DIAGNOSTIC
	if (NewRotation.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("AActor::SetActorLocationAndRotation found NaN in FRotator NewRotation"));
		NewRotation = FRotator::ZeroRotator;
	}
#endif
	if (RootComponent)
	{
		const FVector Delta = NewLocation - GetActorLocation();
		return RootComponent->MoveComponent(Delta, NewRotation, bSweep, OutSweepHitResult, MOVECOMP_NoFlags, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}

	return false;
}

bool AActor::SetActorLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
#if ENABLE_NAN_DIAGNOSTIC
	if (NewRotation.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("AActor::SetActorLocationAndRotation found NaN in FQuat NewRotation"));
	}
#endif
	if (RootComponent)
	{
		const FVector Delta = NewLocation - GetActorLocation();
		return RootComponent->MoveComponent(Delta, NewRotation, bSweep, OutSweepHitResult, MOVECOMP_NoFlags, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}

	return false;
}

void AActor::SetActorScale3D(FVector NewScale3D)
{
	if (RootComponent)
	{
		RootComponent->SetWorldScale3D(NewScale3D);
	}
}


FVector AActor::GetActorScale3D() const
{
	if (RootComponent)
	{
		return RootComponent->GetComponentScale();
	}
	return FVector(1,1,1);
}

void AActor::AddActorWorldOffset(FVector DeltaLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->AddWorldOffset(DeltaLocation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::AddActorWorldRotation(FRotator DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->AddWorldRotation(DeltaRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::AddActorWorldRotation(const FQuat& DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->AddWorldRotation(DeltaRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::AddActorWorldTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->AddWorldTransform(DeltaTransform, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}


bool AActor::SetActorTransform(const FTransform& NewTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	// we have seen this gets NAN from kismet, and would like to see if this
	// happens, and if so, something else is giving NAN as output
	if (RootComponent)
	{
		if (ensureMsgf(!NewTransform.ContainsNaN(), TEXT("SetActorTransform: Get NAN Transform data for %s: %s"), *GetNameSafe(this), *NewTransform.ToString()))
		{
			RootComponent->SetWorldTransform(NewTransform, bSweep, OutSweepHitResult, Teleport);
		}
		else
		{
			if (OutSweepHitResult)
			{
				*OutSweepHitResult = FHitResult();
			}
		}
		return true;
	}

	if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
	return false;
}

void AActor::AddActorLocalOffset(FVector DeltaLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if(RootComponent)
	{
		RootComponent->AddLocalOffset(DeltaLocation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::AddActorLocalRotation(FRotator DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if(RootComponent)
	{
		RootComponent->AddLocalRotation(DeltaRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::AddActorLocalRotation(const FQuat& DeltaRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->AddLocalRotation(DeltaRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::AddActorLocalTransform(const FTransform& NewTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if(RootComponent)
	{
		RootComponent->AddLocalTransform(NewTransform, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeLocation(NewRelativeLocation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::SetActorRelativeRotation(FRotator NewRelativeRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeRotation(NewRelativeRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::SetActorRelativeRotation(const FQuat& NewRelativeRotation, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeRotation(NewRelativeRotation, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::SetActorRelativeTransform(const FTransform& NewRelativeTransform, bool bSweep, FHitResult* OutSweepHitResult, ETeleportType Teleport)
{
	if (RootComponent)
	{
		RootComponent->SetRelativeTransform(NewRelativeTransform, bSweep, OutSweepHitResult, Teleport);
	}
	else if (OutSweepHitResult)
	{
		*OutSweepHitResult = FHitResult();
	}
}

void AActor::SetActorRelativeScale3D(FVector NewRelativeScale)
{
	if (RootComponent)
	{
		if (NewRelativeScale.ContainsNaN())
		{
			FMessageLog("Blueprint").Warning(FText::Format(LOCTEXT("InvalidScale", "Scale '{0}' is not valid."), FText::FromString(NewRelativeScale.ToString())));
			return;
		}

		RootComponent->SetRelativeScale3D(NewRelativeScale);
	}
}

FVector AActor::GetActorRelativeScale3D() const
{
	if (RootComponent)
	{
		return RootComponent->RelativeScale3D;
	}
	return FVector(1,1,1);
}

void AActor::SetActorHiddenInGame( bool bNewHidden )
{
	if (bHidden != bNewHidden)
	{
		bHidden = bNewHidden;
		MarkComponentsRenderStateDirty();
	}
}

void AActor::SetActorEnableCollision(bool bNewActorEnableCollision)
{
	if(bActorEnableCollision != bNewActorEnableCollision)
	{
		bActorEnableCollision = bNewActorEnableCollision;

		// Notify components about the change
		TInlineComponentArray<UActorComponent*> Components;
		GetComponents(Components);

		for(int32 CompIdx=0; CompIdx<Components.Num(); CompIdx++)
		{
			Components[CompIdx]->OnActorEnableCollisionChanged();
		}
	}
}


bool AActor::Destroy( bool bNetForce, bool bShouldModifyLevel )
{
	// It's already pending kill or in DestroyActor(), no need to beat the corpse
	if (!IsPendingKillPending())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->DestroyActor( this, bNetForce, bShouldModifyLevel );
		}
		else
		{
			UE_LOG(LogSpawn, Warning, TEXT("Destroying %s, which doesn't have a valid world pointer"), *GetPathName());
		}
	}

	return IsPendingKillPending();
}

void AActor::K2_DestroyActor()
{
	Destroy();
}

void AActor::K2_DestroyComponent(UActorComponent* Component)
{
	// If its a valid component, and we own it, destroy it
	if(Component && Component->GetOwner() == this)
	{
		Component->DestroyComponent();
	}
}

bool AActor::SetRootComponent(class USceneComponent* NewRootComponent)
{
	/** Only components owned by this actor can be used as a its root component. */
	if (ensure(NewRootComponent == nullptr || NewRootComponent->GetOwner() == this))
	{
		Modify();

		RootComponent = NewRootComponent;
		return true;
	}

	return false;
}

void AActor::GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent) const
{
	const FBox Bounds = GetComponentsBoundingBox(!bOnlyCollidingComponents);

	// To keep consistency with the other GetBounds functions, transform our result into an origin / extent formatting
	Bounds.GetCenterAndExtents(Origin, BoxExtent);
}


AWorldSettings * AActor::GetWorldSettings() const
{
	UWorld* World = GetWorld();
	return (World ? World->GetWorldSettings() : nullptr);
}

UNetDriver* GetNetDriver_Internal(UWorld* World, FName NetDriverName)
{
	if (NetDriverName == NAME_GameNetDriver)
	{
		return (World ? World->GetNetDriver() : nullptr);
	}

	return GEngine->FindNamedNetDriver(World, NetDriverName);
}

// Note: this is a private implementation that should no t be called directly except by the public wrappers (GetNetMode()) where some optimizations are inlined.
ENetMode AActor::InternalGetNetMode() const
{
	UWorld* World = GetWorld();
	UNetDriver* NetDriver = GetNetDriver_Internal(World, NetDriverName);
	if (NetDriver != nullptr)
	{
		return NetDriver->GetNetMode();
	}

	if (World != nullptr && World->DemoNetDriver != nullptr)
	{
		return World->DemoNetDriver->GetNetMode();
	}

	return NM_Standalone;
}

UNetDriver* AActor::GetNetDriver() const
{
	return GetNetDriver_Internal(GetWorld(), NetDriverName);
}

void AActor::SetNetDriverName(FName NewNetDriverName)
{
	if (NewNetDriverName != NetDriverName)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			World->RemoveNetworkActor(this);
		}

		NetDriverName = NewNetDriverName;

		if (World)
		{
			World->AddNetworkActor(this);
		}
	}
}

//
// Return whether a function should be executed remotely.
//
int32 AActor::GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack )
{
	// Quick reject 1.
	if ((Function->FunctionFlags & FUNC_Static))
	{
		// Call local
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Local1: %s"), *Function->GetName());
		return FunctionCallspace::Local;
	}

	if (GAllowActorScriptExecutionInEditor)
	{
		// Call local
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Local2: %s"), *Function->GetName());
		return FunctionCallspace::Local;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		// Call local
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Local3: %s"), *Function->GetName());
		return FunctionCallspace::Local;
	}

	// If we are on a client and function is 'skip on client', absorb it
	FunctionCallspace::Type Callspace = (Role < ROLE_Authority) && Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly) ? FunctionCallspace::Absorbed : FunctionCallspace::Local;
	
	if (IsPendingKill())
	{
		// Never call remote on a pending kill actor. 
		// We can call it local or absorb it depending on authority/role check above.
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace: IsPendingKill %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}

	if (Function->FunctionFlags & FUNC_NetRequest)
	{
		// Call remote
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace NetRequest: %s"), *Function->GetName());
		return FunctionCallspace::Remote;
	}	
	
	if (Function->FunctionFlags & FUNC_NetResponse)
	{
		if (Function->RPCId > 0)
		{
			// Call local
			DEBUG_CALLSPACE(TEXT("GetFunctionCallspace NetResponse Local: %s"), *Function->GetName());
			return FunctionCallspace::Local;
		}

		// Shouldn't happen, so skip call
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace NetResponse Absorbed: %s"), *Function->GetName());
		return FunctionCallspace::Absorbed;
	}

	const ENetMode NetMode = GetNetMode();
	// Quick reject 2. Has to be a network game to continue
	if (NetMode == NM_Standalone)
	{
		if (Role < ROLE_Authority && (Function->FunctionFlags & FUNC_NetServer))
		{
			// Don't let clients call server functions (in edge cases where NetMode is standalone (net driver is null))
			DEBUG_CALLSPACE(TEXT("GetFunctionCallspace No Authority Server Call Absorbed: %s"), *Function->GetName());
			return FunctionCallspace::Absorbed;
		}

		// Call local
		return FunctionCallspace::Local;
	}
	
	// Dedicated servers don't care about "cosmetic" functions.
	if (NetMode == NM_DedicatedServer && Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic))
	{
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Blueprint Cosmetic Absorbed: %s"), *Function->GetName());
		return FunctionCallspace::Absorbed;
	}

	if (!(Function->FunctionFlags & FUNC_Net))
	{
		// Not a network function
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Not Net: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}
	
	bool bIsServer = NetMode == NM_ListenServer || NetMode == NM_DedicatedServer;

	// get the top most function
	while (Function->GetSuperFunction() != NULL)
	{
		Function = Function->GetSuperFunction();
	}

	if ((Function->FunctionFlags & FUNC_NetMulticast))
	{
		if(bIsServer)
		{
			// Server should execute locally and call remotely
			if (RemoteRole != ROLE_None)
			{
				DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Multicast: %s"), *Function->GetName());
				return (FunctionCallspace::Local | FunctionCallspace::Remote);
			}

			DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Multicast NoRemoteRole: %s"), *Function->GetName());
			return FunctionCallspace::Local;
		}
		else
		{
			// Client should only execute locally iff it is allowed to (function is not KismetAuthorityOnly)
			DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Multicast Client: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
			return Callspace;
		}
	}

	// if we are the server, and it's not a send-to-client function,
	if (bIsServer && !(Function->FunctionFlags & FUNC_NetClient))
	{
		// don't replicate
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Server calling Server function: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}
	// if we aren't the server, and it's not a send-to-server function,
	if (!bIsServer && !(Function->FunctionFlags & FUNC_NetServer))
	{
		// don't replicate
		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Client calling Client function: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
		return Callspace;
	}

	// Check if the actor can potentially call remote functions	
	if (Role == ROLE_Authority)
	{
		UNetConnection* NetConnection = GetNetConnection();
		if (NetConnection == NULL)
		{
			UPlayer *ClientPlayer = GetNetOwningPlayer();
			if (ClientPlayer == NULL)
			{
				// Check if a player ever owned this (topmost owner is playercontroller or beacon)
				if (HasNetOwner())
				{
					// Network object with no owning player, we must absorb
					DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Client without owner absorbed %s"), *Function->GetName());
					return FunctionCallspace::Absorbed;
				}
				
				// Role authority object calling a client RPC locally (ie AI owned objects)
				DEBUG_CALLSPACE(TEXT("GetFunctionCallspace authority non client owner %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
				return Callspace;
			}
			else if (Cast<ULocalPlayer>(ClientPlayer) != NULL)
			{
				// This is a local player, call locally
				DEBUG_CALLSPACE(TEXT("GetFunctionCallspace Client local function: %s %s"), *Function->GetName(), FunctionCallspace::ToString(Callspace));
				return Callspace;
			}
		}
		else if (!NetConnection->Driver || !NetConnection->Driver->World)
		{
			// NetDriver does not have a world, most likely shutting down
			DEBUG_CALLSPACE(TEXT("GetFunctionCallspace NetConnection with no driver or world absorbed: %s %s %s"),
				*Function->GetName(), 
				NetConnection->Driver ? *NetConnection->Driver->GetName() : TEXT("NoNetDriver"),
				NetConnection->Driver && NetConnection->Driver->World ? *NetConnection->Driver->World->GetName() : TEXT("NoWorld"));
			return FunctionCallspace::Absorbed;
		}

		// There is a valid net connection, so continue and call remotely
	}

	// about to call remotely - unless the actor is not actually replicating
	if (RemoteRole == ROLE_None)
	{
		if (!bIsServer)
		{
			UE_LOG(LogNet, Warning, TEXT("Client is absorbing remote function %s on actor %s because RemoteRole is ROLE_None"), *Function->GetName(), *GetName() );
		}

		DEBUG_CALLSPACE(TEXT("GetFunctionCallspace RemoteRole None absorbed %s"), *Function->GetName());
		return FunctionCallspace::Absorbed;
	}

	// Call remotely
	DEBUG_CALLSPACE(TEXT("GetFunctionCallspace RemoteRole Remote %s"), *Function->GetName());
	return FunctionCallspace::Remote;
}

bool AActor::CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack )
{
	UNetDriver* NetDriver = GetNetDriver();
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(this, Function, Parameters, OutParms, Stack, NULL);
		return true;
	}

	return false;
}

void AActor::DispatchPhysicsCollisionHit(const FRigidBodyCollisionInfo& MyInfo, const FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData)
{
#if 0
	if(true)
	{
		FString MyName = MyInfo.Component ? FString(*MyInfo.Component->GetPathName()) : FString(TEXT(""));
		FString OtherName = OtherInfo.Component ? FString(*OtherInfo.Component->GetPathName()) : FString(TEXT(""));
		UE_LOG(LogPhysics, Log,  TEXT("COLLIDE! %s - %s"), *MyName, *OtherName );
	}
#endif

	checkSlow(RigidCollisionData.ContactInfos.Num() > 0);

	// @todo At the moment we only pass the first contact in the ContactInfos array. Maybe improve this?
	const FRigidBodyContactInfo& ContactInfo = RigidCollisionData.ContactInfos[0];

	FHitResult Result;
	Result.Location = Result.ImpactPoint = ContactInfo.ContactPosition;
	Result.Normal = Result.ImpactNormal = ContactInfo.ContactNormal;
	Result.PenetrationDepth = ContactInfo.ContactPenetration;
	Result.PhysMaterial = ContactInfo.PhysMaterial[1];
	Result.Actor = OtherInfo.Actor;
	Result.Component = OtherInfo.Component;
	Result.Item = OtherInfo.BodyIndex;
	Result.BoneName = OtherInfo.BoneName;
//#nv begin #Blast Pass our bone that was hit
	Result.OtherBoneName = MyInfo.BoneName;
//nv end
	Result.bBlockingHit = true;

	NotifyHit(MyInfo.Component.Get(), OtherInfo.Actor.Get(), OtherInfo.Component.Get(), true, Result.Location, Result.Normal, RigidCollisionData.TotalNormalImpulse, Result);

	// Execute delegates if bound

	if (OnActorHit.IsBound())
	{
		OnActorHit.Broadcast(this, OtherInfo.Actor.Get(), RigidCollisionData.TotalNormalImpulse, Result);
	}

	UPrimitiveComponent* MyInfoComponent = MyInfo.Component.Get();
	if (MyInfoComponent && MyInfoComponent->OnComponentHit.IsBound())
	{
		MyInfoComponent->OnComponentHit.Broadcast(MyInfoComponent, OtherInfo.Actor.Get(), OtherInfo.Component.Get(), RigidCollisionData.TotalNormalImpulse, Result);
	}
}

#if WITH_EDITOR
bool AActor::IsTemporarilyHiddenInEditor(const bool bIncludeParent) const
{
	if (bHiddenEdTemporary)
	{
		return true;
	}

	if (bIncludeParent)
	{
		if (UChildActorComponent* ParentCAC = ParentComponent.Get())
		{
			return ParentCAC->GetOwner()->IsTemporarilyHiddenInEditor(true);
		}
	}

	return false;
}
#endif

bool AActor::IsChildActor() const
{
	return ParentComponent.IsValid();
}

UChildActorComponent* AActor::GetParentComponent() const
{
	return ParentComponent.Get();
}

AActor* AActor::GetParentActor() const
{
	AActor* ParentActor = nullptr;
	if (UChildActorComponent* ParentComponentPtr = GetParentComponent())
	{
		ParentActor = ParentComponentPtr->GetOwner();
	}

	return ParentActor;
}

void AActor::GetAllChildActors(TArray<AActor*>& ChildActors, bool bIncludeDescendants) const
{
	TInlineComponentArray<UChildActorComponent*> ChildActorComponents(this);

	ChildActors.Reserve(ChildActors.Num() + ChildActorComponents.Num());
	for (UChildActorComponent* CAC : ChildActorComponents)
	{
		if (AActor* ChildActor = CAC->GetChildActor())
		{
			ChildActors.Add(ChildActor);
			if (bIncludeDescendants)
			{
				ChildActor->GetAllChildActors(ChildActors, true);
			}
		}
	}
}


// COMPONENTS

void AActor::UnregisterAllComponents(const bool bForReregister)
{
	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);

	for(UActorComponent* Component : Components)
	{
		if( Component->IsRegistered() && (!bForReregister || Component->AllowReregistration())) // In some cases unregistering one component can unregister another, so we do a check here to avoid trying twice
		{
			Component->UnregisterComponent();
		}
	}

	PostUnregisterAllComponents();
}

void AActor::RegisterAllComponents()
{
	// 0 - means register all components
	verify(IncrementalRegisterComponents(0));

	// Clear this flag as it's no longer deferred
	bHasDeferredComponentRegistration = false;
}

// Walks through components hierarchy and returns closest to root parent component that is unregistered
// Only for components that belong to the same owner
static USceneComponent* GetUnregisteredParent(UActorComponent* Component)
{
	USceneComponent* ParentComponent = nullptr;
	USceneComponent* SceneComponent = Cast<USceneComponent>(Component);
	
	while (	SceneComponent && 
			SceneComponent->GetAttachParent() &&
			SceneComponent->GetAttachParent()->GetOwner() == Component->GetOwner() &&
			!SceneComponent->GetAttachParent()->IsRegistered())
	{
		SceneComponent = SceneComponent->GetAttachParent();
		if (SceneComponent->bAutoRegister && !SceneComponent->IsPendingKill())
		{
			// We found unregistered parent that should be registered
			// But keep looking up the tree
			ParentComponent = SceneComponent;
		}
	}

	return ParentComponent;
}

bool AActor::IncrementalRegisterComponents(int32 NumComponentsToRegister)
{
	if (NumComponentsToRegister == 0)
	{
		// 0 - means register all components
		NumComponentsToRegister = MAX_int32;
	}

	UWorld* const World = GetWorld();
	check(World);

	// If we are not a game world, then register tick functions now. If we are a game world we wait until right before BeginPlay(),
	// so as to not actually tick until BeginPlay() executes (which could otherwise happen in network games).
	if (bAllowTickBeforeBeginPlay || !World->IsGameWorld())
	{
		RegisterAllActorTickFunctions(true, false); // components will be handled when they are registered
	}
	
	// Register RootComponent first so all other children components can reliably use it (i.e., call GetLocation) when they register
	if (RootComponent != NULL && !RootComponent->IsRegistered())
	{
#if PERF_TRACK_DETAILED_ASYNC_STATS
		FScopeCycleCounterUObject ContextScope(RootComponent);
#endif
		if (RootComponent->bAutoRegister)
		{
			// Before we register our component, save it to our transaction buffer so if "undone" it will return to an unregistered state.
			// This should prevent unwanted components hanging around when undoing a copy/paste or duplication action.
			RootComponent->Modify(false);

			RootComponent->RegisterComponentWithWorld(World);
		}
	}

	int32 NumTotalRegisteredComponents = 0;
	int32 NumRegisteredComponentsThisRun = 0;
	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);
	TSet<UActorComponent*> RegisteredParents;
	
	for (int32 CompIdx = 0; CompIdx < Components.Num() && NumRegisteredComponentsThisRun < NumComponentsToRegister; CompIdx++)
	{
		UActorComponent* Component = Components[CompIdx];
		if (!Component->IsRegistered() && Component->bAutoRegister && !Component->IsPendingKill())
		{
			// Ensure that all parent are registered first
			USceneComponent* UnregisteredParentComponent = GetUnregisteredParent(Component);
			if (UnregisteredParentComponent)
			{
				bool bParentAlreadyHandled = false;
				RegisteredParents.Add(UnregisteredParentComponent, &bParentAlreadyHandled);
				if (bParentAlreadyHandled)
				{
					UE_LOG(LogActor, Error, TEXT("AActor::IncrementalRegisterComponents parent component '%s' cannot be registered in actor '%s'"), *GetPathNameSafe(UnregisteredParentComponent), *GetPathName());
					break;
				}

				// Register parent first, then return to this component on a next iteration
				Component = UnregisteredParentComponent;
				CompIdx--;
				NumTotalRegisteredComponents--; // because we will try to register the parent again later...
			}
#if PERF_TRACK_DETAILED_ASYNC_STATS
			FScopeCycleCounterUObject ContextScope(Component);
#endif
				
			// Before we register our component, save it to our transaction buffer so if "undone" it will return to an unregistered state.
			// This should prevent unwanted components hanging around when undoing a copy/paste or duplication action.
			Component->Modify(false);

			Component->RegisterComponentWithWorld(World);
			NumRegisteredComponentsThisRun++;
		}

		NumTotalRegisteredComponents++;
	}

	// See whether we are done
	if (Components.Num() == NumTotalRegisteredComponents)
	{
#if PERF_TRACK_DETAILED_ASYNC_STATS
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AActor_IncrementalRegisterComponents_PostRegisterAllComponents);
#endif
		// Finally, call PostRegisterAllComponents
		PostRegisterAllComponents();
		return true;
	}
	
	// Still have components to register
	return false;
}

bool AActor::HasValidRootComponent()
{ 
	return (RootComponent != NULL && RootComponent->IsRegistered()); 
}

void AActor::MarkComponentsAsPendingKill()
{
	// Iterate components and mark them all as pending kill.
	TInlineComponentArray<UActorComponent*> Components(this);

	for (UActorComponent* Component : Components)
	{
		// Modify component so undo/ redo works in the editor.
		if (GIsEditor)
		{
			Component->Modify();
		}
		Component->OnComponentDestroyed(true);
		Component->MarkPendingKill();
	}
}

void AActor::ReregisterAllComponents()
{
	UnregisterAllComponents(true);
	RegisterAllComponents();
}

void AActor::UpdateComponentTransforms()
{
	for (UActorComponent* ActorComp : GetComponents())
	{
		if (ActorComp && ActorComp->IsRegistered())
		{
			ActorComp->UpdateComponentToWorld();
		}
	}
}

void AActor::MarkComponentsRenderStateDirty()
{
	for (UActorComponent* ActorComp : GetComponents())
	{
		if (ActorComp && ActorComp->IsRegistered())
		{
			ActorComp->MarkRenderStateDirty();
			if (UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(ActorComp))
			{
				if (ChildActorComponent->GetChildActor())
				{
					ChildActorComponent->GetChildActor()->MarkComponentsRenderStateDirty();
				}
			}
		}
	}
}

void AActor::InitializeComponents()
{
	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* ActorComp : Components)
	{
		if (ActorComp->IsRegistered())
		{
			if (ActorComp->bAutoActivate && !ActorComp->IsActive())
			{
				ActorComp->Activate(true);
			}

			if (ActorComp->bWantsInitializeComponent && !ActorComp->HasBeenInitialized())
			{
				// Broadcast the activation event since Activate occurs too early to fire a callback in a game
				ActorComp->InitializeComponent();
			}
		}
	}
}

void AActor::UninitializeComponents()
{
	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* ActorComp : Components)
	{
		if (ActorComp->HasBeenInitialized())
		{
			ActorComp->UninitializeComponent();
		}
	}
}

void AActor::DrawDebugComponents(FColor const& BaseColor) const
{
#if ENABLE_DRAW_DEBUG
	TInlineComponentArray<USceneComponent*> Components;
	GetComponents(Components);

	UWorld* MyWorld = GetWorld();

	for(int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
	{
		USceneComponent const* const Component = Components[ComponentIndex]; 

		FVector const Loc = Component->GetComponentLocation();
		FRotator const Rot = Component->GetComponentRotation();

		// draw coord system at component loc
		DrawDebugCoordinateSystem(MyWorld, Loc, Rot, 10.f);

		// draw line from me to my parent
		if (Component->GetAttachParent())
		{
			DrawDebugLine(MyWorld, Component->GetAttachParent()->GetComponentLocation(), Loc, BaseColor);
		}

		// draw component name
		DrawDebugString(MyWorld, Loc+FVector(0,0,32), *Component->GetName());
	}
#endif // ENABLE_DRAW_DEBUG
}


void AActor::InvalidateLightingCacheDetailed(bool bTranslationOnly)
{
	for (UActorComponent* Component : GetComponents())
	{
		if(Component && Component->IsRegistered())
		{
			Component->InvalidateLightingCacheDetailed(true, bTranslationOnly);
		}
	}
}

 // COLLISION

bool AActor::ActorLineTraceSingle(struct FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params)
{
	OutHit = FHitResult(1.f);
	OutHit.TraceStart = Start;
	OutHit.TraceEnd = End;
	bool bHasHit = false;
	
	TInlineComponentArray<UPrimitiveComponent*> Components;
	GetComponents(Components);

	for (int32 ComponentIndex=0; ComponentIndex<Components.Num(); ComponentIndex++)
	{
		FHitResult HitResult;
		UPrimitiveComponent* Primitive = Components[ComponentIndex];
		if( Primitive->IsRegistered() && Primitive->IsCollisionEnabled() 
			&& (Primitive->GetCollisionResponseToChannel(TraceChannel) == ECollisionResponse::ECR_Block) 
			&& Primitive->LineTraceComponent(HitResult, Start, End, Params) )
		{
			// return closest hit
			if( HitResult.Time < OutHit.Time )
			{
				OutHit = HitResult;
				bHasHit = true;
			}
		}
	}

	return bHasHit;
}

float AActor::ActorGetDistanceToCollision(const FVector& Point, ECollisionChannel TraceChannel, FVector& ClosestPointOnCollision, UPrimitiveComponent** OutPrimitiveComponent) const
{
	ClosestPointOnCollision = Point;
	float ClosestPointDistanceSqr = -1.f;

	TInlineComponentArray<UPrimitiveComponent*> Components;
	GetComponents(Components);

	for (int32 ComponentIndex=0; ComponentIndex<Components.Num(); ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = Components[ComponentIndex];
		if( Primitive->IsRegistered() && Primitive->IsCollisionEnabled() 
			&& (Primitive->GetCollisionResponseToChannel(TraceChannel) == ECollisionResponse::ECR_Block) )
		{
			FVector ClosestPoint;
			float DistanceSqr = -1.f;

			if (!Primitive->GetSquaredDistanceToCollision(Point, DistanceSqr, ClosestPoint))
			{
				// Invalid result, impossible to be better than ClosestPointDistance
				continue;
			}

			if( (ClosestPointDistanceSqr < 0.f) || (DistanceSqr < ClosestPointDistanceSqr) )
			{
				ClosestPointDistanceSqr = DistanceSqr;
				ClosestPointOnCollision = ClosestPoint;
				if( OutPrimitiveComponent )
				{
					*OutPrimitiveComponent = Primitive;
				}

				// If we're inside collision, we're not going to find anything better, so abort search we've got our best find.
				if( DistanceSqr <= KINDA_SMALL_NUMBER )
				{
					break;
				}
			}
		}
	}

	return (ClosestPointDistanceSqr > 0.f ? FMath::Sqrt(ClosestPointDistanceSqr) : ClosestPointDistanceSqr);
}


void AActor::LifeSpanExpired()
{
	Destroy();
}

void AActor::SetLifeSpan( float InLifespan )
{
	// Store the new value
	InitialLifeSpan = InLifespan;
	// Initialize a timer for the actors lifespan if there is one. Otherwise clear any existing timer
	if ((Role == ROLE_Authority || bTearOff) && !IsPendingKill())
	{
		if( InLifespan > 0.0f)
		{
			GetWorldTimerManager().SetTimer( TimerHandle_LifeSpanExpired, this, &AActor::LifeSpanExpired, InLifespan );
		}
		else
		{
			GetWorldTimerManager().ClearTimer( TimerHandle_LifeSpanExpired );		
		}
	}
}

float AActor::GetLifeSpan() const
{
	// Timer remaining returns -1.0f if there is no such timer - return this as ZERO
	const float CurrentLifespan = GetWorldTimerManager().GetTimerRemaining(TimerHandle_LifeSpanExpired);
	return ( CurrentLifespan != -1.0f ) ? CurrentLifespan : 0.0f;
}

void AActor::PostInitializeComponents()
{
	if( !IsPendingKill() )
	{
		bActorInitialized = true;

		UNavigationSystem::OnActorRegistered(this);
		
		UpdateAllReplicatedComponents();
	}
}

void AActor::PreInitializeComponents()
{
	if (AutoReceiveInput != EAutoReceiveInput::Disabled)
	{
		const int32 PlayerIndex = int32(AutoReceiveInput.GetValue()) - 1;

		APlayerController* PC = UGameplayStatics::GetPlayerController(this, PlayerIndex);
		if (PC)
		{
			EnableInput(PC);
		}
		else
		{
			GetWorld()->PersistentLevel->RegisterActorForAutoReceiveInput(this, PlayerIndex);
		}
	}
}

float AActor::GetActorTimeDilation() const
{
	// get actor custom time dilation
	// if you do slomo, that changes WorldSettings->TimeDilation
	// So multiply to get final TimeDilation
	return CustomTimeDilation*GetWorldSettings()->GetEffectiveTimeDilation();
}

UMaterialInstanceDynamic* AActor::MakeMIDForMaterial(class UMaterialInterface* Parent)
{
	// Deprecating this function. 
	// Please use PrimitiveComponent->CreateAndSetMaterialInstanceDynamic
	// OR PrimitiveComponent->CreateAndSetMaterialInstanceDynamicFromMaterial
	// OR UMaterialInstanceDynamic::Create

	return NULL;
}

float AActor::GetDistanceTo(const AActor* OtherActor) const
{
	return OtherActor ? (GetActorLocation() - OtherActor->GetActorLocation()).Size() : 0.f;
}

float AActor::GetSquaredDistanceTo(const AActor* OtherActor) const
{
	return OtherActor ? (GetActorLocation() - OtherActor->GetActorLocation()).SizeSquared() : 0.f;
}

float AActor::GetHorizontalDistanceTo(const AActor* OtherActor) const
{
	return OtherActor ? (GetActorLocation() - OtherActor->GetActorLocation()).Size2D() : 0.f;
}

float AActor::GetVerticalDistanceTo(const AActor* OtherActor) const
{
	return OtherActor ? FMath::Abs((GetActorLocation().Z - OtherActor->GetActorLocation().Z)) : 0.f;
}

float AActor::GetDotProductTo(const AActor* OtherActor) const
{
	if (OtherActor)
	{
		FVector Dir = GetActorForwardVector();
		FVector Offset = OtherActor->GetActorLocation() - GetActorLocation();
		Offset = Offset.GetSafeNormal();
		return FVector::DotProduct(Dir, Offset);
	}
	return -2.0;
}

float AActor::GetHorizontalDotProductTo(const AActor* OtherActor) const
{
	if (OtherActor)
	{
		FVector Dir = GetActorForwardVector();
		FVector Offset = OtherActor->GetActorLocation() - GetActorLocation();
		Offset = Offset.GetSafeNormal2D();
		return FVector::DotProduct(Dir, Offset);
	}
	return -2.0;
}


#if WITH_EDITOR
const int32 AActor::GetNumUncachedStaticLightingInteractions() const
{
	if (GetRootComponent())
	{
		return GetRootComponent()->GetNumUncachedStaticLightingInteractions();
	}
	return 0;
}
#endif // WITH_EDITOR


// K2 versions of various transform changing operations.
// Note: we pass null for the hit result if not sweeping, for better perf.
// This assumes this K2 function is only used by blueprints, which initializes the param for each function call.

bool AActor::K2_SetActorLocation(FVector NewLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	return SetActorLocation(NewLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

bool AActor::K2_SetActorRotation(FRotator NewRotation, bool bTeleportPhysics)
{
	return SetActorRotation(NewRotation, TeleportFlagToEnum(bTeleportPhysics));
}

bool AActor::K2_SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	return SetActorLocationAndRotation(NewLocation, NewRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_AddActorWorldOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddActorWorldOffset(DeltaLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_AddActorWorldRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddActorWorldRotation(DeltaRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_AddActorWorldTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddActorWorldTransform(DeltaTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

bool AActor::K2_SetActorTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	return SetActorTransform(NewTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_AddActorLocalOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddActorLocalOffset(DeltaLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_AddActorLocalRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddActorLocalRotation(DeltaRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_AddActorLocalTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	AddActorLocalTransform(NewTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetActorRelativeLocation(NewRelativeLocation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_SetActorRelativeRotation(FRotator NewRelativeRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetActorRelativeRotation(NewRelativeRotation, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

void AActor::K2_SetActorRelativeTransform(const FTransform& NewRelativeTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport)
{
	SetActorRelativeTransform(NewRelativeTransform, bSweep, (bSweep ? &SweepHitResult : nullptr), TeleportFlagToEnum(bTeleport));
}

float AActor::GetGameTimeSinceCreation()
{
	if (UWorld* MyWorld = GetWorld())
	{
		return MyWorld->GetTimeSeconds() - CreationTime;		
	}
	// return 0.f if GetWorld return's null
	else
	{
		return 0.f;
	}
}

void AActor::SetNetUpdateTime( float NewUpdateTime )
{
	FNetworkObjectInfo* NetActor = GetNetworkObjectInfo();

	if ( NetActor != nullptr )
	{
		// Only allow the next update to be sooner than the current one
		NetActor->NextUpdateTime = FMath::Min( NetActor->NextUpdateTime, (double)NewUpdateTime );
	}			
}

FNetworkObjectInfo* AActor::GetNetworkObjectInfo() const
{
	UWorld* World = GetWorld();

	if ( World != nullptr )
	{
		UNetDriver* NetDriver = World->GetNetDriver();

		if ( NetDriver != nullptr )
		{
			return NetDriver->GetNetworkObjectInfo( this );
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
