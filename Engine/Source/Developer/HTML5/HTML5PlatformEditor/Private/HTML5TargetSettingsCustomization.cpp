// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5TargetSettingsCustomization.h"
#include "HTML5TargetSettings.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Misc/EngineBuildSettings.h"

#define LOCTEXT_NAMESPACE "HTML5TargetSettings"
DEFINE_LOG_CATEGORY_STATIC(LogIOSTargetSettings, Log, All);

//////////////////////////////////////////////////////////////////////////
// FHTML5TargetSettingsCustomization
namespace FHTML5TargetSettingsCustomizationConstants
{
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}


TSharedRef<IDetailCustomization> FHTML5TargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FHTML5TargetSettingsCustomization);
}

FHTML5TargetSettingsCustomization::FHTML5TargetSettingsCustomization()
{
}

FHTML5TargetSettingsCustomization::~FHTML5TargetSettingsCustomization()
{
}

void FHTML5TargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& EmscriptenCategory = DetailLayout.EditCategory(TEXT("Emscripten"));

#define SETUP_SOURCEONLY_PROP(PropName, Category) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UHTML5TargetSettings, PropName)); \
		Category.AddProperty(PropertyHandle) \
			.IsEnabled(FEngineBuildSettings::IsSourceDistribution()) \
			.ToolTip(FEngineBuildSettings::IsSourceDistribution() ? PropertyHandle->GetToolTipText() : FHTML5TargetSettingsCustomizationConstants::DisabledTip); \
	}

	SETUP_SOURCEONLY_PROP(EnableIndexedDB, EmscriptenCategory);

	AudioPluginWidgetManager.BuildAudioCategory(DetailLayout, EAudioPlatform::HTML5);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
