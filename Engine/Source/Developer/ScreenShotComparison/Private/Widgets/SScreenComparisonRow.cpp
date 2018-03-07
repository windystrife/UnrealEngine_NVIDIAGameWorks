// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SScreenComparisonRow.h"
#include "Models/ScreenComparisonModel.h"
#include "Modules/ModuleManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "IImageWrapperModule.h"
#include "Framework/Application/SlateApplication.h"
#include "JsonObjectConverter.h"
#include "SScreenShotImagePopup.h"
#include "SAsyncImage.h"

#define LOCTEXT_NAMESPACE "SScreenShotBrowser"

class SImageComparison : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SImageComparison) {}
		SLATE_ARGUMENT(TSharedPtr<FSlateDynamicImageBrush>, BaseImage)
		SLATE_ARGUMENT(TSharedPtr<FSlateDynamicImageBrush>, ModifiedImage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		BaseImage = InArgs._BaseImage;
		ModifiedImage = InArgs._ModifiedImage;

		//const int32 MaxHeight = FSlateApplication::Get().GetPreferredWorkArea().GetSize().Y * 0.80;

		ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(BaseImage.Get())
					]
						
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(ModifiedImage.Get())
						.ColorAndOpacity(this, &SImageComparison::GetModifiedOpacity)
					]
				]
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("GroundTruth", "Ground Truth"))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(150)
					[
						SAssignNew(OpacitySlider, SSlider)
						.Value(0.5f)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Incoming", "Incoming"))
				]
			]
		];
	}

	FSlateColor GetModifiedOpacity() const
	{
		return FLinearColor(1, 1, 1, OpacitySlider->GetValue());
	}

private:
	TSharedPtr<FSlateDynamicImageBrush> BaseImage;
	TSharedPtr<FSlateDynamicImageBrush> ModifiedImage;

	TSharedPtr<SSlider> OpacitySlider;
};


void SScreenComparisonRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
	ScreenshotManager = InArgs._ScreenshotManager;
	ComparisonDirectory = InArgs._ComparisonDirectory;
	Model = InArgs._ComparisonResult;

	CachedActualImageSize = FIntPoint::NoneValue;

	SMultiColumnTableRow<TSharedPtr<FScreenComparisonModel>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SScreenComparisonRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if ( ColumnName == "Name" )
	{
		if ( Model->GetMetadata().IsSet() )
		{
			return SNew(STextBlock).Text(FText::FromString(Model->GetMetadata()->Name));
		}
		else
		{
			return SNew(STextBlock).Text(LOCTEXT("Unknown", "Unknown Test, no metadata discovered."));
		}
	}
	else if ( ColumnName == "Delta" )
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 2;
		Format.MaximumFractionalDigits = 2;
		const FText GlobalDelta = FText::AsPercent(Model->Report.Comparison.GlobalDifference, &Format);
		const FText LocalDelta = FText::AsPercent(Model->Report.Comparison.MaxLocalDifference, &Format);

		const FText Differences = FText::Format(LOCTEXT("LocalvGlobalDelta", "{0} | {1}"), LocalDelta, GlobalDelta);
		return SNew(STextBlock).Text(Differences);
	}
	else if ( ColumnName == "Preview" )
	{
		const FImageComparisonResult& ComparisonResult = Model->Report.Comparison;
		if ( ComparisonResult.IsNew() )
		{
			return BuildAddedView();
		}
		else
		{
			return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildComparisonPreview()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.IsEnabled(this, &SScreenComparisonRow::CanReplace)
						.Text(LOCTEXT("Replace", "Replace"))
						.OnClicked(this, &SScreenComparisonRow::Replace)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10, 0, 0, 0)
					[
						SNew(SButton)
						.IsEnabled(this, &SScreenComparisonRow::CanAddAsAlternative)
						.Text(LOCTEXT("AddAlternative", "Add As Alternative"))
						.OnClicked(this, &SScreenComparisonRow::AddAlternative)
					]
				];
		}
	}

	return SNullWidget::NullWidget;
}

bool SScreenComparisonRow::CanUseSourceControl() const
{
	return ISourceControlModule::Get().IsEnabled();
}

TSharedRef<SWidget> SScreenComparisonRow::BuildAddedView()
{
	const FImageComparisonResult& ComparisonResult = Model->Report.Comparison;
	FString IncomingFile = Model->Report.ReportFolder / ComparisonResult.ReportIncomingFile;

	return
		SNew(SVerticalBox)
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(100)
			.HAlign(HAlign_Left)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SHorizontalBox)
		
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 4.0f)
					[
						SNew(SAsyncImage)
						.ImageFilePath(IncomingFile)
						//.OnMouseButtonDown(this, &SScreenComparisonRow::OnImageClicked, UnapprovedBrush)
					]
				]
			]
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SButton)
			.IsEnabled(this, &SScreenComparisonRow::CanUseSourceControl)
			.Text(LOCTEXT("AddNew", "Add New!"))
			.OnClicked(this, &SScreenComparisonRow::AddNew)
		];
}

TSharedRef<SWidget> SScreenComparisonRow::BuildComparisonPreview()
{
	const FImageComparisonResult& ComparisonResult = Model->Report.Comparison;

	FString ApprovedFile = Model->Report.ReportFolder / ComparisonResult.ReportApprovedFile;
	FString IncomingFile = Model->Report.ReportFolder / ComparisonResult.ReportIncomingFile;
	FString DeltaFile = Model->Report.ReportFolder / ComparisonResult.ReportComparisonFile;

	// Create the screen shot data widget.
	return 
		SNew(SBorder)
		.BorderImage(nullptr)
		.OnMouseButtonDown(this, &SScreenComparisonRow::OnCompareImages)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(100)
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					[
						SNew(SHorizontalBox)
					
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.0f, 4.0f)
						[
							SAssignNew(ApprovedImageWidget, SAsyncImage)
							.ImageFilePath(ApprovedFile)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.0f, 4.0f)
						[
							SNew(SAsyncImage)
							.ImageFilePath(DeltaFile)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.0f, 4.0f)
						[
							SAssignNew(UnapprovedImageWidget, SAsyncImage)
							.ImageFilePath(IncomingFile)
						]
					]
				]
			]
	
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT( "GroundTruth", "Ground Truth" ))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Difference", "Difference"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Incoming", "Incoming"))
				]
			]
		];
}

bool SScreenComparisonRow::CanAddNew() const
{
	return CanUseSourceControl();
}

FReply SScreenComparisonRow::AddNew()
{
	Model->AddNew(ScreenshotManager);

	return FReply::Handled();
}

bool SScreenComparisonRow::CanReplace() const
{
	return CanUseSourceControl();
}

FReply SScreenComparisonRow::Replace()
{
	Model->Replace(ScreenshotManager);

	return FReply::Handled();
}

bool SScreenComparisonRow::CanAddAsAlternative() const
{
	return CanUseSourceControl() && (Model->Report.Comparison.IncomingFile != Model->Report.Comparison.ApprovedFile);
}

FReply SScreenComparisonRow::AddAlternative()
{
	Model->AddAlternative(ScreenshotManager);

	return FReply::Handled();
}

FReply SScreenComparisonRow::OnCompareImages(const FGeometry& InGeometry, const FPointerEvent& InEvent)
{
	TSharedPtr<FSlateDynamicImageBrush> ApprovedImage = ApprovedImageWidget->GetDynamicBrush();
	TSharedPtr<FSlateDynamicImageBrush> UnapprovedImage = UnapprovedImageWidget->GetDynamicBrush();

	if ( ApprovedImage.IsValid() && UnapprovedImage.IsValid() )
	{
		TSharedRef<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared()).ToSharedRef();

		// Center ourselves in the parent window
		TSharedRef<SWindow> PopupWindow = SNew(SWindow)
			.IsPopupWindow(false)
			.ClientSize(FVector2D(1280, 720))
			.SizingRule(ESizingRule::UserSized)
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.SupportsMaximize(true)
			.SupportsMinimize(true)
			.FocusWhenFirstShown(true)
			.ActivationPolicy(EWindowActivationPolicy::Always)
			.Content()
			[
				SNew(SImageComparison)
				.BaseImage(ApprovedImage)
				.ModifiedImage(UnapprovedImage)
			];

		FSlateApplication::Get().AddWindowAsNativeChild(PopupWindow, ParentWindow, true);
	}

	return FReply::Handled();
}

FReply SScreenComparisonRow::OnImageClicked(const FGeometry& InGeometry, const FPointerEvent& InEvent, TSharedPtr<FSlateDynamicImageBrush> Image)
{
	TSharedRef<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared()).ToSharedRef();

	// Center ourselves in the parent window
	TSharedRef<SWindow> PopupWindow = SNew(SWindow)
		.IsPopupWindow(false)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(Image->ImageSize)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.FocusWhenFirstShown(true)
		.ActivationPolicy(EWindowActivationPolicy::Always)
		.Content()
		[
			SNew(SScreenShotImagePopup)
			.ImageBrush(Image)
			.ImageSize(Image->ImageSize.IntPoint())
		];

	FSlateApplication::Get().AddWindowAsNativeChild(PopupWindow, ParentWindow, true);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE