// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "KismetPins/SGraphPinObject.h"

/////////////////////////////////////////////////////
// SGraphPinClass

class SGraphPinClass : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SGraphPinClass) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
protected:

	// Called when a new class was picked via the asset picker
	void OnPickedNewClass(UClass* ChosenClass);

	//~ Begin SGraphPinObject Interface
	virtual FReply OnClickUse() override;
	virtual bool AllowSelfPinWidget() const override { return false; }
	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	virtual FText GetDefaultComboText() const override;
	virtual FOnClicked GetOnUseButtonDelegate() override;
	virtual const FAssetData& GetAssetData(bool bRuntimePath) const override;
	//~ End SGraphPinObject Interface

	/** Cached AssetData without the _C */
	mutable FAssetData CachedEditorAssetData;
};
