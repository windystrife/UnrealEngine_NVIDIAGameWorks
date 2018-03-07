// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SFiltersAndPresets.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "ProfilerSession.h"
#include "ProfilerManager.h"
#include "Widgets/StatDragDropOp.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SFiltersAndPresets"


struct SFiltersAndPresetsHelper
{
	static const FSlateBrush* GetIconForGroup()
	{
		return FEditorStyle::GetBrush( TEXT( "Profiler.Misc.GenericGroup" ) );
	}

	static const FSlateBrush* GetIconForStatType( const EProfilerSampleTypes::Type StatType )
	{
		const FSlateBrush* HierarchicalTimeIcon = FEditorStyle::GetBrush( TEXT( "Profiler.Type.Hierarchical" ) );
		const FSlateBrush* NumberIntIcon        = FEditorStyle::GetBrush( TEXT( "Profiler.Type.NumberInt" ) );
		const FSlateBrush* NumberFloatIcon      = FEditorStyle::GetBrush( TEXT( "Profiler.Type.NumberFloat" ) );
		const FSlateBrush* MemoryIcon           = FEditorStyle::GetBrush( TEXT( "Profiler.Type.Memory" ) );

		const FSlateBrush* const StatIcons[ EProfilerSampleTypes::InvalidOrMax ] =
		{
			HierarchicalTimeIcon,
			NumberIntIcon,
			NumberFloatIcon,
			MemoryIcon
		};

		return StatIcons[ (int32)StatType ];
	}
};

/*-----------------------------------------------------------------------------
	Filter and presets tooltip
-----------------------------------------------------------------------------*/

class SFiltersAndPresetsTooltip
{
	const uint32 StatID;
	TSharedPtr<FProfilerSession> ProfilerSession;

public:
	SFiltersAndPresetsTooltip( const uint32 InStatID )
		: StatID( InStatID )
	{
		ProfilerSession = FProfilerManager::Get()->GetProfilerSession();
	}

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	TSharedRef<SToolTip> GetTooltip()
	{
		if( ProfilerSession.IsValid() )
		{
			const TSharedRef<SGridPanel> ToolTipGrid = SNew(SGridPanel);
			int CurrentRowPos = 0;

			AddHeader( ToolTipGrid, CurrentRowPos );
			AddDescription( ToolTipGrid, CurrentRowPos );

			const FProfilerAggregatedStat* AggregatedPtr = ProfilerSession->GetAggregatedStat( StatID );
			if( AggregatedPtr )
			{
				AddValuesInformation( ToolTipGrid, CurrentRowPos, *AggregatedPtr );
				AddCallsInformation( ToolTipGrid, CurrentRowPos, *AggregatedPtr );
			}
			else
			{
				AddNoDataInformation( ToolTipGrid, CurrentRowPos );
			}

			return SNew(SToolTip)
			[
				ToolTipGrid
			];
		}
		else
		{
			return SNew(SToolTip)
				.Text( LOCTEXT("NotImplemented","Tooltip for multiple profiler instances has not been implemented yet") );
		}
	}

protected:

	void AddNoDataInformation( const TSharedRef<SGridPanel>& Grid, int32& RowPos )
	{
		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		.ColumnSpan( 3 )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
			.Text( LOCTEXT("NoStatData","There is no data for this stat") )
		];
		RowPos++;
	}

	void AddHeader( const TSharedRef<SGridPanel>& Grid, int32& RowPos )
	{
		const FString InstanceName = ProfilerSession->GetSessionType() == EProfilerSessionTypes::StatsFile ? FPaths::GetBaseFilename( ProfilerSession->GetName() ) : ProfilerSession->GetName();

		Grid->AddSlot( 0, RowPos++ )
		.Padding( 2.0f )
		.ColumnSpan( 3 )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
			.Text( LOCTEXT("StatInstance","Stat information for profiler instance") )
		];

		Grid->AddSlot( 0, RowPos++ )
		.Padding( 2.0f )
		.ColumnSpan( 3 )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text( FText::FromString(InstanceName) )
		];

		AddSeparator( Grid, RowPos );
	}

	void AddDescription( const TSharedRef<SGridPanel>& Grid, int32& RowPos )
	{
		const FProfilerStat& ProfilerStat = ProfilerSession->GetMetaData()->GetStatByID( StatID );
		const EProfilerSampleTypes::Type SampleType = ProfilerSession->GetMetaData()->GetSampleTypeForStatID( StatID );
		const FSlateBrush* const StatIcon = SFiltersAndPresetsHelper::GetIconForStatType( SampleType );

		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
			.Text( LOCTEXT("GroupDesc","Group:") )
		];

		Grid->AddSlot( 1, RowPos )
		.Padding( 2.0f )
		.ColumnSpan( 2 )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text( FText::FromName(ProfilerStat.OwningGroup().Name() ))
		];
		RowPos++;

		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
			.Text( LOCTEXT("NameDesc","Name:") )
		];

		Grid->AddSlot( 1, RowPos )
		.Padding( 2.0f )
		.ColumnSpan( 2 )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text( FText::FromName(ProfilerStat.Name()) )
		];
		RowPos++;

		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
			.Text( LOCTEXT("TypeDesc","Type:") )
		];

		Grid->AddSlot( 1, RowPos )
		.Padding( 2.0f )
		.ColumnSpan( 2 )
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SImage )
				.Image( StatIcon )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( FText::FromString(EProfilerSampleTypes::ToDescription(SampleType)) )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			]
		];
		RowPos++;

		AddSeparator( Grid, RowPos );
	}

	void AddValuesInformation( const TSharedRef<SGridPanel>& Grid, int32& RowPos, const FProfilerAggregatedStat& Aggregated )
	{
		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		.ColumnSpan( 3 )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
			.Text( LOCTEXT("ValueDesc","Value") )
		];
		RowPos++;

		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text(FText::Format(LOCTEXT("MinDesc", "Min: {0}"), FText::FromString(Aggregated.GetFormattedValue(FProfilerAggregatedStat::EMinValue))))
		];

		Grid->AddSlot( 1, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text(FText::Format(LOCTEXT("AvgDesc", "Avg: {0}"), FText::FromString(Aggregated.GetFormattedValue(FProfilerAggregatedStat::EAvgValue))))
		];

		Grid->AddSlot( 2, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text(FText::Format(LOCTEXT("MaxDesc", "Max: {0}"), FText::FromString(Aggregated.GetFormattedValue(FProfilerAggregatedStat::EMaxValue))))
		];
		RowPos++;

		AddSeparator( Grid, RowPos );
	}

	void AddCallsInformation( const TSharedRef<SGridPanel>& Grid, int32& RowPos, const FProfilerAggregatedStat& Aggregated )
	{
		if( !Aggregated.HasCalls() )
		{
			return;
		}

		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		.ColumnSpan( 3 )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
			.Text(FText::Format(LOCTEXT("CallsFramesPctDesc", "Calls Frames with call: {0}"), FText::FromString(Aggregated.GetFormattedValue(FProfilerAggregatedStat::EFramesWithCallPct))))
		];
		RowPos++;

		Grid->AddSlot( 0, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text(FText::Format(LOCTEXT("MinDesc", "Min: {0}"), FText::FromString(Aggregated.GetFormattedValue(FProfilerAggregatedStat::EMinNumCalls))))
		];

		Grid->AddSlot( 1, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text(FText::Format(LOCTEXT("AvgDesc", "Avg: {0}"), FText::FromString(Aggregated.GetFormattedValue(FProfilerAggregatedStat::EAvgNumCalls))))
		];

		Grid->AddSlot( 2, RowPos )
		.Padding( 2.0f )
		[
			SNew(STextBlock)
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
			.Text(FText::Format(LOCTEXT("MaxDesc", "Max: {0}"), FText::FromString(Aggregated.GetFormattedValue(FProfilerAggregatedStat::EMaxNumCalls))))
		];
		RowPos++;

		AddSeparator( Grid, RowPos );
	}

	void AddSeparator( const TSharedRef<SGridPanel>& Grid, int32& RowPos )
	{
		Grid->AddSlot( 0, RowPos++ )
		.Padding( 2.0f )
		.ColumnSpan( 3 )
		[
			SNew(SSeparator)
			.Orientation( Orient_Horizontal )
		];
	}

	END_SLATE_FUNCTION_BUILD_OPTIMIZATION
};

/*-----------------------------------------------------------------------------
	EStatGroupingOrSortingMode
-----------------------------------------------------------------------------*/

FText EStatGroupingOrSortingMode::ToName( const Type StatGroupingOrSortingMode )
{
	switch( StatGroupingOrSortingMode )
	{
	case GroupName: return LOCTEXT("GroupingOrSorting_Name_GroupName", "Group Name");
	case StatName: return LOCTEXT("GroupingOrSorting_Name_StatName", "Stat Name");
	case StatType: return LOCTEXT("GroupingOrSorting_Name_StatType", "Stat Type");
	case StatValue: return LOCTEXT("GroupingOrSorting_Name_StatValue", "Stat Value");

	default: return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

FText EStatGroupingOrSortingMode::ToDescription(const Type StatGroupingOrSortingMode)
{
	switch( StatGroupingOrSortingMode )
	{
	case GroupName: return LOCTEXT("GroupingOrSorting_Desc_GroupName", "Creates groups based on stat metadata groups");
	case StatName: return LOCTEXT("GroupingOrSorting_Desc_StatName", "Creates one group for one letter");
	case StatType: return LOCTEXT("GroupingOrSorting_Desc_StatType", "Creates one group for each stat type");
	case StatValue: return LOCTEXT("GroupingOrSorting_Desc_StatValue", "Creates one group for each logarithmic range ie. 0.001 - 0.01, 0.01 - 0.1, 0.1 - 1.0, 1.0 - 10.0 etc");

	default: return LOCTEXT("InvalidOrMax", "InvalidOrMax");
	}
}

FName EStatGroupingOrSortingMode::ToBrushName( const Type StatGroupingOrSortingMode )
{
	switch( StatGroupingOrSortingMode )
	{
	case GroupName: return TEXT("Profiler.FiltersAndPresets.GroupNameIcon");
	case StatName: return TEXT("Profiler.FiltersAndPresets.StatNameIcon");
	case StatType: return TEXT("Profiler.FiltersAndPresets.StatTypeIcon");
	case StatValue: return TEXT("Profiler.FiltersAndPresets.StatValueIcon");

	default: return NAME_None;
	}
}


/*-----------------------------------------------------------------------------
	SGroupAndStatTableRow
-----------------------------------------------------------------------------*/

DECLARE_DELEGATE_RetVal_OneParam( bool, FShouldBeEnabledDelegate, const uint32 /*StatID*/ );

/** Widget that represents a table row in the groups and stats' tree control.  Generates widgets for each column on demand. */
class SGroupAndStatTableRow : public STableRow< FGroupOrStatNodePtr >
{
public:	
	SLATE_BEGIN_ARGS( SGroupAndStatTableRow )
		{}
		/** Text to be highlighted. */
		SLATE_ATTRIBUTE( FText, HighlightText )

		SLATE_EVENT( FShouldBeEnabledDelegate, OnShouldBeEnabled )
	SLATE_END_ARGS()

public:
	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs	          - Declaration used by the SNew() macro to construct this widget.
	 * @param   InOwnerTableView  - The owner table into which this row is being placed.
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<FGroupOrStatNode>& InGroupOrStatNode )
	{
		GroupOrStatNode = InGroupOrStatNode;
		OnShouldBeEnabled = InArgs._OnShouldBeEnabled;

		SetEnabled( TAttribute<bool>( this, &SGroupAndStatTableRow::HandleShouldBeEnabled ) );

		const FSlateBrush* const IconForGroupOrStat = InGroupOrStatNode->IsGroup() ? SFiltersAndPresetsHelper::GetIconForGroup() : SFiltersAndPresetsHelper::GetIconForStatType( InGroupOrStatNode->GetStatType() );
		const TSharedRef<SToolTip> Tooltip = InGroupOrStatNode->IsGroup() ? SNew(SToolTip) : SFiltersAndPresetsTooltip(InGroupOrStatNode->GetStatID()).GetTooltip();

		ChildSlot
		[
			SNew(SHorizontalBox)

			// Expander arrow.
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew( SExpanderArrow, SharedThis(this) )
			]

			// Icon to visualize group or stat type.
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( 0.0f, 0.0f, 8.0f, 0.0f )
			[
				SNew( SImage )
				.Image(	IconForGroupOrStat )
				.ToolTip( Tooltip )
			]

			// Description text.
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign( VAlign_Center )
			.HAlign( HAlign_Left )
			.Padding( FMargin( 2.0f, 0.0f ) )
			[
				SNew( STextBlock )
				.Text( this, &SGroupAndStatTableRow::GetText )
				.HighlightText( InArgs._HighlightText )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
				.ColorAndOpacity( this, &SGroupAndStatTableRow::GetColorAndOpacity )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Top )
			.Padding( 0.0f, 1.0f, 0.0f, 0.0f )
			[
				SNew( SImage )
				.Visibility( !InGroupOrStatNode->IsGroup() ? EVisibility::Visible : EVisibility::Collapsed )
				.Image( FEditorStyle::GetBrush("Profiler.Tooltip.HintIcon10") )
				.ToolTip( Tooltip )
			]
		];

		STableRow< FGroupOrStatNodePtr >::ConstructInternal(
			STableRow::FArguments()
				.ShowSelection(true),
			InOwnerTableView
		);
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
		if( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ))
		{
			if( GroupOrStatNode->IsGroup() )
			{
				// Add all stat IDs for the group.
				TArray<int32> StatIDs;
				const TArray<FGroupOrStatNodePtr>& FilteredChildren = GroupOrStatNode->GetFilteredChildren();
				const int32 NumFilteredChildren = FilteredChildren.Num();

				StatIDs.Reserve( NumFilteredChildren );
				for( int32 Nx = 0; Nx < NumFilteredChildren; ++Nx )
				{
					StatIDs.Add( FilteredChildren[Nx]->GetStatID() );
				}

				return FReply::Handled().BeginDragDrop( FStatIDDragDropOp::NewGroup( StatIDs, GroupOrStatNode->GetName().GetPlainNameString() ) );
			}
			else
			{
				return FReply::Handled().BeginDragDrop( FStatIDDragDropOp::NewSingle( GroupOrStatNode->GetStatID(), GroupOrStatNode->GetName().GetPlainNameString() ) );
			}
		}

		return STableRow< FGroupOrStatNodePtr >::OnDragDetected(MyGeometry,MouseEvent);
	}

protected:
	/**
	 * @return a text which describes this table row, refers to both groups and stats
	 */
	FText GetText() const
	{
		FText Text = FText::GetEmpty();

		if( GroupOrStatNode->IsGroup() )
		{
			int32 NumDisplayedStats = 0;

			const TArray<FGroupOrStatNodePtr>& Children = GroupOrStatNode->GetChildren();
			const int32 NumChildren = Children.Num();

			for( int32 Nx = 0; Nx < NumChildren; ++Nx )
			{
				const bool bIsStatTracked = FProfilerManager::Get()->IsStatTracked( Children[Nx]->GetStatID() ); 
				if( bIsStatTracked )
				{
					NumDisplayedStats ++;
				}
			}

			Text = FText::Format(LOCTEXT("GroupAndStat_GroupNodeTextFmt", "{0} ({1}) ({2})"), FText::FromName(GroupOrStatNode->GetName()), FText::AsNumber(GroupOrStatNode->GetChildren().Num()), FText::AsNumber(NumDisplayedStats));
		}
		else
		{
			const bool bIsStatTracked = FProfilerManager::Get()->IsStatTracked( GroupOrStatNode->GetStatID() );
			if(bIsStatTracked)
			{
				Text = FText::Format(LOCTEXT("GroupAndStat_GroupNodeTrackedTextFmt", "{0}*"), FText::FromName(GroupOrStatNode->GetName()));
			}
			else
			{
				Text = FText::FromName(GroupOrStatNode->GetName());
			}
		}

		return Text;
	}

	/**
	 * @return a color and opacity value used to draw this table row, refers to both groups and stats
	 */
	FSlateColor GetColorAndOpacity() const
	{
		const bool bIsStatTracked = FProfilerManager::Get()->IsStatTracked( GroupOrStatNode->GetStatID() ); 
		const FSlateColor Color = bIsStatTracked ? FProfilerManager::Get()->GetColorForStatID( GroupOrStatNode->GetStatID() ) : FLinearColor::White;
		return Color;

		//FProfilerManager::GetSettings().GetColorForStat( FName StatName )
	}

	/**
	 * @return a font style which is used to draw this table row, refers to both groups and stats
	 */
	FSlateFontInfo GetFont() const
	{
		const bool bIsStatTracked = FProfilerManager::Get()->IsStatTracked( GroupOrStatNode->GetStatID() ); 
		const FSlateFontInfo FontInfo = bIsStatTracked ? FEditorStyle::GetFontStyle("BoldFont") : FEditorStyle::GetFontStyle("NormalFont");
		return FontInfo;
	}

	bool HandleShouldBeEnabled() const
	{
		bool bResult = false;
		
		if( GroupOrStatNode->IsGroup() )
		{
			bResult = true;
		}
		else
		{
			if( OnShouldBeEnabled.IsBound() )
			{
				bResult = OnShouldBeEnabled.Execute( GroupOrStatNode->GetStatID() );
			}
		}

		return bResult;
	}

	/** The tree item associated with this row of data. */
	FGroupOrStatNodePtr GroupOrStatNode;

	FShouldBeEnabledDelegate OnShouldBeEnabled;
};

/*-----------------------------------------------------------------------------
	SFiltersAndPresets
-----------------------------------------------------------------------------*/

SFiltersAndPresets::SFiltersAndPresets()
	: GroupingMode( EStatGroupingOrSortingMode::GroupName )
	, SortingMode( EStatGroupingOrSortingMode::StatName )
	, bExpansionSaved( false )
{
	FMemory::Memset( bStatTypeIsVisible, 1 );
}

SFiltersAndPresets::~SFiltersAndPresets()
{
	// Remove ourselves from the profiler manager.
	if( FProfilerManager::Get().IsValid() )
	{
		FProfilerManager::Get()->OnRequestFilterAndPresetsUpdate().RemoveAll( this );
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SFiltersAndPresets::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// Search box
		+SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding( 2.0f )
			[
				SNew(SVerticalBox)

				// Search box
				+SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 2.0f )
				.AutoHeight()
				[
					SAssignNew( GroupAndStatSearchBox, SSearchBox )
					.HintText( LOCTEXT("SearchBoxHint", "Search stats or groups") )
					.OnTextChanged( this, &SFiltersAndPresets::SearchBox_OnTextChanged )
					.IsEnabled( this, &SFiltersAndPresets::SearchBox_IsEnabled )
					.ToolTipText( LOCTEXT("FilterSearchHint", "Type here to search stats or group") )
				]

				// Group by and Sort By
				+SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 2.0f )
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.FillWidth( 1.0f )
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("GroupByText", "Group by") )
					]

					+SHorizontalBox::Slot()
					.FillWidth( 2.0f )
					.VAlign(VAlign_Center)
					[
						SAssignNew( GroupByComboBox, SComboBox< TSharedPtr<EStatGroupingOrSortingMode::Type> > )
						.ToolTipText( this, &SFiltersAndPresets::GroupBy_GetSelectedTooltipText )
						.OptionsSource( &GroupByOptionsSource )
						.OnSelectionChanged( this, &SFiltersAndPresets::GroupBy_OnSelectionChanged )
						.OnGenerateWidget( this, &SFiltersAndPresets::GroupBy_OnGenerateWidget )
						[
							SNew( STextBlock )
							.Text( this, &SFiltersAndPresets::GroupBy_GetSelectedText )
						]
					]
				]

				// Sort by
				+SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 2.0f )
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.FillWidth( 1.0f )
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("SortByText", "Sort by") )
					]

					+SHorizontalBox::Slot()
					.FillWidth( 2.0f )
					.VAlign(VAlign_Center)
					[
						SAssignNew( SortByComboBox, SComboBox< TSharedPtr<EStatGroupingOrSortingMode::Type> > )
						.OptionsSource( &SortByOptionsSource )
						.OnSelectionChanged( this, &SFiltersAndPresets::SortBy_OnSelectionChanged )
						.OnGenerateWidget( this, &SFiltersAndPresets::SortBy_OnGenerateWidget )
						[
							SNew( STextBlock )
							.Text( this, &SFiltersAndPresets::SortBy_GetSelectedText )
						]
					]
				]

				// Check boxes for: HierarchicalTime NumberFloat, NumberInt, Memory
				+SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 2.0f )
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.Padding( FMargin(0.0f,0.0f,1.0f,0.0f) )
					.FillWidth( 1.0f )
					[
						GetToggleButtonForStatType( EProfilerSampleTypes::HierarchicalTime )
					]

					+SHorizontalBox::Slot()
					.Padding( FMargin(1.0f,0.0f,1.0f,0.0f) )
					.FillWidth( 1.0f )
					[
						GetToggleButtonForStatType( EProfilerSampleTypes::NumberFloat )
					]

					+SHorizontalBox::Slot()
					.Padding( FMargin(1.0f,0.0f,1.0f,0.0f) )
					.FillWidth( 1.0f )
					[
						GetToggleButtonForStatType( EProfilerSampleTypes::NumberInt )
					]

					+SHorizontalBox::Slot()
					.Padding( FMargin(1.0f,0.0f,0.0f,0.0f) )
					.FillWidth( 1.0f )
					[
						GetToggleButtonForStatType( EProfilerSampleTypes::Memory )
					]
				]
			]
		]	

		// Stat groups tree
		+SVerticalBox::Slot()
		.FillHeight( 1.0f )
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			.Padding( 2.0f )
			[
				SAssignNew( GroupAndStatTree, STreeView< FGroupOrStatNodePtr > )
				.SelectionMode(ESelectionMode::Single)
				.TreeItemsSource( &FilteredGroupNodes )
				.OnGetChildren( this, &SFiltersAndPresets::GroupAndStatTree_OnGetChildren )
				.OnGenerateRow( this, &SFiltersAndPresets::GroupAndStatTree_OnGenerateRow )
				.OnMouseButtonDoubleClick( this, &SFiltersAndPresets::GroupAndStatTree_OnMouseButtonDoubleClick )
				//.OnSelectionChanged( this, &SFiltersAndPresets::GroupAndStatTree_OnSelectionChanged )
				.ItemHeight( 12 )
			]
		]
	];

	// Register ourselves with the profiler manager.
	FProfilerManager::Get()->OnRequestFilterAndPresetsUpdate().AddSP( this, &SFiltersAndPresets::ProfilerManager_OnRequestFilterAndPresetsUpdate );

	// Create the search filters: text based, stat type based etc.
	// @TODO: HandleItemToStringArray should be moved somewhere else
	GroupAndStatTextFilter = MakeShareable( new FGroupAndStatTextFilter( FGroupAndStatTextFilter::FItemToStringArray::CreateSP( this, &SFiltersAndPresets::HandleItemToStringArray ) ) );
	GroupAndStatFilters = MakeShareable( new FGroupAndStatFilterCollection() );
	GroupAndStatFilters->Add( GroupAndStatTextFilter );
	
	CreateGroupByOptionsSources();
	RecreateSortByOptionsSources();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


void SFiltersAndPresets::ProfilerManager_OnRequestFilterAndPresetsUpdate()
{
	TSharedPtr<FProfilerSession> ProfilerSessionLocal = FProfilerManager::Get()->GetProfilerSession();
	UpdateGroupAndStatTree( ProfilerSessionLocal );
}

void SFiltersAndPresets::UpdateGroupAndStatTree( const TSharedPtr<FProfilerSession> InProfilerSession )
{
	const bool bRebuild = InProfilerSession != ProfilerSession;
	if( bRebuild )
	{
		StatNodesMap.Empty( StatNodesMap.Num() );
	}

	ProfilerSession = InProfilerSession;

	if( ProfilerSession.IsValid() )
	{
		const TSharedRef<FProfilerStatMetaData> StatMetaData = ProfilerSession->GetMetaData();

		// Create all stat nodes.
		for( auto It = StatMetaData->GetStatIterator(); It; ++It )
		{
			const FProfilerStat& ProfilerStat = *It.Value();
			const FName StatName = ProfilerStat.Name();

			FGroupOrStatNodePtr* StatPtr = StatNodesMap.Find( StatName );
			if( !StatPtr )
			{
				StatPtr = &StatNodesMap.Add( StatName, MakeShareable( new FGroupOrStatNode( ProfilerStat.OwningGroup().Name(), StatName, ProfilerStat.ID(), ProfilerStat.Type() ) ) );
			}
			// Update stat value ?
		}
	}

	// Create groups, sort stats within the group and apply filtering.
	CreateGroups();
	SortStats();
	ApplyFiltering();
}

void SFiltersAndPresets::CreateGroups()
{
	// Creates groups based on stat metadata groups.
	TMap< FName, FGroupOrStatNodePtr > GroupNodeSet;
	if( GroupingMode == EStatGroupingOrSortingMode::GroupName )
	{
		for( auto It = StatNodesMap.CreateIterator(); It; ++It )
		{
			const FGroupOrStatNodePtr& StatNodePtr = It.Value();
			const FName GroupName = StatNodePtr->GetMetaGropName();

			FGroupOrStatNodePtr* GroupPtr = GroupNodeSet.Find( GroupName );
			if( !GroupPtr )
			{
				GroupPtr = &GroupNodeSet.Add( GroupName, MakeShareable(new FGroupOrStatNode( GroupName )) );
			}

			(*GroupPtr)->AddChildAndSetGroupPtr( StatNodePtr );
		}
	}
	// Creates one group for each stat type.
	else if( GroupingMode == EStatGroupingOrSortingMode::StatType )
	{
		for( auto It = StatNodesMap.CreateIterator(); It; ++It )
		{
			const FGroupOrStatNodePtr& StatNodePtr = It.Value();
			const FName GroupName = *EProfilerSampleTypes::ToName( StatNodePtr->GetStatType() );

			FGroupOrStatNodePtr* GroupPtr = GroupNodeSet.Find( GroupName );
			if( !GroupPtr )
			{
				GroupPtr = &GroupNodeSet.Add( GroupName, MakeShareable(new FGroupOrStatNode( GroupName )) );
			}

			(*GroupPtr)->AddChildAndSetGroupPtr( StatNodePtr );
		}
	}
	// Creates one group for each logarithmic range ie. 0.001 - 0.01, 0.01 - 0.1, 0.1 - 1.0, 1.0 - 10.0 etc.
	else if( GroupingMode == EStatGroupingOrSortingMode::StatValue )
	{
		// TODO:
	}
	// Creates one group for one letter.
	else if( GroupingMode == EStatGroupingOrSortingMode::StatName )
	{
		for( auto It = StatNodesMap.CreateIterator(); It; ++It )
		{
			const FGroupOrStatNodePtr& StatNodePtr = It.Value();
			const FName GroupName = *StatNodePtr->GetName().GetPlainNameString().Left(1);

			FGroupOrStatNodePtr* GroupPtr = GroupNodeSet.Find( GroupName );
			if( !GroupPtr )
			{
				GroupPtr = &GroupNodeSet.Add( GroupName, MakeShareable(new FGroupOrStatNode( GroupName )) );
			}

			(*GroupPtr)->AddChildAndSetGroupPtr( StatNodePtr );
		}
	}

	GroupNodeSet.GenerateValueArray( GroupNodes );
	// Sort by a fake group name.
	GroupNodes.Sort( FGroupAndStatSorting::ByStatName() );
}

void SFiltersAndPresets::SortStats()
{
	const int32 NumGroups = GroupNodes.Num();

	// Sorts stats inside group by a group name.
	if( SortingMode == EStatGroupingOrSortingMode::GroupName )
	{
		for( int32 ID = 0; ID < NumGroups; ++ID )
		{
			GroupNodes[ID]->SortChildren( FGroupAndStatSorting::ByGroupName() );
		}
	}
	// Sorts stats inside group by a stat type.
	else if( SortingMode == EStatGroupingOrSortingMode::StatType )
	{
		for( int32 ID = 0; ID < NumGroups; ++ID )
		{
			GroupNodes[ID]->SortChildren( FGroupAndStatSorting::ByStatType() );
		}
	}
	// Sorts stats inside group by a stat value.
	else if( SortingMode == EStatGroupingOrSortingMode::StatValue )
	{
	}
	// Sorts stats inside group by a stat name.
	else if( SortingMode == EStatGroupingOrSortingMode::StatName )
	{
		for( int32 ID = 0; ID < NumGroups; ++ID )
		{
			GroupNodes[ID]->SortChildren( FGroupAndStatSorting::ByStatName() );
		}
	}
}

void SFiltersAndPresets::ApplyFiltering()
{
	FilteredGroupNodes.Reset();

	// Apply filter to all groups and its children.
	const int32 NumGroups = GroupNodes.Num();
	for( int32 ID = 0; ID < NumGroups; ++ID )
	{
		FGroupOrStatNodePtr& GroupPtr = GroupNodes[ID];
		GroupPtr->ClearFilteredChildren();
		const bool bIsGroupVisible = GroupAndStatFilters->PassesAllFilters( GroupPtr );

		const TArray<FGroupOrStatNodePtr>& GroupChildren = GroupPtr->GetChildren();
		const int32 NumChildren = GroupChildren.Num();
		int32 NumVisibleChildren = 0;
		for( int32 Cx = 0; Cx < NumChildren; ++Cx )
		{
			// Add a child.
			const FGroupOrStatNodePtr& StatPtr = GroupChildren[Cx];
			const bool bIsChildVisible = GroupAndStatFilters->PassesAllFilters( StatPtr ) && bStatTypeIsVisible[StatPtr->GetStatType()];
			if( bIsChildVisible )
			{
				GroupPtr->AddFilteredChild( StatPtr );
			}

			NumVisibleChildren += bIsChildVisible ? 1 : 0;
		}

		if( bIsGroupVisible || NumVisibleChildren>0 )
		{
			// Add a group.
			FilteredGroupNodes.Add( GroupPtr );
			GroupPtr->bForceExpandGroupNode = true;
		}
		else
		{
			GroupPtr->bForceExpandGroupNode = false;
		}
	}

	// Only expand group and stat nodes if we have a text filter.
	const bool bNonEmptyTextFilter = !GroupAndStatTextFilter->GetRawFilterText().IsEmpty();
	if( bNonEmptyTextFilter )
	{
		if( !bExpansionSaved )
		{
			ExpandedNodes.Empty();
			GroupAndStatTree->GetExpandedItems( ExpandedNodes );
			bExpansionSaved = true;
		}

		for( int32 Fx = 0; Fx < FilteredGroupNodes.Num(); Fx++ )
		{
			const FGroupOrStatNodePtr& GroupPtr = FilteredGroupNodes[Fx];
			GroupAndStatTree->SetItemExpansion( GroupPtr, GroupPtr->bForceExpandGroupNode );
		}
	}
	else
	{
		if( bExpansionSaved )
		{
			// Restore previously expanded nodes when the text filter is disabled.
			GroupAndStatTree->ClearExpandedItems();
			for( auto It = ExpandedNodes.CreateConstIterator(); It; ++It )
			{
				GroupAndStatTree->SetItemExpansion( *It, true );
			}
			bExpansionSaved = false;
		}
	}

	// Request tree refresh
	GroupAndStatTree->RequestTreeRefresh();
}

void SFiltersAndPresets::HandleItemToStringArray( const FGroupOrStatNodePtr& GroupOrStatNodePtr, TArray< FString >& out_SearchStrings ) const
{
	// Add group or stat name.
	out_SearchStrings.Add( GroupOrStatNodePtr->GetName().GetPlainNameString() );
}

void SFiltersAndPresets::CreateGroupByOptionsSources()
{
	GroupByOptionsSource.Reset( 4 );

	// Must be added in order of elements in  the EStatGroupingOrSortingMode.
	GroupByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::GroupName ) ) );
	GroupByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::StatName ) ) );
	GroupByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::StatType ) ) );
	//GroupByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::StatValue ) ) );

	GroupByComboBox->SetSelectedItem( GroupByOptionsSource[EStatGroupingOrSortingMode::GroupName] );
	GroupByComboBox->RefreshOptions();

}

void SFiltersAndPresets::RecreateSortByOptionsSources()
{
	SortByOptionsSource.Reset( 4 );

	// Must be added in order of elements in  the EStatGroupingOrSortingMode.
	SortByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::GroupName ) ) );
	SortByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::StatName ) ) );
	SortByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::StatType ) ) );
	//SortByOptionsSource.Add( MakeShareable( new EStatGroupingOrSortingMode::Type( EStatGroupingOrSortingMode::StatValue ) ) );

	// @TODO: Remove useless stat sorting ie: when grouped by group name, sorting by group name doesn't change anything.
	
	// Select default sorting mode based on the grouping mode.
	if( GroupingMode == EStatGroupingOrSortingMode::GroupName )
	{
		SortingMode = EStatGroupingOrSortingMode::StatName;
		SortByComboBox->SetSelectedItem( SortByOptionsSource[SortingMode] );
		SortByOptionsSource.RemoveAtSwap( (int32)GroupingMode );
	}
	else if( GroupingMode == EStatGroupingOrSortingMode::StatName )
	{
		SortingMode = EStatGroupingOrSortingMode::StatName;
		SortByComboBox->SetSelectedItem( SortByOptionsSource[SortingMode] );
	}
	else if( GroupingMode == EStatGroupingOrSortingMode::StatType )
	{
		SortingMode = EStatGroupingOrSortingMode::StatName;
		SortByComboBox->SetSelectedItem( SortByOptionsSource[SortingMode] );
	}
	else if( GroupingMode == EStatGroupingOrSortingMode::StatValue )
	{
		// TODO:
	}

	SortByComboBox->RefreshOptions();
}

TSharedRef<SWidget> SFiltersAndPresets::GetToggleButtonForStatType( const EProfilerSampleTypes::Type StatType )
{
	return SNew( SCheckBox )
		.Style( FEditorStyle::Get(), "ToggleButtonCheckbox" )
		.HAlign( HAlign_Center )
		.Padding( 2.0f )
		.OnCheckStateChanged( this, &SFiltersAndPresets::FilterByStatType_OnCheckStateChanged, StatType )
		.IsChecked( this, &SFiltersAndPresets::FilterByStatType_IsChecked, StatType )
		.ToolTipText( FText::FromString(EProfilerSampleTypes::ToDescription( StatType )) )
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign( VAlign_Center )
				[
					SNew(SImage)
						.Image( SFiltersAndPresetsHelper::GetIconForStatType( StatType ) )
				]

			+SHorizontalBox::Slot()
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				.VAlign( VAlign_Center )
				[
					SNew( STextBlock )
						.Text( FText::FromString(EProfilerSampleTypes::ToName( StatType )) )
						.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Caption") )
				]
		];
}

void SFiltersAndPresets::FilterByStatType_OnCheckStateChanged( ECheckBoxState NewRadioState, const EProfilerSampleTypes::Type InStatType )
{
	bStatTypeIsVisible[InStatType] = NewRadioState == ECheckBoxState::Checked;
	ApplyFiltering();
}

ECheckBoxState SFiltersAndPresets::FilterByStatType_IsChecked( const EProfilerSampleTypes::Type InStatType ) const
{
	return bStatTypeIsVisible[InStatType] ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

/*-----------------------------------------------------------------------------
	GroupAndStatTree
-----------------------------------------------------------------------------*/

TSharedRef< ITableRow > SFiltersAndPresets::GroupAndStatTree_OnGenerateRow( FGroupOrStatNodePtr GroupOrStatNode, const TSharedRef< STableViewBase >& OwnerTable )
{
	TSharedRef< ITableRow > TableRow = 
		SNew(SGroupAndStatTableRow, OwnerTable, GroupOrStatNode.ToSharedRef())
		.OnShouldBeEnabled( this, &SFiltersAndPresets::GroupAndStatTableRow_ShouldBeEnabled )
		.HighlightText( this, &SFiltersAndPresets::GroupAndStatTableRow_GetHighlightText );
	
	return TableRow;
}

void SFiltersAndPresets::GroupAndStatTree_OnGetChildren( FGroupOrStatNodePtr InParent, TArray< FGroupOrStatNodePtr >& out_Children )
{
	out_Children = InParent->GetFilteredChildren();
}

void SFiltersAndPresets::GroupAndStatTree_OnMouseButtonDoubleClick( FGroupOrStatNodePtr GroupOrStatNode )
{
	// @TODO: Add mechanism which will synchronize displayed stats between: GraphPanel, FilterAndPresets and ProfilerManager
	if( !GroupOrStatNode->IsGroup() )
	{
		const bool bIsStatTracked = FProfilerManager::Get()->IsStatTracked( GroupOrStatNode->GetStatID() );
		if( !bIsStatTracked )
		{
			// Add a new graph.
			FProfilerManager::Get()->TrackStat( GroupOrStatNode->GetStatID() );	
		}
		else
		{
			// Remove a graph
			FProfilerManager::Get()->UntrackStat( GroupOrStatNode->GetStatID() );	
		}
	}
	else
	{
		const bool bIsGroupExpanded = GroupAndStatTree->IsItemExpanded( GroupOrStatNode );
		GroupAndStatTree->SetItemExpansion( GroupOrStatNode, !bIsGroupExpanded );
	}
}

FText SFiltersAndPresets::GroupAndStatTableRow_GetHighlightText() const
{
	return GroupAndStatSearchBox->GetText();
}

bool SFiltersAndPresets::GroupAndStatTableRow_ShouldBeEnabled( const uint32 InStatID ) const
{
	return ProfilerSession->GetAggregatedStat( InStatID ) != nullptr;
}

/*-----------------------------------------------------------------------------
	SearchBox
-----------------------------------------------------------------------------*/

void SFiltersAndPresets::SearchBox_OnTextChanged( const FText& InFilterText )
{
	GroupAndStatTextFilter->SetRawFilterText( InFilterText );
	GroupAndStatSearchBox->SetError( GroupAndStatTextFilter->GetFilterErrorText() );
	ApplyFiltering();
}

bool SFiltersAndPresets::SearchBox_IsEnabled() const
{
	return StatNodesMap.Num() > 0;
}

/*-----------------------------------------------------------------------------
	GroupBy
-----------------------------------------------------------------------------*/

void SFiltersAndPresets::GroupBy_OnSelectionChanged( TSharedPtr<EStatGroupingOrSortingMode::Type> NewGroupingMode, ESelectInfo::Type SelectInfo )
{
	if( SelectInfo != ESelectInfo::Direct )
	{
		GroupingMode = *NewGroupingMode;

		// Create groups, sort stats within the group and apply filtering.
		CreateGroups();
		SortStats();
		ApplyFiltering();
		RecreateSortByOptionsSources();
	}
}

TSharedRef<SWidget> SFiltersAndPresets::GroupBy_OnGenerateWidget( TSharedPtr<EStatGroupingOrSortingMode::Type> InGroupingMode ) const
{
	return SNew( STextBlock )
		.Text( EStatGroupingOrSortingMode::ToName( *InGroupingMode ) )
		.ToolTipText( EStatGroupingOrSortingMode::ToDescription( *InGroupingMode ) );
}

FText SFiltersAndPresets::GroupBy_GetSelectedText() const
{
	return EStatGroupingOrSortingMode::ToName( GroupingMode );
}

FText SFiltersAndPresets::GroupBy_GetSelectedTooltipText() const
{
	return EStatGroupingOrSortingMode::ToDescription( GroupingMode );
}

/*-----------------------------------------------------------------------------
	SortBy
-----------------------------------------------------------------------------*/

void SFiltersAndPresets::SortBy_OnSelectionChanged( TSharedPtr<EStatGroupingOrSortingMode::Type> NewSortingMode, ESelectInfo::Type SelectInfo )
{
	if( SelectInfo != ESelectInfo::Direct )
	{
		SortingMode = *NewSortingMode;

		// Create groups, sort stats within the group and apply filtering.
		SortStats();
		ApplyFiltering();
	}
}

TSharedRef<SWidget> SFiltersAndPresets::SortBy_OnGenerateWidget( TSharedPtr<EStatGroupingOrSortingMode::Type> InSortingMode ) const
{
	return SNew( STextBlock )
		.Text( EStatGroupingOrSortingMode::ToName( *InSortingMode ) )
		.ToolTipText( EStatGroupingOrSortingMode::ToDescription( *InSortingMode ) );
}

FText SFiltersAndPresets::SortBy_GetSelectedText() const
{
	return EStatGroupingOrSortingMode::ToName( SortingMode );
}

#undef LOCTEXT_NAMESPACE
