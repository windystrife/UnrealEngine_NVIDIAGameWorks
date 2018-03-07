// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Margin.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "SSplitter.h"

class SScrollBar;

namespace EColumnSortPriority
{
	enum Type
	{
		Primary = 0,
		Secondary = 1,
		Max,
	};
};


namespace EColumnSortMode
{
	enum Type
	{
		/** Unsorted */
		None = 0,

		/** Ascending */
		Ascending = 1,

		/** Descending */
		Descending = 2,
	};
};


namespace EColumnSizeMode
{
	enum Type
	{
		/** Column stretches to a fraction of the header row */
		Fill = 0,
		
		/**	Column is fixed width and cannot be resized */
		Fixed = 1,

		/** Column is set to a width which can be user-sized */
		Manual = 2,
	};
};


enum class EHeaderComboVisibility
{
	/** Always show the drop down at full opacity */
	Always,

	/** Always show the drop down, but in a ghosted way when not hovered */
	Ghosted,

	/** Only show the drop down when hovered */
	OnHover,
};


/** Callback when sort mode changes */
DECLARE_DELEGATE_ThreeParams( FOnSortModeChanged, EColumnSortPriority::Type, const FName&, EColumnSortMode::Type );

/**	Callback when the width of the column changes */
DECLARE_DELEGATE_OneParam( FOnWidthChanged, float );

/**	Callback to fetch the max row width for a specified column id */
DECLARE_DELEGATE_RetVal_TwoParams(FVector2D, FOnGetMaxRowSizeForColumn, const FName&, EOrientation);

/**
 * The header that appears above lists and trees when they are showing multiple columns.
 */
class SLATE_API SHeaderRow : public SBorder
{
public:
	/** Describes a single column header */
	class FColumn
	{
	public:

		HACK_SLATE_SLOT_ARGS(FColumn)
			: _ColumnId()
			, _DefaultLabel()
			, _DefaultTooltip()
			, _FillWidth( 1.0f )
			, _FixedWidth()
			, _ManualWidth()
			, _OnWidthChanged()
			, _HeaderContent()
			, _HAlignHeader( HAlign_Fill )
			, _VAlignHeader( VAlign_Fill )
			, _HeaderContentPadding()
			, _HeaderComboVisibility(EHeaderComboVisibility::OnHover)
			, _MenuContent()
			, _HAlignCell( HAlign_Fill )
			, _VAlignCell( VAlign_Fill )
			, _SortMode( EColumnSortMode::None )
			, _OnSort()
			{}
			SLATE_ARGUMENT( FName, ColumnId )
			SLATE_ATTRIBUTE( FText, DefaultLabel )
			SLATE_ATTRIBUTE( FText, DefaultTooltip )
			SLATE_ATTRIBUTE( float, FillWidth )
			SLATE_ARGUMENT( TOptional< float >, FixedWidth )
			SLATE_ATTRIBUTE( float, ManualWidth )
			SLATE_EVENT( FOnWidthChanged, OnWidthChanged )

			SLATE_DEFAULT_SLOT( FArguments, HeaderContent )
			SLATE_ARGUMENT( EHorizontalAlignment, HAlignHeader )
			SLATE_ARGUMENT( EVerticalAlignment, VAlignHeader )
			SLATE_ARGUMENT( TOptional< FMargin >, HeaderContentPadding )
			SLATE_ARGUMENT( EHeaderComboVisibility, HeaderComboVisibility )
			SLATE_NAMED_SLOT( FArguments, MenuContent )

			SLATE_ARGUMENT( EHorizontalAlignment, HAlignCell )
			SLATE_ARGUMENT( EVerticalAlignment, VAlignCell )

			SLATE_ATTRIBUTE( EColumnSortMode::Type, SortMode )
			SLATE_ATTRIBUTE( EColumnSortPriority::Type, SortPriority )
			SLATE_EVENT( FOnSortModeChanged, OnSort )

			SLATE_ATTRIBUTE(bool, ShouldGenerateWidget)
		SLATE_END_ARGS()

		FColumn( const FArguments& InArgs )
			: ColumnId( InArgs._ColumnId )
			, DefaultText( InArgs._DefaultLabel )
			, DefaultTooltip( InArgs._DefaultTooltip )
			, Width( 1.0f )
			, DefaultWidth( 1.0f )
			, OnWidthChanged( InArgs._OnWidthChanged)
			, SizeRule( EColumnSizeMode::Fill )
			, HeaderContent( InArgs._HeaderContent )
			, HeaderMenuContent( InArgs._MenuContent )
			, HeaderHAlignment( InArgs._HAlignHeader )
			, HeaderVAlignment( InArgs._VAlignHeader )
			, HeaderContentPadding( InArgs._HeaderContentPadding )
			, HeaderComboVisibility (InArgs._HeaderComboVisibility )
			, CellHAlignment( InArgs._HAlignCell )
			, CellVAlignment( InArgs._VAlignCell )
			, SortMode( InArgs._SortMode )
			, SortPriority( InArgs._SortPriority )
			, OnSortModeChanged( InArgs._OnSort )
			, ShouldGenerateWidget(InArgs._ShouldGenerateWidget)
		{
			if ( InArgs._FixedWidth.IsSet() )
			{
				Width = InArgs._FixedWidth.GetValue();
				SizeRule = EColumnSizeMode::Fixed;
			}
			else if ( InArgs._ManualWidth.IsSet() )
			{
				Width = InArgs._ManualWidth;
				SizeRule = EColumnSizeMode::Manual;
			}
			else
			{
				Width = InArgs._FillWidth;
				SizeRule = EColumnSizeMode::Fill;
			}

			DefaultWidth = Width.Get();
		}

	public:

		void SetWidth( float NewWidth )
		{
			if ( OnWidthChanged.IsBound() )
			{
				OnWidthChanged.Execute( NewWidth );
			}
			else
			{
				Width = NewWidth;
			}
		}

		float GetWidth() const
		{
			return Width.Get();
		}

		/** A unique ID for this column, so that it can be saved and restored. */
		FName ColumnId;

		/** Default text to use if no widget is passed in. */
		TAttribute< FText > DefaultText;

		/** Default tooltip to use if no widget is passed in */
		TAttribute< FText > DefaultTooltip;

		/** A column width in Slate Units */
		TAttribute< float > Width;

		/** A original column width in Slate Units */
		float DefaultWidth;

		FOnWidthChanged OnWidthChanged;

		EColumnSizeMode::Type SizeRule;

		TAlwaysValidWidget HeaderContent;	
		TAlwaysValidWidget HeaderMenuContent;

		EHorizontalAlignment HeaderHAlignment;
		EVerticalAlignment HeaderVAlignment;
		TOptional< FMargin > HeaderContentPadding;
		EHeaderComboVisibility HeaderComboVisibility;

		EHorizontalAlignment CellHAlignment;
		EVerticalAlignment CellVAlignment;

		TAttribute< EColumnSortMode::Type > SortMode;
		TAttribute< EColumnSortPriority::Type > SortPriority;
		FOnSortModeChanged OnSortModeChanged;

		TAttribute<bool> ShouldGenerateWidget;
	};

	/** Create a column with the specified ColumnId */
	static FColumn::FArguments Column( const FName& InColumnId )
	{
		FColumn::FArguments NewArgs;
		NewArgs.ColumnId( InColumnId ); 
		return NewArgs;
	}

	DECLARE_EVENT_OneParam( SHeaderRow, FColumnsChanged, const TSharedRef< SHeaderRow >& );
	FColumnsChanged* OnColumnsChanged() { return &ColumnsChanged; }

	SLATE_BEGIN_ARGS(SHeaderRow)
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FHeaderRowStyle>("TableView.Header") )
		, _ResizeMode(ESplitterResizeMode::Fill)
		{}

		SLATE_STYLE_ARGUMENT( FHeaderRowStyle, Style )
		SLATE_SUPPORTS_SLOT_WITH_ARGS( FColumn )	
		SLATE_EVENT( FColumnsChanged::FDelegate, OnColumnsChanged )
		SLATE_EVENT(FOnGetMaxRowSizeForColumn, OnGetMaxRowSizeForColumn)
		SLATE_ARGUMENT(ESplitterResizeMode::Type, ResizeMode)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** Restore the columns to their original width */
	void ResetColumnWidths();

	/** @return the Columns driven by the column headers */
	const TIndirectArray<FColumn>& GetColumns() const;

	/** Adds a column to the header */
	void AddColumn( const FColumn::FArguments& NewColumnArgs );
	void AddColumn( FColumn& NewColumn );

	/** Inserts a column at the specified index in the header */
	void InsertColumn( const FColumn::FArguments& NewColumnArgs, int32 InsertIdx );
	void InsertColumn( FColumn& NewColumn, int32 InsertIdx );

	/** Removes a column from the header */
	void RemoveColumn( const FName& InColumnId );

	/** Force refreshing of the column widgets*/
	void RefreshColumns();

	/** Removes all columns from the header */
	void ClearColumns();

	void SetAssociatedVerticalScrollBar( const TSharedRef< SScrollBar >& ScrollBar, const float ScrollBarSize );

	/** Sets the column, with the specified name, to the desired width */
	void SetColumnWidth( const FName& InColumnId, float InWidth );

	/** Will return the size for this row at the specified slot index */
	FVector2D GetRowSizeForSlotIndex(int32 SlotIndex) const;

	/** Simple function to set the delegate to fetch the max row size for column id */
	void SetOnGetMaxRowSizeForColumn(const FOnGetMaxRowSizeForColumn& Delegate) { OnGetMaxRowSizeForColumn = Delegate; }

private:

	/** Regenerates all widgets in the header */
	void RegenerateWidgets();

	/** Information about the various columns */
	TIndirectArray<FColumn> Columns;
	TArray<TSharedPtr<class STableColumnHeader>> HeaderWidgets;

	FVector2D ScrollBarThickness;
	TAttribute< EVisibility > ScrollBarVisibility;
	const FHeaderRowStyle* Style;
	FColumnsChanged ColumnsChanged;
	ESplitterResizeMode::Type ResizeMode;
	FOnGetMaxRowSizeForColumn OnGetMaxRowSizeForColumn;
};
