// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SEventGraph.h"
#include "Widgets/Layout/SSplitter.h"
#include "Containers/MapBuilder.h"
#include "Widgets/SOverlay.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "Widgets/StatDragDropOp.h"
#include "Widgets/SEventGraphTooltip.h"
#include "Widgets/Input/SSearchBox.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SEventGraph"


namespace EEventGraphViewModes
{
	FText ToName( const Type EventGraphViewMode )
	{
		switch( EventGraphViewMode )
		{
		case Hierarchical: return LOCTEXT("ViewMode_Name_Hierarchical", "Hierarchical");
		case FlatInclusive: return LOCTEXT("ViewMode_Name_FlatInclusive", "Inclusive");
		case FlatInclusiveCoalesced: return LOCTEXT("ViewMode_Name_FlatInclusiveCoalesced", "Inclusive");
		case FlatExclusive: return LOCTEXT("ViewMode_Name_FlatExclusive", "Exclusive");
		case FlatExclusiveCoalesced: return LOCTEXT("ViewMode_Name_FlatExclusiveCoalesced", "Exclusive" );
		case ClassAggregate: return LOCTEXT("ViewMode_Name_ClassAggregate", "ClassAggregate");

		default: return LOCTEXT("InvalidOrMax", "InvalidOrMax");
		}
	}

	FText ToDescription( const Type EventGraphViewMode )
	{
		switch( EventGraphViewMode )
		{
		case Hierarchical: return LOCTEXT("ViewMode_Desc_Hierarchical", "Hierarchical tree view of the events");
		case FlatInclusive: return LOCTEXT("ViewMode_Desc_Flat", "Flat list of the events, sorted by the inclusive time");
		case FlatInclusiveCoalesced: return LOCTEXT("ViewMode_Desc_FlatCoalesced", "Flat list of the events coalesced by the event name, sorted by the inclusive time");
		case FlatExclusive: return LOCTEXT("ViewMode_Desc_FlatExclusive", "Flat list of the events, sorted by the exclusive time");
		case FlatExclusiveCoalesced: return LOCTEXT("ViewMode_Desc_FlatExclusiveCoalesced", "Flat list of the events coalesced by the event name, sorted by the exclusive time");
		case ClassAggregate: return LOCTEXT("ViewMode_Desc_ClassAggregate", "ClassAggregate @TBD");

		default: return LOCTEXT("InvalidOrMax", "InvalidOrMax");
		}
	}

	FName ToBrushName( const Type EventGraphViewMode )
	{
		switch( EventGraphViewMode )
		{
		case Hierarchical: return TEXT("Profiler.EventGraph.HierarchicalIcon");
		case FlatInclusive: return TEXT("Profiler.EventGraph.FlatIcon");
		case FlatInclusiveCoalesced: return TEXT("Profiler.EventGraph.FlatCoalescedIcon");
		case FlatExclusive: return TEXT("Profiler.EventGraph.FlatIcon");
		case FlatExclusiveCoalesced: return TEXT("Profiler.EventGraph.FlatCoalescedIcon");

		default: return NAME_None;
		}
	}
}

struct FEventGraphColumns
{
	/** Default constructor. */
	FEventGraphColumns()
		: NumColumns( EEventPropertyIndex::None+1 )
	{
		// Make event property is initialized.
		FEventGraphSample::InitializePropertyManagement();

		Collection = new FEventGraphColumn[NumColumns];

		Collection[EEventPropertyIndex::StatName] = FEventGraphColumn
		(
			EEventPropertyIndex::StatName,
			TEXT( "name" ),
			LOCTEXT( "EventNameColumnTitle", "Event Name" ),
			LOCTEXT( "EventNameColumnDesc", "Name of the event" ),
			false, true, true, false, false,
			HAlign_Left,
			0.0f
		);

		Collection[EEventPropertyIndex::InclusiveTimeMS] = FEventGraphColumn
		(
			EEventPropertyIndex::InclusiveTimeMS,
			TEXT( "inc" ),
			LOCTEXT( "InclusiveTimeMSTitle", "Inc Time (MS)" ),
			LOCTEXT( "InclusiveTimeMSDesc", "Duration of the sample and its children, in milliseconds" ),
			false, true, true, true, true,
			HAlign_Right,
			72.0f
		);

		Collection[EEventPropertyIndex::InclusiveTimePct] = FEventGraphColumn
		(
			EEventPropertyIndex::InclusiveTimePct,
			TEXT( "inc%" ),
			LOCTEXT( "InclusiveTimePercentageTitle", "Inc Time (%)" ),
			LOCTEXT( "InclusiveTimePercentageDesc", "Duration of the sample and its children as percent of the caller" ),
			true, true, true, false, false,
			HAlign_Right,
			72.0f
		);

		Collection[EEventPropertyIndex::ExclusiveTimeMS] = FEventGraphColumn
		(
			EEventPropertyIndex::ExclusiveTimeMS,
			TEXT( "exc" ),
			LOCTEXT( "ExclusiveTimeMSTitle", "Exc Time (MS)" ),
			LOCTEXT( "ExclusiveTimeMSDesc", "Exclusive time of this event, in milliseconds" ),
			false, true, true, true, false,
			HAlign_Right,
			72.0f
		);

		Collection[EEventPropertyIndex::ExclusiveTimePct] = FEventGraphColumn
		(
			EEventPropertyIndex::ExclusiveTimePct,
			TEXT( "exc%" ),
			LOCTEXT( "ExclusiveTimePercentageTitle", "Exc Time (%)" ),
			LOCTEXT( "ExclusiveTimePercentageDesc", "Exclusive time of this event as percent of this call's inclusive time" ),
			true, true, true, false, false,
			HAlign_Right,
			72.0f
		);

		Collection[EEventPropertyIndex::NumCallsPerFrame] = FEventGraphColumn
		(
			EEventPropertyIndex::NumCallsPerFrame,
			TEXT( "calls" ),
			LOCTEXT( "CallsPerFrameTitle", "Calls" ),
			LOCTEXT( "CallsPerFrameDesc", "Number of times this event was called" ),
			false, true, true, true, false,
			HAlign_Right,
			48.0f
		);

		// Fake column used as a default column for NAME_None
		Collection[EEventPropertyIndex::None] = FEventGraphColumn
		(
			EEventPropertyIndex::None,
			TEXT( "None" ),
			LOCTEXT( "None", "None" ),
			LOCTEXT( "None", "None" ),
			false, false, false, false, false,
			HAlign_Left,
			0.0f
		);

		ColumnNameToIndexMapping = TMapBuilder<FName, const FEventGraphColumn*>()
			.Add( TEXT( "StatName" ), &Collection[EEventPropertyIndex::StatName] )
			.Add( TEXT( "InclusiveTimeMS" ), &Collection[EEventPropertyIndex::InclusiveTimeMS] )
			.Add( TEXT( "InclusiveTimePct" ), &Collection[EEventPropertyIndex::InclusiveTimePct] )
			.Add( TEXT( "ExclusiveTimeMS" ), &Collection[EEventPropertyIndex::ExclusiveTimeMS] )
			.Add( TEXT( "ExclusiveTimePct" ), &Collection[EEventPropertyIndex::ExclusiveTimePct] )
			.Add( TEXT( "NumCallsPerFrame" ), &Collection[EEventPropertyIndex::NumCallsPerFrame] )
			.Add( NAME_None, &Collection[EEventPropertyIndex::None] )
			;
	}

	~FEventGraphColumns()
	{
		delete[] Collection;
	}

	/** Contains basic information about columns used in the event graph widget. Names should be localized. */
	FEventGraphColumn* Collection;

	const uint32 NumColumns;

	// Generated from a XLSX file.
	TMap<FName, const FEventGraphColumn*> ColumnNameToIndexMapping;

	static const FEventGraphColumns& Get()
	{
		static FEventGraphColumns Instance;
		return Instance;
	}
};

/*-----------------------------------------------------------------------------
	SEventTreeItem
-----------------------------------------------------------------------------*/

DECLARE_DELEGATE_TwoParams( FSetHoveredTableCell, const FName /*ColumnID*/, const FEventGraphSamplePtr /*SamplePtr*/ );
DECLARE_DELEGATE_RetVal_OneParam( bool, FIsColumnVisibleDelegate, const FName /*ColumnID*/ );
DECLARE_DELEGATE_RetVal_OneParam( EHorizontalAlignment, FGetColumnOutlineHAlignmentDelegate, const FName /*ColumnID*/ );

class SEventGraphTableCell : public SCompoundWidget
{
public:	
	SLATE_BEGIN_ARGS( SEventGraphTableCell ){}
		SLATE_EVENT( FSetHoveredTableCell, OnSetHoveredTableCell )
		SLATE_ARGUMENT( FEventGraphSamplePtr, EventPtr )
		SLATE_ARGUMENT( FName, ColumnID )
		SLATE_ARGUMENT( bool, IsEventNameColumn )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.
	 *
	 * @param	InArgs		- the declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<class ITableRow>& TableRow, const TWeakPtr<IEventGraph>& InOwnerEventGraph )
	{
		SetHoveredTableCellDelegate = InArgs._OnSetHoveredTableCell;
		EventPtr = InArgs._EventPtr;
		OwnerEventGraph = InOwnerEventGraph;
		ColumnID = InArgs._ColumnID;

		ChildSlot
		[
			GenerateWidgetForColumnID( ColumnID, InArgs._IsEventNameColumn, TableRow )
		];
	}

protected:
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	TSharedRef<SWidget> GenerateWidgetForColumnID( const FName& InColumnID, const bool bIsEventNameColumn, const TSharedRef<class ITableRow>& TableRow )
	{
		const FEventGraphColumn& Column = *FEventGraphColumns::Get().ColumnNameToIndexMapping.FindChecked( InColumnID );
		
		if( bIsEventNameColumn )
		{
			return 
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew( SExpanderArrow, TableRow )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( SImage )
				.Visibility( this, &SEventGraphTableCell::GetHotPathIconVisibility )
				.Image( FEditorStyle::GetBrush(TEXT("Profiler.EventGraph.HotPathSmall")) )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign( VAlign_Center )
			.HAlign( Column.HorizontalAlignment )
			.Padding( FMargin( 2.0f, 0.0f ) )
			[
				SNew( STextBlock )
				.Text( FText::FromName(EventPtr->_StatName) )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
				.ColorAndOpacity( this, &SEventGraphTableCell::GetColorAndOpacity )
				.ShadowColorAndOpacity( this, &SEventGraphTableCell::GetShadowColorAndOpacity )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( SButton )
				.ButtonStyle( FEditorStyle::Get(), TEXT("HoverHintOnly") )
				.ContentPadding( 0.0f )
				.IsFocusable( false )
				.OnClicked( this, &SEventGraphTableCell::ExpandCulledEvents_OnClicked )
				[
					SNew( SImage )
					.Visibility( this, &SEventGraphTableCell::GetCulledEventsIconVisibility )
					.Image( FEditorStyle::GetBrush("Profiler.EventGraph.HasCulledEventsSmall") )
					.ToolTipText( LOCTEXT("HasCulledEvents_TT","This event contains culled children, if you want to see all children, please disable culling or use function details, or press this icon") )
				]	
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew( SImage )
				.Visibility( this, &SEventGraphTableCell::GetHintIconVisibility )
				.Image( FEditorStyle::GetBrush("Profiler.Tooltip.HintIcon10") )
				.ToolTip( SEventGraphTooltip::GetTableCellTooltip( EventPtr ) )
			];
		}
		else
		{
			const FString FormattedValue = EventPtr->GetFormattedValue( Column.Index );

			return

			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.FillWidth( 1.0f )
			.VAlign( VAlign_Center )
			.HAlign( Column.HorizontalAlignment )
			.Padding( FMargin( 2.0f, 0.0f ) )
			[
				SNew( STextBlock )
				.Text( FText::FromString(FormattedValue) )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
				.ColorAndOpacity( this, &SEventGraphTableCell::GetColorAndOpacity )
				.ShadowColorAndOpacity( this, &SEventGraphTableCell::GetShadowColorAndOpacity )
			];
		}	
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * The system will use this event to notify a widget that the cursor has entered it. This event is NOT bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		SCompoundWidget::OnMouseEnter( MyGeometry, MouseEvent );
		SetHoveredTableCellDelegate.ExecuteIfBound( ColumnID, EventPtr );
	}

	/**
	 * The system will use this event to notify a widget that the cursor has left it. This event is NOT bubbled.
	 *
	 * @param MouseEvent Information about the input event
	 */
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override
	{
		SCompoundWidget::OnMouseLeave( MouseEvent );
		SetHoveredTableCellDelegate.ExecuteIfBound( NAME_None, nullptr );
	}

	/**
	 * Called during drag and drop when the drag enters a widget.
	 *
	 * Enter/Leave events in slate are meant as lightweight notifications.
	 * So we do not want to capture mouse or set focus in response to these.
	 * However, OnDragEnter must also support external APIs (e.g. OLE Drag/Drop)
	 * Those require that we let them know whether we can handle the content
	 * being dragged OnDragEnter.
	 *
	 * The concession is to return a can_handled/cannot_handle
	 * boolean rather than a full FReply.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether the contents of the DragDropEvent can potentially be processed by this widget.
	 */
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
	{
		SCompoundWidget::OnDragEnter( MyGeometry, DragDropEvent );
		SetHoveredTableCellDelegate.ExecuteIfBound( ColumnID, EventPtr );
	}
	
	/**
	 * Called during drag and drop when the drag leaves a widget.
	 *
	 * @param DragDropEvent   The drag and drop event.
	 */
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent )  override
	{
		SCompoundWidget::OnDragLeave( DragDropEvent );
		SetHoveredTableCellDelegate.ExecuteIfBound( NAME_None, nullptr );
	}

	EVisibility GetHotPathIconVisibility() const
	{
		const bool bIsHotPathIconVisible = EventPtr->_bIsHotPath;
		return bIsHotPathIconVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GetHintIconVisibility() const
	{
		return IsHovered() ? EVisibility::Visible : EVisibility::Hidden;
	}

	EVisibility GetCulledEventsIconVisibility() const
	{
		return EventPtr->HasCulledChildren() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FSlateColor GetColorAndOpacity() const
	{
		const FLinearColor TextColor = EventPtr->_bIsFiltered ? FLinearColor(1.0f,1.0f,1.0f,0.5f) : FLinearColor::White;
		return TextColor;
	}

	FLinearColor GetShadowColorAndOpacity() const
	{
		const FLinearColor ShadowColor = EventPtr->_bIsFiltered ? FLinearColor(0.f,0.f,0.f,0.25f) : FLinearColor(0.f,0.f,0.f,0.5f);
		return ShadowColor;
	}

	FReply ExpandCulledEvents_OnClicked()
	{
		OwnerEventGraph.Pin()->ExpandCulledEvents( EventPtr );
		return FReply::Handled();
	}

protected:
	FSetHoveredTableCell SetHoveredTableCellDelegate;

	/** A shared pointer to the event graph sample. */
	FEventGraphSamplePtr EventPtr;

	/** The event graph that owns this event graph cell. */
	TWeakPtr< IEventGraph > OwnerEventGraph;

	/** The ID of the column where this event graph belongs. */
	FName ColumnID;
};



/** Widget that represents a table row in the event graph widget.  Generates widgets for each column on demand. */
class SEventGraphTableRow : public SMultiColumnTableRow< FEventGraphSamplePtr >
{
public:	
	SLATE_BEGIN_ARGS( SEventGraphTableRow ){}
		SLATE_EVENT( FIsColumnVisibleDelegate, OnIsColumnVisible )
		SLATE_EVENT( FSetHoveredTableCell, OnSetHoveredTableCell )
		SLATE_EVENT( FGetColumnOutlineHAlignmentDelegate, OnGetColumnOutlineHAlignmentDelegate )
		SLATE_ATTRIBUTE( FName, HighlightedEventName )
		SLATE_ARGUMENT( FEventGraphSamplePtr, EventPtr )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs	          - Declaration used by the SNew() macro to construct this widget.
	 * @param   InOwnerTableView  - The owner table into which this row is being placed.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<IEventGraph>& InOwnerEventGraph )
	{
		IsColumnVisibleDelegate = InArgs._OnIsColumnVisible;
		SetHoveredTableCellDelegate = InArgs._OnSetHoveredTableCell;
		GetColumnOutlineHAlignmentDelegate = InArgs._OnGetColumnOutlineHAlignmentDelegate;
		HighlightedEventName = InArgs._HighlightedEventName;
		EventPtr = InArgs._EventPtr;
		OwnerEventGraph = InOwnerEventGraph;

		SMultiColumnTableRow< FEventGraphSamplePtr >::Construct( SMultiColumnTableRow< FEventGraphSamplePtr >::FArguments(), InOwnerTableView );
	}

	/**
	 * Users of SMultiColumnTableRow would usually some piece of data associated with it.
	 * The type of this data is ItemType; it's the stuff that your TableView (i.e. List or Tree) is visualizing.
	 * The ColumnID tells you which column of the TableView we need to make a widget for.
	 * Make a widget and return it.
	 *
	 * @param ColumnID    A unique ID for a column in this TableView; see SHeaderRow::FColumn for more info.
	 *
	 * @return a widget to represent the contents of a cell in this row of a TableView. 
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnID ) override
	{
		return

		SNew(SOverlay)
		.Visibility( EVisibility::SelfHitTestInvisible )

		+SOverlay::Slot()
		.Padding( 0.0f )
		[
			SNew( SImage )
			.Image( FEditorStyle::GetBrush("Profiler.LineGraphArea") )
			.ColorAndOpacity( this, &SEventGraphTableRow::GetBackgroundColorAndOpacity )
		]

		+SOverlay::Slot()
		.Padding( 0.0f )
		[
			SNew( SImage )
			.Image( this, &SEventGraphTableRow::GetOutlineBrush, ColumnID )
			.ColorAndOpacity( this, &SEventGraphTableRow::GetOutlineColorAndOpacity )
		]

		+SOverlay::Slot()
		[
			SNew( SEventGraphTableCell, SharedThis(this), OwnerEventGraph )
			.Visibility( this, &SEventGraphTableRow::IsColumnVisible, ColumnID )
			.ColumnID( ColumnID )
			.IsEventNameColumn( ColumnID == FEventGraphColumns::Get().Collection[EEventPropertyIndex::StatName].ID )
			.EventPtr( EventPtr )
			.OnSetHoveredTableCell( this, &SEventGraphTableRow::OnSetHoveredTableCell )
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/**
	 * Called when Slate detects that a widget started to be dragged.
	 * Usage:
	 * A widget can ask Slate to detect a drag.
	 * OnMouseDown() reply with FReply::Handled().DetectDrag( SharedThis(this) ).
	 * Slate will either send an OnDragDetected() event or do nothing.
	 * If the user releases a mouse button or leaves the widget before
	 * a drag is triggered (maybe user started at the very edge) then no event will be
	 * sent.
	 *
	 * @param  InMyGeometry  Widget geometry
	 * @param  InMouseEvent  MouseMove that triggered the drag
	 * 
	 */
	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override
	{
		if( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
		{
			return FReply::Handled().BeginDragDrop( FStatIDDragDropOp::NewSingle( EventPtr->_StatID, EventPtr->_StatName.GetPlainNameString() ) );
		}

		return SMultiColumnTableRow< FEventGraphSamplePtr >::OnDragDetected(MyGeometry,MouseEvent);
	}

protected:
	
	FSlateColor GetBackgroundColorAndOpacity() const
	{
		const FLinearColor ThreadColor(5.0f,0.0f,0.0f,1.0f);
		const FLinearColor DefaultColor(0.0f,0.0f,0.0f,0.0f);
		const float Alpha = EventPtr->_FramePct * 0.01f;
		const FLinearColor BackgroundColorAndOpacity = FMath::Lerp(DefaultColor,ThreadColor,Alpha);
		return BackgroundColorAndOpacity;
	}

	FSlateColor GetOutlineColorAndOpacity() const
	{
		const FLinearColor NoColor(0.0f,0.0f,0.0f,0.0f);
		const bool bShouldBeHighlighted = EventPtr->_StatName == HighlightedEventName.Get();
		const FLinearColor OutlineColorAndOpacity = bShouldBeHighlighted ? FLinearColor(FColorList::SlateBlue) : NoColor;
		return OutlineColorAndOpacity;
	}

	const FSlateBrush* GetOutlineBrush( const FName ColumnID ) const
	{
		EHorizontalAlignment Result = HAlign_Center;
		if( IsColumnVisibleDelegate.IsBound() )
		{
			Result = GetColumnOutlineHAlignmentDelegate.Execute( ColumnID );
		}

		const FSlateBrush* Brush = nullptr;
		if( Result == HAlign_Left )
		{
			Brush = FEditorStyle::GetBrush("Profiler.EventGraph.Border.L");
		}
		else if( Result == HAlign_Right )
		{
			Brush = FEditorStyle::GetBrush("Profiler.EventGraph.Border.R");
		}
		else
		{
			Brush = FEditorStyle::GetBrush("Profiler.EventGraph.Border.TB");
		}
		return Brush;
	}
	
	EVisibility IsColumnVisible( const FName ColumnID ) const
	{
		EVisibility Result = EVisibility::Collapsed;

		if( IsColumnVisibleDelegate.IsBound() )
		{
			Result = IsColumnVisibleDelegate.Execute( ColumnID ) ? EVisibility::Visible : EVisibility::Collapsed;
		}

		return Result;
	}

	void OnSetHoveredTableCell( const FName InColumnID, const FEventGraphSamplePtr InSamplePtr )
	{
		SetHoveredTableCellDelegate.ExecuteIfBound( InColumnID, InSamplePtr );
	}

protected:
	FIsColumnVisibleDelegate IsColumnVisibleDelegate;
	FSetHoveredTableCell SetHoveredTableCellDelegate;
	FGetColumnOutlineHAlignmentDelegate GetColumnOutlineHAlignmentDelegate;

	/** Name of the event that should be drawn as highlighted. */
	TAttribute<FName> HighlightedEventName;

	/** A shared pointer to the event graph sample. */
	FEventGraphSamplePtr EventPtr;

	/** The event graph that owns this event graph row. */
	TWeakPtr< IEventGraph > OwnerEventGraph;
};

/*-----------------------------------------------------------------------------
	SEventTree
-----------------------------------------------------------------------------*/

SEventGraph::SEventGraph()
	: CurrentStateIndex( 0 )
{}


SEventGraph::~SEventGraph()
{
	// Remove ourselves from the profiler manager.
	if( FProfilerManager::Get().IsValid() )
	{
		FProfilerManager::Get()->OnViewModeChanged().RemoveAll( this );
	}
}


/*-----------------------------------------------------------------------------
	Event graph construction related functions
-----------------------------------------------------------------------------*/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SEventGraph::Construct( const FArguments& InArgs )
{
	static TArray<FEventGraphSamplePtr> StaticEventArray;

	SAssignNew(ExternalScrollbar, SScrollBar)
		.AlwaysShowScrollbar(true);

	ChildSlot
	[
		SNew(SSplitter)
			.Orientation(Orient_Vertical)

		+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							.Padding(2.0f)
							[
								SNew(SVerticalBox)

								+ SVerticalBox::Slot()
								[
									SNew(SHorizontalBox)

									+ SHorizontalBox::Slot()
										.AutoWidth()
										[
											GetWidgetForEventGraphTypes()
										]

									+ SHorizontalBox::Slot()
										.AutoWidth()
										.Padding(2.0f, 0.0f, 0.0f, 0.0f)
										[
											GetWidgetForEventGraphViewModes()
										]

									+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										.Padding(2.0f, 0.0f, 0.0f, 0.0f)
										[
											GetWidgetBoxForOptions()
										]
								]

								+ SVerticalBox::Slot()
								.Padding(0.0f, 2.0f, 0.0f, 0.0f)
								[
									SNew(SHorizontalBox)

									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									[
										GetWidgetForThreadFilter()
									]
								]
							]
					]
 
				// Function details view ( @see VS2012 profiler )
				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[		
						SAssignNew(FunctionDetailsBox,SBox)
							.HeightOverride(224.0f)
							[
								SNew(SBorder)
									.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
									.Padding(2.0f)
									.Clipping(EWidgetClipping::ClipToBounds)
									[
										SNew(SHorizontalBox)

										// Calling Functions
										+ SHorizontalBox::Slot()
											.FillWidth(1.0f)
											.Padding(2.0f)
											[
												GetVerticalBoxForFunctionDetails(VerticalBox_TopCalling, LOCTEXT("FunctionDetails_CallingFunctions","Calling Functions"))
											]


										// Current Function
										+ SHorizontalBox::Slot()
											.FillWidth(1.0f)
											.Padding(2.0f)
											[
												GetVerticalBoxForCurrentFunction()
											]
										// Called Functions
										+ SHorizontalBox::Slot()
											.FillWidth(1.0f)
											.Padding(2.0f)
											[
												GetVerticalBoxForFunctionDetails( VerticalBox_TopCalled, LOCTEXT("FunctionDetails_CalledFunctions","Called Functions") )
											]
									]
							]
					]
			]

		+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SBorder)
					.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
					.Padding(0.0f)
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
									[
										SAssignNew(TreeView_Base, STreeView<FEventGraphSamplePtr>)
											.ExternalScrollbar(ExternalScrollbar)
											.SelectionMode(ESelectionMode::Multi)
											.TreeItemsSource(&StaticEventArray)
											.OnGetChildren(this, &SEventGraph::EventGraph_OnGetChildren)
											.OnGenerateRow(this, &SEventGraph::EventGraph_OnGenerateRow)
											.OnSelectionChanged(this, &SEventGraph::EventGraph_OnSelectionChanged)
											.OnContextMenuOpening(FOnContextMenuOpening::CreateSP(this, &SEventGraph::EventGraph_GetMenuContent))
											.ItemHeight(12.0f)
											.HeaderRow
											(
												SAssignNew(TreeViewHeaderRow,SHeaderRow)
													.Visibility(EVisibility::Visible)
											)
									]
							]

						+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SBox)
									.WidthOverride(FOptionalSize(16.0f))
									[
										ExternalScrollbar.ToSharedRef()
									]
							]
					]
			]
	];

	InitializeAndShowHeaderColumns();
	BindCommands();

	FProfilerManager::Get()->OnViewModeChanged().AddSP( this, &SEventGraph::ProfilerManager_OnViewModeChanged );
}

TSharedRef<SWidget> SEventGraph::GetToggleButtonForEventGraphType( const EEventGraphTypes::Type EventGraphType, const FName BrushName )
{
	TSharedRef<SHorizontalBox> ButtonContent = SNew(SHorizontalBox);

	if( BrushName != NAME_None )
	{
		ButtonContent->AddSlot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		[
			SNew(SImage)
			.Image( FEditorStyle::GetBrush( BrushName ) )
		];
	}

	ButtonContent->AddSlot()
	.HAlign( HAlign_Center )
	.VAlign( VAlign_Center )
	[
		SNew( STextBlock )
		.Text( FText::FromString(EEventGraphTypes::ToName( EventGraphType )) )
		.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Caption") )
	];

	return
	SNew( SCheckBox )
	.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
	.IsEnabled( this, &SEventGraph::EventGraphType_IsEnabled, EventGraphType )
	.HAlign( HAlign_Center )
	.Padding( 2.0f )
	.OnCheckStateChanged( this, &SEventGraph::EventGraphType_OnCheckStateChanged, EventGraphType )
	.IsChecked( this, &SEventGraph::EventGraphType_IsChecked, EventGraphType )
	.ToolTipText( FText::FromString(EEventGraphTypes::ToDescription( EventGraphType )) )
	[
		ButtonContent
	];
}

TSharedRef<SWidget> SEventGraph::GetToggleButtonForEventGraphViewMode( const EEventGraphViewModes::Type EventGraphViewMode )
{
	return
	SNew( SCheckBox )
	.Style(FEditorStyle::Get(), "ToggleButtonCheckbox")
	.IsEnabled( this, &SEventGraph::EventGraph_IsEnabled )
	.HAlign( HAlign_Center )
	.Padding( 2.0f )
	.OnCheckStateChanged( this, &SEventGraph::EventGraphViewMode_OnCheckStateChanged, EventGraphViewMode )
	.IsChecked( this, &SEventGraph::EventGraphViewMode_IsChecked, EventGraphViewMode )
	.ToolTipText( EEventGraphViewModes::ToDescription( EventGraphViewMode ) )
	.Visibility( this, &SEventGraph::EventGraphViewMode_GetVisibility, EventGraphViewMode )
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		[
			SNew(SImage)
			.Image( FEditorStyle::GetBrush( EEventGraphViewModes::ToBrushName( EventGraphViewMode ) ) )
		]

		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		[
			SNew( STextBlock )
			.Text( EEventGraphViewModes::ToName( EventGraphViewMode ) )
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Caption") )
		]
	];
}

TSharedRef<SWidget> SEventGraph::GetWidgetForEventGraphTypes()
{
	return
	SNew( SBorder )
	.BorderImage( FEditorStyle::GetBrush("Profiler.Group.16") )
	.Padding(FMargin(2.0f, 0.0))
	[
		SNew( SHorizontalBox )

		// EventGraph - Type
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		.AutoWidth()
		.Padding( 2.0f )
		[	
			SNew(STextBlock)
			.Text( LOCTEXT("Toolbar_Type","Type") )
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.CaptionBold") )
		]

		// One-frame
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		.Padding( 2.0f )
		[
			GetToggleButtonForEventGraphType( EEventGraphTypes::OneFrame, NAME_None )
		]

		// Avg
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		.Padding( 2.0f )
		[
			GetToggleButtonForEventGraphType( EEventGraphTypes::Average, TEXT("Profiler.EventGraph.AverageIcon") )
		]

		// Max
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		.Padding( 2.0f )
		[
			GetToggleButtonForEventGraphType( EEventGraphTypes::Maximum, TEXT("Profiler.EventGraph.MaximumIcon") )
		]
	];
}


TSharedRef<SWidget> SEventGraph::GetWidgetForEventGraphViewModes()
{
	return
	SNew( SBorder )
	.BorderImage( FEditorStyle::GetBrush("Profiler.Group.16") )
	.Padding(FMargin(2.0f, 0.0))
	[
		SNew( SHorizontalBox )

		// View mode - Type
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		.AutoWidth()
		.Padding( 2.0f )
		[	
			SNew(STextBlock)
			.Text( LOCTEXT("Toolbar_ViewMode","View mode") )
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.CaptionBold") )
		]

		// View mode - Hierarchical
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		.Padding( 2.0f )
		[
			GetToggleButtonForEventGraphViewMode( EEventGraphViewModes::Hierarchical )
		]

		// View mode - Flat (Inclusive)
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		.Padding( 2.0f )
		[
			GetToggleButtonForEventGraphViewMode( EEventGraphViewModes::FlatInclusive )
		]

		// View mode - Flat Coalesced (Inclusive)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.Padding( 2.0f )
		[
			GetToggleButtonForEventGraphViewMode( EEventGraphViewModes::FlatInclusiveCoalesced )
		]

		// View mode - Flat (Exclusive)
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		.Padding( 1.0f )
		[
			GetToggleButtonForEventGraphViewMode( EEventGraphViewModes::FlatExclusive )
		]

		// View mode - Flat Coalesced (Exclusive)
		+SHorizontalBox::Slot()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.AutoWidth()
		.Padding( 2.0f )
		[
			GetToggleButtonForEventGraphViewMode( EEventGraphViewModes::FlatExclusiveCoalesced )
		]
	];
}

TSharedRef<SWidget> SEventGraph::GetWidgetForThreadFilter()
{
	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Profiler.Group.16"))
		.Padding(FMargin(2.0f, 0.0))
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.AutoWidth()
			.Padding( 2.0f )
			[
				SNew ( STextBlock )
				.Text( LOCTEXT( "Toolbar_Thread", "Thread" ) )
				.TextStyle( FEditorStyle::Get(), TEXT( "Profiler.CaptionBold" ) )
			]

			+SHorizontalBox::Slot()
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Fill )
			.AutoWidth()
			.Padding( 2.0f )
			[
				SAssignNew( ThreadFilterComboBox, SComboBox<TSharedPtr<FName>> )
				.ContentPadding( FMargin( 6.0f, 2.0f ) )
				.OptionsSource( &ThreadNamesForCombo )
				.OnSelectionChanged( this, &SEventGraph::OnThreadFilterChanged )
				.OnGenerateWidget( this, &SEventGraph::OnGenerateWidgetForThreadFilter )
				[
					SNew( STextBlock )
					.Text( this, &SEventGraph::GenerateTextForThreadFilter, FName( TEXT( "SelectedThreadName" ) ) )
				]
			]
	];
}

void SEventGraph::FillThreadFilterOptions()
{
	ThreadNamesForCombo.Empty();

	// Allow None as an option
	ThreadNamesForCombo.Add(MakeShareable(new FName()));

	if ( EventGraphStatesHistory.Num() == 0 )
	{
		return;
	}

	FEventGraphSamplePtr Root = GetCurrentState()->GetRoot();
	if ( !Root.IsValid() )
	{
		return;
	}

	// Add a thread filter entry for each root child
	for ( const FEventGraphSamplePtr Child : Root->GetChildren() )
	{
		ThreadNamesForCombo.Add( MakeShareable( new FName( Child->_ThreadName ) ) );
	}

	// Sort the thread names alphabetically
	ThreadNamesForCombo.Sort([]( const TSharedPtr<FName> Lhs, const TSharedPtr<FName> Rhs ) 
	{
		return Lhs->IsNone() || ( !Rhs->IsNone() && *Lhs < *Rhs );
	});

	// Refresh the combo box
	if ( ThreadFilterComboBox.IsValid() )
	{
		ThreadFilterComboBox->RefreshOptions();
	}
}

FText SEventGraph::GenerateTextForThreadFilter( FName ThreadName ) const
{
	static const FName SelectedThreadName( TEXT( "SelectedThreadName" ) );
	if ( ThreadName == SelectedThreadName )
	{
		if ( EventGraphStatesHistory.Num() > 0 )
		{
			ThreadName = GetCurrentState()->ThreadFilter;
		}
		else
		{
			ThreadName = NAME_None;
		}
	}
	return FText::FromName( ThreadName );
}

void SEventGraph::OnThreadFilterChanged( TSharedPtr<FName> NewThread, ESelectInfo::Type SelectionType )
{
	if ( NewThread.IsValid() )
	{
		GetCurrentState()->ThreadFilter = *NewThread;
		RestoreEventGraphStateFrom( GetCurrentState() );
		GetCurrentState()->GetRoot()->SetBooleanStateForAllChildren<EEventPropertyIndex::bNeedNotCulledChildrenUpdate>(true);
	}
}

TSharedRef<SWidget> SEventGraph::OnGenerateWidgetForThreadFilter( TSharedPtr<FName> ThreadName ) const
{
	return SNew( STextBlock )
		.Text( GenerateTextForThreadFilter( ThreadName.IsValid() ? *ThreadName : NAME_None ) );
}

TSharedRef<SWidget> SEventGraph::GetWidgetBoxForOptions()
{
	return
	SNew( SBorder )
	.BorderImage( FEditorStyle::GetBrush("Profiler.Group.16") )
	.Padding( 0.0f )
	[
		SNew(SHorizontalBox)

		// History Back
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 1.0f )
		[
			SNew( SButton )
			.OnClicked( this,  &SEventGraph::HistoryBack_OnClicked )
			.IsEnabled( this, &SEventGraph::HistoryBack_IsEnabled )
			.ToolTipText( this, &SEventGraph::HistoryBack_GetToolTipText )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.ContentPadding( 2.0f )
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Profiler.EventGraph.HistoryBack") )
			]
		]

		// History Forward
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 1.0f )
		[
			SNew( SButton )
			.OnClicked( this,  &SEventGraph::HistoryForward_OnClicked )
			.IsEnabled( this, &SEventGraph::HistoryForward_IsEnabled )
			.ToolTipText( this, &SEventGraph::HistoryForward_GetToolTipText )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.ContentPadding( 2.0f )
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Profiler.EventGraph.HistoryForward") )
			]
		]

		// History List
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 1.0f )
		[
			SNew( SComboButton )
			.IsEnabled( this, &SEventGraph::HistoryList_IsEnabled )
			.ContentPadding( 0.0f )
			.OnGetMenuContent( this, &SEventGraph::HistoryList_GetMenuContent )
		]

		// Expand hot path
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 1.0f )
		[
			SNew( SButton )
			.IsEnabled( this, &SEventGraph::ContextMenu_ExpandHotPath_CanExecute )
			.ToolTipText( LOCTEXT("ContextMenu_Header_Expand_ExpandHotPath_Desc", 
									"Expands hot path for the selected events, based on the inclusive time, also enables descending sorting by inclusive time") )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.OnClicked( this, &SEventGraph::ExpandHotPath_OnClicked )
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush(TEXT("Profiler.EventGraph.ExpandHotPath16")) )
			]
		]

		// Highlight hot path
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.Padding( 1.0f )
		[
			SNew( SCheckBox )
			.Visibility( EVisibility::Collapsed )
			.IsEnabled( false )
			.OnCheckStateChanged( this, &SEventGraph::HighlightHotPath_OnCheckStateChanged )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("HighlightHotPathCheckboxLabel", "HP") )
			]
		]

		// Configuration 
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.Padding( 1.0f )
		[
			SNew( SButton )
			.Visibility( EVisibility::Collapsed )
			.IsEnabled( false )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Text( LOCTEXT("ConfigurationButtonLabel", "CF") )
		]

		// Export
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.Padding( 1.0f )
		[
			SNew( SButton )
			.Visibility( EVisibility::Collapsed )
			.IsEnabled( false )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Text( LOCTEXT("ExportButtonLabel", "EX") )
		]

		// Search box
		+SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		.HAlign( HAlign_Fill )
		.VAlign( VAlign_Center )
		.Padding( 1.0f )
		[
			SAssignNew( FilteringSearchBox, SSearchBox )
			.HintText( LOCTEXT("FilteringSearchBox_HintText", "Search or filter event(s)") )
			.OnTextChanged( this, &SEventGraph::FilteringSearchBox_OnTextChanged )
			.OnTextCommitted( this, &SEventGraph::FilteringSearchBox_OnTextCommitted )
			.IsEnabled( this, &SEventGraph::FilteringSearchBox_IsEnabled )
			.ToolTipText( LOCTEXT("FilteringSearchBox_TT", "Type here to search or filter events") )
		]

		// Aggressive filtering
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			.Padding(1.0f)
			[
				SNew(SCheckBox)
				.Visibility(EVisibility::Visible)
				.IsEnabled(true)
				.OnCheckStateChanged(this, &SEventGraph::OnAggressiveFilteringToggled)
				[
					SNew(STextBlock)
					.Text( LOCTEXT("AggressiveFilteringLabel", "AF") )
					.ToolTipText(LOCTEXT("AggressiveFiltering_TT", "Toggle aggressive filtering"))
				]
			]


		// Filtering help
		+SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Fill )
		.Padding( 1.0f )
		[
			SNew( SButton )
			.Visibility( EVisibility::Collapsed )
			.IsEnabled( false )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Text( LOCTEXT("FilteringHelpButtonLabel", "?") )
		]
	];
}

TSharedRef<SVerticalBox> SEventGraph::GetVerticalBoxForFunctionDetails( TSharedPtr<SVerticalBox>& out_VerticalBoxTopFuncions, const FText& Caption )
{
	return 
	SNew(SVerticalBox)
	
	+SVerticalBox::Slot()
	.AutoHeight()
	.HAlign( HAlign_Center )
	.VAlign( VAlign_Center )
	.Padding( 2.0f )
	[
		SNew(STextBlock)
		.Text( Caption )
		.TextStyle( FEditorStyle::Get(), TEXT("Profiler.CaptionBold") )
	]

	+SVerticalBox::Slot()
	.AutoHeight()
	.HAlign( HAlign_Fill )
	.VAlign( VAlign_Center )
	.Padding( 2.0f )
	[
		SNew(SSeparator)
		.Orientation( Orient_Horizontal )
	]

	+SVerticalBox::Slot()
	.FillHeight( 1.0f )
	.HAlign( HAlign_Fill )
	.VAlign( VAlign_Fill )
	.Padding( 0.0f )
	[
		SAssignNew(out_VerticalBoxTopFuncions,SVerticalBox)
	];
}

TSharedRef<SVerticalBox> SEventGraph::GetVerticalBoxForCurrentFunction()
{
	return 
	SNew(SVerticalBox)

	+SVerticalBox::Slot()
	.AutoHeight()
	.HAlign( HAlign_Center )
	.VAlign( VAlign_Center )
	.Padding( 2.0f )
	[
		SNew(STextBlock)
		.Text( LOCTEXT("FunctionDetails_CurrentFunction","Current Function") )
		.TextStyle( FEditorStyle::Get(), TEXT("Profiler.CaptionBold") )
	]

	+SVerticalBox::Slot()
	.AutoHeight()
	.HAlign( HAlign_Fill )
	.VAlign( VAlign_Center )
	.Padding( 2.0f )
	[
		SNew(SSeparator)
		.Orientation( Orient_Horizontal )
	]

	+SVerticalBox::Slot()
	.AutoHeight()
	.HAlign( HAlign_Center )
	.VAlign( VAlign_Center )
	.Padding( 2.0f )
	.Expose( CurrentFunctionDescSlot );
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

/*-----------------------------------------------------------------------------
	...
-----------------------------------------------------------------------------*/

TSharedRef< ITableRow > SEventGraph::EventGraph_OnGenerateRow( FEventGraphSamplePtr EventPtr, const TSharedRef<STableViewBase>& OwnerTable )
{	
	TSharedRef< ITableRow > ReturnRow = SNew(SEventGraphTableRow, OwnerTable, SharedThis(this))	
		.OnIsColumnVisible( this, &SEventGraph::EventGraphTableRow_IsColumnVisible )
		.OnSetHoveredTableCell( this, &SEventGraph::EventGraphTableRow_SetHoveredTableCell )
		.OnGetColumnOutlineHAlignmentDelegate( this, &SEventGraph::EventGraphRow_GetColumnOutlineHAlignment )
		.HighlightedEventName( this, &SEventGraph::EventGraphRow_GetHighlightedEventName )
		.EventPtr( EventPtr )
		;

	return ReturnRow;
}

void SEventGraph::EventGraph_OnSelectionChanged( FEventGraphSamplePtr SelectedItem, ESelectInfo::Type SelectInfo )
{
	if( SelectInfo != ESelectInfo::Direct )
	{
		UpdateFunctionDetails();
	}
}

bool SEventGraph::EventGraphTableRow_IsColumnVisible( const FName ColumnID ) const
{
	bool bResult = false;
	const FEventGraphColumn& ColumnPtr = TreeViewHeaderColumns.FindChecked(ColumnID);
	return ColumnPtr.bIsVisible;
}

void SEventGraph::EventGraphTableRow_SetHoveredTableCell( const FName ColumnID, const FEventGraphSamplePtr EventPtr )
{
	HoveredColumnID = ColumnID;

	const bool bIsAnyMenusVisible = FSlateApplication::Get().AnyMenusVisible();
	if( !HasMouseCapture() && !bIsAnyMenusVisible )
	{
		HoveredSamplePtr = EventPtr;
	}

#if	DEBUG_PROFILER_PERFORMANCE
	UE_LOG( Profiler, Log, TEXT("%s -> %s"), *HoveredColumnID.GetPlainNameString(), EventPtr.IsValid() ? *EventPtr->_StatName.GetPlainNameString() : TEXT("nullptr") );
#endif // DEBUG_PROFILER_PERFORMANCE
}


FName SEventGraph::EventGraphRow_GetHighlightedEventName() const
{
	return HighlightedEventName;
}


EHorizontalAlignment SEventGraph::EventGraphRow_GetColumnOutlineHAlignment( const FName ColumnID ) const
{
	const TIndirectArray<SHeaderRow::FColumn>& Columns = TreeViewHeaderRow->GetColumns();
	const int32 LastColumnIdx = Columns.Num()-1;
	 
	// First column
	if( Columns[0].ColumnId == ColumnID )
	{
		return HAlign_Left;
	}
	// Last column
	else if( Columns[LastColumnIdx].ColumnId == ColumnID )
	{
		return HAlign_Right;
	}
	// Middle columns
	{
		return HAlign_Center;
	}
}

void SEventGraph::EventGraph_OnGetChildren( FEventGraphSamplePtr InParent, TArray< FEventGraphSamplePtr >& out_Children )
{
	if( GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical )
	{
		out_Children = InParent->GetNotCulledChildren();
	}
}

void SEventGraph::TreeView_SetItemsExpansion_Recurrent( TArray<FEventGraphSamplePtr>& InEventPtrs, const bool bShouldExpand )
{
	for( int32 EventIndex = 0; EventIndex < InEventPtrs.Num(); EventIndex++ )
	{
		const FEventGraphSamplePtr EventPtr = InEventPtrs[EventIndex];
		TreeView_Base->SetItemExpansion( EventPtr, bShouldExpand );
		TreeView_SetItemsExpansion_Recurrent( EventPtr->GetChildren(), bShouldExpand );
	}
}

void SEventGraph::SetSortModeForColumn( const FName& ColumnID, const EColumnSortMode::Type SortMode )
{
	ColumnBeingSorted = ColumnID;
	ColumnSortMode = SortMode;

	SortEvents();
}

/*-----------------------------------------------------------------------------
	ShowSelectedEventsInViewMode
-----------------------------------------------------------------------------*/

void SEventGraph::ShowSelectedEventsInViewMode_Execute( EEventGraphViewModes::Type NewViewMode )
{
	const TArray<FEventGraphSamplePtr> SelectedEvents = TreeView_Base->GetSelectedItems();
	ShowEventsInViewMode( SelectedEvents, NewViewMode );
}

bool SEventGraph::ShowSelectedEventsInViewMode_CanExecute( EEventGraphViewModes::Type NewViewMode ) const
{
	return TreeView_Base->GetNumItemsSelected() > 0;
}

ECheckBoxState SEventGraph::ShowSelectedEventsInViewMode_GetCheckState( EEventGraphViewModes::Type NewViewMode ) const
{
	return GetCurrentStateViewMode() == NewViewMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

static EEventCompareOps::Type EColumnSortModeToEventCompareOp( const EColumnSortMode::Type ColumnSortMode )
{
	if( ColumnSortMode == EColumnSortMode::Descending )
	{
		return EEventCompareOps::Greater;
	}
	else if( ColumnSortMode == EColumnSortMode::Ascending )
	{
		return EEventCompareOps::Less;
	}
	check( 0 );
	return EEventCompareOps::InvalidOrMax;
}

void SEventGraph::SortEvents()
{
	PROFILER_SCOPE_LOG_TIME( TEXT( "SEventGraph::SortEvents" ), nullptr );

	if( ColumnBeingSorted != NAME_None )
	{
		const FEventGraphColumn& Column = *FEventGraphColumns::Get().ColumnNameToIndexMapping.FindChecked( ColumnBeingSorted );

		if(	GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical || 
			GetCurrentStateViewMode() == EEventGraphViewModes::FlatInclusive || 
			GetCurrentStateViewMode() == EEventGraphViewModes::FlatInclusiveCoalesced ||
			GetCurrentStateViewMode() == EEventGraphViewModes::FlatExclusive || 
			GetCurrentStateViewMode() == EEventGraphViewModes::FlatExclusiveCoalesced )
		{
				FEventArraySorter::Sort( GetCurrentState()->GetRealRoot()->GetChildren(), Column.ID, EColumnSortModeToEventCompareOp( ColumnSortMode ) );
				FEventArraySorter::Sort( Events_FlatCoalesced, Column.ID, EColumnSortModeToEventCompareOp( ColumnSortMode ) ); 
				FEventArraySorter::Sort( Events_Flat, Column.ID, EColumnSortModeToEventCompareOp( ColumnSortMode ) ); 
			
			// Update not culled children.
			GetCurrentState()->GetRoot()->SetBooleanStateForAllChildren<EEventPropertyIndex::bNeedNotCulledChildrenUpdate>(true);
		}
	}	
}

void SEventGraph::FilteringSearchBox_OnTextChanged( const FText& InFilterText )
{
}

static bool RecursiveShowUnfilteredItems(TSharedPtr< STreeView<FEventGraphSamplePtr> >& TreeView, TArray<FEventGraphSamplePtr>& Nodes)
{
	bool bExpandedAnyChildren = false;

	for (FEventGraphSamplePtr& Node : Nodes)
	{
		const bool bChildIsExpanded = RecursiveShowUnfilteredItems(TreeView, Node->GetChildren());
		const bool bThisWantsExpanded = !Node->PropertyValueAsBool(EEventPropertyIndex::bIsFiltered);
		const bool bExpandThis = bChildIsExpanded || bThisWantsExpanded;
		bExpandedAnyChildren |= bExpandThis;

		TreeView->SetItemExpansion(Node, bExpandThis);
		
	}

	return bExpandedAnyChildren;
}

void SEventGraph::FilteringSearchBox_OnTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	PROFILER_SCOPE_LOG_TIME(TEXT("SEventGraph::FilterOutByText_Execute"), nullptr);

	SaveCurrentEventGraphState();
	FEventGraphState* Op = GetCurrentState()->CreateCopyWithTextFiltering(NewText.ToString());
	CurrentStateIndex = EventGraphStatesHistory.Insert(MakeShareable(Op), CurrentStateIndex + 1);
	RestoreEventGraphStateFrom(GetCurrentState());

	// Auto-expand to view the unfiltered items
	if (GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical)
	{
		RecursiveShowUnfilteredItems(TreeView_Base, GetCurrentState()->GetRoot()->GetChildren());
		TreeView_Refresh();
	}
}

bool SEventGraph::FilteringSearchBox_IsEnabled() const
{
	return true;
}


void SEventGraph::OnAggressiveFilteringToggled(ECheckBoxState InState)
{
	GetCurrentState()->SetAggressiveFiltering( InState == ECheckBoxState::Checked ? true : false );

	RestoreEventGraphStateFrom(GetCurrentState());

	// Auto-expand to view the unfiltered items
	if (GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical)
	{
		RecursiveShowUnfilteredItems(TreeView_Base, GetCurrentState()->GetRoot()->GetChildren());
		TreeView_Refresh();
	}
}


TSharedPtr<SWidget> SEventGraph::EventGraph_GetMenuContent() const
{
	const FEventGraphColumn& Column = *FEventGraphColumns::Get().ColumnNameToIndexMapping.FindChecked( HoveredColumnID );
	const TArray<FEventGraphSamplePtr> SelectedEvents = TreeView_Base->GetSelectedItems();
	const int NumSelectedEvents = SelectedEvents.Num();
	FEventGraphSamplePtr SelectedEvent = NumSelectedEvents ? SelectedEvents[0] : nullptr;
	
	FText SelectionStr;
	FText PropertyName;
	FText PropertyValue;
	
	if( NumSelectedEvents == 0 )
	{
		SelectionStr = LOCTEXT( "NothingSelected", "Nothing selected" );
	}
	else if( NumSelectedEvents == 1 )
	{
		SelectionStr = FText::FromString( SelectedEvent->_StatName.GetPlainNameString() );
		PropertyName = Column.ShortName;
		PropertyValue = FText::FromString( SelectedEvent->GetFormattedValue(Column.Index) );
	}
	else
	{
		SelectionStr = LOCTEXT( "MultipleSelection", "Multiple selection" );
	}
	
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL );
	
	// Selection menu
	MenuBuilder.BeginSection("Selection", LOCTEXT("ContextMenu_Header_Selection", "Selection") );
	{
		struct FLocal
		{
			static bool ReturnFalse()
			{
				return false;
			}
		};

		FUIAction DummyUIAction;
		DummyUIAction.CanExecuteAction = FCanExecuteAction::CreateStatic( &FLocal::ReturnFalse );
		MenuBuilder.AddMenuEntry
		( 
			SelectionStr, 
			LOCTEXT("ContextMenu_Selection", "Currently selected events"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "@missing.icon"), DummyUIAction, NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	// Root menu
	MenuBuilder.BeginSection("Root/Culling/Filtering", LOCTEXT("ContextMenu_Header_Root", "Root") );
	{
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Root_Set", "Set Root"), 
			LOCTEXT("ContextMenu_Root_Set_Desc", "Sets the root to the selected event(s) and switches to hierarchical view"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.SetRoot"), SetRoot_Custom(), NAME_None, EUserInterfaceActionType::Button 
		);

		FUIAction Action_AggregateForSelection;

// 		MenuBuilder.AddMenuEntry
// 		( 
// 			LOCTEXT("ContextMenu_Root_Reset", "Reset To Default"), 
// 			LOCTEXT("ContextMenu_Root_Reset_Desc", "Resets the root options and removes from the history"), 
// 			TEXT("Profiler.Misc.ResetToDefault"), ResetRoot_Custom(), NAME_None, EUserInterfaceActionType::Button 
// 		);
	}
	//MenuBuilder.EndSection();

	// Culling menu
	//MenuBuilder.BeginSection("Culling", LOCTEXT("ContextMenu_Culling","Culling") );
	{
		FText CullingDesc;
		
		if( !Column.bCanBeCulled )
		{
			CullingDesc = LOCTEXT("ContextMenu_Culling_DescErrCol","Culling not available, please select a different column");
		}
		else if( NumSelectedEvents == 1 )
		{
			CullingDesc = FText::Format( LOCTEXT("ContextMenu_Culling_DescFmt","Cull events to '{0}' based on '{1}'"), PropertyValue, PropertyName );	
		}
		else
		{
			CullingDesc = LOCTEXT("ContextMenu_Culling_DescErrEve","Culling not available, please select one event");
		}

		MenuBuilder.AddMenuEntry
		( 
			CullingDesc,
			LOCTEXT("ContextMenu_Culling_Desc_TT","Culls the event graph based on the property value of the selected event"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.CullEvents"), CullByProperty_Custom(SelectedEvent,HoveredColumnID,false), NAME_None, EUserInterfaceActionType::Button 
		);

	// Filtering menu
		FText FilteringDesc;

		if( !Column.bCanBeFiltered )
		{
			FilteringDesc = LOCTEXT("ContextMenu_Filtering_DescErrCol","Filtering not available, please select a different column");
		}
		else if( NumSelectedEvents == 1 )
		{
			FilteringDesc = FText::Format( LOCTEXT("ContextMenu_Filtering_DescFmt","Filter events to '{0}' based on '{1}'"), PropertyValue, PropertyName );	
		}
		else
		{
			FilteringDesc = LOCTEXT("ContextMenu_Filtering_DescErrEve","Filtering not available, please select one event");
		}

		MenuBuilder.AddMenuEntry
		( 
			FilteringDesc,
			LOCTEXT("ContextMenu_Filtering_Desc_TT","Filters the event graph based on the property value of the selected event"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.FilterEvents"), FilterOutByProperty_Custom(SelectedEvent,HoveredColumnID,false), NAME_None, EUserInterfaceActionType::Button 
		);

		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_ClearHistory", "Reset to default"), 
			LOCTEXT("ContextMenu_ClearHistory_Reset_Desc", "For the selected event graph resets root/culling/filter to the default state and clears the history"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.ResetToDefault"), ClearHistory_Custom(), NAME_None, EUserInterfaceActionType::Button 
		);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection("Expand", LOCTEXT("ContextMenu_Header_Expand", "Expand") );
	{
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Expand_ExpandAll", "Expand All"), 
			LOCTEXT("ContextMenu_Header_Expand_ExpandAll_Desc", "Expands all events"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ExpandAll"), SetExpansionForEvents_Custom( ESelectedEventTypes::AllEvents, true ), NAME_None, EUserInterfaceActionType::Button 
		);

		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Expand_CollapseAll", "Collapse All"), 
			LOCTEXT("ContextMenu_Header_Expand_CollapseAll_Desc", "Collapses all events"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.CollapseAll"), SetExpansionForEvents_Custom( ESelectedEventTypes::AllEvents, false ), NAME_None, EUserInterfaceActionType::Button 
		);

		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Expand_ExpandSelection", "Expand Selection"), 
			LOCTEXT("ContextMenu_Header_Expand_ExpandSelection_Desc", "Expands selected events"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ExpandSelection"), SetExpansionForEvents_Custom( ESelectedEventTypes::SelectedEvents, true ), NAME_None, EUserInterfaceActionType::Button 
		);

		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Expand_CollapseSelection", "Collapse Selection"), 
			LOCTEXT("ContextMenu_Header_Expand_CollapseSelection_Desc", "Collapses selected events"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.CollapseSelection"), SetExpansionForEvents_Custom( ESelectedEventTypes::SelectedEvents, false ), NAME_None, EUserInterfaceActionType::Button 
		);

		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Expand_ExpandThread", "Expand Thread"), 
			LOCTEXT("ContextMenu_Header_Expand_ExpandThread_Desc", "Expands selected threads"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ExpandThread"), SetExpansionForEvents_Custom( ESelectedEventTypes::SelectedThreadEvents, true ), NAME_None, EUserInterfaceActionType::Button 
		);

		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Expand_CollapseThread", "Collapse Thread"), 
			LOCTEXT("ContextMenu_Header_Expand_CollapseThread_Desc", "Collapses selected threads"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.CollapseThread"), SetExpansionForEvents_Custom( ESelectedEventTypes::SelectedThreadEvents, false ), NAME_None, EUserInterfaceActionType::Button 
		);

		//-----------------------------------------------------------------------------

		FUIAction Action_ExpandHotPath
		(
			FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_ExpandHotPath_Execute ),
			FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_ExpandHotPath_CanExecute )
		);
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Expand_ExpandHotPath", "Expand Hot Path"), 
			LOCTEXT("ContextMenu_Header_Expand_ExpandHotPath_Desc", "Expands hot path for the selected events, based on the inclusive time, also enables descending sorting by inclusive time"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ExpandHotPath"), Action_ExpandHotPath, NAME_None, EUserInterfaceActionType::Button 
		);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection("Navigation", LOCTEXT("ContextMenu_Header_Navigation", "Navigation") );
	{
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Navigation_ShowInHierarchicalView", "Show In Hierarchical View"), 
			LOCTEXT("ContextMenu_Header_Navigation_ShowInHierarchicalView_Desc", "Switches to hierarchical view and expands selected events"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), EEventGraphViewModes::ToBrushName(EEventGraphViewModes::Hierarchical)), ShowSelectedEventsInViewMode_Custom(EEventGraphViewModes::Hierarchical), NAME_None, EUserInterfaceActionType::Check
		);

		//-----------------------------------------------------------------------------

		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatView", "Show In FlatInclusive View"), 
			LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatView_Desc", "Switches to flat view, also enables descending sorting by inclusive time"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), EEventGraphViewModes::ToBrushName(EEventGraphViewModes::FlatExclusive)), ShowSelectedEventsInViewMode_Custom(EEventGraphViewModes::FlatExclusive), NAME_None, EUserInterfaceActionType::Check 
		);

		if( FProfilerManager::GetSettings().bShowCoalescedViewModesInEventGraph )
		{
			MenuBuilder.AddMenuEntry
			( 
				LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatCoalesced", "Show In FlatInclusive Coalesced"), 
				LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatCoalesced_Desc", "Switches to flat coalesced, also enables descending sorting by inclusive time"), 
				FSlateIcon(FEditorStyle::GetStyleSetName(), EEventGraphViewModes::ToBrushName(EEventGraphViewModes::FlatInclusiveCoalesced)), ShowSelectedEventsInViewMode_Custom(EEventGraphViewModes::FlatInclusiveCoalesced), NAME_None, EUserInterfaceActionType::Check 
			);
		}

		//-----------------------------------------------------------------------------

		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatExclusiveView", "Show In Flat Exclusive View"), 
			LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatExclusiveView_Desc", "Switches to flat exclusive view, also enables ascending sorting by exclusive time"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), EEventGraphViewModes::ToBrushName(EEventGraphViewModes::FlatExclusive)), ShowSelectedEventsInViewMode_Custom(EEventGraphViewModes::FlatExclusive), NAME_None, EUserInterfaceActionType::Check 
		);

		if( FProfilerManager::GetSettings().bShowCoalescedViewModesInEventGraph )
		{
			MenuBuilder.AddMenuEntry
			( 
				LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatExclusiveCoalescedView", "Show In Flat Exclusive Coalesced View"), 
				LOCTEXT("ContextMenu_Header_Navigation_ShowInFlatExclusiveCoalescedView_Desc", "Switches to flat exclusive coalesced view, also enables ascending sorting by exclusive time enabled"), 
				FSlateIcon(FEditorStyle::GetStyleSetName(), EEventGraphViewModes::ToBrushName(EEventGraphViewModes::FlatExclusiveCoalesced)), ShowSelectedEventsInViewMode_Custom(EEventGraphViewModes::FlatExclusiveCoalesced), NAME_None, EUserInterfaceActionType::Check 
			);
		}

		//-----------------------------------------------------------------------------
		FUIAction Action_ShowInClassAggregate;
		FUIAction Action_ShowInGraphPanel;
		
		// Highlight all occurrences based on object's name/class
		FUIAction Action_HighlightBasedOnClass;

	}
	MenuBuilder.EndSection();

	/*
		Event graph coloring based on inclusive time as a gradient from black to red? 
	*/
	
	MenuBuilder.BeginSection("Misc", LOCTEXT("ContextMenu_Header_Misc", "Miscellaneous") );
	{
		FUIAction Action_CopySelectedToClipboard
		(
			FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_CopySelectedToClipboard_Execute ),
			FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_CopySelectedToClipboard_CanExecute )
		);
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Misc_CopySelectedToClipboard", "Copy To Clipboard"), 
			LOCTEXT("ContextMenu_Header_Misc_CopySelectedToClipboard_Desc", "Copies selection to clipboard"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.CopyToClipboard"), Action_CopySelectedToClipboard, NAME_None, EUserInterfaceActionType::Button 
		);

		FUIAction Action_SaveSelectedToFile;

		FUIAction Action_SelectStack
		(
			FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SelectStack_Execute ),
			FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SelectStack_CanExecute )
		);
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Misc_SelectStack", "Select Stack"), 
			LOCTEXT("ContextMenu_Header_Misc_SelectStack_Desc", "Selects all events in the stack"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.SelectStack"), Action_SelectStack, NAME_None, EUserInterfaceActionType::Button 
		);

		MenuBuilder.AddSubMenu
		( 
			LOCTEXT("ContextMenu_Header_Misc_Sort", "Sort By"), 
			LOCTEXT("ContextMenu_Header_Misc_Sort_Desc", "Sort by column"), 
			FNewMenuDelegate::CreateSP( this, &SEventGraph::EventGraph_BuildSortByMenu ),
			false, 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortBy")
		);
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection("Columns", LOCTEXT("ContextMenu_Header_Columns", "Columns") );
	{
		MenuBuilder.AddSubMenu
		( 
			LOCTEXT("ContextMenu_Header_Columns_View", "View Column"), 
			LOCTEXT("ContextMenu_Header_Columns_View_Desc", "Hides or shows columns"), 
			FNewMenuDelegate::CreateSP( this, &SEventGraph::EventGraph_BuildViewColumnMenu ),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ViewColumn")
		);

		FUIAction Action_ResetColumns
		(
			FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_ResetColumns_Execute ),
			FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_ResetColumns_CanExecute )
		);
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Columns_ResetColumns", "Reset Columns To Default"), 
			LOCTEXT("ContextMenu_Header_Columns_ResetColumns_Desc", "Resets columns to default"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_ResetColumns, NAME_None, EUserInterfaceActionType::Button 
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SEventGraph::EventGraph_BuildSortByMenu( FMenuBuilder& MenuBuilder )
{
	// TODO: Refactor later @see TSharedPtr<SWidget> SCascadePreviewViewportToolBar::GenerateViewMenu() const
	MenuBuilder.BeginSection("ColumnName", LOCTEXT("ContextMenu_Header_Misc_ColumnName","Column Name"));
	for( auto It = TreeViewHeaderColumns.CreateConstIterator(); It; ++It )
	{
		const FEventGraphColumn& Column = It.Value();

		if( Column.bIsVisible && Column.bCanBeSorted )
		{
			FUIAction Action_SortByColumn
			(
				FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SortByColumn_Execute, Column.ID ),
				FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SortByColumn_CanExecute, Column.ID ),
				FIsActionChecked::CreateSP( this, &SEventGraph::ContextMenu_SortByColumn_IsChecked, Column.ID )
			);
			MenuBuilder.AddMenuEntry
			( 
				Column.ShortName,
				Column.Description, 
				FSlateIcon(), Action_SortByColumn, NAME_None, EUserInterfaceActionType::RadioButton 
			);
		}
	}
	MenuBuilder.EndSection();

	//-----------------------------------------------------------------------------

	MenuBuilder.BeginSection("SortMode", LOCTEXT("ContextMenu_Header_Misc_Sort_SortMode", "Sort Mode") );
	{
		FUIAction Action_SortAscending
		(
			FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SortMode_Execute, EColumnSortMode::Ascending ),
			FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SortMode_CanExecute, EColumnSortMode::Ascending ),
			FIsActionChecked::CreateSP( this, &SEventGraph::ContextMenu_SortMode_IsChecked, EColumnSortMode::Ascending )
		);
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending", "Sort Ascending"), 
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending_Desc", "Sorts ascending"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortAscending"), Action_SortAscending, NAME_None, EUserInterfaceActionType::RadioButton
		);

		FUIAction Action_SortDescending
		(
			FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SortMode_Execute, EColumnSortMode::Descending ),
			FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_SortMode_CanExecute, EColumnSortMode::Descending ),
			FIsActionChecked::CreateSP( this, &SEventGraph::ContextMenu_SortMode_IsChecked, EColumnSortMode::Descending )
		);
		MenuBuilder.AddMenuEntry
		( 
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending", "Sort Descending"), 
			LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending_Desc", "Sorts descending"), 
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortDescending"), Action_SortDescending, NAME_None, EUserInterfaceActionType::RadioButton 
		);
	}
	MenuBuilder.EndSection();
}

void SEventGraph::EventGraph_BuildViewColumnMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection("ViewColumn", LOCTEXT("ContextMenu_Header_Columns_View", "View Column") );

	for( auto It = TreeViewHeaderColumns.CreateConstIterator(); It; ++It )
	{
		const FEventGraphColumn& Column = It.Value();

		FUIAction Action_ToggleColumn
		(
			FExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_ToggleColumn_Execute, Column.ID ),
			FCanExecuteAction::CreateSP( this, &SEventGraph::ContextMenu_ToggleColumn_CanExecute, Column.ID ),
			FIsActionChecked::CreateSP( this, &SEventGraph::ContextMenu_ToggleColumn_IsChecked, Column.ID )
		);
		MenuBuilder.AddMenuEntry
		( 
			Column.ShortName,
			Column.Description, 
			FSlateIcon(), Action_ToggleColumn, NAME_None, EUserInterfaceActionType::ToggleButton 
		);
	}

	MenuBuilder.EndSection();
}

void SEventGraph::EventGraphViewMode_OnCheckStateChanged( ECheckBoxState NewRadioState, const EEventGraphViewModes::Type InViewMode )
{
	if( NewRadioState == ECheckBoxState::Checked && GetCurrentStateViewMode() != InViewMode)
	{
		const TArray< FEventGraphSamplePtr > SelectedEvents = TreeView_Base->GetSelectedItems();
		ShowEventsInViewMode( SelectedEvents, InViewMode );
	}
}

ECheckBoxState SEventGraph::EventGraphViewMode_IsChecked( const EEventGraphViewModes::Type InViewMode ) const
{
	return (GetCurrentStateViewMode() == InViewMode) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SEventGraph::EventGraphType_OnCheckStateChanged( ECheckBoxState NewRadioState, const EEventGraphTypes::Type NewEventGraphType )
{
	const uint32 NumFrames = GetCurrentState()->GetNumFrames();

	if( NewRadioState == ECheckBoxState::Checked && GetCurrentStateEventGraphType() != NewEventGraphType )
	{
		FEventGraphStateRef EventGraphState = GetCurrentState();
		GetHierarchicalExpandedEvents( EventGraphState->ExpandedEvents );
		GetHierarchicalSelectedEvents( EventGraphState->SelectedEvents );

		EventGraphState->UpdateToNewEventGraphType( NewEventGraphType );
		SetEventGraphFromStateInternal( EventGraphState );
	}
}

ECheckBoxState SEventGraph::EventGraphType_IsChecked( const EEventGraphTypes::Type InEventGraphType ) const
{
	if( IsEventGraphStatesHistoryValid() )
	{
		const uint32 NumFrames = GetCurrentState()->GetNumFrames();

		if( NumFrames == 1 )
		{
			return GetCurrentStateEventGraphType() == InEventGraphType ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		else if( NumFrames > 1 )
		{
			return GetCurrentStateEventGraphType() == InEventGraphType ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Unchecked;
}

bool SEventGraph::EventGraphType_IsEnabled( const EEventGraphTypes::Type InEventGraphType ) const
{
	if( IsEventGraphStatesHistoryValid() )
	{
		const uint32 NumFrames = GetCurrentState()->GetNumFrames();
		if( InEventGraphType == EEventGraphTypes::OneFrame )
		{
			return NumFrames == 1 ? true : false;
		}
		else
		{
			return NumFrames > 1 ? true : false;
		}
	}
	return false;
}

void SEventGraph::SetTreeItemsForViewMode( const EEventGraphViewModes::Type NewViewMode, EEventGraphTypes::Type NewEventGraphType )
{
	GetCurrentState()->ViewMode = NewViewMode;
	GetCurrentState()->EventGraphType = NewEventGraphType;

	GetCurrentState()->ApplyCulling();
	GetCurrentState()->ApplyFiltering();

	if( GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical )
	{
		TreeView_Base->SetTreeItemsSource( &GetCurrentState()->GetRoot()->GetChildren() );
	}
	else if( GetCurrentStateViewMode() == EEventGraphViewModes::FlatInclusive || GetCurrentStateViewMode() == EEventGraphViewModes::FlatExclusive )
	{
		TreeView_Base->SetTreeItemsSource( &Events_Flat );
	}
	else if( GetCurrentStateViewMode() == EEventGraphViewModes::FlatInclusiveCoalesced || GetCurrentStateViewMode() == EEventGraphViewModes::FlatExclusiveCoalesced )
	{
		TreeView_Base->SetTreeItemsSource( &Events_FlatCoalesced );
	}
}

FReply SEventGraph::ExpandHotPath_OnClicked()
{
	ContextMenu_ExpandHotPath_Execute();
	return FReply::Handled();
}

void SEventGraph::HighlightHotPath_OnCheckStateChanged( ECheckBoxState InState )
{

}

void SEventGraph::TreeView_Refresh()
{
	if( TreeView_Base.IsValid() )
	{
		TreeView_Base->RequestTreeRefresh();
	}
}

void SEventGraph::TreeViewHeaderRow_CreateColumnArgs( const uint32 ColumnIndex )
{
	const FEventGraphColumn Column = FEventGraphColumns::Get().Collection[ColumnIndex];
	SHeaderRow::FColumn::FArguments ColumnArgs;

	ColumnArgs
	.ColumnId( Column.ID )
	.DefaultLabel( Column.ShortName )
	.SortMode( EColumnSortMode::None )
	.HAlignHeader( HAlign_Fill )
	.VAlignHeader( VAlign_Fill )
	.HeaderContentPadding( TOptional<FMargin>( 2.0f ) )
	.HAlignCell( HAlign_Fill ) 
	.VAlignCell( VAlign_Fill )
	.SortMode( this, &SEventGraph::TreeViewHeaderRow_GetSortModeForColumn, Column.ID )
	.OnSort( this, &SEventGraph::TreeViewHeaderRow_OnSortModeChanged )
	.FixedWidth( Column.FixedColumnWidth > 0.0f ? Column.FixedColumnWidth : TOptional<float>() )
	.HeaderContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign( Column.HorizontalAlignment )
		.VAlign( VAlign_Center )
		[
			SNew(STextBlock)
			.Text( Column.ShortName )
			.ToolTipText( Column.Description )
		]
	]
	.MenuContent()
	[
		TreeViewHeaderRow_GenerateColumnMenu( Column )
	];

	TreeViewHeaderColumnArgs.Add( Column.ID, ColumnArgs );
	TreeViewHeaderColumns.Add( Column.ID, Column );
}

void SEventGraph::InitializeAndShowHeaderColumns()
{
	ColumnSortMode = EColumnSortMode::Descending;
	ColumnBeingSorted = FEventGraphColumns::Get().Collection[EEventPropertyIndex::InclusiveTimeMS].ID;

	for (uint32 ColumnIndex = 0; ColumnIndex < FEventGraphColumns::Get().NumColumns; ColumnIndex++)
	{
		TreeViewHeaderRow_CreateColumnArgs( ColumnIndex );
	}

	for( auto It = TreeViewHeaderColumns.CreateConstIterator(); It; ++It )
	{
		const FEventGraphColumn& Column = It.Value();
		
		if( Column.bIsVisible )
		{
			TreeViewHeaderRow_ShowColumn( Column.ID );
		}
	}
}


void SEventGraph::TreeViewHeaderRow_OnSortModeChanged( const EColumnSortPriority::Type SortPriority, const FName& ColumnID, const EColumnSortMode::Type SortMode )
{
	SetSortModeForColumn( ColumnID, SortMode );
	TreeView_Refresh();
}

EColumnSortMode::Type SEventGraph::TreeViewHeaderRow_GetSortModeForColumn( const FName ColumnID ) const
{
	if( ColumnBeingSorted != ColumnID )
	{
		return EColumnSortMode::None;
	}

	return ColumnSortMode;
}

void SEventGraph::HeaderMenu_HideColumn_Execute( const FName ColumnID )
{
	FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(ColumnID);
	Column.bIsVisible = false;
	TreeViewHeaderRow->RemoveColumn( ColumnID );
}

void SEventGraph::TreeViewHeaderRow_ShowColumn( const FName ColumnID )
{
	FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked( ColumnID );
	Column.bIsVisible = true;
	SHeaderRow::FColumn::FArguments& ColumnArgs = TreeViewHeaderColumnArgs.FindChecked( ColumnID );

	const int32 NumColumns = TreeViewHeaderRow->GetColumns().Num();
	const int32 ColumnIndex = FMath::Max( 0, FMath::Min( (int32)Column.Index, NumColumns ) );
	TreeViewHeaderRow->InsertColumn( ColumnArgs, ColumnIndex );
}

bool SEventGraph::HeaderMenu_HideColumn_CanExecute( const FName ColumnID ) const
{
	const FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(ColumnID);
	return Column.bCanBeHidden;
}

TSharedRef< SWidget > SEventGraph::TreeViewHeaderRow_GenerateColumnMenu( const FEventGraphColumn& Column )
{
	bool bIsMenuVisible = false;
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, NULL );
	{
		if( Column.bCanBeHidden )
		{
			MenuBuilder.BeginSection("Column", LOCTEXT("TreeViewHeaderRow_Header_Column", "Column") );

			FUIAction Action_HideColumn
			( 
				FExecuteAction::CreateSP( this, &SEventGraph::HeaderMenu_HideColumn_Execute, Column.ID ),
				FCanExecuteAction::CreateSP( this, &SEventGraph::HeaderMenu_HideColumn_CanExecute, Column.ID )
			);

			MenuBuilder.AddMenuEntry
			( 
				LOCTEXT("TreeViewHeaderRow_HideColumn", "Hide"), 
				LOCTEXT("TreeViewHeaderRow_HideColumn_Desc", "Hides the selected column"),
				FSlateIcon(), Action_HideColumn, NAME_None, EUserInterfaceActionType::Button 
			);
			bIsMenuVisible = true;

			MenuBuilder.EndSection();
		}

		if( Column.bCanBeSorted )
		{
			MenuBuilder.BeginSection("SortMode", LOCTEXT("ContextMenu_Header_Misc_Sort_SortMode", "Sort Mode") );

			FUIAction Action_SortAscending
			(
				FExecuteAction::CreateSP( this, &SEventGraph::HeaderMenu_SortMode_Execute, Column.ID, EColumnSortMode::Ascending ),
				FCanExecuteAction::CreateSP( this, &SEventGraph::HeaderMenu_SortMode_CanExecute, Column.ID, EColumnSortMode::Ascending ),
				FIsActionChecked::CreateSP( this, &SEventGraph::HeaderMenu_SortMode_IsChecked, Column.ID, EColumnSortMode::Ascending )
			);
			MenuBuilder.AddMenuEntry
			( 
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending", "Sort Ascending"), 
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortAscending_Desc", "Sorts ascending"), 
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortAscending"), Action_SortAscending, NAME_None, EUserInterfaceActionType::RadioButton
			);

			FUIAction Action_SortDescending
			(
				FExecuteAction::CreateSP( this, &SEventGraph::HeaderMenu_SortMode_Execute, Column.ID, EColumnSortMode::Descending ),
				FCanExecuteAction::CreateSP( this, &SEventGraph::HeaderMenu_SortMode_CanExecute, Column.ID, EColumnSortMode::Descending ),
				FIsActionChecked::CreateSP( this, &SEventGraph::HeaderMenu_SortMode_IsChecked, Column.ID, EColumnSortMode::Descending )
			);
			MenuBuilder.AddMenuEntry
			( 
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending", "Sort Descending"), 
				LOCTEXT("ContextMenu_Header_Misc_Sort_SortDescending_Desc", "Sorts descending"), 
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.Misc.SortDescending"), Action_SortDescending, NAME_None, EUserInterfaceActionType::RadioButton 
			);
			bIsMenuVisible = true;

			MenuBuilder.EndSection();
		}

		if( Column.bCanBeFiltered )
		{
			MenuBuilder.BeginSection("FilterMode", LOCTEXT("ContextMenu_Header_Misc_Filter_FilterMode", "Filter Mode") );
			bIsMenuVisible = true;
			MenuBuilder.EndSection();
		}

		if( Column.bCanBeCulled )
		{
			MenuBuilder.BeginSection("CullMode", LOCTEXT("ContextMenu_Header_Misc_Cull_CullMode", "Cull Mode") );
			bIsMenuVisible = true;
			MenuBuilder.EndSection();
		}
	}

	/*
	HEADER COLUMN
	Show top ten
	Show top bottom
	Filter by list (avg, median, 10%,90%,etc.)
	Text box for filtering for each column instead of one text box used for filtering
	Grouping button for flat view modes (show at most X groups, show all groups for names)
	*/

	return bIsMenuVisible ? MenuBuilder.MakeWidget() : (TSharedRef<SWidget>)SNullWidget::NullWidget;
}

//-----------------------------------------------------------------------------

void SEventGraph::ContextMenu_ExecuteDummy( const FName ActionName )
{
#if	DEBUG_PROFILER_PERFORMANCE
	UE_LOG( Profiler, Log, TEXT("SEventGraph::ContextMenu_ExecuteDummy -> %s"), *ActionName.ToString() );
#endif // DEBUG_PROFILER_PERFORMANCE
}

bool SEventGraph::ContextMenu_CanExecuteDummy( const FName ActionName ) const
{
#if	DEBUG_PROFILER_PERFORMANCE
	UE_LOG( Profiler, Log, TEXT("SEventGraph::ContextMenu_CanExecuteDummy -> %s"), *ActionName.ToString() );
#endif // DEBUG_PROFILER_PERFORMANCE
	return false;
}

bool SEventGraph::ContextMenu_IsCheckedDummy( const FName ActionName ) const
{
#if	DEBUG_PROFILER_PERFORMANCE
	UE_LOG( Profiler, Log, TEXT("SEventGraph::ContextMenu_IsCheckedDummy -> %s"), *ActionName.ToString() );
#endif // DEBUG_PROFILER_PERFORMANCE
	return false;
}

/*-----------------------------------------------------------------------------
	UI ACTIONS
-----------------------------------------------------------------------------*/
void SEventGraph::ContextMenu_ExpandHotPath_Execute()
{
	TArray<FEventGraphSamplePtr> SelectedItems = TreeView_Base->GetSelectedItems();
	FEventGraphSamplePtr EventPtr = SelectedItems[0];

	ColumnSortMode = EColumnSortMode::Descending;
	ColumnBeingSorted = FEventGraphColumns::Get().Collection[EEventPropertyIndex::InclusiveTimeMS].ID;
	SortEvents();
	
	// Clear hot path
	TreeView_Base->ClearExpandedItems();
	GetCurrentState()->GetRoot()->SetBooleanStateForAllChildren<EEventPropertyIndex::bIsHotPath>(false);

	FEventGraphSamplePtr LastHotEvent;
	for( FEventGraphSamplePtr HotEvent = EventPtr; HotEvent.IsValid(); HotEvent = HotEvent->GetChildren().Num() > 0 ? HotEvent->GetChildren()[0] : NULL )
	{
		HotEvent->PropertyValueAsBool(EEventPropertyIndex::bIsHotPath) = true;

		HotEvent->_bIsHotPath = true;
		LastHotEvent = HotEvent;
	}

	// Expand all events from the bottom to the top most event.
	TArray<FEventGraphSamplePtr> StackToExpand;
	LastHotEvent->GetStack( StackToExpand );

	for( int EventIndex = StackToExpand.Num()-1; EventIndex >= 0; EventIndex--  )
	{
		TreeView_Base->SetItemExpansion( StackToExpand[EventIndex], true );
	}

	TreeView_Refresh();
}

bool SEventGraph::ContextMenu_ExpandHotPath_CanExecute() const
{
	return GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical && TreeView_Base->GetNumItemsSelected() == 1;
}

void SEventGraph::ContextMenu_CopySelectedToClipboard_Execute()
{
	TArray<FEventGraphSamplePtr> SelectedEvents = TreeView_Base->GetSelectedItems();

	FString Result;

	// Prepare header.
	for (uint32 ColumnIndex = 0; ColumnIndex < FEventGraphColumns::Get().NumColumns; ColumnIndex++)
	{
		const FEventGraphColumn& Column = FEventGraphColumns::Get().Collection[ColumnIndex];
		Result += FString::Printf( TEXT("\"%s\","), *Column.ShortName.ToString() );
	}
	Result += LINE_TERMINATOR;

	// Prepare selected samples.
	for( int32 EventIndex = 0; EventIndex < SelectedEvents.Num(); EventIndex++ )
	{
		const FEventGraphSamplePtr EventPtr = SelectedEvents[EventIndex];
		
		for (uint32 ColumnIndex = 0; ColumnIndex < FEventGraphColumns::Get().NumColumns; ColumnIndex++)
		{
			const FEventGraphColumn& Column = FEventGraphColumns::Get().Collection[ColumnIndex];
			if( Column.Index != EEventPropertyIndex::None )
			{
				const FString FormattedValue = EventPtr->GetFormattedValue( Column.Index );
				Result += FString::Printf( TEXT("\"%s\","), *FormattedValue );
			}
		}
		Result += LINE_TERMINATOR;
	}

	if( Result.Len() )
	{
		FPlatformApplicationMisc::ClipboardCopy( *Result );
	}
}

bool SEventGraph::ContextMenu_CopySelectedToClipboard_CanExecute() const
{
	const int32 NumSelectedItems = TreeView_Base->GetNumItemsSelected();
	return NumSelectedItems > 0;
}

void SEventGraph::ContextMenu_SelectStack_Execute()
{
	TArray<FEventGraphSamplePtr> SelectedEvents = TreeView_Base->GetSelectedItems();

	TArray<FEventGraphSamplePtr> ArrayStack;
	SelectedEvents[0]->GetStack( ArrayStack );

	for( int32 Nx = 0; Nx < ArrayStack.Num(); ++Nx )
	{
		TreeView_Base->SetItemSelection( ArrayStack[Nx], true, ESelectInfo::Direct );
	}
}

bool SEventGraph::ContextMenu_SelectStack_CanExecute() const
{
	bool bResult = false;
	TArray<FEventGraphSamplePtr> SelectedEvents = TreeView_Base->GetSelectedItems();
	if( SelectedEvents.Num() == 1 )
	{
		FEventGraphSamplePtr StackEvent = SelectedEvents[0];
		bResult = StackEvent->GetParent().IsValid() && !StackEvent->GetParent()->IsRoot();
	}

	return bResult;
}

void SEventGraph::ContextMenu_SortByColumn_Execute( const FName ColumnID )
{
	SetSortModeForColumn( ColumnID, EColumnSortMode::Descending );
	TreeView_Refresh();
}

bool SEventGraph::ContextMenu_SortByColumn_CanExecute( const FName ColumnID ) const
{
	return ColumnID != ColumnBeingSorted;
}

bool SEventGraph::ContextMenu_SortByColumn_IsChecked( const FName ColumnID )
{
	return ColumnID == ColumnBeingSorted;
}

void SEventGraph::ContextMenu_SortMode_Execute( const EColumnSortMode::Type InSortMode )
{
	SetSortModeForColumn( ColumnBeingSorted, InSortMode );
	TreeView_Refresh();
}

bool SEventGraph::ContextMenu_SortMode_CanExecute( const EColumnSortMode::Type InSortMode ) const
{
	return ColumnSortMode != InSortMode;
}

bool SEventGraph::ContextMenu_SortMode_IsChecked( const EColumnSortMode::Type InSortMode )
{
	return ColumnSortMode == InSortMode;
}

void SEventGraph::ContextMenu_ResetColumns_Execute()
{
	ColumnSortMode = EColumnSortMode::Descending;
	ColumnBeingSorted = FEventGraphColumns::Get().Collection[EEventPropertyIndex::InclusiveTimeMS].ID;

	for( uint32 ColumnIndex = 0; ColumnIndex < FEventGraphColumns::Get().NumColumns; ColumnIndex++ )
	{
		const FEventGraphColumn& DefaultColumn = FEventGraphColumns::Get().Collection[ColumnIndex];
		const FEventGraphColumn& CurrentColumn = TreeViewHeaderColumns.FindChecked( DefaultColumn.ID );

		if( DefaultColumn.bIsVisible && !CurrentColumn.bIsVisible )
		{
			TreeViewHeaderRow_ShowColumn( DefaultColumn.ID );
		}
		else if( !DefaultColumn.bIsVisible && CurrentColumn.bIsVisible )
		{
			HeaderMenu_HideColumn_Execute( DefaultColumn.ID );
		}
	}
}

bool SEventGraph::ContextMenu_ResetColumns_CanExecute() const
{
	return true;
}

void SEventGraph::ContextMenu_ToggleColumn_Execute( const FName ColumnID )
{
	FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(ColumnID);
	if( Column.bIsVisible )
	{
		HeaderMenu_HideColumn_Execute( ColumnID );
	}
	else
	{
		TreeViewHeaderRow_ShowColumn( ColumnID );
	}
}

bool SEventGraph::ContextMenu_ToggleColumn_CanExecute( const FName ColumnID ) const
{
	const FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(ColumnID);
	return Column.bCanBeHidden;
}

bool SEventGraph::ContextMenu_ToggleColumn_IsChecked( const FName ColumnID )
{
	const FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(ColumnID);
	return Column.bIsVisible;
}

void SEventGraph::HeaderMenu_SortMode_Execute( const FName ColumnID, const EColumnSortMode::Type InSortMode )
{
	SetSortModeForColumn( ColumnID, InSortMode );
	TreeView_Refresh();
}

bool SEventGraph::HeaderMenu_SortMode_CanExecute( const FName ColumnID, const EColumnSortMode::Type InSortMode ) const
{
	const FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(ColumnID);
	const bool bIsValid = Column.bCanBeSorted;

	bool bCanExecute = ColumnBeingSorted != ColumnID ? true : ColumnSortMode != InSortMode;
	bCanExecute = bCanExecute && bIsValid;

	return bCanExecute;
}

bool SEventGraph::HeaderMenu_SortMode_IsChecked( const FName ColumnID, const EColumnSortMode::Type InSortMode )
{
	return ColumnBeingSorted == ColumnID && ColumnSortMode == InSortMode;
}

/*-----------------------------------------------------------------------------
	CreateEvents_...
-----------------------------------------------------------------------------*/

void SEventGraph::CreateEvents()
{
	// #YRX_Profiler 2015-12-17  Fix recursive calls accounting.

	// Linear
	GetCurrentState()->GetRoot()->GetLinearEvents( Events_Flat, true );

	// Linear Coalesced by Name
	// Event name -> Events
	TMap< FName, TArray<FEventGraphSamplePtr> > FlatIncCoalescedEvents;
	const int32 NumLinearSamples = Events_Flat.Num();
	Events_FlatCoalesced.Reset( NumLinearSamples );
	HierarchicalToFlatCoalesced.Reset();

	for( int32 LinearEventIndex = 0; LinearEventIndex < NumLinearSamples; LinearEventIndex++ )
	{
		const FEventGraphSamplePtr EventPtr = Events_Flat[LinearEventIndex];
		FlatIncCoalescedEvents.FindOrAdd( EventPtr->_StatName ).Add( EventPtr->DuplicateSimplePtr() );
		HierarchicalToFlatCoalesced.Add( EventPtr->_StatName, EventPtr );
	}

	// Should ignore recursion!
	for( auto It = FlatIncCoalescedEvents.CreateConstIterator(); It; ++It )
	{
		const TArray<FEventGraphSamplePtr>& InclusiveCoalescedEvents = It.Value();

		FEventGraphSamplePtr CoalescedEvent = InclusiveCoalescedEvents[0];
		for( int32 EventIndex = 1; EventIndex < InclusiveCoalescedEvents.Num(); EventIndex++ )
		{
			CoalescedEvent->Combine( InclusiveCoalescedEvents[EventIndex] );
		}

		Events_FlatCoalesced.Add( CoalescedEvent );
	}

	// CoalesceEventsByColumnName/ID
}

void SEventGraph::ShowEventsInViewMode( const TArray<FEventGraphSamplePtr>& EventsToSynchronize, const EEventGraphViewModes::Type NewViewMode )
{
	FEventGraphStateRef EventGraphState = GetCurrentState();

	GetHierarchicalSelectedEvents( EventGraphState->SelectedEvents, &EventsToSynchronize );
	GetHierarchicalExpandedEvents( EventGraphState->ExpandedEvents );

	SetTreeItemsForViewMode( NewViewMode, GetCurrentStateEventGraphType() );

	SetHierarchicalSelectedEvents( EventGraphState->SelectedEvents );
	SetHierarchicalExpandedEvents( EventGraphState->ExpandedEvents );
		
	EEventPropertyIndex ColumnIndex = FEventGraphColumns::Get().ColumnNameToIndexMapping.FindChecked( ColumnBeingSorted )->Index;

	if( NewViewMode == EEventGraphViewModes::FlatInclusive || NewViewMode == EEventGraphViewModes::FlatInclusiveCoalesced || NewViewMode == EEventGraphViewModes::Hierarchical )
	{
		ColumnIndex = EEventPropertyIndex::InclusiveTimeMS;
		SetSortModeForColumn( FEventGraphColumns::Get().Collection[ColumnIndex].ID, EColumnSortMode::Descending );
	}
	else if( NewViewMode == EEventGraphViewModes::FlatExclusive || NewViewMode == EEventGraphViewModes::FlatExclusiveCoalesced )
	{
		ColumnIndex = EEventPropertyIndex::ExclusiveTimeMS;
		SetSortModeForColumn( FEventGraphColumns::Get().Collection[ColumnIndex].ID, EColumnSortMode::Descending );
	}

	ScrollToTheSlowestSelectedEvent( ColumnIndex );

	if( NewViewMode == EEventGraphViewModes::Hierarchical )
	{
		TArray<FEventGraphSamplePtr> SelectedEvents = TreeView_Base->GetSelectedItems();
		for( int32 Nx = 0; Nx < SelectedEvents.Num(); ++Nx )
		{
			// Find stack for the specified event and expand that stack.
			FEventGraphSamplePtr EventToExpand = SelectedEvents[Nx];
			TArray<FEventGraphSamplePtr> StackToExpand;
			EventToExpand->GetStack( StackToExpand );

			for( int32 Index = 0; Index < StackToExpand.Num(); ++Index )
			{
				TreeView_Base->SetItemExpansion( StackToExpand[Index], true );
			}	
		}
	}	
		
	TreeView_Refresh();
}

void SEventGraph::ScrollToTheSlowestSelectedEvent( EEventPropertyIndex ColumnIndex )
{
	TArray<FEventGraphSamplePtr> SelectedEvents = TreeView_Base->GetSelectedItems();
	if( SelectedEvents.Num() > 0 )
	{
		// Sort events by the inclusive or the exclusive time, depends on the view mode.
		const FEventGraphColumn& Column = FEventGraphColumns::Get().Collection[ColumnIndex];
		FEventArraySorter::Sort( SelectedEvents, Column.ID, EEventCompareOps::Greater );

		// Scroll to the the slowest item.
		TreeView_Base->RequestScrollIntoView( SelectedEvents[0] );
	}
}

/*-----------------------------------------------------------------------------
	Get/Set HierarchicalSelectedEvents
-----------------------------------------------------------------------------*/

void SEventGraph::GetHierarchicalSelectedEvents( TArray< FEventGraphSamplePtr >& out_HierarchicalSelectedEvents, const TArray<FEventGraphSamplePtr>* SelectedEvents /*= NULL*/ ) const
{
	out_HierarchicalSelectedEvents.Reset();

	TArray<FEventGraphSamplePtr> ViewSelectedEvents;
	if( SelectedEvents != NULL )
	{
		ViewSelectedEvents = *SelectedEvents;
	}
	else
	{
		ViewSelectedEvents = TreeView_Base->GetSelectedItems();
	}

	if( GetCurrentStateViewMode() == EEventGraphViewModes::FlatInclusiveCoalesced || GetCurrentStateViewMode() == EEventGraphViewModes::FlatExclusiveCoalesced )
	{
		for( int32 Nx = 0; Nx < ViewSelectedEvents.Num(); ++Nx )
		{
			HierarchicalToFlatCoalesced.MultiFind( ViewSelectedEvents[Nx]->_StatName, out_HierarchicalSelectedEvents );
		}
	}
	else
	{
		out_HierarchicalSelectedEvents = ViewSelectedEvents;
	}
}

void SEventGraph::SetHierarchicalSelectedEvents( const TArray<FEventGraphSamplePtr>& HierarchicalSelectedEvents )
{
	struct FCoalescedEventMatcher
	{
		FCoalescedEventMatcher( const FEventGraphSamplePtr& InEventPtr )
			: EventPtr( InEventPtr )
		{}

		bool operator()(const FEventGraphSamplePtr& Other) const
		{
			return EventPtr->_StatName == Other->_StatName;
		}
	protected:
		const FEventGraphSamplePtr& EventPtr;
	};

	TArray<FEventGraphSamplePtr> SelectedEvents;

	if( GetCurrentStateViewMode() == EEventGraphViewModes::FlatInclusiveCoalesced || GetCurrentStateViewMode() == EEventGraphViewModes::FlatExclusiveCoalesced )
	{
		for( int32 Nx = 0; Nx < HierarchicalSelectedEvents.Num(); ++Nx )
		{
			const int32 Index = Events_FlatCoalesced.IndexOfByPredicate(FCoalescedEventMatcher(HierarchicalSelectedEvents[Nx]));
			if( Index != INDEX_NONE )
			{
				SelectedEvents.AddUnique( Events_FlatCoalesced[Index] );
			}
		}
	}
	else
	{
		SelectedEvents = HierarchicalSelectedEvents;
	}

	TreeView_Base->ClearSelection();
	for( int32 Nx = 0; Nx < SelectedEvents.Num(); ++Nx )
	{
		TreeView_Base->SetItemSelection( SelectedEvents[Nx], true );
	}
}

/*-----------------------------------------------------------------------------
	Get/Set HierarchicalExpandedEvents
-----------------------------------------------------------------------------*/

void SEventGraph::GetHierarchicalExpandedEvents( TSet<FEventGraphSamplePtr>& out_HierarchicalExpandedEvents ) const
{
	if( GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical )
	{
		out_HierarchicalExpandedEvents.Empty();
		TreeView_Base->GetExpandedItems( out_HierarchicalExpandedEvents );
	}
}

void SEventGraph::SetHierarchicalExpandedEvents( const TSet<FEventGraphSamplePtr>& HierarchicalExpandedEvents )
{
	if( GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical )
	{
		TreeView_Base->ClearExpandedItems();
		for( auto It = HierarchicalExpandedEvents.CreateConstIterator(); It; ++It )
		{
			TreeView_Base->SetItemExpansion( *It, true );
		}
	}
}

/*-----------------------------------------------------------------------------
	Function details
-----------------------------------------------------------------------------*/

FReply SEventGraph::CallingCalledFunctionButton_OnClicked( FEventGraphSamplePtr EventPtr )
{
	if( !EventPtr->_bIsCulled )
	{
		UpdateFunctionDetailsForEvent( EventPtr );

		TArray<FEventGraphSamplePtr> Events;
		Events.Add( EventPtr );
		ShowEventsInViewMode( Events, GetCurrentStateViewMode() );
	}

	return FReply::Handled();
}

void SEventGraph::DisableFunctionDetails()
{
	(*CurrentFunctionDescSlot )
	[
		SNew(STextBlock)
		.WrapTextAt( 128.0f )
		.Text( LOCTEXT("FunctionDetails_SelectOneEvent","Function details view works only if you select one event. Please select an individual event to proceed.") )
		.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
	];

	VerticalBox_TopCalling->ClearChildren();
	VerticalBox_TopCalled->ClearChildren();

	HighlightedEventName = NAME_None;
}

void SEventGraph::UpdateFunctionDetailsForEvent( FEventGraphSamplePtr SelectedEvent )
{
	GenerateCallerCalleeGraph( SelectedEvent );

	(*CurrentFunctionDescSlot)
	[
		GetContentForEvent(SelectedEvent,1.0f,true)
	];

	RecreateWidgetsForTopEvents( VerticalBox_TopCalling, TopCallingFunctionEvents );
	RecreateWidgetsForTopEvents( VerticalBox_TopCalled, TopCalledFunctionEvents );

	HighlightedEventName = SelectedEvent->_StatName;
}

void SEventGraph::UpdateFunctionDetails()
{
	TArray<FEventGraphSamplePtr> SelectedItems = TreeView_Base->GetSelectedItems();
	if( SelectedItems.Num() == 1 )
	{
		UpdateFunctionDetailsForEvent( SelectedItems[0] );
	}
	else
	{
		DisableFunctionDetails(); 
	}
}


/*-----------------------------------------------------------------------------
	Function details
-----------------------------------------------------------------------------*/

void SEventGraph::GenerateCallerCalleeGraph( FEventGraphSamplePtr SelectedEvent )
{
	TArray< FEventGraphSamplePtr > EventsByName;
	HierarchicalToFlatCoalesced.MultiFind( SelectedEvent->_StatName, EventsByName );

	// Parent
	TSet< FEventGraphSamplePtr > CallingFunctionEventSet;
	for( int32 Nx = 0; Nx < EventsByName.Num(); ++Nx )
	{
		FEventGraphSamplePtr ParentPtr = EventsByName[Nx]->GetParent();
		if( ParentPtr.IsValid() && !ParentPtr->IsRoot() )
		{
			CallingFunctionEventSet.Add( ParentPtr );
		}
	}

	GenerateTopEvents( CallingFunctionEventSet, TopCallingFunctionEvents );
	CalculateEventWeights( TopCallingFunctionEvents );

	// Children
	TSet< FEventGraphSamplePtr > CalledFunctionEventSet;
	for( int32 Nx = 0; Nx < EventsByName.Num(); ++Nx )
	{
		CalledFunctionEventSet.Append( EventsByName[Nx]->GetChildren() );
	}

	GenerateTopEvents( CalledFunctionEventSet, TopCalledFunctionEvents );
	CalculateEventWeights( TopCalledFunctionEvents );
}

void SEventGraph::GenerateTopEvents( const TSet< FEventGraphSamplePtr >& EventPtrSet, TArray<FEventPtrAndMisc>& out_Results )
{
	const int NumTopEvents = 5;
	TArray<FEventGraphSamplePtr> EventPtrArray = EventPtrSet.Array();

	// Calculate total time.
	double TotalTimeMS = 0.0f;
	for( int32 Nx = 0; Nx < EventPtrArray.Num(); ++Nx )
	{
		TotalTimeMS += EventPtrArray[Nx]->_InclusiveTimeMS;
	}

	// Sort events by the inclusive time.
	const FEventGraphColumn& Column = FEventGraphColumns::Get().Collection[EEventPropertyIndex::InclusiveTimeMS];
	FEventArraySorter::Sort( EventPtrArray, Column.ID, EEventCompareOps::Greater );

	// Calculate total time for the top events.
	double Top5TimeMS = 0.0f;
	for( int32 Nx = 0; Nx < EventPtrArray.Num() && Nx < NumTopEvents; ++Nx )
	{
		Top5TimeMS += EventPtrArray[Nx]->_InclusiveTimeMS;
	}

	// Calculate values for top events.
	out_Results.Reset();
	for( int32 Nx = 0; Nx < EventPtrArray.Num() && Nx < NumTopEvents; ++Nx )
	{
		const FEventGraphSamplePtr EventPtr = EventPtrArray[Nx];
		const float IncTimeToTotalPct = EventPtr->_InclusiveTimeMS / TotalTimeMS;
		const float HeightPct = EventPtr->_InclusiveTimeMS / Top5TimeMS;
		out_Results.Add( FEventPtrAndMisc(EventPtr,IncTimeToTotalPct,HeightPct) );
	}
}

void SEventGraph::CalculateEventWeights( TArray<FEventPtrAndMisc>& Events )
{
	// TODO: This value was calculated by hand 
	// and gives reasonable results for scaling button in the function details.
	// Maximum number of visible buttons is 5, 5 buttons require 100px, where 20px for each button
	// The height of the area is 190px so 190px/20px = ~9
	const float MaxButtons = 9.0f;
	const float TotalHeightPct = MaxButtons / 5.0f;
	const float MinHeightPct = TotalHeightPct / MaxButtons;

	// Update min height pct for buttons where the ratio is too low
	float CurrentHeightPct = 0.0f;
	for( int32 Tx = 0; Tx < Events.Num(); ++Tx )
	{
		FEventPtrAndMisc& EventPtr = Events[Tx];
		EventPtr.HeightPct = FMath::Max( EventPtr.HeightPct, MinHeightPct );
		CurrentHeightPct += EventPtr.HeightPct;
	}

	// Update height pct to fit all button into visible area.
	const float FitHeightPct = TotalHeightPct / CurrentHeightPct;
	for( int32 Tx = 0; Tx < Events.Num(); ++Tx )
	{
		FEventPtrAndMisc& EventPtr = Events[Tx];
		EventPtr.HeightPct *= FitHeightPct;
	}
}

void SEventGraph::RecreateWidgetsForTopEvents( const TSharedPtr<SVerticalBox>& DestVerticalBox, const TArray<FEventPtrAndMisc>& TopEvents )
{
	DestVerticalBox->ClearChildren();
	for( int32 Nx = 0; Nx < TopEvents.Num(); ++Nx )
	{
		const FEventPtrAndMisc& EventPtrAndPct = TopEvents[Nx];

		DestVerticalBox->AddSlot()
		.FillHeight( EventPtrAndPct.HeightPct )
		.Padding( 1.0f )
		[
			SNew(SButton)
			.HAlign( HAlign_Left )
			.VAlign( VAlign_Center )
			.TextStyle( FEditorStyle::Get(), "Profiler.Tooltip" )
			.ContentPadding( FMargin(4.0f, 1.0f) )
			.OnClicked( this, &SEventGraph::CallingCalledFunctionButton_OnClicked, EventPtrAndPct.EventPtr )
			[
				GetContentForEvent( EventPtrAndPct.EventPtr, EventPtrAndPct.IncTimeToTotalPct, false )
			]
		];	
	}
}

FString SEventGraph::GetEventDescription( FEventGraphSamplePtr EventPtr, float Pct, const bool bSimple )
{
	const bool bIgnoreEventName = EventPtr->_ThreadName == EventPtr->_StatName;

	const FString ThreadName = EventPtr->_ThreadName.GetPlainNameString().LeftChop( 9 );// TODO: Fix this
	const FString EventName = FProfilerHelper::ShortenName(EventPtr->_StatName.GetPlainNameString(),28);

	const FString ThreadAndEventName = bIgnoreEventName ? ThreadName : FString::Printf( TEXT("%s:%s"), *ThreadName, *EventName );

	const FString Caption = FString::Printf
	( 
		TEXT("%s, %.1f%% (%s)"), 
		*ThreadAndEventName,
		Pct*100.0f, 
		*EventPtr->GetFormattedValue(EEventPropertyIndex::InclusiveTimeMS)
	);

	return bSimple ? ThreadAndEventName : Caption;
}


TSharedRef<SHorizontalBox> SEventGraph::GetContentForEvent( FEventGraphSamplePtr EventPtr, float Pct, const bool bSimple )
{
	TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);

	Content->AddSlot()
	.AutoWidth()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(STextBlock)
		.Text( FText::FromString(GetEventDescription(EventPtr,Pct,bSimple)) )
		.TextStyle( FEditorStyle::Get(), bSimple ? TEXT("Profiler.Tooltip") : TEXT("Profiler.EventGraph.DarkText") )
	];

	if( EventPtr->_bIsCulled )
	{
		Content->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew( SImage )
			.Image( FEditorStyle::GetBrush("Profiler.EventGraph.CulledEvent") )
			.ToolTipText( LOCTEXT("Misc_EventCulled","Event is culled") )
		];
	}

	if( EventPtr->_bIsFiltered )
	{
		Content->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew( SImage )
			.Image( FEditorStyle::GetBrush("Profiler.EventGraph.FilteredEvent") )
			.ToolTipText( LOCTEXT("Misc_EventFiltered","Event is filtered") )
		];
	}

	Content->AddSlot()
	.AutoWidth()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew( SImage )
		.Image( FEditorStyle::GetBrush("Profiler.Tooltip.HintIcon10") )
		.ToolTip( SEventGraphTooltip::GetTableCellTooltip( EventPtr ) )
	];

	return Content;
}


/*-----------------------------------------------------------------------------
	History management
-----------------------------------------------------------------------------*/

void SEventGraph::SetNewEventGraphState( const FEventGraphDataRef AverageEventGraph, const FEventGraphDataRef MaximumEventGraph, bool bInitial )
{
	PROFILER_SCOPE_LOG_TIME( TEXT( "SEventGraph::UpdateEventGraph" ), nullptr );

	// Store current operation.
	SaveCurrentEventGraphState();
	FEventGraphState* Op = new FEventGraphState( AverageEventGraph->DuplicateAsRef(), MaximumEventGraph->DuplicateAsRef() );
	CurrentStateIndex = EventGraphStatesHistory.Add( MakeShareable(Op) );
	RestoreEventGraphStateFrom( GetCurrentState(), bInitial );
	FillThreadFilterOptions();
}

FReply SEventGraph::HistoryBack_OnClicked()
{
	SwitchToEventGraphState( CurrentStateIndex-1 );
	return FReply::Handled();
}

bool SEventGraph::HistoryBack_IsEnabled() const
{
	return EventGraphStatesHistory.Num() > 1 && CurrentStateIndex > 0;
}

FText SEventGraph::HistoryBack_GetToolTipText() const
{
	// TODO: Add a nicer custom tooltip.
	if( HistoryBack_IsEnabled() )
	{
		return FText::Format( LOCTEXT("HistoryBack_Tooltip", "Back to {0}"), EventGraphStatesHistory[CurrentStateIndex-1]->GetFullDescription() );
	}

	return FText::GetEmpty();
}

FReply SEventGraph::HistoryForward_OnClicked()
{
	SwitchToEventGraphState( CurrentStateIndex+1 );
	return FReply::Handled();
}

bool SEventGraph::HistoryForward_IsEnabled() const
{
	return EventGraphStatesHistory.Num() > 1 && CurrentStateIndex < EventGraphStatesHistory.Num()-1;
}

FText SEventGraph::HistoryForward_GetToolTipText() const
{
	// TODO: Add a nicer custom tooltip.
	if( HistoryForward_IsEnabled() )
	{
		return FText::Format( LOCTEXT("HistoryForward_Tooltip", "Forward to {0}"), EventGraphStatesHistory[CurrentStateIndex+1]->GetFullDescription() );
	}

	return FText::GetEmpty();
}

bool SEventGraph::EventGraph_IsEnabled() const
{
	return EventGraphStatesHistory.Num() > 0;
}

bool SEventGraph::HistoryList_IsEnabled() const
{
	return EventGraphStatesHistory.Num() > 1;
}

TSharedRef<SWidget> SEventGraph::HistoryList_GetMenuContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	for( int32 StateIndex = 0; StateIndex < EventGraphStatesHistory.Num(); ++StateIndex )
	{
		const FEventGraphStateRef StateRef = EventGraphStatesHistory[StateIndex];
		MenuBuilder.AddMenuEntry
		(
			StateRef->GetFullDescription(),
			FText(),
			FSlateIcon(),
			HistoryList_GoTo_Custom( StateIndex ),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	}

	return MenuBuilder.MakeWidget();
}

void SEventGraph::HistoryList_GoTo_Execute( int32 StateIndex )
{
	if( StateIndex != CurrentStateIndex )
	{
		SwitchToEventGraphState( StateIndex );
	}
}

void SEventGraph::SaveCurrentEventGraphState()
{
	if( EventGraphStatesHistory.Num() > 0 )
	{
		FEventGraphStateRef EventGraphState = GetCurrentState();
		GetHierarchicalExpandedEvents( EventGraphState->ExpandedEvents );
		GetHierarchicalSelectedEvents( EventGraphState->SelectedEvents );
	}
}

void SEventGraph::SetEventGraphFromStateInternal( const FEventGraphStateRef& EventGraphState )
{
	EventGraphState->ApplyCulling();
	EventGraphState->ApplyFiltering();

	if( EventGraphState->ExpandedCulledEvents.Num() > 0 )
	{
		const int32 NumChildren = EventGraphState->ExpandedCulledEvents.Num();
		for( int32 Nx = 0; Nx < NumChildren; ++Nx )
		{
			const FEventGraphSamplePtr& EventPtr = EventGraphState->ExpandedCulledEvents[Nx];
			//EventPtr->PropertyValueAsBool( EEventPropertyIndex::bIsCulled) = false;
			EventPtr->_bIsCulled = false;
			EventPtr->RequestNotCulledChildrenUpdate();
		}
	}

	CreateEvents();
	SetTreeItemsForViewMode( EventGraphState->ViewMode, EventGraphState->EventGraphType );

	SetHierarchicalSelectedEvents( EventGraphState->SelectedEvents );
	SetHierarchicalExpandedEvents( EventGraphState->ExpandedEvents );
	SortEvents();
	ScrollToTheSlowestSelectedEvent( EEventPropertyIndex::InclusiveTimeMS );

	UpdateFunctionDetails();
	TreeView_Refresh();
}


void SEventGraph::RestoreEventGraphStateFrom( const FEventGraphStateRef EventGraphState, const bool bRestoredFromHistoryEvent /*= true*/ )
{
	SetEventGraphFromStateInternal( EventGraphState );

	if( bRestoredFromHistoryEvent )
	{
		// Broadcast that a new graph event has been set.
		EventGraphRestoredFromHistoryEvent.Broadcast( EventGraphState->GetEventGraph()->GetFrameStartIndex(), EventGraphState->GetEventGraph()->GetFrameEndIndex() );
	}
}

void SEventGraph::SwitchToEventGraphState( int32 StateIndex )
{
	SaveCurrentEventGraphState();
	CurrentStateIndex = StateIndex;
	RestoreEventGraphStateFrom( GetCurrentState() );
}

/*-----------------------------------------------------------------------------
	UI Actions
-----------------------------------------------------------------------------*/

void SEventGraph::BindCommands()
{
	Map_SelectAllFrames_Global();
}

void SEventGraph::SetRoot_Execute()
{
	TArray<FEventGraphSamplePtr> SelectedLeafs;
	GetHierarchicalSelectedEvents( SelectedLeafs );
	TMap< FEventGraphSamplePtr, TSet<FEventGraphSamplePtr> > StacksForSelectedLeafs;

	// Grab stack for all selected events.
	for( int32 Nx = 0; Nx < SelectedLeafs.Num(); ++Nx )
	{
		const FEventGraphSamplePtr SelectedLeaf = SelectedLeafs[Nx];
		TArray<FEventGraphSamplePtr> ArrayStack;
		SelectedLeaf->GetStack( ArrayStack );

		TSet<FEventGraphSamplePtr> SetStack;
		SetStack.Append( ArrayStack );

		StacksForSelectedLeafs.Add( SelectedLeaf, SetStack );
	}

	// Remove duplicated stacks.
	// Not super efficient, but should be ok for now.
	for( auto OuterIt = StacksForSelectedLeafs.CreateIterator(); OuterIt; ++OuterIt )
	{
		const TSet<FEventGraphSamplePtr>& OuterStack = OuterIt.Value();
		const FEventGraphSamplePtr OuterLeafPtr = OuterIt.Key();

		for( auto InnerIt = StacksForSelectedLeafs.CreateIterator(); InnerIt; ++InnerIt )
		{
			const TSet<FEventGraphSamplePtr>& InnerStack = InnerIt.Value();
			const FEventGraphSamplePtr InnerLeafPtr = InnerIt.Key();

			// The same roots, so ignore.
			if( InnerLeafPtr == OuterLeafPtr )
			{
				continue;
			}

			const bool bRemoveInner  = InnerStack.Contains( OuterLeafPtr );
			const bool bRemoveOuter = OuterStack.Contains( InnerLeafPtr );

			if( bRemoveOuter )
			{
				OuterIt.RemoveCurrent();
				break;
			}
			else if( bRemoveInner )
			{
				InnerIt.RemoveCurrent();
			}
		}
	}

	TArray<FEventGraphSamplePtr> UniqueLeafs;
	StacksForSelectedLeafs.GenerateKeyArray( UniqueLeafs );

	// Store current operation.
	SaveCurrentEventGraphState();
	FEventGraphState* Op = GetCurrentState()->CreateCopyWithNewRoot( UniqueLeafs );
	CurrentStateIndex = EventGraphStatesHistory.Insert( MakeShareable(Op), CurrentStateIndex+1 );
	RestoreEventGraphStateFrom( GetCurrentState() );
}

bool SEventGraph::SetRoot_CanExecute() const
{
	const int32 NumItemsSelected = TreeView_Base->GetNumItemsSelected();
	return NumItemsSelected > 0 && NumItemsSelected < 16;
}

void SEventGraph::ClearHistory_Execute()
{
	// Remove all history from the currently visible event graph, but leave the default state.
	const FEventGraphStateRef EventGraphState = GetCurrentState();

	for( int32 Nx = 0; Nx < EventGraphStatesHistory.Num(); ++Nx )
	{
		const FEventGraphStateRef It = EventGraphStatesHistory[Nx];
		if( It->MaximumEventGraph == EventGraphState->MaximumEventGraph && It->HistoryType != EEventHistoryTypes::NewEventGraph )
		{
			EventGraphStatesHistory.RemoveAt( Nx, 1, false );
			Nx--;
		}
	}

	// Find new index of the current state.
	for( int32 Nx = 0; Nx < EventGraphStatesHistory.Num(); ++Nx )
	{
		const FEventGraphStateRef It = EventGraphStatesHistory[Nx];
		if( It->MaximumEventGraph == EventGraphState->MaximumEventGraph )
		{
			CurrentStateIndex = Nx;
			break;
		}
	}

	RestoreEventGraphStateFrom( GetCurrentState() );
}

bool SEventGraph::ClearHistory_CanExecute() const
{
	bool bCanExecute = false;
	const FEventGraphStateRef EventGraphState = GetCurrentState();

	for( int32 Nx = 0; Nx < EventGraphStatesHistory.Num(); ++Nx )
	{
		const FEventGraphStateRef It = EventGraphStatesHistory[Nx];
		if( It->MaximumEventGraph == EventGraphState->MaximumEventGraph && It->HistoryType != EEventHistoryTypes::NewEventGraph )
		{
			bCanExecute = true;
		}
	}
	return bCanExecute;
}

void SEventGraph::FilterOutByProperty_Execute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset )
{
	PROFILER_SCOPE_LOG_TIME( TEXT( "SEventGraph::FilterOutByProperty_Execute" ), nullptr );

	// Store current operation.
	SaveCurrentEventGraphState();
	FEventGraphState* Op = GetCurrentState()->CreateCopyWithFiltering( PropertyName, EventPtr );
	CurrentStateIndex = EventGraphStatesHistory.Insert( MakeShareable(Op), CurrentStateIndex+1 );
	RestoreEventGraphStateFrom( GetCurrentState() );
}

bool SEventGraph::FilterOutByProperty_CanExecute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset ) const
{
	const FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(PropertyName);
	if( bReset )
	{
		return false;
	}
	else
	{
		return EventPtr.IsValid() && Column.bCanBeFiltered;
	}
}

void SEventGraph::CullByProperty_Execute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset )
{
	PROFILER_SCOPE_LOG_TIME( TEXT( "SEventGraph::CullByProperty_Execute" ), nullptr );

	// Store current operation.
	SaveCurrentEventGraphState();
	FEventGraphState* Op = GetCurrentState()->CreateCopyWithCulling( PropertyName, EventPtr );
	CurrentStateIndex = EventGraphStatesHistory.Insert( MakeShareable(Op), CurrentStateIndex+1 );
	RestoreEventGraphStateFrom( GetCurrentState() );
}

bool SEventGraph::CullByProperty_CanExecute( const FEventGraphSamplePtr EventPtr, const FName PropertyName, const bool bReset ) const
{
	const FEventGraphColumn& Column = TreeViewHeaderColumns.FindChecked(PropertyName);
	if( bReset )
	{
		return false;
	}
	else
	{
		return EventPtr.IsValid() && Column.bCanBeCulled;
	}
}

void SEventGraph::GetEventsForChangingExpansion( TArray<FEventGraphSamplePtr>& out_Events, const ESelectedEventTypes::Type SelectedEventType )
{
	if( SelectedEventType == ESelectedEventTypes::AllEvents )
	{
		out_Events = GetCurrentState()->GetRealRoot()->GetChildren();
	}
	else if( SelectedEventType == ESelectedEventTypes::SelectedEvents )
	{
		out_Events = TreeView_Base->GetSelectedItems();
	}
	else if( SelectedEventType == ESelectedEventTypes::SelectedThreadEvents )
	{
		TArray<FEventGraphSamplePtr> SelectedItems = TreeView_Base->GetSelectedItems();
		TSet<FEventGraphSamplePtr> ThreadEventSet;

		for( int32 EventIndex = 0; EventIndex < SelectedItems.Num(); EventIndex++ )
		{
			const FEventGraphSamplePtr ThreadEvent = SelectedItems[EventIndex]->GetOutermost();
			ThreadEventSet.Add( ThreadEvent );
		}

		out_Events = ThreadEventSet.Array();
	}
}

void SEventGraph::SetExpansionForEvents_Execute( const ESelectedEventTypes::Type SelectedEventType, bool bShouldExpand )
{
	TArray<FEventGraphSamplePtr> Events;
	GetEventsForChangingExpansion( Events, SelectedEventType );
	TreeView_SetItemsExpansion_Recurrent( Events, bShouldExpand );
}

bool SEventGraph::SetExpansionForEvents_CanExecute( const ESelectedEventTypes::Type SelectedEventType, bool bShouldExpand ) const
{
	const int32 NumSelectedItems = TreeView_Base->GetNumItemsSelected();
	if( SelectedEventType == ESelectedEventTypes::AllEvents )
	{
		return GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical;
	}
	else if( SelectedEventType == ESelectedEventTypes::SelectedEvents )
	{	
		return GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical && NumSelectedItems > 0;
	}
	else if( SelectedEventType == ESelectedEventTypes::SelectedThreadEvents )
	{
		return GetCurrentStateViewMode() == EEventGraphViewModes::Hierarchical && NumSelectedItems > 0;
	}
	return false;
}

void SEventGraph::Map_SelectAllFrames_Global()
{
	TSharedRef<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	const FProfilerCommands& ProfilerCommands = FProfilerManager::GetCommands();
	const FProfilerActionManager& ProfilerActionManager = FProfilerManager::GetActionManager();

	// Assumes only one instance of the event graph, this may change in future.
	const FUIAction* UIAction = ProfilerCommandList->GetActionForCommand( ProfilerCommands.EventGraph_SelectAllFrames );

	if( !UIAction )
	{
		ProfilerCommandList->MapAction( ProfilerCommands.EventGraph_SelectAllFrames, SelectAllFrames_Custom() );
	}
	else
	{
		// Replace with the new UI Action.
		const FUIAction NewUIAction = SelectAllFrames_Custom();
		new((void*)UIAction) FUIAction(NewUIAction);
	}
}

/*-----------------------------------------------------------------------------
		Settings
-----------------------------------------------------------------------------*/

EVisibility SEventGraph::EventGraphViewMode_GetVisibility( const EEventGraphViewModes::Type ViewMode ) const
{
	if( ViewMode == EEventGraphViewModes::FlatInclusiveCoalesced || ViewMode == EEventGraphViewModes::FlatExclusiveCoalesced )
	{
		const EVisibility Vis = FProfilerManager::GetSettings().bShowCoalescedViewModesInEventGraph ? EVisibility::Visible : EVisibility::Collapsed;
		if( Vis == EVisibility::Collapsed )
		{
			// If view mode is not available event graph will switch to the hierarchical view mode.
			SEventGraph* MutableThis = const_cast< SEventGraph* >( this );
			MutableThis->EventGraphViewMode_OnCheckStateChanged( ECheckBoxState::Checked, EEventGraphViewModes::Hierarchical );
		}
		return Vis;
	}
	else
	{
		return EVisibility::Visible;
	}
}

void SEventGraph::ExpandCulledEvents( FEventGraphSamplePtr EventPtr )
{
	// Update not culled children.
	EventPtr->SetBooleanStateForAllChildren<EEventPropertyIndex::bIsCulled>(false);
	EventPtr->SetBooleanStateForAllChildren<EEventPropertyIndex::bNeedNotCulledChildrenUpdate>(true);

	struct FAddExcludedCulledEvents
	{
		FORCEINLINE void operator()( FEventGraphSample* InEventPtr, TArray<FEventGraphSamplePtr>& out_ExpandedCulledEvents )
		{
			out_ExpandedCulledEvents.Add( InEventPtr->AsShared() );
		}
	};
	EventPtr->ExecuteOperationForAllChildren( FAddExcludedCulledEvents(), GetCurrentState()->ExpandedCulledEvents );

	CreateEvents();
	TreeView_Refresh();
}

void SEventGraph::SelectAllFrames_Execute()
{
	SwitchToEventGraphState( 0 );
}

bool SEventGraph::SelectAllFrames_CanExecute() const
{
	return IsEventGraphStatesHistoryValid();
}

void SEventGraph::ProfilerManager_OnViewModeChanged( EProfilerViewMode NewViewMode )
{
// 	if( NewViewMode == EProfilerViewMode::LineIndexBased )
// 	{
// 		FunctionDetailsBox->SetVisibility( EVisibility::Visible );
// 		FunctionDetailsBox->SetEnabled( true );
// 	}
// 	else if( NewViewMode == EProfilerViewMode::ThreadViewTimeBased )
// 	{
// 		FunctionDetailsBox->SetVisibility( EVisibility::Collapsed );
// 		FunctionDetailsBox->SetEnabled( false );
// 	}
}

#undef LOCTEXT_NAMESPACE

/*-----------------------------------------------------------------------------
	EventGraphState
-----------------------------------------------------------------------------*/

#define LOCTEXT_NAMESPACE "FEventGraphState"

FText SEventGraph::FEventGraphState::GetFullDescription() const
{
	FTextBuilder Builder;

	FFormatNamedArguments Args;
	Args.Add(TEXT("FrameStartIndex"), GetEventGraph()->GetFrameStartIndex());
	Args.Add(TEXT("FrameEndIndex"), GetEventGraph()->GetFrameEndIndex());
	Args.Add(TEXT("NumberOfFrames"), GetNumFrames());
	Builder.AppendLineFormat(LOCTEXT("FullDesc", "Event graph with range ({FrameStartIndex},{FrameEndIndex}) contains {NumberOfFrames} frame(s)"), Args);

	Builder.Indent();

	if( IsRooted() )
	{
		Builder.AppendLine(GetRootedDesc());
	}

	if( IsCulled() )
	{
		Builder.AppendLine(GetCullingDesc());
	}

	if( IsFiltered() )
	{
		Builder.AppendLine(GetFilteringDesc());
	}

	return Builder.ToText();
}

FText SEventGraph::FEventGraphState::GetRootedDesc() const
{
	const int32 NumFakeRoots = FakeRoot->GetChildren().Num();

	if (NumFakeRoots == 1)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("StatName"), FText::FromName(FakeRoot->GetChildren()[0]->_StatName));
		return FText::Format(LOCTEXT("RootedDesc_SingleChild", "Rooted: {StatName}"), Args);
	}

	return LOCTEXT("RootedDesc_MultipleChildren", "Rooted: Multiple");
}

FText SEventGraph::FEventGraphState::GetCullingDesc() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("CulledPropertyName"), FText::FromName(CullPropertyName));
	Args.Add(TEXT("EventName"), FText::FromString(CullEventPtr->GetFormattedValue(FEventGraphSample::GetEventPropertyByName(CullPropertyName).Index)));

	return FText::Format(LOCTEXT("CulledDesc", "Culled: {CulledPropertyName} {EventName}"), Args);
}

FText SEventGraph::FEventGraphState::GetFilteringDesc() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("FilterPropertyName"), FText::FromName(FilterPropertyName));
	Args.Add(TEXT("EventName"), FText::FromString(FilterEventPtr->GetFormattedValue(FEventGraphSample::GetEventPropertyByName(FilterPropertyName).Index)));

	return FText::Format(LOCTEXT("FilteredDesc", "Filtered: {FilterPropertyName} {EventName}"), Args);
}

FText SEventGraph::FEventGraphState::GetHistoryDesc() const
{
	FText Result = LOCTEXT("DefaultDesc", "Default state");

	if( HistoryType == EEventHistoryTypes::Rooted )
	{
		Result = GetRootedDesc();
	}
	else if( HistoryType == EEventHistoryTypes::Culled )
	{
		Result = GetCullingDesc();
	}
	else if( HistoryType == EEventHistoryTypes::Filtered )
	{
		Result = GetFilteringDesc();
	}

	return Result;
}

static void CreateOneToOneMapping_EventGraphSample
( 
	const FEventGraphSamplePtr LocalEvent, 
	const FEventGraphSamplePtr SourceEvent, 
	TMap< FEventGraphSamplePtr, FEventGraphSamplePtr >& out_MaximumToAverageMapping,
	TMap< FEventGraphSamplePtr, FEventGraphSamplePtr >& out_AverageToMaximumMapping
)
{
	out_MaximumToAverageMapping.Add( LocalEvent, SourceEvent );
	out_AverageToMaximumMapping.Add( SourceEvent, LocalEvent );

	check( LocalEvent->GetChildren().Num() == SourceEvent->GetChildren().Num() );
	for( int32 Index = 0; Index < LocalEvent->GetChildren().Num(); ++Index )
	{
		CreateOneToOneMapping_EventGraphSample( LocalEvent->GetChildren()[Index], SourceEvent->GetChildren()[Index], out_MaximumToAverageMapping, out_AverageToMaximumMapping );
	}
}

void SEventGraph::FEventGraphState::CreateOneToOneMapping()
{
	CreateOneToOneMapping_EventGraphSample( MaximumEventGraph->GetRoot(), AverageEventGraph->GetRoot(), MaximumToAverageMapping, AverageToMaximumMapping );
}

#undef LOCTEXT_NAMESPACE
