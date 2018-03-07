// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Engine/Brush.h"
#include "Engine/Engine.h"
#include "Animation/AnimSequence.h"
#include "Editor/UnrealEdEngine.h"
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Engine/Light.h"
#include "Camera/CameraAnim.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "EditorDirectories.h"
#include "UnrealEdGlobals.h"
#include "Matinee/InterpGroup.h"
#include "MatineeGroupData.h"
#include "Matinee/InterpTrack.h"
#include "MatineeTrackData.h"
#include "EngineGlobals.h"
#include "Editor.h"
#include "MatineeModule.h"

#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackInstMove.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackToggle.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackVisibility.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackFloatProp.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "Matinee/InterpTrackLinearColorBase.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackParticleReplay.h"
#include "Matinee/InterpTrackVectorMaterialParam.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackInstDirector.h"
#include "Matinee/InterpTrackAudioMaster.h"
#include "Matinee/InterpGroupDirector.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpGroupInstDirector.h"
#include "Matinee/InterpFilter_Custom.h"

#include "MatineeActions.h"
#include "MatineeOptions.h"
#include "MatineeTransBuffer.h"
#include "SMatineeRecorder.h"
#include "Engine/InterpCurveEdSetup.h"
#include "Matinee.h"
#include "MatineeDelegates.h"

#include "CameraController.h"
#include "DesktopPlatformModule.h"
#include "PackageTools.h"


#include "Matinee/MatineeActor.h"
#include "FbxExporter.h"

#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "Interfaces/IMainFrameModule.h"

#include "Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextComboPopup.h"
#include "Sound/SoundBase.h"


#define LOCTEXT_NAMESPACE "MatineeMenus"


namespace
{
void* GetMatineeDialogParentWindow()
{
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
	return (MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid()) ? MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
}
}

FText FMatinee::GenericTextEntryModal(const FText& Title, const FText& DialogText, const FText& DefaultText)
{
	FText Result;
	struct Local
	{
		static void OnTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, TSharedRef<SWindow> Window, FText* OutText) 
		{
			if (CommitInfo == ETextCommit::OnEnter)
			{
				*OutText = InText;
			}
			Window->RequestDestroyWindow();
		}
	};

	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(Title)
		.SizingRule( ESizingRule::Autosized )
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.FocusWhenFirstShown(true)
		;

	TSharedRef<STextEntryPopup> TextEntryPopup = 
		SNew(STextEntryPopup)
		.Label(DialogText)
		.DefaultText( DefaultText )
		.SelectAllTextWhenFocused(true)
		.OnTextCommitted_Static(&Local::OnTextCommitted, NewWindow, &Result)
		;
	NewWindow->SetContent(TextEntryPopup);
	
	GEditor->EditorAddModalWindow(NewWindow);

	return Result;
}

void FMatinee::GenericTextEntryModeless(const FText& DialogText, const FText& DefaultText, FOnTextCommitted OnTextComitted)
{
	TSharedRef<STextEntryPopup> TextEntryPopup = 
		SNew(STextEntryPopup)
		.Label(DialogText)
		.DefaultText(DefaultText)
		.OnTextCommitted(OnTextComitted)
		.ClearKeyboardFocusOnCommit(false)
		.SelectAllTextWhenFocused(true)
		.MaxWidth(1024.0f);

	EntryPopupMenu = FSlateApplication::Get().PushMenu(
		ToolkitHost.Pin()->GetParentWidget(),
		FWidgetPath(),
		TextEntryPopup,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
		);
}

void FMatinee::CloseEntryPopupMenu()
{
	if (EntryPopupMenu.IsValid())
	{
		EntryPopupMenu.Pin()->Dismiss();
	}
}

/**
	* Utility function for retrieving a new name from the user.
	*
	* @note	Any spaces in the name will be converted to underscores. 
	*
	* @param	InDialogTitle			The title of dialog.
	* @param	InDialogCaption			The caption that displayed next to the text box.
	* @param	InDefaultText			The default name to put in the text box when first showing the dialog.
	* @param	OnTextCommitted			Delegate to call when text is successfully committed
	*/
void FMatinee::GetNewNamePopup( const FText& InDialogTitle, const FText& InDialogCaption, const FText& InDefaultText, const FText& InOriginalName, FOnTextCommitted OnTextCommitted )
{
	// Decide what the title will be. If we were given the original name of the 
	// group, we want to put that in the dialog's title to help out the user.

	FFormatNamedArguments TitleArgs;
	TitleArgs.Add( TEXT("DialogTitle"), InDialogTitle );
	TitleArgs.Add( TEXT("OriginalName"), InOriginalName );
	const FText Title = !InOriginalName.IsEmpty() ? FText::Format( LOCTEXT("NewNameWindowTitle", "{DialogTitle} - {OriginalName}"), TitleArgs ) : InDialogTitle;

	FFormatNamedArguments TextArgs;
	TextArgs.Add( TEXT("Title"), Title );
	TextArgs.Add( TEXT("DialogCaption"), InDialogCaption );
	const FText DialogText = FText::Format( LOCTEXT("NewNameWindowTitleWithCaption", "{Title} - {DialogCaption}"), TextArgs );

	//Get the new name (dialog)...
	GenericTextEntryModeless(DialogText, InDefaultText, FOnTextCommitted::CreateSP(this, &FMatinee::OnNewNamePopupTextCommitted, OnTextCommitted));			
}

void FMatinee::OnNewNamePopupTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FOnTextCommitted OnTextComitted)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{	
		CloseEntryPopupMenu();
		if (!InText.IsEmpty())
		{
			// Make sure there are no spaces!
			FString EnteredString = InText.ToString();
			EnteredString = EnteredString.Replace(TEXT(" "),TEXT("_"));

			OnTextComitted.ExecuteIfBound( FText::FromString( EnteredString ),CommitInfo );
		}
	}
};

///// MENU CALLBACKS

// Add a new keyframe on the selected track 
void FMatinee::OnMenuAddKey()
{
	AddKey();
}

void FMatinee::OnContextNewGroup( FMatineeCommands::EGroupAction::Type InActionId)
{
	const bool bIsNewFolder = (InActionId == FMatineeCommands::EGroupAction::NewFolder); 
	const bool bDirGroup = (InActionId == FMatineeCommands::EGroupAction::NewDirectorGroup); 
	const bool bDuplicateGroup = (InActionId == FMatineeCommands::EGroupAction::DuplicateGroup); 
	const bool bLightingGroup = (InActionId == FMatineeCommands::EGroupAction::NewLightingGroup); 

	// Only one director group is allowed
	if (bDirGroup && IData->FindDirectorGroup())
	{
		FNotificationInfo Info( NSLOCTEXT("UnrealEd", "FailedToAddDirectorGroupNotification", "Warning: A new director group cannot be added; one already exists.") );
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		return;
	}

	if(bDuplicateGroup && !HasAGroupSelected())
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "InterpEd_Duplicate_NoGroup", "Must Select A Group Before Duplicating") );
		return;
	}

	// This is temporary - need a unified way to associate tracks with components/actors etc... hmm..
	AActor* GroupActor = NULL;
	TArray<AActor *> OtherActorsToAddToGroup;

	if(!bIsNewFolder && !bDirGroup && !bDuplicateGroup)
	{
		// find if they have any other actor they want
		for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
		{
			AActor* Actor = static_cast<AActor*>( *It );
			checkSlow( Actor->IsA(AActor::StaticClass()) );

			if (Actor == MatineeActor)
			{
				// Don't even attempt to add ourself to the group
				continue;
			}

			if ( PrepareToAddActorAndWarnUser(Actor) )
			{
				// first set the GroupActor
				if (GroupActor==NULL)
				{
					GroupActor = Actor;
				}
				else // if GroupActor is set, now add to OtherActorsToAdd 
				{
					OtherActorsToAddToGroup.Add(Actor);
				}
			}
		}

		// ignore any other actor unless it's pawn
		if (bLightingGroup && GroupActor)
		{
			if (!GroupActor->IsA(ALight::StaticClass()))
			{
				GroupActor = NULL;
			}
		}

		if(GroupActor)
		{
			// Check that the Outermost of both the Matinee actor and the actor to interp are the same
			// We can't create a group for an Actor that is not in the same level as the Matinee Actor
			UObject* MatineeActorOutermost = MatineeActor->GetOutermost();
			UObject* ActorOutermost = GroupActor->GetOutermost();
			if(ActorOutermost != MatineeActorOutermost)
			{
				FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_ActorNotInSequenceLevel", "Actor is not in the same Level as the Matinee Actor trying to control it.") );
				return;
			}

		}
	}

	NewGroupPopup( InActionId, GroupActor,  OtherActorsToAddToGroup);
}

bool FMatinee::CanCreateNewGroup( FMatineeCommands::EGroupAction::Type InActionId ) const
{
	return !IsCameraAnim();
}

void FMatinee::NewGroupPopup( FMatineeCommands::EGroupAction::Type InActionId, AActor* GroupActor, TArray<AActor *> OtherActorsToAddToGroup )
{
	// Find out if we want to make a 'Director' group.
	const bool bIsNewFolder = (InActionId == FMatineeCommands::EGroupAction::NewFolder); 
	const bool bDirGroup = (InActionId == FMatineeCommands::EGroupAction::NewDirectorGroup); 
	const bool bDuplicateGroup = (InActionId == FMatineeCommands::EGroupAction::DuplicateGroup); 

	// If not a director group - ask for a name.
	if(!bDirGroup)
	{
		FText DialogName;
		FText DefaultNewGroupName;
		switch( InActionId )
		{
		case FMatineeCommands::EGroupAction::NewCameraGroup: 
			DialogName = LOCTEXT( "NewGroupName", "New Group Name" );
			DefaultNewGroupName = LOCTEXT( "NewCameraGroup", "NewCameraGroup" );
			break;

		case FMatineeCommands::EGroupAction::NewParticleGroup: 
			DialogName = LOCTEXT( "NewGroupName", "New Group Name" );
			DefaultNewGroupName = LOCTEXT( "NewParticleGroup", "NewParticleGroup" );
			break;

		case FMatineeCommands::EGroupAction::NewSkeletalMeshGroup: 
			DialogName = LOCTEXT( "NewGroupName", "New Group Name" );
			DefaultNewGroupName = LOCTEXT( "NewSkeletalMeshGroup", "NewSkeletalMeshGroup" );
			break;

		case FMatineeCommands::EGroupAction::NewLightingGroup: 
			DialogName = LOCTEXT( "NewGroupName", "New Group Name" );
			DefaultNewGroupName = LOCTEXT( "NewLightingGroup", "NewLightingGroup" );
			break;

		case FMatineeCommands::EGroupAction::NewFolder: 
			DialogName = LOCTEXT( "NewFolderName", "New Folder Name" );
			DefaultNewGroupName = LOCTEXT( "NewFolder", "NewFolder" );
			break;

		case FMatineeCommands::EGroupAction::DuplicateGroup: 
			// When duplicating, we use unlocalized text 
			// at the moment. So, the spaces are needed.
			DialogName = LOCTEXT( "NewGroupName", "New Group Name" );
			DefaultNewGroupName = LOCTEXT( "NewGroup", "New Group" );
			break;

		default:
			DialogName = LOCTEXT( "NewGroupName", "New Group Name" );
			DefaultNewGroupName = LOCTEXT( "NewGroup", "New Group" );
			break;
		}

		if( bDuplicateGroup )
		{
			for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
			{
				FName NewName;
				const FText GroupName = FText::FromName( (*GroupIt)->GroupName );

				GetNewNamePopup(DialogName, FText::GetEmpty(), GroupName, GroupName,
					FOnTextCommitted::CreateSP(this, 
						&FMatinee::NewGroupPopupTextCommitted, InActionId, GroupActor, OtherActorsToAddToGroup, *GroupIt)
					);
				break;
			}
		}
		else
		{
			GetNewNamePopup(DialogName, FText::GetEmpty(), DefaultNewGroupName, FText::GetEmpty(),  
				FOnTextCommitted::CreateSP(this, 
					&FMatinee::NewGroupPopupTextCommitted, InActionId, GroupActor, OtherActorsToAddToGroup, (UInterpGroup*)NULL)
				);
		}
	}
	else
	{
		//For director group... we have no popup, just commit with empty text
		NewGroupPopupTextCommitted(FText(), ETextCommit::OnEnter, InActionId, GroupActor, OtherActorsToAddToGroup, NULL);
	}
}

void FMatinee::NewGroupPopupTextCommitted(
	const FText& InText, ETextCommit::Type, 
	FMatineeCommands::EGroupAction::Type InActionId, 
	AActor* GroupActor, 
	TArray<AActor *> OtherActorsToAddToGroup, 
	UInterpGroup* GroupToDuplicate)
{
	//note: we don't need to check commit type... handled by GetNewNamePopup

	FName NewGroupName = FName(*InText.ToString().Left(NAME_SIZE));
	const bool bIsNewFolder = (InActionId == FMatineeCommands::EGroupAction::NewFolder); 
	const bool bDirGroup = (InActionId == FMatineeCommands::EGroupAction::NewDirectorGroup); 
	const bool bDuplicateGroup = (InActionId == FMatineeCommands::EGroupAction::DuplicateGroup); 

	TMap<UInterpGroup*, FName> DuplicateGroupToNameMap;
	if (bDuplicateGroup && GroupToDuplicate != NULL)
	{
		DuplicateGroupToNameMap.Add( GroupToDuplicate, NewGroupName );	
	}

	// Create new InterpGroup.
	TArray<UInterpGroup*> NewGroups;

	// Begin undo transaction
	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "NewGroup", "New Group") );
	MatineeActor->Modify();
	IData->Modify();

	TArray< FAnalyticsEventAttribute > GroupAttribs;
	GroupAttribs.Add(FAnalyticsEventAttribute(TEXT("ActionId"), FString::Printf(TEXT("%d"), static_cast<int32>(InActionId))));
	if(bDirGroup)
	{
		UInterpGroup* NewDirector = NewObject<UInterpGroupDirector>(IData, NAME_None, RF_Transactional);
		NewGroups.Add(NewDirector);
		GroupAttribs.Add(FAnalyticsEventAttribute(TEXT("Name"), NewDirector->GroupName.ToString()));
	}
	else if(bDuplicateGroup)
	{
		// There should not be a director selected because there can only be one!
		check( !HasAGroupSelected(UInterpGroupDirector::StaticClass()) );

		// Duplicate each selected group.
		for( TMap<UInterpGroup*,FName>::TIterator GroupIt(DuplicateGroupToNameMap); GroupIt; ++GroupIt )
		{
			UInterpGroup* DupGroup = (UInterpGroup*)StaticDuplicateObject( GroupIt.Key(), IData, NAME_None, RF_Transactional );
			DupGroup->GroupName = GroupIt.Value();
			// we need to insert these into correct spot if we'd keep the folder, and if not this will add to the last group or folder or whatever
			// which will crash again, so I'm disabling duplicating parenting. 
			DupGroup->bIsParented = false;
			NewGroups.Add(DupGroup);
			GroupAttribs.Add(FAnalyticsEventAttribute(TEXT("Name"), DupGroup->GroupName.ToString()));
		}
	}
	else
	{
		UInterpGroup* NewGroup = NewObject<UInterpGroup>(IData, NAME_None, RF_Transactional);
		NewGroup->GroupName = NewGroupName;
		NewGroups.Add(NewGroup);
		GroupAttribs.Add(FAnalyticsEventAttribute(TEXT("Name"), NewGroup->GroupName.ToString()));
	}

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.NewGroup"), GroupAttribs);
	}

	IData->InterpGroups.Append(NewGroups);

	// Deselect any previous group so that we are only selecting the duplicated groups.
	DeselectAllGroups(false);

	// if there's no group actor
	if ( !GroupActor )
	{
		if( InActionId == FMatineeCommands::EGroupAction::NewCameraGroup )
		{
			// find the first perspective viewport - if one exists
			FLevelEditorViewportClient* ViewportClient = NULL;
			for( int32 iView = 0; iView < GEditor->LevelViewportClients.Num(); iView++ )
			{
				ViewportClient = GEditor->LevelViewportClients[ iView ];
				if( ViewportClient->IsPerspective() )
				{
					break;
				}
			}
			UWorld* World = ViewportClient ? ViewportClient->GetWorld() : GWorld;
			ACameraActor* NewCamera = World->SpawnActor<ACameraActor>();
			if( ViewportClient )
			{
				NewCamera->SetActorLocation( ViewportClient->GetViewLocation(), false );
				NewCamera->SetActorRotation( ViewportClient->GetViewRotation() );
			}
			GroupActor = NewCamera;
		}
	}

	for( TArray<UInterpGroup*>::TIterator NewGroupIt(NewGroups); NewGroupIt; ++NewGroupIt )
	{
		UInterpGroup* NewGroup = *NewGroupIt;
		// since now it's multiple groups, it should find what is current iterator of new groups

		// All groups must have a unique name.
		NewGroup->EnsureUniqueName();

		// Randomly generate a group colour for the new group.
		NewGroup->GroupColor = FColor::MakeRandomColor();

		// Set whether this is a folder or not
		NewGroup->bIsFolder = bIsNewFolder;

		NewGroup->Modify();

		// Folders don't need a group instance
		UInterpGroupInst* NewGroupInst = NULL;
		//@todo UE4 Matinee: No Kismet
		//USeqVar_Character* AIVarObj = NULL;
		if( !bIsNewFolder )
		{
			// Create new InterpGroupInst
			if(bDirGroup)
			{
				NewGroupInst = NewObject<UInterpGroupInstDirector>(MatineeActor, NAME_None, RF_Transactional);
				NewGroupInst->InitGroupInst(NewGroup, NULL);
			}
			else
			{
				NewGroupInst = NewObject<UInterpGroupInst>(MatineeActor, NAME_None, RF_Transactional);
				// Initialize group instance, saving ref to actor it works on.
				NewGroupInst->InitGroupInst(NewGroup, GroupActor);
			}

			const int32 NewGroupInstIndex = MatineeActor->GroupInst.Add(NewGroupInst);

			NewGroupInst->Modify();
		}


		// Don't need to save state here - no tracks!

		// If a director group, create a director track for it now.
		if(bDirGroup)
		{
			UInterpTrack* NewDirTrack = NewObject<UInterpTrackDirector>(NewGroup, NAME_None, RF_Transactional);
			const int32 TrackIndex = NewGroup->InterpTracks.Add(NewDirTrack);

			UInterpTrackInst* NewDirTrackInst = NewObject<UInterpTrackInstDirector>(NewGroupInst, NAME_None, RF_Transactional);
			NewGroupInst->TrackInst.Add(NewDirTrackInst);

			NewDirTrackInst->InitTrackInst(NewDirTrack);
			NewDirTrackInst->SaveActorState(NewDirTrack);

			// Save for undo then redo.
			NewDirTrack->Modify();
			NewDirTrackInst->Modify();

			SelectTrack( NewGroup, NewDirTrack );
		}
		// If regular track, create a new object variable connector, and variable containing selected actor if there is one.
		else
		{
			// Folder's don't need to be bound to actors
			if( !bIsNewFolder )
			{
				MatineeActor->InitGroupActorForGroup(NewGroup, GroupActor);
			}

			// For Camera or Skeletal Mesh groups, add a Movement track
			// FIXME: this doesn't work like this anymore
			// if you'd like to create multiple groups at once
			if( InActionId == FMatineeCommands::EGroupAction::NewCameraGroup || 
				InActionId == FMatineeCommands::EGroupAction::NewSkeletalMeshGroup )
			{
				int32 NewTrackIndex = INDEX_NONE;
				AddTrackToGroup( NewGroup, UInterpTrackMove::StaticClass(), NULL, false, NewTrackIndex );
			}

			// For Camera groups, add a Float Property track for FOV
			if (InActionId == FMatineeCommands::EGroupAction::NewCameraGroup)
			{
				// Set the property name for the new track.  This is a global that will be used when setting everything up.
				SetTrackAddPropName( FName( TEXT( "FOVAngle" ) ) );

				int32 NewTrackIndex = INDEX_NONE;
				UInterpTrack* NewTrack = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, false, NewTrackIndex );
			}

			// For Lighting groups, add a Movement, Brightness, Light Color, and Radius Property track
			if ( InActionId == FMatineeCommands::EGroupAction::NewLightingGroup )
			{
				UInterpTrack* NewMovTrack = NewObject<UInterpTrackMove>(NewGroup, NAME_None, RF_Transactional);
				const int32 TrackIndex = NewGroup->InterpTracks.Add(NewMovTrack);

				UInterpTrackInst* NewMovTrackInst = NewObject<UInterpTrackInstMove>(NewGroupInst, NAME_None, RF_Transactional);
				NewGroupInst->TrackInst.Add(NewMovTrackInst);

				NewMovTrackInst->InitTrackInst(NewMovTrack);
				NewMovTrackInst->SaveActorState(NewMovTrack);

				// Save for undo then redo.
				NewMovTrack->Modify();
				NewMovTrackInst->Modify();

				int32 NewTrackIndex = INDEX_NONE;

				// Set the property name for the new track.  Since this is a global we need to add the track after calling this and then
				// set the next prop name.
				SetTrackAddPropName( FName( TEXT( "Intensity" ) ) );
				UInterpTrack* NewTrackBrightness = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, false, NewTrackIndex );

				SetTrackAddPropName( FName( TEXT( "LightColor" ) ) );
				UInterpTrack* NewTrackLightColor = AddTrackToGroup( NewGroup, UInterpTrackColorProp::StaticClass(), NULL, false, NewTrackIndex );

				SetTrackAddPropName( FName( TEXT( "Radius" ) ) );
				UInterpTrack* NewTrackRadius = AddTrackToGroup( NewGroup, UInterpTrackFloatProp::StaticClass(), NULL, false, NewTrackIndex );
			}

			// For Skeletal Mesh groups, add an Anim track
			if( InActionId == FMatineeCommands::EGroupAction::NewSkeletalMeshGroup)
			{
				int32 NewTrackIndex = INDEX_NONE;
				AddTrackToGroup( NewGroup, UInterpTrackAnimControl::StaticClass(), NULL, false, NewTrackIndex );
			}

			// For Particle groups, add a Toggle track
			if( InActionId == FMatineeCommands::EGroupAction::NewParticleGroup )
			{
				int32 NewTrackIndex = INDEX_NONE;
				AddTrackToGroup( NewGroup, UInterpTrackToggle::StaticClass(), NULL, false, NewTrackIndex );
			}
		}


		// If we have a custom filter tab currently selected, then add the new group to that filter tab
		{
			UInterpFilter_Custom* CustomFilter = Cast< UInterpFilter_Custom >( IData->SelectedFilter );
			if( CustomFilter != NULL && IData->InterpFilters.Contains( CustomFilter ) )
			{
				check( !CustomFilter->GroupsToInclude.Contains( NewGroup ) );

				// Add the new group to the custom filter tab!
				CustomFilter->GroupsToInclude.Add( NewGroup );
			}
		}

		// add extra tracks if it's required
		for (int32 I=0; I<OtherActorsToAddToGroup.Num(); ++I)
		{
			if (OtherActorsToAddToGroup[I])
			{
				AddActorToGroup(NewGroup, OtherActorsToAddToGroup[I]);
			}
		}

		// After the group has been setup, add it to the current group selection.
		SelectGroup(NewGroup, false);
	}


	InterpEdTrans->EndSpecial();

	// Make sure particle replay tracks have up-to-date editor-only transient state
	UpdateParticleReplayTracks();

	// A new group or track may have been added, so we'll update the group list scroll bar
	UpdateTrackWindowScrollBars();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();

	// If adding a camera- make sure its frustum colour is updated.
	UpdateCamColours();

	// Reimage actor world locations.  This must happen after the group was created.
	MatineeActor->RecaptureActorState();

	GEditor->RedrawAllViewports();
}

void FMatinee::OnContextNewTrack(UClass* NewInterpTrackClass)
{
	// You can only add a new track if only one group is selected
	if( GetSelectedGroupCount() != 1 )
	{
		return;
	}

	check(NewInterpTrackClass->IsChildOf(UInterpTrack::StaticClass()));

	AddTrackToSelectedGroup(NewInterpTrackClass, NULL);
}

bool FMatinee::CanCreateNewTrack(UClass* NewInterpTrackClass) const
{
	if(IsCameraAnim())
	{
		check( NewInterpTrackClass->IsChildOf(UInterpTrack::StaticClass()) );

		return  NewInterpTrackClass->IsChildOf(UInterpTrackMove::StaticClass()) ||
				NewInterpTrackClass->IsChildOf(UInterpTrackFloatProp::StaticClass()) ||
				NewInterpTrackClass->IsChildOf(UInterpTrackVectorProp::StaticClass()) || 
				NewInterpTrackClass->IsChildOf(UInterpTrackLinearColorProp::StaticClass());
	}

	return true;
}


/**
 * Called when the user selects the 'Expand All Groups' option from a menu.  Expands every group such that the
 * entire hierarchy of groups and tracks are displayed.
 */
void FMatinee::OnExpandAllGroups()
{
	const bool bWantExpand = true;
	ExpandOrCollapseAllVisibleGroups( bWantExpand );
}



/**
 * Called when the user selects the 'Collapse All Groups' option from a menu.  Collapses every group in the group
 * list such that no tracks are displayed.
 */
void FMatinee::OnCollapseAllGroups()
{
	const bool bWantExpand = false;
	ExpandOrCollapseAllVisibleGroups( bWantExpand );
}



/**
 * Expands or collapses all visible groups in the track editor
 *
 * @param bExpand true to expand all groups, or false to collapse them all
 */
void FMatinee::ExpandOrCollapseAllVisibleGroups( const bool bExpand )
{
	// We'll keep track of whether or not something changes
	bool bAnythingChanged = false;

	// Iterate over each group
	for( int32 CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex	)
	{
		UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];
		check( CurGroup != NULL );
															 
		// Only expand/collapse visible groups
		const bool bIsCollapsing = !bExpand;
		if( CurGroup->bVisible )
		{
			if( CurGroup->bCollapsed != bIsCollapsing )
			{
				// Expand or collapse this group!
				CurGroup->bCollapsed = bIsCollapsing;
			}
		}
	}

	if( bAnythingChanged )
	{
		// @todo: Should we re-scroll to the currently selected group if needed?

		// At least one group has been expanded or collapsed, so we need to update our scroll bar
		UpdateTrackWindowScrollBars();
	}
}


void FMatinee::OnMenuPlay(const bool bShouldLoop, const bool bPlayForward)
{
	StartPlaying( bShouldLoop, bPlayForward );
}


void FMatinee::OnMenuStop()
{
	StopPlaying();
}

void FMatinee::OnMenuPause()
{
	if (MatineeActor->bIsPlaying)
	{
		StopPlaying();
	}
	else
	{
		// Start playback and retain whatever direction we were already playing
		ResumePlaying();
	}
}

void FMatinee::OnChangePlaySpeed( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo )
{
	int32 Index;
	bool bFound = SpeedSettingStrings.Find(NewSelection, Index);
	check(bFound);

	PlaybackSpeed = 1.f;
	
	switch( Index )
	{
	case 4: 
		PlaybackSpeed = 0.01f;
		break;
	case 3: 
		PlaybackSpeed = 0.1f;
		break;
	case 2: 
		PlaybackSpeed = 0.25f;
		break;
	case 1: 
		PlaybackSpeed = 0.5f;
		break;
	case 0: 
		PlaybackSpeed = 1.0f;
		break;
	}
	
	// Playback speed changed, so reset our playback start time so fixed time step playback can
	// gate frame rate properly
	PlaybackStartRealTime = FPlatformTime::Seconds();
	NumContinuousFixedTimeStepFrames = 0;
}

void FMatinee::StretchSection( bool bUseSelectedOnly )
{
	// Edit section markers should always be within sequence...
	float SectionStart = IData->EdSectionStart;
	float SectionEnd = IData->EdSectionEnd;

	if( bUseSelectedOnly )
	{
		// reverse the section start/end - good way to initialise the data to be written over
		SectionStart = IData->EdSectionEnd;
		SectionEnd = IData->EdSectionStart;

		if( !Opt->SelectedKeys.Num() )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "InterpEd_NoKeyframesSelected", "No Keyframes Were Selected" ) );
		}

		for( int32 iKey = 0; iKey < Opt->SelectedKeys.Num(); iKey++ )
		{
			FInterpEdSelKey& SelKey = Opt->SelectedKeys[ iKey ];

			UInterpTrack* Track = SelKey.Track;
			float CurrentKeyTime = Track->GetKeyframeTime(SelKey.KeyIndex);
			if( CurrentKeyTime < SectionStart )
			{
				SectionStart = CurrentKeyTime;
			}
			if( CurrentKeyTime > SectionEnd )
			{
				SectionEnd = CurrentKeyTime;
			}
		}
	}

	const float CurrentSectionLength = SectionEnd - SectionStart;
	if(CurrentSectionLength < 0.01f)
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_HighlightNonZeroLength", "You must highlight a non-zero length section before stretching it.") );
		return;
	}

	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "StretchSection", "New Length..."),
		FText::AsNumber( CurrentSectionLength ),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnStretchSectionTextEntered, SectionStart, SectionEnd, CurrentSectionLength)
		);
}

void FMatinee::OnStretchSectionTextEntered(const FText& InText, ETextCommit::Type CommitInfo, float SectionStart, float SectionEnd, float CurrentSectionLength )
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double dNewSectionLength = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if(!bIsNumber)
			return;

		const float NewSectionLength = (float)dNewSectionLength;
		if(NewSectionLength <= 0.f)
			return;

		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "StretchSection", "Stretch Section") );

		IData->Modify();
		Opt->Modify();

		const float LengthDiff = NewSectionLength - CurrentSectionLength;
		const float StretchRatio = NewSectionLength/CurrentSectionLength;

		// Iterate over all tracks.
		for(int32 i=0; i<IData->InterpGroups.Num(); i++)
		{
			UInterpGroup* Group = IData->InterpGroups[i];
			for(int32 j=0; j<Group->InterpTracks.Num(); j++)
			{
				UInterpTrack* Track = Group->InterpTracks[j];

				Track->Modify();

				for(int32 k=0; k<Track->GetNumKeyframes(); k++)
				{
					const float KeyTime = Track->GetKeyframeTime(k);

					// Key is before start of stretched section
					if(KeyTime < SectionStart)
					{
						// Leave key as it is
					}
					// Key is in section being stretched
					else if(KeyTime < SectionEnd)
					{
						// Calculate new key time.
						const float FromSectionStart = KeyTime - SectionStart;
						const float NewKeyTime = SectionStart + (StretchRatio * FromSectionStart);

						Track->SetKeyframeTime(k, NewKeyTime, false);
					}
					// Key is after stretched section
					else
					{
						// Move it on by the increase in sequence length.
						Track->SetKeyframeTime(k, KeyTime + LengthDiff, false);
					}
				}
			}
		}

		// Move the end of the interpolation to account for changing the length of this section.
		SetInterpEnd(IData->InterpLength + LengthDiff);

		// Move end marker of section to new, stretched position.
		MoveLoopMarker( IData->EdSectionEnd + LengthDiff, false );

		InterpEdTrans->EndSpecial();
	}
}

void FMatinee::OnMenuStretchSection()
{
	StretchSection( false );
}

void FMatinee::OnMenuStretchSelectedKeyframes()
{
	StretchSection( true );
}

/** Remove the currernt section, reducing the length of the sequence and moving any keys after the section earlier in time. */
void FMatinee::OnMenuDeleteSection()
{
	const float CurrentSectionLength = IData->EdSectionEnd - IData->EdSectionStart;
	if(CurrentSectionLength < 0.01f)
		return;

	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "DeleteSection", "Delete Section") );

	IData->Modify();
	Opt->Modify();

	// Add keys that are within current section to selection
	SelectKeysInLoopSection();

	// Delete current selection
	DeleteSelectedKeys(false);

	// Then move any keys after the current section back by the length of the section.
	for(int32 i=0; i<IData->InterpGroups.Num(); i++)
	{
		UInterpGroup* Group = IData->InterpGroups[i];
		for(int32 j=0; j<Group->InterpTracks.Num(); j++)
		{
			UInterpTrack* Track = Group->InterpTracks[j];
			Track->Modify();

			for(int32 k=0; k<Track->GetNumKeyframes(); k++)
			{
				// Move keys after section backwards by length of the section
				float KeyTime = Track->GetKeyframeTime(k);
				if(KeyTime > IData->EdSectionEnd)
				{
					// Add to selection for deletion.
					Track->SetKeyframeTime(k, KeyTime - CurrentSectionLength, false);
				}
			}
		}
	}

	// Move the end of the interpolation to account for changing the length of this section.
	SetInterpEnd(IData->InterpLength - CurrentSectionLength);

	// Move section end marker on top of section start marker (section has vanished).
	MoveLoopMarker( IData->EdSectionStart, false );

	InterpEdTrans->EndSpecial();

	// Notify Curve Editor of the change
	CurveEd->CurveChanged();
}

/** Insert an amount of space (specified by user in dialog) at the current position in the sequence. */
void FMatinee::OnMenuInsertSpace()
{
	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "InsertEmptySpace", "Seconds:"),
		FText::AsNumber( 1.0f ),
		FOnTextCommitted::CreateSP(this,&FMatinee::OnInsertSpaceTextEntry)
		);
}

void FMatinee::OnInsertSpaceTextEntry(const FText& InText, ETextCommit::Type CommitInfo)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		bool bIsNumber = InText.IsNumeric();
		if(!bIsNumber)
			return;

		double dAddTime = FCString::Atod(*InText.ToString());
		float AddTime = (float)dAddTime;

		// Ignore if adding a negative amount of time!
		if(AddTime <= 0.f)
			return;

		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InsertSpace", "Insert Space") );

		IData->Modify();
		Opt->Modify();

		// Move the end of the interpolation on by the amount we are adding.
		SetInterpEnd(IData->InterpLength + AddTime);

		// Iterate over all tracks.
		for(int32 i=0; i<IData->InterpGroups.Num(); i++)
		{
			UInterpGroup* Group = IData->InterpGroups[i];
			for(int32 j=0; j<Group->InterpTracks.Num(); j++)
			{
				UInterpTrack* Track = Group->InterpTracks[j];

				Track->Modify();

				for(int32 k=0; k<Track->GetNumKeyframes(); k++)
				{
					float KeyTime = Track->GetKeyframeTime(k);
					if(KeyTime > MatineeActor->InterpPosition)
					{
						Track->SetKeyframeTime(k, KeyTime + AddTime, false);
					}
				}
			}
		}

		InterpEdTrans->EndSpecial();
	}
}

void FMatinee::OnMenuSelectInSection()
{
	SelectKeysInLoopSection();
}

void FMatinee::OnMenuDuplicateSelectedKeys()
{
	DuplicateSelectedKeys();
}

void FMatinee::OnSavePathTime()
{
	IData->PathBuildTime = MatineeActor->InterpPosition;
}

void FMatinee::OnJumpToPathTime()
{
	SetInterpPosition(IData->PathBuildTime);
}

void FMatinee::OnViewHide3DTracks()
{
	bHide3DTrackView = !bHide3DTrackView;
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("Hide3DTracks"), bHide3DTrackView, GEditorPerProjectIni );
}

bool FMatinee::IsViewHide3DTracksToggled()
{
	return !bHide3DTrackView;
}


void FMatinee::OnViewZoomToScrubPos()
{
	bZoomToScrubPos = !bZoomToScrubPos;
	
	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("ZoomToScrubPos"), bZoomToScrubPos, GEditorPerProjectIni );
}

bool FMatinee::IsViewZoomToScrubPosToggled()
{
	return bZoomToScrubPos;
}

void FMatinee::OnEnableEditingGrid()
{
	bEditingGridEnabled = !bEditingGridEnabled;
	GConfig->SetBool( TEXT("Matinee"), TEXT("EnableEditingGrid"), bEditingGridEnabled, GEditorPerProjectIni );
}

bool FMatinee::IsEnableEditingGridToggled()
{
	return bEditingGridEnabled;
}

void FMatinee::OnSetEditingGrid( uint32 InGridSize )
{
	EditingGridSize = InGridSize;

	GConfig->SetInt( TEXT("Matinee"), TEXT("EditingGridSize"), EditingGridSize, GEditorPerProjectIni );
}

bool FMatinee::IsEditingGridChecked(uint32 InGridSize)
{
	return (EditingGridSize == InGridSize);
}

void FMatinee::OnToggleEditingCrosshair()
{
	bEditingCrosshairEnabled = !bEditingCrosshairEnabled;
	GConfig->SetBool( TEXT("Matinee"), TEXT("EditingCrosshair"), bEditingCrosshairEnabled, GEditorPerProjectIni );
}

bool FMatinee::IsEditingCrosshairToggled()
{
	return bEditingCrosshairEnabled;
}

void FMatinee::OnToggleViewportFrameStats()
{
	bViewportFrameStatsEnabled = !bViewportFrameStatsEnabled;

	// Save to ini when it changes.
	GConfig->SetBool( TEXT("Matinee"), TEXT("ViewportFrameStats"), bViewportFrameStatsEnabled, GEditorPerProjectIni );
}

bool FMatinee::IsViewportFrameStatsToggled()
{
	return bViewportFrameStatsEnabled;
}

/** Called when the "Toggle Gore Preview" button is pressed */
void FMatinee::OnToggleGorePreview()
{
	MatineeActor->bShouldShowGore = !MatineeActor->bShouldShowGore;
}


/** Called when the "Toggle Gore Preview" UI should be updated */
bool FMatinee::IsGorePreviewToggled()
{
	return (MatineeActor && MatineeActor->bShouldShowGore);
}

/** Called when the "Create Camera Actor at Current Camera Location" button is pressed */
void FMatinee::OnCreateCameraActorAtCurrentCameraLocation()
{	
	// no actor to add
	TArray<AActor *> OtherActorsToAddToGroup;
	NewGroupPopup( FMatineeCommands::EGroupAction::NewCameraGroup, NULL, OtherActorsToAddToGroup );
}

/** Called when the "Launch Custom Preview Viewport" is pressed */
void FMatinee::OnLaunchRecordingViewport()
{
	FGlobalTabmanager::Get()->InvokeTab( FName("RecordingViewport") );
}

TSharedRef<SDockTab> FMatinee::SpawnRecordingViewport( const FSpawnTabArgs& Args )
{
	FText Label = NSLOCTEXT( "SMatineeRecorder", "MatineeRecorder", "Matinee Recorder" );

	TSharedPtr< SMatineeRecorder > NewMatineeRecorderWindow;
	TSharedRef< SDockTab > NewTab = SNew( SDockTab )
		.TabRole( ETabRole::MajorTab )
		.Label( Label )
		.ToolTip( IDocumentation::Get()->CreateToolTip( Label, nullptr, "Shared/Matinee", "RecorderTab" ) )
		[
			SAssignNew( NewMatineeRecorderWindow, SMatineeRecorder )
			.MatineeWindow( SharedThis(this) )
		];

	MatineeRecorderWindow = NewMatineeRecorderWindow;
	MatineeRecorderTab = NewTab;
	return NewTab;
}

void FMatinee::OnContextTrackRename()
{
	if( !HasATrackSelected() )
	{
		return;
	}

	for( FSelectedTrackIterator TrackIt(GetSelectedTrackIterator()); TrackIt; ++TrackIt )
	{
		UInterpTrack* Track = *TrackIt;
		const FText TrackTitle = FText::FromString( Track->TrackTitle );
		GetNewNamePopup(LOCTEXT("RenameTrack", "Rename Track"), LOCTEXT("NewTrackName", "New Track Name"), TrackTitle, TrackTitle,
			FOnTextCommitted::CreateSP(this, &FMatinee::OnContextTrackRenameTextCommitted,Track, TrackIt.GetGroup())
			);
		
		break;
	}
}

void FMatinee::OnContextTrackRenameTextCommitted(const FText& InText, ETextCommit::Type, UInterpTrack* Track, UInterpGroup* Group)
{
	//note - no need to check the Commit type, handled by GetNewNamePopup
	InterpEdTrans->BeginSpecial( LOCTEXT( "TrackRename", "Track Rename" ) );

	Track->Modify();
	Track->TrackTitle = InText.ToString();

	// In case this track is being displayed on the curve editor, update its name there too.
	FString CurveName = FString::Printf( TEXT("%s_%s"), *(Group->GroupName.ToString()), *Track->TrackTitle);
	IData->CurveEdSetup->ChangeCurveName(Track, CurveName);
	CurveEd->CurveChanged();		

	InterpEdTrans->EndSpecial();
}

void FMatinee::OnContextTrackDelete()
{
	//stop recording
	StopRecordingInterpValues();

	DeleteSelectedTracks();
}

void FMatinee::OnContextSelectActor( int32 InIndex )
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
	// Find the class of the new track we want to add.
	const int32 NewGroupTrackIndex = InIndex; 
	check( NewGroupTrackIndex >= 0 && NewGroupTrackIndex < MatineeActor->GroupInst.Num() );

	GEditor->SelectNone(true, true, false);
	UInterpGroupInst * GroupInst = MatineeActor->GroupInst[NewGroupTrackIndex];
	if (GroupInst->GetGroupActor())
	{
		GEditor->SelectActor(GroupInst->GetGroupActor(), true, false);
	}
}

void FMatinee::OnContextGotoActors( int32 InIndex )
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
	// Find the class of the new track we want to add.
	const int32 NewGroupTrackIndex = InIndex; 
	check( NewGroupTrackIndex >= 0 && NewGroupTrackIndex < MatineeActor->GroupInst.Num() );

	GEditor->SelectNone(true, true, false);
	UInterpGroupInst * GroupInst = MatineeActor->GroupInst[NewGroupTrackIndex];
	if (GroupInst->GetGroupActor())
	{
		GEditor->SelectActor(GroupInst->GetGroupActor(), true, false);
		GUnrealEd->Exec( MatineeActor->GetWorld(), TEXT( "CAMERA ALIGN" ) );
	}
}

void FMatinee::OnContextReplaceActor( int32 InIndex )
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
	// Find the class of the new track we want to add.
	const int32 NewGroupTrackIndex = InIndex; 
	check( NewGroupTrackIndex >= 0 && NewGroupTrackIndex < MatineeActor->GroupInst.Num() );

	AActor* SelectedActor = NULL;

	for( FSelectionIterator SelectionIt( *GEditor->GetSelectedActors() ); SelectionIt; ++SelectionIt )
	{
		SelectedActor = CastChecked< AActor >( *SelectionIt );
		break;
	}

	if (!SelectedActor)
	{
		return;
	}

	// verify ActorToAdd isn't in the group yet
	if (SelectedActor->IsA(AMatineeActor::StaticClass()))
	{
		UE_LOG(LogSlateMatinee, Warning, TEXT("You can't add Matinee Actor to the group"));
		return;
	}


	UInterpGroupInst * GroupInst = MatineeActor->GroupInst[NewGroupTrackIndex];
	if (GroupInst->GetGroupActor())
	{
		AActor* OldGroupActor = GroupInst->GroupActor;
		GroupInst->RestoreGroupActorState();
		GroupInst->GroupActor = SelectedActor;
		GroupInst->SaveGroupActorState();

		MatineeActor->ReplaceActorGroupInfo(GroupInst->Group, OldGroupActor, SelectedActor);
	}
}

void FMatinee::OnContextRemoveActors( int32 InIndex )
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
	// Find the class of the new track we want to add.
	const int32 NewGroupTrackIndex = InIndex; 
	check( NewGroupTrackIndex >= 0 && NewGroupTrackIndex < MatineeActor->GroupInst.Num() );

	UInterpGroupInst * GroupInst = MatineeActor->GroupInst[NewGroupTrackIndex];
	if (GroupInst->GetGroupActor())
	{
		RemoveActorFromGroup(SelectedGroup, GroupInst->GetGroupActor());
	}
}

void FMatinee::OnContextAddAllActors()
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
	check (MatineeActor);

	// get selected actors
	// Get the set of objects being debugged
	for( FSelectionIterator SelectionIt( *GEditor->GetSelectedActors() ); SelectionIt; ++SelectionIt )
	{
		AActor* Actor = CastChecked< AActor >( *SelectionIt );

		AddActorToGroup(SelectedGroup, Actor);
	}
}

void FMatinee::OnContextSelectAllActors()
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();

	GEditor->SelectNone(false, true, false);
	for (int32 I=0; I<MatineeActor->GroupInst.Num(); ++I)
	{
		UInterpGroupInst * GroupInst = MatineeActor->GroupInst[I];
		if (GroupInst->Group == SelectedGroup)
		{
			GEditor->SelectActor(GroupInst->GetGroupActor(), true, false);
		}
	}
}

void FMatinee::OnContextReplaceAllActors()
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
	// delete all actors, and add all selected actors
	RemoveActorFromGroup(SelectedGroup, NULL);

	// Get the set of objects being debugged
	for( FSelectionIterator SelectionIt( *GEditor->GetSelectedActors() ); SelectionIt; ++SelectionIt )
	{
		AActor* Actor = CastChecked< AActor >( *SelectionIt );

		AddActorToGroup(SelectedGroup, Actor);
	}
}

void FMatinee::OnContextRemoveAllActors()
{
	UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
	check (MatineeActor);

	RemoveActorFromGroup(SelectedGroup, NULL);
}

bool FMatinee::PrepareToAddActorAndWarnUser(AActor* ActorToAdd)
{
	const AMatineeActor::EActorAddWarningType ActorAddWarning = MatineeActor->IsValidActorToAdd( ActorToAdd );
	if( ActorAddWarning != AMatineeActor::ActorAddOk )
	{
		bool Success = false;

		FText Message;

		switch( ActorAddWarning )
		{
		case AMatineeActor::ActorAddWarningSameLevel:
			Message = FText::Format(LOCTEXT("CannotAddActorSameLevelInAMatinee", "Cannot add {0} to matinee {1},\nit has be the same level to avoid cross reference"),
				FText::FromString(ActorToAdd->GetName()), FText::FromString(MatineeActor->GetLevel()->GetName()));
			break;
		case AMatineeActor::ActorAddWarningStatic:
			if ( ActorToAdd->IsA(ABrush::StaticClass()) && !Cast<ABrush>(ActorToAdd)->IsVolumeBrush() )
			{
				Message = FText::Format(LOCTEXT("CannotAddActorStaticInAMatinee", "Cannot add {0} to matinee {1}.  It is static and can not be movable"),
					FText::FromString(ActorToAdd->GetName()), FText::FromString(MatineeActor->GetLevel()->GetName()));
			}
			else
			{
				if ( ActorToAdd->GetRootComponent() )
				{
					ActorToAdd->GetRootComponent()->SetMobility(EComponentMobility::Movable);

					Message = FText::Format(LOCTEXT("ChangingStaticActorToMovableInAMatinee", "Changing {0}'s Mobility to Movable"),
						FText::FromString(ActorToAdd->GetName()));
					Success = true;
				}
			}
			break;
		case AMatineeActor::ActorAddWarningGroup:
			Message = FText::Format(LOCTEXT("CannotAddActorMatineeInAMatinee", "Cannot add {0} to matinee {1}, as it is a Matinee Actor"),
				FText::FromString(ActorToAdd->GetName()), FText::FromString(MatineeActor->GetLevel()->GetName()));
			break;
		}

		FNotificationInfo Info( Message );
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification( Info );

		return Success;
	}

	return true;
}

void FMatinee::AddActorToGroup(UInterpGroup* GroupToAdd, AActor* ActorToAdd)
{
	// create new groupinst
	UInterpGroupInst* NewGroupInst = NULL;

	// verify ActorToAdd isn't in the group yet
	if ( !PrepareToAddActorAndWarnUser(ActorToAdd) )
	{
		return;
	}

	for (int32 I=0; I<MatineeActor->GroupInst.Num(); ++I)
	{
		UInterpGroupInst* GrInst = MatineeActor->GroupInst[I];

		// we have groupinst that don't have GroupActor, assign that
		if (GrInst->Group == GroupToAdd && GrInst->GetGroupActor() == NULL)
		{
			NewGroupInst = GrInst;
			break;
		}

		//don't re-add the same actor
		if (GrInst->Group == GroupToAdd && GrInst->GetGroupActor() == ActorToAdd)
		{
			return;
		}
	}

	if ( NewGroupInst )
	{
		AActor* OldActor = NewGroupInst->GroupActor;
		NewGroupInst->GroupActor = ActorToAdd;
		MatineeActor->ReplaceActorGroupInfo(NewGroupInst->Group, OldActor, ActorToAdd);
		NewGroupInst->InitGroupInst(GroupToAdd, ActorToAdd);
	}
	else
	{
		NewGroupInst = NewObject<UInterpGroupInst>(MatineeActor, NAME_None, RF_Transactional);
		// Instantiate the Matinee group data structure.
		MatineeActor->GroupInst.Add(NewGroupInst);
		NewGroupInst->InitGroupInst(GroupToAdd, ActorToAdd);
		MatineeActor->InitGroupActorForGroup( GroupToAdd, ActorToAdd );
	}

	NewGroupInst->SaveGroupActorState();
	MatineeActor->ConditionallySaveActorState(NewGroupInst, ActorToAdd);
}

/** If ActorToRemove == NULL, it will remove all **/
void FMatinee::RemoveActorFromGroup(UInterpGroup* GroupToRemove, AActor* ActorToRemove)
{
	bool DefaultGroupInstExists = false;

	// you can remove if this isn't the last Inst of this Group
	// we assume one group will have AT LEAST ONE GROUPINST
	// so we can't remove the last one
	for (int32 I=0; I<MatineeActor->GroupInst.Num(); ++I)
	{
		UInterpGroupInst* GrInst = MatineeActor->GroupInst[I];

		if (GrInst->Group == GroupToRemove
			// if actor == NULL or groupActor is 
			&& (!ActorToRemove || GrInst->GroupActor == ActorToRemove) )
		{
			// Restore Actors to the state they were in when we opened Matinee.
			GrInst->RestoreGroupActorState();
			
			if (DefaultGroupInstExists)
			{
				MatineeActor->DeleteActorGroupInfo(GroupToRemove, GrInst->GroupActor);

				// if delete extra groupinst
				GrInst->TermGroupInst(false);
				// remove inst
				MatineeActor->GroupInst.RemoveAt(I);
				--I;
			}
			else
			{
				MatineeActor->ReplaceActorGroupInfo(GroupToRemove, GrInst->GroupActor, NULL);
				// make sure default one exists, we don't delete default one
				DefaultGroupInstExists = true;
				GrInst->GroupActor = NULL;
			}
		}
	}
}
/**
 * Toggles visibility of the trajectory for the selected movement track
 */
void FMatinee::OnContextTrackShow3DTrajectory()
{
	check( HasATrackSelected() );

	// Check to make sure there is a movement track in list
	// before attempting to start the transaction system.
	if( HasATrackSelected( UInterpTrackMove::StaticClass() ) )
	{
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Undo_ToggleTrajectory", "Toggle 3D Trajectory for Track") );

		TTrackClassTypeIterator<UInterpTrackMove> MoveTrackIt(GetSelectedTrackIterator<UInterpTrackMove>());
		for( ; MoveTrackIt; ++MoveTrackIt )
		{
			// Grab the movement track for the selected group
			UInterpTrackMove* MoveTrack = *MoveTrackIt;
			MoveTrack->Modify();

			MoveTrack->bHide3DTrack = !MoveTrack->bHide3DTrack;
		}

		InterpEdTrans->EndSpecial();
	}
}


/**
 * Exports the animations in the selected track to FBX
 */
void FMatinee::OnContextTrackExportAnimFBX()
{
	// Check to make sure there is an animation track in list
	// before attempting to start the transaction system.
	if( HasATrackSelected( UInterpTrackAnimControl::StaticClass() ) )
	{
		TArray<UInterpTrack*> SelectedTracks;
		GetSelectedTracks(SelectedTracks);

		check( SelectedTracks.Num() == 1 );

		UInterpTrack* SelectedTrack = SelectedTracks[0];

		// Make sure the track is an animation track
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(SelectedTrack);

		// Find the skeletal mesh for this anim track
		USkeletalMesh* SkelMesh = NULL;
		
		// Get the owning group of the track
		UInterpGroup* Group = CastChecked<UInterpGroup>( SelectedTrack->GetOuter() ) ;

		ASkeletalMeshActor* SkelMeshActor = NULL;

		if ( MatineeActor != NULL )
		{
			// Get the first group instance for this track.  In the case of animations there is only one instance usually
			UInterpGroupInst* GroupInst = MatineeActor->FindFirstGroupInst(Group);

			// Get the actor for this group.
			SkelMeshActor = Cast<ASkeletalMeshActor>(GroupInst->GroupActor);
		}

		// Someone could have hooked up an invalid actor.  In that case do nothing
		if( SkelMeshActor )
		{
			SkelMesh = SkelMeshActor->GetSkeletalMeshComponent()->SkeletalMesh;
		}
		

		// If this is a valid anim track and it has a valid skeletal mesh
		if(AnimTrack && SkelMesh)
		{
			if( MatineeActor != NULL )
			{
				TArray<FString> SaveFilenames;
				IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
				bool bSaved = false;
				if (DesktopPlatform != NULL)
				{
					bSaved = DesktopPlatform->SaveFileDialog(
						GetMatineeDialogParentWindow(), 
						NSLOCTEXT("UnrealEd", "ExportMatineeAnimTrack", "Export UnrealMatinee Animation Track").ToString(), 
						*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)), 
						TEXT(""), 
						TEXT("FBX document|*.fbx"), 
						EFileDialogFlags::None, 
						SaveFilenames);
				}
				if (bSaved)
				{
					FString ExportFilename = SaveFilenames[0];
					FString FileName = SaveFilenames[0];
					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, FPaths::GetPath(FileName)); // Save path as default for next time.

					UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();

					//Show the fbx export dialog options
					bool ExportCancel = false;
					bool ExportAll = false;
					Exporter->FillExportOptions(false, true, ExportFilename, ExportCancel, ExportAll);
					if (!ExportCancel)
					{
						// Export the Matinee information to a COLLADA document.
						Exporter->CreateDocument();
						Exporter->SetTrasformBaking(bBakeTransforms);
						const bool bKeepHierarchy = GetDefault<UEditorPerProjectUserSettings>()->bKeepAttachHierarchy;
						Exporter->SetKeepHierarchy(bKeepHierarchy);

						// Export the anim sequences
						TArray<UAnimSequence*> AnimSequences;
						for (int32 TrackKeyIndex = 0; TrackKeyIndex < AnimTrack->AnimSeqs.Num(); ++TrackKeyIndex)
						{
							UAnimSequence* AnimSeq = AnimTrack->AnimSeqs[TrackKeyIndex].AnimSeq;

							if (AnimSeq)
							{
								AnimSequences.Push(AnimSeq);
							}
							else
							{
								UE_LOG(LogSlateMatinee, Warning, TEXT("Warning: Animation not found when exporting %s"), *AnimTrack->GetName());
							}
						}

						GWarn->BeginSlowTask(LOCTEXT("BeginExportingAnimationsTask", "Exporting Animations"), true);
						FString ExportName = Group->GroupName.ToString() + TEXT("_") + AnimTrack->GetName();
						Exporter->ExportAnimSequencesAsSingle(SkelMesh, SkelMeshActor, ExportName, AnimSequences, AnimTrack->AnimSeqs);
						GWarn->EndSlowTask();
						// Save to disk
						Exporter->WriteToFile(*ExportFilename);
					}
				}
			}
		}
	}
}

/**
 * Shows or hides all movement track trajectories in the Matinee sequence
 */
void FMatinee::OnViewShowOrHideAll3DTrajectories( bool bShow )
{
	// Are we showing or hiding track trajectories?
	const bool bShouldHideTrajectories = !bShow;

	bool bAnyTracksModified = false;

	// Iterate over each group
	for( int32 CurGroupIndex = 0; CurGroupIndex < IData->InterpGroups.Num(); ++CurGroupIndex	)
	{
		UInterpGroup* CurGroup = IData->InterpGroups[ CurGroupIndex ];
		check( CurGroup != NULL );

		// Iterate over tracks in this group
		for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
		{
			UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];
			check( CurTrack != NULL );

			// Is this a movement track?  Only movement tracks have trajectories
			UInterpTrackMove* MovementTrack = Cast<UInterpTrackMove>( CurTrack );
			if( MovementTrack != NULL )
			{
				if( bShouldHideTrajectories != MovementTrack->bHide3DTrack )
				{
					// Begin our undo transaction if we haven't started on already
					if( !bAnyTracksModified )
					{
						InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Undo_ShowOrHideAllTrajectories", "Show or Hide All Trajectories") );
						bAnyTracksModified = true;
					}

					// Show or hide the trajectory for this movement track
					MovementTrack->Modify();
					MovementTrack->bHide3DTrack = bShouldHideTrajectories;
				}
			}
		}
	}

	// End our undo transaction, but only if we actually modified something
	if( bAnyTracksModified )
	{
		InterpEdTrans->EndSpecial();
	}
}



/** Toggles 'capture mode' for particle replay tracks */
void FMatinee::OnParticleReplayTrackContext_ToggleCapture( bool bInEnableCapture )
{
	check( HasATrackSelected() );

	const bool bEnableCapture = bInEnableCapture; 

	TTrackClassTypeIterator<UInterpTrackParticleReplay> ReplayTrackIt(GetSelectedTrackIterator<UInterpTrackParticleReplay>());
	for( ; ReplayTrackIt; ++ReplayTrackIt )
	{
		UInterpTrackParticleReplay* ParticleReplayTrack = *ReplayTrackIt;

		// Toggle capture mode
		ParticleReplayTrack->bIsCapturingReplay = bEnableCapture;

		// Dirty the track window viewports
		InvalidateTrackWindowViewports();
	}
}

void FMatinee::OnContextGroupRename()
{
	if( !HasAGroupSelected() )
	{
		return;
	}

	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* GroupToRename = *GroupIt;
		const FText OriginalName = FText::FromName( GroupToRename->GroupName );

		bool bValidName = false;
		bool bUserCancelled = false;

		FText DialogName = GroupToRename->bIsFolder ? LOCTEXT("MatineeRenameFolder", "Rename Folder") : LOCTEXT("MatineeRenameGroup", "Rename Group");
		FText PromptName = GroupToRename->bIsFolder ? LOCTEXT("MatineeNewFolderName", "New Folder Name") : LOCTEXT("MatineeNewGroupName", "New Group Name");
		
		GetNewNamePopup( DialogName, PromptName, OriginalName, OriginalName,
			FOnTextCommitted::CreateSP(this, &FMatinee::OnContextGroupRenameCommitted, GroupToRename) 
			);

		break;
	}
}

void FMatinee::OnContextGroupRenameCommitted(const FText& InText, ETextCommit::Type, UInterpGroup* GroupToRename)
{
	//no need to check ETextCommit, handled by GetNewNamePopup

	FName NewName = *InText.ToString().Left(NAME_SIZE);
	
	bool bValidName = true;

	// Check this name does not already exist.
	for(int32 i=0; i<IData->InterpGroups.Num() && bValidName; i++)
	{
		if(IData->InterpGroups[i]->GroupName == NewName)
		{
			bValidName = false;
		}
	}

	if(!bValidName)
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_NameAlreadyExists", "Name already exists - please choose a unique name for this Group.") );
		return;
	}
		
	InterpEdTrans->BeginSpecial( LOCTEXT( "GroupRename", "Group Rename" ) );

	// Update any camera cuts to point to new group name
	UInterpGroupDirector* DirGroup = IData->FindDirectorGroup();
	if(DirGroup)
	{
		UInterpTrackDirector* DirTrack = DirGroup->GetDirectorTrack();
		if(DirTrack)
		{
			DirTrack->Modify();
			for(int32 i=0; i<DirTrack->CutTrack.Num(); i++)
			{
				FDirectorTrackCut& Cut = DirTrack->CutTrack[i];
				if(Cut.TargetCamGroup == GroupToRename->GroupName)
				{
					Cut.TargetCamGroup = NewName;
				}	
			}
		}
	}
	// Change the name of the InterpGroup.
	GroupToRename->Modify();
	GroupToRename->GroupName = NewName;

	InterpEdTrans->EndSpecial();
}

void FMatinee::OnContextGroupDelete()
{
	//stop recording
	StopRecordingInterpValues();

	// Must have at least one group selected for this function.
	check( HasAGroupSelected() );

	for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
	{
		UInterpGroup* Group = *GroupIt;

		if( Group->bIsFolder )
		{
			// Check we REALLY want to do this.
			bool bDoDestroy = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, FText::Format(
				NSLOCTEXT("UnrealEd", "InterpEd_DeleteSelectedFolder", "Are you sure you want to delete folder ({0})?  Any groups that are attached to this folder will be detached first (but not deleted)!" ),
				FText::FromName(Group->GroupName)) );

			// The user backed out of deleting this group. 
			// Deselect the group so it doesn't get deleted.
			if(!bDoDestroy)
			{
				DeselectGroup(Group, false);
			}
		}
	}

	if (HasAGroupSelected())
	{
		DeleteSelectedGroups();
	}
}

bool FMatinee::CanGroupDelete() const
{
	return !IsCameraAnim();
}

/** Prompts the user for a name for a new filter and creates a custom filter. */
void FMatinee::OnContextGroupCreateTab()
{
	// Display dialog and let user enter new time.
	FString TabName;

	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "GroupTabName", "Group Tab Name"), 
		FText::GetEmpty(),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnContextGroupCreateTabTextCommitted)
		);
}

bool FMatinee::CanGroupCreateTab() const
{
	return !IsCameraAnim();
}

void FMatinee::OnContextGroupCreateTabTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	CloseEntryPopupMenu();	
	if (CommitInfo == ETextCommit::OnEnter)
	{
		// Create a new tab.
		if( HasAGroupSelected() )
		{
			UInterpFilter_Custom* Filter = NewObject<UInterpFilter_Custom>(IData, NAME_None, RF_Transactional);

			if(!InText.IsEmpty())
			{
				Filter->Caption = InText.ToString();
			}
			else
			{
				Filter->Caption = Filter->GetName();
			}
	
			TArray<UInterpGroup*> SelectedGroups;
			GetSelectedGroups(SelectedGroups);
			Filter->GroupsToInclude.Append(SelectedGroups);

			IData->InterpFilters.Add(Filter);

			// Update the UI
			GroupFilterContainer->SetContent(BuildGroupFilterToolbar());
		}
	}
}

/** Sends the selected group to the tab the user specified.  */
void FMatinee::OnContextGroupSendToTab( int32 InIndex )
{
	int32 TabIndex = InIndex; 

	if(TabIndex >=0 && TabIndex < IData->InterpFilters.Num())
	{
		// Make sure the active group isnt already in the filter's set of groups.
		UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(IData->InterpFilters[TabIndex]);

		if(Filter != NULL)
		{
			for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
			{
				// Only add move the selected group to the tab if it's not already in the tab.
				if( !Filter->GroupsToInclude.Contains(*GroupIt) )
				{
					Filter->GroupsToInclude.Add(*GroupIt);
				}
			}
		}
	}
}

/** Removes the group from the current tab.  */
void FMatinee::OnContextGroupRemoveFromTab()
{
	// Make sure the active group exists in the selected filter and that the selected filter isn't a default filter.
	UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(IData->SelectedFilter);

	bool bInvalidateViewports = false;

	if(Filter != NULL && IData->InterpFilters.Contains(Filter) == true)
	{
		for( FSelectedGroupIterator GroupIt(GetSelectedGroupIterator()); GroupIt; ++GroupIt )
		{
			UInterpGroup* Group = *GroupIt;

			if( Filter->GroupsToInclude.Contains(Group) )
			{
				Filter->GroupsToInclude.Remove(Group);
				Group->bVisible = false;

				bInvalidateViewports = true;
			}
		}

		if( bInvalidateViewports )
		{
			// Dirty the track window viewports
			InvalidateTrackWindowViewports();
		}
	}
}

/** Exports all the animations in the group as a single FBX file.  */
void FMatinee::OnContextGroupExportAnimFBX()
{
	// Check to make sure there is an animation track in list
	// before attempting to start the transaction system.
	if( HasAGroupSelected() )
	{
		TArray<UInterpGroup*> SelectedGroups;
		GetSelectedGroups(SelectedGroups);

		if( SelectedGroups.Num() == 1 )
		{
			UInterpGroup* SelectedGroup = SelectedGroups[0];

			// Only export this group if it has at least one animation track
			if( SelectedGroup->HasAnimControlTrack() )
			{
				// Find the skeletal mesh for this group
				USkeletalMeshComponent* SkelMeshComponent = NULL;

				if (MatineeActor != NULL)
				{
					// Get the first group instance for this group.  In the case of animations there is usually only one instance
					UInterpGroupInst* GroupInst = MatineeActor->FindFirstGroupInst( SelectedGroup );

					// Get the actor for this group.
					ASkeletalMeshActor* SkelMeshActor = Cast<ASkeletalMeshActor>( GroupInst->GroupActor );

					// Someone could have hooked up an invalid actor.  In that case do nothing
					if( SkelMeshActor )
					{
						SkelMeshComponent = SkelMeshActor->GetSkeletalMeshComponent();
					}
				}

				// If this is a valid skeletal mesh
				if(SkelMeshComponent)
				{
					if( MatineeActor != NULL )
					{
						TArray<FString> SaveFilenames;
						IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
						bool bSaved = false;
						if (DesktopPlatform != NULL)
						{
							bSaved = DesktopPlatform->SaveFileDialog(
								GetMatineeDialogParentWindow(), 
								NSLOCTEXT("UnrealEd", "ExportMatineeAnimTrack", "Export UnrealMatinee Animation Track").ToString(), 
								*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)), 
								TEXT(""), 
								TEXT("FBX document|*.fbx"),
								EFileDialogFlags::None, 
								SaveFilenames);
						}


						// Show dialog and execute the import if the user did not cancel out
						if( bSaved )
						{
							// Get the filename from dialog
							FString ExportFilename = SaveFilenames[0];
							FString FileName = SaveFilenames[0];
							FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, FPaths::GetPath(FileName)); // Save path as default for next time.

							UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
							
							//Show the fbx export dialog options
							bool ExportCancel = false;
							bool ExportAll = false;
							Exporter->FillExportOptions(false, true, ExportFilename, ExportCancel, ExportAll);
							if (!ExportCancel)
							{
								FFormatNamedArguments Args;
								Args.Add(TEXT("MatineeGroupName"), FText::FromString(SelectedGroup->GetName()));
								GWarn->BeginSlowTask(FText::Format(LOCTEXT("BeginExportingMatineeGroupTask", "Exporting {MatineeGroupName}"), Args), true);
								// Export the Matinee information to an FBX document.
								Exporter->CreateDocument();
								Exporter->SetTrasformBaking(bBakeTransforms);
								const bool bKeepHierarchy = GetDefault<UEditorPerProjectUserSettings>()->bKeepAttachHierarchy;
								Exporter->SetKeepHierarchy(bKeepHierarchy);

								// Export the animation sequences in the group by sampling the skeletal mesh over the
								// duration of the matinee sequence
								Exporter->ExportMatineeGroup(MatineeActor, SkelMeshComponent);

								// Save to disk
								Exporter->WriteToFile(*ExportFilename);

								GWarn->EndSlowTask();
							}

						}
					}
				}
			}
		}
	}
}

/** Deletes the currently selected group tab.  */
void FMatinee::OnContextDeleteGroupTab()
{
	// Make sure the active group exists in the selected filter and that the selected filter isn't a default filter.
	UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(IData->SelectedFilter);

	if(Filter != NULL)
	{
		IData->InterpFilters.Remove(Filter);

		// Set the selected filter back to the all filter.
		if(IData->DefaultFilters.Num())
		{
			SetSelectedFilter(IData->DefaultFilters[0]);
		}
		else
		{
			SetSelectedFilter(NULL);
		}

		// Update the UI
		GroupFilterContainer->SetContent(BuildGroupFilterToolbar());
	}
}



/** Called when the user selects to move a group to another group folder */
void FMatinee::OnContextGroupChangeGroupFolder( FMatineeCommands::EGroupAction::Type InActionId, int32 InIndex )
{
	// To invoke this command, there must be at least one group selected.
	check( HasAGroupSelected() );

	// Figure out if we're moving the active group to a new group, or if we simply want to unparent it
	const bool bIsParenting =  ( InActionId != FMatineeCommands::EGroupAction::RemoveFromGroupFolder ); 

	// Figure out which direction we're moving things: A group to the selected folder?  Or, the selected group
	// to a folder?
	bool bIsMovingSelectedGroupToFolder = false;
	bool bIsMovingGroupToSelectedFolder = false;

	if( bIsParenting )
	{
		bIsMovingSelectedGroupToFolder =
			(InActionId == FMatineeCommands::EGroupAction::MoveActiveGroupToFolder);
		bIsMovingGroupToSelectedFolder = !bIsMovingSelectedGroupToFolder;
	}

	// Store the source group to the destination group index.
	TMap<UInterpGroup*, UInterpGroup*> SourceGroupToDestGroup;

	for( FSelectedGroupIterator GroupIter(GetSelectedGroupIterator()); GroupIter; ++GroupIter )
	{
		UInterpGroup* SelectedGroup = *GroupIter;

		// Make sure we're dealing with a valid group index
		int32 MenuGroupIndex = INDEX_NONE;

		if( bIsParenting )
		{
			MenuGroupIndex = InIndex;
		}
		else
		{
			// If we're unparenting, then use ourselves as the destination index
			MenuGroupIndex = GroupIter.GetGroupIndex();

			// Make sure we're not already in the desired state; this would be a UI error
			check( SelectedGroup->bIsParented );
		}

		bool bIsValidGroupIndex = IData->InterpGroups.IsValidIndex( MenuGroupIndex );
		if( !bIsValidGroupIndex )
		{
			continue;
		}


		// Figure out what our source and destination groups are for this operation
		if( !bIsParenting || bIsMovingSelectedGroupToFolder )
		{
			// We're moving the selected group to a group, or unparenting a group
			SourceGroupToDestGroup.Add( SelectedGroup, IData->InterpGroups[ MenuGroupIndex ] );
		}
		else
		{
			// We're moving a group to our selected group
			SourceGroupToDestGroup.Add( IData->InterpGroups[MenuGroupIndex], IData->InterpGroups[ GroupIter.GetGroupIndex() ]);
		}
	}



	// OK, to pull this off we need to do two things.  First, we need to relocate the source group such that
	// it's at the bottom of the destination group's children in our list.  Then, we'll need to mark the
	// group as 'parented'!

	// We're about to modify stuff!
	InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_ChangeGroupFolder", "Change Group Folder") );
	MatineeActor->Modify();
	IData->Modify();

	for( TMap<UInterpGroup*,UInterpGroup*>::TIterator SrcToDestMap(SourceGroupToDestGroup); SrcToDestMap; ++SrcToDestMap )
	{
		UInterpGroup* SourceGroup = SrcToDestMap.Key();
		UInterpGroup* DestGroup = SrcToDestMap.Value();

		//if they are not the same and they are both folders, skip.  No folder on folder operations
		if (SourceGroup->bIsFolder && DestGroup->bIsFolder)
		{
			continue;
		}

		// First, remove ourselves from the group list
		{
			int32 SourceGroupIndex = IData->InterpGroups.Find( SourceGroup );
			IData->InterpGroups.RemoveAt( SourceGroupIndex );
		}
		int32 DestGroupIndex = IData->InterpGroups.Find( DestGroup );

		int32 TargetGroupIndex = DestGroupIndex + 1;
		for( int32 OtherGroupIndex = TargetGroupIndex; OtherGroupIndex < IData->InterpGroups.Num(); ++OtherGroupIndex )
		{
			UInterpGroup* OtherGroup = IData->InterpGroups[ OtherGroupIndex ];

			// Is this group parented?
			if( OtherGroup->bIsParented )
			{
				// OK, this is a child group of the destination group.  We want to append our new group to the end of
				// the destination group's list of children, so we'll just keep on iterating.
				++TargetGroupIndex;
			}
			else
			{
				// This group isn't the destination group or a child of the destination group.  We now have the index
				// we're looking for!
				break;
			}
		}

		// OK, now we know where we need to place the source group to in the list.  Let's do it!
		IData->InterpGroups.Insert( SourceGroup, TargetGroupIndex );

		// OK, now mark the group as parented!  Note that if we're relocating a group from one folder to another, it
		// may already be tagged as parented.
		if( SourceGroup->bIsParented != bIsParenting )
		{
			SourceGroup->Modify();
			SourceGroup->bIsParented = bIsParenting;
		}
	}

	// Complete undo state
	InterpEdTrans->EndSpecial();

	// Dirty the track window viewports
	InvalidateTrackWindowViewports();
}



// Iterate over keys changing their interpolation mode and adjusting tangents appropriately.
void FMatinee::OnContextKeyInterpMode( FMatineeCommands::EKeyAction::Type InActionId )
{

	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];
		UInterpTrack* Track = SelKey.Track;

		if (InActionId == FMatineeCommands::EKeyAction::KeyModeLinear) 
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_Linear );
		}
		else if (InActionId == FMatineeCommands::EKeyAction::KeyModeCurveAuto) 
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveAuto );
		}
		else if (InActionId == FMatineeCommands::EKeyAction::KeyModeCurveAutoClamped) 
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveAutoClamped );
		}
		else if(InActionId == FMatineeCommands::EKeyAction::KeyModeCurveBreak) 
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_CurveBreak );
		}
		else if(InActionId == FMatineeCommands::EKeyAction::KeyModeConstant) 
		{
			Track->SetKeyInterpMode( SelKey.KeyIndex, CIM_Constant );
		}
	}

	CurveEd->RefreshViewport();
}

/** Pops up menu and lets you set the time for the selected key. */
void FMatinee::OnContextSetKeyTime()
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;

	// Display dialog and let user enter new time.
	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "SetKeyTime", "New Time"),
		FText::AsNumber( Track->GetKeyframeTime(SelKey.KeyIndex) ),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnContextSetKeyTimeTextCommitted, &SelKey, Track)
		);
}

void FMatinee::OnContextSetKeyTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, UInterpTrack* Track)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double dNewTime = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric(); 
		if(!bIsNumber)
			return;

		const float NewKeyTime = (float)dNewTime;

		// Save off the original key index to check if a movement 
		// track needs its initial transform updated. 
		const int32 OldKeyIndex = SelKey->KeyIndex;

		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "SetTime", "Set Time") );
		Track->Modify();

		// Move the key. Also update selected to reflect new key index.
		SelKey->KeyIndex = Track->SetKeyframeTime( SelKey->KeyIndex, NewKeyTime );

		InterpEdTrans->EndSpecial();

		// Update positions at current time but with new keyframe times.
		RefreshInterpPosition();
		CurveEd->RefreshViewport();
	}
}

/** Pops up a menu and lets you set the value for the selected key. Not all track types are supported. */
void FMatinee::OnContextSetValue()
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;

	// If its a float track - pop up text entry dialog.
	UInterpTrackFloatBase* FloatTrack = Cast<UInterpTrackFloatBase>(Track);
	if(FloatTrack)
	{
		// Get current float value of the key
		GenericTextEntryModeless(
			NSLOCTEXT("Matinee.Popups", "NewValue", "New Value"),
			FText::AsNumber( FloatTrack->FloatTrack.Points[SelKey.KeyIndex].OutVal ),
			FOnTextCommitted::CreateSP(this, &FMatinee::OnContextSetValueTextCommitted, &SelKey, FloatTrack)
			);
	}

	// Update positions at current time but with new keyframe times.
	RefreshInterpPosition();
	CurveEd->RefreshViewport(); 
}

void FMatinee::OnContextSetValueTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, UInterpTrackFloatBase* FloatTrack)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double dNewVal = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if(!bIsNumber)
			return;

		// Set new value, and update tangents.
		const float NewVal = (float)dNewVal;
		FloatTrack->FloatTrack.Points[SelKey->KeyIndex].OutVal = NewVal;
		FloatTrack->FloatTrack.AutoSetTangents(FloatTrack->CurveTension);

		// Update positions at current time but with new keyframe times.
		RefreshInterpPosition();
		CurveEd->RefreshViewport(); 
	}
}

/** Pops up a menu and lets you set the color for the selected key. Not all track types are supported. */
void FMatinee::OnContextSetColor()
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;

	UInterpTrackVectorMaterialParam* VectorMaterialParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
	if(VectorMaterialParamTrack)
	{
		FVector CurrentColorVector = VectorMaterialParamTrack->VectorTrack.Points[SelKey.KeyIndex].OutVal;
		static FLinearColor CurrentColor;

		CurrentColor = FLinearColor(CurrentColorVector.X, CurrentColorVector.Y, CurrentColorVector.Z);

		TArray<FLinearColor*> LinearColorArray;
		LinearColorArray.Add(&CurrentColor);

		FColorPickerArgs PickerArgs;
		PickerArgs.bOnlyRefreshOnMouseUp = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.LinearColorArray = &LinearColorArray;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateRaw( this, &FMatinee::OnUpdateFromColorSelection);

		OpenColorPicker(PickerArgs);

		return;
	}

	// If its a color prop track - pop up color dialog.
	UInterpTrackColorProp* ColorPropTrack = Cast<UInterpTrackColorProp>(Track);
	if(ColorPropTrack)
	{
		FVector CurrentColorVector = ColorPropTrack->VectorTrack.Points[SelKey.KeyIndex].OutVal;
		static FColor CurrentColor;

		CurrentColor = FLinearColor(CurrentColorVector.X, CurrentColorVector.Y, CurrentColorVector.Z).ToFColor(true);

		TArray<FColor*> FColorArray;
		FColorArray.Add(&CurrentColor);

		FColorPickerArgs PickerArgs;
		PickerArgs.bOnlyRefreshOnMouseUp = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.ColorArray = &FColorArray;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateRaw( this, &FMatinee::OnUpdateFromColorSelection);

		OpenColorPicker(PickerArgs);

		return;
	}

	// We also support linear color tracks!
	UInterpTrackLinearColorProp* LinearColorPropTrack = Cast<UInterpTrackLinearColorProp>(Track);
	if(LinearColorPropTrack)
	{
		static FLinearColor CurrentColor;
		
		CurrentColor = LinearColorPropTrack->LinearColorTrack.Points[SelKey.KeyIndex].OutVal;
		TArray<FLinearColor*> FLinearColorArray;
		FLinearColorArray.Add(&CurrentColor);

		FColorPickerArgs PickerArgs;
		PickerArgs.bOnlyRefreshOnMouseUp = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.LinearColorArray = &FLinearColorArray;
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateRaw( this, &FMatinee::OnUpdateFromColorSelection);

		OpenColorPicker(PickerArgs);

		return;
	}
}

/** 
 * Called during color selection to updat tracks and refresh realtime viewports 
 *
 * @param	NewColor	The newly selected color
 */
void FMatinee::OnUpdateFromColorSelection(FLinearColor NewColor)
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		// Get the time the selected key is currently at.
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackVectorMaterialParam* VectorMaterialParamTrack = Cast<UInterpTrackVectorMaterialParam>(Track);
		if(VectorMaterialParamTrack)
		{
			FLinearColor VectorColor(NewColor);

			VectorMaterialParamTrack->VectorTrack.Points[SelKey.KeyIndex].OutVal = FVector(VectorColor.R, VectorColor.G, VectorColor.B);
			VectorMaterialParamTrack->VectorTrack.AutoSetTangents(VectorMaterialParamTrack->CurveTension);
		}

		//Update the vector color track
		UInterpTrackColorProp* ColorPropTrack = Cast<UInterpTrackColorProp>(Track);
		if(ColorPropTrack)
		{

			FLinearColor VectorColor(NewColor);

			ColorPropTrack->VectorTrack.Points[SelKey.KeyIndex].OutVal = FVector(VectorColor.R, VectorColor.G, VectorColor.B);
			ColorPropTrack->VectorTrack.AutoSetTangents(ColorPropTrack->CurveTension);
		}

		//Update the linear color track
		UInterpTrackLinearColorProp* LinearColorPropTrack = Cast<UInterpTrackLinearColorProp>(Track);
		if(LinearColorPropTrack)
		{
			LinearColorPropTrack->LinearColorTrack.Points[SelKey.KeyIndex].OutVal = NewColor;
			LinearColorPropTrack->LinearColorTrack.AutoSetTangents(LinearColorPropTrack->CurveTension);	
		}

		//Update our tracks to display the new color in the viewports
		RefreshInterpPosition();
		CurveEd->RefreshViewport();
	}
}

/**
 * Flips the value of the selected key for a boolean property.
 *
 * @note	Assumes that the user was only given the option of flipping the 
 *			value in the context menu (i.e. true -> false or false -> true).
 *
 */
void FMatinee::OnContextSetBool()
{
	// Only works if one key is selected.
	if( Opt->SelectedKeys.Num() != 1 )
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelectedKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelectedKey.Track;

	// Can't flip the boolean unless the owning track is a boolean property track. 
	if( Track->IsA(UInterpTrackBoolProp::StaticClass()) )
	{
		UInterpTrackBoolProp* BoolPropTrack = CastChecked<UInterpTrackBoolProp>(Track);
		FBoolTrackKey& Key = BoolPropTrack->BoolTrack[SelectedKey.KeyIndex];

		// Flip the value.
		Key.Value = !Key.Value;
	}
}


/** Pops up menu and lets the user set a group to use to lookup transform info for a movement keyframe. */
void FMatinee::OnSetMoveKeyLookupGroup()
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;

	// Only perform work if we are on a movement track.
	UInterpTrackMoveAxis* MoveTrackAxis = NULL;
	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
	if(!MoveTrack)
	{
		MoveTrackAxis = Cast<UInterpTrackMoveAxis>( Track );
	}

	if( MoveTrack || MoveTrackAxis )
	{
		// Make array of group names
		TArray<FString> GroupNames;
		for ( int32 GroupIdx = 0; GroupIdx < IData->InterpGroups.Num(); GroupIdx++ )
		{
			// Skip folder groups
			if( !IData->InterpGroups[ GroupIdx ]->bIsFolder )
			{
				if(IData->InterpGroups[GroupIdx] != SelKey.Group)
				{
					GroupNames.Add( *(IData->InterpGroups[GroupIdx]->GroupName.ToString()) );
				}
			}
		}

		TSharedRef<STextComboPopup> TextEntryPopup = 
			SNew(STextComboPopup)
			.Label(NSLOCTEXT("Matinee.Popups", "SelectGroup", "Select Group"))
			.TextOptions(GroupNames)
			.OnTextChosen(this, &FMatinee::OnSetMoveKeyLookupGroupTextChosen, SelKey.KeyIndex, MoveTrack, MoveTrackAxis)
			;

		EntryPopupMenu = FSlateApplication::Get().PushMenu(
			ToolkitHost.Pin()->GetParentWidget(),
			FWidgetPath(),
			TextEntryPopup,
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
			);
	}
}

void FMatinee::OnSetMoveKeyLookupGroupTextChosen(const FString& ChosenText, int32 KeyIndex, UInterpTrackMove* MoveTrack, UInterpTrackMoveAxis* MoveTrackAxis)
{
	FName KeyframeLookupGroup = FName( *ChosenText );
	if( MoveTrack )
	{
		MoveTrack->SetLookupKeyGroupName(KeyIndex, KeyframeLookupGroup);
	}
	else
	{
		MoveTrackAxis->SetLookupKeyGroupName( KeyIndex, KeyframeLookupGroup );
	}
	CloseEntryPopupMenu();
}



/** Clears the lookup group for a currently selected movement key. */
void FMatinee::OnClearMoveKeyLookupGroup()
{
	// Only works if one key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Get the time the selected key is currently at.
	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;

	// Only perform work if we are on a movement track.
	UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(Track);
	if(MoveTrack)
	{
		MoveTrack->ClearLookupKeyGroupName(SelKey.KeyIndex);
	}
	else
	{
		UInterpTrackMoveAxis* MoveTrackAxis = Cast<UInterpTrackMoveAxis>(Track);
		if( MoveTrackAxis )
		{
			MoveTrackAxis->ClearLookupKeyGroupName( SelKey.KeyIndex );
		}
	}
}


/** Rename an event. Handle removing/adding connectors as appropriate. */
void FMatinee::OnContextRenameEventKey()
{
	// Only works if one Event key is selected.
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	// Find the EventNames of selected key
	FName EventNameToChange;
	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>(Track);
	if(EventTrack)
	{
		EventNameToChange = EventTrack->EventTrack[SelKey.KeyIndex].EventName; 
	}
	else
	{
		return;
	}

	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "NewEventName", "New Event Name"),
		FText::FromName( EventNameToChange ),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnContextRenameEventKeyTextCommitted, EventNameToChange)
		);
}

void FMatinee::OnContextRenameEventKeyTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FName EventNameToChange)
{ 
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		FString TempString = InText.ToString().Left(NAME_SIZE);
		TempString = TempString.Replace(TEXT(" "),TEXT("_"));
		FName NewEventName = FName( *TempString );

		// If this Event name is already in use- disallow it
		if( IData->IsEventName( NewEventName ) )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_EventNameInUse", "Sorry - Event name already in use.") );
			return;
		}
	
		InterpEdTrans->BeginSpecial( LOCTEXT( "EventRename", "Event Rename" ) );

		MatineeActor->Modify();
		IData->Modify();

		// Then go through all keys, changing those with this name to the new one.
		for(int32 i=0; i<IData->InterpGroups.Num(); i++)
		{
			UInterpGroup* Group = IData->InterpGroups[i];
			for(int32 j=0; j<Group->InterpTracks.Num(); j++)
			{
				UInterpTrackEvent* EventTrack = Cast<UInterpTrackEvent>( Group->InterpTracks[j] );
				if(EventTrack)
				{
					bool bModified = false;
					for(int32 k=0; k<EventTrack->EventTrack.Num(); k++)
					{
						FEventTrackKey& Key = EventTrack->EventTrack[k];
						if(Key.EventName == EventNameToChange)
						{
							if ( !bModified )
							{
								EventTrack->Modify();
								bModified = true;
							}
							Key.EventName = NewEventName;
						}	
					}
				}			
			}
		}

		// Fire a delegate so other places that use the name can also update
		FMatineeDelegates::Get().OnEventKeyframeRenamed.Broadcast( MatineeActor, EventNameToChange, NewEventName );

		IData->UpdateEventNames();

		InterpEdTrans->EndSpecial();
	}
}

void FMatinee::OnSetAnimKeyLooping( bool bInLooping )
{
	bool bNewLooping = bInLooping; 

	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];
		UInterpTrack* Track = SelKey.Track;
		UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
		if(AnimTrack)
		{
			AnimTrack->AnimSeqs[SelKey.KeyIndex].bLooping = bNewLooping;
		}
	}
}

void FMatinee::OnSetAnimOffset( bool bInEndOffset )
{
	bool bEndOffset = bInEndOffset; 

	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return;
	}

	float CurrentOffset = 0.f;
	if(bEndOffset)
	{
		CurrentOffset = AnimTrack->AnimSeqs[SelKey.KeyIndex].AnimEndOffset;
	}
	else
	{
		CurrentOffset = AnimTrack->AnimSeqs[SelKey.KeyIndex].AnimStartOffset;
	}

	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "NewAnimOffset", "New Offset"),
		FText::AsNumber( CurrentOffset ),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnSetAnimOffsetTextCommitted, &SelKey, AnimTrack, bEndOffset)
		);
}

void FMatinee::OnSetAnimOffsetTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, UInterpTrackAnimControl* AnimTrack, bool bEndOffset)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double dNewOffset = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if(!bIsNumber)
			return;

		const float NewOffset = FMath::Max( (float)dNewOffset, 0.f );

		if(bEndOffset)
		{
			AnimTrack->AnimSeqs[SelKey->KeyIndex].AnimEndOffset = NewOffset;
		}
		else
		{
			AnimTrack->AnimSeqs[SelKey->KeyIndex].AnimStartOffset = NewOffset;
		}


		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

void FMatinee::OnSetAnimPlayRate()
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return;
	}

	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "NewAnimRates", "Play Rate"),
		FText::AsNumber( AnimTrack->AnimSeqs[SelKey.KeyIndex].AnimPlayRate ),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnSetAnimPlayRateTextCommitted, &SelKey, AnimTrack)
		);
}

void FMatinee::OnSetAnimPlayRateTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, UInterpTrackAnimControl* AnimTrack)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double dNewRate = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if(!bIsNumber)
			return;

		const float NewRate = FMath::Clamp( (float)dNewRate, 0.01f, 100.f );

		AnimTrack->AnimSeqs[SelKey->KeyIndex].AnimPlayRate = NewRate;

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

/** Handler for the toggle animation reverse menu item. */
void FMatinee::OnToggleReverseAnim()
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return;
	}

	AnimTrack->AnimSeqs[SelKey.KeyIndex].bReverse = !AnimTrack->AnimSeqs[SelKey.KeyIndex].bReverse;
}

/** Handler for UI update requests for the toggle anim reverse menu item. */
bool FMatinee::IsReverseAnimToggled()
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return false;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackAnimControl* AnimTrack = Cast<UInterpTrackAnimControl>(Track);
	if(!AnimTrack)
	{
		return false;
	}

	return (AnimTrack->AnimSeqs[SelKey.KeyIndex].bReverse==true);
}

void FMatinee::ExportCameraAnimationNameCommitted(const FText& InAnimationPackageName, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		FString PackageName = InAnimationPackageName.ToString();
		UPackage* Package = CreatePackage(NULL, *PackageName);
		check(Package);

		FString ObjectName = FPackageName::GetLongPackageAssetName(PackageName);

		UObject* ExistingPackage = FindPackage(NULL, *PackageName);
		if( ExistingPackage == NULL )
		{
			// Create the package
			ExistingPackage = CreatePackage(NULL,*PackageName);
		}

		// Make sure packages objects are duplicated into are fully loaded.
		TArray<UPackage*> TopLevelPackages;
		if( ExistingPackage )
		{
			TopLevelPackages.Add( ExistingPackage->GetOutermost() );
		}

		if(!PackageName.Len() || !ObjectName.Len())
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_InvalidInput", "Invalid input.") );
		}
		else
		{
			bool bNewObject = false, bSavedSuccessfully = false;

			UObject* ExistingObject = StaticFindObject(UCameraAnim::StaticClass(), ExistingPackage, *ObjectName, true);

			if (ExistingObject == NULL)
			{
				// attempting to create a new object, need to handle fully loading
				if( PackageTools::HandleFullyLoadingPackages( TopLevelPackages, NSLOCTEXT("UnrealEd", "ExportCameraAnim", "Export To CameraAnim") ) )
				{
					FText Reason;

					// make sure name of new object is unique
					if (ExistingPackage && !IsUniqueObjectName(*ObjectName, ExistingPackage, Reason))
					{
						FMessageDialog::Open( EAppMsgType::Ok, Reason);
					}
					else
					{
						// create it, then copy params into it
						ExistingObject = NewObject<UCameraAnim>(ExistingPackage, *ObjectName, RF_Public | RF_Standalone);
						bNewObject = true;
					}
				}
			}

			if (ExistingObject)
			{
				// copy params into it
				UCameraAnim* CamAnim = Cast<UCameraAnim>(ExistingObject);
				// Create the camera animation from the first selected group
				// because there should only be one selected group. 
				if (CamAnim->CreateFromInterpGroup(*GetSelectedGroupIterator(), MatineeActor))
				{
					bSavedSuccessfully = true;
					CamAnim->MarkPackageDirty();
				}
			}

			if (bNewObject)
			{
				if (!bSavedSuccessfully)
				{
					// delete the new object
					ExistingObject->MarkPendingKill();
				}
			}
		}
	}

	CloseEntryPopupMenu();
}

void FMatinee::OnContextSaveAsCameraAnimation()
{
	// There must be one and only one selected group to save a camera animation out. 
	check( GetSelectedGroupCount() == 1 );

	UObject* const SelectedCamAnim = GEditor->GetSelectedObjects()->GetTop<UCameraAnim>();
	FString ObjName = SelectedCamAnim ? *SelectedCamAnim->GetName() : TEXT("MyCameraAnimation");

	const FString PackageName(FString::Printf(TEXT("/Game/Unsorted/%s"), *ObjName));

	TSharedRef<STextEntryPopup> TextEntry = 
		SNew(STextEntryPopup)
		.Label(NSLOCTEXT("Matinee.Popups", "ExportCameraAnim_Header", "Export To CameraAnim"))
		.DefaultText(  FText::FromString(PackageName) )
		.OnTextCommitted(this, &FMatinee::ExportCameraAnimationNameCommitted)
		.ClearKeyboardFocusOnCommit( false );

	EntryPopupMenu = FSlateApplication::Get().PushMenu(
		ToolkitHost.Pin()->GetParentWidget(),
		FWidgetPath(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
		);
}

/**
 * Calculates The timeline position of the longest track, which includes 
 *			the duration of any assets, such as: sounds or animations.
 *
 * @note	Use the template parameter to define which tracks to consider (all, selected only, etc).
 * 
 * @return	The timeline position of the longest track.
 */
template <class TrackFilterType>
float FMatinee::GetLongestTrackTime() const
{
	float LongestTrackTime = 0.0f;

	// Iterate through each group to find the longest track time.
	for( TInterpTrackIterator<TrackFilterType> TrackIter(IData->InterpGroups); TrackIter; ++TrackIter )
	{
		float TrackEndTime = (*TrackIter)->GetTrackEndTime();

		// Update the longest track time. 
		if( TrackEndTime > LongestTrackTime )
		{
			LongestTrackTime = TrackEndTime;
		}
	}

	return LongestTrackTime;
}

/**
 * Moves the marker the user grabbed to the given time on the timeline. 
 *
 * @param	MarkerType	The marker to move.
 * @param	InterpTime	The position on the timeline to move the marker. 
 */
void FMatinee::MoveGrabbedMarker( float InterpTime )
{
	const bool bIgnoreSelectedKeys = false;
	const bool bIsLoopStartMarker = (GrabbedMarkerType == EMatineeMarkerType::ISM_LoopStart);

	switch( GrabbedMarkerType )
	{
	case EMatineeMarkerType::ISM_LoopStart:
		MoveLoopMarker( SnapTime(InterpTime, bIgnoreSelectedKeys), bIsLoopStartMarker );
		break;

	case EMatineeMarkerType::ISM_LoopEnd:
		MoveLoopMarker( SnapTime(InterpTime, bIgnoreSelectedKeys), bIsLoopStartMarker );
		break;

	case EMatineeMarkerType::ISM_SeqEnd:
		SetInterpEnd( SnapTime(InterpTime, bIgnoreSelectedKeys) );
		break;

	// Intentionally ignoring ISM_SeqStart because 
	// the sequence start must always be zero. 

	default:
		break;
	}
}

/**
 * Handler to move the grabbed marker to the current timeline position.
 */
void FMatinee::OnContextMoveMarkerToCurrentPosition()
{
	MoveGrabbedMarker(MatineeActor->InterpPosition);
}

/**
 * Handler to move the clicked-marker to the beginning of the sequence.
 */
void FMatinee::OnContextMoveMarkerToBeginning()
{
	MoveGrabbedMarker(0.0f);
}

/**
 * Handler to move the clicked-marker to the end of the sequence.
 */
void FMatinee::OnContextMoveMarkerToEnd()
{
	MoveGrabbedMarker(IData->InterpLength);
}

/**
 * Handler to move the clicked-marker to the end of the longest track.
 */
void FMatinee::OnContextMoveMarkerToEndOfLongestTrack()
{
	MoveGrabbedMarker( GetLongestTrackTime<FAllTrackFilter>() );
}

/**
 * Handler to move the clicked-marker to the end of the selected track.
 */
void FMatinee::OnContextMoveMarkerToEndOfSelectedTrack()
{
	MoveGrabbedMarker( GetLongestTrackTime<FSelectedTrackFilter>() );
}

/**
 * Called when the user toggles the preference for allowing clicks on keyframe "bars" to cause a selection
 */
void FMatinee::OnToggleKeyframeBarSelection()
{
	bAllowKeyframeBarSelection = !bAllowKeyframeBarSelection;
	GConfig->SetBool( TEXT("Matinee"), TEXT("AllowKeyframeBarSelection"), bAllowKeyframeBarSelection, GEditorPerProjectIni );
}

/**
 * Update the UI for the keyframe bar selection option
 */
bool FMatinee::IsKeyframeBarSelectionToggled()
{
	return bAllowKeyframeBarSelection;
}

/**
 * Called when the user toggles the preference for allowing clicks on keyframe text to cause a selection
 */
void FMatinee::OnToggleKeyframeTextSelection()
{
	bAllowKeyframeTextSelection = !bAllowKeyframeTextSelection;
	GConfig->SetBool( TEXT("Matinee"), TEXT("AllowKeyframeTextSelection"), bAllowKeyframeTextSelection, GEditorPerProjectIni );
}

bool FMatinee::IsKeyframeTextSelectionToggled()
{
	return bAllowKeyframeTextSelection;
}

/**
 * Update the UI for the lock camera pitch option
 */
bool FMatinee::IsLockCameraPitchToggled()
{
	return bLockCameraPitch;
}

/**
 * Called when the user toggles the preference for allowing to lock/unlock the camera pitch contraints
 */
void FMatinee::OnToggleLockCameraPitch()
{
	LockCameraPitchInViewports( !bLockCameraPitch );
}

/**
 * Updates the "lock camera pitch" value in all perspective viewports
 */
void FMatinee::LockCameraPitchInViewports(bool bLock)
{
	bLockCameraPitch = bLock;

	FLevelEditorViewportClient* ViewportClient = NULL;
	for( int32 iView = 0; iView < GEditor->LevelViewportClients.Num(); iView++ )
	{
		ViewportClient = GEditor->LevelViewportClients[ iView ];
		if( ViewportClient->IsPerspective() )
		{
			FEditorCameraController* CameraController = ViewportClient->GetCameraController();
			check(CameraController);
			CameraController->AccessConfig().bLockedPitch = bLock; 
		}
	}
}

void FMatinee::GetLockCameraPitchFromConfig()
{
	FLevelEditorViewportClient* ViewportClient = NULL;
	for( int32 iView = 0; iView < GEditor->LevelViewportClients.Num(); iView++ )
	{
		ViewportClient = GEditor->LevelViewportClients[ iView ];
		if( ViewportClient->IsPerspective() )
		{
			FEditorCameraController* CameraController = ViewportClient->GetCameraController();
			check(CameraController);
			bLockCameraPitch = CameraController->GetConfig().bLockedPitch; 
		}
	}
}

/**
 * Prompts the user to edit volumes for the selected sound keys.
 */
void FMatinee::OnSetSoundVolume()
{
	TArray<int32> SoundTrackKeyIndices;
	bool bFoundVolume = false;
	bool bKeysDiffer = false;
	float Volume = 1.0f;

	// Make a list of all keys and what their volumes are.
	for( int32 i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[i];
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackSound* SoundTrack		= Cast<UInterpTrackSound>( Track );

		if( SoundTrack )
		{
			SoundTrackKeyIndices.Add(i);
			const FSoundTrackKey& SoundTrackKey	= SoundTrack->Sounds[SelKey.KeyIndex];
			if ( !bFoundVolume )
			{
				bFoundVolume = true;
				Volume = SoundTrackKey.Volume;
			}
			else
			{
				if ( FMath::Abs(Volume-SoundTrackKey.Volume) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = true;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{
		// Display dialog and let user enter new rate.
		GenericTextEntryModeless(
			NSLOCTEXT("Matinee.Popups", "SetSoundVolume", "Volume"),
			FText::AsNumber( bKeysDiffer ? 1.0f : Volume ),
			FOnTextCommitted::CreateSP(this, &FMatinee::OnSetSoundVolumeTextEntered, SoundTrackKeyIndices)
		);
		

		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

void FMatinee::OnSetSoundVolumeTextEntered( const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices)
{
	CloseEntryPopupMenu();
	if( CommitInfo ==  ETextCommit::OnEnter )
	{
		double NewVolume = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if( bIsNumber )
		{
			const float ClampedNewVolume = FMath::Clamp( (float)NewVolume, 0.f, 100.f );
			for ( int32 i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
			{
				const int32 Index						= SoundTrackKeyIndices[i];
				const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[Index];
				UInterpTrack* Track					= SelKey.Track;
				UInterpTrackSound* SoundTrack		= CastChecked<UInterpTrackSound>( Track );
				FSoundTrackKey& SoundTrackKey		= SoundTrack->Sounds[SelKey.KeyIndex];
				SoundTrackKey.Volume				= ClampedNewVolume;
			}
		}
	
		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

/**
 * Prompts the user to edit pitches for the selected sound keys.
 */
void FMatinee::OnSetSoundPitch()
{
	TArray<int32> SoundTrackKeyIndices;
	bool bFoundPitch = false;
	bool bKeysDiffer = false;
	float Pitch = 1.0f;

	// Make a list of all keys and what their pitches are.
	for( int32 i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[i];
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackSound* SoundTrack		= Cast<UInterpTrackSound>( Track );

		if( SoundTrack )
		{
			SoundTrackKeyIndices.Add(i);
			const FSoundTrackKey& SoundTrackKey	= SoundTrack->Sounds[SelKey.KeyIndex];
			if ( !bFoundPitch )
			{
				bFoundPitch = true;
				Pitch = SoundTrackKey.Pitch;
			}
			else
			{
				if ( FMath::Abs(Pitch-SoundTrackKey.Pitch) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = true;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{
		// Display dialog and let user enter new rate.
		GenericTextEntryModeless(
			NSLOCTEXT("Matinee.Popups", "SetSoundPitch", "Pitch"),
			FText::AsNumber( bKeysDiffer ? 1.0f : Pitch ),
			FOnTextCommitted::CreateSP(this, &FMatinee::OnSetSoundPitchTextEntered, SoundTrackKeyIndices)
		);

		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

void FMatinee::OnSetSoundPitchTextEntered(const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double NewPitch = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if( bIsNumber )
		{
			const float ClampedNewPitch = FMath::Clamp( (float)NewPitch, 0.f, 100.f );
			for ( int32 i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
			{
				const int32 Index						= SoundTrackKeyIndices[i];
				const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[Index];
				UInterpTrack* Track					= SelKey.Track;
				UInterpTrackSound* SoundTrack		= CastChecked<UInterpTrackSound>( Track );
				FSoundTrackKey& SoundTrackKey		= SoundTrack->Sounds[SelKey.KeyIndex];
				SoundTrackKey.Pitch					= ClampedNewPitch;
			}
		}
	
		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

/** Syncs the generic browser to the currently selected sound track key */
void FMatinee::OnKeyContext_SyncGenericBrowserToSoundCue()
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		// Does this key have a sound cue set?
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[ 0 ];
		UInterpTrackSound* SoundTrack = Cast<UInterpTrackSound>( SelKey.Track );
		USoundBase* KeySound = SoundTrack->Sounds[ SelKey.KeyIndex ].Sound;
		if( KeySound != NULL )
		{
			TArray< UObject* > Objects;
			Objects.Add( KeySound );

			// Sync the generic/content browser!
			GEditor->SyncBrowserToObjects(Objects);
		}
	}
}



/** Called when the user wants to set the master volume on Audio Master track keys */
void FMatinee::OnKeyContext_SetMasterVolume()
{
	TArray<int32> SoundTrackKeyIndices;
	bool bFoundVolume = false;
	bool bKeysDiffer = false;
	float Volume = 1.0f;

	// Make a list of all keys and what their volumes are.
	for( int32 i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[i];
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

		if( AudioMasterTrack != NULL )
		{
			// SubIndex 0 = Volume
			const float CurKeyVolume = AudioMasterTrack->GetKeyOut( 0, SelKey.KeyIndex );

			SoundTrackKeyIndices.Add(i);
			if ( !bFoundVolume )
			{
				bFoundVolume = true;
				Volume = CurKeyVolume;
			}
			else
			{
				if ( FMath::Abs(Volume-CurKeyVolume) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = true;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{

		// Display dialog and let user enter new rate.
		GenericTextEntryModeless(
			NSLOCTEXT("Matinee.Popups", "SetMasterVolume", "Volume"),
			FText::AsNumber( bKeysDiffer ? 1.f : Volume ),
			FOnTextCommitted::CreateSP(this, &FMatinee::OnKeyContext_SetMasterVolumeTextCommitted, SoundTrackKeyIndices)
		);

		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

void FMatinee::OnKeyContext_SetMasterVolumeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double NewVolume = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if( bIsNumber )
		{
			const float ClampedNewVolume = FMath::Clamp( (float)NewVolume, 0.f, 100.f );
			for ( int32 i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
			{
				const int32 Index						= SoundTrackKeyIndices[i];
				const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[Index];
				UInterpTrack* Track					= SelKey.Track;
				UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

				// SubIndex 0 = Volume
				AudioMasterTrack->SetKeyOut( 0, SelKey.KeyIndex, ClampedNewVolume );
			}
		}

		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

/** Called when the user wants to set the master pitch on Audio Master track keys */
void FMatinee::OnKeyContext_SetMasterPitch()
{
	TArray<int32> SoundTrackKeyIndices;
	bool bFoundPitch = false;
	bool bKeysDiffer = false;
	float Pitch = 1.0f;

	// Make a list of all keys and what their pitches are.
	for( int32 i = 0 ; i < Opt->SelectedKeys.Num() ; ++i )
	{
		const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[i];
		UInterpTrack* Track					= SelKey.Track;
		UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

		if( AudioMasterTrack != NULL )
		{
			// SubIndex 1 = Pitch
			const float CurKeyPitch = AudioMasterTrack->GetKeyOut( 1, SelKey.KeyIndex );

			SoundTrackKeyIndices.Add(i);
			if ( !bFoundPitch )
			{
				bFoundPitch = true;
				Pitch = CurKeyPitch;
			}
			else
			{
				if ( FMath::Abs(Pitch-CurKeyPitch) > KINDA_SMALL_NUMBER )
				{
					bKeysDiffer = true;
				}
			}
		}
	}

	if ( SoundTrackKeyIndices.Num() )
	{
		// Display dialog and let user enter new rate.
		GenericTextEntryModeless(
			NSLOCTEXT("Menu.Popups", "SetMasterSoundPitch", "Pitch"),
			FText::AsNumber( bKeysDiffer ? 1.f : Pitch ),
			FOnTextCommitted::CreateSP(this, &FMatinee::OnKeyContext_SetMasterPitchTextCommitted, SoundTrackKeyIndices)
		);

		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}
void FMatinee::OnKeyContext_SetMasterPitchTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, TArray<int32> SoundTrackKeyIndices)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double NewPitch = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if( bIsNumber )
		{
			const float ClampedNewPitch = FMath::Clamp( (float)NewPitch, 0.f, 100.f );
			for ( int32 i = 0 ; i < SoundTrackKeyIndices.Num() ; ++i )
			{
				const int32 Index						= SoundTrackKeyIndices[i];
				const FInterpEdSelKey& SelKey		= Opt->SelectedKeys[Index];
				UInterpTrack* Track					= SelKey.Track;
				UInterpTrackAudioMaster* AudioMasterTrack = Cast<UInterpTrackAudioMaster>( Track );

				// SubIndex 1 = Pitch
				AudioMasterTrack->SetKeyOut( 1, SelKey.KeyIndex, ClampedNewPitch );
			}
		}


		MatineeActor->MarkPackageDirty();

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

/** Called when the user wants to set the clip ID number for Particle Replay track keys */
void FMatinee::OnParticleReplayKeyContext_SetClipIDNumber()
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		const FInterpEdSelKey& FirstSelectedKey = Opt->SelectedKeys[ 0 ];

		// We only support operating on one key at a time, we'll use the first selected key.
		UInterpTrackParticleReplay* ParticleReplayTrack =
			Cast< UInterpTrackParticleReplay >( FirstSelectedKey.Track );
		if( ParticleReplayTrack != NULL )
		{
			FParticleReplayTrackKey& ParticleReplayKey =
				ParticleReplayTrack->TrackKeys[ FirstSelectedKey.KeyIndex ];

			GenericTextEntryModeless(
				NSLOCTEXT("Matinee.Popup", "ParticleReplayKey.SetClipIDNumber", "Clip ID Number"),
				FText::AsNumber( ParticleReplayKey.ClipIDNumber ),
				FOnTextCommitted::CreateSP(this, &FMatinee::OnParticleReplayKeyContext_SetClipIDNumberTextCommitted, &ParticleReplayKey)
			);
			
		}
	}
}

void FMatinee::OnParticleReplayKeyContext_SetClipIDNumberTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FParticleReplayTrackKey* ParticleReplayKey)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		int32 NewClipIDNumber = FCString::Atoi(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if( bIsNumber )
		{
			// Store the new value!
			ParticleReplayKey->ClipIDNumber = NewClipIDNumber;

			// Mark the package as dirty
			MatineeActor->MarkPackageDirty();

			// Refresh Matinee
			RefreshInterpPosition();
		}
	}
}

/** Called when the user wants to set the duration of Particle Replay track keys */
void FMatinee::OnParticleReplayKeyContext_SetDuration()
{
	if( Opt->SelectedKeys.Num() > 0 )
	{
		const FInterpEdSelKey& FirstSelectedKey = Opt->SelectedKeys[ 0 ];

		// We only support operating on one key at a time, we'll use the first selected key.
		UInterpTrackParticleReplay* ParticleReplayTrack =
			Cast< UInterpTrackParticleReplay >( FirstSelectedKey.Track );
		if( ParticleReplayTrack != NULL )
		{
			FParticleReplayTrackKey& ParticleReplayKey =
				ParticleReplayTrack->TrackKeys[ FirstSelectedKey.KeyIndex ];

			GenericTextEntryModeless(
				NSLOCTEXT("Matinee.Popups", "ParticleReplayKey.SetDuration", "Duration"),
				FText::AsNumber( ParticleReplayKey.Duration ),
				FOnTextCommitted::CreateSP(this, &FMatinee::OnParticleReplayKeyContext_SetDurationTextCommitted, &ParticleReplayKey)
			);
		}
	}
}

void FMatinee::OnParticleReplayKeyContext_SetDurationTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FParticleReplayTrackKey* ParticleReplayKey)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		int32 NewDuration = FCString::Atoi(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if( bIsNumber )
		{
			// Store the new value!
			ParticleReplayKey->Duration = ( float )NewDuration;

			// Mark the package as dirty
			MatineeActor->MarkPackageDirty();

			// Refresh Matinee
			RefreshInterpPosition();
		}
	}
}

	
/** Called to delete the currently selected keys */
void FMatinee::OnDeleteSelectedKeys()
{
	const bool bWantTransactions = true;
	DeleteSelectedKeys( bWantTransactions );
}



void FMatinee::OnContextDirKeyTransitionTime()
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>(Track);
	if(!DirTrack)
	{
		return;
	}

	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "NewTransitionTime", "Time"),
		FText::AsNumber( DirTrack->CutTrack[SelKey.KeyIndex].TransitionTime ),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnContextDirKeyTransitionTimeTextCommitted, &SelKey, DirTrack)
		);
}

void FMatinee::OnContextDirKeyTransitionTimeTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, UInterpTrackDirector* DirTrack)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		double dNewTime = FCString::Atod(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if(!bIsNumber)
			return;

		const float NewTime = (float)dNewTime;

		DirTrack->CutTrack[SelKey->KeyIndex].TransitionTime = NewTime;

		// Update stuff in case doing this has changed it.
		RefreshInterpPosition();
	}
}

void FMatinee::OnContextDirKeyRenameCameraShot()
{
	if(Opt->SelectedKeys.Num() != 1)
	{
		return;
	}

	FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];
	UInterpTrack* Track = SelKey.Track;
	UInterpTrackDirector* DirTrack = Cast<UInterpTrackDirector>(Track);
	if(!DirTrack)
	{
		return;
	}
	
	GenericTextEntryModeless(
		NSLOCTEXT("Matinee.Popups", "SetNewCameraShotNumber", "Shot Number"),
		FText::AsNumber( DirTrack->CutTrack[SelKey.KeyIndex].ShotNumber ),
		FOnTextCommitted::CreateSP(this, &FMatinee::OnContextDirKeyRenameCameraShotTextCommitted, &SelKey, DirTrack)
	);
}

void FMatinee::OnContextDirKeyRenameCameraShotTextCommitted(const FText& InText, ETextCommit::Type CommitInfo, FInterpEdSelKey* SelKey, UInterpTrackDirector* DirTrack)
{
	CloseEntryPopupMenu();
	if (CommitInfo == ETextCommit::OnEnter)
	{
		uint32 NewShot = FCString::Atoi(*InText.ToString());    
		const bool bIsNumber = InText.IsNumeric();
		if(!bIsNumber)
			return;
	
		DirTrack->CutTrack[SelKey->KeyIndex].ShotNumber = NewShot;
	}
}

void FMatinee::OnFlipToggleKey()
{
	for (int32 KeyIndex = 0; KeyIndex < Opt->SelectedKeys.Num(); KeyIndex++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[KeyIndex];
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackToggle* ToggleTrack = Cast<UInterpTrackToggle>(Track);
		if (ToggleTrack)
		{
			FToggleTrackKey& ToggleKey = ToggleTrack->ToggleTrack[SelKey.KeyIndex];
			ToggleKey.ToggleAction = (ToggleKey.ToggleAction == ETTA_Off) ? ETTA_On : ETTA_Off;
			Track->MarkPackageDirty();
		}

		UInterpTrackVisibility* VisibilityTrack = Cast<UInterpTrackVisibility>(Track);
		if (VisibilityTrack)
		{
			FVisibilityTrackKey& VisibilityKey = VisibilityTrack->VisibilityTrack[SelKey.KeyIndex];
			VisibilityKey.Action = (VisibilityKey.Action == EVTA_Hide) ? EVTA_Show : EVTA_Hide;
			Track->MarkPackageDirty();
		}
	}
}



/** Called when a new key condition is selected in a track keyframe context menu */
void FMatinee::OnKeyContext_SetCondition( FMatineeCommands::EKeyAction::Type InCondition )
{
	for (int32 KeyIndex = 0; KeyIndex < Opt->SelectedKeys.Num(); KeyIndex++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[KeyIndex];
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackVisibility* VisibilityTrack = Cast<UInterpTrackVisibility>(Track);
		if (VisibilityTrack)
		{
			FVisibilityTrackKey& VisibilityKey = VisibilityTrack->VisibilityTrack[SelKey.KeyIndex];

			switch( InCondition )
			{
			case FMatineeCommands::EKeyAction::ConditionAlways: 
					VisibilityKey.ActiveCondition = EVTC_Always;
					break;

			case FMatineeCommands::EKeyAction::ConditionGoreEnabled: 
					VisibilityKey.ActiveCondition = EVTC_GoreEnabled;
					break;

			case FMatineeCommands::EKeyAction::ConditionGoreDisabled: 
					VisibilityKey.ActiveCondition = EVTC_GoreDisabled;
					break;
			}

			Track->MarkPackageDirty();
		}
	}
}

bool FMatinee::KeyContext_IsSetConditionToggled( FMatineeCommands::EKeyAction::Type InCondition )
{
	for (int32 KeyIndex = 0; KeyIndex < Opt->SelectedKeys.Num(); KeyIndex++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[KeyIndex];
		UInterpTrack* Track = SelKey.Track;

		UInterpTrackVisibility* VisibilityTrack = Cast<UInterpTrackVisibility>(Track);
		if (VisibilityTrack)
		{
			FVisibilityTrackKey& VisibilityKey = VisibilityTrack->VisibilityTrack[SelKey.KeyIndex];

			switch( InCondition )
			{
			case FMatineeCommands::EKeyAction::ConditionAlways: 
				if (VisibilityKey.ActiveCondition != EVTC_Always)
				{
					return false;
				}
				break;

			case FMatineeCommands::EKeyAction::ConditionGoreEnabled: 
				if (VisibilityKey.ActiveCondition != EVTC_GoreEnabled)
				{
					return false;
				}
				break;

			case FMatineeCommands::EKeyAction::ConditionGoreDisabled: 
				if (VisibilityKey.ActiveCondition != EVTC_GoreDisabled)
				{
					return false;
				}
				break;
			}
		}
	}

	return true;
}


void FMatinee::OnMenuUndo()
{
	InterpEdUndo();
}

void FMatinee::OnMenuRedo()
{
	InterpEdRedo();
}

/** Menu handler for cut operations. */
void FMatinee::OnMenuCut()
{
	CopySelectedGroupOrTrack(true);
}

bool FMatinee::CanCut()
{
	return !IsCameraAnim();
}

/** Menu handler for copy operations. */
void FMatinee::OnMenuCopy()
{
	CopySelectedGroupOrTrack(false);
}

/** Menu handler for paste operations. */
void FMatinee::OnMenuPaste()
{
	PasteSelectedGroupOrTrack();
}


void FMatinee::OnMenuImport()
{
	if( MatineeActor != NULL )
	{
		TArray<FString> OpenFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bOpened = false;
		if (DesktopPlatform != NULL)
		{
			bOpened = DesktopPlatform->OpenFileDialog(
				GetMatineeDialogParentWindow(), 
				NSLOCTEXT("UnrealEd", "ImportMatineeSequence", "Import UnrealMatinee Sequence").ToString(),
				*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT)),
				TEXT(""),
				TEXT("FBX document|*.fbx"),
				EFileDialogFlags::None, 
				OpenFilenames);
		}
		if (bOpened)
		{
			// Get the filename from dialog
			FString ImportFilename = OpenFilenames[0];
			FString FileName = OpenFilenames[0];
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, FPaths::GetPath(FileName)); // Save path as default for next time.
		
			const FString FileExtension = FPaths::GetExtension(FileName);
			const bool bIsFBX = FileExtension.Equals( TEXT("FBX"), ESearchCase::IgnoreCase );

			if (bIsFBX)
			{
				// Import the Matinee information from the FBX document.
				UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
				if (FFbxImporter->ImportFromFile(ImportFilename, FileExtension ))
				{
					FFbxImporter->SetProcessUnknownCameras(false);
					
					if ( FFbxImporter->HasUnknownCameras( MatineeActor ) )
					{
						// Ask the user whether to create any missing cameras.
						EAppReturnType::Type Result = FMessageDialog::Open( EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "ImportMatineeSequence_MissingCameras", "Create all cameras not in the current Unreal scene but present in the file?") );
						FFbxImporter->SetProcessUnknownCameras(Result == EAppReturnType::Yes);
					}

					// Re-create the Matinee sequence.
					if (FFbxImporter->ImportMatineeSequence(MatineeActor))
					{
						if (FEngineAnalytics::IsAvailable())
						{
							FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.Imported"));
						}
					}

					// We have modified the sequence, so update its UI.
					NotifyPostChange(NULL, NULL);
				}
				FFbxImporter->ReleaseScene();
			}
			else
			{
				// Invalid filename 
			}
		}
	}
}

void FMatinee::OnMenuExport()
{
	if( MatineeActor != NULL )
	{
		TArray<FString> SaveFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bSaved = false;
		if (DesktopPlatform != NULL)
		{
			bSaved = DesktopPlatform->SaveFileDialog(
				GetMatineeDialogParentWindow(), 
				NSLOCTEXT("UnrealEd", "ExportMatineeSequence", "Export UnrealMatinee Sequence").ToString(), 
				*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)), 
				TEXT(""), 
				TEXT("FBX document|*.fbx"),
				EFileDialogFlags::None, 
				SaveFilenames);
		}


		// Show dialog and execute the import if the user did not cancel out
		if( bSaved )
		{
			// Get the filename from dialog
			FString ExportFilename = SaveFilenames[0];
			FString FileName = SaveFilenames[0];
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, FPaths::GetPath(FileName)); // Save path as default for next time.

			const FString FileExtension = FPaths::GetExtension(FileName);
			const bool bIsFBX = FCString::Stricmp(*FileExtension, TEXT("FBX")) == 0;
			
			MatineeExporter* Exporter = NULL;
			if (bIsFBX)
			{
				Exporter = UnFbx::FFbxExporter::GetInstance();

				//Show the fbx export dialog options
				bool ExportCancel = false;
				bool ExportAll = false;
				Exporter->FillExportOptions(false, true, ExportFilename, ExportCancel, ExportAll);
				if (!ExportCancel)
				{
					// Export the Matinee information to an FBX file
					Exporter->CreateDocument();
					Exporter->SetTrasformBaking(bBakeTransforms);
					const bool bKeepHierarchy = GetDefault<UEditorPerProjectUserSettings>()->bKeepAttachHierarchy;
					Exporter->SetKeepHierarchy(bKeepHierarchy);

					UnFbx::FFbxExporter::FMatineeNodeNameAdapter NodeNameAdapter(MatineeActor);

					const bool bSelectedOnly = false;
					// Export the persistent level and all of it's actors
					Exporter->ExportLevelMesh(MatineeActor->GetWorld()->PersistentLevel, bSelectedOnly, NodeNameAdapter);

					// Export streaming levels and actors
					for (int32 CurLevelIndex = 0; CurLevelIndex < MatineeActor->GetWorld()->GetNumLevels(); ++CurLevelIndex)
					{
						ULevel* CurLevel = MatineeActor->GetWorld()->GetLevel(CurLevelIndex);
						if (CurLevel != NULL && CurLevel != (MatineeActor->GetWorld()->PersistentLevel))
						{
							Exporter->ExportLevelMesh(CurLevel, bSelectedOnly, NodeNameAdapter);
						}
					}

					// Export Matinee
					if (Exporter->ExportMatinee(MatineeActor))
					{
						if (FEngineAnalytics::IsAvailable())
						{
							FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.Exported"));
						}
					}

					// Save to disk
					Exporter->WriteToFile(*ExportFilename);
				}
			}
			else
			{
				// Invalid file
			}
		}
	}
}



void FMatinee::OnExportSoundCueInfoCommand()
{
	if( MatineeActor == NULL )
	{
		return;
	}
	
	TArray<FString> SaveFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bOpened = false;
	if (DesktopPlatform != NULL)
	{
		bOpened = DesktopPlatform->SaveFileDialog(
			GetMatineeDialogParentWindow(), 
			NSLOCTEXT("UnrealEd", "InterpEd_ExportSoundCueInfoDialogTitle", "Export Sound Cue Info" ).ToString(),
			*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)),
			TEXT( "" ),
			TEXT( "CSV file|*.csv" ),
			EFileDialogFlags::None, 
			SaveFilenames);
	}

	// Show dialog and execute the import if the user did not cancel out
	if( bOpened )
	{
		// Get the filename from dialog
		FString ExportFilename = FPaths::GetPath(SaveFilenames[0]);
		FString FileName = SaveFilenames[0];

		// Save path as default for next time.
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, FPaths::GetPath(FileName));


		FArchive* CSVFile = IFileManager::Get().CreateFileWriter( *FileName );
		if( CSVFile != NULL )
		{
			// Write header
			{
				FString TextLine( TEXT( "Group,Track,SoundCue,Time,Frame,Anim,AnimTime,AnimFrame" ) LINE_TERMINATOR );
				CSVFile->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
			}

			for( int32 CurGroupIndex = 0; CurGroupIndex < MatineeActor->MatineeData->InterpGroups.Num(); ++CurGroupIndex )
			{
				const UInterpGroup* CurGroup = MatineeActor->MatineeData->InterpGroups[ CurGroupIndex ];
				if( CurGroup != NULL )
				{
					for( int32 CurTrackIndex = 0; CurTrackIndex < CurGroup->InterpTracks.Num(); ++CurTrackIndex )
					{
						const UInterpTrack* CurTrack = CurGroup->InterpTracks[ CurTrackIndex ];
						if( CurTrack != NULL )
						{
							const UInterpTrackSound* SoundTrack = Cast<const  UInterpTrackSound >( CurTrack );
							if( SoundTrack != NULL )
							{
								for( int32 CurSoundIndex = 0; CurSoundIndex < SoundTrack->Sounds.Num(); ++CurSoundIndex )
								{
									const FSoundTrackKey& CurSound = SoundTrack->Sounds[ CurSoundIndex ];
									if( CurSound.Sound != NULL )
									{
										FString FoundAnimName;
										float FoundAnimTime = 0.0f;

										// Search for an animation track in this group that overlaps this sound's start time
										for( TArray< UInterpTrack* >::TConstIterator TrackIter( CurGroup->InterpTracks ); TrackIter; ++TrackIter )
										{
											const UInterpTrackAnimControl* AnimTrack = Cast<const  UInterpTrackAnimControl >( *TrackIter );
											if( AnimTrack != NULL )
											{
												// Iterate over animations in this anim track
												for( TArray< FAnimControlTrackKey >::TConstIterator AnimKeyIter( AnimTrack->AnimSeqs ); AnimKeyIter; ++AnimKeyIter )
												{
													const FAnimControlTrackKey& CurAnimKey = *AnimKeyIter;

													// Does this anim track overlap the sound's start time?
													if( CurSound.Time >= CurAnimKey.StartTime )
													{
														FoundAnimName = CurAnimKey.AnimSeq ? CurAnimKey.AnimSeq->GetName() : FString::Printf(TEXT("NULL"));

														// Compute the time the sound exists at within this animation
														FoundAnimTime = ( CurSound.Time - CurAnimKey.StartTime ) + CurAnimKey.AnimStartOffset;

														// NOTE: The array is ordered, so we'll take the LAST anim we find that overlaps the sound!
													}
												}
											}
										}


										// Also store values as frame numbers instead of time values if a frame rate is selected
										const int32 SoundFrameIndex = bSnapToFrames ? FMath::TruncToInt( CurSound.Time / SnapAmount ) : 0;

										FString TextLine = FString::Printf(
											TEXT( "%s,%s,%s,%0.2f,%i" ),
											*CurGroup->GroupName.ToString(),
											*CurTrack->TrackTitle,
											*CurSound.Sound->GetName(),
											CurSound.Time,
											SoundFrameIndex );

										// Did we find an animation that overlaps this sound?  If so, we'll emit that info
										if( FoundAnimName.Len() > 0 )
										{
											// Also store values as frame numbers instead of time values if a frame rate is selected
											const int32 AnimFrameIndex = bSnapToFrames ? FMath::TruncToInt( FoundAnimTime / SnapAmount ) : 0;

											TextLine += FString::Printf(
												TEXT( ",%s,%.2f,%i" ),
												*FoundAnimName,
												FoundAnimTime,
												AnimFrameIndex );
										}

										TextLine += LINE_TERMINATOR;

										CSVFile->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
									}
								}
							}
						}
					}
				}
			}

			// Close and delete archive
			CSVFile->Close();
			delete CSVFile;

			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.ExportedSoundCue"));
			}
		}
		else
		{
			UE_LOG(LogSlateMatinee, Warning, TEXT("Could not create CSV file %s for writing."), *FileName );
		}
	}
}

void FMatinee::OnExportAnimationInfoCommand()
{
	if( MatineeActor == NULL )
	{
		return;
	}

	UInterpData* InterpData = MatineeActor->MatineeData;
			
	//Get Our File Name from the Obj Comment
	FString MatineeComment = TEXT("");
	FString FileName = TEXT("");
	
	MatineeComment = MatineeActor->GetName();
	FileName = FString::Printf(TEXT("MatineeAnimInfo%s"),*MatineeComment);
	//remove whitespaces
	FileName = FileName.Replace(TEXT(" "),TEXT(""));

	TArray<FString> SaveFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	bool bSaved = false;
	if (DesktopPlatform != NULL)
	{
		bSaved = DesktopPlatform->SaveFileDialog(
			GetMatineeDialogParentWindow(), 
			NSLOCTEXT("UnrealEd", "InterpEd_ExportAnimationInfoDialogTitle", "Export Animation Info" ).ToString(),
			*(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT)),
			*FileName,
			TEXT( "Text file|*.txt" ),
			EFileDialogFlags::None, 
			SaveFilenames);
	}


	// Show dialog and execute the import if the user did not cancel out
	if( bSaved )
	{
		// Get the filename from dialog
		FString ExportFilename = FPaths::GetPath(SaveFilenames[0]);
		FString SaveFileName = SaveFilenames[0];

		// Save path as default for next time.
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, FPaths::GetPath(SaveFileName));

		
		FArchive* File = IFileManager::Get().CreateFileWriter( *SaveFileName );
		if( File != NULL )
		{	
			FString TextLine;
			
			//Header w/Comment	
			TextLine = FString::Printf(TEXT("Matinee Animation Data Export%s"),LINE_TERMINATOR);
			TextLine += FString::Printf(TEXT("Comment: %s%s"),*MatineeComment,LINE_TERMINATOR);
			TextLine += LINE_TERMINATOR;
			File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );	
			
			
			// Director Track Data...
			UInterpGroupDirector* DirGroup = InterpData->FindDirectorGroup();
			UInterpTrackDirector* DirTrack = DirGroup ? DirGroup->GetDirectorTrack() : NULL;
			
			TextLine = FString::Printf(TEXT("Director:%s"),LINE_TERMINATOR);
			File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
			if ( DirTrack && DirTrack->CutTrack.Num() > 0 )
			{
				//Keys
				for( int32 KeyFrameIndex=0; KeyFrameIndex < DirTrack->CutTrack.Num(); KeyFrameIndex++)
				{
					const FDirectorTrackCut& Cut = DirTrack->CutTrack[KeyFrameIndex];
					
					float Time = Cut.Time;
					FString TargetCamGroup = Cut.TargetCamGroup.ToString();

					FString ShotName = DirTrack->GetViewedCameraShotName( Cut.Time );
					if( ShotName.IsEmpty() )
					{
						ShotName = TEXT("<Unknown>");
					}
					
					TextLine = FString::Printf(TEXT("\tKeyFrame: %d,\tTime: %.2f,\tCameraGroup: %s,\tShotName: %s%s"),KeyFrameIndex,Time,*TargetCamGroup,*ShotName,LINE_TERMINATOR);
					File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 								
				}
			}
			else
			{
				TextLine = FString::Printf(TEXT("\t(No Director Track Data)%s"),LINE_TERMINATOR);
				File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 			
			}
			
			//Anim Group/Track Data
			bool bAnimDataFound = false;
			if(InterpData)
			{
				//Groups
				for(int32 GroupIndex=0; GroupIndex < InterpData->InterpGroups.Num(); GroupIndex++)
				{
					UInterpGroup* Group = InterpData->InterpGroups[GroupIndex];
					
					//check for any animation tracks...
					TArray<UInterpTrack*> AnimTracks; //= Cast<UInterpTrackAnimControl>(Track);
					Group->FindTracksByClass(UInterpTrackAnimControl::StaticClass(),AnimTracks);
					if (AnimTracks.Num() > 0)
					{
						TextLine = LINE_TERMINATOR;
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 	
					
						FString GroupName = Group->GroupName.ToString();
						TextLine = FString::Printf(TEXT("Group: %s%s"),*GroupName,LINE_TERMINATOR);	
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 	
						bAnimDataFound = true;										
					}
					//Tracks
					for (int32 TrackIndex=0; TrackIndex < AnimTracks.Num(); TrackIndex++)
					{		
						UInterpTrackAnimControl *Track = Cast<UInterpTrackAnimControl>(AnimTracks[TrackIndex]);
						FString TrackName = Track->TrackTitle;
						TextLine = FString::Printf(TEXT("\tTrack: %s%s"),*TrackName,LINE_TERMINATOR);	
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
						
						//Keys 				
						for( int32 KeyFrameIndex=0; KeyFrameIndex < Track->AnimSeqs.Num(); KeyFrameIndex++)
						{
							FAnimControlTrackKey &Key = Track->AnimSeqs[KeyFrameIndex];
							UAnimSequence* Seq = Key.AnimSeq;

							//animation controls					
							float Time = Track->GetKeyframeTime(KeyFrameIndex);
							
							FString AnimSeqName = Seq ? Key.AnimSeq->GetName() : TEXT("None");
							
							float AnimStartTime = Key.AnimStartOffset;
							
							float AnimEndTime = (Seq) ? (Seq->SequenceLength - Key.AnimEndOffset) : 0.0f;
					
							float AnimPlayRate = Key.AnimPlayRate;
							bool bLooping = Key.bLooping;
							bool bReverse = Key.bReverse;
						
							TextLine = FString::Printf(
								TEXT("\t\tKeyFrame: %d,\tTime: %.2f,"),
								KeyFrameIndex,
								Time,
								LINE_TERMINATOR);
							//do a bit of formatting to clean up our file
							AnimSeqName += TEXT(",");
							AnimSeqName = AnimSeqName.RightPad(20);
							TextLine += FString::Printf(
								TEXT("\tSequence: %s\tAnimStart: %.2f,\tAnimEnd: %.2f,\tPlayRate: %.2f,\tLoop:%d, Reverse:%d%s"),
								*AnimSeqName,
								AnimStartTime,
								AnimEndTime,
								AnimPlayRate,
								bLooping,
								bReverse,
								LINE_TERMINATOR);
							File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 					
						}
						
						TextLine = LINE_TERMINATOR;
						File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 				
					}							
				}
			}
			
			if (!bAnimDataFound)
			{
				TextLine = LINE_TERMINATOR;
				File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() );
				TextLine = FString::Printf(TEXT("(No Animation Data)%s"),LINE_TERMINATOR);
				File->Serialize( TCHAR_TO_ANSI( *TextLine ), TextLine.Len() ); 		
			}
									

			// Close and delete archive
			File->Close();
			delete File;

			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Matinee.ExportedAnimationInfo"));
			}
		}
	}
}

/**
 * Called when the user toggles the ability to export a key every frame. 
 */
void FMatinee::OnToggleBakeTransforms()
{
	bBakeTransforms = !bBakeTransforms;
}

/**
 * Updates the checked-menu item for baking transforms
 */
bool FMatinee::IsBakeTransformsToggled()
{
	return bBakeTransforms;
}

/**
 * Called when the user toggles the ability to export a key every frame. 
 */
void FMatinee::OnToggleKeepHierarchy()
{
	auto* Settings = GetMutableDefault<UEditorPerProjectUserSettings>();
	Settings->bKeepAttachHierarchy = !Settings->bKeepAttachHierarchy;
}

/**
 * Updates the checked-menu item for baking transforms
 */
bool FMatinee::IsKeepHierarchyToggled()
{
	return GetDefault<UEditorPerProjectUserSettings>()->bKeepAttachHierarchy;
}


void FMatinee::OnMenuReduceKeys()
{
	ReduceKeys();
}

/** Toggles interting of the panning the interp editor left and right */
void FMatinee::OnToggleInvertPan()
{
	bInvertPan = !bInvertPan;

	GConfig->SetBool(TEXT("Matinee"), TEXT("InterpEdPanInvert"), bInvertPan, GEditorPerProjectIni);
}

bool FMatinee::IsInvertPanToggled()
{
	return bInvertPan;
}

/** Called when split translation and rotation is selected from a movement track context menu */
void FMatinee::OnSplitTranslationAndRotation()
{
	check( HasATrackSelected() );

	ClearKeySelection();

	// Check to make sure there is a movement track in list
	// before attempting to start the transaction system.
	if( HasATrackSelected( UInterpTrackMove::StaticClass() ) )
	{
		InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "InterpEd_Undo_SplitTranslationAndRotation", "Split translation and rotation") );

		MatineeActor->Modify();
		IData->Modify();

		TTrackClassTypeIterator<UInterpTrackMove> MoveTrackIt(GetSelectedTrackIterator<UInterpTrackMove>());
		for( ; MoveTrackIt; ++MoveTrackIt )
		{
			// Grab the movement track for the selected group
			UInterpTrackMove* MoveTrack = *MoveTrackIt;
			MoveTrack->Modify();
			// Remove from the Curve editor, if its there.
			IData->CurveEdSetup->RemoveCurve(MoveTrack);
			DeselectTrack( (UInterpGroup*)MoveTrack->GetOuter(), MoveTrack );
			MoveTrack->SplitTranslationAndRotation();

		}

		InterpEdTrans->EndSpecial();

		UpdateTrackWindowScrollBars();
	}
	
	// Make sure the curve editor is in sync
	CurveEd->CurveChanged();
}

/** Normalize Velocity Dialog */
class SMatineeNormalizeVelocity : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMatineeNormalizeVelocity)
	{}
	SLATE_END_ARGS()	

	/** SCompoundWidget interface */
	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow)
	{
		bResult = false;
		ParentWindowPtr = InParentWindow;

		const float Width = .7f;
		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(Width)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Matinee.NormalizeVelocity", "IntervalStart", "Interval Start"))
				]
				+SHorizontalBox::Slot()
				.FillWidth(1 - Width)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SMatineeNormalizeVelocity::GetIntervalStart)
					.OnValueChanged(this, &SMatineeNormalizeVelocity::SetIntervalStart)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(Width)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Matinee.NormalizeVelocity", "IntervalEnd", "Interval End"))
				]
				+SHorizontalBox::Slot()
				.FillWidth(1 - Width)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SMatineeNormalizeVelocity::GetIntervalEnd)
					.OnValueChanged(this, &SMatineeNormalizeVelocity::SetIntervalEnd)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(Width)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Matinee.NormalizeVelocity", "FullInterval", "Full Interval"))
				]
				+SHorizontalBox::Slot()
				.FillWidth(1 - Width)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SMatineeNormalizeVelocity::ToggleFullInterval)
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked(this, &SMatineeNormalizeVelocity::OnOkay)
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SMatineeNormalizeVelocity::OnCancel)
				]
			]
		];
	}

public:
	// The return values of the dialog.
	float IntervalStart;
	float IntervalEnd;
	bool bFullInterval;

	//true for okay, false for cancel
	bool bResult;

private:
	TWeakPtr<SWindow> ParentWindowPtr;

	void SetIntervalStart(float InStart) { IntervalStart = InStart; }
	void SetIntervalEnd(float InEnd) { IntervalEnd = InEnd; }
	void ToggleFullInterval(ECheckBoxState CheckState) { bFullInterval = (CheckState == ECheckBoxState::Checked); }

	bool  UseFullInterval() { return bFullInterval; }
	TOptional<float> GetIntervalStart() const {return IntervalStart; }
	TOptional<float> GetIntervalEnd() const {return IntervalEnd; }


	FReply OnOkay()
	{
		bResult = true;
		ParentWindowPtr.Pin()->RequestDestroyWindow();

		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bResult = false;
		ParentWindowPtr.Pin()->RequestDestroyWindow();

		return FReply::Handled();
	}
};



/** 
 * Reparameterizes the curve in the passed in movement track in terms of arc length.  (For constant velocity)
 * 
 * @param	InMoveTrack	The move track containing curves to reparameterize
 * @param	StartTime	The start of the time range to reparameterize
 * @param	EndTime		The end of the time range to reparameterize
 * @param	OutReparameterizedCurve	The resulting reparameterized curve
 */
static float ReparameterizeCurve( const UInterpTrackMove* InMoveTrack, float StartTime, float EndTime, FInterpCurveFloat& OutReparameterizedCurve )
{
	//@todo Should really be adaptive
	const int32 NumSteps = 500;

	// Clear out any existing points
	OutReparameterizedCurve.Reset();

	// This should only be called on split tracks
	check( InMoveTrack->SubTracks.Num() > 0 );

	// Get each curve
	const FInterpCurveFloat& XAxisCurve = CastChecked<UInterpTrackMoveAxis>( InMoveTrack->SubTracks[AXIS_TranslationX] )->FloatTrack;
	const FInterpCurveFloat& YAxisCurve = CastChecked<UInterpTrackMoveAxis>( InMoveTrack->SubTracks[AXIS_TranslationY] )->FloatTrack;
	const FInterpCurveFloat& ZAxisCurve = CastChecked<UInterpTrackMoveAxis>( InMoveTrack->SubTracks[AXIS_TranslationZ] )->FloatTrack;

	// Current time should start at the passed in start time
	float CurTime = StartTime;
	// Determine the amount of time to step
	const float Interval = (EndTime - CurTime)/((float)(NumSteps-1)); 

	// Add first entry, using first point on curve, total distance will be 0
	FVector StartPos = FVector::ZeroVector;
	StartPos.X = XAxisCurve.Eval( CurTime, 0.f );
	StartPos.Y = YAxisCurve.Eval( CurTime, 0.f );
	StartPos.Z = ZAxisCurve.Eval( CurTime, 0.f );

	float TotalLen = 0.f;
	OutReparameterizedCurve.AddPoint( TotalLen, CurTime );

	// Increment time past the first entry
	CurTime += Interval;

	// Iterate over the curve
	for(int32 i=1; i<NumSteps; i++)
	{
		// Determine the length of this segment.
		FVector NewPos = FVector::ZeroVector;
		NewPos.X = XAxisCurve.Eval( CurTime, 0.f );
		NewPos.Y = YAxisCurve.Eval( CurTime, 0.f );
		NewPos.Z = ZAxisCurve.Eval( CurTime, 0.f );

		// Add the total length of this segment to the current total length.
		TotalLen += (NewPos - StartPos).Size();

		// Set up the start pos for the next segment to be the end of this segment
		StartPos = NewPos;

		// Add a new entry in the reparmeterized curve.
		OutReparameterizedCurve.AddPoint( TotalLen, CurTime );

		// Increment time
		CurTime += Interval;
	}

	return TotalLen;
}


/**
 * Removes keys from the specified move track if they are within the specified time range
 * 
 * @param Track	Track to remove keys from
 * @param StartTime The start of the time range.
 * @param End		The end of the time range
*/
static void ClearKeysInTimeRange( UInterpTrackMoveAxis* Track, float StartTime, float EndTime )
{
	for( int32 KeyIndex = Track->FloatTrack.Points.Num() - 1; KeyIndex >= 0 ; --KeyIndex )
	{
		float KeyTime = Track->FloatTrack.Points[ KeyIndex ].InVal;
		if( KeyTime >= StartTime && KeyTime <= EndTime )
		{
			// This point is in the time range, remove it.
			Track->FloatTrack.Points.RemoveAt( KeyIndex );
			// Since there must be an equal number of lookup track keys we must remove the key from the lookup track at the same index.
			Track->LookupTrack.Points.RemoveAt( KeyIndex );
		}
	}
}

/** Called when a user selects the normalize velocity option on a movement track */
void FMatinee::NormalizeVelocity()
{
	check( HasATrackSelected( UInterpTrackMove::StaticClass() ) );

	UInterpTrackMove* MoveTrack = *GetSelectedTrackIterator<UInterpTrackMove>();

	if( MoveTrack->SubTracks.Num() > 0 )
	{
		// Find the group instance for this move track 
		UInterpGroup* Group = MoveTrack->GetOwningGroup();
		UInterpGroupInst* GrInst = MatineeActor->FindFirstGroupInst( Group );
		check(GrInst);

		// Find the track instance which is needed to reduce keys
		UInterpTrackInst* TrackInst = NULL;
		int32 MoveTrackIndex = Group->InterpTracks.Find( MoveTrack );
		check(MoveTrackIndex != INDEX_NONE);

		TrackInst = GrInst->TrackInst[ MoveTrackIndex ];
		check(TrackInst);

		// Get this movment track's subtracks.
		UInterpTrackMoveAxis* XAxisTrack = CastChecked<UInterpTrackMoveAxis>( MoveTrack->SubTracks[AXIS_TranslationX] );
		UInterpTrackMoveAxis* YAxisTrack = CastChecked<UInterpTrackMoveAxis>( MoveTrack->SubTracks[AXIS_TranslationY] );
		UInterpTrackMoveAxis* ZAxisTrack = CastChecked<UInterpTrackMoveAxis>( MoveTrack->SubTracks[AXIS_TranslationZ] );

		// Get each curve.
		FInterpCurveFloat& XAxisCurve = XAxisTrack->FloatTrack;
		FInterpCurveFloat& YAxisCurve = YAxisTrack->FloatTrack;
		FInterpCurveFloat& ZAxisCurve = ZAxisTrack->FloatTrack;
		
		// The start and end time of the segment we are modifying
		float SegmentStartTime = 0.0f;
		float SegmentEndTime = 0.0f;

		// The start and end time of the full track lenth
		float FullStartTime = 0.0f;
		float FullEndTime = 0.0f;

		// Get the full time range
		MoveTrack->GetTimeRange( FullStartTime, FullEndTime );

		TSharedRef<SWindow> NewWindow = SNew(SWindow)
			.Title(NSLOCTEXT("Matinee.KeyReduction", "Title", "Key Reduction"))
			.SizingRule( ESizingRule::Autosized )
			.SupportsMinimize(false)
			.SupportsMaximize(false);

		TSharedRef<SMatineeNormalizeVelocity> Dialog = 
			SNew(SMatineeNormalizeVelocity, NewWindow);

		NewWindow->SetContent(
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush(TEXT("PropertyWindow.WindowBorder")) )
			[
				Dialog
			]);
		
		GEditor->EditorAddModalWindow(NewWindow);

		if( Dialog->bResult )
		{
			SegmentStartTime = Dialog->IntervalStart;
			SegmentEndTime = Dialog->IntervalEnd;
			
			// Make sure the user didnt enter any invalid values
			FMath::Clamp(SegmentStartTime, FullStartTime, FullEndTime );
			FMath::Clamp(SegmentEndTime, FullStartTime, FullEndTime );

			// If we have a valid start and end time, normalize the track
			if( SegmentStartTime != SegmentEndTime )
			{
				InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "NormalizeVelocity", "Normalize Velocity") );

				FInterpCurveFloat ReparameterizedCurve;
				float TotalLen = ReparameterizeCurve( MoveTrack, FullStartTime, FullEndTime, ReparameterizedCurve );

				MoveTrack->Modify();
				XAxisTrack->Modify();
				YAxisTrack->Modify();
				ZAxisTrack->Modify();
			
				const float TotalTime = FullEndTime - FullStartTime;
				const int32 NumSteps = FMath::CeilToInt(TotalTime/(1.0f/60.0f));
				const float Interval = (SegmentEndTime-SegmentStartTime)/NumSteps; 

				// An array of points that were created in order to normalize velocity
				TArray< FInterpCurvePoint<FVector> > CreatedPoints;

				float Time = SegmentStartTime;
				for( int32 Step = 0; Step < NumSteps; ++Step )
				{
					// Determine how far along the curve we should be at the given time.
					float PctDone = Time/TotalTime;
					float TotalDistSoFar = TotalLen * PctDone;

					// Given the total distance along the curve that has been traversed so far, find the actual time where we should evaluate the original curve.
					float NewTime = ReparameterizedCurve.Eval( TotalDistSoFar, 0.f );

					// Evaluate the curve given the new time and create a new point
					FInterpCurvePoint<FVector> Point;
					Point.InVal = Time;
					Point.OutVal.X = XAxisCurve.Eval( NewTime, 0.0f );
					Point.OutVal.Y = YAxisCurve.Eval( NewTime, 0.0f );
					Point.OutVal.Z = ZAxisCurve.Eval( NewTime, 0.0f );
					Point.InterpMode = CIM_CurveAuto;
					Point.ArriveTangent = FVector::ZeroVector;
					Point.LeaveTangent = FVector::ZeroVector;

					CreatedPoints.Add( Point );

					// Increment time
					Time+=Interval;
				}

				// default name for lookup track keys
				FName DefaultName(NAME_None);

				// If we didnt start at the beginning add a key right before the modification.  This preserves the part we dont modify.
				if( SegmentStartTime > FullStartTime )
				{
					float KeyTime = SegmentStartTime - 0.01f;

					FInterpCurvePoint<FVector> PointToAdd;
					PointToAdd.InVal = KeyTime;
					PointToAdd.OutVal.X = XAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Y = YAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Z = ZAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.ArriveTangent = FVector::ZeroVector;
					PointToAdd.LeaveTangent = PointToAdd.ArriveTangent;
					PointToAdd.InterpMode = CIM_CurveAuto;
	
					CreatedPoints.Add( PointToAdd );
				}

				// If we didnt stop at the end of the track add a key right after the modification.  This preserves the part we dont modify.
				if( SegmentEndTime < FullEndTime )
				{
					float KeyTime = SegmentEndTime + 0.01f;

					FInterpCurvePoint<FVector> PointToAdd;
					PointToAdd.InVal = KeyTime;
					PointToAdd.OutVal.X = XAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Y = YAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.OutVal.Z = ZAxisCurve.Eval( KeyTime, 0.0f );
					PointToAdd.ArriveTangent = FVector::ZeroVector;
					PointToAdd.LeaveTangent = PointToAdd.ArriveTangent;
					PointToAdd.InterpMode = CIM_CurveAuto;

					CreatedPoints.Add( PointToAdd );
				}

				// Empty all points in the time range for each axis curve.  Normalized velocity means the original points are now invalid.
				ClearKeysInTimeRange( XAxisTrack, SegmentStartTime, SegmentEndTime );
				ClearKeysInTimeRange( YAxisTrack, SegmentStartTime, SegmentEndTime );
				ClearKeysInTimeRange( ZAxisTrack, SegmentStartTime, SegmentEndTime );

				// Add each created point to each curve
				for( int32 I = 0; I < CreatedPoints.Num(); ++I )
				{
					// Created points are vectors so we must split them into their individual components.
					const FInterpCurvePoint<FVector>& CreatedPoint = CreatedPoints[I];

					// X Axis
					{
						int32 Index = XAxisCurve.AddPoint( CreatedPoint.InVal, CreatedPoint.OutVal.X );
						FInterpCurvePoint<float>& AddedPoint = XAxisCurve.Points[ Index ];

						AddedPoint.InterpMode = CreatedPoint.InterpMode;
						AddedPoint.ArriveTangent = CreatedPoint.ArriveTangent.X;
						AddedPoint.LeaveTangent = CreatedPoint.LeaveTangent.X;

						// Add a point to each lookup track since the curve and the lookup track must have an equal number of points.
						XAxisTrack->LookupTrack.AddPoint( CreatedPoint.InVal, DefaultName );
					}

					// Y Axis
					{
						int32 Index = YAxisCurve.AddPoint( CreatedPoint.InVal, CreatedPoint.OutVal.Y );
						FInterpCurvePoint<float>& AddedPoint = YAxisCurve.Points[ Index ];

						AddedPoint.InterpMode = CreatedPoint.InterpMode;
						AddedPoint.ArriveTangent = CreatedPoint.ArriveTangent.Y;
						AddedPoint.LeaveTangent = CreatedPoint.LeaveTangent.Y;

						// Add a point to each lookup track since the curve and the lookup track must have an equal number of points.
						YAxisTrack->LookupTrack.AddPoint( CreatedPoint.InVal, DefaultName );
					}

					// Z Axis
					{
						int32 Index = ZAxisCurve.AddPoint( CreatedPoint.InVal, CreatedPoint.OutVal.Z );
						FInterpCurvePoint<float>& AddedPoint = ZAxisCurve.Points[ Index ];

						AddedPoint.InterpMode = CreatedPoint.InterpMode;
						AddedPoint.ArriveTangent = CreatedPoint.ArriveTangent.Y;
						AddedPoint.LeaveTangent = CreatedPoint.LeaveTangent.Y;

						// Add a point to each lookup track since the curve and the lookup track must have an equal number of points.
						ZAxisTrack->LookupTrack.AddPoint( CreatedPoint.InVal, DefaultName );
					}
				}

				// Calculate tangents
				XAxisCurve.AutoSetTangents();
				YAxisCurve.AutoSetTangents();
				ZAxisCurve.AutoSetTangents();

				// Reduce the number of keys we created as there were probably too many.
				ReduceKeysForTrack( XAxisTrack, TrackInst, SegmentStartTime, SegmentEndTime, 1.0f );
				ReduceKeysForTrack( YAxisTrack, TrackInst, SegmentStartTime, SegmentEndTime, 1.0f );
				ReduceKeysForTrack( ZAxisTrack, TrackInst, SegmentStartTime, SegmentEndTime, 1.0f );

				InterpEdTrans->EndSpecial();
			}
		}
	}
}

void FMatinee::ScaleTranslationByAmount( const FText& InText, ETextCommit::Type CommitInfo, UInterpTrackMove* MoveTrack )
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		CloseEntryPopupMenu();
		float Amount = FCString::Atof(*InText.ToString());
		const bool bIsNumber = InText.IsNumeric();
		if (bIsNumber)
		{
			InterpEdTrans->BeginSpecial( NSLOCTEXT("UnrealEd", "ScaleTranslation", "Scale Translation") );
			MoveTrack->Modify();

			if (MoveTrack->SubTracks.Num() > 0)
			{
				// Get this movment track's subtracks.
				UInterpTrackMoveAxis* XAxisTrack = CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[AXIS_TranslationX]);
				UInterpTrackMoveAxis* YAxisTrack = CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[AXIS_TranslationY]);
				UInterpTrackMoveAxis* ZAxisTrack = CastChecked<UInterpTrackMoveAxis>(MoveTrack->SubTracks[AXIS_TranslationZ]);

				// Get each curve.
				FInterpCurveFloat& XAxisCurve = XAxisTrack->FloatTrack;
				FInterpCurveFloat& YAxisCurve = YAxisTrack->FloatTrack;
				FInterpCurveFloat& ZAxisCurve = ZAxisTrack->FloatTrack;

				for (FInterpCurvePoint<float>& X : XAxisTrack->FloatTrack.Points)
				{
					X.OutVal *= Amount;
				}

				for (FInterpCurvePoint<float>& Y : YAxisTrack->FloatTrack.Points)
				{
					Y.OutVal *= Amount;
				}

				for (FInterpCurvePoint<float>& Z : ZAxisTrack->FloatTrack.Points)
				{
					Z.OutVal *= Amount;
				}
			}
			else
			{
				for (FInterpCurvePoint<FVector>& Pos : MoveTrack->PosTrack.Points)
				{
					Pos.OutVal *= Amount;
				}

			}
			InterpEdTrans->EndSpecial();
		}
	}
}
void FMatinee::ScaleMoveTrackTranslation( )
{
	check(HasATrackSelected(UInterpTrackMove::StaticClass()));

	UInterpTrackMove* MoveTrack = *GetSelectedTrackIterator<UInterpTrackMove>();

	if( MoveTrack )
	{
		// Display dialog and let user enter new time.
		GenericTextEntryModeless(
			NSLOCTEXT("Matinee.Popups", "NewScale", "New Scale"),
			FText::AsNumber(1.0f),
			FOnTextCommitted::CreateSP( this, &FMatinee::ScaleTranslationByAmount, MoveTrack)
			);
	}

}

/** Turn keyframe snap on/off. */
void FMatinee::OnToggleSnap()
{
	SetSnapEnabled( !IsSnapToggled() );
}


/** Updates UI state for 'snap keys' option */
bool FMatinee::IsSnapToggled()
{
	return bSnapEnabled;
}


/** Called when the 'snap time to frames' command is triggered from the GUI */
void FMatinee::OnToggleSnapTimeToFrames()
{
	SetSnapTimeToFrames( !IsSnapTimeToFramesToggled() );
}

/** Updates UI state for 'snap time to frames' option */
bool FMatinee::IsSnapTimeToFramesToggled()
{
	return bSnapToFrames && bSnapTimeToFrames;
}

bool FMatinee::IsSnapTimeToFramesEnabled()
{
	return bSnapToFrames;
}


/** Called when the 'fixed time step playback' command is triggered from the GUI */
void FMatinee::OnFixedTimeStepPlaybackCommand()
{
	SetFixedTimeStepPlayback( !IsFixedTimeStepPlaybackToggled() );
}

/** Updates UI state for 'fixed time step playback' option */
bool FMatinee::IsFixedTimeStepPlaybackToggled()
{
	return bSnapToFrames && bFixedTimeStepPlayback;
}

bool FMatinee::IsFixedTimeStepPlaybackEnabled()
{
	return bSnapToFrames;
}

/** Called when the 'prefer frame numbers' command is triggered from the GUI */
void FMatinee::OnPreferFrameNumbersCommand()
{
	SetPreferFrameNumbers( !IsPreferFrameNumbersToggled() );
}



/** Updates UI state for 'prefer frame numbers' option */
bool FMatinee::IsPreferFrameNumbersToggled()
{
	return bSnapToFrames && bPreferFrameNumbers;
}

bool FMatinee::IsPreferFrameNumbersEnabled()
{
	return bSnapToFrames;
}

/** Called when the 'show time cursor pos for all keys' command is triggered from the GUI */
void FMatinee::OnShowTimeCursorPosForAllKeysCommand()
{
	SetShowTimeCursorPosForAllKeys( !IsShowTimeCursorPosForAllKeysToggled() );
}
/** Updates UI state for 'show time cursor pos for all keys' option */
bool FMatinee::IsShowTimeCursorPosForAllKeysToggled()
{
	return bShowTimeCursorPosForAllKeys;
}

/** The snap resolution combo box was changed. */
void FMatinee::OnChangeSnapSize( TSharedPtr< FString > SelectedString, ESelectInfo::Type SelectInfo )
{
	int32 NewSelection; // = InIndex;

	bool bFound = SnapComboStrings.Find(SelectedString, NewSelection);
	check(bFound);

	if(NewSelection == ARRAY_COUNT(InterpEdSnapSizes)+ARRAY_COUNT(InterpEdFPSSnapSizes))
	{
		bSnapToFrames = false;
		bSnapToKeys = true;
		SnapAmount = 1.0f / 30.0f;	// Shouldn't be used
		CurveEd->SetInSnap(false, SnapAmount, bSnapToFrames);
	}
	else if(NewSelection<ARRAY_COUNT(InterpEdSnapSizes))	// see if they picked a second snap amount
	{
		bSnapToFrames = false;
		bSnapToKeys = false;
		SnapAmount = InterpEdSnapSizes[NewSelection];
		CurveEd->SetInSnap(bSnapEnabled, SnapAmount, bSnapToFrames);
	}
	else if(NewSelection<ARRAY_COUNT(InterpEdFPSSnapSizes)+ARRAY_COUNT(InterpEdSnapSizes))	// See if they picked a FPS snap amount.
	{
		bSnapToFrames = true;
		bSnapToKeys = false;
		SnapAmount = InterpEdFPSSnapSizes[NewSelection-ARRAY_COUNT(InterpEdSnapSizes)];
		CurveEd->SetInSnap(bSnapEnabled, SnapAmount, bSnapToFrames);
	}

	SnapSelectionIndex = NewSelection;

	// Save selected snap mode to INI.
	GConfig->SetInt(TEXT("Matinee"), TEXT("SelectedSnapMode"), NewSelection, GEditorPerProjectIni );

	// Snap time to frames right now if we need to
	SetSnapTimeToFrames( bSnapTimeToFrames );

	// If 'fixed time step playback' is turned on, we also need to make sure the benchmarking time step
	// is set when this changes
	SetFixedTimeStepPlayback( bFixedTimeStepPlayback );

	// The 'prefer frame numbers' option requires bSnapToFrames to be enabled, so update it's state
	SetPreferFrameNumbers( bPreferFrameNumbers );

	// Make sure any particle replay tracks are filled in with the correct state
	UpdateParticleReplayTracks();

	// Update Tracks Windows
	UpdateTrackWindowScrollBars();
}



/**
 * Called when the initial curve interpolation mode for newly created keys is changed
 */
void FMatinee::OnChangeInitialInterpMode( TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo )
{ 
	int32 NewSelection;
	bool bFound = InitialInterpModeStrings.Find(ItemSelected, NewSelection);
	check(bFound);

	InitialInterpMode = ( EInterpCurveMode )NewSelection;

	// Save selected mode to user's preference file
	GConfig->SetInt( TEXT( "Matinee" ), TEXT( "InitialInterpMode2" ), NewSelection, GEditorPerProjectIni );
}



/** Adjust the view so the entire sequence fits into the viewport. */
void FMatinee::OnViewFitSequence()
{
	ViewFitSequence();
}

/** Adjust the view so the selected keys fit into the viewport. */
void FMatinee::OnViewFitToSelected()
{
	ViewFitToSelected();
}

/** Adjust the view so the looped section fits into the viewport. */
void FMatinee::OnViewFitLoop()
{
	ViewFitLoop();
}

/** Adjust the view so the looped section fits into the entire sequence. */
void FMatinee::OnViewFitLoopSequence()
{
	ViewFitLoopSequence();
}

/** Move the view to the end of the currently selected track(s). */
void FMatinee::OnViewEndOfTrack()
{
	ViewEndOfTrack();
}


/*-----------------------------------------------------------------------------
	Menu Bar
-----------------------------------------------------------------------------*/

void FMatinee::ExtendDefaultToolbarMenu()
{
	struct Local
	{
		static void FillFileMenu( FMenuBuilder& InMenuBarBuilder, FAssetEditorToolkit* AssetEditorToolkit )
		{
			InMenuBarBuilder.BeginSection("FileImportExport", NSLOCTEXT("Matinee", "ImportFileHeading", "Import/Export"));
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FileImport);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FileExport);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ExportSoundCueInfo);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ExportAnimInfo);
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("Export");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FileExportBakeTransforms);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FileExportKeepHierarchy);
			}
			InMenuBarBuilder.EndSection();
		}
		
		static void FillEditMenu( FMenuBuilder& InMenuBarBuilder )
		{
			InMenuBarBuilder.BeginSection("EditMatineeKeys", NSLOCTEXT("Matinee", "MatineeFileHeading.Keys", "Keys"));
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().DeleteSelectedKeys);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().DuplicateKeys);
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("EditMatineeSection", NSLOCTEXT("Matinee", "MatineeFileHeading.Section", "Section"));
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().InsertSpace);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().StretchSection);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().StretchSelectedKeyFrames);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().DeleteSection);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().SelectInSection);
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("EditMatineeReduce");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ReduceKeys);
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("EditMatineePathTime");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().SavePathTime);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().JumpToPathTime);
			}
			InMenuBarBuilder.EndSection();
		}

		static void AddPlaybackMenu(FMenuBarBuilder& InMenuBarBuilder, FMatinee* InMatinee)
		{
			InMenuBarBuilder.AddPullDownMenu(
				NSLOCTEXT("Matinee.Menus", "PlaybackMenu", "Playback"),
				FText::GetEmpty(),
				FNewMenuDelegate::CreateStatic(&Local::FillPlaybackMenu, InMatinee),
				"Playback");
		}

		static void FillPlaybackMenu(FMenuBuilder& InMenuBarBuilder, FMatinee* InMatinee)
		{
			InMenuBarBuilder.BeginSection("PlaybackSection");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().PlayPause);
				InMenuBarBuilder.AddMenuSeparator();
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().Play);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().PlayLoop);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().Stop);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().PlayReverse);
			}
			InMenuBarBuilder.EndSection();
		}
		
		static void AddViewMenu( FMenuBarBuilder& InMenuBarBuilder, FMatinee* InMatinee )
		{
			InMenuBarBuilder.AddPullDownMenu(
				NSLOCTEXT("Matinee.Menus", "ViewMenu", "View"),
				FText::GetEmpty(),
				FNewMenuDelegate::CreateStatic( &Local::FillViewMenu, InMatinee ),
				"View");
		}

		static void FillViewMenu( FMenuBuilder& InMenuBarBuilder, FMatinee* InMatinee )
		{
			InMenuBarBuilder.BeginSection("ViewDrawFlags", NSLOCTEXT("Matinee", "MatineeFileHeading.DrawFlags", "Draw"));
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().Draw3DTrajectories);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ShowAll3DTrajectories);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().HideAll3DTrajectories);
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("ViewSnap", NSLOCTEXT("Matinee", "MatineeFileHeading.Snap", "Snap"));
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ToggleSnap);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ToggleSnapTimeToFrames);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FixedTimeStepPlayback);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().PreferFrameNumbers);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ShowTimeCursorPosForAllKeys);
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("ViewMatinee", NSLOCTEXT("Matinee", "MatineeFileHeading.View", "View"));
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ZoomToTimeCursorPosition);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ViewFrameStats);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().EditingCrosshair);
				InMenuBarBuilder.AddSubMenu(NSLOCTEXT("Matinee.Menus", "EditingGridHeading", "Editing Grid"), FText::GetEmpty(), FNewMenuDelegate::CreateStatic(&Local::FillGridSubMenu, InMatinee), false, FSlateIcon(FEditorStyle::GetStyleSetName(), "ViewMatineeGrid"));
				InMenuBarBuilder.AddSubMenu(NSLOCTEXT("Matinee.Menus", "SafeFrameSettingsHeading", "Safe Frame Settings"), FText::GetEmpty(),FNewMenuDelegate::CreateStatic(&Local::FillSafeFrameSettings, InMatinee), false);
			}
			InMenuBarBuilder.EndSection();
			
			InMenuBarBuilder.BeginSection("ViewFit", NSLOCTEXT("Matinee", "MatineeFileHeading.Fit", "Fit"));
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FitSequence);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FitViewToSelected);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FitLoop);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().FitLoopSequence);
			}
			InMenuBarBuilder.EndSection();
			
			InMenuBarBuilder.BeginSection("MatineeMenusViewEndOfTrack");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ViewEndofTrack, "ViewEndOfTrack");
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("MatineeMenusGorePreview");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ToggleGorePreview, "ToggleGorePreview");
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("MatineeMenusPanInvert");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().TogglePanInvert, "TogglePanInvert");
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("ViewToggleKeyframe");
			{
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ToggleAllowKeyframeBarSelection);
				InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().ToggleAllowKeyframeTextSelection);
			}
			InMenuBarBuilder.EndSection();

			InMenuBarBuilder.BeginSection("ViewToggleLockCameraPitch");
			{
				InMenuBarBuilder.AddMenuEntry( FMatineeCommands::Get().ToggleLockCameraPitch, "ToggleLockCameraPitch" );
			}
			InMenuBarBuilder.EndSection();
		}

		static void FillGridSubMenu( FMenuBuilder&  InMenuBarBuilder, FMatinee* InMatinee )
		{
			InMenuBarBuilder.AddMenuEntry(FMatineeCommands::Get().EnableEditingGrid, "EnableEditingGrid");
			
			InMenuBarBuilder.BeginSection("GridSizes");
			for( uint32 GridSize = 1; GridSize <= 16; ++GridSize )
			{
				const FText MenuStr = FText::Format( LOCTEXT("SquareGridSize", "{0} x {0}"), FText::AsNumber( GridSize ) );
				InMenuBarBuilder.AddMenuEntry(MenuStr, FText(), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(InMatinee, &FMatinee::OnSetEditingGrid, GridSize),
						FCanExecuteAction::CreateSP(InMatinee, &FMatinee::IsEditingGridEnabled),
						FIsActionChecked::CreateSP(InMatinee, &FMatinee::IsEditingGridChecked, GridSize)
						),
					NAME_None,
					EUserInterfaceActionType::RadioButton
					);
			}
			InMenuBarBuilder.EndSection();
		}

		static void FillSafeFrameSettings( FMenuBuilder&  InMenuBarBuilder, FMatinee* InMatinee )
		{
			{
				FUIAction AspectRatioBarsAction( FExecuteAction::CreateSP( InMatinee, &FMatinee::OnToggleAspectRatioBars ), FCanExecuteAction(), FIsActionChecked::CreateSP( InMatinee, &FMatinee::AreAspectRatioBarsEnabled ) );
				InMenuBarBuilder.AddMenuEntry
				(
					NSLOCTEXT("Matinee", "ShowCameraAspectRatioBars", "Enable Aspect Ratio Bars"), 
					NSLOCTEXT("Matinee", "ShowCameraAspectRatioBars_ToolTip", "Toggles displaying black bars to simulate constraining the camera aspect ratio"),
					FSlateIcon(),
					AspectRatioBarsAction,
					NAME_None,
					EUserInterfaceActionType::ToggleButton
				);
			}

			{
				FUIAction SafeFrameAction( FExecuteAction::CreateSP( InMatinee, &FMatinee::OnToggleSafeFrames ), FCanExecuteAction(), FIsActionChecked::CreateSP( InMatinee, &FMatinee::IsSafeFrameDisplayEnabled ) );
				InMenuBarBuilder.AddMenuEntry
				( 
					NSLOCTEXT("Matinee", "EnableSafeFrames", "Enable Safe Frames"), 
					NSLOCTEXT("Matinee", "EnableSafeFrames_ToolTip", "Toggles safe frame display in all matinee controlled viewports when a camera is selected"),
					FSlateIcon(),
					SafeFrameAction,
					NAME_None,
					EUserInterfaceActionType::ToggleButton 
				);
			}
		}

	};
	
	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);

	// Add asset-specific menu items to the top of the "File" menu
	MenuExtender->AddMenuExtension(
		"FileLoadAndSave",
		EExtensionHook::Before,
		GetToolkitCommands(), 
		FMenuExtensionDelegate::CreateStatic( &Local::FillFileMenu, (FAssetEditorToolkit*)this )
		);

	MenuExtender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		GetToolkitCommands(), 
		FMenuExtensionDelegate::CreateStatic( &Local::FillEditMenu )
		);

	MenuExtender->AddMenuBarExtension(
		"Edit",
		EExtensionHook::After,
		GetToolkitCommands(),
		FMenuBarExtensionDelegate::CreateStatic(&Local::AddPlaybackMenu, this)
		);

	MenuExtender->AddMenuBarExtension(
		"Edit",
		EExtensionHook::After,
		GetToolkitCommands(), 
		FMenuBarExtensionDelegate::CreateStatic( &Local::AddViewMenu, this )
		);
	
	AddMenuExtender(MenuExtender);

	IMatineeModule* MatineeModule = &FModuleManager::LoadModuleChecked<IMatineeModule>( "Matinee" );
	AddMenuExtender(MatineeModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

/*-----------------------------------------------------------------------------
	TabMenu
-----------------------------------------------------------------------------*/

TSharedPtr<SWidget> FMatinee::CreateTabMenu()
{
	// only show a context menu for custom filters
	UInterpFilter_Custom* Filter = Cast<UInterpFilter_Custom>(IData->SelectedFilter);
	if(Filter && IData->InterpFilters.Contains(Filter)) // make sure this isn't a default filter; if we add more entries this check only affects GroupDeleteTab
	{
		FMenuBuilder MenuBuilder( true, ToolkitCommands );
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().GroupDeleteTab);
		return MenuBuilder.MakeWidget();
	}

	return nullptr;
}

/*-----------------------------------------------------------------------------
	GroupMenu
-----------------------------------------------------------------------------*/

TSharedPtr<SWidget> FMatinee::CreateGroupMenu()
{
	FMenuBuilder MenuBuilder(true, ToolkitCommands);

	// If no group is selected, then this menu should 
	// not have been created in the first place.
	check( HasAGroupSelected() );

	const int32 SelectedGroupCount = GetSelectedGroupCount();
	const bool bHasOneGroupSelected = (SelectedGroupCount == 1);

	// Certain menu options are only available if only one group is selected. 
	if( bHasOneGroupSelected )
	{
		UInterpGroup* SelectedGroup = *GetSelectedGroupIterator();
		check (GetMatineeActor());
		const bool bIsFolder = SelectedGroup->bIsFolder;
		const bool bIsDirGroup = SelectedGroup->IsA(UInterpGroupDirector::StaticClass());

		if ( !bIsDirGroup && !bIsFolder )
		{
			UInterpGroupInst* GrInst = GetMatineeActor()->FindFirstGroupInst(SelectedGroup);
			check(GrInst);
			if (GrInst)
			{
				struct ActorManageMenu
				{
					static void Create(FMenuBuilder& InMenuBuilder, FMatinee* InMatinee, UInterpGroup* InSelectedGroup)
					{
						bool bSelectionExists = GEditor->GetSelectedActorCount() > 0;

						InMenuBuilder.BeginSection("MatineeMenusActorSelection");
						{
							InMenuBuilder.AddMenuEntry(FMatineeCommands::Get().ActorSelectAll);

							if (bSelectionExists)
							{
								InMenuBuilder.AddMenuEntry(FMatineeCommands::Get().ActorAddAll);
								InMenuBuilder.AddMenuEntry(FMatineeCommands::Get().ActorReplaceAll);
							}

							InMenuBuilder.AddMenuEntry(FMatineeCommands::Get().ActorRemoveAll);
						}
						InMenuBuilder.EndSection();

						// add actor listing first, use I as actor index for menu
						// it can sparse
						bool OnlyOneSelected = (GEditor->GetSelectedActorCount() == 1);
						for (int32 I=0; I< InMatinee->GetMatineeActor()->GroupInst.Num(); ++I)
						{
							UInterpGroupInst* IterGrInst = InMatinee->GetMatineeActor()->GroupInst[I];

							if ( IterGrInst && IterGrInst->Group == InSelectedGroup && IterGrInst->GetGroupActor() )
							{
								AActor* GrActor = IterGrInst->GetGroupActor();
								// right now it only allows 1000 indexing. If more, we'll get trouble
								if (ensure(I<1000))
								{
									struct ActorGroupMenu
									{
										static void Create(FMenuBuilder& In_MenuBuilder, FMatinee* In_Matinee, bool bOnlyOneSelected, int32 Index)
										{
											In_MenuBuilder.AddMenuEntry( NSLOCTEXT("Matinee.Menus", "ContextMenu.Group.SelectActor", "Select Actor"), FText::GetEmpty(), FSlateIcon(),
												FUIAction(FExecuteAction::CreateSP(In_Matinee, &FMatinee::OnContextSelectActor, Index))
												);

											In_MenuBuilder.AddMenuEntry( NSLOCTEXT("Matinee.Menus", "ContextMenu.Group.GotoActor", "Goto Actor"), FText::GetEmpty(), FSlateIcon(),
												FUIAction(FExecuteAction::CreateSP(In_Matinee, &FMatinee::OnContextGotoActors, Index))
												);

											// don't give option if more than 1 selected
											if (bOnlyOneSelected)
											{
												In_MenuBuilder.AddMenuEntry( NSLOCTEXT("Matinee.Menus", "ContextMenu.Group.ReplaceActor", "Replace Actor"), FText::GetEmpty(), FSlateIcon(),
													FUIAction(FExecuteAction::CreateSP(In_Matinee, &FMatinee::OnContextReplaceActor, Index))
													);
											}
											In_MenuBuilder.AddMenuEntry( NSLOCTEXT("Matinee.Menus", "ContextMenu.Group.RemoveActor", "Remove Actor"), FText::GetEmpty(), FSlateIcon(),
												FUIAction(FExecuteAction::CreateSP(In_Matinee, &FMatinee::OnContextRemoveActors, Index))
												);
										}
									};

									InMenuBuilder.BeginSection("MatineeMenusGroup");
									{
										FFormatNamedArguments Args;
										Args.Add( TEXT("ActorDisplayName"), FText::FromString( GrActor->GetActorLabel() ) );
										Args.Add( TEXT("ActorName"), FText::FromString( GrActor->GetName() ) );

										// add menu for actor
										InMenuBuilder.AddSubMenu( 
											FText::Format( LOCTEXT("ActorSubMenu", "{ActorDisplayName}({ActorName})"), Args ),
											FText(),
											FNewMenuDelegate::CreateStatic(&ActorGroupMenu::Create, InMatinee, OnlyOneSelected, I)
											);
									}
									InMenuBuilder.EndSection();
								}
							}
						}
					}
				};

				// Alt. Bone Weight Track editing
				MenuBuilder.BeginSection("MatineeMenusActors");
				{
					MenuBuilder.AddSubMenu( 
						NSLOCTEXT("Matinee.Menus", "GroupMenu.ActorSubMenu", "Actors"), FText::GetEmpty(), 
						FNewMenuDelegate::CreateStatic(&ActorManageMenu::Create, this, SelectedGroup)
						);
				}
				MenuBuilder.EndSection();
			}
		}

		// When we have only one group selected and it's not a folder, 
		// then we can create tracks on the selected group.
		if( !bIsFolder )
		{
			MenuBuilder.BeginSection("MatineeMenusContextNewTrack");
			{
				for ( UClass* TrackClass : InterpTrackClasses )
				{
					UInterpTrack* DefTrack = TrackClass->GetDefaultObject<UInterpTrack>();
					if ( !DefTrack->bDirGroupOnly && !DefTrack->bSubTrackOnly )
					{
						FText NewTrackText = FText::Format(NSLOCTEXT("UnrealEd", "AddNew_F", "Add New {0}"), FText::FromString(TrackClass->GetDescription()));
						MenuBuilder.AddMenuEntry(
							NewTrackText,
							FText::GetEmpty(),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateSP(this, &FMatinee::OnContextNewTrack, TrackClass), FCanExecuteAction::CreateSP(this, &FMatinee::CanCreateNewTrack, TrackClass))
							);

					}
				}
			}
			MenuBuilder.EndSection();

			//TMap<FString, TArray<UClass*>> TrackGroups;

			//for ( UClass* TrackClass : InterpTrackClasses)
			//{
			//	UInterpTrack* DefTrack = TrackClass->GetDefaultObject<UInterpTrack>();
			//	if ( !DefTrack->bDirGroupOnly && !DefTrack->bSubTrackOnly )
			//	{
			//		const FString& DisplayGroup = TrackClass->GetMetaData("DisplayGroup");
			//		TrackGroups.FindOrAdd(DisplayGroup).Add(TrackClass);
			//	}
			//}

			//TArray<FString> GroupKeys;
			//TrackGroups.GetKeys(GroupKeys);
			//GroupKeys.Sort();

			//for ( FString& GroupKey : GroupKeys )
			//{
			//	const TArray<UClass*>& Group = TrackGroups.FindRef(GroupKey);
			//	FText GroupText = FText::Format(FText::FromString("Add {0} Track"), FText::FromString(GroupKey));

			//	MenuBuilder.BeginSection(NAME_None, GroupText);
			//	{
			//		for ( UClass* TrackClass : Group )
			//		{
			//			FText NewTrackText = FText::FromString(TrackClass->GetDescription());
			//			MenuBuilder.AddMenuEntry(
			//				NewTrackText,
			//				FText::GetEmpty(),
			//				FSlateIcon(),
			//				FUIAction(FExecuteAction::CreateSP(this, &FMatinee::OnContextNewTrack, TrackClass), FCanExecuteAction::CreateSP(this, &FMatinee::CanCreateNewTrack, TrackClass))
			//				);
			//		}
			//	}
			//	MenuBuilder.EndSection();
			//}
		}


		// Add Director-group specific tracks to separate menu underneath.
		if( bIsDirGroup )
		{
			MenuBuilder.BeginSection("MatineeMenusContextNewTrack");
			{
				for ( UClass* TrackClass : InterpTrackClasses )
				{
					UInterpTrack* DefTrack = TrackClass->GetDefaultObject<UInterpTrack>();
					if(DefTrack->bDirGroupOnly)
					{
						FText NewTrackText = FText::Format(NSLOCTEXT("UnrealEd", "AddNew_F", "Add New {0}"), FText::FromString(TrackClass->GetDescription()));
						MenuBuilder.AddMenuEntry( 
							NewTrackText, 
							NewTrackText,
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateSP(this, &FMatinee::OnContextNewTrack, TrackClass))
							);
					}
				}
			}
			MenuBuilder.EndSection();
		}

		// Add CameraAnim export option if appropriate
		if ( !bIsDirGroup && !bIsFolder )
		{
			UInterpGroupInst* GrInst = GetMatineeActor()->FindFirstGroupInst(SelectedGroup);
			check(GrInst);
			if (GrInst)
			{
				AActor* const GroupActor = GrInst->GetGroupActor();
				bool bControllingACameraActor = GroupActor && GroupActor->IsA(ACameraActor::StaticClass());
				if (bControllingACameraActor)
				{
					// add strings to unrealed.int
					MenuBuilder.BeginSection("MatineeMenusExportCameraAnim");
					{
						MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ExportCameraAnim );
					}
					MenuBuilder.EndSection();
				}
			}
		}

		if( SelectedGroup->HasAnimControlTrack() )
		{
			// Add menu item to export group animations to fbx
			// Should be very similar to the anim control track right click menu 
			MenuBuilder.BeginSection("MatineeMenusExportAnimGroupFBX");
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ExportAnimGroupFBX );
			}
			MenuBuilder.EndSection();
		}
	}

	const bool bHasAFolderSelected = HasAFolderSelected();
	const bool bHasADirectorSelected = HasAGroupSelected(UInterpGroupDirector::StaticClass());

	// Copy/Paste not supported on folders yet
	if( !bHasAFolderSelected )
	{
		MenuBuilder.BeginSection("MatineeMenusEdit");
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().EditCut );
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().EditCopy );
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().EditPaste );
		}
		MenuBuilder.EndSection();
	}

	FText RenameText = LOCTEXT("MatineeRenameGroup", "Rename Group");
	FText DeleteText = LOCTEXT("MatineeDeleteGroup", "Delete Group");

	if( bHasAFolderSelected )
	{
		if( AreAllSelectedGroupsFolders() )
		{
			RenameText = LOCTEXT("MatineeRenameFolder", "Rename Folder");
			DeleteText = LOCTEXT("MatineeDeleteFolder", "Delete Folder");
		}
		else
		{
			RenameText = LOCTEXT("MatineeRenameFolderAndGroup", "Rename Folder And Group");
			DeleteText = LOCTEXT("MatineeDeleteFolderAndGroup", "Delete Folder And Group");
		}
	}

	MenuBuilder.AddMenuEntry( RenameText, FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FMatinee::OnContextGroupRename))
		);

	// Cannot duplicate Director groups or folders
	if( !bHasADirectorSelected && !bHasAFolderSelected )
	{
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().GroupDuplicate );
	}

	MenuBuilder.AddMenuEntry( DeleteText, FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FMatinee::OnContextGroupDelete))
		);

	bool bPotentialParentFoldersMenu = false;
	bool bPotentialChildGroupsMenu = false;

	struct PotentialParentFoldersMenu
	{
		static void Create( FMenuBuilder& InMenuBuilder, FMatinee* InMatinee, TArray<FInterpGroupParentInfo> InMasterFolderArray )
		{
			for( TArray<FInterpGroupParentInfo>::TIterator ParentIter(InMasterFolderArray); ParentIter; ++ParentIter )
			{
				FInterpGroupParentInfo& CurrentParent = *ParentIter;
				InMenuBuilder.AddMenuEntry(
					FText::FromString( CurrentParent.Group->GroupName.ToString() ),
					FText::GetEmpty(),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP( InMatinee, &FMatinee::OnContextGroupChangeGroupFolder, FMatineeCommands::EGroupAction::MoveActiveGroupToFolder, CurrentParent.GroupIndex))
					);
			}
		}
	};

	struct PotentialChildGroupsMenu
	{
		static void Create( FMenuBuilder& InMenuBuilder, FMatinee* InMatinee)
		{
			FInterpGroupParentInfo SelectedGroupInfo(*InMatinee->GetSelectedGroupIterator());

			// @todo: If more than 1000 groups exist in the data set, this limit will start to cause us problems
			const int32 MaxAllowedGroupIndex = 1000; 

			for( FGroupIterator GroupIter(InMatinee->GetGroupIterator()); GroupIter; ++GroupIter )
			{
				FInterpGroupParentInfo CurrentGroupInfo = InMatinee->GetParentInfo(*GroupIter);


				if( CurrentGroupInfo.GroupIndex > MaxAllowedGroupIndex )
				{
					// We've run out of space in the sub menu (no more resource IDs!).  Oh well, we won't display these items.
					// Since we are iterating incrementally, all groups after this can't be added either. So, break out of the loop.
					break;
				}

				// If the current group can be re-parented by the only selected group, then 
				// we can add an option to move the current group into the selected folder.
				if( InMatinee->CanReparent(CurrentGroupInfo, SelectedGroupInfo) )
				{
					InMenuBuilder.AddMenuEntry( 
						FText::FromString( CurrentGroupInfo.Group->GroupName.ToString() ),
						FText::GetEmpty(),
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateSP(InMatinee, &FMatinee::OnContextGroupChangeGroupFolder, FMatineeCommands::EGroupAction::MoveGroupToActiveFolder, CurrentGroupInfo.GroupIndex)
							)
						);

				}
			}
		}
	};

	TArray<FInterpGroupParentInfo> MasterFolderArray;

	// If only one group is selectd and that group is a folder, then 
	// we can setup a sub-menu to move selected groups to the folder.
	if( bHasOneGroupSelected && bHasAFolderSelected )
	{
		bPotentialChildGroupsMenu = true;
	}
	// Else, we have may have multiple groups selected. Attempt to setup a 
	// sub-menu for moving the selected groups to all the potential folders.
	else
	{
		
		//TArray<FInterpGroupParentInfo> MasterFolderArray;

		// @todo: If more than 1000 groups exist in the data set, this limit will start to cause us problems
		const int32 MaxAllowedGroupIndex = 1000; 

		for( FSelectedGroupIterator SelectedGroupIter(GetSelectedGroupIterator()); SelectedGroupIter; ++SelectedGroupIter )
		{
			FInterpGroupParentInfo SelectedGroupInfo = GetParentInfo(*SelectedGroupIter);

			// We have to compare the current selected group to each existing group to find all potential folders to move to.
			for( FGroupIterator GroupIter(GetGroupIterator()); GroupIter; ++GroupIter )
			{
				FInterpGroupParentInfo CurrentGroupInfo = GetParentInfo(*GroupIter);

				if( CurrentGroupInfo.GroupIndex > MaxAllowedGroupIndex )
				{
					// We've run out of space in the sub menu (no more resource IDs!).  Oh well, we won't display these items.
					// Since we are iterating incrementally, all groups after this can't be added either. So, break out of the loop.
					break;
				}

				// If we can re-parent the selected group to be parented by the current 
				// group, then the current group is a potential folder to move to.
				if( CanReparent(SelectedGroupInfo, CurrentGroupInfo) )
				{
					MasterFolderArray.AddUnique( CurrentGroupInfo );
				}
			}
		}

		// If we have folders that all selected groups can move to, add a sub-menu for that!
		if( MasterFolderArray.Num() )
		{	
			bPotentialParentFoldersMenu = true;
		}
	}

	bool bAddedFolderMenuItem = false;

	MenuBuilder.BeginSection("MatineeMenusMoveRemove");
	{
		if (bPotentialParentFoldersMenu)
		{
			MenuBuilder.AddSubMenu( 
				NSLOCTEXT("Matinee.Menus", "Context.Group.MoveGroupIntoFolder", "Move Group Into Folder"),
				FText::GetEmpty(),
				FNewMenuDelegate::CreateStatic(&PotentialParentFoldersMenu::Create, this, MasterFolderArray)
				);

			bAddedFolderMenuItem = true;
		}

		if (bPotentialChildGroupsMenu)
		{
			MenuBuilder.AddSubMenu( 
				NSLOCTEXT("Matinee.Menus", "Context.Group.MoveGroupIntoFolder", "Move Group Into Folder"),
				FText::GetEmpty(),
				FNewMenuDelegate::CreateStatic(&PotentialChildGroupsMenu::Create, this)
				);

			bAddedFolderMenuItem = true;
		}

		// If the group is parented, then add an option to remove it from the group folder its in
		if( AreAllSelectedGroupsParented() )
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().RemoveFromGroupFolder );
			bAddedFolderMenuItem = true;
		}
	}
	MenuBuilder.EndSection();

	if( !bHasAFolderSelected )
	{
		MenuBuilder.BeginSection("MatineeMenusGroupAddRemove");
		{
			// Add entries for creating and sending to tabs.
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().GroupCreateTab );

			// See if the user can remove this group from the current tab.
			UInterpFilter* Filter = Cast<UInterpFilter_Custom>(IData->SelectedFilter);
			if(Filter != NULL && HasAGroupSelected() && IData->InterpFilters.Contains(Filter))
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().GroupRemoveFromTab );
			}

			if(GetMatineeActor()->MatineeData->InterpFilters.Num())
			{
				struct TabMenu
				{
					static void Create( FMenuBuilder& InMenuBuilder, FMatinee *InMatinee )
					{
						for(int32 FilterIdx=0; FilterIdx < InMatinee->IData->InterpFilters.Num(); FilterIdx++)
						{
							UInterpFilter* InterpFilter = InMatinee->IData->InterpFilters[FilterIdx];
						
							InMenuBuilder.AddMenuEntry(
								FText::FromString( InterpFilter->Caption ),
								FText(),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateSP(InMatinee,&FMatinee::OnContextGroupSendToTab,FilterIdx))
								);
						}		
					}
				};
			
				MenuBuilder.AddSubMenu( 
					NSLOCTEXT("Matinee.Menus", "Context.Group.SendToGroupTab", "Add To Group Tab"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateStatic(&TabMenu::Create, this)
					);
			}
		}
		MenuBuilder.EndSection(); //MatineeMenusGroupAddRemove
	}

	return MenuBuilder.MakeWidget();
}

/*-----------------------------------------------------------------------------
	TrackMenu
-----------------------------------------------------------------------------*/

TSharedPtr<SWidget> FMatinee::CreateTrackMenu()
{
	FMenuBuilder MenuBuilder(true, ToolkitCommands);

	// Must have a track selected to create this menu
	check( HasATrackSelected() );
	
	const bool bOnlyOneTrackSelected = (GetSelectedTrackCount() == 1);

	UInterpTrack* Track = *GetSelectedTrackIterator();
	
	MenuBuilder.BeginSection("MatineeMenusTrackEdit");
	{
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().EditCut);
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().EditCopy);
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().EditPaste);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection("MatineeMenusTrackRenameDelete");
	{
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().TrackRename);
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().TrackDelete);
	}
	MenuBuilder.EndSection();

	// These menu commands are only accessible if only one track is selected.
	if( bOnlyOneTrackSelected )
	{
		if( Track->IsA(UInterpTrackAnimControl::StaticClass()) )
		{
			MenuBuilder.BeginSection("MatineeMenusExportAnimTrackFBX");
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ExportAnimTrackFBX );
			}
			MenuBuilder.EndSection();
		}
		else if( Track->IsA(UInterpTrackMove::StaticClass()) )
		{
			UInterpTrackMove* MoveTrack = CastChecked<UInterpTrackMove>(Track);

			MenuBuilder.BeginSection("MatineeMenusTrajectory");
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().Show3DTrajectory );
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ShowAll3DTrajectories );
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().HideAll3DTrajectories );
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("MatineeMenusTrackSplitNormalize");
			{
				if( MoveTrack->SubTracks.Num() == 0 )
				{
					MenuBuilder.AddMenuEntry( FMatineeCommands::Get().TrackSplitTransAndRot);
				}
				else
				{
					// Normalizing velocity is only possible for split tracks.
					MenuBuilder.AddMenuEntry( FMatineeCommands::Get().TrackNormalizeVelocity);
				}

				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ScaleTranslation );
			}


			MenuBuilder.EndSection();
		}
		// If this is a Particle Replay track, add buttons for toggling Capture Mode
		else if( Track->IsA(UInterpTrackParticleReplay::StaticClass()) )
		{
			UInterpTrackParticleReplay* ParticleTrack = CastChecked<UInterpTrackParticleReplay>(Track);

			MenuBuilder.BeginSection("MatineeMenusParticleReplay");
			{
				if( ParticleTrack->bIsCapturingReplay )
				{
					MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ParticleReplayTrackContextStopRecording );
				}
				else
				{
					MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ParticleReplayTrackContextStartRecording );
				}
			}
			MenuBuilder.EndSection();
		}
	}

	return MenuBuilder.MakeWidget();
}

/*-----------------------------------------------------------------------------
	BkgMenu
-----------------------------------------------------------------------------*/

//SMBMatineeBkgMenu::SMBMatineeBkgMenu(FMatinee* InterpEd)
TSharedPtr<SWidget> FMatinee::CreateBkgMenu(bool bIsDirectorTrackWindow)
{
	FMenuBuilder MenuBuilder( true, ToolkitCommands );

	MenuBuilder.BeginSection("MatineeMenusBkgEdit");
	{
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().EditPaste);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection("MatineeMenusBkgNewFolder");
	{
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().NewFolder);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("MatineeMenusBkgNewEmpty");
	{
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().NewEmptyGroup);
	}
	MenuBuilder.EndSection();

	// Prefab group types
	MenuBuilder.BeginSection("MatineeMenusBkgNew");
	{
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().NewCameraGroup);
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().NewParticleGroup);
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().NewSkeletalMeshGroup);
		MenuBuilder.AddMenuEntry(FMatineeCommands::Get().NewLightingGroup);
	}
	MenuBuilder.EndSection();

	TArray<UInterpTrack*> Results;
	IData->FindTracksByClass( UInterpTrackDirector::StaticClass(), Results );
	if(Results.Num() == 0)
	{
		MenuBuilder.BeginSection("MatineeMenusBkgNewDirectorGroup");
		{
			MenuBuilder.AddMenuEntry(FMatineeCommands::Get().NewDirectorGroup);
		}
		MenuBuilder.EndSection();
	}

	if (bIsDirectorTrackWindow)
	{
		MenuBuilder.BeginSection("MatnieeMenusBkgNewDirectorTimeline");
		{
			MenuBuilder.AddMenuEntry(FMatineeCommands::Get().ToggleDirectorTimeline);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}


/*-----------------------------------------------------------------------------
	KeyMenu
-----------------------------------------------------------------------------*/

TSharedPtr<SWidget> FMatinee::CreateKeyMenu()
{
	FMenuBuilder MenuBuilder(true, ToolkitCommands);

	bool bHaveMoveKeys = false;
	bool bHaveFloatKeys = false;
	bool bHaveBoolKeys = false;
	bool bHaveVectorKeys = false;
	bool bHaveLinearColorKeys = false;
	bool bHaveColorKeys = false;
	bool bHaveEventKeys = false;
	bool bHaveAnimKeys = false;
	bool bHaveDirKeys = false;
	bool bAnimIsLooping = false;
	bool bHaveToggleKeys = false;
	bool bHaveVisibilityKeys = false;
	bool bHaveAudioMasterKeys = false;
	bool bHaveParticleReplayKeys = false;

	// true if at least one sound key is selected.
	bool bHaveSoundKeys = false;


	// Keep track of the conditions required for all selected visibility keys to fire
	bool bAllKeyConditionsAreSetToAlways = true;
	bool bAllKeyConditionsAreGoreEnabled = true;
	bool bAllKeyConditionsAreGoreDisabled = true;


	for(int32 i=0; i<Opt->SelectedKeys.Num(); i++)
	{
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[i];
		UInterpTrack* Track = SelKey.Track;

		if( Track->IsA(UInterpTrackMove::StaticClass()) )
		{
			bHaveMoveKeys = true;
		}
		else if( Track->IsA(UInterpTrackEvent::StaticClass()) )
		{
			bHaveEventKeys = true;
		}
		else if( Track->IsA(UInterpTrackDirector::StaticClass()) )
		{
			bHaveDirKeys = true;
		}
		else if( Track->IsA(UInterpTrackAnimControl::StaticClass()) )
		{
			bHaveAnimKeys = true;

			UInterpTrackAnimControl* AnimTrack = (UInterpTrackAnimControl*)Track;
			bAnimIsLooping = AnimTrack->AnimSeqs[SelKey.KeyIndex].bLooping;
		}
		else if( Track->IsA(UInterpTrackFloatBase::StaticClass()) )
		{
			bHaveFloatKeys = true;
		}
		else if( Track->IsA(UInterpTrackBoolProp::StaticClass()) )
		{
			bHaveBoolKeys = true;
		}
		else if( Track->IsA(UInterpTrackColorProp::StaticClass()) || Track->IsA(UInterpTrackVectorMaterialParam::StaticClass()) )
		{
			bHaveColorKeys = true;
		}
		else if( Track->IsA(UInterpTrackVectorBase::StaticClass()) )
		{
			bHaveVectorKeys = true;
		}
		else if( Track->IsA(UInterpTrackLinearColorBase::StaticClass()) )
		{
			bHaveLinearColorKeys = true;
		}

		if( Track->IsA(UInterpTrackSound::StaticClass()) )
		{
			bHaveSoundKeys = true;
		}

		if( Track->IsA( UInterpTrackToggle::StaticClass() ) )
		{
			bHaveToggleKeys = true;
		}

		if( Track->IsA( UInterpTrackVisibility::StaticClass() ) )
		{
			bHaveVisibilityKeys = true;

			UInterpTrackVisibility* VisibilityTrack = CastChecked<UInterpTrackVisibility>(Track);
			FVisibilityTrackKey& VisibilityKey = VisibilityTrack->VisibilityTrack[ SelKey.KeyIndex ];

			if( VisibilityKey.ActiveCondition != EVTC_Always )
			{
				bAllKeyConditionsAreSetToAlways = false;
			}

			if( VisibilityKey.ActiveCondition != EVTC_GoreEnabled )
			{
				bAllKeyConditionsAreGoreEnabled = false;
			}

			if( VisibilityKey.ActiveCondition != EVTC_GoreDisabled )
			{
				bAllKeyConditionsAreGoreDisabled = false;
			}
		}

		if( Track->IsA( UInterpTrackAudioMaster::StaticClass() ) )
		{
			bHaveAudioMasterKeys = true;
		}

		if( Track->IsA( UInterpTrackParticleReplay::StaticClass() ) )
		{
			bHaveParticleReplayKeys = true;
		}
	}

	if(bHaveMoveKeys || bHaveFloatKeys || bHaveVectorKeys || bHaveColorKeys || bHaveLinearColorKeys)
	{
		struct MoveMenu
		{
			static void Create( FMenuBuilder& InMenuBuilder )
			{
				InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeyModeCurveAuto );
				InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeyModeCurveAutoClamped );
				InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeyModeCurveBreak );
				InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeyModeLinear );
				InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeyModeConstant );
			}
		};
		MenuBuilder.AddSubMenu(
			NSLOCTEXT("Matinee.Menus", "Context.Key.ModeMenu", "Interp Mode"), 
			FText::GetEmpty(), 
			FNewMenuDelegate::CreateStatic(&MoveMenu::Create)
			);
		
	}

	if(Opt->SelectedKeys.Num() == 1)
	{
		MenuBuilder.BeginSection("MatineeMenusKeySetTime");
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetTime );
		}
		MenuBuilder.EndSection();

		FInterpEdSelKey& SelKey = Opt->SelectedKeys[0];

		MenuBuilder.BeginSection("MatineeMenusKeys");
		{
			if(bHaveMoveKeys)
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().MoveKeySetLookup );
			
				UInterpTrackMove* MoveTrack = Cast<UInterpTrackMove>(SelKey.Track);

				if( MoveTrack )
				{
					FName GroupName = MoveTrack->GetLookupKeyGroupName(SelKey.KeyIndex);

					if(GroupName == NAME_None)
					{
					}
					else
					{
						const FText Text = FText::Format(NSLOCTEXT("UnrealEd", "ClearGroupLookup_F", "Clear Transform Lookup Group ({0})"), FText::FromName(GroupName));
						MenuBuilder.AddMenuEntry( FMatineeCommands::Get().MoveKeyClearLookup, NAME_None, Text );
					}
				}
			}

			if(bHaveFloatKeys)
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetValue );
			}

			if(bHaveBoolKeys)
			{
				UInterpTrackBoolProp* BoolPropTrack = Cast<UInterpTrackBoolProp>(SelKey.Track);
					
				FText Text;
				if( BoolPropTrack->BoolTrack[SelKey.KeyIndex].Value == false )
				{
					Text = NSLOCTEXT("UnrealEd", "SetToTrue", "Set To True");
				}
				// Otherwise, the boolean value is true, the user only has the option to set it to false. 
				else
				{
					Text = NSLOCTEXT("UnrealEd", "SetToFalse", "Set To False");
				}
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetBool, NAME_None, Text);
			}

			if(bHaveColorKeys || bHaveLinearColorKeys)
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetColor );
			}

			if(bHaveEventKeys)
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().EventKeyRename );
			}

			if(bHaveDirKeys)
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().DirKeySetTransitionTime );
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().DirKeyRenameCameraShot );
			}

			if( bHaveAudioMasterKeys )
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetMasterVolume );
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetMasterPitch );
			}
		}
		MenuBuilder.EndSection(); //MatineeMenusKeys
	}

	MenuBuilder.BeginSection("MatineeMenusKeys");
	{
		if( bHaveToggleKeys || bHaveVisibilityKeys )
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ToggleKeyFlip );
		}

		if( bHaveVisibilityKeys )
		{
			struct ConditionMenu
			{
				static void Create( FMenuBuilder& InMenuBuilder )
				{
					InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetConditionAlways );
					InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetConditionGoreEnabled );
					InMenuBuilder.AddMenuEntry( FMatineeCommands::Get().KeySetConditionGoreDisabled );
				}
			};

			MenuBuilder.AddSubMenu( 
				NSLOCTEXT("Matinee.Menus", "ContextMenu.Key.ConditionMenu", "Active Condition"), 
				FText::GetEmpty(),
				FNewMenuDelegate::CreateStatic(&ConditionMenu::Create)
				);
		}

		if(bHaveAnimKeys)
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().AnimKeyLoop );
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().AnimKeyNoLoop );

			if(Opt->SelectedKeys.Num() == 1)
			{
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().AnimKeySetStartOffset );
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().AnimKeySetEndOffset );
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().AnimKeySetPlayRate );
				MenuBuilder.AddMenuEntry( FMatineeCommands::Get().AnimKeyToggleReverse );
			}
		}
	}
	MenuBuilder.EndSection(); //MatineeMenusKeys

	if ( bHaveSoundKeys )
	{
		MenuBuilder.BeginSection("MatineeMenusKeys");
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().SoundKeySetVolume );
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().SoundKeySetPitch );
		}
		MenuBuilder.EndSection();

		// Does this key have a sound cue set?
		FInterpEdSelKey& SelKey = Opt->SelectedKeys[ 0 ];
		UInterpTrackSound* SoundTrack = Cast<UInterpTrackSound>( SelKey.Track );
		USoundBase* KeySoundCue = SoundTrack->Sounds[ SelKey.KeyIndex ].Sound;

		if( KeySoundCue != NULL )
		{
			MenuBuilder.BeginSection("MatineeMenusKeySyncSoundCue");
			{
				MenuBuilder.AddMenuEntry( 
					FMatineeCommands::Get().KeySyncGenericBrowserToSoundCue, "", 
					FText::Format( NSLOCTEXT("UnrealEd", "InterpEd_KeyContext_SyncGenericBrowserToSoundCue_F", "Find {0} in Generic Browser..." ), FText::FromString(KeySoundCue->GetName()) ) );
			}
			MenuBuilder.EndSection();
		}
	}

	if( bHaveParticleReplayKeys)
	{
		MenuBuilder.BeginSection("MatineeMenusParticleReplay");
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ParticleReplayKeySetClipIDNumber );
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ParticleReplayKeySetDuration );
		}
		MenuBuilder.EndSection();
	}


	if( Opt->SelectedKeys.Num() > 0 )
	{
		MenuBuilder.BeginSection("MatineeMenusDeleteKeys");
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().DeleteSelectedKeys );
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}


/*-----------------------------------------------------------------------------
	CollapseExpandMenu
-----------------------------------------------------------------------------*/

TSharedPtr<SWidget> FMatinee::CreateCollapseExpandMenu()
{
	FMenuBuilder MenuBuilder(true, ToolkitCommands);
	MenuBuilder.AddMenuEntry( FMatineeCommands::Get().ExpandAllGroups );
	MenuBuilder.AddMenuEntry( FMatineeCommands::Get().CollapseAllGroups );

	return MenuBuilder.MakeWidget();
}


/**
 * Default constructor. 
 * Create a context menu with menu items based on the type of marker clicked-on.
 *
 * @param	InterpEd	The interp editor.
 * @param	MarkerType	The type of marker right-clicked on.
 */
TSharedPtr<SWidget> FMatinee::CreateMarkerMenu(EMatineeMarkerType::Type MarkerType)
{
	FMenuBuilder MenuBuilder(true, ToolkitCommands);

	// The sequence start marker should never move. 
	// Thus, this context menu doesn't support it. 
	check( MarkerType != EMatineeMarkerType::ISM_SeqStart );

	// Move marker to beginning of sequence
	if( EMatineeMarkerType::ISM_LoopStart == MarkerType )
	{
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().MarkerMoveToBeginning );
	}

	// Only makes sense to move the loop marker to the sequence end point.
	// Moving the sequence end marker to the sequence end would not move it and
	// moving the loop start marker would cause the loop section to be zero. 
	if( EMatineeMarkerType::ISM_LoopEnd == MarkerType )
	{
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().MarkerMoveToEnd );
	}

	// Doesn't make sense to move the start loop maker to the end of 
	// the longest track because the loop section would be zero. 
	const bool bCanMoveMarkerToTrackEnd = ( EMatineeMarkerType::ISM_SeqEnd == MarkerType || EMatineeMarkerType::ISM_LoopEnd == MarkerType );

	// In order to move a marker to the end of a track, we must actually have a track.
	if( bCanMoveMarkerToTrackEnd && HasATrack() )
	{
		// The user always has the option of moving the marker to the end of 
		// the longest track if we have at least one track, selected or not.
		MenuBuilder.AddMenuEntry( FMatineeCommands::Get().MarkerMoveToEndOfLongestTrack );

		// When one or more tracks are selected, the user has the option of moving the markers 
		// to the end of the longest selected track instead of the longest overall track. 
		if( HasATrackSelected() )
		{
			MenuBuilder.AddMenuEntry( FMatineeCommands::Get().MarkerMoveToEndOfSelectedTrack );
		}
	}

	// All non-sequence start markers can be moved to the current, timeline position.
	MenuBuilder.AddMenuEntry( FMatineeCommands::Get().MarkerMoveToCurrentPosition );

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
