// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

class FClothPainterCommands : public TCommands<FClothPainterCommands>
{
public:
	FClothPainterCommands()
		: TCommands<FClothPainterCommands>(TEXT("ClothPainterTools"), NSLOCTEXT("Contexts", "ClothPainter", "Cloth Painter"), NAME_None, FEditorStyle::GetStyleSetName())
	{

	}

	virtual void RegisterCommands() override;
	static const FClothPainterCommands& Get();

	/** Clothing commands */
	TSharedPtr<FUICommandInfo> TogglePaintMode;
};