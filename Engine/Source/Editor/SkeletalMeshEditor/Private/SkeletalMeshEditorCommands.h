// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FSkeletalMeshEditorCommands : public TCommands<FSkeletalMeshEditorCommands>
{
public:
	FSkeletalMeshEditorCommands()
		: TCommands<FSkeletalMeshEditorCommands>(TEXT("SkeletalMeshEditor"), NSLOCTEXT("Contexts", "SkeletalMeshEditor", "Skeletal Mesh Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:

	// reimport current mesh
	TSharedPtr<FUICommandInfo> ReimportMesh;

	// selecting mesh section using hit proxies
	TSharedPtr<FUICommandInfo> MeshSectionSelection;
};
