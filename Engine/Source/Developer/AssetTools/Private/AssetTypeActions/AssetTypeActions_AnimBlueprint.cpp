// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_AnimBlueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/MessageDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Animation/AnimInstance.h"
#include "Factories/AnimBlueprintFactory.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "AssetTools.h"
#include "PersonaModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "SBlueprintDiff.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SSkeletonWidget.h"
#include "Styling/SlateIconFinder.h"
#include "IAnimationBlueprintEditorModule.h"
#include "Preferences/PersonaOptions.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_AnimBlueprint::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_Blueprint::GetActions(InObjects, MenuBuilder);

	auto AnimBlueprints = GetTypedWeakObjectPtrs<UAnimBlueprint>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimBlueprint_FindSkeleton", "Find Skeleton"),
		LOCTEXT("AnimBlueprint_FindSkeletonTooltip", "Finds the skeleton used by the selected Anim Blueprints in the content browser."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.FindSkeleton"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimBlueprint::ExecuteFindSkeleton, AnimBlueprints ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddSubMenu( 
		LOCTEXT("RetargetBlueprintSubmenu", "Retarget Anim Blueprints"),
		LOCTEXT("RetargetBlueprintSubmenu_ToolTip", "Opens the retarget blueprints menu"),
		FNewMenuDelegate::CreateSP( this, &FAssetTypeActions_AnimBlueprint::FillRetargetMenu, InObjects ),
		false,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton")
		);

}

void FAssetTypeActions_AnimBlueprint::FillRetargetMenu( FMenuBuilder& MenuBuilder, const TArray<UObject*> InObjects )
{
	bool bAllSkeletonsNull = true;

	for(auto Iter = InObjects.CreateConstIterator(); Iter; ++Iter)
	{
		if(UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(*Iter))
		{
			if(AnimBlueprint->TargetSkeleton)
			{
				bAllSkeletonsNull = false;
				break;
			}
		}
	}

	if(bAllSkeletonsNull)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AnimBlueprint_RetargetSkeletonInPlace", "Retarget skeleton on existing Anim Blueprints"),
			LOCTEXT("AnimBlueprint_RetargetSkeletonInPlaceTooltip", "Retargets the selected Anim Blueprints to a new skeleton (and optionally all referenced animations too)"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.RetargetSkeleton"),
			FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimBlueprint::RetargetAssets, InObjects, false ), // false = do not duplicate assets first
			FCanExecuteAction()
			)
			);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimBlueprint_DuplicateAndRetargetSkeleton", "Duplicate Anim Blueprints and Retarget"),
		LOCTEXT("AnimBlueprint_DuplicateAndRetargetSkeletonTooltip", "Duplicates and then retargets the selected Anim Blueprints to a new skeleton (and optionally all referenced animations too)"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Persona.AssetActions.DuplicateAndRetargetSkeleton"),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_AnimBlueprint::RetargetAssets, InObjects, true ), // true = duplicate assets and retarget them
		FCanExecuteAction()
		)
		);
}

UThumbnailInfo* FAssetTypeActions_AnimBlueprint::GetThumbnailInfo(UObject* Asset) const
{
	UAnimBlueprint* AnimBlueprint = CastChecked<UAnimBlueprint>(Asset);
	UThumbnailInfo* ThumbnailInfo = AnimBlueprint->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(AnimBlueprint, NAME_None, RF_Transactional);
		AnimBlueprint->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

UFactory* FAssetTypeActions_AnimBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UAnimBlueprintFactory* AnimBlueprintFactory = NewObject<UAnimBlueprintFactory>();
	UAnimBlueprint* AnimBlueprint = CastChecked<UAnimBlueprint>(InBlueprint);
	AnimBlueprintFactory->ParentClass = TSubclassOf<UAnimInstance>(*InBlueprint->GeneratedClass);
	AnimBlueprintFactory->TargetSkeleton = AnimBlueprint->TargetSkeleton;
	return AnimBlueprintFactory;
}

void FAssetTypeActions_AnimBlueprint::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto AnimBlueprint = Cast<UAnimBlueprint>(*ObjIt);
		if (AnimBlueprint != NULL && AnimBlueprint->SkeletonGeneratedClass && AnimBlueprint->GeneratedClass)
		{
			if(!AnimBlueprint->TargetSkeleton)
			{
				FText ShouldRetargetMessage = LOCTEXT("ShouldRetarget_Message", "Could not find the skeleton for Anim Blueprint '{BlueprintName}' Would you like to choose a new one?");
				
				FFormatNamedArguments Arguments;
				Arguments.Add( TEXT("BlueprintName"), FText::FromString(AnimBlueprint->GetName()));

				if ( FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(ShouldRetargetMessage, Arguments)) == EAppReturnType::Yes )
				{
					bool bDuplicateAssets = false;
					TArray<UObject*> AnimBlueprints;
					AnimBlueprints.Add(AnimBlueprint);
					RetargetAssets(AnimBlueprints, bDuplicateAssets);
				}
			}
			else
			{
				const bool bBringToFrontIfOpen = true;
				if (IAssetEditorInstance* EditorInstance = FAssetEditorManager::Get().FindEditorForAsset(AnimBlueprint, bBringToFrontIfOpen))
				{
					EditorInstance->FocusWindow(AnimBlueprint);
				}
				else
				{
					IAnimationBlueprintEditorModule& AnimationBlueprintEditorModule = FModuleManager::LoadModuleChecked<IAnimationBlueprintEditorModule>("AnimationBlueprintEditor");
					AnimationBlueprintEditorModule.CreateAnimationBlueprintEditor(Mode, EditWithinLevelEditor, AnimBlueprint);
				}
			}
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadCorruptAnimBlueprint", "The Anim Blueprint could not be loaded because it is corrupt."));
		}
	}
}

void FAssetTypeActions_AnimBlueprint::PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const
{
	UBlueprint* OldBlueprint = CastChecked<UBlueprint>(Asset1);
	UBlueprint* NewBlueprint = CastChecked<UBlueprint>(Asset2);

	// sometimes we're comparing different revisions of one single asset (other 
	// times we're comparing two completely separate assets altogether)
	bool bIsSingleAsset = (NewBlueprint->GetName() == OldBlueprint->GetName());

	FText WindowTitle = LOCTEXT("NamelessAnimationBlueprintDiff", "Animation Blueprint Diff");
	// if we're diffing one asset against itself 
	if (bIsSingleAsset)
	{
		// identify the assumed single asset in the window's title
		WindowTitle = FText::Format(LOCTEXT("AnimationBlueprintDiff", "{0} - Animation Blueprint Diff"), FText::FromString(NewBlueprint->GetName()));
	}

	const TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(FVector2D(1000, 800));

	Window->SetContent(SNew(SBlueprintDiff)
		.BlueprintOld(OldBlueprint)
		.BlueprintNew(NewBlueprint)
		.OldRevision(OldRevision)
		.NewRevision(NewRevision)
		.ShowAssetNames(!bIsSingleAsset));

	// Make this window a child of the modal window if we've been spawned while one is active.
	TSharedPtr<SWindow> ActiveModal = FSlateApplication::Get().GetActiveModalWindow();
	if (ActiveModal.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), ActiveModal.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	}
}

void FAssetTypeActions_AnimBlueprint::ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects)
{
	TArray<UObject*> ObjectsToSync;
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Object = (*ObjIt).Get();
		if ( Object )
		{
			USkeleton* Skeleton = Object->TargetSkeleton;
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

void FAssetTypeActions_AnimBlueprint::RetargetAnimationHandler(USkeleton* OldSkeleton, USkeleton* NewSkeleton, bool bRemapReferencedAssets, bool bAllowRemapToExisting, bool bConvertSpaces, const EditorAnimUtils::FNameDuplicationRule* NameRule, TArray<TWeakObjectPtr<UObject>> AnimBlueprints)
{
	if(!OldSkeleton || OldSkeleton->GetPreviewMesh(true))
	{
		FAnimationRetargetContext RetargetContext(AnimBlueprints, bRemapReferencedAssets, bConvertSpaces);

		if(bAllowRemapToExisting)
		{
			SAnimationRemapAssets::ShowWindow(RetargetContext, NewSkeleton);
		}

		EditorAnimUtils::RetargetAnimations(OldSkeleton, NewSkeleton, RetargetContext, bRemapReferencedAssets, NameRule);
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

void FAssetTypeActions_AnimBlueprint::RetargetAssets(TArray<UObject*> InAnimBlueprints, bool bDuplicateAssets)
{
	bool bRemapReferencedAssets = false;
	USkeleton* OldSkeleton = NULL;

	if ( InAnimBlueprints.Num() > 0 )
	{
		UAnimBlueprint * AnimBP = CastChecked<UAnimBlueprint>(InAnimBlueprints[0]);
		OldSkeleton = AnimBP->TargetSkeleton;
	}

	const FText Message = LOCTEXT("RemapSkeleton_Warning", "Select the skeleton to remap this asset to.");
	auto AnimBlueprints = GetTypedWeakObjectPtrs<UObject>(InAnimBlueprints);

	SAnimationRemapSkeleton::ShowWindow(OldSkeleton, Message, bDuplicateAssets, FOnRetargetAnimation::CreateSP(this, &FAssetTypeActions_AnimBlueprint::RetargetAnimationHandler, AnimBlueprints));
}

TSharedPtr<SWidget> FAssetTypeActions_AnimBlueprint::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	const FSlateBrush* Icon = FSlateIconFinder::FindIconBrushForClass(UAnimBlueprint::StaticClass());

	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetNoBrush())
		.Visibility(EVisibility::HitTestInvisible)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 3.0f))
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		[
			SNew(SImage)
			.Image(Icon)
		];
}

#undef LOCTEXT_NAMESPACE
