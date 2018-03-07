// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeToLevelSequenceModule.h"
#include "MatineeToLevelSequenceLog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "LevelSequence.h"
#include "Factories/Factory.h"
#include "AssetData.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Character.h"
#include "Camera/CameraActor.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/LightComponentBase.h"
#include "Engine/Light.h"
#include "Engine/Selection.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackColorScale.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackFade.h"
#include "Matinee/InterpTrackSlomo.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackAudioMaster.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "Matinee/InterpTrackVisibility.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpGroupDirector.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/MatineeActorCameraAnim.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "MatineeUtils.h"
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "LevelEditor.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "LevelSequenceActor.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Matinee/InterpTrackToggle.h"
#include "MatineeImportTools.h"
#include "MovieSceneFolder.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneParticleTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneSlomoTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Animation/SkeletalMeshActor.h"
#include "Dialogs/Dialogs.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "MatineeToLevelSequence"

DEFINE_LOG_CATEGORY(LogMatineeToLevelSequence);

/**
 * Implements the MatineeToLevelSequence module.
 */
class FMatineeToLevelSequenceModule
	: public IMatineeToLevelSequenceModule
{
public:
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		if (GEditor)
		{
			GEditor->OnShouldOpenMatinee().BindRaw(this, &FMatineeToLevelSequenceModule::ShouldOpenMatinee);
		}
		
		RegisterMenuExtensions();
	}
	
	virtual void ShutdownModule() override
	{
		UnregisterMenuExtensions();
	}

 	FDelegateHandle RegisterTrackConverterForMatineeClass(TSubclassOf<UInterpTrack> InterpTrackClass, FOnConvertMatineeTrack OnConvertMatineeTrack)
	{
		if (ExtendedInterpConverters.Contains(InterpTrackClass))
		{
			UE_LOG(LogMatineeToLevelSequence, Warning, TEXT("Track converter already registered for: %s"), InterpTrackClass->GetClass());
			return FDelegateHandle();
		}

		return ExtendedInterpConverters.Add(InterpTrackClass, OnConvertMatineeTrack).GetHandle();
	}
 	
	void UnregisterTrackConverterForMatineeClass(FDelegateHandle RemoveDelegate)
	{
		for (auto InterpConverter : ExtendedInterpConverters)
		{
			if (InterpConverter.Value.GetHandle() == RemoveDelegate)
			{
				ExtendedInterpConverters.Remove(*InterpConverter.Key);
				return;
			}
		}

		UE_LOG(LogMatineeToLevelSequence, Warning, TEXT("Attempted to remove track convert that could not be found"));
	}

protected:

	/** Register menu extensions for the level editor toolbar. */
	void RegisterMenuExtensions()
	{
		// Register level editor menu extender
		LevelEditorMenuExtenderDelegate = FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FMatineeToLevelSequenceModule::ExtendLevelViewportContextMenu);
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
		MenuExtenders.Add(LevelEditorMenuExtenderDelegate);
		LevelEditorExtenderDelegateHandle = MenuExtenders.Last().GetHandle();
	}

protected:

	/** Unregisters menu extensions for the level editor toolbar. */
	void UnregisterMenuExtensions()
	{
		// Unregister level editor menu extender
		if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll([&](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) {
				return Delegate.GetHandle() == LevelEditorExtenderDelegateHandle;
			});
		}
	}

	TSharedRef<FExtender> ExtendLevelViewportContextMenu(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> SelectedActors)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		TArray<TWeakObjectPtr<AActor> > ActorsToConvert;
		for (AActor* Actor : SelectedActors)
		{
			if (Actor->IsA(AMatineeActor::StaticClass()))
			{
				ActorsToConvert.Add(Actor);
			}
		}

		if (ActorsToConvert.Num())
		{
			// Add the convert to level sequence asset sub-menu extender
			Extender->AddMenuExtension(
				"ActorSelectVisibilityLevels",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateRaw(this, &FMatineeToLevelSequenceModule::CreateLevelViewportContextMenuEntries, ActorsToConvert));
		}

		return Extender;
	}

	void CreateLevelViewportContextMenuEntries(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<AActor> > ActorsToConvert)
	{
		MenuBuilder.BeginSection("LevelSequence", LOCTEXT("LevelSequenceLevelEditorHeading", "Level Sequence"));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("MenuExtensionConvertMatineeToLevelSequence", "Convert to Level Sequence"),
			LOCTEXT("MenuExtensionConvertMatineeToLevelSequence_Tooltip", "Convert to Level Sequence"),
			FSlateIcon(),
			FExecuteAction::CreateRaw(this, &FMatineeToLevelSequenceModule::OnConvertMatineeToLevelSequence, ActorsToConvert),
			NAME_None,
			EUserInterfaceActionType::Button);

		MenuBuilder.EndSection();
	}

	/** Callback when opening a matinee. Prompts the user whether to convert this matinee to a level sequence actor */
	bool ShouldOpenMatinee(AMatineeActor* MatineeActor)
	{
		//@todo Camera anims aren't supported as level sequence assets yet
		if (MatineeActor->IsA(AMatineeActorCameraAnim::StaticClass()))
		{
			return true;
		}

		// Pop open a dialog asking whether the user to convert and launcher sequencer or no
		FSuppressableWarningDialog::FSetupInfo Info( 
			LOCTEXT("MatineeToLevelSequencePrompt", "Matinee is now a legacy tool. Would you like to continue opening Matinee or convert your Matinee to a Level Sequence Asset?"), 
			LOCTEXT("MatineeToLevelSequenceTitle", "Convert Matinee to Level Sequence Asset"), 
			TEXT("MatineeToLevelSequence") );
		Info.ConfirmText = LOCTEXT("MatineeToLevelSequence_ConfirmText", "Open Matinee");
		Info.CancelText = LOCTEXT("MatineeToLevelSequence_CancelText", "Convert");
		Info.CheckBoxText = LOCTEXT("MatineeToLevelSequence_CheckBoxText", "Don't Ask Again");

		FSuppressableWarningDialog ShouldOpenMatineeDialog( Info );

		if (ShouldOpenMatineeDialog.ShowModal() == FSuppressableWarningDialog::EResult::Cancel)
		{
			TArray<TWeakObjectPtr<AActor>> ActorsToConvert;
			ActorsToConvert.Add(MatineeActor);
			OnConvertMatineeToLevelSequence(ActorsToConvert);

			// Return false so that the editor doesn't open matinee
			return false;
		}
	
		return true;
	}

	/** Callback for converting a matinee to a level sequence asset. */
	void OnConvertMatineeToLevelSequence(TArray<TWeakObjectPtr<AActor> > ActorsToConvert)
	{
		TArray<TWeakObjectPtr<ALevelSequenceActor> > NewActors;

		int32 NumWarnings = 0;
		for (TWeakObjectPtr<AActor> Actor : ActorsToConvert)
		{
			TWeakObjectPtr<ALevelSequenceActor> NewActor = ConvertSingleMatineeToLevelSequence(Actor, NumWarnings);
			if (NewActor.IsValid())
			{
				NewActors.Add(NewActor);
			}
		}

		// Select the newly created level sequence actors
		const bool bNotifySelectionChanged = true;
		const bool bDeselectBSP = true;
		const bool bWarnAboutTooManyActors = false;
		const bool bSelectEvenIfHidden = false;

		GEditor->GetSelectedActors()->Modify();
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();

		GEditor->SelectNone( bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors );

		for (TWeakObjectPtr<AActor> NewActor : NewActors )
		{
			GEditor->SelectActor(NewActor.Get(), true, bNotifySelectionChanged, bSelectEvenIfHidden );
		}

		GEditor->GetSelectedActors()->EndBatchSelectOperation();
		GEditor->NoteSelectionChange();

		// Edit the first asset
		if (NewActors.Num())
		{
			UObject* NewAsset = NewActors[0]->LevelSequence.TryLoad();
			if (NewAsset)
			{
				FAssetEditorManager::Get().OpenEditorForAsset(NewAsset);
			}

			FText NotificationText;
			if (NewActors.Num() == 1)
			{
				NotificationText = FText::Format(LOCTEXT("MatineeToLevelSequence_Result", "Conversion to {0} complete with {1} warnings"), FText::FromString(NewActors[0]->GetActorLabel()), FText::AsNumber(NumWarnings));
			}
			else
			{
				NotificationText = FText::Format(LOCTEXT("MatineeToLevelSequence_Result", "Converted {0} with {1} warnings"), FText::AsNumber(NewActors.Num()), FText::AsNumber(NumWarnings));
			}

			FNotificationInfo NotificationInfo(NotificationText);
			NotificationInfo.ExpireDuration = 5.f;
			NotificationInfo.Hyperlink = FSimpleDelegate::CreateStatic([](){ FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog")); });
			NotificationInfo.HyperlinkText = LOCTEXT("ShowMessageLogHyperlink", "Show Output Log");
			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		}
	}

	/** Find or add a folder for the given actor **/
	static UMovieSceneFolder* FindOrAddFolder(UMovieScene* MovieScene, FName FolderName)
	{
		// look for a folder to put us in
		UMovieSceneFolder* FolderToUse = nullptr;
		for (UMovieSceneFolder* Folder : MovieScene->GetRootFolders())
		{
			if (Folder->GetFolderName() == FolderName)
			{
				FolderToUse = Folder;
				break;
			}
		}

		if (FolderToUse == nullptr)
		{
			FolderToUse = NewObject<UMovieSceneFolder>(MovieScene, NAME_None, RF_Transactional);
			FolderToUse->SetFolderName(FolderName);
			MovieScene->GetRootFolders().Add(FolderToUse);
		}

		return FolderToUse;
	}

	/** Find or add a folder for the given actor **/
	static void FindOrAddFolder(UMovieScene* MovieScene, TWeakObjectPtr<AActor> Actor, FGuid Guid)
	{
		FName FolderName(NAME_None);
		if(Actor.Get()->IsA<ACharacter>() || Actor.Get()->IsA<ASkeletalMeshActor>())
		{
			FolderName = TEXT("Characters");
		}
		else if(Actor.Get()->GetClass()->IsChildOf(ACameraActor::StaticClass()))
		{
			FolderName = TEXT("Cameras");
		}
		else if(Actor.Get()->GetClass()->IsChildOf(ALight::StaticClass()))
		{
			FolderName = TEXT("Lights");
		}
		else if (Actor.Get()->GetComponentsByClass(UParticleSystemComponent::StaticClass()).Num())
		{
			FolderName = TEXT("Particles");
		}
		else
		{
			FolderName = TEXT("Misc");
		}

		UMovieSceneFolder* FolderToUse = FindOrAddFolder(MovieScene, FolderName);
		FolderToUse->AddChildObjectBinding(Guid);
	}

	/** Add master track to a folder **/
	static void AddMasterTrackToFolder(UMovieScene* MovieScene, UMovieSceneTrack* MovieSceneTrack, FName FolderName)
	{
		UMovieSceneFolder* FolderToUse = FindOrAddFolder(MovieScene, FolderName);
		FolderToUse->AddChildMasterTrack(MovieSceneTrack);
	}

	/** Add property to possessable node **/
	template <typename T>
	static T* AddPropertyTrack(FName InPropertyName, AActor* InActor, const FGuid& PossessableGuid, IMovieScenePlayer& Player, UMovieSceneSequence* NewSequence, UMovieScene* NewMovieScene, int32& NumWarnings)
	{
		T* PropertyTrack = nullptr;

		// Find the property that matinee uses
		void* PropContainer = NULL;
		UProperty* Property = NULL;
		UObject* PropObject = FMatineeUtils::FindObjectAndPropOffset(PropContainer, Property, InActor, InPropertyName );

		FGuid ObjectGuid = PossessableGuid;
		if (PropObject && Property)
		{
			// If the property object that owns this property isn't already bound, add a binding to the property object
			ObjectGuid = Player.FindObjectId(*PropObject, MovieSceneSequenceID::Root);
			if (!ObjectGuid.IsValid())
			{
				UObject* BindingContext = InActor->GetWorld();
				ObjectGuid = NewMovieScene->AddPossessable(PropObject->GetName(), PropObject->GetClass());
				NewSequence->BindPossessableObject(ObjectGuid, *PropObject, BindingContext);
			}

			// cbb: String manipulations to get the property path in the right form for sequencer
			FString PropertyName = Property->GetFName().ToString();

			// Special case for Light components which have some deprecated names
			if (PropObject->GetClass()->IsChildOf(ULightComponentBase::StaticClass()))
			{
				TMap<FName, FName> PropertyNameRemappings;
				PropertyNameRemappings.Add(TEXT("Brightness"), TEXT("Intensity"));
				PropertyNameRemappings.Add(TEXT("Radius"), TEXT("AttenuationRadius"));

				FName* RemappedName = PropertyNameRemappings.Find(*PropertyName);
				if (RemappedName != nullptr)
				{
					PropertyName = RemappedName->ToString();
				}
			}

			TArray<UObject*> PropertyArray;
			UObject* Outer = Property->GetOuter();
			while (Outer->IsA(UProperty::StaticClass()) || Outer->IsA(UScriptStruct::StaticClass()))
			{
				PropertyArray.Insert(Outer, 0);
				Outer = Outer->GetOuter();
			}

			FString PropertyPath;
			for (auto PropertyIt : PropertyArray)
			{
				if (PropertyPath.Len())
				{
					PropertyPath = PropertyPath + TEXT(".");
				}
				PropertyPath = PropertyPath + PropertyIt->GetName();
			}
			if (PropertyPath.Len())
			{
				PropertyPath = PropertyPath + TEXT(".");
			}
			PropertyPath = PropertyPath + PropertyName;

			PropertyTrack = NewMovieScene->AddTrack<T>(ObjectGuid);	
			PropertyTrack->SetPropertyNameAndPath(*PropertyName, PropertyPath);
		}
		else
		{
			UE_LOG(LogMatineeToLevelSequence, Warning, TEXT("Can't find property '%s' for '%s'."), *InPropertyName.ToString(), *InActor->GetActorLabel());
			++NumWarnings;
		}

		return PropertyTrack;
	}

	/** Convert an interp group */
	void ConvertInterpGroup(UInterpGroup* Group, AActor* GroupActor, IMovieScenePlayer& Player, UMovieSceneSequence* NewSequence, UMovieScene* NewMovieScene, int32& NumWarnings)
	{
		FGuid PossessableGuid;

		// Bind the group actor as a possessable						
		if (GroupActor)
		{
			UObject* BindingContext = GroupActor->GetWorld();
			PossessableGuid = NewMovieScene->AddPossessable(GroupActor->GetActorLabel(), GroupActor->GetClass());
			NewSequence->BindPossessableObject(PossessableGuid, *GroupActor, BindingContext);
	
			FindOrAddFolder(NewMovieScene, GroupActor, PossessableGuid);
		}

		for (int32 j=0; j<Group->InterpTracks.Num(); ++j)
		{
			UInterpTrack* Track = Group->InterpTracks[j];
			if (Track->IsDisabled())
			{
				continue;
			}

			// Handle each track class
			if (ExtendedInterpConverters.Find(Track->GetClass()))
			{
				ExtendedInterpConverters.Find(Track->GetClass())->Execute(Track, PossessableGuid, NewMovieScene);
			}
			else if (Track->IsA(UInterpTrackMove::StaticClass()))
			{
				UInterpTrackMove* MatineeMoveTrack = StaticCast<UInterpTrackMove*>(Track);

				bool bHasKeyframes = MatineeMoveTrack->GetNumKeyframes() != 0;

				for (auto SubTrack : MatineeMoveTrack->SubTracks)
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

				if ( bHasKeyframes && PossessableGuid.IsValid())
				{
					FVector DefaultScale = GroupActor != nullptr ? GroupActor->GetActorScale() : FVector(1.f);
					UMovieScene3DTransformTrack* TransformTrack = NewMovieScene->AddTrack<UMovieScene3DTransformTrack>(PossessableGuid);								
					FMatineeImportTools::CopyInterpMoveTrack(MatineeMoveTrack, TransformTrack, DefaultScale);
				}
			}
			else if (Track->IsA(UInterpTrackAnimControl::StaticClass()))
			{
				UInterpTrackAnimControl* MatineeAnimControlTrack = StaticCast<UInterpTrackAnimControl*>(Track);
				if (MatineeAnimControlTrack->GetNumKeyframes() != 0 && PossessableGuid.IsValid())
				{
					UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack = NewMovieScene->AddTrack<UMovieSceneSkeletalAnimationTrack>(PossessableGuid);	
					float EndPlaybackRange = NewMovieScene->GetPlaybackRange().GetUpperBoundValue();
					FMatineeImportTools::CopyInterpAnimControlTrack(MatineeAnimControlTrack, SkeletalAnimationTrack, EndPlaybackRange);
				}
			}
			else if (Track->IsA(UInterpTrackToggle::StaticClass()))
			{
				UInterpTrackToggle* MatineeParticleTrack = StaticCast<UInterpTrackToggle*>(Track);
				if (MatineeParticleTrack->GetNumKeyframes() != 0 && PossessableGuid.IsValid())
				{
					UMovieSceneParticleTrack* ParticleTrack = NewMovieScene->AddTrack<UMovieSceneParticleTrack>(PossessableGuid);	
					FMatineeImportTools::CopyInterpParticleTrack(MatineeParticleTrack, ParticleTrack);
				}
			}
			else if (Track->IsA(UInterpTrackEvent::StaticClass()))
			{
				UInterpTrackEvent* MatineeEventTrack = StaticCast<UInterpTrackEvent*>(Track);
				if (MatineeEventTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneEventTrack* EventTrack = NewMovieScene->AddMasterTrack<UMovieSceneEventTrack>();
					FString EventTrackName = Group->GroupName.ToString() + TEXT("Events");
					EventTrack->SetDisplayName(FText::FromString(EventTrackName));
					FMatineeImportTools::CopyInterpEventTrack(MatineeEventTrack, EventTrack);

					static FName EventsFolder("Events");
					AddMasterTrackToFolder(NewMovieScene, EventTrack, EventsFolder);
				}
			}
			else if (Track->IsA(UInterpTrackSound::StaticClass()))
			{
				UInterpTrackSound* MatineeSoundTrack = StaticCast<UInterpTrackSound*>(Track);
				if (MatineeSoundTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneAudioTrack* AudioTrack = NewMovieScene->AddMasterTrack<UMovieSceneAudioTrack>();
					FString AudioTrackName = Group->GroupName.ToString() + TEXT("Audio");
					AudioTrack->SetDisplayName(FText::FromString(AudioTrackName));					
					FMatineeImportTools::CopyInterpSoundTrack(MatineeSoundTrack, AudioTrack);

					static FName AudioFolder("Audio");
					AddMasterTrackToFolder(NewMovieScene, AudioTrack, AudioFolder);
				}
			}
			else if (Track->IsA(UInterpTrackBoolProp::StaticClass()))
			{
				UInterpTrackBoolProp* MatineeBoolTrack = StaticCast<UInterpTrackBoolProp*>(Track);
				if (MatineeBoolTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneBoolTrack* BoolTrack = AddPropertyTrack<UMovieSceneBoolTrack>(MatineeBoolTrack->PropertyName, GroupActor, PossessableGuid, Player, NewSequence, NewMovieScene, NumWarnings);
					if (BoolTrack)
					{
						FMatineeImportTools::CopyInterpBoolTrack(MatineeBoolTrack, BoolTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackFloatProp::StaticClass()))
			{
				UInterpTrackFloatProp* MatineeFloatTrack = StaticCast<UInterpTrackFloatProp*>(Track);
				if (MatineeFloatTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneFloatTrack* FloatTrack = AddPropertyTrack<UMovieSceneFloatTrack>(MatineeFloatTrack->PropertyName, GroupActor, PossessableGuid, Player, NewSequence, NewMovieScene, NumWarnings);
					if (FloatTrack)
					{
						FMatineeImportTools::CopyInterpFloatTrack(MatineeFloatTrack, FloatTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackVectorProp::StaticClass()))
			{
				UInterpTrackVectorProp* MatineeVectorTrack = StaticCast<UInterpTrackVectorProp*>(Track);
				if (MatineeVectorTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneVectorTrack* VectorTrack = AddPropertyTrack<UMovieSceneVectorTrack>(MatineeVectorTrack->PropertyName, GroupActor, PossessableGuid, Player, NewSequence, NewMovieScene, NumWarnings);
					if (VectorTrack)
					{
						VectorTrack->SetNumChannelsUsed(3);
						FMatineeImportTools::CopyInterpVectorTrack(MatineeVectorTrack, VectorTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackColorProp::StaticClass()))
			{
				UInterpTrackColorProp* MatineeColorTrack = StaticCast<UInterpTrackColorProp*>(Track);
				if (MatineeColorTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneColorTrack* ColorTrack = AddPropertyTrack<UMovieSceneColorTrack>(MatineeColorTrack->PropertyName, GroupActor, PossessableGuid, Player, NewSequence, NewMovieScene, NumWarnings);
					if (ColorTrack)
					{
						FMatineeImportTools::CopyInterpColorTrack(MatineeColorTrack, ColorTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackLinearColorProp::StaticClass()))
			{
				UInterpTrackLinearColorProp* MatineeLinearColorTrack = StaticCast<UInterpTrackLinearColorProp*>(Track);
				if (MatineeLinearColorTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneColorTrack* ColorTrack = AddPropertyTrack<UMovieSceneColorTrack>(MatineeLinearColorTrack->PropertyName, GroupActor, PossessableGuid, Player, NewSequence, NewMovieScene, NumWarnings);
					if (ColorTrack)
					{
						FMatineeImportTools::CopyInterpLinearColorTrack(MatineeLinearColorTrack, ColorTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackVisibility::StaticClass()))
			{
				UInterpTrackVisibility* MatineeVisibilityTrack = StaticCast<UInterpTrackVisibility*>(Track);
				if (MatineeVisibilityTrack->GetNumKeyframes() != 0 && GroupActor && PossessableGuid.IsValid())
				{
					UMovieSceneVisibilityTrack* VisibilityTrack = NewMovieScene->AddTrack<UMovieSceneVisibilityTrack>(PossessableGuid);	
					if (VisibilityTrack)
					{
						VisibilityTrack->SetPropertyNameAndPath(TEXT("bHidden"), GroupActor->GetPathName() + TEXT(".bHidden"));

						FMatineeImportTools::CopyInterpVisibilityTrack(MatineeVisibilityTrack, VisibilityTrack);
					}
				}
			}
			else if (Track->IsA(UInterpTrackDirector::StaticClass()))
			{
				// Intentionally left blank - The director track is converted in a separate pass below.
			}
			else
			{
				if (GroupActor)
				{
					UE_LOG(LogMatineeToLevelSequence, Warning, TEXT("Unsupported track '%s' for '%s'."), *Track->TrackTitle, *GroupActor->GetActorLabel());
				}
				else
				{
					UE_LOG(LogMatineeToLevelSequence, Warning, TEXT("Unsupported track '%s'."), *Track->TrackTitle);
				}

				++NumWarnings;
			}
		}
	}

	/** Convert a single matinee to a level sequence asset */
	TWeakObjectPtr<ALevelSequenceActor> ConvertSingleMatineeToLevelSequence(TWeakObjectPtr<AActor> ActorToConvert, int32& NumWarnings)
	{
		UObject* AssetOuter = ActorToConvert->GetOuter();
		UPackage* AssetPackage = AssetOuter->GetOutermost();

		FString NewLevelSequenceAssetName = ActorToConvert->GetActorLabel() + FString("LevelSequence");
		FString NewLevelSequenceAssetPath = AssetPackage->GetName();
		int LastSlashPos = NewLevelSequenceAssetPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		NewLevelSequenceAssetPath = NewLevelSequenceAssetPath.Left(LastSlashPos);

		// Create a new level sequence asset with the appropriate name
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UObject* NewAsset = nullptr;
		for (TObjectIterator<UClass> It ; It ; ++It)
		{
			UClass* CurrentClass = *It;
			if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
			{
				UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
				if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == ULevelSequence::StaticClass())
				{
					NewAsset = AssetTools.CreateAssetWithDialog(NewLevelSequenceAssetName, NewLevelSequenceAssetPath, ULevelSequence::StaticClass(), Factory);
					break;
				}
			}
		}

		if (!NewAsset)
		{
			return nullptr;
		}

		UMovieSceneSequence* NewSequence = Cast<UMovieSceneSequence>(NewAsset);
		UMovieScene* NewMovieScene = NewSequence->GetMovieScene();

		// Add a level sequence actor for this new sequence
		UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ALevelSequenceActor::StaticClass());
		if (!ensure(ActorFactory))
		{
			return nullptr;
		}

		ALevelSequenceActor* NewActor = CastChecked<ALevelSequenceActor>(GEditor->UseActorFactory(ActorFactory, FAssetData(NewAsset), &FTransform::Identity));

		struct FTemporaryPlayer : IMovieScenePlayer
		{
			FTemporaryPlayer(UMovieSceneSequence& InSequence, UObject* InContext)
				: Context(InContext)
			{
				RootInstance.Initialize(InSequence, *this);
			}

			virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() { return RootInstance; }
			virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject = nullptr, bool bJumpCut = false) {}
			virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) {}
			virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const {}
			virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const { return EMovieScenePlayerStatus::Stopped; }
			virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) {}
			virtual UObject* GetPlaybackContext() const { return Context; }

			FMovieSceneRootEvaluationTemplateInstance RootInstance;
			UObject* Context;

		} TemporaryPlayer(*NewSequence, NewActor->GetWorld());

		// Walk through all the interp group data and create corresponding tracks on the new level sequence asset
		if (ActorToConvert->IsA(AMatineeActor::StaticClass()))
		{
			AMatineeActor* MatineeActor = StaticCast<AMatineeActor*>(ActorToConvert.Get());
			MatineeActor->InitInterp();

			// Set the length
			NewMovieScene->SetPlaybackRange(0.0f, MatineeActor->MatineeData->InterpLength);

			// Convert the groups
			for (int32 i=0; i<MatineeActor->GroupInst.Num(); ++i)
			{
				UInterpGroupInst* GrInst = MatineeActor->GroupInst[i];
				UInterpGroup* Group = GrInst->Group;
				AActor* GroupActor = GrInst->GetGroupActor();
				ConvertInterpGroup(Group, GroupActor, TemporaryPlayer, NewSequence, NewMovieScene, NumWarnings);
			}

			// Director group - convert this after the regular groups to ensure that the camera cut bindings are there
			UInterpGroupDirector* DirGroup = MatineeActor->MatineeData->FindDirectorGroup();
			if (DirGroup)
			{
				UInterpTrackDirector* MatineeDirectorTrack = DirGroup->GetDirectorTrack();
				if (MatineeDirectorTrack && MatineeDirectorTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneCameraCutTrack* CameraCutTrack = NewMovieScene->AddMasterTrack<UMovieSceneCameraCutTrack>();
					FMatineeImportTools::CopyInterpDirectorTrack(MatineeDirectorTrack, CameraCutTrack, MatineeActor, TemporaryPlayer);
				}

				UInterpTrackFade* MatineeFadeTrack = DirGroup->GetFadeTrack();
				if (MatineeFadeTrack && MatineeFadeTrack->GetNumKeyframes() != 0)
				{						
					UMovieSceneFadeTrack* FadeTrack = NewMovieScene->AddMasterTrack<UMovieSceneFadeTrack>();
					FMatineeImportTools::CopyInterpFadeTrack(MatineeFadeTrack, FadeTrack);
				}

				UInterpTrackSlomo* MatineeSlomoTrack = DirGroup->GetSlomoTrack();
				if (MatineeSlomoTrack && MatineeSlomoTrack->GetNumKeyframes() != 0)
				{
					UMovieSceneSlomoTrack* SlomoTrack = NewMovieScene->AddMasterTrack<UMovieSceneSlomoTrack>();
					FMatineeImportTools::CopyInterpFloatTrack(MatineeSlomoTrack, SlomoTrack);
				}
				
				UInterpTrackColorScale* MatineeColorScaleTrack = DirGroup->GetColorScaleTrack();
				if (MatineeColorScaleTrack && MatineeColorScaleTrack->GetNumKeyframes() != 0)
				{
					UE_LOG(LogMatineeToLevelSequence, Warning, TEXT("Unsupported track '%s'."), *MatineeColorScaleTrack->TrackTitle);
					++NumWarnings;
				}

				UInterpTrackAudioMaster* MatineeAudioMasterTrack = DirGroup->GetAudioMasterTrack();
				if (MatineeAudioMasterTrack && MatineeAudioMasterTrack->GetNumKeyframes() != 0)
				{
					UE_LOG(LogMatineeToLevelSequence, Warning, TEXT("Unsupported track '%s'."), *MatineeAudioMasterTrack->TrackTitle);
					++NumWarnings;
				}
			}

			MatineeActor->TermInterp();
		}
		return NewActor;
	}

private:

	FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors LevelEditorMenuExtenderDelegate;

	FDelegateHandle LevelEditorExtenderDelegateHandle;

	// IMatineeToLevelSequenceModule interface
	TMap<TSubclassOf<UInterpTrack>, FOnConvertMatineeTrack > ExtendedInterpConverters;
};

IMPLEMENT_MODULE(FMatineeToLevelSequenceModule, MatineeToLevelSequence);

#undef LOCTEXT_NAMESPACE
