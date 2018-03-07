// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAddContentWidget.h"
#include "ViewModels/ContentSourceViewModel.h"
#include "ViewModels/AddContentWidgetViewModel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "AddContentDialogStyle.h"

#include "Widgets/Input/SSearchBox.h"
#include "WidgetCarouselStyle.h"
#include "SWidgetCarouselWithNavigation.h"

#define LOCTEXT_NAMESPACE "AddContentDialog"

void SAddContentWidget::Construct(const FArguments& InArgs)
{
	ViewModel = FAddContentWidgetViewModel::CreateShared();
	ViewModel->SetOnCategoriesChanged(FAddContentWidgetViewModel::FOnCategoriesChanged::CreateSP(
		this, &SAddContentWidget::CategoriesChanged));
	ViewModel->SetOnContentSourcesChanged(FAddContentWidgetViewModel::FOnContentSourcesChanged::CreateSP(
		this, &SAddContentWidget::ContentSourcesChanged));
	ViewModel->SetOnSelectedContentSourceChanged(FAddContentWidgetViewModel::FOnSelectedContentSourceChanged::CreateSP(
		this, &SAddContentWidget::SelectedContentSourceChanged));

	ChildSlot
	[
		SNew(SVerticalBox)

		// Tab Buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(10, 0))
		[
			SAssignNew(CategoryTabsContainer, SBox)
			[
				CreateCategoryTabs()
			]
		]

		// Content Source Tab Page
		+ SVerticalBox::Slot()
		.FillHeight(3.0f)
		[
			SNew(SBorder)
			.BorderImage(FAddContentDialogStyle::Get().GetBrush("AddContentDialog.TabBackground"))
			.Padding(FMargin(15))
			[
				SNew(SHorizontalBox)

				// Content Source Tiles
				+ SHorizontalBox::Slot()
				[
					SNew(SVerticalBox)

					// Content Source Filter
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0, 0, 0, 5))
					[
						SAssignNew(SearchBoxPtr, SSearchBox)
						.OnTextChanged(this, &SAddContentWidget::SearchTextChanged)
					]

					// Content Source Tile View
					+ SVerticalBox::Slot()
					[
						CreateContentSourceTileView()
					]
				]

				// Splitter
				+ SHorizontalBox::Slot()
				.Padding(FMargin(10, 0))
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(2)
					[
						SNew(SImage)
						.Image(FAddContentDialogStyle::Get().GetBrush("AddContentDialog.Splitter"))
					]
				]

				// Content Source Details
				+ SHorizontalBox::Slot()
				[
					SAssignNew(ContentSourceDetailContainer, SBox)
					[
						CreateContentSourceDetail(ViewModel->GetSelectedContentSource())
					]
				]
			]
		]
	];
}

const TArray<TSharedPtr<IContentSource>>* SAddContentWidget::GetContentSourcesToAdd()
{
	return &ContentSourcesToAdd;
}

TSharedRef<SWidget> SAddContentWidget::CreateCategoryTabs()
{
	TSharedRef<SHorizontalBox> TabBox = SNew(SHorizontalBox);
	for (auto Category : *ViewModel->GetCategories())
	{
		TabBox->AddSlot()
		.Padding(FMargin(0, 0, 5, 0))
		.AutoWidth()
		[
			SNew(SCheckBox)
			.Style(FAddContentDialogStyle::Get(), "AddContentDialog.CategoryTab")
			.OnCheckStateChanged(this, &SAddContentWidget::CategoryCheckBoxCheckStateChanged, Category)
			.IsChecked(this, &SAddContentWidget::GetCategoryCheckBoxCheckState, Category)
			.Padding(FMargin(5))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0, 0, 5, 0))
				[
					SNew(SImage)
					.Image(Category.GetIconBrush())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FAddContentDialogStyle::Get(), "AddContentDialog.HeadingText")
					.Text(Category.GetText())
				]
			]
		];
	}
	return TabBox;
}

TSharedRef<SWidget> SAddContentWidget::CreateContentSourceTileView()
{
	SAssignNew(ContentSourceTileView, STileView<TSharedPtr<FContentSourceViewModel>>)
	.ListItemsSource(ViewModel->GetContentSources())
	.OnGenerateTile(this, &SAddContentWidget::CreateContentSourceIconTile)
	.OnSelectionChanged(this, &SAddContentWidget::ContentSourceTileViewSelectionChanged)
	.ItemWidth(70)
	.ItemHeight(115)
	.SelectionMode(ESelectionMode::Single);
	ContentSourceTileView->SetSelection(ViewModel->GetSelectedContentSource(), ESelectInfo::Direct);
	return ContentSourceTileView.ToSharedRef();
}

TSharedRef<ITableRow> SAddContentWidget::CreateContentSourceIconTile(TSharedPtr<FContentSourceViewModel> ContentSource, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(3))
		[
			SNew(SImage)
			.Image(ContentSource->GetIconBrush().Get())
		]

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(FMargin(3, 0, 3, 3))
		[
			SNew(STextBlock)
			.Text(ContentSource->GetName())
			.WrapTextAt(64)
			.Justification(ETextJustify::Center)
		]
	];
}

TSharedRef<SWidget> SAddContentWidget::CreateContentSourceDetail(TSharedPtr<FContentSourceViewModel> ContentSource)
{
	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	if (ContentSource.IsValid())
	{
		VerticalBox->AddSlot()
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 5))
			.HAlign(EHorizontalAlignment::HAlign_Left)
			[
				CreateScreenshotCarousel(ContentSource)
			]

			+SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 5))
			[
				SNew(STextBlock)
				.TextStyle(FAddContentDialogStyle::Get(), "AddContentDialog.HeadingText")
				.Text(ContentSource->GetName())
				.AutoWrapText(true)
			]

			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 5))
			[
				SNew(STextBlock)
				.Text(ContentSource->GetDescription())
				.AutoWrapText(true)
			]
		
			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 5))
			[
				SNew(STextBlock)						
				.Visibility(ContentSource->GetAssetTypes().IsEmpty() == false ? EVisibility::Visible : EVisibility::Collapsed)
				.TextStyle(FAddContentDialogStyle::Get(), "AddContentDialog.HeadingText")
				.Text(LOCTEXT("FeaturePackAssetReferences", "Asset types used in this pack:"))
			]
			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 5))
			[
				SNew(STextBlock)
				.Text(ContentSource->GetAssetTypes())
				.Visibility(ContentSource->GetAssetTypes().IsEmpty() == false ? EVisibility::Visible : EVisibility::Collapsed)				
				.AutoWrapText(true)
			]
		
			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 5))
			[
				SNew(STextBlock)
				.TextStyle(FAddContentDialogStyle::Get(), "AddContentDialog.HeadingText")
				.Visibility(ContentSource->GetClassTypes().IsEmpty() == false ? EVisibility::Visible : EVisibility::Collapsed)
				.Text(LOCTEXT("FeaturePackClassReferences", "Class types used in this pack:"))
			] 
			+ SScrollBox::Slot()
			.Padding(FMargin(0, 0, 0, 5))
			[
				SNew(STextBlock)
				.Text(FText::FromString(ContentSource->GetClassTypes()))
				.Visibility(ContentSource->GetClassTypes().IsEmpty() == false ? EVisibility::Visible : EVisibility::Collapsed)
				.AutoWrapText(true)
			]
		];
	
		VerticalBox->AddSlot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
			.OnClicked(this, &SAddContentWidget::AddButtonClicked)
			.ContentPadding(FMargin(5, 5, 5, 5))
			.HAlign(EHorizontalAlignment::HAlign_Center)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(0, 0, 2, 0)
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "NormalText.Important")
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
					.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FAddContentDialogStyle::Get(), "AddContentDialog.AddButton.TextStyle")
					.Text(LOCTEXT("AddToProjectButton", "Add to Project"))
				]
			]
		];
	}
	return VerticalBox;
}

TSharedRef<SWidget> SAddContentWidget::CreateScreenshotCarousel(TSharedPtr<FContentSourceViewModel> ContentSource)
{
	return SNew(SWidgetCarouselWithNavigation<TSharedPtr<FSlateBrush>>)
		.NavigationBarStyle(FWidgetCarouselModuleStyle::Get(), "CarouselNavigationBar")
		.NavigationButtonStyle(FWidgetCarouselModuleStyle::Get(), "CarouselNavigationButton")
		.OnGenerateWidget(this, &SAddContentWidget::CreateScreenshotWidget)
		.WidgetItemsSource(ContentSource->GetScreenshotBrushes());
}

void SAddContentWidget::CategoryCheckBoxCheckStateChanged(ECheckBoxState CheckState, FCategoryViewModel Category)
{
	if (CheckState == ECheckBoxState::Checked)
	{
		ViewModel->SetSelectedCategory(Category);
	}
}

ECheckBoxState SAddContentWidget::GetCategoryCheckBoxCheckState(FCategoryViewModel Category) const
{
	return (Category == ViewModel->GetSelectedCategory()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAddContentWidget::SearchTextChanged(const FText& SearchText)
{
	ViewModel->SetSearchText(SearchText);
	SearchBoxPtr->SetError(ViewModel->GetSearchErrorText());
}

void SAddContentWidget::ContentSourceTileViewSelectionChanged( TSharedPtr<FContentSourceViewModel> SelectedContentSource, ESelectInfo::Type SelectInfo )
{
	ViewModel->SetSelectedContentSource( SelectedContentSource );
}

FReply SAddContentWidget::AddButtonClicked()
{
	if (ViewModel->GetSelectedContentSource().IsValid())
	{
		ViewModel->GetSelectedContentSource()->GetContentSource()->InstallToProject( "/Game" );
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SAddContentWidget::CreateScreenshotWidget(TSharedPtr<FSlateBrush> ScreenshotBrush)
{
	return SNew(SImage)
	.Image(ScreenshotBrush.Get());
}

void SAddContentWidget::CategoriesChanged()
{
	CategoryTabsContainer->SetContent(CreateCategoryTabs());
}

void SAddContentWidget::ContentSourcesChanged()
{
	ContentSourceTileView->RequestListRefresh();
}

void SAddContentWidget::SelectedContentSourceChanged()
{
	ContentSourceTileView->SetSelection(ViewModel->GetSelectedContentSource(), ESelectInfo::Direct);
	ContentSourceDetailContainer->SetContent(CreateContentSourceDetail(ViewModel->GetSelectedContentSource()));
}

SAddContentWidget::~SAddContentWidget()
{
}

#undef LOCTEXT_NAMESPACE
