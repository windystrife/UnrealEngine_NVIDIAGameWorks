// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PoseAssetFactory.cpp: Factory for PoseAsset
=============================================================================*/

#include "Factories/PoseAssetFactory.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "Animation/Skeleton.h"
#include "Editor.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "Animation/PoseAsset.h"

#define LOCTEXT_NAMESPACE "PoseAssetFactory"

UPoseAssetFactory::UPoseAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	SupportedClass = UPoseAsset::StaticClass();
}

bool UPoseAssetFactory::ConfigureProperties()
{
	// Null the skeleton so we can check for selection later
	TargetSkeleton = NULL;
	SourceAnimation = NULL;

	// Load the content browser module to display an asset picker
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FAssetPickerConfig AssetPickerConfig;
	
	/** The asset picker will only show skeletons */
	AssetPickerConfig.Filter.ClassNames.Add(USkeleton::StaticClass()->GetFName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;

	/** The delegate that fires when an asset was selected */
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UPoseAssetFactory::OnTargetSkeletonSelected);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	PickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreatePoseAssetOptions", "Pick Skeleton"))
	.ClientSize(FVector2D(500, 600))
	.SupportsMinimize(false) .SupportsMaximize(false)
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];

	GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
	PickerWindow.Reset();

	return TargetSkeleton != NULL;
}

UObject* UPoseAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (TargetSkeleton || SourceAnimation)
	{
		UPoseAsset* PoseAsset = NewObject<UPoseAsset>(InParent, Class, Name, Flags);

		if(SourceAnimation)
		{
			PoseAsset->CreatePoseFromAnimation( SourceAnimation );
		}
		
		PoseAsset->SetSkeleton( TargetSkeleton );
		if (PreviewSkeletalMesh)
		{
			PoseAsset->SetPreviewMesh(PreviewSkeletalMesh);
		}
		return PoseAsset;
	}

	return NULL;
}

void UPoseAssetFactory::OnTargetSkeletonSelected(const FAssetData& SelectedAsset)
{
	TargetSkeleton = Cast<USkeleton>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}

#undef LOCTEXT_NAMESPACE
