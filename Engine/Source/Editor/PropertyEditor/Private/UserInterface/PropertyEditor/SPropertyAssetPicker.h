// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SMenuAnchor.h"

class SPropertyAssetPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPropertyAssetPicker ) {}
		SLATE_EVENT( FOnGetAllowedClasses, OnGetAllowedClasses )
		SLATE_EVENT( FOnAssetSelected, OnAssetSelected )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
private:

	FReply OnClicked();

	TSharedRef<SWidget> OnGenerateAssetPicker();

	void OnAssetSelectedFromPicker( const struct FAssetData& AssetData );
private:
	/** Menu anchor for opening and closing the asset picker */
	TSharedPtr< class SMenuAnchor > AssetPickerAnchor;

	FOnGetAllowedClasses OnGetAllowedClasses;
	FOnAssetSelected OnAssetSelected;
};
