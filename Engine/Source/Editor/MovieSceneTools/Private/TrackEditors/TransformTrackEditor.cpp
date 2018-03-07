// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/TransformTrackEditor.h"
#include "GameFramework/Actor.h"
#include "Framework/Commands/Commands.h"
#include "Animation/AnimSequence.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "GameFramework/Character.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "UnrealEdGlobals.h"
#include "ISectionLayoutBuilder.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "FloatCurveKeyArea.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "TransformPropertySection.h"
#include "SequencerUtilities.h"

#define LOCTEXT_NAMESPACE "MovieScene_TransformTrack"

void GetActorAndSceneComponentFromObject( UObject* Object, AActor*& OutActor, USceneComponent*& OutSceneComponent )
{
	OutActor = Cast<AActor>( Object );
	if ( OutActor != nullptr && OutActor->GetRootComponent() )
	{
		OutSceneComponent = OutActor->GetRootComponent();
	}
	else
	{
		// If the object wasn't an actor attempt to get it directly as a scene component and then get the actor from there.
		OutSceneComponent = Cast<USceneComponent>( Object );
		if ( OutSceneComponent != nullptr )
		{
			OutActor = Cast<AActor>( OutSceneComponent->GetOuter() );
		}
	}
}

/**
 * Class that draws a transform section in the sequencer
 */
class F3DTransformSection : public FTransformSection
{
public:

	F3DTransformSection(TWeakObjectPtr<UMovieSceneSection> InSection, TWeakPtr<ISequencer> InSequencer, FGuid InObjectBinding)
		: FTransformSection(InSection, InSequencer, InObjectBinding)
	{
	}

protected:

	virtual TOptional<FTransform> GetCurrentValue() const
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		if (Sequencer.IsValid())
		{
			for (TWeakObjectPtr<> WeakObject : Sequencer->FindBoundObjects(ObjectBinding, Sequencer->GetFocusedTemplateID()))
			{
				if (UObject* Object = WeakObject.Get())
				{
					AActor* Actor = nullptr;
					USceneComponent* Component = nullptr;
					GetActorAndSceneComponentFromObject(Object, Actor, Component);
					if (Component)
					{
						return Component->GetRelativeTransform();
					}
				}
			}
		}
		return TOptional<FTransform>();
	}
};


class F3DTransformTrackCommands
	: public TCommands<F3DTransformTrackCommands>
{
public:

	F3DTransformTrackCommands()
		: TCommands<F3DTransformTrackCommands>
	(
		"3DTransformTrack",
		NSLOCTEXT("Contexts", "3DTransformTrack", "3DTransformTrack"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
		, BindingCount(0)
	{ }
		
	/** Sets a transform key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddTransformKey;

	/** Sets a translation key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddTranslationKey;

	/** Sets a rotation key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddRotationKey;

	/** Sets a scale key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > AddScaleKey;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

	mutable uint32 BindingCount;
};


void F3DTransformTrackCommands::RegisterCommands()
{
	UI_COMMAND( AddTransformKey, "Add Transform Key", "Add a transform key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EKeys::S) );
	UI_COMMAND( AddTranslationKey, "Add Translation Key", "Add a translation key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::W) );
	UI_COMMAND( AddRotationKey, "Add Rotation Key", "Add a rotation key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::E) );
	UI_COMMAND( AddScaleKey, "Add Scale Key", "Add a scale key at the current time for the selected actor.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::R) );
}

FName F3DTransformTrackEditor::TransformPropertyName("Transform");

F3DTransformTrackEditor::F3DTransformTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FKeyframeTrackEditor<UMovieScene3DTransformTrack, UMovieScene3DTransformSection, FTransformKey>( InSequencer ) 
{
	// Listen for actor/component movement
	GEditor->OnBeginObjectMovement().AddRaw( this, &F3DTransformTrackEditor::OnPreTransformChanged );
	GEditor->OnEndObjectMovement().AddRaw( this, &F3DTransformTrackEditor::OnTransformChanged );

	// Listen for the viewport's viewed through camera starts and stops movement
	GEditor->OnBeginCameraMovement().AddRaw( this, &F3DTransformTrackEditor::OnPreTransformChanged );
	GEditor->OnEndCameraMovement().AddRaw( this, &F3DTransformTrackEditor::OnTransformChanged );

	F3DTransformTrackCommands::Register();
}


F3DTransformTrackEditor::~F3DTransformTrackEditor()
{
}


void F3DTransformTrackEditor::OnRelease()
{
	GEditor->OnBeginObjectMovement().RemoveAll( this );
	GEditor->OnEndObjectMovement().RemoveAll( this );

	GEditor->OnBeginCameraMovement().RemoveAll( this );
	GEditor->OnEndCameraMovement().RemoveAll( this );

	const F3DTransformTrackCommands& Commands = F3DTransformTrackCommands::Get();
	Commands.BindingCount--;
	
	if (Commands.BindingCount < 1)
	{
		F3DTransformTrackCommands::Unregister();
	}

	ClearLockedCameras(nullptr);
}


TSharedRef<ISequencerTrackEditor> F3DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F3DTransformTrackEditor( InSequencer ) );
}


bool F3DTransformTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	// We support animatable transforms
	return Type == UMovieScene3DTransformTrack::StaticClass();
}


void CopyInterpMoveTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack)
{
	if (FMatineeImportTools::CopyInterpMoveTrack(MoveTrack, TransformTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}


bool CanCopyInterpMoveTrack(UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack)
{
	if (!MoveTrack || !TransformTrack)
	{
		return false;
	}

	bool bHasKeyframes = MoveTrack->GetNumKeyframes() != 0;

	for (auto SubTrack : MoveTrack->SubTracks)
	{
		if (SubTrack->IsA(UInterpTrackMoveAxis::StaticClass()))
		{
			UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>(SubTrack);
			if (MoveSubTrack)
			{
				if (MoveSubTrack->FloatTrack.Points.Num() > 0)
				{
					bHasKeyframes = true;
					break;
				}
			}
		}
	}
		
	return bHasKeyframes;
}

void F3DTransformTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackMove* MoveTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MoveTrack = Cast<UInterpTrackMove>( CopyPasteObject );
		if ( MoveTrack != nullptr )
		{
			break;
		}
	}
	UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeMoveTrack", "Paste Matinee Move Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeMoveTrackTooltip", "Pastes keys from a Matinee move track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &CopyInterpMoveTrack, GetSequencer().ToSharedRef(), MoveTrack, TransformTrack ),
			FCanExecuteAction::CreateStatic( &CanCopyInterpMoveTrack, MoveTrack, TransformTrack ) ) );

	//		FCanExecuteAction::CreateLambda( [=]()->bool { return MoveTrack != nullptr && MoveTrack->GetNumKeys() > 0 && TransformTrack != nullptr; } ) ) );

	auto AnimSubMenuDelegate = [](FMenuBuilder& InMenuBuilder, TSharedRef<ISequencer> InSequencer, UMovieScene3DTransformTrack* InTransformTrack)
	{
		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.Filter.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateStatic(&F3DTransformTrackEditor::ImportAnimSequenceTransforms, InSequencer, InTransformTrack);

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		InMenuBuilder.AddWidget(
			SNew(SBox)
			.WidthOverride(200.0f)
			.HeightOverride(400.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			], 
			FText(), true, false);
	};

	const bool bInOpenSubMenuOnClick = true;

	MenuBuilder.AddSubMenu(
		NSLOCTEXT( "Sequencer", "ImportTransforms", "Import From Animation Root" ),
		NSLOCTEXT( "Sequencer", "ImportTransformsTooltip", "Import transform keys from an animation sequence's root motion." ),
		FNewMenuDelegate::CreateLambda(AnimSubMenuDelegate, GetSequencer().ToSharedRef(), TransformTrack), 
		bInOpenSubMenuOnClick);

	MenuBuilder.AddMenuSeparator();
	FKeyframeTrackEditor::BuildTrackContextMenu(MenuBuilder, Track);
}


TSharedRef<ISequencerSection> F3DTransformTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShareable(new F3DTransformSection(&SectionObject, GetSequencer(), ObjectBinding));
}

bool F3DTransformTrackEditor::HasTransformTrack(UObject& InObject) const
{
	FGuid Binding = GetSequencer()->FindObjectId(InObject, GetSequencer()->GetFocusedTemplateID());
	if (Binding.IsValid())
	{
		if (GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(Binding, TransformPropertyName))
		{
			return true;
		}
	}

	return false;
}


void F3DTransformTrackEditor::OnPreTransformChanged( UObject& InObject )
{
	if (!GetSequencer()->IsAllowedToChange())
	{
		return;
	}

	float AutoKeyTime = GetTimeForKey();
	AActor* Actor = Cast<AActor>(&InObject);
	// If Sequencer is allowed to autokey and we are clicking on an Actor that can be autokeyed
	if(Actor && !Actor->IsEditorOnly())
	{
		AActor* ActorThatChanged = nullptr;
		USceneComponent* SceneComponentThatChanged = nullptr;
		GetActorAndSceneComponentFromObject(&InObject, ActorThatChanged, SceneComponentThatChanged);

		if( SceneComponentThatChanged )
		{
			// Cache off the existing transform so we can detect which components have changed
			// and keys only when something has changed
			FTransformData Transform( SceneComponentThatChanged );
			
			ObjectToExistingTransform.Add(&InObject, Transform);

			const bool bUnwindRotation = GetSequencer()->IsRecordingLive();
			
			bool bObjectHasTransformTrack = HasTransformTrack(InObject);
			bool bComponentHasTransformTrack = HasTransformTrack(*SceneComponentThatChanged);

			// If there's no existing track, key the existing transform on pre-change so that the current transform before interaction is stored as the default state. 
			// If keying only happens at the end of interaction, the transform after interaction would end up incorrectly as the default state.
			if (!bObjectHasTransformTrack && !bComponentHasTransformTrack)
			{
				FTransformData DummyTransform;
				AddTransformKeys(bComponentHasTransformTrack ? SceneComponentThatChanged : &InObject, DummyTransform, Transform, EKey3DTransformChannel::All, bUnwindRotation, ESequencerKeyMode::AutoKey);
			}
		}
	}
}


/**
 * Temp struct used because delegates only accept 4 or less payloads
 * FTransformKey is immutable and would require heavy re-architecting to fit here
 */
struct FTransformDataPair
{
	FTransformDataPair(FTransformData InTransformData, FTransformData InLastTransformData)
		: TransformData(InTransformData)
		, LastTransformData(InLastTransformData) {}

	FTransformData TransformData;
	FTransformData LastTransformData;
};


void F3DTransformTrackEditor::OnTransformChanged( UObject& InObject )
{
	if (!GetSequencer()->IsAllowedToChange())
	{
		return;
	}

	AActor* Actor = nullptr;
	USceneComponent* SceneComponentThatChanged = nullptr;
	GetActorAndSceneComponentFromObject(&InObject, Actor, SceneComponentThatChanged);

	// If the Actor that just finished transforming doesn't have autokey disabled
	if( SceneComponentThatChanged != nullptr && (Actor && !Actor->IsEditorOnly()))
	{
		// Find an existing transform if possible.  If one exists we will compare against the new one to decide what components of the transform need keys
		FTransformData ExistingTransform = ObjectToExistingTransform.FindRef( &InObject );

		// Remove it from the list of cached transforms. 
		// @todo sequencer livecapture: This can be made much for efficient by not removing cached state during live capture situation
		ObjectToExistingTransform.Remove( &InObject );

		// Build new transform data
		FTransformData NewTransformData( SceneComponentThatChanged );

		const bool bUnwindRotation = GetSequencer()->IsRecordingLive();

		bool bComponentHasTransformTrack = HasTransformTrack(*SceneComponentThatChanged);

		AddTransformKeys(bComponentHasTransformTrack ? SceneComponentThatChanged : &InObject, ExistingTransform, NewTransformData, EKey3DTransformChannel::All, bUnwindRotation, ESequencerKeyMode::AutoKey);
	}
}


void F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects( EKey3DTransformChannel::Type Channel )
{
	// WASD hotkeys to fly the viewport can conflict with hotkeys for setting keyframes (ie. s). 
	// If the viewport is moving, disregard setting keyframes.
	for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
	{		
		FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
		if (LevelVC && LevelVC->IsMovingCamera())
		{
			return;
		}
	}

	TArray<UObject*> SelectedObjects;
	for (FSelectedEditableComponentIterator It(GEditor->GetSelectedEditableComponentIterator()); It; ++It)
	{
		USceneComponent* SceneComponent = Cast<USceneComponent>(*It);
		if (SceneComponent)
		{
			SelectedObjects.Add(SceneComponent);
		}
	}
	
	if (SelectedObjects.Num() == 0)
	{
		USelection* CurrentSelection = GEditor->GetSelectedActors();
		CurrentSelection->GetSelectedObjects( AActor::StaticClass(), SelectedObjects );
	}

	for (TArray<UObject*>::TIterator It(SelectedObjects); It; ++It)
	{
		AddTransformKeysForObject(*It, Channel, ESequencerKeyMode::ManualKeyForced);
	}
}


void F3DTransformTrackEditor::BindCommands(TSharedRef<FUICommandList> SequencerCommandBindings)
{
	const F3DTransformTrackCommands& Commands = F3DTransformTrackCommands::Get();

	SequencerCommandBindings->MapAction(
		Commands.AddTransformKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::All ) );

	SequencerCommandBindings->MapAction(
		Commands.AddTranslationKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::Translation ) );

	SequencerCommandBindings->MapAction(
		Commands.AddRotationKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::Rotation ) );

	SequencerCommandBindings->MapAction(
		Commands.AddScaleKey,
		FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::OnAddTransformKeysForSelectedObjects, EKey3DTransformChannel::Scale ) );

	Commands.BindingCount++;
}


void F3DTransformTrackEditor::BuildObjectBindingEditButtons(TSharedPtr<SHorizontalBox> EditBox, const FGuid& ObjectGuid, const UClass* ObjectClass)
{
	bool bHasCameraComponent = false;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		if (Actor != nullptr)
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(Actor);
			if (CameraComponent)
			{
				bHasCameraComponent = true;
			}
		}
	}

	if (bHasCameraComponent)
	{
		// If this is a camera track, add a button to lock the viewport to the camera
		EditBox.Get()->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			.Padding(4, 0, 0, 0)
			[
				SNew(SCheckBox)		
					.IsFocusable(false)
					.Visibility(this, &F3DTransformTrackEditor::IsCameraVisible, ObjectGuid)
					.IsChecked(this, &F3DTransformTrackEditor::IsCameraLocked, ObjectGuid)
					.OnCheckStateChanged(this, &F3DTransformTrackEditor::OnLockCameraClicked, ObjectGuid)
					.ToolTipText(this, &F3DTransformTrackEditor::GetLockCameraToolTip, ObjectGuid)
					.ForegroundColor(FLinearColor::White)
					.CheckedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
					.CheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
					.CheckedPressedImage(FEditorStyle::GetBrush("Sequencer.LockCamera"))
					.UncheckedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
					.UncheckedHoveredImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
					.UncheckedPressedImage(FEditorStyle::GetBrush("Sequencer.UnlockCamera"))
			];
	}
}


void F3DTransformTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AActor::StaticClass()) || ObjectClass->IsChildOf(USceneComponent::StaticClass()))
	{
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("Sequencer", "AddTransform", "Transform"),
			NSLOCTEXT("Sequencer", "AddPTransformTooltip", "Adds a transform track."),
			FSlateIcon(),
			FUIAction( 
				FExecuteAction::CreateSP( this, &F3DTransformTrackEditor::AddTransformKeysForHandle, ObjectBinding, EKey3DTransformChannel::All, ESequencerKeyMode::ManualKey )
			)
		);
	}
}


bool F3DTransformTrackEditor::CanAddTransformTrackForActorHandle( FGuid ObjectBinding ) const
{
	if (GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene()->FindTrack<UMovieScene3DTransformTrack>(ObjectBinding, TransformPropertyName))
	{
		return false;
	}
	return true;
}

EVisibility F3DTransformTrackEditor::IsCameraVisible(FGuid ObjectGuid) const
{
	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		
		if (Actor != nullptr)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}

ECheckBoxState F3DTransformTrackEditor::IsCameraLocked(FGuid ObjectGuid) const
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		
		if (Actor != nullptr)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (CameraActor.IsValid())
	{
		// First, check the active viewport
		FViewport* ActiveViewport = GEditor->GetActiveViewport();

		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];

			if (LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown)
			{
				if (LevelVC->Viewport == ActiveViewport)
				{
					if (CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
					{
						return ECheckBoxState::Checked;
					}
					else
					{
						return ECheckBoxState::Unchecked;
					}
				}
			}
		}

		// Otherwise check all other viewports
		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];

			if (LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown && CameraActor.IsValid() && LevelVC->IsActorLocked(CameraActor.Get()))
			{
				return ECheckBoxState::Checked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}


void F3DTransformTrackEditor::OnLockCameraClicked(ECheckBoxState CheckBoxState, FGuid ObjectGuid)
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());

		if (Actor != nullptr)
		{
			CameraActor = Actor;
			break;
		}
	}

	// If toggle is on, lock the active viewport to the camera
	if (CheckBoxState == ECheckBoxState::Checked)
	{
		// Set the active viewport or any viewport if there is no active viewport
		FViewport* ActiveViewport = GEditor->GetActiveViewport();

		FLevelEditorViewportClient* LevelVC = nullptr;

		for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
		{		
			FLevelEditorViewportClient* Viewport = GEditor->LevelViewportClients[i];
			if (Viewport && Viewport->IsPerspective() && Viewport->GetViewMode() != VMI_Unknown)
			{
				LevelVC = Viewport;

				if (LevelVC->Viewport == ActiveViewport)
				{
					break;
				}
			}
		}

		if (LevelVC != nullptr && CameraActor.IsValid())
		{
			GetSequencer()->SetPerspectiveViewportCameraCutEnabled(false);
			LevelVC->SetMatineeActorLock(nullptr);
			LevelVC->SetActorLock(CameraActor.Get());
			LevelVC->bLockedCameraView = true;
			LevelVC->UpdateViewForLockedActor();
			LevelVC->Invalidate();
		}
	}
	// Otherwise, clear all locks on the camera
	else
	{
		ClearLockedCameras(CameraActor.Get());
	}
}

void F3DTransformTrackEditor::ClearLockedCameras(AActor* LockedActor)
{
	for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i)
	{		
		FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
		if (LevelVC && LevelVC->IsPerspective() && LevelVC->GetViewMode() != VMI_Unknown)
		{
			if (LockedActor == nullptr || LevelVC->IsActorLocked(LockedActor))
			{
				LevelVC->SetMatineeActorLock(nullptr);
				LevelVC->SetActorLock(nullptr);
				LevelVC->bLockedCameraView = false;
				LevelVC->ViewFOV = LevelVC->FOVAngle;
				LevelVC->RemoveCameraRoll();
				LevelVC->UpdateViewForLockedActor();
				LevelVC->Invalidate();
			}
		}
	}
}


FText F3DTransformTrackEditor::GetLockCameraToolTip(FGuid ObjectGuid) const
{
	TWeakObjectPtr<AActor> CameraActor;

	for (auto Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectGuid))
	{
		AActor* Actor = Cast<AActor>(Object.Get());

		if (Actor != nullptr)
		{
			CameraActor = Actor;
			break;
		}
	}

	if (CameraActor.IsValid())
	{
		return IsCameraLocked(ObjectGuid) == ECheckBoxState::Checked ?
			FText::Format(LOCTEXT("UnlockCamera", "Unlock {0} from Viewport"), FText::FromString(CameraActor.Get()->GetActorLabel())) :
			FText::Format(LOCTEXT("LockCamera", "Lock {0} to Selected Viewport"), FText::FromString(CameraActor.Get()->GetActorLabel()));
	}
	return FText();
}

void GetKeysForVector( bool LastVectorIsValid, const FVector& LastVector, const FVector& CurrentVector, EKey3DTransformChannel::Type VectorChannel, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, TArray<FTransformKey>& OutNewKeys, TArray<FTransformKey>& OutDefaultKeys)
{
	bool bKeyChannel = ChannelsToKey == EKey3DTransformChannel::All || ChannelsToKey == VectorChannel;

	TArray<FTransformKey>& XKeys = bKeyChannel && ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.X, CurrentVector.X ) == false ) ? OutNewKeys : OutDefaultKeys;
	XKeys.Add( FTransformKey( VectorChannel, EAxis::X, CurrentVector.X, bUnwindRotation ) );
	
	TArray<FTransformKey>& YKeys = bKeyChannel && ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.Y, CurrentVector.Y ) == false ) ? OutNewKeys : OutDefaultKeys;
	YKeys.Add( FTransformKey( VectorChannel, EAxis::Y, CurrentVector.Y, bUnwindRotation ) );

	TArray<FTransformKey>& ZKeys = bKeyChannel && ( LastVectorIsValid == false || FMath::IsNearlyEqual( LastVector.Z, CurrentVector.Z ) == false ) ? OutNewKeys : OutDefaultKeys;
	ZKeys.Add( FTransformKey( VectorChannel, EAxis::Z, CurrentVector.Z, bUnwindRotation ) );
}

void F3DTransformTrackEditor::GetTransformKeys( const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, TArray<FTransformKey>& OutNewKeys, TArray<FTransformKey>& OutDefaultKeys )
{
	bool bLastVectorIsValid = LastTransform.IsValid();

	// If key all is enabled, for a key on all the channels
	if (GetSequencer()->GetKeyAllEnabled())
	{
		bLastVectorIsValid = false;
		ChannelsToKey = EKey3DTransformChannel::All;
	}

	GetKeysForVector( bLastVectorIsValid, LastTransform.Translation, CurrentTransform.Translation, EKey3DTransformChannel::Translation, ChannelsToKey, bUnwindRotation, OutNewKeys, OutDefaultKeys );
	GetKeysForVector( bLastVectorIsValid, LastTransform.Rotation.Euler(), CurrentTransform.Rotation.Euler(), EKey3DTransformChannel::Rotation, ChannelsToKey, bUnwindRotation, OutNewKeys, OutDefaultKeys );
	GetKeysForVector( bLastVectorIsValid, LastTransform.Scale, CurrentTransform.Scale, EKey3DTransformChannel::Scale, ChannelsToKey, bUnwindRotation, OutNewKeys, OutDefaultKeys );
}

void F3DTransformTrackEditor::AddTransformKeysForHandle( FGuid ObjectHandle, EKey3DTransformChannel::Type ChannelToKey, ESequencerKeyMode KeyMode )
{
	for ( TWeakObjectPtr<UObject> Object : GetSequencer()->FindObjectsInCurrentSequence(ObjectHandle) )
	{
		AddTransformKeysForObject(Object.Get(), ChannelToKey, KeyMode);
	}
}


void F3DTransformTrackEditor::AddTransformKeysForObject( UObject* Object, EKey3DTransformChannel::Type ChannelToKey, ESequencerKeyMode KeyMode )
{
	AActor* Actor = nullptr;
	USceneComponent* SceneComponent = nullptr;
	GetActorAndSceneComponentFromObject( Object, Actor, SceneComponent );
	if ( Actor != nullptr && SceneComponent != nullptr )
	{
		FTransformData CurrentTransform( SceneComponent );
		if(Object->GetClass()->IsChildOf(AActor::StaticClass()))
		{
			AddTransformKeys( Actor, FTransformData(), CurrentTransform, ChannelToKey, false, KeyMode );
		}
		else if(Object->GetClass()->IsChildOf(USceneComponent::StaticClass()))
		{
			AddTransformKeys( SceneComponent, FTransformData(), CurrentTransform, ChannelToKey, false, KeyMode );
		}
	}
}


void F3DTransformTrackEditor::AddTransformKeys( UObject* ObjectToKey, const FTransformData& LastTransform, const FTransformData& CurrentTransform, EKey3DTransformChannel::Type ChannelsToKey, bool bUnwindRotation, ESequencerKeyMode KeyMode )
{
	if (!GetSequencer()->IsAllowedToChange())
	{
		return;
	}

	TArray<FTransformKey> NewKeys;
	TArray<FTransformKey> DefaultKeys;
	GetTransformKeys(LastTransform, CurrentTransform, ChannelsToKey, bUnwindRotation, NewKeys, DefaultKeys);

	AnimatablePropertyChanged( FOnKeyProperty::CreateRaw(this, &F3DTransformTrackEditor::OnAddTransformKeys, ObjectToKey, &NewKeys, &DefaultKeys, CurrentTransform, KeyMode ) );
}


FKeyPropertyResult F3DTransformTrackEditor::OnAddTransformKeys( float Time, UObject* ObjectToKey, TArray<FTransformKey>* NewKeys, TArray<FTransformKey>* DefaultKeys, FTransformData CurrentTransform, ESequencerKeyMode KeyMode )
{
	TArray<UObject*> ObjectsToKey;
	ObjectsToKey.Add(ObjectToKey);
	
	return AddKeysToObjects(
		ObjectsToKey,
		Time,
		*NewKeys,
		*DefaultKeys,
		KeyMode,
		UMovieScene3DTransformTrack::StaticClass(),
		TransformPropertyName,
		[this](UMovieScene3DTransformTrack* NewTrack) {
			NewTrack->SetPropertyNameAndPath(TransformPropertyName, TransformPropertyName.ToString());
		}
	);
}


void F3DTransformTrackEditor::ImportAnimSequenceTransforms(const FAssetData& Asset, TSharedRef<ISequencer> Sequencer, UMovieScene3DTransformTrack* TransformTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(Asset.GetAsset());

	// find object binding to recover any component transforms we need to incorporate (for characters)
	FTransform InvComponentTransform;
	UMovieSceneSequence* MovieSceneSequence = Sequencer->GetFocusedMovieSceneSequence();
	if(MovieSceneSequence)
	{
		UMovieScene* MovieScene = MovieSceneSequence->GetMovieScene();
		if(MovieScene)
		{
			FGuid ObjectBinding;
			if(MovieScene->FindTrackBinding(*TransformTrack, ObjectBinding))
			{
				const UClass* ObjectClass = nullptr;
				if(FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding))
				{
					ObjectClass = Spawnable->GetObjectTemplate()->GetClass();
				}
				else if(FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding))
				{
					ObjectClass = Possessable->GetPossessedObjectClass();
				}

				if(ObjectClass)
				{
					const ACharacter* Character = Cast<const ACharacter>(ObjectClass->ClassDefaultObject);
					if(Character)
					{
						const USkeletalMeshComponent* SkeletalMeshComponent = Character->GetMesh();
						FTransform MeshRelativeTransform = SkeletalMeshComponent->GetRelativeTransform();
						InvComponentTransform = MeshRelativeTransform.GetRelativeTransform(SkeletalMeshComponent->GetOwner()->GetTransform()).Inverse();
					}
				}
			}
		}
	}

	if(AnimSequence && AnimSequence->GetRawAnimationData().Num() > 0)
	{
		const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "ImportAnimSequenceTransforms", "Import Anim Sequence Transforms" ) );

		TransformTrack->Modify();

		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
		Section->GetScaleCurve(EAxis::X).SetDefaultValue(1);
		Section->GetScaleCurve(EAxis::Y).SetDefaultValue(1);
		Section->GetScaleCurve(EAxis::Z).SetDefaultValue(1);
		TransformTrack->AddSection(*Section);

		if (Section->TryModify())
		{
			float SectionMin = Section->GetStartTime();
			float SectionMax = Section->GetEndTime();

			struct FTempTransformKey
			{
				FTransform Transform;
				FRotator WoundRotation;
				float Time;
			};

			TArray<FTempTransformKey> TempKeys;

			FRawAnimSequenceTrack& RawTrack = AnimSequence->GetRawAnimationTrack(0);
			const int32 KeyCount = FMath::Max(FMath::Max(RawTrack.PosKeys.Num(), RawTrack.RotKeys.Num()), RawTrack.ScaleKeys.Num());
			for(int32 KeyIndex = 0; KeyIndex < KeyCount; KeyIndex++)
			{
				FTempTransformKey TempKey;
				TempKey.Time = AnimSequence->GetTimeAtFrame(KeyIndex);

				if(RawTrack.PosKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetTranslation(RawTrack.PosKeys[KeyIndex]);
				}
				else if(RawTrack.PosKeys.Num() > 0)
				{
					TempKey.Transform.SetTranslation(RawTrack.PosKeys[0]);
				}
				
				if(RawTrack.RotKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetRotation(RawTrack.RotKeys[KeyIndex]);
				}
				else if(RawTrack.RotKeys.Num() > 0)
				{
					TempKey.Transform.SetRotation(RawTrack.RotKeys[0]);
				}

				if(RawTrack.ScaleKeys.IsValidIndex(KeyIndex))
				{
					TempKey.Transform.SetScale3D(RawTrack.ScaleKeys[KeyIndex]);
				}
				else if(RawTrack.ScaleKeys.Num() > 0)
				{
					TempKey.Transform.SetScale3D(RawTrack.ScaleKeys[0]);
				}

				// apply component transform if any
				TempKey.Transform = InvComponentTransform * TempKey.Transform;

				TempKey.WoundRotation = TempKey.Transform.GetRotation().Rotator();

				TempKeys.Add(TempKey);
			}

			int32 TransformCount = TempKeys.Num();
			for(int32 TransformIndex = 0; TransformIndex < TransformCount - 1; TransformIndex++)
			{
				FRotator& Rotator = TempKeys[TransformIndex].WoundRotation;
				FRotator& NextRotator = TempKeys[TransformIndex + 1].WoundRotation;

				FMath::WindRelativeAnglesDegrees(Rotator.Pitch, NextRotator.Pitch);
				FMath::WindRelativeAnglesDegrees(Rotator.Yaw, NextRotator.Yaw);
				FMath::WindRelativeAnglesDegrees(Rotator.Roll, NextRotator.Roll);
			}

			const bool bUnwindRotation = false;
			for(const FTempTransformKey& TempKey : TempKeys)
			{
				SectionMin = FMath::Min( SectionMin, TempKey.Time );
				SectionMax = FMath::Max( SectionMax, TempKey.Time );

				Section->SetStartTime( SectionMin );
				Section->SetEndTime( SectionMax );

				const FVector Translation = TempKey.Transform.GetTranslation();
				const FVector Rotation = TempKey.WoundRotation.Euler();
				const FVector Scale = TempKey.Transform.GetScale3D();

				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Translation, EAxis::X, Translation.X, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Translation, EAxis::Y, Translation.Y, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Translation, EAxis::Z, Translation.Z, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);

				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Rotation, EAxis::X, Rotation.X, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Y, Rotation.Y, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Z, Rotation.Z, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);

				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Scale, EAxis::X, Scale.X, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Scale, EAxis::Y, Scale.Y, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
				Section->AddKey(TempKey.Time, FTransformKey(EKey3DTransformChannel::Scale, EAxis::Z, Scale.Z, bUnwindRotation), EMovieSceneKeyInterpolation::Linear);
			}

			Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
		}
	}
}

#undef LOCTEXT_NAMESPACE
