// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISkeletonTreeBuilder.h"
#include "IEditableSkeleton.h"
#include "ISkeletonTreeItem.h"

class IPersonaPreviewScene;
class ISkeletonTree;
class USkeletalMeshSocket;

/** Options for skeleton building */
struct FSkeletonTreeBuilderArgs
{
	FSkeletonTreeBuilderArgs(bool bInShowBones = true, bool bInShowSockets = true, bool bInShowAttachedAssets = true, bool bInShowVirtualBones = true)
		: bShowBones(bInShowBones)
		, bShowSockets(bInShowSockets)
		, bShowAttachedAssets(bInShowAttachedAssets)
		, bShowVirtualBones(bInShowVirtualBones)
	{}

	/** Whether we should show bones */
	bool bShowBones;

	/** Whether we should show sockets */
	bool bShowSockets;

	/** Whether we should show attached assets */
	bool bShowAttachedAssets;

	/** Whether we should show virtual bones */
	bool bShowVirtualBones;
};

/** Enum which tells us what type of bones we should be showing */
enum class EBoneFilter : int32
{
	All,
	Mesh,
	LOD,
	Weighted, /** only showing weighted bones of current LOD */
	None,
	Count
};

/** Enum which tells us what type of sockets we should be showing */
enum class ESocketFilter : int32
{
	Active,
	Mesh,
	Skeleton,
	All,
	None,
	Count
};

class SKELETONEDITOR_API FSkeletonTreeBuilder : public ISkeletonTreeBuilder
{
public:
	FSkeletonTreeBuilder(const FSkeletonTreeBuilderArgs& InBuilderArgs);

	/** ISkeletonTreeBuilder interface */
	virtual void Initialize(const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedPtr<class IPersonaPreviewScene>& InPreviewScene, FOnFilterSkeletonTreeItem InOnFilterSkeletonTreeItem) override;
	virtual void Build(FSkeletonTreeBuilderOutput& Output) override;
	virtual void Filter(const FSkeletonTreeFilterArgs& InArgs, const TArray<TSharedPtr<class ISkeletonTreeItem>>& InItems, TArray<TSharedPtr<class ISkeletonTreeItem>>& OutFilteredItems) override;
	virtual ESkeletonTreeFilterResult FilterItem(const FSkeletonTreeFilterArgs& InArgs, const TSharedPtr<class ISkeletonTreeItem>& InItem) override;
	virtual bool IsShowingBones() const override;
	virtual bool IsShowingSockets() const override;
	virtual bool IsShowingAttachedAssets() const override;

protected:
	/** Add Bones */
	void AddBones(FSkeletonTreeBuilderOutput& Output);

	/** Add Sockets */
	void AddSockets(FSkeletonTreeBuilderOutput& Output);

	void AddAttachedAssets(FSkeletonTreeBuilderOutput& Output);

	void AddSocketsFromData(const TArray< USkeletalMeshSocket* >& SocketArray, ESocketParentType ParentType, FSkeletonTreeBuilderOutput& Output);

	void AddAttachedAssetContainer(const FPreviewAssetAttachContainer& AttachedObjects, FSkeletonTreeBuilderOutput& Output);

	void AddVirtualBones(FSkeletonTreeBuilderOutput& Output);

	/** Create an item for a bone */
	TSharedRef<class ISkeletonTreeItem> CreateBoneTreeItem(const FName& InBoneName);

	/** Create an item for a socket */
	TSharedRef<class ISkeletonTreeItem> CreateSocketTreeItem(USkeletalMeshSocket* InSocket, ESocketParentType ParentType, bool bIsCustomized);

	/** Create an item for an attached asset */
	TSharedRef<class ISkeletonTreeItem> CreateAttachedAssetTreeItem(UObject* InAsset, const FName& InAttachedTo);

	/** Create an item for a virtual bone */
	TSharedRef<class ISkeletonTreeItem> CreateVirtualBoneTreeItem(const FName& InBoneName);

	/** Helper function for filtering */
	ESkeletonTreeFilterResult FilterRecursive(const FSkeletonTreeFilterArgs& InArgs, const TSharedPtr<ISkeletonTreeItem>& InItem, TArray<TSharedPtr<ISkeletonTreeItem>>& OutFilteredItems);

protected:
	/** Options for building */
	FSkeletonTreeBuilderArgs BuilderArgs;

	/** Delegate used for filtering */
	FOnFilterSkeletonTreeItem OnFilterSkeletonTreeItem;

	/** The skeleton tree we will build against */
	TWeakPtr<class ISkeletonTree> SkeletonTreePtr;

	/** The editable skeleton that the skeleton tree represents */
	TWeakPtr<class IEditableSkeleton> EditableSkeletonPtr;

	/** The (optional) preview scene we will build against */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
};
