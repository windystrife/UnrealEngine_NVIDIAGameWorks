// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompositeFactory.cpp: Factory for AnimComposite
=============================================================================*/

#include "Factories/AnimCompositeFactory.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "EditorStyleSet.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCompositeBase.h"
#include "Editor.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Animation/AnimComposite.h"

#define LOCTEXT_NAMESPACE "AnimCompositeFactory"

UAnimCompositeFactory::UAnimCompositeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	SupportedClass = UAnimComposite::StaticClass();
}

bool UAnimCompositeFactory::ConfigureProperties()
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
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateUObject(this, &UAnimCompositeFactory::OnTargetSkeletonSelected);

	/** The default view mode should be a list view */
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

	PickerWindow = SNew(SWindow)
	.Title(LOCTEXT("CreateAnimCompositeOptions", "Pick Skeleton"))
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

UObject* UAnimCompositeFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (TargetSkeleton || SourceAnimation)
	{
		UAnimComposite* AnimComposite = NewObject<UAnimComposite>(InParent, Class, Name, Flags);

		if(SourceAnimation)
		{
			USkeleton* SourceSkeleton = SourceAnimation->GetSkeleton();
			//Make sure we haven't asked to create an AnimComposite with mismatching skeletons
			check(TargetSkeleton == NULL || TargetSkeleton == SourceSkeleton);
			TargetSkeleton = SourceSkeleton;

			FAnimSegment NewSegment;
			NewSegment.AnimReference = SourceAnimation;
			NewSegment.AnimStartTime = 0.f;
			NewSegment.AnimEndTime = SourceAnimation->SequenceLength;
			NewSegment.AnimPlayRate = 1.f;
			NewSegment.LoopingCount = 1;
			NewSegment.StartPos = 0.f;

			AnimComposite->AnimationTrack.AnimSegments.Add(NewSegment);
			AnimComposite->SetSequenceLength( AnimComposite->AnimationTrack.GetLength() );
		}
		
		AnimComposite->SetSkeleton( TargetSkeleton );
		if (PreviewSkeletalMesh)
		{
			AnimComposite->SetPreviewMesh(PreviewSkeletalMesh);
		}

		return AnimComposite;
	}

	return NULL;
}

void UAnimCompositeFactory::OnTargetSkeletonSelected(const FAssetData& SelectedAsset)
{
	TargetSkeleton = Cast<USkeleton>(SelectedAsset.GetAsset());
	PickerWindow->RequestDestroyWindow();
}

#undef LOCTEXT_NAMESPACE
