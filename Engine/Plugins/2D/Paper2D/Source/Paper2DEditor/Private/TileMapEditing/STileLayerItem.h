// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "PaperTileMap.h"

class SButton;
class UPaperTileLayer;
struct FSlateBrush;

//////////////////////////////////////////////////////////////////////////
// STileLayerItem

class STileLayerItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STileLayerItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, int32 Index, class UPaperTileMap* InMap, FIsSelected InIsSelectedDelegate);

	void BeginEditingName();

protected:
	int32 MyIndex;
	UPaperTileMap* MyMap;
	UPaperTileLayer* GetMyLayer() const { return MyMap->TileLayers[MyIndex]; }

	TSharedPtr<SButton> VisibilityButton;

	const FSlateBrush* EyeClosed;
	const FSlateBrush* EyeOpened;

	TSharedPtr<SInlineEditableTextBlock> LayerNameWidget;

protected:
	FText GetLayerDisplayName() const;
	void OnLayerNameCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	const FSlateBrush* GetVisibilityBrushForLayer() const;
	FSlateColor GetForegroundColorForVisibilityButton() const;
	FReply OnToggleVisibility();
};
