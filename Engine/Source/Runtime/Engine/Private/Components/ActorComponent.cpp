// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// ActorComponent.cpp: Actor component implementation.

#include "Components/ActorComponent.h"
#include "Misc/App.h"
#include "EngineStats.h"
#include "UObject/UObjectIterator.h"
#include "Engine/MemberReference.h"
#include "ComponentInstanceDataCache.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ContentStreaming.h"
#include "ComponentReregisterContext.h"
#include "Engine/AssetUserData.h"
#include "Engine/LevelStreamingPersistent.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectHash.h"
#include "Engine/NetDriver.h"
#include "Net/UnrealNetwork.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Engine/SimpleConstructionScript.h"
#include "ComponentUtils.h"

#if WITH_EDITOR
#include "Kismet2/ComponentEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "ActorComponent"

DEFINE_LOG_CATEGORY(LogActorComponent);

DECLARE_CYCLE_STAT(TEXT("RegisterComponent"), STAT_RegisterComponent, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("UnregisterComponent"), STAT_UnregisterComponent, STATGROUP_Component);

DECLARE_CYCLE_STAT(TEXT("Component OnRegister"), STAT_ComponentOnRegister, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("Component OnUnregister"), STAT_ComponentOnUnregister, STATGROUP_Component);

DECLARE_CYCLE_STAT(TEXT("Component CreateRenderState"), STAT_ComponentCreateRenderState, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("Component DestroyRenderState"), STAT_ComponentDestroyRenderState, STATGROUP_Component);

DECLARE_CYCLE_STAT(TEXT("Component CreatePhysicsState"), STAT_ComponentCreatePhysicsState, STATGROUP_Component);
DECLARE_CYCLE_STAT(TEXT("Component DestroyPhysicsState"), STAT_ComponentDestroyPhysicsState, STATGROUP_Component);

// Should we tick latent actions fired for a component at the same time as the component?
// - Non-zero values behave the same way as actors do, ticking pending latent action when the component ticks, instead of later on in the frame
// - Prior to 4.16, components behaved as if the value were 0, which meant their latent actions behaved differently to actors
//DEPRECATED(4.16, "This CVar will be removed, with the behavior permanently changing in the future to always tick component latent actions along with the component")
int32 GTickComponentLatentActionsWithTheComponent = 1;

// Should we tick latent actions fired for a component at the same time as the component?
FAutoConsoleVariableRef GTickComponentLatentActionsWithTheComponentCVar(
	TEXT("t.TickComponentLatentActionsWithTheComponent"),
	GTickComponentLatentActionsWithTheComponent,
	TEXT("Should we tick latent actions fired for a component at the same time as the component?\n")
	TEXT(" 0: Tick component latent actions later on in the frame (behavior prior to 4.16, provided for games relying on the old behavior but will be removed in the future)\n")
	TEXT(" 1: Tick component latent actions at the same time as the component (default)"));

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

#if WITH_EDITOR
FUObjectAnnotationSparseBool GSelectedComponentAnnotation;
#endif

/** Static var indicating activity of reregister context */
int32 FGlobalComponentReregisterContext::ActiveGlobalReregisterContextCount = 0;

FGlobalComponentReregisterContext::FGlobalComponentReregisterContext()
{
	ActiveGlobalReregisterContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach all actor components.
	for(UActorComponent* Component : TObjectRange<UActorComponent>())
	{
		new(ComponentContexts) FComponentReregisterContext(Component);
	}
}

FGlobalComponentReregisterContext::FGlobalComponentReregisterContext(const TArray<UClass*>& ExcludeComponents)
{
	ActiveGlobalReregisterContextCount++;

	// wait until resources are released
	FlushRenderingCommands();

	// Detach only actor components that are not in the excluded list
	for (UActorComponent* Component : TObjectRange<UActorComponent>())
	{
		bool bShouldReregister=true;
		for (UClass* ExcludeClass : ExcludeComponents)
		{
			if( ExcludeClass &&
				Component->IsA(ExcludeClass) )
			{
				bShouldReregister = false;
				break;
			}
		}
		if( bShouldReregister )
		{
			new(ComponentContexts) FComponentReregisterContext(Component);		
		}
	}
}

FGlobalComponentReregisterContext::~FGlobalComponentReregisterContext()
{
	check(ActiveGlobalReregisterContextCount > 0);
	// We empty the array now, to ensure that the FComponentReregisterContext destructors are called while ActiveGlobalReregisterContextCount still indicates activity
	ComponentContexts.Empty();
	ActiveGlobalReregisterContextCount--;
}

FGlobalComponentRecreateRenderStateContext::FGlobalComponentRecreateRenderStateContext()
{
	// wait until resources are released
	FlushRenderingCommands();

	// recreate render state for all components.
	for (UActorComponent* Component : TObjectRange<UActorComponent>())
	{
		new(ComponentContexts) FComponentRecreateRenderStateContext(Component);
	}
}

FGlobalComponentRecreateRenderStateContext::~FGlobalComponentRecreateRenderStateContext()
{
	ComponentContexts.Empty();
}

// Create Physics global delegate
FActorComponentGlobalCreatePhysicsSignature UActorComponent::GlobalCreatePhysicsDelegate;
// Destroy Physics global delegate
FActorComponentGlobalDestroyPhysicsSignature UActorComponent::GlobalDestroyPhysicsDelegate;

const FString UActorComponent::ComponentTemplateNameSuffix(TEXT("_GEN_VARIABLE"));

UActorComponent::UActorComponent(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	OwnerPrivate = GetTypedOuter<AActor>();

	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.SetTickFunctionEnable(false);

	CreationMethod = EComponentCreationMethod::Native;

	bAllowReregistration = true;
	bAutoRegister = true;
	bNetAddressable = false;
	bEditableWhenInherited = true;
#if WITH_EDITOR
	bCanUseCachedOwner = true;
#endif

	bCanEverAffectNavigation = false;
	bNavigationRelevant = false;
}

void UActorComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Instance components will be added during the owner's initialization
	if (OwnerPrivate && CreationMethod != EComponentCreationMethod::Instance)
	{
		if (!FPlatformProperties::RequiresCookedData() && CreationMethod == EComponentCreationMethod::Native && HasAllFlags(RF_NeedLoad|RF_DefaultSubObject))
		{
			UObject* MyArchetype = GetArchetype();
			if (!MyArchetype->IsPendingKill() && MyArchetype != GetClass()->ClassDefaultObject)
			{
				OwnerPrivate->AddOwnedComponent(this);
			}
			else
			{
				// else: this is a natively created component that thinks its archetype is the CDO of
				// this class, rather than a template component and this isn't the template component.
				// Delete this stale component:
				MarkPendingKill();
			}
		}
		else
		{
			OwnerPrivate->AddOwnedComponent(this);
		}
	}
}

void UActorComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (GetLinkerUE4Version() < VER_UE4_ACTOR_COMPONENT_CREATION_METHOD)
	{
		if (IsTemplate())
		{
			CreationMethod = EComponentCreationMethod::Native;
		}
		else if (bCreatedByConstructionScript_DEPRECATED)
		{
			CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
		}
		else if (bInstanceComponent_DEPRECATED)
		{
			CreationMethod = EComponentCreationMethod::Instance;
		}

		if (CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
		{
			UBlueprintGeneratedClass* Class = CastChecked<UBlueprintGeneratedClass>(GetOuter()->GetClass());
			while (Class)
			{
				USimpleConstructionScript* SCS = Class->SimpleConstructionScript;
				if (SCS != nullptr && SCS->FindSCSNode(GetFName()))
				{
					break;
				}
				else
				{
					Class = Cast<UBlueprintGeneratedClass>(Class->GetSuperClass());
					if (Class == nullptr)
					{
						CreationMethod = EComponentCreationMethod::UserConstructionScript;
					}
				}
			}
		}
	}
#endif

	if (CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	{
		if ((GetLinkerUE4Version() < VER_UE4_TRACK_UCS_MODIFIED_PROPERTIES) && !HasAnyFlags(RF_ClassDefaultObject))
		{
			DetermineUCSModifiedProperties();
		}
	}
	else
	{
		// For a brief period of time we were inadvertently storing these for all components, need to clear it out
		UCSModifiedProperties.Empty();
	}
}

bool UActorComponent::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	bRoutedPostRename = false;

	const FName OldName = GetFName();
	const UObject* OldOuter = GetOuter();
	
	const bool bRenameSuccessful = Super::Rename(InName, NewOuter, Flags);
	
	const bool bMoved = (OldName != GetFName()) || (OldOuter != GetOuter());
	if (!bRoutedPostRename && ((Flags & REN_Test) == 0) && bMoved)
	{
		UE_LOG(LogActorComponent, Fatal, TEXT("%s failed to route PostRename.  Please call Super::PostRename() in your <className>::PostRename() function. "), *GetFullName() );
	}

	return bRenameSuccessful;
}

void UActorComponent::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	if (OldOuter != GetOuter())
	{
		OwnerPrivate = GetTypedOuter<AActor>();
		AActor* OldOwner = (OldOuter->IsA<AActor>() ? static_cast<AActor*>(OldOuter) : OldOuter->GetTypedOuter<AActor>());

		if (OwnerPrivate != OldOwner)
		{
			if (OldOwner)
			{
				OldOwner->RemoveOwnedComponent(this);
			}
			if (OwnerPrivate)
			{
				OwnerPrivate->AddOwnedComponent(this);
			}

			TArray<UObject*> Children;
			GetObjectsWithOuter(this, Children, /*bIncludeNestedObjects=*/false);

			for (int32 Index = 0; Index < Children.Num(); ++Index)
			{
				UObject* Child = Children[Index];

				// Cut off if we have a nested Actor
				if (!Child->IsA<AActor>())
				{
					if (UActorComponent* ChildComponent = Cast<UActorComponent>(Child))
					{
						ChildComponent->OwnerPrivate = OwnerPrivate;
						if (OldOwner)
						{
							OldOwner->RemoveOwnedComponent(ChildComponent);
						}
						if (OwnerPrivate)
						{
							OwnerPrivate->AddOwnedComponent(ChildComponent);
						}
					}
					GetObjectsWithOuter(Child, Children, /*bIncludeNestedObjects=*/false);
				}
			}
		}
	}

	bRoutedPostRename = true;
}

bool UActorComponent::IsCreatedByConstructionScript() const
{
	return ((CreationMethod == EComponentCreationMethod::SimpleConstructionScript) || (CreationMethod == EComponentCreationMethod::UserConstructionScript));
}

#if WITH_EDITOR
void UActorComponent::CheckForErrors()
{
	if (AActor* MyOwner = GetOwner())
	{
		if (GetClass()->HasAnyClassFlags(CLASS_Deprecated))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ComponentName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("OwnerName"), FText::FromString(MyOwner->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(MyOwner))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_DeprecatedClass", "{ComponentName}::{OwnerName} is obsolete and must be removed (Class is deprecated)" ), Arguments ) ) )
				->AddToken(FMapErrorToken::Create(FMapErrors::DeprecatedClass));
		}

		if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ComponentName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("OwnerName"), FText::FromString(MyOwner->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(MyOwner))
				->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_AbstractClass", "{ComponentName}::{OwnerName} is obsolete and must be removed (Class is abstract)" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::AbstractClass));
		}
	}
}
#endif

bool UActorComponent::IsOwnerSelected() const
{
	AActor* MyOwner = GetOwner();
	return MyOwner && MyOwner->IsSelected();
}

UWorld* UActorComponent::GetWorld_Uncached() const
{
	UWorld* ComponentWorld = nullptr;

	AActor* MyOwner = GetOwner();
	// If we don't have a world yet, it may be because we haven't gotten registered yet, but we can try to look at our owner
	if (MyOwner && !MyOwner->HasAnyFlags(RF_ClassDefaultObject))
	{
		ComponentWorld = MyOwner->GetWorld();
	}

	if( ComponentWorld == nullptr )
	{
		// As a fallback check the outer of this component for a world. In some cases components are spawned directly in the world
		ComponentWorld = Cast<UWorld>(GetOuter());
	}

	return ComponentWorld;
}

bool UActorComponent::ComponentHasTag(FName Tag) const
{
	return (Tag != NAME_None) && ComponentTags.Contains(Tag);
}


ENetMode UActorComponent::InternalGetNetMode() const
{
	AActor* MyOwner = GetOwner();
	return MyOwner ? MyOwner->GetNetMode() : NM_Standalone;
}

FSceneInterface* UActorComponent::GetScene() const
{
	return (WorldPrivate ? WorldPrivate->Scene : NULL);
}

ULevel* UActorComponent::GetComponentLevel() const
{
	// For model components Level is outer object
	AActor* MyOwner = GetOwner();
	return (MyOwner ? Cast<ULevel>(MyOwner->GetOuter()) : Cast<ULevel>( GetOuter() ) );
}

bool UActorComponent::ComponentIsInLevel(const ULevel *TestLevel) const
{
	return (GetComponentLevel() == TestLevel);
}

bool UActorComponent::ComponentIsInPersistentLevel(bool bIncludeLevelStreamingPersistent) const
{
	ULevel* MyLevel = GetComponentLevel();
	UWorld* MyWorld = GetWorld();

	if (MyLevel == NULL || MyWorld == NULL)
	{
		return false;
	}

	return ( (MyLevel == MyWorld->PersistentLevel) || ( bIncludeLevelStreamingPersistent && MyWorld->StreamingLevels.Num() > 0 &&
														Cast<ULevelStreamingPersistent>(MyWorld->StreamingLevels[0]) != NULL &&
														MyWorld->StreamingLevels[0]->GetLoadedLevel() == MyLevel ) );
}

FString UActorComponent::GetReadableName() const
{
	FString Result = GetNameSafe(GetOwner()) + TEXT(".") + GetName();
	UObject const *Add = AdditionalStatObject();
	if (Add)
	{
		Result += TEXT(" ");
		Add->AppendName(Result);
	}
	return Result;
}

void UActorComponent::BeginDestroy()
{
	if (bHasBegunPlay)
	{
		EndPlay(EEndPlayReason::Destroyed);
	}

	// Ensure that we call UninitializeComponent before we destroy this component
	if (bHasBeenInitialized)
	{
		UninitializeComponent();
	}

	ExecuteUnregisterEvents();

	// Ensure that we call OnComponentDestroyed before we destroy this component
	if (bHasBeenCreated)
	{
		OnComponentDestroyed(GExitPurge);
	}

	WorldPrivate = nullptr;

	// Remove from the parent's OwnedComponents list
	if (AActor* MyOwner = GetOwner())
	{
		MyOwner->RemoveOwnedComponent(this);
	}

	Super::BeginDestroy();
}

bool UActorComponent::NeedsLoadForClient() const
{
	check(GetOuter());
	// For Component Blueprints, avoid calling into the class to avoid recursion
	bool bNeedsLoadOuter = HasAnyFlags(RF_ClassDefaultObject) || GetOuter()->NeedsLoadForClient();
	return (!IsEditorOnly() && bNeedsLoadOuter && Super::NeedsLoadForClient());
}

bool UActorComponent::NeedsLoadForServer() const
{
	check(GetOuter());
	// For Component Blueprints, avoid calling into the class to avoid recursion
	bool bNeedsLoadOuter = HasAnyFlags(RF_ClassDefaultObject) || GetOuter()->NeedsLoadForServer();
	return (!IsEditorOnly() && bNeedsLoadOuter && Super::NeedsLoadForServer());
}

int32 UActorComponent::GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack )
{
	AActor* MyOwner = GetOwner();
	return (MyOwner ? MyOwner->GetFunctionCallspace(Function, Parameters, Stack) : FunctionCallspace::Local);
}

bool UActorComponent::CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack )
{
	if (AActor* MyOwner = GetOwner())
	{
		UNetDriver* NetDriver = MyOwner->GetNetDriver();
		if (NetDriver)
		{
			NetDriver->ProcessRemoteFunction(MyOwner, Function, Parameters, OutParms, Stack, this);
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR

/** FComponentReregisterContexts for components which have had PreEditChange called but not PostEditChange. */
static TMap<TWeakObjectPtr<UActorComponent>,FComponentReregisterContext*> EditReregisterContexts;

bool UActorComponent::Modify( bool bAlwaysMarkDirty/*=true*/ )
{
	// If this is a construction script component we don't store them in the transaction buffer.  Instead, mark
	// the Actor as modified so that we store of the transaction annotation that has the component properties stashed
	AActor* MyOwner = GetOwner();
	if (MyOwner && IsCreatedByConstructionScript())
	{
		return MyOwner->Modify(bAlwaysMarkDirty);
	}

	return Super::Modify(bAlwaysMarkDirty);
}

void UActorComponent::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if(IsRegistered())
	{
		// The component or its outer could be pending kill when calling PreEditChange when applying a transaction.
		// Don't do do a full recreate in this situation, and instead simply detach.
		if( !IsPendingKill() )
		{
			check(!EditReregisterContexts.Find(this));
			EditReregisterContexts.Add(this,new FComponentReregisterContext(this));
		}
		else
		{
			ExecuteUnregisterEvents();
			WorldPrivate = nullptr;
		}
	}

	// Flush rendering commands to ensure the rendering thread processes the component detachment before it is modified.
	FlushRenderingCommands();
}

void UActorComponent::PreEditUndo()
{
	Super::PreEditUndo();

	OwnerPrivate = nullptr;
	bCanUseCachedOwner = false;
}

void UActorComponent::PostEditUndo()
{
	// Objects marked pending kill don't call PostEditChange() from UObject::PostEditUndo(),
	// so they can leave an EditReregisterContexts entry around if they are deleted by an undo action.
	if( IsPendingKill() )
	{
		// For the redo case, ensure that we're no longer in the OwnedComponents array.
		if (AActor* OwningActor = GetOwner())
		{
			OwningActor->RemoveOwnedComponent(this);
		}

		// The reregister context won't bother attaching components that are 'pending kill'. 
		FComponentReregisterContext* ReregisterContext = nullptr;
		if (EditReregisterContexts.RemoveAndCopyValue(this, ReregisterContext))
		{
			delete ReregisterContext;
		}
		else
		{
			// This means there are likely some stale elements left in there now, strip them out
			for (auto It(EditReregisterContexts.CreateIterator()); It; ++It)
			{
				if (!It.Key().IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
	}
	else
	{
		bIsBeingDestroyed = false;

		OwnerPrivate = GetTypedOuter<AActor>();
		bCanUseCachedOwner = true;

		// Let the component be properly registered, after it was restored.
		if (OwnerPrivate)
		{
			OwnerPrivate->AddOwnedComponent(this);
		}

		TArray<UObject*> Children;
		GetObjectsWithOuter(this, Children, /*bIncludeNestedObjects=*/false);

		for (int32 Index = 0; Index < Children.Num(); ++Index)
		{
			UObject* Child = Children[Index];

			// Cut off if we have a nested Actor
			if (!Child->IsA<AActor>())
			{
				if (UActorComponent* ChildComponent = Cast<UActorComponent>(Child))
				{
					if (ChildComponent->OwnerPrivate)
					{
						ChildComponent->OwnerPrivate->RemoveOwnedComponent(ChildComponent);
					}
					ChildComponent->OwnerPrivate = OwnerPrivate;
					if (OwnerPrivate)
					{
						OwnerPrivate->AddOwnedComponent(ChildComponent);
					}
				}
				GetObjectsWithOuter(Child, Children, /*bIncludeNestedObjects=*/false);
			}
		}

		if (UWorld* MyWorld = GetWorld())
		{
			MyWorld->UpdateActorComponentEndOfFrameUpdateState(this);
		}
	}
	Super::PostEditUndo();
}

bool UActorComponent::IsSelectedInEditor() const
{
	return !IsPendingKill() && GSelectedComponentAnnotation.Get(this);
}

void UActorComponent::ConsolidatedPostEditChange(const FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_CanEverAffectNavigation = GET_MEMBER_NAME_CHECKED(UActorComponent, bCanEverAffectNavigation);

	FComponentReregisterContext* ReregisterContext = nullptr;
	if(EditReregisterContexts.RemoveAndCopyValue(this, ReregisterContext))
	{
		delete ReregisterContext;

		AActor* MyOwner = GetOwner();
		if ( MyOwner && !MyOwner->IsTemplate() && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
		{
			MyOwner->RerunConstructionScripts();
		}
	}
	else
	{
		// This means there are likely some stale elements left in there now, strip them out
		for (auto It(EditReregisterContexts.CreateIterator()); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetFName() == NAME_CanEverAffectNavigation)
	{
		HandleCanEverAffectNavigationChange(/*bForce=*/true);
	}

	// The component or its outer could be pending kill when calling PostEditChange when applying a transaction.
	// Don't do do a full recreate in this situation, and instead simply detach.
	if( IsPendingKill() )
	{
		// @todo UE4 james should this call UnregisterComponent instead to remove itself from the RegisteredComponents array on the owner?
		ExecuteUnregisterEvents();
		WorldPrivate = nullptr;
	}
}

void UActorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ConsolidatedPostEditChange(PropertyChangedEvent);
}

void UActorComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	ConsolidatedPostEditChange(PropertyChangedEvent);
}


#endif // WITH_EDITOR

void UActorComponent::OnRegister()
{
	checkf(!IsUnreachable(), TEXT("%s"), *GetDetailedInfo());
	checkf(!GetOuter()->IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo());
	checkf(!IsTemplate(), TEXT("'%s' (%s)"), *GetOuter()->GetFullName(), *GetDetailedInfo() );
	checkf(WorldPrivate, TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), GetOwner() ? *GetOwner()->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!bRegistered, TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), GetOwner() ? *GetOwner()->GetFullName() : TEXT("*** No Owner ***") );
	checkf(!IsPendingKill(), TEXT("OnRegister: %s to %s"), *GetDetailedInfo(), GetOwner() ? *GetOwner()->GetFullName() : TEXT("*** No Owner ***") );

	bRegistered = true;

	UpdateComponentToWorld();

	if (bAutoActivate)
	{
		AActor* Owner = GetOwner();
		if (!WorldPrivate->IsGameWorld() || Owner == nullptr || Owner->IsActorInitialized())
		{
			Activate(true);
		}
	}
}

void UActorComponent::OnUnregister()
{
	check(bRegistered);
	bRegistered = false;

	ClearNeedEndOfFrameUpdate();
}

void UActorComponent::InitializeComponent()
{
	check(bRegistered);
	check(!bHasBeenInitialized);

	bHasBeenInitialized = true;
}

void UActorComponent::UninitializeComponent()
{
	check(bHasBeenInitialized);

	bHasBeenInitialized = false;
}

void UActorComponent::BeginPlay()
{
	check(bRegistered);
	check(!bHasBegunPlay);
	checkSlow(bTickFunctionsRegistered); // If this fails, someone called BeginPlay() without first calling RegisterAllComponentTickFunctions().

	ReceiveBeginPlay();

	bHasBegunPlay = true;
}

void UActorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	check(bHasBegunPlay);

	// If we're in the process of being garbage collected it is unsafe to call out to blueprints
	if (!HasAnyFlags(RF_BeginDestroyed) && !IsUnreachable())
	{
		ReceiveEndPlay(EndPlayReason);
	}

	bHasBegunPlay = false;
}

FActorComponentInstanceData* UActorComponent::GetComponentInstanceData() const
{
	FActorComponentInstanceData* InstanceData = new FActorComponentInstanceData(this);

	if (!InstanceData->ContainsSavedProperties())
	{
		delete InstanceData;
		InstanceData = nullptr;
	}

	return InstanceData;
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	ExecuteTickHelper(Target, Target->bTickInEditor, DeltaTime, TickType, [this, TickType](float DilatedTime)
	{
		Target->TickComponent(DilatedTime, TickType, this);
	});
}

FString FActorComponentTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[TickComponent]");
}

bool UActorComponent::SetupActorComponentTickFunction(struct FTickFunction* TickFunction)
{
	if(TickFunction->bCanEverTick && !IsTemplate())
	{
		AActor* MyOwner = GetOwner();
		if (!MyOwner || !MyOwner->IsTemplate())
		{
			ULevel* ComponentLevel = (MyOwner ? MyOwner->GetLevel() : GetWorld()->PersistentLevel);
			TickFunction->SetTickFunctionEnable(TickFunction->bStartWithTickEnabled || TickFunction->IsTickFunctionEnabled());
			TickFunction->RegisterTickFunction(ComponentLevel);
			return true;
		}
	}
	return false;
}

void UActorComponent::SetComponentTickEnabled(bool bEnabled)
{
	if (PrimaryComponentTick.bCanEverTick && !IsTemplate())
	{
		PrimaryComponentTick.SetTickFunctionEnable(bEnabled);
	}
}

void UActorComponent::SetComponentTickEnabledAsync(bool bEnabled)
{
	if (PrimaryComponentTick.bCanEverTick && !IsTemplate())
	{
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.SetComponentTickEnabledAsync"),
			STAT_FSimpleDelegateGraphTask_SetComponentTickEnabledAsync,
			STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UActorComponent::SetComponentTickEnabled, bEnabled),
			GET_STATID(STAT_FSimpleDelegateGraphTask_SetComponentTickEnabledAsync), NULL, ENamedThreads::GameThread
		);
	}
}

bool UActorComponent::IsComponentTickEnabled() const
{
	return PrimaryComponentTick.IsTickFunctionEnabled();
}

void UActorComponent::SetComponentTickInterval(float TickInterval)
{
	PrimaryComponentTick.TickInterval = TickInterval;
}

float UActorComponent::GetComponentTickInterval() const
{
	return PrimaryComponentTick.TickInterval;
}

static UActorComponent* GTestRegisterComponentTickFunctions = NULL;

void UActorComponent::RegisterComponentTickFunctions(bool bRegister)
{
	if(bRegister)
	{
		if (SetupActorComponentTickFunction(&PrimaryComponentTick))
		{
			PrimaryComponentTick.Target = this;
		}
	}
	else
	{
		if(PrimaryComponentTick.IsTickFunctionRegistered())
		{
			PrimaryComponentTick.UnRegisterTickFunction();
		}
	}

	GTestRegisterComponentTickFunctions = this; // we will verify the super call chain is intact. Don't not copy paste this to a derived class!
}

void UActorComponent::RegisterAllComponentTickFunctions(bool bRegister)
{
	check(GTestRegisterComponentTickFunctions == NULL);
	// Components don't have tick functions until they are registered with the world
	if (bRegistered)
	{
		// Prevent repeated redundant attempts
		if (bTickFunctionsRegistered != bRegister)
		{
			RegisterComponentTickFunctions(bRegister);
			bTickFunctionsRegistered = bRegister;
			checkf(GTestRegisterComponentTickFunctions == this, TEXT("Failed to route component RegisterTickFunctions (%s)"), *GetFullName());
			GTestRegisterComponentTickFunctions = NULL;
		}
	}
}

void UActorComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	check(bRegistered);

	ReceiveTick(DeltaTime);

	if (GTickComponentLatentActionsWithTheComponent)
	{
		// Update any latent actions we have for this component, this will update even if paused if bUpdateWhilePaused is enabled
		// If this tick is skipped on a frame because we've got a TickInterval, our latent actions will be ticked
		// anyway by UWorld::Tick(). Given that, our latent actions don't need to be passed a larger
		// DeltaSeconds to make up the frames that they missed (because they wouldn't have missed any).
		// So pass in the world's DeltaSeconds value rather than our specific DeltaSeconds value.
		if (UWorld* ComponentWorld = GetWorld())
		{
			ComponentWorld->GetLatentActionManager().ProcessLatentActions(this, ComponentWorld->GetDeltaSeconds());
		}
	}
}

void UActorComponent::RegisterComponentWithWorld(UWorld* InWorld)
{
	SCOPE_CYCLE_COUNTER(STAT_RegisterComponent);
	FScopeCycleCounterUObject ComponentScope(this);

	checkf(!IsUnreachable(), TEXT("%s"), *GetFullName());

	if(IsPendingKill())
	{
		UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) Trying to register component with IsPendingKill() == true. Aborting."), *GetPathName());
		return;
	}

	// If the component was already registered, do nothing
	if(IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) Already registered. Aborting."), *GetPathName());
		return;
	}

	if(InWorld == nullptr)
	{
		//UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) NULL InWorld specified. Aborting."), *GetPathName());
		return;
	}

	// If not registered, should not have a scene
	checkf(WorldPrivate == nullptr, TEXT("%s"), *GetFullName());

	AActor* MyOwner = GetOwner();
	checkSlow(MyOwner == nullptr || MyOwner->OwnsComponent(this));

	if (MyOwner && MyOwner->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: Owner belongs to a DEADCLASS"));
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Can only register with an Actor if we are created within one
	if(MyOwner)
	{
		checkf(!MyOwner->IsUnreachable(), TEXT("%s"), *GetFullName());
		// can happen with undo because the owner will be restored "next"
		//checkf(!MyOwner->IsPendingKill(), TEXT("%s"), *GetFullName());

		if(InWorld != MyOwner->GetWorld())
		{
			// The only time you should specify a scene that is not Owner->GetWorld() is when you don't have an Actor
			UE_LOG(LogActorComponent, Log, TEXT("RegisterComponentWithWorld: (%s) Specifying a world, but an Owner Actor found, and InWorld is not GetOwner()->GetWorld()"), *GetPathName());
		}
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	if (!bHasBeenCreated)
	{
		OnComponentCreated();
	}

	WorldPrivate = InWorld;

	ExecuteRegisterEvents();

	// If not in a game world register ticks now, otherwise defer until BeginPlay. If no owner we won't trigger BeginPlay either so register now in that case as well.
	if (!InWorld->IsGameWorld())
	{
		RegisterAllComponentTickFunctions(true);
	}
	else if (MyOwner == nullptr)
	{
		if (!bHasBeenInitialized && bWantsInitializeComponent)
		{
			InitializeComponent();
		}

		RegisterAllComponentTickFunctions(true);
	}
	else
	{
		if (!bHasBeenInitialized && bWantsInitializeComponent && MyOwner->IsActorInitialized())
		{
			InitializeComponent();
		}

		if (MyOwner->HasActorBegunPlay() || MyOwner->IsActorBeginningPlay())
		{
			RegisterAllComponentTickFunctions(true);
			if (!bHasBegunPlay)
			{
				BeginPlay();
			}
		}
	}

	// If this is a blueprint created component and it has component children they can miss getting registered in some scenarios
	if (IsCreatedByConstructionScript())
	{
		TArray<UObject*> Children;
		GetObjectsWithOuter(this, Children, true, RF_NoFlags, EInternalObjectFlags::PendingKill);

		for (UObject* Child : Children)
		{
			UActorComponent* ChildComponent = Cast<UActorComponent>(Child);
			if (ChildComponent && !ChildComponent->IsRegistered() && ChildComponent->GetOwner() == MyOwner)
			{
				ChildComponent->RegisterComponentWithWorld(InWorld);
			}
		}

	}
}

void UActorComponent::RegisterComponent()
{
	AActor* MyOwner = GetOwner();
	UWorld* MyOwnerWorld = (MyOwner ? MyOwner->GetWorld() : nullptr);
	if (ensure(MyOwnerWorld))
	{
		RegisterComponentWithWorld(MyOwnerWorld);
	}
}

void UActorComponent::UnregisterComponent()
{
	SCOPE_CYCLE_COUNTER(STAT_UnregisterComponent);
	FScopeCycleCounterUObject ComponentScope(this);

	// Do nothing if not registered
	if(!IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("UnregisterComponent: (%s) Not registered. Aborting."), *GetPathName());
		return;
	}

	// If registered, should have a world
	checkf(WorldPrivate != nullptr, TEXT("%s"), *GetFullName());

	RegisterAllComponentTickFunctions(false);
	ExecuteUnregisterEvents();

	WorldPrivate = nullptr;
}

void UActorComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	// Avoid re-entrancy
	if (bIsBeingDestroyed)
	{
		return;
	}

	bIsBeingDestroyed = true;

	if (bHasBegunPlay)
	{
		EndPlay(EEndPlayReason::Destroyed);
	}

	// Ensure that we call UninitializeComponent before we destroy this component
	if (bHasBeenInitialized)
	{
		UninitializeComponent();
	}

	// Unregister if registered
	if(IsRegistered())
	{
		UnregisterComponent();
	}

	// Then remove from Components array, if we have an Actor
	if(AActor* MyOwner = GetOwner())
	{
		if (IsCreatedByConstructionScript())
		{
			MyOwner->BlueprintCreatedComponents.Remove(this);
		}
		else
		{
			MyOwner->RemoveInstanceComponent(this);
		}
		MyOwner->RemoveOwnedComponent(this);
		if (MyOwner->GetRootComponent() == this)
		{
			MyOwner->SetRootComponent(NULL);
		}
	}

	// Tell the component it is being destroyed
	OnComponentDestroyed(false);

	// Finally mark pending kill, to NULL out any other refs
	MarkPendingKill();
}

void UActorComponent::OnComponentCreated()
{
	ensure(!bHasBeenCreated);
	bHasBeenCreated = true;
}

void UActorComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// @TODO: Would be nice to ensure(bHasBeenCreated), but there are still many places where components are created without calling OnComponentCreated
	bHasBeenCreated = false;
}

void UActorComponent::K2_DestroyComponent(UObject* Object)
{
	AActor* MyOwner = GetOwner();
	if (bAllowAnyoneToDestroyMe || Object == this || MyOwner == NULL || MyOwner == Object)
	{
		DestroyComponent();
	}
	else
	{
		// TODO: Put in Message Log
		UE_LOG(LogActorComponent, Error, TEXT("May not destroy component %s owned by %s."), *GetFullName(), *MyOwner->GetFullName());
	}
}

void UActorComponent::CreateRenderState_Concurrent()
{
	check(IsRegistered());
	check(WorldPrivate->Scene);
	check(!bRenderStateCreated);
	bRenderStateCreated = true;

	bRenderStateDirty = false;
	bRenderTransformDirty = false;
	bRenderDynamicDataDirty = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("CreateRenderState_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::SendRenderTransform_Concurrent()
{
	check(bRenderStateCreated);
	bRenderTransformDirty = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("SendRenderTransform_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::SendRenderDynamicData_Concurrent()
{
	check(bRenderStateCreated);
	bRenderDynamicDataDirty = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("SendRenderDynamicData_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::DestroyRenderState_Concurrent()
{
	check(bRenderStateCreated);
	bRenderStateCreated = false;

#if LOG_RENDER_STATE
	UE_LOG(LogActorComponent, Log, TEXT("DestroyRenderState_Concurrent: %s"), *GetPathName());
#endif
}

void UActorComponent::OnCreatePhysicsState()
{
	check(IsRegistered());
	check(ShouldCreatePhysicsState());
	check(WorldPrivate->GetPhysicsScene());
	check(!bPhysicsStateCreated);
	bPhysicsStateCreated = true;
}

void UActorComponent::OnDestroyPhysicsState()
{
	ensure(bPhysicsStateCreated);
	bPhysicsStateCreated = false;
}


void UActorComponent::CreatePhysicsState()
{
	SCOPE_CYCLE_COUNTER(STAT_ComponentCreatePhysicsState);

	if (!bPhysicsStateCreated && WorldPrivate->GetPhysicsScene() && ShouldCreatePhysicsState())
	{
		// Call virtual
		OnCreatePhysicsState();

		checkf(bPhysicsStateCreated, TEXT("Failed to route OnCreatePhysicsState (%s)"), *GetFullName());

		// Broadcast delegate
		GlobalCreatePhysicsDelegate.Broadcast(this);
	}
}

void UActorComponent::DestroyPhysicsState()
{
	SCOPE_CYCLE_COUNTER(STAT_ComponentDestroyPhysicsState);

	if (bPhysicsStateCreated)
	{
		// Broadcast delegate
		GlobalDestroyPhysicsDelegate.Broadcast(this);

		ensureMsgf(bRegistered, TEXT("Component has physics state when not registered (%s)"), *GetFullName()); // should not have physics state unless we are registered

		// Call virtual
		OnDestroyPhysicsState();

		checkf(!bPhysicsStateCreated, TEXT("Failed to route OnDestroyPhysicsState (%s)"), *GetFullName());
		checkf(!HasValidPhysicsState(), TEXT("Failed to destroy physics state (%s)"), *GetFullName());
	}
}

void UActorComponent::ExecuteRegisterEvents()
{
	if(!bRegistered)
	{
		SCOPE_CYCLE_COUNTER(STAT_ComponentOnRegister);
		OnRegister();
		checkf(bRegistered, TEXT("Failed to route OnRegister (%s)"), *GetFullName());
	}

	if(FApp::CanEverRender() && !bRenderStateCreated && WorldPrivate->Scene && ShouldCreateRenderState())
	{
		SCOPE_CYCLE_COUNTER(STAT_ComponentCreateRenderState);
		CreateRenderState_Concurrent();
		checkf(bRenderStateCreated, TEXT("Failed to route CreateRenderState_Concurrent (%s)"), *GetFullName());
	}

	CreatePhysicsState();
}


void UActorComponent::ExecuteUnregisterEvents()
{
	DestroyPhysicsState();

	if(bRenderStateCreated)
	{
		SCOPE_CYCLE_COUNTER(STAT_ComponentDestroyRenderState);
		checkf(bRegistered, TEXT("Component has render state when not registered (%s)"), *GetFullName());
		DestroyRenderState_Concurrent();
		checkf(!bRenderStateCreated, TEXT("Failed to route DestroyRenderState_Concurrent (%s)"), *GetFullName());
	}

	if(bRegistered)
	{
		SCOPE_CYCLE_COUNTER(STAT_ComponentOnUnregister);
		OnUnregister();
		checkf(!bRegistered, TEXT("Failed to route OnUnregister (%s)"), *GetFullName());
	}
}

void UActorComponent::ReregisterComponent()
{
	if(!IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("ReregisterComponent: (%s) Not currently registered. Aborting."), *GetPathName());
		return;
	}

	FComponentReregisterContext(this);
}

void UActorComponent::RecreateRenderState_Concurrent()
{
	if(bRenderStateCreated)
	{
		check(IsRegistered()); // Should never have render state unless registered
		DestroyRenderState_Concurrent();
		checkf(!bRenderStateCreated, TEXT("Failed to route DestroyRenderState_Concurrent (%s)"), *GetFullName());
	}

	if(IsRegistered() && WorldPrivate->Scene)
	{
		CreateRenderState_Concurrent();
		checkf(bRenderStateCreated, TEXT("Failed to route CreateRenderState_Concurrent (%s)"), *GetFullName());
	}
}

void UActorComponent::RecreatePhysicsState()
{
	DestroyPhysicsState();

	if (IsRegistered())
	{
		CreatePhysicsState();
	}
}

void UActorComponent::SetTickGroup(ETickingGroup NewTickGroup)
{
	PrimaryComponentTick.TickGroup = NewTickGroup;
}


void UActorComponent::AddTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrimaryComponentTick.bCanEverTick && PrerequisiteActor && PrerequisiteActor->PrimaryActorTick.bCanEverTick)
	{
		PrimaryComponentTick.AddPrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void UActorComponent::AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrimaryComponentTick.bCanEverTick && PrerequisiteComponent && PrerequisiteComponent->PrimaryComponentTick.bCanEverTick)
	{
		PrimaryComponentTick.AddPrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

void UActorComponent::RemoveTickPrerequisiteActor(AActor* PrerequisiteActor)
{
	if (PrerequisiteActor)
	{
		PrimaryComponentTick.RemovePrerequisite(PrerequisiteActor, PrerequisiteActor->PrimaryActorTick);
	}
}

void UActorComponent::RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent)
{
	if (PrerequisiteComponent)
	{
		PrimaryComponentTick.RemovePrerequisite(PrerequisiteComponent, PrerequisiteComponent->PrimaryComponentTick);
	}
}

void UActorComponent::DoDeferredRenderUpdates_Concurrent()
{
	checkf(!IsUnreachable(), TEXT("%s"), *GetFullName());
	checkf(!IsTemplate(), TEXT("%s"), *GetFullName());
	checkf(!IsPendingKill(), TEXT("%s"), *GetFullName());

	FScopeCycleCounterUObject ContextScope(this);

	if(!IsRegistered())
	{
		UE_LOG(LogActorComponent, Log, TEXT("UpdateComponent: (%s) Not registered, Aborting."), *GetPathName());
		return;
	}

	if(bRenderStateDirty)
	{
		SCOPE_CYCLE_COUNTER(STAT_PostTickComponentRecreate);
		RecreateRenderState_Concurrent();
		checkf(!bRenderStateDirty, TEXT("Failed to route CreateRenderState_Concurrent (%s)"), *GetFullName());
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_PostTickComponentLW);
		if(bRenderTransformDirty)
		{
			// Update the component's transform if the actor has been moved since it was last updated.
			SendRenderTransform_Concurrent();
		}

		if(bRenderDynamicDataDirty)
		{
			SendRenderDynamicData_Concurrent();
		}
	}
}


void UActorComponent::MarkRenderStateDirty()
{
	// If registered and has a render state to mark as dirty
	if(IsRegistered() && bRenderStateCreated && (!bRenderStateDirty || !GetWorld()))
	{
		// Flag as dirty
		bRenderStateDirty = true;
		MarkForNeededEndOfFrameRecreate();
	}
}


void UActorComponent::MarkRenderTransformDirty()
{
	if (IsRegistered() && bRenderStateCreated)
	{
		bRenderTransformDirty = true;
		MarkForNeededEndOfFrameUpdate();
	}
}


void UActorComponent::MarkRenderDynamicDataDirty()
{
	// If registered and has a render state to mark as dirty
	if(IsRegistered() && bRenderStateCreated)
	{
		// Flag as dirty
		bRenderDynamicDataDirty = true;
		MarkForNeededEndOfFrameUpdate();
	}
}

void UActorComponent::MarkForNeededEndOfFrameUpdate()
{
	if (bNeverNeedsRenderUpdate)
	{
		return;
	}

	UWorld* ComponentWorld = GetWorld();
	if (ComponentWorld)
	{
		ComponentWorld->MarkActorComponentForNeededEndOfFrameUpdate(this, RequiresGameThreadEndOfFrameUpdates());
	}
	else if (!IsUnreachable())
	{
		// we don't have a world, do it right now.
		DoDeferredRenderUpdates_Concurrent();
	}
}

void UActorComponent::ClearNeedEndOfFrameUpdate_Internal()
{
	// If this is being garbage collected we don't really need to worry about clearing this
	if (!HasAnyFlags(RF_BeginDestroyed) && !IsUnreachable())
	{
		if (UWorld* World = GetWorld())
		{
			World->ClearActorComponentEndOfFrameUpdate(this);
		}
	}
}

void UActorComponent::MarkForNeededEndOfFrameRecreate()
{
	if (bNeverNeedsRenderUpdate)
	{
		return;
	}

	UWorld* ComponentWorld = GetWorld();
	if (ComponentWorld)
	{
		// by convention, recreates are always done on the gamethread
		ComponentWorld->MarkActorComponentForNeededEndOfFrameUpdate(this, RequiresGameThreadEndOfFrameRecreate());
	}
	else if (!IsUnreachable())
	{
		// we don't have a world, do it right now.
		DoDeferredRenderUpdates_Concurrent();
	}
}

bool UActorComponent::RequiresGameThreadEndOfFrameUpdates() const
{
	return false;
}

bool UActorComponent::RequiresGameThreadEndOfFrameRecreate() const
{
	return true;
}

void UActorComponent::Activate(bool bReset)
{
	if (bReset || ShouldActivate()==true)
	{
		SetComponentTickEnabled(true);
		bIsActive = true;

		OnComponentActivated.Broadcast(this, bReset);
	}
}

void UActorComponent::Deactivate()
{
	if (ShouldActivate()==false)
	{
		SetComponentTickEnabled(false);
		bIsActive = false;

		OnComponentDeactivated.Broadcast(this);
	}
}

bool UActorComponent::ShouldActivate() const
{
	// if not active, should activate
	return !bIsActive;
}

void UActorComponent::SetActive(bool bNewActive, bool bReset)
{
	// if it wants to activate
	if (bNewActive)
	{
		// make sure to check if it should activate
		Activate(bReset);	
	}
	// otherwise, make sure it shouldn't activate
	else 
	{
		Deactivate();
	}
}

void UActorComponent::SetAutoActivate(bool bNewAutoActivate)
{
	if (!bRegistered || IsOwnerRunningUserConstructionScript())
	{
		bAutoActivate = bNewAutoActivate;
	}
	else
	{
		UE_LOG(LogActorComponent, Warning, TEXT("SetAutoActivate called on component %s after construction!"), *GetFullName());
	}
}

void UActorComponent::ToggleActive()
{
	SetActive(!bIsActive);
}

void UActorComponent::SetTickableWhenPaused(bool bTickableWhenPaused)
{
	PrimaryComponentTick.bTickEvenWhenPaused = bTickableWhenPaused;
}

bool UActorComponent::IsOwnerRunningUserConstructionScript() const
{
	AActor* MyOwner = GetOwner();
	return (MyOwner && MyOwner->bRunningUserConstructionScript);
}

void UActorComponent::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UActorComponent::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UActorComponent::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

void UActorComponent::SetNetAddressable()
{
	bNetAddressable = true;
}

bool UActorComponent::IsNameStableForNetworking() const
{
	/** 
	 * IsNameStableForNetworking means a component can be referred to its path name (relative to owning AActor*) over the network
	 *
	 * Components are net addressable if:
	 *	-They are Default Subobjects (created in C++ constructor)
	 *	-They were loaded directly from a package (placed in map actors)
	 *	-They were explicitly set to bNetAddressable (blueprint components created by SCS)
	 */

	return bNetAddressable || Super::IsNameStableForNetworking();
}

bool UActorComponent::IsSupportedForNetworking() const
{
	return GetIsReplicated() || IsNameStableForNetworking();
}

void UActorComponent::SetIsReplicated(bool ShouldReplicate)
{
	if (bReplicates != ShouldReplicate)
	{
		if (GetComponentClassCanReplicate())
		{
			bReplicates = ShouldReplicate;

			if (AActor* MyOwner = GetOwner())
			{
				MyOwner->UpdateReplicatedComponent( this );
			}
		}
		else
		{
			UE_LOG(LogActorComponent, Error, TEXT("Calling SetIsReplicated on component of Class '%s' which cannot replicate."), *GetClass()->GetName());
		}
	}
}

bool UActorComponent::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	return false;
}

void UActorComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if (BPClass != NULL)
	{
		BPClass->InstancePreReplication(this, ChangedPropertyTracker);
	}
}

bool UActorComponent::GetComponentClassCanReplicate() const
{
	return true;
}

ENetRole UActorComponent::GetOwnerRole() const
{
	AActor* MyOwner = GetOwner();
	return (MyOwner ? MyOwner->Role.GetValue() : ROLE_None);
}

void UActorComponent::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if (BPClass != NULL)
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}

	DOREPLIFETIME( UActorComponent, bIsActive );
	DOREPLIFETIME( UActorComponent, bReplicates );
}

void UActorComponent::OnRep_IsActive()
{
	SetComponentTickEnabled(bIsActive);
}

bool UActorComponent::IsEditableWhenInherited() const
{
	bool bCanEdit = bEditableWhenInherited;
	if (bCanEdit)
	{
#if WITH_EDITOR
		if (CreationMethod == EComponentCreationMethod::Native && !IsTemplate())
		{
			bCanEdit = FComponentEditorUtils::CanEditNativeComponent(this);
		}
		else
#endif
			if (CreationMethod == EComponentCreationMethod::UserConstructionScript)
		{
			bCanEdit = false;
		}
	}
	return bCanEdit;
}

void UActorComponent::DetermineUCSModifiedProperties()
{
	UCSModifiedProperties.Empty();

	if (CreationMethod == EComponentCreationMethod::SimpleConstructionScript)
	{
		class FComponentPropertySkipper : public FArchive
		{
		public:
			FComponentPropertySkipper()
				: FArchive()
			{
				ArIsSaving = true;

				// Include properties that would normally skip tagged serialization (e.g. bulk serialization of array properties).
				ArPortFlags |= PPF_ForceTaggedSerialization;
			}

			virtual bool ShouldSkipProperty(const UProperty* InProperty) const override
			{
				return (    InProperty->HasAnyPropertyFlags(CPF_Transient)
						|| !InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_Interp));
			}
		} PropertySkipper;

		UClass* ComponentClass = GetClass();
		UObject* ComponentArchetype = GetArchetype();

		for (TFieldIterator<UProperty> It(ComponentClass); It; ++It)
		{
			UProperty* Property = *It;
			if( Property->ShouldSerializeValue(PropertySkipper) )
			{
				for( int32 Idx=0; Idx<Property->ArrayDim; Idx++ )
				{
					uint8* DataPtr      = Property->ContainerPtrToValuePtr           <uint8>((uint8*)this, Idx);
					uint8* DefaultValue = Property->ContainerPtrToValuePtrForDefaults<uint8>(ComponentClass, (uint8*)ComponentArchetype, Idx);
					if (!Property->Identical( DataPtr, DefaultValue))
					{
						UCSModifiedProperties.Add(FSimpleMemberReference());
						FMemberReference::FillSimpleMemberReference<UProperty>(Property, UCSModifiedProperties.Last());
						break;
					}
				}
			}
		}
	}
}

void UActorComponent::GetUCSModifiedProperties(TSet<const UProperty*>& ModifiedProperties) const
{
	for (const FSimpleMemberReference& MemberReference : UCSModifiedProperties)
	{
		ModifiedProperties.Add(FMemberReference::ResolveSimpleMemberReference<UProperty>(MemberReference));
	}
}

void UActorComponent::RemoveUCSModifiedProperties(const TArray<UProperty*>& Properties)
{
	for (UProperty* Property : Properties)
	{
		FSimpleMemberReference MemberReference;
		FMemberReference::FillSimpleMemberReference<UProperty>(Property, MemberReference);
		UCSModifiedProperties.RemoveSwap(MemberReference);
	}
}

void UActorComponent::SetCanEverAffectNavigation(bool bRelevant)
{
	if (bCanEverAffectNavigation != bRelevant)
	{
		bCanEverAffectNavigation = bRelevant;

		HandleCanEverAffectNavigationChange();
	}
}

void UActorComponent::HandleCanEverAffectNavigationChange(bool bForceUpdate)
{
	// update octree if already registered
	if (bRegistered || bForceUpdate)
	{
		if (bCanEverAffectNavigation)
		{
			bNavigationRelevant = IsNavigationRelevant();
			UNavigationSystem::OnComponentRegistered(this);
		}
		else
		{
			UNavigationSystem::OnComponentUnregistered(this);
		}
	}
}

void UActorComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading() && (Ar.HasAnyPortFlags(PPF_DuplicateForPIE)||!Ar.HasAnyPortFlags(PPF_Duplicate)) && !IsTemplate())
	{
		bHasBeenCreated = true;
	}
}

AActor* UActorComponent::GetActorOwnerNoninline() const
{
	// This is defined out-of-line because AActor isn't defined where the inlined function is.

	return GetTypedOuter<AActor>();
}

#undef LOCTEXT_NAMESPACE
