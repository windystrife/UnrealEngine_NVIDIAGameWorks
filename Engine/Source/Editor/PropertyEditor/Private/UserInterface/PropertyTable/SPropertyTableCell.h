// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "IPropertyTableCellPresenter.h"
#include "Widgets/SWindow.h"
#include "IPropertyTableCell.h"

class FPaintArgs;
class FSlateWindowElementList;
class SMenuAnchor;

class SPropertyTableCell : public SCompoundWidget
{
	public:

	SLATE_BEGIN_ARGS( SPropertyTableCell ) 
		: _Presenter( NULL )
		, _Style( TEXT("PropertyTable") )
	{}
		SLATE_ARGUMENT( TSharedPtr< IPropertyTableCellPresenter >, Presenter)
		SLATE_ARGUMENT( FName, Style )
	SLATE_END_ARGS()

	SPropertyTableCell()
		: DropDownAnchor( NULL )
		, Presenter( NULL )
		, Cell( NULL )
		, bEnterEditingMode( false )
		, Style()
	{ }

	void Construct( const FArguments& InArgs, const TSharedRef< class IPropertyTableCell >& InCell );

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;


private:

	TSharedRef< SWidget > ConstructCellContents();

	void SetContent( const TSharedRef< SWidget >& NewContents );

	TSharedRef< class SWidget > ConstructEditModeCellWidget();

	TSharedRef< class SWidget > ConstructEditModeDropDownWidget();

	TSharedRef< class SBorder > ConstructInvalidPropertyWidget();

	const FSlateBrush* GetCurrentCellBorder() const;

	void OnAnchorWindowClosed( const TSharedRef< SWindow >& WindowClosing );

	void EnteredEditMode();

	void ExitedEditMode();

	void OnCellValueChanged( UObject* Object, FPropertyChangedEvent& PropertyChangedEvent );

	/** One-off active timer to trigger entering the editing mode */
	EActiveTimerReturnType TriggerEnterEditingMode(double InCurrentTime, float InDeltaTime);

private:

	TSharedPtr< SMenuAnchor > DropDownAnchor;

	TSharedPtr< class IPropertyTableCellPresenter > Presenter;
	TSharedPtr< class IPropertyTableCell > Cell;
	bool bEnterEditingMode;
	FName Style;

	const FSlateBrush* CellBackground;
};
