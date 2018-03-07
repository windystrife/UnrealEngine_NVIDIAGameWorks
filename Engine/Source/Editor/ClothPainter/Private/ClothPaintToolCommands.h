// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

namespace ClothPaintToolCommands
{
	void RegisterClothPaintToolCommands();
}

class FClothPaintToolCommands_Gradient : public TCommands<FClothPaintToolCommands_Gradient>
{
public:

	FClothPaintToolCommands_Gradient()
		: TCommands<FClothPaintToolCommands_Gradient>(TEXT("ClothPainter"), NSLOCTEXT("Contexts", "ClothFillTool", "Cloth Painter - Fill Tool"), NAME_None, FEditorStyle::GetStyleSetName())
	{

	}

	virtual void RegisterCommands() override;
	static const FClothPaintToolCommands_Gradient& Get();

	/** Applies the gradient when using the gradient cloth paint tool */
	TSharedPtr<FUICommandInfo> ApplyGradient;
};