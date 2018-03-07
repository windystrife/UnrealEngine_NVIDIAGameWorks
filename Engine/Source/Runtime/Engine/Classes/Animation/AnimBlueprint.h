// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Engine/Blueprint.h"
#include "AnimBlueprint.generated.h"

class SWidget;
class UAnimationAsset;
class USkeletalMesh;

USTRUCT()
struct FAnimGroupInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FLinearColor Color;

	FAnimGroupInfo()
		: Color(FLinearColor::White)
	{
	}
};

USTRUCT()
struct FAnimParentNodeAssetOverride
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UAnimationAsset* NewAsset;
	UPROPERTY()
	FGuid ParentNodeGuid;

	FAnimParentNodeAssetOverride(FGuid InGuid, UAnimationAsset* InNewAsset)
		: NewAsset(InNewAsset)
		, ParentNodeGuid(InGuid)
	{}

	FAnimParentNodeAssetOverride()
		: NewAsset(NULL)
	{}

	bool operator ==(const FAnimParentNodeAssetOverride& Other)
	{
		return ParentNodeGuid == Other.ParentNodeGuid;
	}
};

/**
 * An Anim Blueprint is essentially a specialized Blueprint whose graphs control the animation of a Skeletal Mesh.
 * It can perform blending of animations, directly control the bones of the skeleton, and output a final pose
 * for a Skeletal Mesh each frame.
 */
UCLASS(BlueprintType)
class ENGINE_API UAnimBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

	/** The kind of skeleton that animation graphs compiled from the blueprint will animate */
	UPROPERTY(AssetRegistrySearchable)
	class USkeleton* TargetSkeleton;

	// List of animation sync groups
	UPROPERTY()
	TArray<FAnimGroupInfo> Groups;

	/**
	 * Allows this anim Blueprint to update its native update, blend tree, montages and asset players on
	 * a worker thread. The compiler will attempt to pick up any issues that may occur with threaded update.
	 * For updates to run in multiple threads both this flag and the project setting "Allow Multi Threaded 
	 * Animation Update" should be set.
	 */
	UPROPERTY(EditAnywhere, Category = Optimization)
	bool bUseMultiThreadedAnimationUpdate;

	/**
	 * Selecting this option will cause the compiler to emit warnings whenever a call into Blueprint
	 * is made from the animation graph. This can help track down optimizations that need to be made.
	 */
	UPROPERTY(EditAnywhere, Category = Optimization)
	bool bWarnAboutBlueprintUsage;

	// @todo document
	class UAnimBlueprintGeneratedClass* GetAnimBlueprintGeneratedClass() const;

	// @todo document
	class UAnimBlueprintGeneratedClass* GetAnimBlueprintSkeletonClass() const;

#if WITH_EDITOR

	virtual UClass* GetBlueprintClass() const override;

	// Inspects the hierarchy and looks for an override for the requested node GUID
	// @param NodeGuid - Guid of the node to search for
	// @param bIgnoreSelf - Ignore this blueprint and only search parents, handy for finding parent overrides
	FAnimParentNodeAssetOverride* GetAssetOverrideForNode(FGuid NodeGuid, bool bIgnoreSelf = false) const ;

	// Inspects the hierarchy and builds a list of all asset overrides for this blueprint
	// @param OutOverrides - Array to fill with overrides
	// @return bool - Whether any overrides were found
	bool GetAssetOverrides(TArray<FAnimParentNodeAssetOverride*>& OutOverrides);

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}

	virtual bool IsValidForBytecodeOnlyRecompile() const override { return false; }
	virtual bool CanRecompileWhilePlayingInEditor() const override;
	// End of UBlueprint interface

	// Finds the index of the specified group, or creates a new entry for it (unless the name is NAME_None, which will return INDEX_NONE)
	int32 FindOrAddGroup(FName GroupName);

	/** Returns the most base anim blueprint for a given blueprint (if it is inherited from another anim blueprint, returning null if only native / non-anim BP classes are it's parent) */
	static UAnimBlueprint* FindRootAnimBlueprint(UAnimBlueprint* DerivedBlueprint);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOverrideChangedMulticaster, FGuid, UAnimationAsset*);

	typedef FOnOverrideChangedMulticaster::FDelegate FOnOverrideChanged;

	void RegisterOnOverrideChanged(const FOnOverrideChanged& Delegate)
	{
		OnOverrideChanged.Add(Delegate);
	}

	void UnregisterOnOverrideChanged(SWidget* Widget)
	{
		OnOverrideChanged.RemoveAll(Widget);
	}

	void NotifyOverrideChange(FAnimParentNodeAssetOverride& Override)
	{
		OnOverrideChanged.Broadcast(Override.ParentNodeGuid, Override.NewAsset);
	}

	virtual void PostLoad() override;

	virtual void Serialize(FArchive& Ar) override;

	/** Set the preview mesh for this animation blueprint */
	void SetPreviewMesh(USkeletalMesh* PreviewMesh);

	/** 
	 * Get the preview mesh for this animation blueprint 
	 * Note: loads the mesh if it is not already loaded, or nulls it out if the skeleton has changed since.
	 */
	USkeletalMesh* GetPreviewMesh();

	/** Get the preview mesh for this animation blueprint */
	USkeletalMesh* GetPreviewMesh() const;

protected:
	// Broadcast when an override is changed, allowing derived blueprints to be updated
	FOnOverrideChangedMulticaster OnOverrideChanged;
#endif	// #if WITH_EDITOR

#if WITH_EDITORONLY_DATA
public:
	// Array of overrides to asset containing nodes in the parent that have been overridden
	UPROPERTY()
	TArray<FAnimParentNodeAssetOverride> ParentAssetOverrides;

	// Array of active pose watches (pose watch allows us to see the bone pose at a 
	// particular point of the anim graph) 
	UPROPERTY(transient)
	TArray<class UPoseWatch*> PoseWatches;

private:
	/** The default skeletal mesh to use when previewing this asset - this only applies when you open Persona using this asset*/
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TSoftObjectPtr<class USkeletalMesh> PreviewSkeletalMesh;
#endif
};
