// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMaterialEditorTitleBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Views/SListView.h"
#include "EditorStyleSet.h"

void SMaterialEditorTitleBar::Construct(const FArguments& InArgs)
{
	Visibility = EVisibility::HitTestInvisible;

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush( TEXT("Graph.TitleBackground") ) )
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			// Title text/icon
			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(10)
			.AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle( FEditorStyle::Get(), TEXT("GraphBreadcrumbButtonText") )
				.Text( InArgs._TitleText )
			]
			+SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.f)
				.Padding(5,0)
				[
					SAssignNew(MaterialInfoList, SListView<TSharedPtr<FMaterialInfo>>)
					.ListItemsSource(InArgs._MaterialInfoList)
					.OnGenerateRow(this, &SMaterialEditorTitleBar::MakeMaterialInfoWidget)
					.SelectionMode( ESelectionMode::None )
					.Visibility((InArgs._MaterialInfoList != NULL) ? EVisibility::Visible : EVisibility::Collapsed)
				]
			]
		]
	];
}

TSharedRef<ITableRow> SMaterialEditorTitleBar::MakeMaterialInfoWidget(TSharedPtr<FMaterialInfo> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const int32 FontSize = 9;

	FMaterialInfo Info = *Item.Get();
	FLinearColor TextColor = Info.Color;
	FString Text = Info.Text;

	if( Text.IsEmpty() )
	{
		return
			SNew(STableRow< TSharedPtr<FMaterialInfo> >, OwnerTable)
			[
				SNew(SSpacer)
			];
	}
	else 
	{
		return
			SNew(STableRow< TSharedPtr<FMaterialInfo> >, OwnerTable)
			[
				SNew(STextBlock)
				.ColorAndOpacity(TextColor)
				.Font(FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), FontSize ))
				.Text(FText::FromString(Text))
			];
	}
}

void SMaterialEditorTitleBar::RequestRefresh()
{
	MaterialInfoList->RequestListRefresh();
}
