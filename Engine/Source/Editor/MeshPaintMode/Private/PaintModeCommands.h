// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FPaintModeCommands : public TCommands<FPaintModeCommands>
{
public:
	FPaintModeCommands() : TCommands<FPaintModeCommands> ( "PaintModeCommands", NSLOCTEXT("PaintMode", "CommandsName", "Paint Mode Commands"), NAME_None, FEditorStyle::GetStyleSetName()) {}

	/**
	* Initialize commands
	*/
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> NextTexture;
	TSharedPtr<FUICommandInfo> PreviousTexture;
	TSharedPtr<FUICommandInfo> CommitTexturePainting;
	
	TSharedPtr<FUICommandInfo> Copy;
	TSharedPtr<FUICommandInfo> Paste;
	TSharedPtr<FUICommandInfo> Remove;
	TSharedPtr<FUICommandInfo> Fix;
	TSharedPtr<FUICommandInfo> Fill;
	TSharedPtr<FUICommandInfo> Propagate;
	TSharedPtr<FUICommandInfo> Import;
	TSharedPtr<FUICommandInfo> Save;

	TSharedPtr<FUICommandInfo> PropagateTexturePaint;
	TSharedPtr<FUICommandInfo> SaveTexturePaint;

	TArray<TSharedPtr<FUICommandInfo>> Commands;
};