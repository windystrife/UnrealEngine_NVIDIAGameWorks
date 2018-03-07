// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPropertyTableCellPresenter
{
public:
	virtual ~IPropertyTableCellPresenter() { }

	virtual TSharedRef< class SWidget > ConstructDisplayWidget() = 0;

	virtual bool RequiresDropDown() = 0;

	virtual TSharedRef< class SWidget > ConstructEditModeCellWidget() = 0;

	virtual TSharedRef< class SWidget > ConstructEditModeDropDownWidget() = 0;

	virtual TSharedRef< class SWidget > WidgetToFocusOnEdit() = 0;

	virtual FString GetValueAsString() = 0;

	virtual FText GetValueAsText() = 0;

	virtual bool HasReadOnlyEditMode() = 0;
};
