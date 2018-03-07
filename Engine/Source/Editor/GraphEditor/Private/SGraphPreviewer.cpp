// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SGraphPreviewer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "SGraphPanel.h"

void SGraphPreviewer::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Keep the graph constantly zoomed to fit
	GraphPanel->ZoomToFit(false);

	// Refresh the graph if needed
	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;
		GraphPanel->Update();
	}	
}

void SGraphPreviewer::Construct( const FArguments& InArgs, UEdGraph* InGraphObj )
{
	EdGraphObj = InGraphObj;
	bNeedsRefresh = true;

	TSharedPtr<SOverlay> DisplayStack;

	this->ChildSlot
	[
		SAssignNew(DisplayStack, SOverlay)

		// The graph panel
		+SOverlay::Slot()
		[
			SAssignNew(GraphPanel, SGraphPanel)
			.GraphObj( EdGraphObj )
			.IsEditable( false )
			.ShowGraphStateOverlay(InArgs._ShowGraphStateOverlay)
			.InitialZoomToFit( true )
		]

		// Bottom-right corner text indicating the type of tool
		+SOverlay::Slot()
		.Padding(4)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Visibility( EVisibility::HitTestInvisible )
			.TextStyle( FEditorStyle::Get(), "GraphPreview.CornerText" )
			.Text( InArgs._CornerOverlayText )
		]
	];

	// Add the title bar if specified
	if (InArgs._TitleBar.IsValid())
	{
		DisplayStack->AddSlot()
			.VAlign(VAlign_Top)
			[
				InArgs._TitleBar.ToSharedRef()
			];
	}
}
