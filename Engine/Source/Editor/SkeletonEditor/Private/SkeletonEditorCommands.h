// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FSkeletonEditorCommands : public TCommands<FSkeletonEditorCommands>
{
public:
	FSkeletonEditorCommands()
		: TCommands<FSkeletonEditorCommands>(TEXT("SkeletonEditor"), NSLOCTEXT("Contexts", "SkeletonEditor", "Skeleton Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:

	// skeleton menu options
	// Command to allow users to set the skeletons preview mesh
	TSharedPtr<FUICommandInfo> ChangeSkeletonPreviewMesh;
	// Command to allow users to remove unused bones (not referenced by any skeletalmesh) from the skeleton
	TSharedPtr<FUICommandInfo> RemoveUnusedBones;
	// Command to allow users to look for unused curves (curves that exist in animations that do not drive any morph/materials
	TSharedPtr<FUICommandInfo> TestSkeletonCurveNamesForUse;
	// Command to show Anim Notify window
	TSharedPtr<FUICommandInfo> AnimNotifyWindow;
	// Command to show Retarget Source Manager
	TSharedPtr<FUICommandInfo> RetargetManager;
	// Import Mesh for this Skeleton
	TSharedPtr<FUICommandInfo> ImportMesh;

	// Command to allow users to remove unused bones (not referenced by any skeletalmesh) from the skeleton
	TSharedPtr<FUICommandInfo> UpdateSkeletonRefPose;
};
