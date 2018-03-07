// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SEventGraphTooltip.h"
#include "Misc/Paths.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SToolTip.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SEventGraphTooltip"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedPtr<SToolTip> SEventGraphTooltip::GetTableCellTooltip( const TSharedPtr<FEventGraphSample> EventSample )
{
	const FSlateFontInfo TitleFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10 );
	const FSlateFontInfo DescriptionFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8 );
	const FSlateFontInfo DescriptionFontB( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 8 );

	const FLinearColor ThreadColor(5.0f,0.0f,0.0f,1.0f);
	const FLinearColor DefaultColor(1.0f,1.0f,1.0f,1.0f);
	const float Alpha = EventSample->_FramePct * 0.01f;
	const FLinearColor ColorAndOpacity = FMath::Lerp(DefaultColor,ThreadColor,Alpha);

	const FText InclusiveTimePctCaller = FText::Format(LOCTEXT("PctOfTheCaller", "({0} of the caller)"), FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::InclusiveTimePct)));
	const FText ExclusiveTimePctCaller = FText::Format(LOCTEXT("PctOfThisCallIncTime", "({0} of this call's inc time)"), FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::ExclusiveTimePct)));

	TAttribute< EVisibility > HotPathIconVisibility = TAttribute< EVisibility >::Create( TAttribute<EVisibility>::FGetter::CreateStatic( &SEventGraphTooltip::GetHotPathIconVisibility, EventSample ) );

	TSharedPtr<SHorizontalBox> HBoxCaption;
	TSharedPtr<SGridPanel> GridPanel;
	TSharedPtr<SHorizontalBox> HBox;

	TSharedPtr<SToolTip> TableCellTooltip =
		SNew(SToolTip)
		[
			SAssignNew(HBox,SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( 2.0f )
				[
					SAssignNew(HBoxCaption,SHorizontalBox)
				]

				+SVerticalBox::Slot()
					.AutoHeight()
					.Padding( 2.0f )
					[
						SNew(SSeparator)
						.Orientation( Orient_Horizontal )
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding( 2.0f )
					[
						SNew(SHorizontalBox)
						.Visibility( HotPathIconVisibility )

						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew( SImage )
							.Image( FEditorStyle::GetBrush(TEXT("Profiler.EventGraph.HotPathSmall")) )
						]

						+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text( LOCTEXT("HotPathInformation", "Hot path, should be investigated for bottlenecks") )
								//.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
								.ColorAndOpacity( ColorAndOpacity )
								.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8 ) )
								.ShadowOffset( FVector2D(1.0f, 1.0f) )
								.ShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f,0.5f) )
							]
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					.Padding( 2.0f )
					[
						SNew(SSeparator)
						.Orientation( Orient_Horizontal )
						.Visibility( HotPathIconVisibility )
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					.Padding( 2.0f )
					[
						SNew(SGridPanel)
					
						//-----------------------------------------------------------------------------
						// First row 
						// Thread: [_ThreadName]
						+SGridPanel::Slot(0,0)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_Thread", "Thread:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						+SGridPanel::Slot(1,0)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromName(EventSample->_ThreadName ))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						// Event: [_StatName]
						+SGridPanel::Slot(2,0)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_Event", "Event:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						+SGridPanel::Slot(3,0)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetShortEventName()) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						// Group: [_GroupName]
						+SGridPanel::Slot(4,0)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_Group", "Group:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						+SGridPanel::Slot(5,0)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromName(EventSample->_GroupName) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						//-----------------------------------------------------------------------------
						// Second row
						// Inclusive time: 
						+SGridPanel::Slot(0,1)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_InclusiveTime", "Inclusive time:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [InclusiveTimeMS] ms
						+SGridPanel::Slot(1,1)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::InclusiveTimeMS)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]
						// ([_InclusiveTimePct] % of the caller)
						+SGridPanel::Slot(2,1)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( InclusiveTimePctCaller )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]
						// Average inclusive time per call of all instances for this event, in milliseconds
						+SGridPanel::Slot(3,1)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_AvgIncTimePerCall", "Avg inc time per call:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						
						//-----------------------------------------------------------------------------
						// Third row
						// 		_ThreadDurationMS, (_ThreadPct); /** Percent of inclusive time spent by this event in the particular thread. */
						// 		_FrameDurationMS, (_FramePct); /** Percent of inclusive time spent by this event in the particular frame. */
						+SGridPanel::Slot(0,2)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_PctOfThread", "% of thread:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [ThreadPct] %
						+SGridPanel::Slot(1,2)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::ThreadPct)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						+SGridPanel::Slot(3,2)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_PctOfFrame", "% of frame:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [FramePct] %
						+SGridPanel::Slot(4,2)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::FramePct)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						//-----------------------------------------------------------------------------
						// Fourth row
						// _MinInclusiveTimeMS, _MaxInclusiveTimeMS, _AvgInclusiveTimeMS;
						// Min inclusive time: 
						+SGridPanel::Slot(0,3)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_MinMaxAvgIncTime", "Min/Max/Avg inclusive time:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [_MinInclusiveTimeMS] ms
						+SGridPanel::Slot(1,3)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::MinInclusiveTimeMS)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]
						// [_MaxInclusiveTimeMS] ms
						+SGridPanel::Slot(2,3)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::MaxInclusiveTimeMS)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]
						// [_AvgInclusiveTimeMS] ms
						+SGridPanel::Slot(3,3)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::AvgInclusiveTimeMS)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						//-----------------------------------------------------------------------------
						// Fifth row
						// _ExclusiveTimeMS, _ExclusiveTimePct;
						// Inclusive time: 
						+SGridPanel::Slot(0,4)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_ExclusiveTime", "Exclusive time:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [_ExclusiveTimeMS] ms
						+SGridPanel::Slot(1,4)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::ExclusiveTimeMS)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]
						// ([_ExclusiveTimePct] % of this call's inc time)
						+SGridPanel::Slot(2,4)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( ExclusiveTimePctCaller )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						//-----------------------------------------------------------------------------
						// Sixth row
						// _NumCallsPerFrame, _AvgNumCallsPerFrame;
						// Inclusive time: 
						+SGridPanel::Slot(0,5)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_NulCallsPerFrame", "Num calls per frame:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [_NumCallsPerFrame]
						+SGridPanel::Slot(1,5)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::NumCallsPerFrame)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]


						//-----------------------------------------------------------------------------
						// Seventh row
						// _MinNumCallsPerFrame, _MaxNumCallsPerFrame, _AvgNumCallsPerFrame;
						// Min inclusive time: 
						+SGridPanel::Slot( 0, 6 )
						.Padding( 2.0f )
						[
							SNew( STextBlock )
							.Text( LOCTEXT( "TT_MinMaxAvgNumCallsPerFrame", "Min/Max/Avg calls per frame:" ) )
							.TextStyle( FEditorStyle::Get(), TEXT( "Profiler.TooltipBold" ) )
						]
						// [_MinNumCallsPerFrame]
						+ SGridPanel::Slot( 1, 6 )
						.Padding( 2.0f )
						[
							SNew( STextBlock )
							.Text( FText::FromString( EventSample->GetFormattedValue( EEventPropertyIndex::MinNumCallsPerFrame ) ) )
							.TextStyle( FEditorStyle::Get(), TEXT( "Profiler.Tooltip" ) )
						]
						// [_MaxNumCallsPerFrame]
						+ SGridPanel::Slot( 2, 6 )
						.Padding( 2.0f )
						[
							SNew( STextBlock )
							.Text( FText::FromString( EventSample->GetFormattedValue( EEventPropertyIndex::MaxNumCallsPerFrame ) ) )
							.TextStyle( FEditorStyle::Get(), TEXT( "Profiler.Tooltip" ) )
						]
						// [_AvgNumCallsPerFrame]
						+ SGridPanel::Slot( 3, 6 )
						.Padding( 2.0f )
						[
							SNew( STextBlock )
							.Text( FText::FromString( EventSample->GetFormattedValue( EEventPropertyIndex::AvgNumCallsPerFrame ) ) )
							.TextStyle( FEditorStyle::Get(), TEXT( "Profiler.Tooltip" ) )
						]
						
						//-----------------------------------------------------------------------------
						// Eighth row
						// _ThreadDurationMS, _FrameDurationMS, _ThreadToFramePct
						// Thread duration: 
						+SGridPanel::Slot(0,7)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_ThreadDuration", "Thread duration:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [_ThreadDurationMS]
						+SGridPanel::Slot(1,7)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::ThreadDurationMS)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						// Frame duration: 
						+SGridPanel::Slot(2,7)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("TT_FrameDuration", "Frame duration:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [_FrameDurationMS]
						+SGridPanel::Slot(3,7)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::FrameDurationMS)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]

						/** Percent of time spent in the thread in relation to the entire frame. */
						// Thread -> Frame: 
						+SGridPanel::Slot(4,7)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ThreadToFrame", "Thread to Frame:"))
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.TooltipBold") )
						]
						// [_FrameDurationMS]
						+SGridPanel::Slot(5,7)
						.Padding( 2.0f )
						[
							SNew(STextBlock)
							.Text( FText::FromString(EventSample->GetFormattedValue(EEventPropertyIndex::ThreadToFramePct)) )
							.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Tooltip") )
						]
					]

				+SVerticalBox::Slot()
					.AutoHeight()
					.Padding( 2.0f )
					[
						SNew(SSeparator)
						.Orientation( Orient_Horizontal )
					]
			]
		];

	// 		GetParent(), GetChildren( top3 );
	const bool bHasParent = EventSample->GetParent().IsValid();
	const bool bHasChildren = EventSample->GetChildren().Num() > 0;

	if( bHasParent )
	{
		const FString ParentName = EventSample->GetParent()->GetShortEventName();

		HBoxCaption->AddSlot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text( FText::FromString(ParentName) )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Caption") )
			];

		HBoxCaption->AddSlot()
			.AutoWidth()
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("BreadcrumbTrail.Delimiter") )
			];
	}

	HBoxCaption->AddSlot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text( FText::FromString(EventSample->GetShortEventName()) )
			.TextStyle( FEditorStyle::Get(), TEXT("Profiler.CaptionBold") )
		];

	if( bHasChildren )
	{
		typedef TKeyValuePair<float,FString> FEventNameAndPct;
		TArray<FEventNameAndPct> MinimalChildren;

		const TArray<FEventGraphSamplePtr>& Children = EventSample->GetChildren();
		for( int32 ChildIndex = 0; ChildIndex < Children.Num(); ChildIndex++ )
		{
			const FEventGraphSamplePtr& Child = Children[ChildIndex];
			MinimalChildren.Add( FEventNameAndPct(Child->_InclusiveTimePct,Child->GetShortEventName()) );
		}

		struct FCompareByFloatDescending
		{
			FORCEINLINE bool operator()( const FEventNameAndPct& A, const FEventNameAndPct& B ) const 
			{
				return A.Key > B.Key; 
			}
		};
		MinimalChildren.Sort( FCompareByFloatDescending() );

		FString ChildrenNames;
		const int NumChildrenToDisplay = FMath::Min( MinimalChildren.Num(), 3 );
		for( int32 SortedChildIndex = 0; SortedChildIndex < NumChildrenToDisplay; SortedChildIndex++ )
		{
			const FEventNameAndPct& MinimalChild = MinimalChildren[SortedChildIndex];
			ChildrenNames += FString::Printf( TEXT("%s (%.1f %%)"), *MinimalChild.Value, MinimalChild.Key );

			const bool bAddDelimiter = SortedChildIndex < NumChildrenToDisplay-1;
			if( bAddDelimiter )
			{
				ChildrenNames += TEXT(", ");
			}
		}

		HBoxCaption->AddSlot()
			.AutoWidth()
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush("BreadcrumbTrail.Delimiter") )
			];

		HBoxCaption->AddSlot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text( FText::FromString(ChildrenNames) )
				.TextStyle( FEditorStyle::Get(), TEXT("Profiler.Caption") )
			];
	}

	return TableCellTooltip;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


EVisibility SEventGraphTooltip::GetHotPathIconVisibility(const TSharedPtr<FEventGraphSample> EventSample)
{
	const bool bIsHotPathIconVisible = EventSample->_bIsHotPath;
	return bIsHotPathIconVisible ? EVisibility::Visible : EVisibility::Collapsed;
}


#undef LOCTEXT_NAMESPACE
