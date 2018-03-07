// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationAsset.h"
#include "Editor/ContentBrowser/Public/IContentBrowserSingleton.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"
#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"

class FMenuBuilder;
class UAnimBlueprint;
class UAnimCompress;
class UAnimSequence;
class UEdGraph;
class UPoseWatch;

/** dialog to prompt users to decide an animation asset name */
class SCreateAnimationAssetDlg : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SCreateAnimationAssetDlg)
	{
	}
		SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_END_ARGS()

		SCreateAnimationAssetDlg()
		: UserResponse(EAppReturnType::Cancel)
	{
	}

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

	/** Gets the resulting asset name */
	FString GetAssetName();

	/** Gets the resulting full asset path (path+'/'+name) */
	FString GetFullAssetPath();

protected:
	void OnPathChange(const FString& NewPath);
	void OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);

	bool ValidatePackage();

	EAppReturnType::Type UserResponse;
	FText AssetPath;
	FText AssetName;

	static FText LastUsedAssetPath;
};

/** Defines FCanExecuteAction delegate interface.  Returns true when an action is able to execute. */
DECLARE_DELEGATE_OneParam(FAnimAssetCreated, TArray<class UObject*>);

//Animation editor utility functions
namespace AnimationEditorUtils
{
	UNREALED_API void CreateAnimationAssets(const TArray<TWeakObjectPtr<UObject>>& SkeletonsOrSkeletalMeshes, TSubclassOf<UAnimationAsset> AssetClass, const FString& InPrefix, FAnimAssetCreated AssetCreated, UObject* NameBaseObject = nullptr, bool bDoNotShowNameDialog = false);
	
	UNREALED_API void CreateNewAnimBlueprint(TArray<TWeakObjectPtr<UObject>> SkeletonsOrSkeletalMeshes, FAnimAssetCreated AssetCreated);
	UNREALED_API void FillCreateAssetMenu(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<UObject>>& SkeletonsOrSkeletalMeshes, FAnimAssetCreated AssetCreated, bool bInContentBrowser=true);
	UNREALED_API void CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName);
	UNREALED_API bool ApplyCompressionAlgorithm(TArray<UAnimSequence*>& AnimSequencePtrs, UAnimCompress* Algorithm);

	// template version of simple creating animation asset
	template< class T >
	T* CreateAnimationAsset(UObject* SkeletonOrSkeletalMesh, const FString& AssetPath, const FString& InPrefix)
	{
		USkeletalMesh* SkeletalMesh = nullptr;
		USkeleton* Skeleton = Cast<USkeleton>(SkeletonOrSkeletalMesh);
		if (Skeleton == nullptr)
		{
			SkeletalMesh = CastChecked<USkeletalMesh>(SkeletonOrSkeletalMesh);
			Skeleton = SkeletalMesh->Skeleton;
		}

		if (Skeleton)
		{
			FString Name;
			FString PackageName;
			// Determine an appropriate name
			CreateUniqueAssetName(AssetPath, InPrefix, PackageName, Name);

			// Create the asset, and assign its skeleton
			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
			T* NewAsset = Cast<T>(AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), T::StaticClass(), NULL));

			if (NewAsset)
			{
				NewAsset->SetSkeleton(Skeleton);
				if (SkeletalMesh)
				{
					NewAsset->SetPreviewMesh(SkeletalMesh);
				}
				NewAsset->MarkPackageDirty();
			}

			return NewAsset;
		}

		return nullptr;
	}
	
	// The following functions are used to fix subgraph arrays for assets
	UNREALED_API void RegenerateSubGraphArrays(UAnimBlueprint* Blueprint);
	void RegenerateGraphSubGraphs(UAnimBlueprint* OwningBlueprint, UEdGraph* GraphToFix);
	void RemoveDuplicateSubGraphs(UEdGraph* GraphToClean);
	void FindChildGraphsFromNodes(UEdGraph* GraphToSearch, TArray<UEdGraph*>& ChildGraphs);
	UNREALED_API void SetPoseWatch(UPoseWatch* PoseWatch, UAnimBlueprint* AnimBlueprintIfKnown = nullptr);
	UNREALED_API UPoseWatch* FindPoseWatchForNode(const UEdGraphNode* Node, UAnimBlueprint* AnimBlueprintIfKnown=nullptr);
	UNREALED_API void MakePoseWatchForNode(UAnimBlueprint* AnimBlueprint, UEdGraphNode* Node, FColor PoseWatchColour);
	UNREALED_API void RemovePoseWatch(UPoseWatch* PoseWatch, UAnimBlueprint* AnimBlueprintIfKnown=nullptr);
	UNREALED_API void UpdatePoseWatchColour(UPoseWatch* PoseWatch, FColor NewPoseWatchColour);

	//////////////////////////////////////////////////////////////////////////////////////////

	template <typename TFactory, typename T>
	void ExecuteNewAnimAsset(TArray<TWeakObjectPtr<UObject>> SkeletonsOrSkeletalMeshes, const FString InSuffix, FAnimAssetCreated AssetCreated, bool bInContentBrowser)
	{
		if(bInContentBrowser && SkeletonsOrSkeletalMeshes.Num() == 1)
		{
			USkeletalMesh* SkeletalMesh = nullptr;
			USkeleton* Skeleton = Cast<USkeleton>(SkeletonsOrSkeletalMeshes[0].Get());
			if (Skeleton == nullptr)
			{
				SkeletalMesh = CastChecked<USkeletalMesh>(SkeletonsOrSkeletalMeshes[0].Get());
				Skeleton = SkeletalMesh->Skeleton;
			}

			if(Skeleton)
			{
				// Determine an appropriate name for inline-rename
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Skeleton->GetOutermost()->GetName(), InSuffix, PackageName, Name);

				TFactory* Factory = NewObject<TFactory>();
				Factory->TargetSkeleton = Skeleton;
				Factory->PreviewSkeletalMesh = SkeletalMesh;

				FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), T::StaticClass(), Factory);

				if(AssetCreated.IsBound())
				{
					// @TODO: this doesn't work
					//FString LongPackagePath = FPackageName::GetLongPackagePath(PackageName);
					UObject* 	Parent = FindPackage(NULL, *PackageName);
					UObject* NewAsset = FindObject<UObject>(Parent, *Name, false);
					if(NewAsset)
					{
						TArray<UObject*> NewAssets;
						NewAssets.Add(NewAsset);
						AssetCreated.Execute(NewAssets);
					}
				}
			}
		}
		else
		{
			CreateAnimationAssets(SkeletonsOrSkeletalMeshes, T::StaticClass(), InSuffix, AssetCreated);
		}
	}
} // namespace AnimationEditorUtils
