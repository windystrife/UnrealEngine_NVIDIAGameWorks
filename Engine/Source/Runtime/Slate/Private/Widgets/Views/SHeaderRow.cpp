// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
 
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Layout/SSplitter.h"
#include "Types/SlateStructs.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"


class STableColumnHeader : public SCompoundWidget
{
public:
	 
	SLATE_BEGIN_ARGS(STableColumnHeader)
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FTableColumnHeaderStyle>("TableView.Header.Column") )
		{}
		SLATE_STYLE_ARGUMENT( FTableColumnHeaderStyle, Style )

	SLATE_END_ARGS()

	STableColumnHeader()
		: SortMode( EColumnSortMode::None )
		, SortPriority( EColumnSortPriority::Primary )
		, OnSortModeChanged()
		, ContextMenuContent( SNullWidget::NullWidget )
		, ColumnId( NAME_None )
		, Style( nullptr )
	{

	}

	/**
	 * Construct the widget
	 * 
	 * @param InDeclaration   A declaration from which to construct the widget
	 */
	void Construct( const STableColumnHeader::FArguments& InArgs, const SHeaderRow::FColumn& Column, const FMargin DefaultHeaderContentPadding )
	{
		check(InArgs._Style);

		SWidget::Construct( InArgs._ToolTipText, InArgs._ToolTip, InArgs._Cursor, InArgs._IsEnabled, InArgs._Visibility, InArgs._RenderTransform, InArgs._RenderTransformPivot, InArgs._Tag, InArgs._ForceVolatile, InArgs._Clipping, InArgs.MetaData );

		Style = InArgs._Style;
		ColumnId = Column.ColumnId;
		SortMode = Column.SortMode;
		SortPriority = Column.SortPriority;

		OnSortModeChanged = Column.OnSortModeChanged;
		ContextMenuContent = Column.HeaderMenuContent.Widget;

		ComboVisibility = Column.HeaderComboVisibility;

		FMargin AdjustedDefaultHeaderContentPadding = DefaultHeaderContentPadding;

		TAttribute< FText > LabelText = Column.DefaultText;
		TAttribute< FText > TooltipText = Column.DefaultTooltip;

		if (Column.HeaderContent.Widget == SNullWidget::NullWidget)
		{
			if (!Column.DefaultText.IsSet())
			{
				LabelText = FText::FromString( Column.ColumnId.ToString() + TEXT("[LabelMissing]") );
			}

			if (!Column.DefaultTooltip.IsSet())
			{
				TooltipText = LabelText;
			}
		}

		TSharedPtr< SHorizontalBox > Box;
		TSharedRef< SOverlay > Overlay = SNew( SOverlay );

		Overlay->AddSlot( 0 )
			[
				SAssignNew( Box, SHorizontalBox )
			];

		TSharedRef< SWidget > PrimaryContent = Column.HeaderContent.Widget;
		if ( PrimaryContent == SNullWidget::NullWidget )
		{
			PrimaryContent = 
				SNew( SBox )
				.Padding( OnSortModeChanged.IsBound() ? FMargin( 0, 2, 0, 2 ) : FMargin( 0, 4, 0, 4 ) )
				.VAlign( VAlign_Center )
				[
					SNew(STextBlock)
					.Text( LabelText )
					.ToolTipText( TooltipText )
				];
		}

		if ( OnSortModeChanged.IsBound() )
		{
			//optional main button with the column's title. Used to toggle sorting modes.
			PrimaryContent = SNew(SButton)
			.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
			.ForegroundColor( FSlateColor::UseForeground() )
			.ContentPadding( FMargin( 0, 2, 0, 2 ) )
			.OnClicked(this, &STableColumnHeader::OnTitleClicked)
			[
				PrimaryContent
			];
		}
		
		Box->AddSlot()
		.FillWidth(1.0f)
		[
			PrimaryContent
		];

		if( Column.HeaderMenuContent.Widget != SNullWidget::NullWidget )
		{
			// Add Drop down menu button (only if menu content has been specified)
			Box->AddSlot()
			.AutoWidth()
			[
				SAssignNew( MenuOverlay, SOverlay )
				.Visibility( this, &STableColumnHeader::GetMenuOverlayVisibility )
				+SOverlay::Slot()
				[
					SNew( SSpacer )
					.Size( FVector2D( 12.0f, 0 ) )
				]

				+SOverlay::Slot()
				.Padding(FMargin(0,1,0,1))
				[
					SNew( SBorder )
					.Padding( FMargin( 0, 0, AdjustedDefaultHeaderContentPadding.Right, 0 ) )
					.BorderImage( this, &STableColumnHeader::GetComboButtonBorderBrush )
					[
						SAssignNew( ComboButton, SComboButton)
						.HasDownArrow(false)
						.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
						.ContentPadding( FMargin(0) )
						.ButtonContent()
						[
							SNew( SSpacer )
							.Size( FVector2D( 14.0f, 0 ) )
						]
						.MenuContent()
						[
							ContextMenuContent
						]
					]
				]

				+SOverlay::Slot()
				.Padding(FMargin(0,0,0,2))
				.HAlign( HAlign_Center )
				.VAlign( VAlign_Bottom )
				[
					SNew(SImage)
					.Image( &Style->MenuDropdownImage )
					.ColorAndOpacity( this, &STableColumnHeader::GetComboButtonTint )
					.Visibility( EVisibility::HitTestInvisible )
				]
			];		

			AdjustedDefaultHeaderContentPadding.Right = 0;
		}

		Overlay->AddSlot( 1 )
			.HAlign(HAlign_Center)
			.VAlign( VAlign_Top )
			.Padding( FMargin( 0, 2, 0, 0 ) )
			[
				SNew(SImage)
				.Image( this, &STableColumnHeader::GetSortingBrush )
				.Visibility( this, &STableColumnHeader::GetSortModeVisibility )
			];

		this->ChildSlot
		[
			SNew( SBorder )
			.BorderImage( this, &STableColumnHeader::GetHeaderBackgroundBrush )
			.HAlign( Column.HeaderHAlignment )
			.VAlign( Column.HeaderVAlignment )
			.Padding( Column.HeaderContentPadding.Get( AdjustedDefaultHeaderContentPadding ) )
			[
				Overlay
			]
		];
	}

	/** Gets sorting mode */
	EColumnSortMode::Type GetSortMode() const
	{
		return SortMode.Get();
	}

	/** Sets sorting mode */
	void SetSortMode( EColumnSortMode::Type NewMode )
	{
		SortMode = NewMode;
	}

	/** Gets sorting order */
	EColumnSortPriority::Type GetSortPriority() const
	{
		return SortPriority.Get();
	}

	/** Sets sorting order */
	void SetSortPriority(EColumnSortPriority::Type NewPriority)
	{
		SortPriority = NewPriority;
	}

	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ContextMenuContent != SNullWidget::NullWidget )
		{
			OpenContextMenu( MouseEvent );
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	EVisibility GetMenuOverlayVisibility() const
	{
		if (ComboVisibility == EHeaderComboVisibility::OnHover)
		{
			if (!ComboButton.IsValid() || !(IsHovered() || ComboButton->IsOpen()))
			{
				return EVisibility::Collapsed;
			}
		}

		return EVisibility::Visible;
	}

	const FVector2D& GetMenuOverlaySize() const 
	{ 
		return MenuOverlay.IsValid() ? MenuOverlay->GetDesiredSize() : FVector2D::ZeroVector; 
	}

private:


	const FSlateBrush* GetHeaderBackgroundBrush() const
	{
		if ( IsHovered() && SortMode.IsBound() )
		{
			return &Style->HoveredBrush;
		}

		return &Style->NormalBrush;
	}
	
	const FSlateBrush* GetComboButtonBorderBrush() const
	{
		if ( ComboButton.IsValid() && ( ComboButton->IsHovered() || ComboButton->IsOpen() ) )
		{
			return &Style->MenuDropdownHoveredBorderBrush;
		}

		if ( IsHovered() || ComboVisibility == EHeaderComboVisibility::Always )
		{
			return &Style->MenuDropdownNormalBorderBrush;
		}

		return FStyleDefaults::GetNoBrush();
	}

	FSlateColor GetComboButtonTint() const
	{
		if (!ComboButton.IsValid())
		{
			return FLinearColor::White;
		}

		switch (ComboVisibility)
		{
		case EHeaderComboVisibility::Always:
			{
				return FLinearColor::White;
			}

		case EHeaderComboVisibility::Ghosted:
			if ( ComboButton->IsHovered() || ComboButton->IsOpen() )
			{
				return FLinearColor::White;
			}
			else
			{
				return FLinearColor::White.CopyWithNewOpacity(0.5f);
			}

		case EHeaderComboVisibility::OnHover:
			if ( IsHovered() || ComboButton->IsHovered() || ComboButton->IsOpen() )
			{
				return FLinearColor::White;
			}
			break;

		default:
			break;
		}
		
		return FLinearColor::Transparent;
	}

	/** Gets the icon associated with the current sorting mode */
	const FSlateBrush* GetSortingBrush() const
	{
		EColumnSortPriority::Type ColumnSortPriority = SortPriority.Get();

		return ( SortMode.Get() == EColumnSortMode::Ascending ? 
			(SortPriority.Get() == EColumnSortPriority::Secondary ? &Style->SortSecondaryAscendingImage : &Style->SortPrimaryAscendingImage) :
			(SortPriority.Get() == EColumnSortPriority::Secondary ? &Style->SortSecondaryDescendingImage : &Style->SortPrimaryDescendingImage) );
	}

	/** Checks if sorting mode has been selected */
	EVisibility GetSortModeVisibility() const
	{
		return (SortMode.Get() != EColumnSortMode::None) ? EVisibility::HitTestInvisible : EVisibility::Hidden;
	}
	
	/** Called when the column title has been clicked to change sorting mode */
	FReply OnTitleClicked()
	{
		if ( OnSortModeChanged.IsBound() )
		{
			const bool bIsShiftClicked = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
			EColumnSortPriority::Type ColumnSortPriority = SortPriority.Get();
			EColumnSortMode::Type ColumnSortMode = SortMode.Get();
			if (ColumnSortMode == EColumnSortMode::None)
			{
				if (bIsShiftClicked && SortPriority.IsBound())
				{
					ColumnSortPriority = EColumnSortPriority::Secondary;
				}
				else
				{
					ColumnSortPriority = EColumnSortPriority::Primary;
				}

				ColumnSortMode = EColumnSortMode::Ascending;
			}
			else
			{
				if (!bIsShiftClicked && ColumnSortPriority == EColumnSortPriority::Secondary)
				{
					ColumnSortPriority = EColumnSortPriority::Primary;
				}

				if (ColumnSortMode == EColumnSortMode::Descending)
				{
					ColumnSortMode = EColumnSortMode::Ascending;
				}
				else
				{
					ColumnSortMode = EColumnSortMode::Descending;
				}
			}

			OnSortModeChanged.Execute(ColumnSortPriority, ColumnId, ColumnSortMode);
		}

		return FReply::Handled();
	}

	void OpenContextMenu(const FPointerEvent& MouseEvent)
	{
		if ( ContextMenuContent != SNullWidget::NullWidget )
		{
			const FVector2D& SummonLocation = MouseEvent.GetScreenSpacePosition();
			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, ContextMenuContent, SummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
	}


private:

	/** Current sorting mode */
	TAttribute< EColumnSortMode::Type > SortMode;

	/** Current sorting order */
	TAttribute< EColumnSortPriority::Type > SortPriority;

	/** Callback triggered when sorting mode changes */
	FOnSortModeChanged OnSortModeChanged;

	TSharedRef< SWidget > ContextMenuContent;

	TSharedPtr< SComboButton > ComboButton;
	
	/** The visibility method of the combo button */
	EHeaderComboVisibility ComboVisibility;

	TSharedPtr< SOverlay > MenuOverlay;

	FName ColumnId;

	const FTableColumnHeaderStyle* Style;
};

void SHeaderRow::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	SWidget::Construct( InArgs._ToolTipText, InArgs._ToolTip, InArgs._Cursor, InArgs._IsEnabled, InArgs._Visibility, InArgs._RenderTransform, InArgs._RenderTransformPivot, InArgs._Tag, InArgs._ForceVolatile, InArgs._Clipping, InArgs.MetaData );

	ScrollBarThickness = FVector2D::ZeroVector;
	ScrollBarVisibility = EVisibility::Collapsed;
	Style = InArgs._Style;
	OnGetMaxRowSizeForColumn = InArgs._OnGetMaxRowSizeForColumn;
	ResizeMode = InArgs._ResizeMode;

	if ( InArgs._OnColumnsChanged.IsBound() )
	{
		ColumnsChanged.Add( InArgs._OnColumnsChanged );
	}

	SBorder::Construct( SBorder::FArguments()
		.Padding( 0 )
		.BorderImage( &Style->BackgroundBrush )
		.ForegroundColor( Style->ForegroundColor )
	);

	// Copy all the column info from the declaration
	bool bHaveFillerColumn = false;
	for ( int32 SlotIndex=0; SlotIndex < InArgs.Slots.Num(); ++SlotIndex )
	{
		FColumn* const Column = InArgs.Slots[SlotIndex];
		Columns.Add( Column );
	}

	// Generate widgets for all columns
	RegenerateWidgets();
}

void SHeaderRow::ResetColumnWidths()
{
	for ( int32 iColumn = 0; iColumn < Columns.Num(); iColumn++ )
	{
		FColumn& Column = Columns[iColumn];
		Column.SetWidth( Column.DefaultWidth );
	}
}

const TIndirectArray<SHeaderRow::FColumn>& SHeaderRow::GetColumns() const
{
	return Columns;
}

void SHeaderRow::AddColumn( const FColumn::FArguments& NewColumnArgs )
{
	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn( NewColumnArgs );
	AddColumn( *NewColumn );
}

void SHeaderRow::AddColumn( SHeaderRow::FColumn& NewColumn )
{
	int32 InsertIdx = Columns.Num();
	InsertColumn( NewColumn, InsertIdx );
}

void SHeaderRow::InsertColumn( const FColumn::FArguments& NewColumnArgs, int32 InsertIdx )
{
	SHeaderRow::FColumn* NewColumn = new SHeaderRow::FColumn( NewColumnArgs );
	InsertColumn( *NewColumn, InsertIdx );
}

void SHeaderRow::InsertColumn( FColumn& NewColumn, int32 InsertIdx )
{
	check(NewColumn.ColumnId != NAME_None);

	if ( Columns.Num() > 0 && Columns[Columns.Num() - 1].ColumnId == NAME_None )
	{
		// Insert before the filler column, or where the filler column used to be if we replaced it.
		InsertIdx--;
	}

	Columns.Insert( &NewColumn, InsertIdx );
	ColumnsChanged.Broadcast( SharedThis( this ) );

	RegenerateWidgets();
}

void SHeaderRow::RemoveColumn( const FName& InColumnId )
{
	check(InColumnId != NAME_None);

	bool bHaveFillerColumn = false;
	for ( int32 SlotIndex=Columns.Num() - 1; SlotIndex >= 0; --SlotIndex )
	{
		FColumn& Column = Columns[SlotIndex];
		if ( Column.ColumnId == InColumnId )
		{
			Columns.RemoveAt(SlotIndex);
		}
	}

	ColumnsChanged.Broadcast( SharedThis( this ) );
	RegenerateWidgets();
}

void SHeaderRow::RefreshColumns()
{
	RegenerateWidgets();
}

void SHeaderRow::ClearColumns()
{
	Columns.Empty();
	ColumnsChanged.Broadcast( SharedThis( this ) );

	RegenerateWidgets();
}

void SHeaderRow::SetAssociatedVerticalScrollBar( const TSharedRef< SScrollBar >& ScrollBar, const float ScrollBarSize )
{
	ScrollBarThickness.X = ScrollBarSize;
	ScrollBarVisibility.Bind( TAttribute< EVisibility >::FGetter::CreateSP( ScrollBar, &SScrollBar::ShouldBeVisible ) );
	RegenerateWidgets();
}

void SHeaderRow::SetColumnWidth( const FName& InColumnId, float InWidth )
{
	check(InColumnId != NAME_None);

	for ( int32 SlotIndex=Columns.Num() - 1; SlotIndex >= 0; --SlotIndex )
	{
		FColumn& Column = Columns[SlotIndex];
		if ( Column.ColumnId == InColumnId )
		{
			Column.SetWidth( InWidth );
		}
	}
}

FVector2D SHeaderRow::GetRowSizeForSlotIndex(int32 SlotIndex) const
{
	if (Columns.IsValidIndex(SlotIndex))
	{
		const TSharedPtr<STableColumnHeader>& HeaderWidget = HeaderWidgets[SlotIndex];
		const FColumn& Column = Columns[SlotIndex];

		FVector2D HeaderSize = HeaderWidget->GetDesiredSize();

		if (Column.HeaderMenuContent.Widget != SNullWidget::NullWidget && HeaderWidget->GetMenuOverlayVisibility() != EVisibility::Visible)
		{
			HeaderSize += HeaderWidget->GetMenuOverlaySize();
		}

		if (OnGetMaxRowSizeForColumn.IsBound())
		{
			// It's assume that a header is at the top, so the sizing is for the width
			FVector2D MaxChildColumnSize = OnGetMaxRowSizeForColumn.Execute(Column.ColumnId, EOrientation::Orient_Horizontal);

			return MaxChildColumnSize.Component(EOrientation::Orient_Horizontal) < HeaderSize.Component(EOrientation::Orient_Horizontal) ? HeaderSize : MaxChildColumnSize;
		}
	}

	return FVector2D::ZeroVector;
}

void SHeaderRow::RegenerateWidgets()
{
	const float SplitterHandleDetectionSize = 5.0f;
	HeaderWidgets.Empty();

	TSharedPtr<SSplitter> Splitter;

	TSharedRef< SHorizontalBox > Box = 
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		[
			SAssignNew(Splitter, SSplitter)
			.Style( &Style->ColumnSplitterStyle )
			.ResizeMode(ResizeMode)
			.PhysicalSplitterHandleSize( 0.0f )
			.HitDetectionSplitterHandleSize( SplitterHandleDetectionSize )
			.OnGetMaxSlotSize(this, &SHeaderRow::GetRowSizeForSlotIndex)
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 0 )
		[
			SNew( SSpacer )
			.Size( ScrollBarThickness )
			.Visibility( ScrollBarVisibility )
		];

	// Construct widgets for all columns
	{
		const float HalfSplitterDetectionSize = ( SplitterHandleDetectionSize + 2 ) / 2;

		// Populate the slot with widgets that represent the columns.
		TSharedPtr<STableColumnHeader> NewlyMadeHeader;
		for ( int32 SlotIndex=0; SlotIndex < Columns.Num(); ++SlotIndex )
		{
			FColumn& SomeColumn = Columns[SlotIndex];

			if ( SomeColumn.ShouldGenerateWidget.Get(true))
			{
				// Keep track of the last header we created.
				TSharedPtr<STableColumnHeader> PrecedingHeader = NewlyMadeHeader;
				NewlyMadeHeader.Reset();

				FMargin DefaultPadding = FMargin(HalfSplitterDetectionSize, 0, HalfSplitterDetectionSize, 0);

				TSharedRef<STableColumnHeader> NewHeader =
					SAssignNew(NewlyMadeHeader, STableColumnHeader, SomeColumn, DefaultPadding)
					.Style((SlotIndex + 1 == Columns.Num()) ? &Style->LastColumnStyle : &Style->ColumnStyle);

				HeaderWidgets.Add(NewlyMadeHeader);

				switch (SomeColumn.SizeRule)
				{
				case EColumnSizeMode::Fill:
				{
					TAttribute<float> WidthBinding;
					WidthBinding.BindRaw(&SomeColumn, &FColumn::GetWidth);

					// Add resizable cell
					Splitter->AddSlot()
						.Value(WidthBinding)
						.SizeRule(SSplitter::FractionOfParent)
						.OnSlotResized(SSplitter::FOnSlotResized::CreateRaw(&SomeColumn, &FColumn::SetWidth))
						[
							NewHeader
						];
				}
				break;

				case EColumnSizeMode::Fixed:
				{
					// Add fixed size cell
					Splitter->AddSlot()
						.SizeRule(SSplitter::SizeToContent)
						[
							SNew(SBox)
							.WidthOverride(SomeColumn.GetWidth())
						[
							NewHeader
						]
						];
				}
				break;

				case EColumnSizeMode::Manual:
				{
					// Sizing grip to put at the end of the column - we can't use a SSplitter here as it doesn't have the resizing behavior we need
					const float GripSize = 5.0f;
					TSharedRef<SBorder> SizingGrip = SNew(SBorder)
						.Padding(0.0f)
						.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
						.Cursor(EMouseCursor::ResizeLeftRight)
						.Content()
						[
							SNew(SSpacer)
							.Size(FVector2D(GripSize, GripSize))
						];

					TWeakPtr<SBorder> WeakSizingGrip = SizingGrip;
					auto SizingGrip_OnMouseButtonDown = [&SomeColumn, WeakSizingGrip](const FGeometry&, const FPointerEvent&) -> FReply
					{
						TSharedPtr<SBorder> SizingGripPtr = WeakSizingGrip.Pin();
						if (SizingGripPtr.IsValid())
						{
							return FReply::Handled().CaptureMouse(SizingGripPtr.ToSharedRef());
						}
						return FReply::Unhandled();
					};

					auto SizingGrip_OnMouseButtonUp = [&SomeColumn, WeakSizingGrip](const FGeometry&, const FPointerEvent&) -> FReply
					{
						TSharedPtr<SBorder> SizingGripPtr = WeakSizingGrip.Pin();
						if (SizingGripPtr.IsValid() && SizingGripPtr->HasMouseCapture())
						{
							return FReply::Handled().ReleaseMouseCapture();
						}
						return FReply::Unhandled();
					};

					auto SizingGrip_OnMouseMove = [&SomeColumn, WeakSizingGrip](const FGeometry&, const FPointerEvent& InPointerEvent) -> FReply
					{
						TSharedPtr<SBorder> SizingGripPtr = WeakSizingGrip.Pin();
						if (SizingGripPtr.IsValid() && SizingGripPtr->HasMouseCapture())
						{
							// The sizing grip has been moved, so update our columns size from the movement delta
							const float NewWidth = SomeColumn.GetWidth() + InPointerEvent.GetCursorDelta().X;
							SomeColumn.SetWidth(FMath::Max(20.0f, NewWidth));
							return FReply::Handled();
						}
						return FReply::Unhandled();
					};

					// Bind the events to handle the drag sizing
					SizingGrip->SetOnMouseButtonDown(FPointerEventHandler::CreateLambda(SizingGrip_OnMouseButtonDown));
					SizingGrip->SetOnMouseButtonUp(FPointerEventHandler::CreateLambda(SizingGrip_OnMouseButtonUp));
					SizingGrip->SetOnMouseMove(FPointerEventHandler::CreateLambda(SizingGrip_OnMouseMove));

					auto GetColumnWidthAsOptionalSize = [&SomeColumn]() -> FOptionalSize
					{
						const float DesiredWidth = SomeColumn.GetWidth();
						return FOptionalSize(DesiredWidth);
					};

					TAttribute<FOptionalSize> WidthBinding;
					WidthBinding.Bind(TAttribute<FOptionalSize>::FGetter::CreateLambda(GetColumnWidthAsOptionalSize));

					// Add resizable cell
					Splitter->AddSlot()
						.SizeRule(SSplitter::SizeToContent)
						[
							SNew(SBox)
							.WidthOverride(WidthBinding)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									NewHeader
								]
								+ SOverlay::Slot()
								.HAlign(HAlign_Right)
								[
									SizingGrip
								]
							]
						];
				}
				break;

				default:
					break;
				}
			}			
		}
	}

	// Create a box to contain widgets for each column
	SetContent( Box );
}
