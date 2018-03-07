// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "IPropertyUtilities.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "IPropertyTreeRow.h"

typedef SMultiColumnTableRow< TSharedPtr<class FPropertyNode*> > SPropertyRowBase;

/**
 * A wrapper around property editor if its shown in a tree (I.E SPropertyTreeView)                   
 */
class SPropertyEditorTableRow : public SPropertyRowBase, public IPropertyTreeRow
{
public:

	SLATE_BEGIN_ARGS(SPropertyEditorTableRow)
		: _OnMiddleClicked()
	{}
		SLATE_EVENT( FOnPropertyClicked, OnMiddleClicked )
		SLATE_EVENT( FConstructExternalColumnCell, ConstructExternalColumnCell )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& InPropertyEditor, const TSharedRef< class IPropertyUtilities >& InPropertyUtilities, const TSharedRef<STableViewBase>& InOwnerTable );

	/**
	 * Called to generate a widget for a column in this row
	 * 
	 * @param ColumnName The name of the column to generate a widget for
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;
	
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual TSharedPtr< FPropertyPath > GetPropertyPath() const override;

	virtual bool IsCursorHovering() const override;


private:

	EVisibility OnGetFavoritesVisibility() const;
	const FSlateBrush* OnGetFavoriteImage() const;
	FReply OnToggleFavoriteClicked();

	void OnEditConditionCheckChanged( ECheckBoxState CheckState );
	ECheckBoxState OnGetEditConditionCheckState() const;

	FReply OnNameDoubleClicked();

	TSharedRef< SWidget > ConstructNameColumnWidget();
	TSharedRef< SWidget > ConstructValueColumnWidget();

	TSharedRef<SWidget> ConstructPropertyEditorWidget();

	EVisibility GetDiffersFromDefaultAsVisibility() const;
	const FSlateBrush* GetImageBrush() const;
	FReply OnClickedTestButton();


private:

	/** The property editor in this row */
	TSharedPtr< class FPropertyEditor > PropertyEditor;

	TSharedPtr< class IPropertyUtilities > PropertyUtilities;

	TSharedPtr< SWidget > ValueEditorWidget;

	TSharedPtr< FPropertyPath > PropertyPath;

	FOnPropertyClicked OnMiddleClicked;

	/** Called to construct any the cell contents for columns created by external code*/
	FConstructExternalColumnCell ConstructExternalColumnCell;
};
