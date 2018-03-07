// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessStreamingAccuracyLegend.h: PP to apply the streaming accuracy legend
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "CompositionLighting/PostProcessPassThrough.h"

class FCanvas;

class FRCPassPostProcessStreamingAccuracyLegend : public FRCPassPostProcessPassThrough
{
public:

	FRCPassPostProcessStreamingAccuracyLegend(const TArray<FLinearColor>& InColors)
		: FRCPassPostProcessPassThrough(nullptr), Colors(InColors)
	{}

protected:

	virtual void DrawCustom(FRenderingCompositePassContext& Context) override;

	void DrawDesc(FCanvas& Canvas, float PosX, float PosY, const FText& Text);
	void DrawBox(FCanvas& Canvas, float PosX, float PosY, const FLinearColor& Color, const FText& Text);
	void DrawCheckerBoard(FCanvas& Canvas, float PosX, float PosY, const FLinearColor& Color0, const FLinearColor& Color1, const FText& Text);

private: 

	TArray<FLinearColor> Colors;
};
