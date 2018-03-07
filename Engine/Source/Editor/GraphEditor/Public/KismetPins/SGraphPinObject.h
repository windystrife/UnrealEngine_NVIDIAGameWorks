// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "SGraphPin.h"

class SButton;

class GRAPHEDITOR_API SGraphPinObject : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinObject) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	/** Delegate to be called when the use current selected item in asset browser button is clicked */
	virtual FOnClicked GetOnUseButtonDelegate();
	/** Delegate to be called when the browse for item button is clicked */
	virtual FOnClicked GetOnBrowseButtonDelegate();

	/** Clicked Use button */
	virtual FReply OnClickUse();
	/** Clicked Browse button */
	virtual FReply OnClickBrowse();
	/** Get text tooltip for object */
	FText GetObjectToolTip() const;
	/** Get text tooltip for object */
	FString GetObjectToolTipAsString() const;
	/** Get string value for object */
	FText GetValue() const;
	/** Get name of the object*/
	FText GetObjectName() const;
	/** Get default text for the picker combo */
	virtual FText GetDefaultComboText() const;
	/** Allow self pin widget */
	virtual bool AllowSelfPinWidget() const { return true; }
	/** Generate asset picker window */
	virtual TSharedRef<SWidget> GenerateAssetPicker();
	/** Called to validate selection from picker window */
	virtual void OnAssetSelectedFromPicker(const struct FAssetData& AssetData);

	/** Used to update the combo button text */
	FText OnGetComboTextValue() const;
	/** Combo Button Color and Opacity delegate */
	FSlateColor OnGetComboForeground() const;
	/** Button Color and Opacity delegate */
	FSlateColor OnGetWidgetForeground() const;
	/** Button Color and Opacity delegate */
	FSlateColor OnGetWidgetBackground() const;

	/** Returns asset data of currently selected object, if bRuntimePath is true this will include _C for blueprint classes, for false it will point to UBlueprint instead */
	virtual const FAssetData& GetAssetData(bool bRuntimePath) const;

protected:
	/** Object manipulator buttons. */
	TSharedPtr<SButton> UseButton;
	TSharedPtr<SButton> BrowseButton;

	/** Menu anchor for opening and closing the asset picker */
	TSharedPtr<class SMenuAnchor> AssetPickerAnchor;

	/** Cached AssetData of object selected */
	mutable FAssetData CachedAssetData;
};
