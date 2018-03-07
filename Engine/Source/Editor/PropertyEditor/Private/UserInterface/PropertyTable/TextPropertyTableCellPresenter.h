// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/SWidget.h"
#include "EditorStyleSet.h"
#include "IPropertyTableCellPresenter.h"
#include "UserInterface/PropertyTable/PropertyTableConstants.h"

class FTextPropertyTableCellPresenter : public TSharedFromThis< FTextPropertyTableCellPresenter >, public IPropertyTableCellPresenter
{
public:

	FTextPropertyTableCellPresenter( const TSharedRef< class FPropertyEditor >& InPropertyEditor, const TSharedRef< class IPropertyTableUtilities >& InPropertyUtilities, FSlateFontInfo InFont = FEditorStyle::GetFontStyle( PropertyTableConstants::NormalFontStyle ) );

	virtual ~FTextPropertyTableCellPresenter() {}

	virtual TSharedRef< class SWidget > ConstructDisplayWidget() override;

	virtual bool RequiresDropDown() override;

	virtual TSharedRef< class SWidget > ConstructEditModeCellWidget() override;

	virtual TSharedRef< class SWidget > ConstructEditModeDropDownWidget() override;

	virtual TSharedRef< class SWidget > WidgetToFocusOnEdit() override;

	virtual FString GetValueAsString() override;

	virtual FText GetValueAsText() override;

	virtual bool HasReadOnlyEditMode() override { return HasReadOnlyEditingWidget; }


private:

	bool CalculateIfUsingReadOnlyEditingWidget() const;


private:

	TSharedPtr< class SWidget > PropertyWidget; 

	TSharedRef< class FPropertyEditor > PropertyEditor;
	TSharedRef< class IPropertyTableUtilities > PropertyUtilities;

	bool HasReadOnlyEditingWidget;
	FSlateFontInfo Font;
};
