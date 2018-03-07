// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FAnimationEditorCommands : public TCommands<FAnimationEditorCommands>
{
public:
	FAnimationEditorCommands()
		: TCommands<FAnimationEditorCommands>(TEXT("AnimationEditor"), NSLOCTEXT("Contexts", "AnimationEditor", "Animation Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:
	// import animation
	TSharedPtr<FUICommandInfo> ImportAnimation;
	// reimport animation
	TSharedPtr<FUICommandInfo> ReimportAnimation;
	// apply compression
	TSharedPtr<FUICommandInfo> ApplyCompression;
	// export to FBX
	TSharedPtr<FUICommandInfo> ExportToFBX_AnimData;
	// export to FBX
	TSharedPtr<FUICommandInfo> ExportToFBX_PreviewMesh;
	// Add looping interpolation
	TSharedPtr<FUICommandInfo> AddLoopingInterpolation;
	// set key for bone track
	TSharedPtr<FUICommandInfo> SetKey;
	// bake bone track curves to animation
	TSharedPtr<FUICommandInfo> ApplyAnimation;
};
