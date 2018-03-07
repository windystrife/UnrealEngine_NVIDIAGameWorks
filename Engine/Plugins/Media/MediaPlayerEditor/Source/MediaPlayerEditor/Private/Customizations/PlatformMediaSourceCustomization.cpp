// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customizations/PlatformMediaSourceCustomization.h"
#include "MediaSource.h"
#include "PlatformMediaSource.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditorStyleSet.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IMediaModule.h"
#include "PlatformInfo.h"
#include "PropertyCustomizationHelpers.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Images/SImage.h"



#define LOCTEXT_NAMESPACE "FPlatformMediaSourceCustomization"


/* IDetailCustomization interface
 *****************************************************************************/

void FPlatformMediaSourceCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// customize 'Platforms' category
	IDetailCategoryBuilder& SourcesCategory = DetailBuilder.EditCategory("Sources");
	{
		// PlatformMediaSources
		PlatformMediaSourcesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPlatformMediaSource, PlatformMediaSources));
		{
			IDetailPropertyRow& PlatformMediaSourcesRow = SourcesCategory.AddProperty(PlatformMediaSourcesProperty);

			PlatformMediaSourcesRow
				.ShowPropertyButtons(false)
				.CustomWidget()
					.NameContent()
					[
						PlatformMediaSourcesProperty->CreatePropertyNameWidget()
					]
					.ValueContent()
					.MaxDesiredWidth(0.0f)
					[
						MakePlatformMediaSourcesValueWidget()
					];
		}
	}
}


/* FPlatformMediaSourceCustomization implementation
 *****************************************************************************/

TSharedRef<SWidget> FPlatformMediaSourceCustomization::MakePlatformMediaSourcesValueWidget()
{
	// get registered player plug-ins
	auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule == nullptr)
	{
		return SNew(STextBlock)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("NoPlayersAvailableLabel", "No players available"));
	}

	const TArray<IMediaPlayerFactory*>& PlayerFactories = MediaModule->GetPlayerFactories();

	// get available platforms
	TArray<const PlatformInfo::FPlatformInfo*> AvailablePlatforms;

	for (const PlatformInfo::FPlatformInfo& PlatformInfo : PlatformInfo::EnumeratePlatformInfoArray())
	{
		if (PlatformInfo.IsVanilla() && (PlatformInfo.PlatformType == PlatformInfo::EPlatformType::Game) && (PlatformInfo.PlatformInfoName != TEXT("AllDesktop")))
		{
			AvailablePlatforms.Add(&PlatformInfo);
		}
	}

	// sort available platforms alphabetically
	AvailablePlatforms.Sort([](const PlatformInfo::FPlatformInfo& One, const PlatformInfo::FPlatformInfo& Two) -> bool
	{
		return One.DisplayName.CompareTo(Two.DisplayName) < 0;
	});

	// build value widget
	TSharedRef<SGridPanel> PlatformPanel = SNew(SGridPanel);

	for (int32 PlatformIndex = 0; PlatformIndex < AvailablePlatforms.Num(); ++PlatformIndex)
	{
		const PlatformInfo::FPlatformInfo* Platform = AvailablePlatforms[PlatformIndex];

		// platform icon
		PlatformPanel->AddSlot(0, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
					.Image(FEditorStyle::GetBrush(Platform->GetIconStyleName(PlatformInfo::EPlatformIconSize::Normal)))
			];

		// platform name
		PlatformPanel->AddSlot(1, PlatformIndex)
			.Padding(4.0f, 0.0f, 16.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(Platform->DisplayName)
			];

		// player combo box
		PlatformPanel->AddSlot(2, PlatformIndex)
			.VAlign(VAlign_Center)
			[
				SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMediaSource::StaticClass())
					.AllowClear(true)
					.ObjectPath(this, &FPlatformMediaSourceCustomization::HandleMediaSourceEntryBoxObjectPath, Platform->IniPlatformName)
					.OnObjectChanged(this, &FPlatformMediaSourceCustomization::HandleMediaSourceEntryBoxChanged, Platform->IniPlatformName)
					.OnShouldFilterAsset(this, &FPlatformMediaSourceCustomization::HandleMediaSourceEntryBoxShouldFilterAsset)
			];
	}

	return PlatformPanel;
}


void FPlatformMediaSourceCustomization::SetPlatformMediaSourcesValue(FString PlatformName, UMediaSource* MediaSource)
{
	TArray<UObject*> OuterObjects;
	{
		PlatformMediaSourcesProperty->GetOuterObjects(OuterObjects);
	}

	for (auto Object : OuterObjects)
	{
		UMediaSource*& OldMediaSource = Cast<UPlatformMediaSource>(Object)->PlatformMediaSources.FindOrAdd(PlatformName);;

		if (OldMediaSource != MediaSource)
		{
			Object->Modify(true);
			OldMediaSource = MediaSource;
		}
	}
}


/* FPlatformMediaSourceCustomization callbacks
 *****************************************************************************/

void FPlatformMediaSourceCustomization::HandleMediaSourceEntryBoxChanged(const FAssetData& AssetData, FString PlatformName)
{
	TArray<UObject*> OuterObjects;
	{
		PlatformMediaSourcesProperty->GetOuterObjects(OuterObjects);
	}

	for (auto Object : OuterObjects)
	{
		UMediaSource*& OldMediaSource = Cast<UPlatformMediaSource>(Object)->PlatformMediaSources.FindOrAdd(PlatformName);;

		if (OldMediaSource != AssetData.GetAsset())
		{
			Object->Modify(true);
			OldMediaSource = Cast<UMediaSource>(AssetData.GetAsset());
		}
	}
}


FString FPlatformMediaSourceCustomization::HandleMediaSourceEntryBoxObjectPath(FString PlatformName) const
{
	TArray<UObject*> OuterObjects;
	{
		PlatformMediaSourcesProperty->GetOuterObjects(OuterObjects);
	}

	if (OuterObjects.Num() == 0)
	{
		return FString();
	}

	UMediaSource* MediaSource = Cast<UPlatformMediaSource>(OuterObjects[0])->PlatformMediaSources.FindRef(PlatformName);

	for (int32 ObjectIndex = 1; ObjectIndex < OuterObjects.Num(); ++ObjectIndex)
	{
		if (Cast<UPlatformMediaSource>(OuterObjects[ObjectIndex])->PlatformMediaSources.FindRef(PlatformName) != MediaSource)
		{
			return FString();
		}
	}

	if (MediaSource == nullptr)
	{
		return FString();
	}

	return MediaSource->GetPathName();
}


bool FPlatformMediaSourceCustomization::HandleMediaSourceEntryBoxShouldFilterAsset(const FAssetData& AssetData)
{
	// Don't allow nesting platform media sources.
	UClass* AssetClass = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());
	return AssetClass->IsChildOf(UPlatformMediaSource::StaticClass());
}


#undef LOCTEXT_NAMESPACE
