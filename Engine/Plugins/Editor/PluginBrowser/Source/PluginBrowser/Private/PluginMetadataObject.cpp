// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PluginMetadataObject.h"
#include "Misc/Paths.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Interfaces/IPluginManager.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "SExternalImageReference.h"

UPluginMetadataObject::UPluginMetadataObject(const FObjectInitializer& ObjectInitializer)
{
}

void UPluginMetadataObject::PopulateFromDescriptor(const FPluginDescriptor& InDescriptor)
{
	Version = InDescriptor.Version;
	VersionName = InDescriptor.VersionName;
	FriendlyName = InDescriptor.FriendlyName;
	Description = InDescriptor.Description;
	Category = InDescriptor.Category;
	CreatedBy = InDescriptor.CreatedBy;
	CreatedByURL = InDescriptor.CreatedByURL;
	DocsURL = InDescriptor.DocsURL;
	MarketplaceURL = InDescriptor.MarketplaceURL;
	SupportURL = InDescriptor.SupportURL;
	bCanContainContent = InDescriptor.bCanContainContent;
	bIsBetaVersion = InDescriptor.bIsBetaVersion;
}

void UPluginMetadataObject::CopyIntoDescriptor(FPluginDescriptor& OutDescriptor)
{
	OutDescriptor.Version = Version;
	OutDescriptor.VersionName = VersionName;
	OutDescriptor.FriendlyName = FriendlyName;
	OutDescriptor.Description = Description;
	OutDescriptor.Category = Category;
	OutDescriptor.CreatedBy = CreatedBy;
	OutDescriptor.CreatedByURL = CreatedByURL;
	OutDescriptor.DocsURL = DocsURL;
	OutDescriptor.MarketplaceURL = MarketplaceURL;
	OutDescriptor.SupportURL = SupportURL;
	OutDescriptor.bCanContainContent = bCanContainContent;
	OutDescriptor.bIsBetaVersion = bIsBetaVersion;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<IDetailCustomization> FPluginMetadataCustomization::MakeInstance()
{
	return MakeShareable(new FPluginMetadataCustomization());
}

void FPluginMetadataCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if(Objects.Num() == 1 && Objects[0].IsValid())
	{
		UPluginMetadataObject* PluginMetadata = Cast<UPluginMetadataObject>(Objects[0].Get());
		if(PluginMetadata != nullptr && PluginMetadata->TargetIconPath.Len() > 0)
		{
			// Get the current icon path
			FString CurrentIconPath = PluginMetadata->TargetIconPath;
			if(!FPaths::FileExists(CurrentIconPath))
			{
				CurrentIconPath = IPluginManager::Get().FindPlugin(TEXT("PluginBrowser"))->GetBaseDir() / TEXT("Resources") / TEXT("DefaultIcon128.png");
			}

			// Add the customization to edit the icon row
			IDetailCategoryBuilder& ImageCategory = DetailBuilder.EditCategory(TEXT("Icon"));
			const FText IconDesc(NSLOCTEXT("PluginBrowser", "PluginIcon", "Icon"));
			ImageCategory.AddCustomRow(IconDesc)
			.NameContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding( FMargin( 0, 1, 0, 1 ) )
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(IconDesc)
					.Font(DetailBuilder.GetDetailFont())
				]
			]
			.ValueContent()
			.MaxDesiredWidth(500.0f)
			.MinDesiredWidth(100.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SExternalImageReference, CurrentIconPath, PluginMetadata->TargetIconPath)
					.FileDescription(IconDesc)
					.RequiredSize(FIntPoint(128, 128))
				]
			];
		}
	}
}
