// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "EditorStyleSet.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "AssetTools.h"
#include "PersonaModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SSkeletonWidget.h"
#include "IAnimationEditorModule.h"
#include "Preferences/PersonaOptions.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Algo/Transform.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_AnimationAsset::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto AnimAssets = GetTypedWeakObjectPtrs<UAnimationAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimSequenceBase_OpenInNewWindow", "Open In New Window"),
		LOCTEXT("AnimSequenceBase_OpenInNewWindowTooltip", "Will always open asset in a new window, and not re-use existing window. (Shift+Double-Click)"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.OpenInExternalEditor"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_AnimationAsset::ExecuteOpenInNewWindow, AnimAssets),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimSequenceBase_FindSkeleton", "Find Skeleton"),
		LOCTEXT("AnimSequenceBase_FindSkeletonTooltip", "Finds the skeleton for the selected assets in the content browser."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.FindSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimationAsset::ExecuteFindSkeleton, AnimAssets ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddSubMenu( 
		LOCTEXT("RetargetAnimSubmenu", "Retarget Anim Assets"),
		LOCTEXT("RetargetAnimSubmenu_ToolTip", "Opens the retarget anim assets menu"),
		FNewMenuDelegate::CreateSP( this, &FAssetTypeActions_AnimationAsset::FillRetargetMenu, InObjects ),
		false,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton")
		);
}

void FAssetTypeActions_AnimationAsset::FillRetargetMenu( FMenuBuilder& MenuBuilder, const TArray<UObject*> InObjects )
{
	bool bAllSkeletonsNull = true;

	for(auto Iter = InObjects.CreateConstIterator(); Iter; ++Iter)
	{
		if(UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(*Iter))
		{
			if(AnimAsset->GetSkeleton())
			{
				bAllSkeletonsNull = false;
				break;
			}
		}
	}

	if(bAllSkeletonsNull)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AnimAsset_RetargetSkeletonInPlace", "Retarget skeleton on existing Anim Assets"),
			LOCTEXT("AnimAsset_RetargetSkeletonInPlaceTooltip", "Retargets the selected Anim Assets to a new skeleton (and optionally all referenced animations too)"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton"),
			FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimationAsset::RetargetAssets, InObjects, false, false, TSharedPtr<IToolkitHost>() ), // false = do not duplicate assets first
			FCanExecuteAction()
			)
			);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimAsset_DuplicateAndRetargetSkeleton", "Duplicate Anim Assets and Retarget"),
		LOCTEXT("AnimAsset_DuplicateAndRetargetSkeletonTooltip", "Duplicates and then retargets the selected Anim Assets to a new skeleton (and optionally all referenced animations too)"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.DuplicateAndRetargetSkeleton"),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimationAsset::RetargetAssets, InObjects, true, false, TSharedPtr<IToolkitHost>() ), // true = duplicate assets and retarget them
		FCanExecuteAction()
		)
		);
}

UThumbnailInfo* FAssetTypeActions_AnimationAsset::GetThumbnailInfo(UObject* Asset) const
{
	UAnimationAsset* Anim = CastChecked<UAnimationAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = Anim->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(Anim, NAME_None, RF_Transactional);
		Anim->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_AnimationAsset::OpenAnimAssetEditor(const TArray<UObject*>& InObjects, bool bForceNewEditor, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	// Force new window if control held down
	bForceNewEditor |= FSlateApplication::Get().GetModifierKeys().IsShiftDown();

	// Find all the anim assets
	TArray<UAnimationAsset*> AnimAssets;
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(*ObjIt))
		{
			AnimAssets.Add(AnimAsset);
		}
	}

	// For each one..
	for (UAnimationAsset* AnimAsset : AnimAssets)
	{
		USkeleton* AnimSkeleton = AnimAsset->GetSkeleton();
		if (!AnimSkeleton)
		{
			FText ShouldRetargetMessage = LOCTEXT("ShouldRetargetAnimAsset_Message", "Could not find the skeleton for Anim '{AnimName}' Would you like to choose a new one?");

			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("AnimName"), FText::FromString(AnimAsset->GetName()));

			if (FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(ShouldRetargetMessage, Arguments)) == EAppReturnType::Yes)
			{
				bool bDuplicateAssets = false;
				TArray<UObject*> AssetsToRetarget;
				AssetsToRetarget.Add(AnimAsset);
				RetargetAssets(AssetsToRetarget, bDuplicateAssets, true, EditWithinLevelEditor);
			}
		}
		else
		{
			// First see if we already have it open
			const bool bBringToFrontIfOpen = true;
			if (IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(AnimAsset, bBringToFrontIfOpen))
			{
				EditorInstance->FocusWindow();
			}
			else
			{
				// See if we are trying to open a single asset. If we are, we re-use a compatible anim editor.
				bool bSingleAsset = AnimAssets.Num() == 1;
				bool bFoundEditor = false;
				if (bSingleAsset && !bForceNewEditor)
				{
					// See if there is an animation asset with the same skeleton already being edited
					TArray<UObject*> AllEditedAssets = FAssetEditorManager::Get().GetAllEditedAssets();
					UAnimationAsset* CompatibleEditedAsset = nullptr;
					for (UObject* EditedAsset : AllEditedAssets)
					{
						UAnimationAsset* EditedAnimAsset = Cast<UAnimationAsset>(EditedAsset);
						if (EditedAnimAsset && EditedAnimAsset->GetSkeleton() == AnimAsset->GetSkeleton())
						{
							CompatibleEditedAsset = EditedAnimAsset;
							break;
						}
					}

					// If there is..
					if(CompatibleEditedAsset)
					{
						// Find the anim editors that are doing it
						TArray<IAssetEditorInstance*> AssetEditors = FAssetEditorManager::Get().FindEditorsForAsset(CompatibleEditedAsset);
						for (IAssetEditorInstance* ExistingEditor : AssetEditors)
						{
							if (ExistingEditor->GetEditorName() == FName("AnimationEditor"))
							{
								// Change the current anim to this one
								IAnimationEditor* AnimEditor = static_cast<IAnimationEditor*>(ExistingEditor);
								AnimEditor->SetAnimationAsset(AnimAsset);
								AnimEditor->FocusWindow();
								bFoundEditor = true;
								break;
							}
						}
					}
				}

				// We didn't find an editor, make a new one
				if (!bFoundEditor)
				{
					IAnimationEditorModule& AnimationEditorModule = FModuleManager::LoadModuleChecked<IAnimationEditorModule>("AnimationEditor");
					AnimationEditorModule.CreateAnimationEditor(Mode, EditWithinLevelEditor, AnimAsset);
				}
			}
		}
	}
}


void FAssetTypeActions_AnimationAsset::ExecuteOpenInNewWindow(TArray<TWeakObjectPtr<UAnimationAsset>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	Algo::Transform(Objects, ObjectsToSync, [](TWeakObjectPtr<UAnimationAsset> Obj) { return Obj.Get(); });

	OpenAnimAssetEditor(ObjectsToSync, true, nullptr);
}


void FAssetTypeActions_AnimationAsset::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	OpenAnimAssetEditor(InObjects, false, EditWithinLevelEditor);
}

void FAssetTypeActions_AnimationAsset::ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimationAsset>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			USkeleton* Skeleton = Object->GetSkeleton();
			if (Skeleton)
			{
				ObjectsToSync.AddUnique(Skeleton);
			}
		}
	}

	if ( ObjectsToSync.Num() > 0 )
	{
		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
}

void FAssetTypeActions_AnimationAsset::RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bAllowRemapToExisting, bool bConvertSpaces, const EditorAnimUtils::FNameDuplicationRule* NameRule, TArray<TWeakObjectPtr<UObject>> InAnimAssets)
{
	if(!OldSkeleton || OldSkeleton->GetPreviewMesh(false))
	{
		EditorAnimUtils::RetargetAnimations(OldSkeleton, NewSkeleton, InAnimAssets, bRemapReferencedAssets, NameRule, bConvertSpaces);
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("OldSkeletonName"), FText::FromString(GetNameSafe(OldSkeleton)));
		Args.Add(TEXT("NewSkeletonName"), FText::FromString(GetNameSafe(NewSkeleton)));
		FNotificationInfo Info(FText::Format(LOCTEXT("Retarget Failed", "Old Skeleton {OldSkeletonName} and New Skeleton {NewSkeletonName} need to have Preview Mesh set up to convert animation"), Args));
		Info.ExpireDuration = 5.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if(Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

void FAssetTypeActions_AnimationAsset::RetargetNonSkeletonAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bAllowRemapToExisting, bool bConvertSpaces, const EditorAnimUtils::FNameDuplicationRule* NameRule, TArray<TWeakObjectPtr<UObject>> InAnimAssets, TWeakPtr<IToolkitHost> EditWithinLevelEditor)
{
	RetargetAnimationHandler(OldSkeleton, NewSkeleton, bRemapReferencedAssets, bAllowRemapToExisting, bConvertSpaces, NameRule, InAnimAssets);

	if(NewSkeleton)
	{
		for(auto Asset : InAnimAssets)
		{
			if (Asset.IsValid())
			{
				if (EditWithinLevelEditor.IsValid())
				{
					FAssetEditorManager::Get().OpenEditorForAsset(Asset.Get(), EToolkitMode::WorldCentric, EditWithinLevelEditor.Pin());
				}
				else
				{
					FAssetEditorManager::Get().OpenEditorForAsset(Asset.Get());
				}
			}
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadSkeletonlessAnimAsset", "The Anim Asset could not be loaded because it's skeleton is missing."));
	}
}


void FAssetTypeActions_AnimationAsset::RetargetAssets(TArray<UObject*> InAnimAssets, bool bDuplicateAssets, bool bOpenEditor, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	bool bRemapReferencedAssets = false;
	USkeleton* NewSkeleton = NULL;
	USkeleton* OldSkeleton = NULL;
	if(InAnimAssets.Num() > 0)
	{
		UAnimationAsset * AnimAsset = CastChecked<UAnimationAsset>(InAnimAssets[0]);
		OldSkeleton = AnimAsset->GetSkeleton();
	}

	const FText Message = LOCTEXT("SelectSkeletonToRemap", "Select the skeleton to remap this asset to.");

	auto AnimAssets = GetTypedWeakObjectPtrs<UObject>(InAnimAssets);

	if (bOpenEditor)
	{
		SAnimationRemapSkeleton::ShowWindow(OldSkeleton, Message, bDuplicateAssets, FOnRetargetAnimation::CreateSP(this, &FAssetTypeActions_AnimationAsset::RetargetNonSkeletonAnimationHandler, AnimAssets, TWeakPtr<class IToolkitHost> (EditWithinLevelEditor)) );
	}
	else
	{
		SAnimationRemapSkeleton::ShowWindow(OldSkeleton, Message, bDuplicateAssets, FOnRetargetAnimation::CreateSP(this, &FAssetTypeActions_AnimationAsset::RetargetAnimationHandler, AnimAssets) );
	}
}


#undef LOCTEXT_NAMESPACE
