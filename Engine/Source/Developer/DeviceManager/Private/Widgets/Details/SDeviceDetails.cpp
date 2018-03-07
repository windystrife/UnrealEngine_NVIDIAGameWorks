// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Details/SDeviceDetails.h"
#include "Widgets/SOverlay.h"
#include "SlateOptMacros.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Interfaces/ITargetPlatform.h"
#include "Widgets/Shared/SDeviceQuickInfo.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

#include "Models/DeviceDetailsFeature.h"
#include "Models/DeviceManagerModel.h"
#include "Widgets/Details/SDeviceDetailsFeatureListRow.h"


#define LOCTEXT_NAMESPACE "SDeviceDetails"


/* SMessagingEndpoints structors
*****************************************************************************/

SDeviceDetails::~SDeviceDetails()
{
	if (Model.IsValid())
	{
		Model->OnSelectedDeviceServiceChanged().RemoveAll(this);
	}
}


/* SDeviceDetails interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDeviceDetails::Construct(const FArguments& InArgs, const TSharedRef<FDeviceManagerModel>& InModel)
{
	Model = InModel;

	// callback for getting the visibility of the details box.
	auto DetailsBoxVisibility = [this]() -> EVisibility {
		return Model->GetSelectedDeviceService().IsValid()
			? EVisibility::Visible
			: EVisibility::Hidden;
	};

	// callback for generating a row widget for the feature list view.
	auto FeatureListGenerateRow = [](TSharedPtr<FDeviceDetailsFeature> Feature, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow> {
		return SNew(SDeviceDetailsFeatureListRow, OwnerTable, Feature.ToSharedRef());
			//.Style(Style);
	};

	// callback for getting the visibility of the 'Select a device' message.
	auto HandleSelectDeviceOverlayVisibility = [this]() -> EVisibility {
		return Model->GetSelectedDeviceService().IsValid()
			? EVisibility::Hidden
			: EVisibility::Visible;
	};

	// construct children
	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
					.Visibility_Lambda(DetailsBoxVisibility)

				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.0f, 2.0f)
					[
						// quick info
						SAssignNew(QuickInfo, SDeviceQuickInfo)
					]

				+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							.Padding(0.0f)
							[
								// feature list view
								SAssignNew(FeatureListView, SListView<TSharedPtr<FDeviceDetailsFeature>>)
									.ItemHeight(24.0f)
									.ListItemsSource(&FeatureList)
									.OnGenerateRow_Lambda(FeatureListGenerateRow)
									.SelectionMode(ESelectionMode::None)
									.HeaderRow
									(
										SNew(SHeaderRow)

										+ SHeaderRow::Column("Feature")
											.DefaultLabel(LOCTEXT("FeatureListFeatureColumnHeader", "Feature"))
											.FillWidth(0.6f)

										+ SHeaderRow::Column("Available")
											.DefaultLabel(LOCTEXT("FeatureListAvailableColumnHeader", "Available"))
											.FillWidth(0.4f)
									)
							]
					]
			]

		+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
					.Padding(8.0f)
					.Visibility_Lambda(HandleSelectDeviceOverlayVisibility)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("SelectSessionOverlayText", "Please select a device from the Device Browser"))
					]
			]
	];

	// callback for handling device service selection changes.
	auto ModelSelectedDeviceServiceChanged = [this]() {
		FeatureList.Empty();

		ITargetDeviceServicePtr DeviceService = Model->GetSelectedDeviceService();

		if (DeviceService.IsValid())
		{
			ITargetDevicePtr TargetDevice = DeviceService->GetDevice();

			if (TargetDevice.IsValid())
			{
				const ITargetPlatform& TargetPlatform = TargetDevice->GetTargetPlatform();

				// platform features
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("AudioStreaming"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::AudioStreaming))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("DistanceFieldShadows"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::DistanceFieldShadows))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("GrayscaleSRGB"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::GrayscaleSRGB))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("HighQualityLightmaps"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::HighQualityLightmaps))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("LowQualityLightmaps"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::LowQualityLightmaps))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("MultipleGameInstances"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::MultipleGameInstances))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("Packaging"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::Packaging))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("SdkConnectDisconnect"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::SdkConnectDisconnect))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("Tessellation"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::Tessellation))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("TextureStreaming"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::TextureStreaming))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("UserCredentials"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::UserCredentials))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("MobileRendering"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::MobileRendering))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("DeferredRendering"), TargetPlatform.SupportsFeature(ETargetPlatformFeatures::DeferredRendering))));

				// device features
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("MultiLaunch"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::MultiLaunch))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("PowerOff"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::PowerOff))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("PowerOn"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::PowerOn))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("ProcessSnapshot"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::ProcessSnapshot))));
				FeatureList.Add(MakeShareable(new FDeviceDetailsFeature(TEXT("Reboot"), TargetDevice->SupportsFeature(ETargetDeviceFeatures::Reboot))));
			}
		}

		FeatureListView->RequestListRefresh();
		QuickInfo->SetDeviceService(DeviceService);
	};

	// wire up models
	Model->OnSelectedDeviceServiceChanged().AddLambda(ModelSelectedDeviceServiceChanged);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


#undef LOCTEXT_NAMESPACE
