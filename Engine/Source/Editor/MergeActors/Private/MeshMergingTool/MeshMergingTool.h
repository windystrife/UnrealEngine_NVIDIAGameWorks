// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Widgets/SWidget.h"
#include "Engine/MeshMerging.h"
#include "IMergeActorsTool.h"

#include "MeshMergingTool.generated.h"

class SMeshMergingDialog;

/** Singleton wrapper to allow for using the setting structure in SSettingsView */
UCLASS(config = Engine)
class UMeshMergingSettingsObject : public UObject
{
	GENERATED_BODY()
public:
	UMeshMergingSettingsObject()		
	{
		Settings.bMergePhysicsData = true;
		// In this case set to AllLODs value since calculating the LODs is not possible and thus disabled in the UI
		Settings.LODSelectionType = EMeshLODSelectionType::AllLODs;
	}

	static UMeshMergingSettingsObject* Get()
	{
		static bool bInitialized = false;
		// This is a singleton, use default object
		UMeshMergingSettingsObject* DefaultSettings = GetMutableDefault<UMeshMergingSettingsObject>();

		if (!bInitialized)
		{
			bInitialized = true;
		}

		return DefaultSettings;
	}

public:
	UPROPERTY(editanywhere, meta = (ShowOnlyInnerProperties), Category = MergeSettings)
	FMeshMergingSettings Settings;
};

/**
 * Mesh Merging Tool
 */
class FMeshMergingTool : public IMergeActorsTool
{
	friend class SMeshMergingDialog;

public:

	FMeshMergingTool();

	// IMergeActorsTool interface
	virtual TSharedRef<SWidget> GetWidget() override;
	virtual FName GetIconName() const override { return "MergeActors.MeshMergingTool"; }
	virtual FText GetTooltipText() const override;
	virtual FString GetDefaultPackageName() const override;
	virtual bool CanMerge() const override;
	virtual bool RunMerge(const FString& PackageName) override;	
private:
	/** Whether to replace source actors with a merged actor in the world */
	bool bReplaceSourceActors;

	/** Pointer to the mesh merging dialog containing settings for the merge */
	TSharedPtr<SMeshMergingDialog> MergingDialog;

	/** Pointer to singleton settings object */
	UMeshMergingSettingsObject* SettingsObject;
};
