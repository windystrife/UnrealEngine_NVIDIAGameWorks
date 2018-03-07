// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/IMenu.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/EngineTypes.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Matinee/MatineeAnimInterface.h"
#include "AssetData.h"
#include "Sound/SoundBase.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "EditorModeInterpolation.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpData.h"
#include "Matinee.h"
#include "InterpTrackHelper.h"
#include "MatineeTrackAnimControlHelper.h"
#include "MatineeTrackBoolPropHelper.h"
#include "MatineeTrackDirectorHelper.h"
#include "MatineeTrackEventHelper.h"
#include "MatineeTrackFloatPropHelper.h"
#include "MatineeTrackParticleReplayHelper.h"
#include "MatineeTrackSoundHelper.h"
#include "MatineeTrackToggleHelper.h"
#include "MatineeTrackVectorPropHelper.h"
#include "MatineeTrackColorPropHelper.h"
#include "MatineeTrackLinearColorPropHelper.h"
#include "MatineeTrackVisibilityHelper.h"
#include "MatineeUtils.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Matinee/MatineeActor.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackToggle.h"
#include "Matinee/InterpTrackVisibility.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpGroupInst.h"
#include "Widgets/Input/STextComboPopup.h"
#include "Widgets/Input/STextEntryPopup.h"

#define LOCTEXT_NAMESPACE "MatineeTrackHelpers"

FName	UInterpTrackHelper::KeyframeAddDataName = NAME_None;

UInterpTrackHelper::UInterpTrackHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

AActor* UInterpTrackHelper::GetGroupActor(const UInterpTrack* Track) const
{
	FEdModeInterpEdit* mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	check(mode != NULL);

	IMatineeBase* InterpEd = mode->InterpEd;
	check(InterpEd != NULL);

	UInterpGroupInst* GrInst = NULL;

	// Traverse through the selected tracks in hopes of finding the associated group. 
	for( FSelectedTrackIterator TrackIt(InterpEd->GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		if( (*TrackIt) == Track )
		{
			GrInst = InterpEd->GetMatineeActor()->FindFirstGroupInst(TrackIt.GetGroup());
			break;
		}
	}
	
	return ( GrInst != NULL ) ? GrInst->GetGroupActor() : NULL;
}

// Common FName used just for storing name information while adding Keyframes to tracks.
static UAnimSequence*	KeyframeAddAnimSequence = NULL;
static USoundBase*		KeyframeAddSound = NULL;
static FName			TrackAddPropName = NAME_None;
static FName			AnimSlotName = NAME_None;
static TWeakPtr< class IMenu > EntryMenu;

/**
 * Sets the global property name to use for newly created property tracks
 *
 * @param NewName The property name
 */
void FMatinee::SetTrackAddPropName( const FName NewName )
{
	TrackAddPropName = NewName;
}

UMatineeTrackAnimControlHelper::UMatineeTrackAnimControlHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackAnimControlHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const
{
	// For AnimControl tracks - pop up a dialog to choose slot name.
	AnimSlotName = FAnimSlotGroup::DefaultSlotName;

	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	check(Mode != NULL);

	IMatineeBase* InterpEd = Mode->InterpEd;
	check(InterpEd != NULL);

	UInterpGroupInst* GrInst = InterpEd->GetMatineeActor()->FindFirstGroupInst(Group);
	check(GrInst);

	AActor* Actor = GrInst->GetGroupActor();
	if ( Actor != NULL )
	{
		IMatineeAnimInterface* MatineeAnimInterface = Cast<IMatineeAnimInterface>(Actor);
		if (!MatineeAnimInterface)
		{
			UE_LOG(LogSlateMatinee, Log, TEXT("InterpGroup : MatineeAnimInterface is missing for (%s)"), *Actor->GetName());
			return false;
		}

		// If this is the first AnimControlTrack, then init anim control now.
		// We need that before calling GetAnimControlSlotDesc
		if( !Group->HasAnimControlTrack() )
		{
			MatineeAnimInterface->PreviewBeginAnimControl( Group );
		}

		if( bAllowPrompts )
		{
			TArray<FAnimSlotDesc> SlotDescs;
			MatineeAnimInterface->GetAnimControlSlotDesc(SlotDescs);

			// If we get no information - just allow it to be created with empty slot.
			if( SlotDescs.Num() == 0 )
			{
				return true;
			}

			// Build combo to let you pick a slot. Don't put any names in that have already used all their channels. */

			TArray<FString> SlotStrings;
			for(int32 i=0; i<SlotDescs.Num(); i++)
			{
				int32 ChannelsUsed = GrInst->Group->GetAnimTracksUsingSlot( SlotDescs[i].SlotName );
				if(ChannelsUsed < SlotDescs[i].NumChannels)
				{
					SlotStrings.Add(*SlotDescs[i].SlotName.ToString());
				}
			}

			// If no slots free - we fail to create track.
			if(SlotStrings.Num() == 0)
			{
				FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_NoAnimChannelsLeft", "This Actor Has No AnimControl Channels Left.") );
				return false;
			}

			TSharedRef<SWindow> NewWindow = SNew(SWindow)
				.Title(NSLOCTEXT("Matinee.Popups", "ChooseAnimSlotTitle", "Choose Anim Slot..."))
				.SizingRule( ESizingRule::Autosized )
				.SupportsMinimize(false)
				.SupportsMaximize(false);

			FString Result;
			TSharedRef<STextComboPopup> TextEntryPopup = 
				SNew(STextComboPopup)
				.Label(NSLOCTEXT("Matinee.Popups", "ChooseAnimSlot", "Choose Anim Slot..."))
				.TextOptions(SlotStrings)
				.OnTextChosen_UObject(this, &UMatineeTrackAnimControlHelper::OnCreateTrackTextEntry, NewWindow, (FString *)&Result)
				;

			NewWindow->SetContent(TextEntryPopup);
			GEditor->EditorAddModalWindow(NewWindow);
			if (!Result.IsEmpty())
			{			
				AnimSlotName = FName( *Result );
				if ( AnimSlotName != NAME_None )
				{
					return true;
				}
			}
		}
		else
		{
			// Prompts aren't allowed, so just succeed with defaults
			return true;
		}
	}

	return false;
}

void UMatineeTrackAnimControlHelper::OnCreateTrackTextEntry(const FString& ChosenText, TSharedRef<SWindow> Window, FString* OutputString)
{
	*OutputString = ChosenText;
	Window->RequestDestroyWindow();
}

void  UMatineeTrackAnimControlHelper::PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const
{
	UInterpTrackAnimControl* AnimTrack = CastChecked<UInterpTrackAnimControl>(Track);
	AnimTrack->SlotName = AnimSlotName;

	// When you change the SlotName, change the TrackTitle to reflect that.
	UInterpTrackAnimControl* DefAnimTrack = AnimTrack->GetClass()->GetDefaultObject<UInterpTrackAnimControl>();
	FString DefaultTrackTitle = DefAnimTrack->TrackTitle;

	if(AnimTrack->SlotName == NAME_None)
	{
		AnimTrack->TrackTitle = DefaultTrackTitle;
	}
	else
	{
		AnimTrack->TrackTitle = FString::Printf( TEXT("%s:%s"), *DefaultTrackTitle, *AnimTrack->SlotName.ToString() );
	}
}


bool UMatineeTrackAnimControlHelper::PreCreateKeyframe( UInterpTrack *Track, float fTime ) const
{
	KeyframeAddAnimSequence = NULL;
	UInterpTrackAnimControl	*AnimTrack = CastChecked<UInterpTrackAnimControl>(Track);
	UInterpGroup* Group = CastChecked<UInterpGroup>(Track->GetOuter());

	AActor* Actor = GetGroupActor(Track);
	if (!Actor)
	{
		// error message
		UE_LOG(LogSlateMatinee, Warning, TEXT("No Actor is selected. Select actor first."));
		return false;
	}

	USkeletalMeshComponent * SkelMeshComp = NULL;

	TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	Actor->GetComponents(SkeletalMeshComponents);

	for (int32 I=0; I<SkeletalMeshComponents.Num(); ++I)
	{
		USkeletalMeshComponent * CurSkelMeshComp = SkeletalMeshComponents[I];
		// if qualified to play animation, break
		if (CurSkelMeshComp->SkeletalMesh && CurSkelMeshComp->SkeletalMesh->Skeleton)
		{
			SkelMeshComp = CurSkelMeshComp;
			break;
		}
	}

	if (!SkelMeshComp)
	{
		UE_LOG(LogSlateMatinee, Warning, TEXT("SkeletalMeshComponent isn't found in the selected actor or it does not have Mesh/Skeleton set up in order to play animation"));
		return false;
	}

	USkeleton* Skeleton = SkelMeshComp->SkeletalMesh->Skeleton;
	if ( Skeleton )
	{
		// Show the dialog.
		FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
		check(Mode != NULL);
		check(Mode->InterpEd != NULL);
		
		TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
		if ( Parent.IsValid() )
		{
			FAssetPickerConfig AssetPickerConfig;
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject( this, &UMatineeTrackAnimControlHelper::OnAddKeyTextEntry, Mode->InterpEd, Track );
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

			// Filter config
			AssetPickerConfig.Filter.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
			AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());

			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

			FMenuBuilder MenuBuilder(true, NULL);
			MenuBuilder.BeginSection(NAME_None, LOCTEXT("MatineeAnimPicker", "Browse"));
			{
				TSharedPtr<SBox> MenuEntry = SNew(SBox)
				.WidthOverride(300.0f)
				.HeightOverride(300.f)
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				];
				MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
			}
			MenuBuilder.EndSection();

			EntryMenu = FSlateApplication::Get().PushMenu(
				Parent.ToSharedRef(),
				FWidgetPath(),
				MenuBuilder.MakeWidget(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
				);
		}
	}
	else
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "NoAnimSeqsFound", "No AnimSequences Found. Make sure to load AnimSequences.") );
	}

	return false;
}

void UMatineeTrackAnimControlHelper::OnAddKeyTextEntry(const FAssetData& AssetData, IMatineeBase* Matinee, UInterpTrack* Track)
{
	if (EntryMenu.IsValid())
	{
		EntryMenu.Pin()->Dismiss();
	}

	UObject* SelectedObject = AssetData.GetAsset();
	if (SelectedObject && SelectedObject->IsA(UAnimSequence::StaticClass()))
	{
		KeyframeAddAnimSequence = CastChecked<UAnimSequence>(AssetData.GetAsset());

		Matinee->FinishAddKey(Track, true);
	}
}

void  UMatineeTrackAnimControlHelper::PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const
{
	UInterpTrackAnimControl	*AnimTrack = CastChecked<UInterpTrackAnimControl>(Track);
	FAnimControlTrackKey& NewSeqKey = AnimTrack->AnimSeqs[ KeyIndex ];
	NewSeqKey.AnimSeq = KeyframeAddAnimSequence;
	KeyframeAddAnimSequence = NULL;
}


UMatineeTrackDirectorHelper::UMatineeTrackDirectorHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackDirectorHelper::PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const

{
	// If adding a cut, bring up combo to let user choose group to cut to.
	KeyframeAddDataName = NAME_None;

	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	check(Mode != NULL && Mode->InterpEd != NULL);

	if ( (Mode != NULL) && (Mode->InterpEd != NULL) )
	{
		// Make array of group names
		TArray<FString> GroupNames;
		for ( UInterpGroup* InterpGroup : Mode->InterpEd->GetInterpData()->InterpGroups )
		{
			if ( !InterpGroup->bIsFolder )
			{
				GroupNames.Add(InterpGroup->GroupName.ToString());
			}
		}

		TSharedRef<STextComboPopup> TextEntryPopup =
			SNew(STextComboPopup)
			.Label(NSLOCTEXT("Matinee.Popups", "NewCut", "Cut to Group..."))
			.TextOptions(GroupNames)
			.OnTextChosen_UObject(this, &UMatineeTrackDirectorHelper::OnAddKeyTextEntry, Mode->InterpEd, Track);

		TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
		if ( Parent.IsValid() )
		{
			EntryMenu = FSlateApplication::Get().PushMenu(
				Parent.ToSharedRef(),
				FWidgetPath(),
				TextEntryPopup,
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
				);
		}
	}

	return false;
}

void UMatineeTrackDirectorHelper::OnAddKeyTextEntry(const FString& ChosenText, IMatineeBase* Matinee, UInterpTrack* Track)
{
	if (EntryMenu.IsValid())
	{
		EntryMenu.Pin()->Dismiss();
	}
	
	KeyframeAddDataName = FName( *ChosenText );
	Matinee->FinishAddKey(Track,true);
}


void  UMatineeTrackDirectorHelper::PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const
{
	UInterpTrackDirector	*DirectorTrack = CastChecked<UInterpTrackDirector>(Track);
	FDirectorTrackCut& NewDirCut = DirectorTrack->CutTrack[ KeyIndex ];
	NewDirCut.TargetCamGroup = KeyframeAddDataName;
	KeyframeAddDataName = NAME_None;
}



UMatineeTrackEventHelper::UMatineeTrackEventHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


bool UMatineeTrackEventHelper::PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const
{
	KeyframeAddDataName = NAME_None;

	// Prompt user for name of new event.
	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	check(Mode != NULL);
	check(Mode->InterpEd != NULL);
	TSharedRef<STextEntryPopup> TextEntryPopup = 
		SNew(STextEntryPopup)
		.Label(NSLOCTEXT("Matinee.Popups", "NewEventName", "New Event Name"))
		.DefaultText(FText::FromString(TEXT("Event")))
		.OnTextCommitted_UObject(this, &UMatineeTrackEventHelper::OnAddKeyTextEntry, (IMatineeBase*)Mode->InterpEd, Track)
		.SelectAllTextWhenFocused(true)
		.ClearKeyboardFocusOnCommit(false)
		.MaxWidth(1024.0f)
		;

	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if ( Parent.IsValid() )
	{
		EntryMenu = FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			FWidgetPath(),
			TextEntryPopup,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}

	return false;
}

void UMatineeTrackEventHelper::OnAddKeyTextEntry(const FText& ChosenText, ETextCommit::Type CommitInfo, IMatineeBase* Matinee, UInterpTrack* Track)
{
	if (EntryMenu.IsValid())
	{
		EntryMenu.Pin()->Dismiss();
	}

	if (CommitInfo == ETextCommit::OnEnter)
	{
		FString TempString = ChosenText.ToString().Left(NAME_SIZE);
		TempString = TempString.Replace(TEXT(" "),TEXT("_"));
		KeyframeAddDataName = FName( *TempString );

		Matinee->FinishAddKey(Track,true);
	}
}

void  UMatineeTrackEventHelper::PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const
{
	UInterpTrackEvent	*EventTrack = CastChecked<UInterpTrackEvent>(Track);
	FEventTrackKey& NewEventKey = EventTrack->EventTrack[ KeyIndex ];
	NewEventKey.EventName = KeyframeAddDataName;

	// Update AllEventNames array now we have given it a name
	UInterpGroup* Group = CastChecked<UInterpGroup>( EventTrack->GetOuter() );
	UInterpData* IData = CastChecked<UInterpData>( Group->GetOuter() );
	IData->Modify();
	IData->UpdateEventNames();

	KeyframeAddDataName = NAME_None;
}

UMatineeTrackSoundHelper::UMatineeTrackSoundHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackSoundHelper::PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();
	KeyframeAddSound = GEditor->GetSelectedObjects()->GetTop<USoundBase>();
	if ( KeyframeAddSound )
	{
		return true;
	}

	FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "NoSoundCueSelected", "Cannot Add Sound. No SoundCue Selected In Browser.") );
	return false;
}


void  UMatineeTrackSoundHelper::PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const
{
	UInterpTrackSound	*SoundTrack = CastChecked<UInterpTrackSound>(Track);

	// Assign the chosen SoundCue to the new key.
	FSoundTrackKey& NewSoundKey = SoundTrack->Sounds[ KeyIndex ];
	NewSoundKey.Sound = KeyframeAddSound;
	KeyframeAddSound = NULL;
}

UMatineeTrackFloatPropHelper::UMatineeTrackFloatPropHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackFloatPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const
{
	if( bAllowPrompts && bDuplicatingTrack == false )
	{
		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
		check(Mode != NULL);

		IMatineeBase* InterpEd = Mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->GetMatineeActor()->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			FMatineeUtils::GetInterpFloatPropertyNames(Actor, PropNames);

			if( PropNames.Num() > 0 )
			{
				TArray<FString> PropStrings;
				PropStrings.AddZeroed( PropNames.Num() );
				for(int32 i=0; i<PropNames.Num(); i++)
				{
					PropStrings[i] = PropNames[i].ToString();
				}

				TSharedRef<SWindow> NewWindow = SNew(SWindow)
					.Title(NSLOCTEXT("Matinee.Popups", "ChooseProperty", "Choose Property..."))
					.SizingRule( ESizingRule::Autosized )
					.SupportsMinimize(false)
					.SupportsMaximize(false);

				FString Result;
				TSharedRef<STextComboPopup> TextEntryPopup = 
					SNew(STextComboPopup)
					.Label(NSLOCTEXT("Matinee.Popups", "PropertyName", "Property Name"))
					.TextOptions(PropStrings)
					.OnTextChosen_UObject(this, &UMatineeTrackFloatPropHelper::OnCreateTrackTextEntry, NewWindow, (FString *)&Result)
					;

				NewWindow->SetContent(TextEntryPopup);
				GEditor->EditorAddModalWindow(NewWindow);
			
				if (!Result.IsEmpty())
				{
					TrackAddPropName = FName( *Result );
					if ( TrackAddPropName != NAME_None )
					{
						// Check we don't already have a track controlling this property.
						for(int32 i=0; i<Group->InterpTracks.Num(); i++)
						{
							UInterpTrackFloatProp* TestFloatTrack = Cast<UInterpTrackFloatProp>( Group->InterpTracks[i] );
							if(TestFloatTrack && TestFloatTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_PropertyAlreadyControlled", "Already a FloatProp track controlling this property.") );
								return false;
							}

							UInterpTrackBoolProp* TestBoolTrack = Cast<UInterpTrackBoolProp>( Group->InterpTracks[i] );
							if(TestBoolTrack && TestBoolTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_BoolPropertyAlreadyControlled", "Already a BoolProp track controlling this property.") );
								return false;
							}

							UInterpTrackVectorProp* TestVectorTrack = Cast<UInterpTrackVectorProp>( Group->InterpTracks[i] );
							if(TestVectorTrack && TestVectorTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_VectorPropertyAlreadyControlled", "Already a VectorProp track controlling this property.") );
								return false;
							}
							UInterpTrackLinearColorProp* TestLinearColorTrack = Cast<UInterpTrackLinearColorProp>( Group->InterpTracks[i] );
							if(TestLinearColorTrack && TestLinearColorTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_LinearPropertyAlreadyControlled", "Already a LinearProp track controlling this property.") );
								return false;
							}					
						}

						return true;
					}
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT( "MatineeFloatTrackHelper_NoProperties", "No Float track properties are available for this actor" ) );
			}
		}

		return false;
	}
	else
	{
		return true;
	}
}

void  UMatineeTrackFloatPropHelper::OnCreateTrackTextEntry(const FString& ChosenText, TSharedRef<SWindow> Window, FString* OutputString)
{
	*OutputString = ChosenText;
	Window->RequestDestroyWindow();
}

void  UMatineeTrackFloatPropHelper::PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const
{
	if(bDuplicatingTrack == false)
	{
		UInterpTrackFloatProp	*PropTrack = CastChecked<UInterpTrackFloatProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		int32 PeriodPos = PropString.Find(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}




UMatineeTrackBoolPropHelper::UMatineeTrackBoolPropHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


bool UMatineeTrackBoolPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack* TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const
{
	if( bAllowPrompts && bDuplicatingTrack == false )
	{
		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
		check(Mode != NULL);

		IMatineeBase* InterpEd = Mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->GetMatineeActor()->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			FMatineeUtils::GetInterpBoolPropertyNames(Actor, PropNames);

			if( PropNames.Num() > 0 )
			{
				TArray<FString> PropStrings;
				PropStrings.AddZeroed( PropNames.Num() );
				for(int32 i=0; i<PropNames.Num(); i++)
				{
					PropStrings[i] = PropNames[i].ToString();
				}

				TSharedRef<SWindow> NewWindow = SNew(SWindow)
					.Title(NSLOCTEXT("Matinee.Popups", "ChooseProperty", "Choose Property..."))
					.SizingRule( ESizingRule::Autosized )
					.SupportsMinimize(false)
					.SupportsMaximize(false);

				FString Result;
				TSharedRef<STextComboPopup> TextEntryPopup = 
					SNew(STextComboPopup)
					.Label(NSLOCTEXT("Matinee.Popups", "PropertyName", "Property Name"))
					.TextOptions(PropStrings)
					.OnTextChosen_UObject(this, &UMatineeTrackBoolPropHelper::OnCreateTrackTextEntry, TWeakPtr<SWindow>(NewWindow), (FString *)&Result);

				NewWindow->SetContent(TextEntryPopup);
				GEditor->EditorAddModalWindow(NewWindow);
				if (!Result.IsEmpty())
				{
					TrackAddPropName = FName( *Result );
					if ( TrackAddPropName != NAME_None )
					{
						// Check we don't already have a track controlling this property.
						for(int32 i=0; i<Group->InterpTracks.Num(); i++)
						{
							UInterpTrackFloatProp* TestFloatTrack = Cast<UInterpTrackFloatProp>( Group->InterpTracks[i] );
							if(TestFloatTrack && TestFloatTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_PropertyAlreadyControlled", "Already a FloatProp track controlling this property.") );
								return false;
							}

							UInterpTrackBoolProp* TestBoolTrack = Cast<UInterpTrackBoolProp>( Group->InterpTracks[i] );
							if(TestBoolTrack && TestBoolTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_BoolPropertyAlreadyControlled", "Already a BoolProp track controlling this property.") );
								return false;
							}

							UInterpTrackVectorProp* TestVectorTrack = Cast<UInterpTrackVectorProp>( Group->InterpTracks[i] );
							if(TestVectorTrack && TestVectorTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_VectorPropertyAlreadyControlled", "Already a VectorProp track controlling this property.") );
								return false;
							}
							UInterpTrackLinearColorProp* TestLinearColorTrack = Cast<UInterpTrackLinearColorProp>( Group->InterpTracks[i] );
							if(TestLinearColorTrack && TestLinearColorTrack->PropertyName == TrackAddPropName)
							{
								FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_LinearPropertyAlreadyControlled", "Already a LinearProp track controlling this property.") );
								return false;
							}					
						}

						return true;
					}
				}
			}
			else
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT( "MatineeBoolTrackHelper_NoProperties", "No Bool track properties are available for this actor" ) );
			}
		}

		return false;
	}
	else
	{
		return true;
	}
}

void UMatineeTrackBoolPropHelper::OnCreateTrackTextEntry(const FString& ChosenText, TWeakPtr<SWindow> Window, FString* OutputString)
{
	*OutputString = ChosenText;
	if( Window.IsValid() )
	{
		Window.Pin()->RequestDestroyWindow();
	}
}

void UMatineeTrackBoolPropHelper::PostCreateTrack( UInterpTrack* Track, bool bDuplicatingTrack, int32 TrackIndex ) const
{
	if(bDuplicatingTrack == false)
	{
		UInterpTrackBoolProp* PropTrack = CastChecked<UInterpTrackBoolProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		int32 PeriodPos = PropString.Find(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}


UMatineeTrackToggleHelper::UMatineeTrackToggleHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


bool UMatineeTrackToggleHelper::PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const
{

	bool bResult = false;

	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	check(Mode != NULL);

	IMatineeBase* InterpEd = Mode->InterpEd;
	check(InterpEd != NULL);

	TArray<FString> PropStrings;
	PropStrings.AddZeroed( 3 );
	PropStrings[0] = TEXT("Trigger");
	PropStrings[1] = TEXT("On");
	PropStrings[2] = TEXT("Off");

	TSharedRef<STextComboPopup> TextEntryPopup = 
		SNew(STextComboPopup)
		.Label(NSLOCTEXT("Matinee.Popups", "ToggleAction", "Toggle Action"))
		.TextOptions(PropStrings)
		.OnTextChosen_UObject(this, &UMatineeTrackToggleHelper::OnAddKeyTextEntry, InterpEd, Track)
		;

	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if ( Parent.IsValid() )
	{
		EntryMenu = FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			FWidgetPath(),
			TextEntryPopup,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}

	return bResult;
}

void UMatineeTrackToggleHelper::OnAddKeyTextEntry(const FString& ChosenText, IMatineeBase* Matinee, UInterpTrack* Track)
{
	if (EntryMenu.IsValid())
	{
		EntryMenu.Pin()->Dismiss();
	}

	KeyframeAddDataName = FName(*ChosenText);
	Matinee->FinishAddKey(Track,true);
}

void  UMatineeTrackToggleHelper::PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const
{
	UInterpTrackToggle* ToggleTrack = CastChecked<UInterpTrackToggle>(Track);

	FToggleTrackKey& NewToggleKey = ToggleTrack->ToggleTrack[KeyIndex];
	if (KeyframeAddDataName == FName(TEXT("On")))
	{
		NewToggleKey.ToggleAction = ETTA_On;
	}
	else
		if (KeyframeAddDataName == FName(TEXT("Trigger")))
		{
			NewToggleKey.ToggleAction = ETTA_Trigger;
		}
		else
		{
			NewToggleKey.ToggleAction = ETTA_Off;
		}

		KeyframeAddDataName = NAME_None;
}

UMatineeTrackVectorPropHelper::UMatineeTrackVectorPropHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackVectorPropHelper::ChooseProperty(TArray<FName> &PropNames) const
{
	bool bResult = false;

	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	check(Mode != NULL);

	IMatineeBase* InterpEd = Mode->InterpEd;
	check(InterpEd != NULL);
	check( InterpEd->GetSelectedGroupCount() == 1 );

	UInterpGroup* Group = *(InterpEd->GetSelectedGroupIterator());

	TArray<FString> PropStrings;
	PropStrings.AddZeroed( PropNames.Num() );
	for(int32 i=0; i<PropNames.Num(); i++)
	{
		PropStrings[i] = PropNames[i].ToString();
	}

	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(NSLOCTEXT("Matinee.Popups", "ChooseProperty", "Choose Property..."))
		.SizingRule( ESizingRule::Autosized )
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	FString Result;
	TSharedRef<STextComboPopup> TextEntryPopup = 
		SNew(STextComboPopup)
		.Label(NSLOCTEXT("Matinee.Popups", "PropertyName", "Property Name"))
		.TextOptions(PropStrings)
		.OnTextChosen_UObject(this, &UMatineeTrackVectorPropHelper::OnCreateTrackTextEntry, TWeakPtr<SWindow>(NewWindow), (FString *)&Result)
		;

	NewWindow->SetContent(TextEntryPopup);
	GEditor->EditorAddModalWindow(NewWindow);
	if (!Result.IsEmpty())
	{
		TrackAddPropName = FName( *Result );
		if ( TrackAddPropName != NAME_None )
		{
			bResult = true;

			// Check we don't already have a track controlling this property.
			for(int32 i=0; i<Group->InterpTracks.Num(); i++)
			{
				UInterpTrackFloatProp* TestFloatTrack = Cast<UInterpTrackFloatProp>( Group->InterpTracks[i] );
				if(TestFloatTrack && TestFloatTrack->PropertyName == TrackAddPropName)
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_PropertyAlreadyControlled", "Already a FloatProp track controlling this property.") );
					bResult = false;
				}

				UInterpTrackBoolProp* TestBoolTrack = Cast<UInterpTrackBoolProp>( Group->InterpTracks[i] );
				if(TestBoolTrack && TestBoolTrack->PropertyName == TrackAddPropName)
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_BoolPropertyAlreadyControlled", "Already a BoolProp track controlling this property.") );
					return false;
				}

				UInterpTrackVectorProp* TestVectorTrack = Cast<UInterpTrackVectorProp>( Group->InterpTracks[i] );
				if(TestVectorTrack && TestVectorTrack->PropertyName == TrackAddPropName)
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_VectorPropertyAlreadyControlled", "Already a VectorProp track controlling this property.") );
					bResult = false;
				}

				UInterpTrackLinearColorProp* TestLinearColorTrack = Cast<UInterpTrackLinearColorProp>( Group->InterpTracks[i] );
				if(TestLinearColorTrack && TestLinearColorTrack->PropertyName == TrackAddPropName)
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_LinearPropertyAlreadyControlled", "Already a LinearProp track controlling this property.") );
					bResult = false;
				}			
			}
		}
	}

	return bResult;
}

void UMatineeTrackVectorPropHelper::OnCreateTrackTextEntry(const FString& ChosenText, TWeakPtr<SWindow> Window, FString* OutputString)
{
	*OutputString = ChosenText;
	if( Window.IsValid() )
	{
		Window.Pin()->RequestDestroyWindow();
	}
}

bool UMatineeTrackVectorPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const
{
	bool bResult = true;

	if( bAllowPrompts && bDuplicatingTrack == false )
	{
		bResult = false;

		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
		check(Mode != NULL);

		IMatineeBase* InterpEd = Mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->GetMatineeActor()->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			FMatineeUtils::GetInterpVectorPropertyNames(Actor, PropNames);
			bResult = ChooseProperty(PropNames);
		}
	}

	return bResult;
}

void  UMatineeTrackVectorPropHelper::PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const
{
	if(bDuplicatingTrack == false)
	{
		UInterpTrackVectorProp	*PropTrack = CastChecked<UInterpTrackVectorProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		int32 PeriodPos = PropString.Find(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}

UMatineeTrackColorPropHelper::UMatineeTrackColorPropHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackColorPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const
{
	bool bResult = true;

	if( bAllowPrompts && bDuplicatingTrack == false )
	{
		bResult = false;

		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
		check(Mode != NULL);

		IMatineeBase* InterpEd = Mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->GetMatineeActor()->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			FMatineeUtils::GetInterpColorPropertyNames(Actor, PropNames);
			bResult = PropNames.Num() > 0 ? ChooseProperty(PropNames) : false;

			if( !bResult )
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT( "MatineeColorTrackHelper_NoProperties", "No Color track properties are available for this actor" ) );
			}
		}
	}

	return bResult;
}

void  UMatineeTrackColorPropHelper::PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const
{
	if(bDuplicatingTrack == false)
	{
		UInterpTrackColorProp	*PropTrack = CastChecked<UInterpTrackColorProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		int32 PeriodPos = PropString.Find(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}



UMatineeTrackLinearColorPropHelper::UMatineeTrackLinearColorPropHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackLinearColorPropHelper::PreCreateTrack( UInterpGroup* Group, const UInterpTrack *TrackDef, bool bDuplicatingTrack, bool bAllowPrompts ) const
{
	bool bResult = true;

	if( bAllowPrompts && bDuplicatingTrack == false )
	{
		bResult = false;

		// For Property tracks - pop up a dialog to choose property name.
		TrackAddPropName = NAME_None;

		FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
		check(Mode != NULL);

		IMatineeBase *InterpEd = Mode->InterpEd;
		check(InterpEd != NULL);

		UInterpGroupInst* GrInst = InterpEd->GetMatineeActor()->FindFirstGroupInst(Group);
		check(GrInst);

		AActor* Actor = GrInst->GetGroupActor();
		if ( Actor != NULL )
		{
			TArray<FName> PropNames;
			FMatineeUtils::GetInterpLinearColorPropertyNames(Actor, PropNames);
			bResult = PropNames.Num() > 0 ? ChooseProperty(PropNames) : false;

			if( !bResult )
			{
				FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT( "MatineeLinearColorTrackHelper_NoProperties", "No LinearColor track properties are available for this actor" ) );
			}
		}
	}

	return bResult;
}

void  UMatineeTrackLinearColorPropHelper::PostCreateTrack( UInterpTrack *Track, bool bDuplicatingTrack, int32 TrackIndex ) const
{
	if(bDuplicatingTrack == false)
	{
		UInterpTrackLinearColorProp	*PropTrack = CastChecked<UInterpTrackLinearColorProp>(Track);

		// Set track title to property name (cut off component name if there is one).
		FString PropString = TrackAddPropName.ToString();
		int32 PeriodPos = PropString.Find(TEXT("."));
		if(PeriodPos != INDEX_NONE)
		{
			PropString = PropString.Mid(PeriodPos+1);
		}

		PropTrack->PropertyName = TrackAddPropName;
		PropTrack->TrackTitle = *PropString;

		TrackAddPropName = NAME_None;
	}
}

UMatineeTrackVisibilityHelper::UMatineeTrackVisibilityHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


bool UMatineeTrackVisibilityHelper::PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const
{
	bool bResult = false;

	FEdModeInterpEdit* Mode = (FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );
	check(Mode != NULL);

	IMatineeBase *InterpEd = Mode->InterpEd;
	check(InterpEd != NULL);

	TArray<FString> PropStrings;
	PropStrings.AddZeroed( 3 );
	PropStrings[0] = TEXT("Show");
	PropStrings[1] = TEXT("Hide");
	PropStrings[2] = TEXT("Toggle");

	TSharedRef<STextComboPopup> TextEntryPopup = 
		SNew(STextComboPopup)
		.Label(NSLOCTEXT("Matinee.Popups", "VisibilityAction", "Visibility Action"))
		.TextOptions(PropStrings)
		.OnTextChosen_UObject(this, &UMatineeTrackVisibilityHelper::OnAddKeyTextEntry, Mode->InterpEd, Track)
		;

	TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow();
	if ( Parent.IsValid() )
	{
		EntryMenu = FSlateApplication::Get().PushMenu(
			Parent.ToSharedRef(),
			FWidgetPath(),
			TextEntryPopup,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}

	return bResult;
}

void UMatineeTrackVisibilityHelper::OnAddKeyTextEntry(const FString& ChosenText, IMatineeBase* Matinee, UInterpTrack* Track)
{
	if (EntryMenu.IsValid())
	{
		EntryMenu.Pin()->Dismiss();
	}

	KeyframeAddDataName = FName(*ChosenText);
	Matinee->FinishAddKey(Track,true);
}

void  UMatineeTrackVisibilityHelper::PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const
{
	UInterpTrackVisibility* VisibilityTrack = CastChecked<UInterpTrackVisibility>(Track);

	FVisibilityTrackKey& NewVisibilityKey = VisibilityTrack->VisibilityTrack[KeyIndex];

	if (KeyframeAddDataName == FName(TEXT("Show")))
	{
		NewVisibilityKey.Action = EVTA_Show;
	}
	else
		if (KeyframeAddDataName == FName(TEXT("Toggle")))
		{
			NewVisibilityKey.Action = EVTA_Toggle;
		}
		else	// "Hide"
		{
			NewVisibilityKey.Action = EVTA_Hide;
		}


		// Default to Always firing this event.  The user can change it later by right clicking on the
		// track keys in the editor.
		NewVisibilityKey.ActiveCondition = EVTC_Always;

		KeyframeAddDataName = NAME_None;
}



UMatineeTrackParticleReplayHelper::UMatineeTrackParticleReplayHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMatineeTrackParticleReplayHelper::PreCreateKeyframe( UInterpTrack *Track, float KeyTime ) const
{
	// We don't currently need to do anything here

	// @todo: It would be nice to pop up a dialog where the user can select a clip ID number
	//        from a list of replay clips that exist in emitter actor.

	return true;
}

void  UMatineeTrackParticleReplayHelper::PostCreateKeyframe( UInterpTrack *Track, int32 KeyIndex ) const
{
	// We don't currently need to do anything here
}

#undef LOCTEXT_NAMESPACE
