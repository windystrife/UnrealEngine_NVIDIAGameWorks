// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Fonts/SlateFontInfo.h"
#include "EditorStyleSet.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"

class FPropertyEditor;

class SPropertyEditorSet : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyEditorSet)
		: _Font(FEditorStyle::GetFontStyle(PropertyEditorConstants::PropertyFontStyle))
		{}
		SLATE_ATTRIBUTE(FSlateFontInfo, Font)
	SLATE_END_ARGS()

	static bool Supports(const TSharedRef< class FPropertyEditor >& InPropertyEditor);

	void Construct(const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor);

	void GetDesiredWidth(float& OutMinDesiredWidth, float& OutMaxDesiredWidth);

private:

	FText GetSetTextValue() const;
	FText GetSetTooltipText() const;

	/** @return True if the property can be edited */
	bool CanEdit() const;

private:
	TSharedPtr< FPropertyEditor > PropertyEditor;
};
