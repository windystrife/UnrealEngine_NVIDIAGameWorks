// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MacTargetSettingsDetails.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorDirectories.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "SExternalImageReference.h"
#include "SNumericDropDown.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/Notifications/SErrorText.h"
#include "IDetailPropertyRow.h"

namespace MacTargetSettingsDetailsConstants
{
	/** The filename for the game splash screen */
	const FString GameSplashFileName(TEXT("Splash/Splash.bmp"));

	/** The filename for the editor splash screen */
	const FString EditorSplashFileName(TEXT("Splash/EdSplash.bmp"));
}

#define LOCTEXT_NAMESPACE "MacTargetSettingsDetails"

TSharedRef<IDetailCustomization> FMacTargetSettingsDetails::MakeInstance()
{
	return MakeShareable(new FMacTargetSettingsDetails);
}

namespace EMacImageScope
{
	enum Type
	{
		Engine,
		GameOverride
	};
}

/* Helper function used to generate filenames for splash screens */
static FString GetSplashFilename(EMacImageScope::Type Scope, bool bIsEditorSplash)
{
	FString Filename;

	if (Scope == EMacImageScope::Engine)
	{
		Filename = FPaths::EngineContentDir();
	}
	else
	{
		Filename = FPaths::ProjectContentDir();
	}

	if(bIsEditorSplash)
	{
		Filename /= MacTargetSettingsDetailsConstants::EditorSplashFileName;
	}
	else
	{
		Filename /= MacTargetSettingsDetailsConstants::GameSplashFileName;
	}

	Filename = FPaths::ConvertRelativePathToFull(Filename);

	return Filename;
}

/* Helper function used to generate filenames for icons */
static FString GetIconFilename(EMacImageScope::Type Scope)
{
	const FString& PlatformName = FModuleManager::GetModuleChecked<ITargetPlatformModule>("MacTargetPlatform").GetTargetPlatform()->PlatformName();

	if (Scope == EMacImageScope::Engine)
	{
		FString Filename = FPaths::EngineDir() / FString(TEXT("Source/Runtime/Launch/Resources")) / PlatformName / FString("UE4.icns");
		return FPaths::ConvertRelativePathToFull(Filename);
	}
	else
	{
		FString Filename = FPaths::ProjectDir() / TEXT("Build/Mac/Application.icns");
		if(!FPaths::FileExists(Filename))
		{
			FString LegacyFilename = FPaths::GameSourceDir() / FString(FApp::GetProjectName()) / FString(TEXT("Resources")) / PlatformName / FString(FApp::GetProjectName()) + TEXT(".icns");
			if(FPaths::FileExists(LegacyFilename))
			{
				Filename = LegacyFilename;
			}
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}
}

void FMacTargetSettingsDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FSimpleDelegate OnUpdateShaderStandardWarning = FSimpleDelegate::CreateSP(this, &FMacTargetSettingsDetails::UpdateShaderStandardWarning);
	
	ITargetPlatform* TargetPlatform = FModuleManager::GetModuleChecked<ITargetPlatformModule>("MacTargetPlatform").GetTargetPlatform();
	
	// Setup the supported/targeted RHI property view
	TargetShaderFormatsDetails = MakeShareable(new FShaderFormatsPropertyDetails(&DetailBuilder, TEXT("TargetedRHIs"), TEXT("Targeted RHIs")));
	TargetShaderFormatsDetails->SetOnUpdateShaderWarning(OnUpdateShaderStandardWarning);
	TargetShaderFormatsDetails->CreateTargetShaderFormatsPropertyView(TargetPlatform);
	
	// Setup the shader version property view
    // Handle max. shader version a little specially.
    {
        IDetailCategoryBuilder& RenderCategory = DetailBuilder.EditCategory(TEXT("Rendering"));
        ShaderVersionPropertyHandle = DetailBuilder.GetProperty(TEXT("MaxShaderLanguageVersion"));
		
		// Drop-downs for setting type of lower and upper bound normalization
		IDetailPropertyRow& ShaderVersionPropertyRow = RenderCategory.AddProperty(ShaderVersionPropertyHandle.ToSharedRef());
		ShaderVersionPropertyRow.CustomWidget()
		.NameContent()
		[
			ShaderVersionPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FMacTargetSettingsDetails::OnGetShaderVersionContent)
				.ContentPadding(FMargin( 2.0f, 2.0f ))
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FMacTargetSettingsDetails::GetShaderVersionDesc)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(2)
			[
				SAssignNew(ShaderVersionWarningTextBox, SErrorText)
				.AutoWrapText(true)
			]
		];
		
		UpdateShaderStandardWarning();
    }
	
	// Add the splash image customization
	const FText EditorSplashDesc(LOCTEXT("EditorSplashLabel", "Editor Splash"));
	IDetailCategoryBuilder& SplashCategoryBuilder = DetailBuilder.EditCategory(TEXT("Splash"));
	FDetailWidgetRow& EditorSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(EditorSplashDesc);

	const FString EditorSplash_TargetImagePath = GetSplashFilename(EMacImageScope::GameOverride, true);
	const FString EditorSplash_DefaultImagePath = GetSplashFilename(EMacImageScope::Engine, true);

	EditorSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(EditorSplashDesc)
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
			SNew(SExternalImageReference, EditorSplash_DefaultImagePath, EditorSplash_TargetImagePath)
			.FileDescription(EditorSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	const FText GameSplashDesc(LOCTEXT("GameSplashLabel", "Game Splash"));
	FDetailWidgetRow& GameSplashWidgetRow = SplashCategoryBuilder.AddCustomRow(GameSplashDesc);

	const FString GameSplash_TargetImagePath = GetSplashFilename(EMacImageScope::GameOverride, false);
	const FString GameSplash_DefaultImagePath = GetSplashFilename(EMacImageScope::Engine, false);

	GameSplashWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(GameSplashDesc)
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
			SNew(SExternalImageReference, GameSplash_DefaultImagePath, GameSplash_TargetImagePath)
			.FileDescription(GameSplashDesc)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	IDetailCategoryBuilder& IconsCategoryBuilder = DetailBuilder.EditCategory(TEXT("Icon"));	
	FDetailWidgetRow& GameIconWidgetRow = IconsCategoryBuilder.AddCustomRow(LOCTEXT("GameIconLabel", "Game Icon"));
	GameIconWidgetRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GameIconLabel", "Game Icon"))
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
			SNew(SExternalImageReference, GetIconFilename(EMacImageScope::Engine), GetIconFilename(EMacImageScope::GameOverride))
			.FileDescription(GameSplashDesc)
			.OnPreExternalImageCopy(FOnPreExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePreExternalIconCopy))
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FMacTargetSettingsDetails::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FMacTargetSettingsDetails::HandlePostExternalIconCopy))
		]
	];

	AudioPluginWidgetManager.BuildAudioCategory(DetailBuilder, EAudioPlatform::Mac);
}


bool FMacTargetSettingsDetails::HandlePreExternalIconCopy(const FString& InChosenImage)
{
	return true;
}


FString FMacTargetSettingsDetails::GetPickerPath()
{
	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}


bool FMacTargetSettingsDetails::HandlePostExternalIconCopy(const FString& InChosenImage)
{
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
	return true;
}

static uint32 GMacTargetSettingsMinOSVers[][3] = {
	{10,11,6},
	{10,11,6},
	{10,12,6},
	{10,13,0}
};

TSharedRef<SWidget> FMacTargetSettingsDetails::OnGetShaderVersionContent()
{
	FMenuBuilder MenuBuilder(true, NULL);
	
	UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMacMetalShaderStandard"), true);
	
	for (int32 i = 0; i < Enum->GetMaxEnumValue(); i++)
	{
		bool bValidTargetForCurrentOS = true;
#if PLATFORM_MAC
		bValidTargetForCurrentOS = FPlatformMisc::MacOSXVersionCompare(GMacTargetSettingsMinOSVers[i][0], GMacTargetSettingsMinOSVers[i][1], GMacTargetSettingsMinOSVers[i][2]) >= 0;
#endif
		if (Enum->IsValidEnumValue(i) && bValidTargetForCurrentOS)
		{
			FUIAction ItemAction(FExecuteAction::CreateSP(this, &FMacTargetSettingsDetails::SetShaderStandard, i));
			MenuBuilder.AddMenuEntry(Enum->GetDisplayNameTextByValue(i), TAttribute<FText>(), FSlateIcon(), ItemAction);
		}
	}
	
	return MenuBuilder.MakeWidget();
}

FText FMacTargetSettingsDetails::GetShaderVersionDesc() const
{
	uint8 EnumValue;
	ShaderVersionPropertyHandle->GetValue(EnumValue);
	
	UEnum* Enum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMacMetalShaderStandard"), true);
	
	if (EnumValue < Enum->GetMaxEnumValue() && Enum->IsValidEnumValue(EnumValue))
	{
		return Enum->GetDisplayNameTextByValue(EnumValue);
	}
	
	return FText::GetEmpty();
}

void FMacTargetSettingsDetails::SetShaderStandard(int32 Value)
{
	FPropertyAccess::Result Res = ShaderVersionPropertyHandle->SetValue((uint8)Value);
	check(Res == FPropertyAccess::Success);
}

void FMacTargetSettingsDetails::UpdateShaderStandardWarning()
{
	// Update the UI
	uint8 EnumValue;
	ShaderVersionPropertyHandle->GetValue(EnumValue);
	SetShaderStandard(EnumValue);
}

#undef LOCTEXT_NAMESPACE
