// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Margin.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"

class FPropertyNode;

class SPropertyTreeCategoryRow : public STableRow< TSharedPtr< class FPropertyNode* > >
{
public:

	SLATE_BEGIN_ARGS( SPropertyTreeCategoryRow )
		: _DisplayName()
	{}
		SLATE_ARGUMENT( FText, DisplayName )

	SLATE_END_ARGS()


	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable )
	{
		STableRow< TSharedPtr< class FPropertyNode* > >::Construct(
			STableRow< TSharedPtr< class FPropertyNode* > >::FArguments()
				.Padding(3)
				[
					SNew(SBorder)
					.VAlign(VAlign_Center)
					.BorderImage(FEditorStyle::GetBrush("PropertyWindow.CategoryBackground"))
					.Padding(FMargin(5,1,5,0))
					.ForegroundColor(FEditorStyle::GetColor("PropertyWindow.CategoryForeground"))
					[
						SNew( STextBlock )
						.Text( InArgs._DisplayName )
						.Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::CategoryFontStyle ) )
					]
				]
			, InOwnerTable );
	}
};
