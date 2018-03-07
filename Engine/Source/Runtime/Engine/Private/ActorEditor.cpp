// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreDelegates.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorSupportDelegates.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "LevelUtils.h"
#include "Misc/MapErrors.h"
#include "ActorEditorUtils.h"
#include "EngineGlobals.h"

#if WITH_EDITOR

#include "Editor.h"

#define LOCTEXT_NAMESPACE "ErrorChecking"

void AActor::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	UObjectProperty* ObjProp = Cast<UObjectProperty>(PropertyThatWillChange);
	UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(GetClass());
	if ( BPGC != nullptr && ObjProp != nullptr )
	{
		BPGC->UnbindDynamicDelegatesForProperty(this, ObjProp);
	}

	// During SIE, allow components to be unregistered here, and then reregistered and reconstructed in PostEditChangeProperty.
	if ((GEditor && GEditor->bIsSimulatingInEditor) || ReregisterComponentsWhenModified())
	{
		UnregisterAllComponents();
	}
}

static FName Name_RelativeLocation = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation);
static FName Name_RelativeRotation = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation);
static FName Name_RelativeScale3D = GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D);

void AActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	FName PropertyName = PropertyThatChanged != NULL ? PropertyThatChanged->GetFName() : NAME_None;
	
	const bool bTransformationChanged = (PropertyName == Name_RelativeLocation || PropertyName == Name_RelativeRotation || PropertyName == Name_RelativeScale3D);

	// During SIE, allow components to reregistered and reconstructed in PostEditChangeProperty.
	// This is essential as construction is deferred during spawning / duplication when in SIE.
	if ((GEditor && GEditor->bIsSimulatingInEditor && GetWorld() != nullptr) || ReregisterComponentsWhenModified())
	{
		// In the Undo case we have an annotation storing information about constructed components and we do not want
		// to improperly apply out of date changes so we need to skip registration of all blueprint created components
		// and defer instance components attached to them until after rerun
		if (CurrentTransactionAnnotation.IsValid())
		{
			UnregisterAllComponents();

			TInlineComponentArray<UActorComponent*> Components;
			GetComponents(Components);

			Components.Sort([](UActorComponent& A, UActorComponent& B)
			{
				if (&B == B.GetOwner()->GetRootComponent())
				{
					return false;
				}
				if (USceneComponent* ASC = Cast<USceneComponent>(&A))
				{
					if (ASC->GetAttachParent() == &B)
					{
						return false;
					}
				}
				return true;
			});

			bool bRequiresReregister = false;
			for (UActorComponent* Component : Components)
			{
				if (Component->CreationMethod == EComponentCreationMethod::Native)
				{
					Component->RegisterComponent();
				}
				else if (Component->CreationMethod == EComponentCreationMethod::Instance)
				{
					USceneComponent* SC = Cast<USceneComponent>(Component);
					if (SC == nullptr || SC == RootComponent || (SC->GetAttachParent() && SC->GetAttachParent()->IsRegistered()))
					{
						Component->RegisterComponent();
					}
					else
					{
						bRequiresReregister = true;
					}
				}
				else
				{
					bRequiresReregister = true;
				}
			}

			RerunConstructionScripts();

			if (bRequiresReregister)
			{
				ReregisterAllComponents();
			}
		}
		else
		{
			UnregisterAllComponents();
			RerunConstructionScripts();
			ReregisterAllComponents();
		}
	}

	// Let other systems know that an actor was moved
	if (bTransformationChanged)
	{
		GEngine->BroadcastOnActorMoved( this );
	}

	FEditorSupportDelegates::UpdateUI.Broadcast();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AActor::PostEditMove(bool bFinished)
{
	if ( ReregisterComponentsWhenModified() && !FLevelUtils::IsMovingLevel())
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
		if (bFinished || bRunConstructionScriptOnDrag || (Blueprint && Blueprint->bRunConstructionScriptOnDrag))
		{
			FNavigationLockContext NavLock(GetWorld(), ENavigationLockReason::AllowUnregister);
			RerunConstructionScripts();
		}
	}

	if ( bFinished )
	{
		UWorld* World = GetWorld();

		World->UpdateCullDistanceVolumes(this);
		World->bAreConstraintsDirty = true;

		FEditorSupportDelegates::RefreshPropertyWindows.Broadcast();

		// Let other systems know that an actor was moved
		GEngine->BroadcastOnActorMoved( this );

		FEditorSupportDelegates::UpdateUI.Broadcast();
	}

	// If the root component was not just recreated by the construction script - call PostEditComponentMove on it
	if(RootComponent != NULL && !RootComponent->IsCreatedByConstructionScript())
	{
		// @TODO Should we call on ALL components?
		RootComponent->PostEditComponentMove(bFinished);
	}

	if (bFinished)
	{
		// update actor and all its components in navigation system after finishing move
		// USceneComponent::UpdateNavigationData works only in game world
		UNavigationSystem::UpdateNavOctreeBounds(this);

		TArray<AActor*> ParentedActors;
		GetAttachedActors(ParentedActors);
		for (int32 Idx = 0; Idx < ParentedActors.Num(); Idx++)
		{
			UNavigationSystem::UpdateNavOctreeBounds(ParentedActors[Idx]);
		}

		// not doing manual update of all attached actors since UpdateActorAndComponentsInNavOctree should take care of it
		UNavigationSystem::UpdateActorAndComponentsInNavOctree(*this);
	}
}

bool AActor::ReregisterComponentsWhenModified() const
{
	return !IsTemplate() && !GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor) && GetWorld() != nullptr;
}

void AActor::DebugShowComponentHierarchy(  const TCHAR* Info, bool bShowPosition )
{	
	TArray<AActor*> ParentedActors;
	GetAttachedActors( ParentedActors );
	if( Info  )
	{
		UE_LOG( LogActor, Warning, TEXT("--%s--"), Info );
	}
	else
	{
		UE_LOG( LogActor, Warning, TEXT("--------------------------------------------------") );
	}
	UE_LOG( LogActor, Warning, TEXT("--------------------------------------------------") );
	UE_LOG( LogActor, Warning, TEXT("Actor [%x] (%s)"), this, *GetFName().ToString() );
	USceneComponent* SceneComp = GetRootComponent();
	if( SceneComp )
	{
		int32 NestLevel = 0;
		DebugShowOneComponentHierarchy( SceneComp, NestLevel, bShowPosition );			
	}
	else
	{
		UE_LOG( LogActor, Warning, TEXT("Actor has no root.") );		
	}
	UE_LOG( LogActor, Warning, TEXT("--------------------------------------------------") );
}

void AActor::DebugShowOneComponentHierarchy( USceneComponent* SceneComp, int32& NestLevel, bool bShowPosition )
{
	FString Nest = "";
	for (int32 iNest = 0; iNest < NestLevel ; iNest++)
	{
		Nest = Nest + "---->";	
	}
	NestLevel++;
	FString PosString;
	if( bShowPosition )
	{
		FVector Posn = SceneComp->GetComponentTransform().GetLocation();
		//PosString = FString::Printf( TEXT("{R:%f,%f,%f- W:%f,%f,%f}"), SceneComp->RelativeLocation.X, SceneComp->RelativeLocation.Y, SceneComp->RelativeLocation.Z, Posn.X, Posn.Y, Posn.Z );
		PosString = FString::Printf( TEXT("{R:%f- W:%f}"), SceneComp->RelativeLocation.Z, Posn.Z );
	}
	else
	{
		PosString = "";
	}
	AActor* OwnerActor = SceneComp->GetOwner();
	if( OwnerActor )
	{
		UE_LOG(LogActor, Warning, TEXT("%sSceneComp [%x] (%s) Owned by %s %s"), *Nest, SceneComp, *SceneComp->GetFName().ToString(), *OwnerActor->GetFName().ToString(), *PosString );
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("%sSceneComp [%x] (%s) No Owner"), *Nest, SceneComp, *SceneComp->GetFName().ToString() );
	}
	if( SceneComp->GetAttachParent())
	{
		if( bShowPosition )
		{
			FVector Posn = SceneComp->GetComponentTransform().GetLocation();
			//PosString = FString::Printf( TEXT("{R:%f,%f,%f- W:%f,%f,%f}"), SceneComp->RelativeLocation.X, SceneComp->RelativeLocation.Y, SceneComp->RelativeLocation.Z, Posn.X, Posn.Y, Posn.Z );
			PosString = FString::Printf( TEXT("{R:%f- W:%f}"), SceneComp->RelativeLocation.Z, Posn.Z );
		}
		else
		{
			PosString = "";
		}
		UE_LOG(LogActor, Warning, TEXT("%sAttachParent [%x] (%s) %s"), *Nest, SceneComp->GetAttachParent(), *SceneComp->GetAttachParent()->GetFName().ToString(), *PosString );
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("%s[NO PARENT]"), *Nest );
	}

	if( SceneComp->GetAttachChildren().Num() != 0 )
	{
		for (USceneComponent* EachSceneComp : SceneComp->GetAttachChildren())
		{			
			DebugShowOneComponentHierarchy(EachSceneComp,NestLevel, bShowPosition );
		}
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("%s[NO CHILDREN]"), *Nest );
	}
}

AActor::FActorTransactionAnnotation::FActorTransactionAnnotation(const AActor* Actor, const bool bCacheRootComponentData)
	: ComponentInstanceData(Actor)
{
	USceneComponent* ActorRootComponent = Actor->GetRootComponent();
	if (bCacheRootComponentData && ActorRootComponent && ActorRootComponent->IsCreatedByConstructionScript())
	{
		bRootComponentDataCached = true;
		RootComponentData.Transform = ActorRootComponent->GetComponentTransform();
		RootComponentData.Transform.SetTranslation(ActorRootComponent->GetComponentLocation()); // take into account any custom location
		RootComponentData.TransformRotationCache = ActorRootComponent->GetRelativeRotationCache();

		if (ActorRootComponent->GetAttachParent())
		{
			RootComponentData.AttachedParentInfo.Actor = ActorRootComponent->GetAttachParent()->GetOwner();
			RootComponentData.AttachedParentInfo.AttachParent = ActorRootComponent->GetAttachParent();
			RootComponentData.AttachedParentInfo.AttachParentName = ActorRootComponent->GetAttachParent()->GetFName();
			RootComponentData.AttachedParentInfo.SocketName = ActorRootComponent->GetAttachSocketName();
			RootComponentData.AttachedParentInfo.RelativeTransform = ActorRootComponent->GetRelativeTransform();
		}

		for (USceneComponent* AttachChild : ActorRootComponent->GetAttachChildren())
		{
			AActor* ChildOwner = (AttachChild ? AttachChild->GetOwner() : NULL);
			if (ChildOwner && ChildOwner != Actor)
			{
				// Save info about actor to reattach
				FActorRootComponentReconstructionData::FAttachedActorInfo Info;
				Info.Actor = ChildOwner;
				Info.SocketName = AttachChild->GetAttachSocketName();
				Info.RelativeTransform = AttachChild->GetRelativeTransform();
				RootComponentData.AttachedToInfo.Add(Info);
			}
		}
	}
	else
	{
		bRootComponentDataCached = false;
	}
}

void AActor::FActorTransactionAnnotation::AddReferencedObjects(FReferenceCollector& Collector)
{
	ComponentInstanceData.AddReferencedObjects(Collector);
}

bool AActor::FActorTransactionAnnotation::HasInstanceData() const
{
	return (bRootComponentDataCached || ComponentInstanceData.HasInstanceData());
}

TSharedPtr<ITransactionObjectAnnotation> AActor::GetTransactionAnnotation() const
{
	if (CurrentTransactionAnnotation.IsValid())
	{
		return CurrentTransactionAnnotation;
	}

	TSharedPtr<FActorTransactionAnnotation> TransactionAnnotation = MakeShareable(new FActorTransactionAnnotation(this));

	if (!TransactionAnnotation->HasInstanceData())
	{
		// If there is nothing in the annotation don't bother storing it.
		TransactionAnnotation = nullptr;
	}

	return TransactionAnnotation;
}

void AActor::PreEditUndo()
{
	// Check if this Actor needs to be re-instanced
	UClass* OldClass = GetClass();
	UClass* NewClass = OldClass->GetAuthoritativeClass();
	if (NewClass != OldClass)
	{
		// Empty the OwnedComponents array, it's filled with invalid information
		OwnedComponents.Empty();
	}

	// Since child actor components will rebuild themselves get rid of the Actor before we make changes
	TInlineComponentArray<UChildActorComponent*> ChildActorComponents;
	GetComponents(ChildActorComponents);

	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (ChildActorComponent->IsCreatedByConstructionScript())
		{
			ChildActorComponent->DestroyChildActor();
		}
	}

	// let navigation system know to not care about this actor anymore
	UNavigationSystem::ClearNavOctreeAll(this);

	Super::PreEditUndo();
}

bool AActor::InternalPostEditUndo()
{
	// Check if this Actor needs to be re-instanced
	UClass* OldClass = GetClass();
	if (OldClass->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		UClass* NewClass = OldClass->GetAuthoritativeClass();
		if (!ensure(NewClass != OldClass))
		{
			UE_LOG(LogActor, Warning, TEXT("WARNING: %s is out of date and is the same as its AuthoritativeClass during PostEditUndo!"), *OldClass->GetName());
		};

		// Early exit, letting anything more occur would be invalid due to the REINST_ class
		return false;
	}

	// Notify LevelBounds actor that level bounding box might be changed
	if (!IsTemplate())
	{
		if (ULevel* Level = GetLevel())
		{
			Level->MarkLevelBoundsDirty();
		}
	}

	// Restore OwnedComponents array
	if (!IsPendingKill())
	{
		ResetOwnedComponents();

		// BP created components are not serialized, so this should be cleared and will be filled in as the construction scripts are run
		BlueprintCreatedComponents.Reset();

		// notify navigation system
		UNavigationSystem::UpdateActorAndComponentsInNavOctree(*this);
	}
	else
	{
		UNavigationSystem::ClearNavOctreeAll(this);
	}

	// This is a normal undo, so call super
	return true;
}

void AActor::PostEditUndo()
{
	if (InternalPostEditUndo())
	{
		Super::PostEditUndo();
	}
}

void AActor::PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation)
{
	CurrentTransactionAnnotation = StaticCastSharedPtr<FActorTransactionAnnotation>(TransactionAnnotation);

	if (InternalPostEditUndo())
	{
		Super::PostEditUndo(TransactionAnnotation);
	}
}

// @todo: Remove this hack once we have decided on the scaling method to use.
bool AActor::bUsePercentageBasedScaling = false;

void AActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	if( RootComponent != NULL )
	{
		FTransform NewTransform = GetRootComponent()->GetComponentTransform();
		NewTransform.SetTranslation(NewTransform.GetTranslation() + DeltaTranslation);
		GetRootComponent()->SetWorldTransform(NewTransform);
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyTranslation %s has no root component"), *GetName() );
	}
}

void AActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	if( RootComponent != NULL )
	{
		const FRotator Rot = RootComponent->GetAttachParent() != NULL ? GetActorRotation() : RootComponent->RelativeRotation;

		FRotator ActorRotWind, ActorRotRem;
		Rot.GetWindingAndRemainder(ActorRotWind, ActorRotRem);

		const FQuat ActorQ = ActorRotRem.Quaternion();
		const FQuat DeltaQ = DeltaRotation.Quaternion();
		const FQuat ResultQ = DeltaQ * ActorQ;
		const FRotator NewActorRotRem = FRotator( ResultQ );
		FRotator DeltaRot = NewActorRotRem - ActorRotRem;
		DeltaRot.Normalize();

		if( RootComponent->GetAttachParent() != NULL )
		{
			RootComponent->SetWorldRotation( Rot + DeltaRot );
		}
		else
		{
			// No attachment.  Directly set relative rotation (to support winding)
			RootComponent->SetRelativeRotation( Rot + DeltaRot );
		}
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyRotation %s has no root component"), *GetName() );
	}
}


void AActor::EditorApplyScale( const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown )
{
	if( RootComponent != NULL )
	{
		const FVector CurrentScale = GetRootComponent()->RelativeScale3D;

		// @todo: Remove this hack once we have decided on the scaling method to use.
		FVector ScaleToApply;

		if( AActor::bUsePercentageBasedScaling )
		{
			ScaleToApply = CurrentScale * (FVector(1.0f) + DeltaScale);
		}
		else
		{
			ScaleToApply = CurrentScale + DeltaScale;
		}

		GetRootComponent()->SetRelativeScale3D(ScaleToApply);

		if (PivotLocation)
		{
			const FVector CurrentScaleSafe(CurrentScale.X ? CurrentScale.X : 1.0f,
										   CurrentScale.Y ? CurrentScale.Y : 1.0f,
										   CurrentScale.Z ? CurrentScale.Z : 1.0f);

			const FRotator ActorRotation = GetActorRotation();
			const FVector WorldDelta = GetActorLocation() - (*PivotLocation);
			const FVector LocalDelta = (ActorRotation.GetInverse()).RotateVector(WorldDelta);
			const FVector LocalScaledDelta = LocalDelta * (ScaleToApply / CurrentScaleSafe);
			const FVector WorldScaledDelta = ActorRotation.RotateVector(LocalScaledDelta);

			GetRootComponent()->SetWorldLocation(WorldScaledDelta + (*PivotLocation));
		}
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyTranslation %s has no root component"), *GetName() );
	}

	FEditorSupportDelegates::UpdateUI.Broadcast();
}


void AActor::EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation)
{
	const FRotationMatrix TempRot( GetActorRotation() );
	const FVector New0( TempRot.GetScaledAxis( EAxis::X ) * MirrorScale );
	const FVector New1( TempRot.GetScaledAxis( EAxis::Y ) * MirrorScale );
	const FVector New2( TempRot.GetScaledAxis( EAxis::Z ) * MirrorScale );
	// Revert the handedness of the rotation, but make up for it in the scaling.
	// Arbitrarily choose the X axis to remain fixed.
	const FMatrix NewRot( -New0, New1, New2, FVector::ZeroVector );

	if( RootComponent != NULL )
	{
		GetRootComponent()->SetRelativeRotation( NewRot.Rotator() );
		FVector Loc = GetActorLocation();
		Loc -= PivotLocation;
		Loc *= MirrorScale;
		Loc += PivotLocation;
		GetRootComponent()->SetRelativeLocation( Loc );

		FVector Scale3D = GetRootComponent()->RelativeScale3D;
		Scale3D.X = -Scale3D.X;
		GetRootComponent()->SetRelativeScale3D(Scale3D);
	}
	else
	{
		UE_LOG(LogActor, Warning, TEXT("WARNING: EditorApplyMirror %s has no root component"), *GetName() );
	}
}

bool AActor::IsHiddenEd() const
{
	// If any of the standard hide flags are set, return true
	if( bHiddenEdLayer || !bEditable || ( GIsEditor && ( IsTemporarilyHiddenInEditor() || bHiddenEdLevel ) ) )
	{
		return true;
	}
	// Otherwise, it's visible
	return false;
}

void AActor::SetIsTemporarilyHiddenInEditor( bool bIsHidden )
{
	if( bHiddenEdTemporary != bIsHidden )
	{
		bHiddenEdTemporary = bIsHidden;
		MarkComponentsRenderStateDirty();
	}
}

bool AActor::IsEditable() const
{
	return bEditable;
}

bool AActor::IsListedInSceneOutliner() const
{
	return bListedInSceneOutliner;
}

bool AActor::EditorCanAttachTo(const AActor* InParent, FText& OutReason) const
{
	return true;
}

const FString& AActor::GetActorLabel() const
{
	// If the label string is empty then we'll use the default actor label (usually the actor's class name.)
	// We actually cache the default name into our ActorLabel property.  This will be saved out with the
	// actor if the actor gets saved.  The reasons we like caching the name here is:
	//
	//		a) We can return it by const&	(performance)
	//		b) Calling GetDefaultActorLabel() is slow because of FName stuff  (performance)
	//		c) If needed, we could always empty the ActorLabel string if it matched the default
	//
	// Remember, ActorLabel is currently an editor-only property.

	if( ActorLabel.IsEmpty() )
	{
		// Treating ActorLabel as mutable here (no 'mutable' keyword in current script compiler)
		AActor* MutableThis = const_cast< AActor* >( this );

		// Get the class
		UClass* ActorClass = GetClass();

		// NOTE: Calling GetName() is actually fairly slow (does ANSI->Wide conversion, lots of copies, etc.)
		FString DefaultActorLabel = ActorClass->GetName();

		// Strip off the ugly "_C" suffix for Blueprint class actor instances
		UBlueprint* GeneratedByClassBlueprint = Cast<UBlueprint>( ActorClass->ClassGeneratedBy );
		if( GeneratedByClassBlueprint != nullptr && DefaultActorLabel.EndsWith( TEXT( "_C" ) ) )
		{
			DefaultActorLabel.RemoveFromEnd( TEXT( "_C" ) );
		}

		// We want the actor's label to be initially unique, if possible, so we'll use the number of the
		// actor's FName when creating the initially.  It doesn't actually *need* to be unique, this is just
		// an easy way to tell actors apart when observing them in a list.  The user can always go and rename
		// these labels such that they're no longer unique.
		{
			// Don't bother adding a suffix for number '0'
			const int32 NameNumber = NAME_INTERNAL_TO_EXTERNAL( GetFName().GetNumber() );
			if( NameNumber != 0 )
			{
				DefaultActorLabel.AppendInt(NameNumber);
			}
		}

		// Remember, there could already be an actor with the same label in the level.  But that's OK, because
		// actor labels aren't supposed to be unique.  We just try to make them unique initially to help
		// disambiguate when opening up a new level and there are hundreds of actors of the same type.
		MutableThis->ActorLabel = DefaultActorLabel;
	}

	return ActorLabel;
}

void AActor::SetActorLabel( const FString& NewActorLabelDirty, bool bMarkDirty )
{
	const bool bMakeGloballyUniqueFName = false;
	SetActorLabelInternal(NewActorLabelDirty, bMakeGloballyUniqueFName, bMarkDirty );
}

void AActor::SetActorLabelInternal( const FString& NewActorLabelDirty, bool bMakeGloballyUniqueFName, bool bMarkDirty )
{
	// Clean up the incoming string a bit
	FString NewActorLabel = NewActorLabelDirty;
	NewActorLabel.TrimStartAndEndInline();


	// First, update the actor label
	{
		// Has anything changed?
		if( FCString::Strcmp( *NewActorLabel, *GetActorLabel() ) != 0 )
		{
			// Store new label
			Modify( bMarkDirty );
			ActorLabel = NewActorLabel;
		}
	}


	// Next, update the actor's name
	{
		// Generate an object name for the actor's label
		const FName OldActorName = GetFName();
		FName NewActorName = MakeObjectNameFromDisplayLabel( GetActorLabel(), OldActorName );

		// Has anything changed?
		if( OldActorName != NewActorName )
		{
			// Try to rename the object
			UObject* NewOuter = NULL;		// Outer won't be changing
			ERenameFlags RenFlags = bMakeGloballyUniqueFName ? (REN_DontCreateRedirectors | REN_ForceGlobalUnique) : REN_DontCreateRedirectors;
			bool bCanRename = Rename( *NewActorName.ToString(), NewOuter, REN_Test | REN_DoNotDirty | REN_NonTransactional | RenFlags );
			if( bCanRename )
			{
				// NOTE: Will assert internally if rename fails
				const bool bWasRenamed = Rename( *NewActorName.ToString(), NewOuter, RenFlags );
			}
			else
			{
				// Unable to rename the object.  Use a unique object name variant.
				NewActorName = MakeUniqueObjectName( bMakeGloballyUniqueFName ? ANY_PACKAGE : GetOuter(), GetClass(), NewActorName );

				bCanRename = Rename( *NewActorName.ToString(), NewOuter, REN_Test | REN_DoNotDirty | REN_NonTransactional | RenFlags );
				if( bCanRename )
				{
					// NOTE: Will assert internally if rename fails
					const bool bWasRenamed = Rename( *NewActorName.ToString(), NewOuter, RenFlags );
				}
				else
				{
					// Unable to rename the object.  Oh well, not a big deal.
				}
			}
		}
	}

	FPropertyChangedEvent PropertyEvent( FindField<UProperty>( AActor::StaticClass(), "ActorLabel" ) );
	PostEditChangeProperty(PropertyEvent);

	FCoreDelegates::OnActorLabelChanged.Broadcast(this);
}

bool AActor::IsActorLabelEditable() const
{
	return bActorLabelEditable && !FActorEditorUtils::IsABuilderBrush(this);
}

void AActor::ClearActorLabel()
{
	ActorLabel = TEXT("");
}

const FName& AActor::GetFolderPath() const
{
	return FolderPath;
}

void AActor::SetFolderPath(const FName& NewFolderPath)
{
	if (NewFolderPath != FolderPath)
	{
		Modify();

		FName OldPath = FolderPath;
		FolderPath = NewFolderPath;

		if (GEngine)
		{
			GEngine->BroadcastLevelActorFolderChanged(this, OldPath);
		}
	}
}

void AActor::SetFolderPath_Recursively(const FName& NewFolderPath)
{
	FActorEditorUtils::TraverseActorTree_ParentFirst(this, [&](AActor* InActor){
		InActor->SetFolderPath(NewFolderPath);
		return true;
	});
}

void AActor::CheckForDeprecated()
{
	if ( GetClass()->HasAnyClassFlags(CLASS_Deprecated) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ActorIsObselete_Deprecated", "{ActorName} : Obsolete and must be removed! (Class is deprecated)" ), Arguments) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorIsObselete));
	}

	if ( GetClass()->HasAnyClassFlags(CLASS_Abstract) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ActorIsObselete_Abstract", "{ActorName} : Obsolete and must be removed! (Class is abstract)" ), Arguments) ) )
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorIsObselete));
	}
}

void AActor::CheckForErrors()
{
	if ( GetClass()->HasAnyClassFlags(CLASS_Deprecated) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ActorIsObselete_Deprecated", "{ActorName} : Obsolete and must be removed! (Class is deprecated)" ), Arguments) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorIsObselete));
		return;
	}
	if ( GetClass()->HasAnyClassFlags(CLASS_Abstract) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ActorIsObselete_Abstract", "{ActorName} : Obsolete and must be removed! (Class is abstract)" ), Arguments) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorIsObselete));
		return;
	}

	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(RootComponent);
	if( PrimComp && (PrimComp->Mobility != EComponentMobility::Movable) && PrimComp->BodyInstance.bSimulatePhysics)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_StaticPhysNone", "{ActorName} : Static object with bSimulatePhysics set to true" ), Arguments) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticPhysNone));
	}

	if( RootComponent && FMath::IsNearlyZero( GetRootComponent()->RelativeScale3D.X * GetRootComponent()->RelativeScale3D.Y * GetRootComponent()->RelativeScale3D.Z ) )
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_InvalidDrawscale", "{ActorName} : Invalid DrawScale/DrawScale3D" ), Arguments) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::InvalidDrawscale));
	}

	// Route error checking to components.
	TInlineComponentArray<UActorComponent*> Components;
	GetComponents(Components);

	for ( int32 ComponentIndex = 0 ; ComponentIndex < Components.Num() ; ++ComponentIndex )
	{
		UActorComponent* ActorComponent = Components[ ComponentIndex ];
		if (ActorComponent->IsRegistered())
		{
			ActorComponent->CheckForErrors();
		}
	}
}

bool AActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass( GetClass() );
	if (Blueprint)
	{
		Objects.AddUnique(Blueprint);
	}
	return true;
}

void AActor::SetLODParent(UPrimitiveComponent* InLODParent, float InParentDrawDistance)
{
	if(InLODParent)
	{
		InLODParent->MinDrawDistance = InParentDrawDistance;
		InLODParent->MarkRenderStateDirty();
	}

	TArray<UPrimitiveComponent*> ComponentsToBeReplaced;
	GetComponents(ComponentsToBeReplaced);

	for(UPrimitiveComponent* Component : ComponentsToBeReplaced)
	{
		// parent primitive will be null if no LOD parent is selected
		Component->SetLODParentPrimitive(InLODParent);
	}
}
#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
