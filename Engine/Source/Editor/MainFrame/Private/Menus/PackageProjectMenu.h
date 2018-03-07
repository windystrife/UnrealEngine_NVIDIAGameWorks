// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Misc/Paths.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Frame/MainFrameActions.h"
#include "HAL/FileManager.h"
#include "GameProjectGenerationModule.h"
#include "PlatformInfo.h"
#include "Interfaces/IProjectTargetPlatformEditorModule.h"
#include "Interfaces/IProjectManager.h"
#include "InstalledPlatformInfo.h"

#define LOCTEXT_NAMESPACE "FPackageProjectMenu"

/**
 * Static helper class for populating the "Package Project" menu.
 */
class FPackageProjectMenu
{
public:

	/**
	 * Creates the menu.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void MakeMenu( FMenuBuilder& MenuBuilder )
	{
		const TArray<FString>& ConfidentalPlatforms = FPlatformMisc::GetConfidentialPlatforms();

		TArray<PlatformInfo::FVanillaPlatformEntry> VanillaPlatforms = PlatformInfo::BuildPlatformHierarchy(PlatformInfo::EPlatformFilter::All);
		if (!VanillaPlatforms.Num())
		{
			return;
		}

		VanillaPlatforms.Sort([](const PlatformInfo::FVanillaPlatformEntry& One, const PlatformInfo::FVanillaPlatformEntry& Two) -> bool
		{
			return One.PlatformInfo->DisplayName.CompareTo(Two.PlatformInfo->DisplayName) < 0;
		});

		IProjectTargetPlatformEditorModule& ProjectTargetPlatformEditorModule = FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor");
		EProjectType ProjectType = FGameProjectGenerationModule::Get().ProjectHasCodeFiles() ? EProjectType::Code : EProjectType::Content;

		// Build up a menu from the tree of platforms
		for (const PlatformInfo::FVanillaPlatformEntry& VanillaPlatform : VanillaPlatforms)
		{
			check(VanillaPlatform.PlatformInfo->IsVanilla());

			// Only care about game targets
			if (VanillaPlatform.PlatformInfo->PlatformType != PlatformInfo::EPlatformType::Game || !VanillaPlatform.PlatformInfo->bEnabledForUse || !FInstalledPlatformInfo::Get().CanDisplayPlatform(VanillaPlatform.PlatformInfo->BinaryFolderName, ProjectType))
			{
				continue;
			}

			// Make sure we're able to run this platform
			if (VanillaPlatform.PlatformInfo->bIsConfidential && !ConfidentalPlatforms.Contains(VanillaPlatform.PlatformInfo->IniPlatformName))
			{
				continue;
			}

			if (VanillaPlatform.PlatformFlavors.Num())
			{
				MenuBuilder.AddSubMenu(
					ProjectTargetPlatformEditorModule.MakePlatformMenuItemWidget(*VanillaPlatform.PlatformInfo), 
					FNewMenuDelegate::CreateStatic(&FPackageProjectMenu::AddPlatformSubPlatformsToMenu, VanillaPlatform.PlatformFlavors),
					false
					);
			}
			else
			{
				AddPlatformToMenu(MenuBuilder, *VanillaPlatform.PlatformInfo);
			}
		}

		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().ZipUpProject);

		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddSubMenu(
			LOCTEXT("PackageProjectBuildConfigurationSubMenuLabel", "Build Configuration"),
			LOCTEXT("PackageProjectBuildConfigurationSubMenuToolTip", "Select the build configuration to package the project with"),
			FNewMenuDelegate::CreateStatic(&FPackageProjectMenu::MakeBuildConfigurationsMenu)
		);

		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddMenuEntry(FMainFrameCommands::Get().PackagingSettings);

		ProjectTargetPlatformEditorModule.AddOpenProjectTargetPlatformEditorMenuItem(MenuBuilder);
	}

protected:

	/**
	 * Creates the platform menu entries.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 * @param Platform The target platform we allow packaging for
	 */
	static void AddPlatformToMenu(FMenuBuilder& MenuBuilder, const PlatformInfo::FPlatformInfo& PlatformInfo)
	{
		EProjectType ProjectType = FGameProjectGenerationModule::Get().ProjectHasCodeFiles() ? EProjectType::Code : EProjectType::Content;

		// don't add sub-platforms that can't be displayed in an installed build
		if (!FInstalledPlatformInfo::Get().CanDisplayPlatform(PlatformInfo.BinaryFolderName, ProjectType))
		{
			return;
		}

		IProjectTargetPlatformEditorModule& ProjectTargetPlatformEditorModule = FModuleManager::LoadModuleChecked<IProjectTargetPlatformEditorModule>("ProjectTargetPlatformEditor");

		FUIAction Action(
			FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageProject, PlatformInfo.PlatformInfoName),
			FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageProjectCanExecute, PlatformInfo.PlatformInfoName)
			);

		// ... generate tooltip text
		FFormatNamedArguments TooltipArguments;
		TooltipArguments.Add(TEXT("DisplayName"), PlatformInfo.DisplayName);
		FText Tooltip = FText::Format(LOCTEXT("PackageGameForPlatformTooltip", "Build, cook and package your game for the {DisplayName} platform"), TooltipArguments);

		FProjectStatus ProjectStatus;
		if (IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus) && !ProjectStatus.IsTargetPlatformSupported(PlatformInfo.VanillaPlatformName))
		{
			FText TooltipLine2 = FText::Format(LOCTEXT("PackageUnsupportedPlatformWarning", "{DisplayName} is not listed as a target platform for this project, so may not run as expected."), TooltipArguments);
			Tooltip = FText::Format(FText::FromString(TEXT("{0}\n\n{1}")), Tooltip, TooltipLine2);
		}

		// ... and add a menu entry
		MenuBuilder.AddMenuEntry(
			Action, 
			ProjectTargetPlatformEditorModule.MakePlatformMenuItemWidget(PlatformInfo), 
			NAME_None, 
			Tooltip
			);
	}

	/**
	 * Creates the platform menu entries for a given platforms sub-platforms.
	 * e.g. Windows has multiple sub-platforms - Win32 and Win64
	 *
	 * @param MenuBuilderThe builder for the menu that owns this menu.
	 * @param SubPlatformInfos The Sub-platform information
	 */
	static void AddPlatformSubPlatformsToMenu(FMenuBuilder& MenuBuilder, TArray<const PlatformInfo::FPlatformInfo*> SubPlatformInfos)
	{
		for (const PlatformInfo::FPlatformInfo* SubPlatformInfo : SubPlatformInfos)
		{
			AddPlatformToMenu(MenuBuilder, *SubPlatformInfo);
		}
	}

	/**
	 * Creates a build configuration sub-menu.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void MakeBuildConfigurationsMenu(FMenuBuilder& MenuBuilder)
	{
		// Only show the debug game configurations if the game has source code. 
		TArray<FString> TargetFileNames;
		IFileManager::Get().FindFiles(TargetFileNames, *(FPaths::GameSourceDir() / TEXT("*.target.cs")), true, false);
		
		if (TargetFileNames.Num() > 0)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DebugConfiguration", "DebugGame"),
				LOCTEXT("DebugConfigurationTooltip", "Package the project for debugging"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfiguration, PPBC_DebugGame),
					FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CanPackageBuildConfiguration, PPBC_DebugGame),
					FIsActionChecked::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfigurationIsChecked, PPBC_DebugGame)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		// Add the Client configurations if there is a {ProjectName}Client.Target.cs file.
		TArray<FString> ClientTargetFileNames;
		IFileManager::Get().FindFiles(ClientTargetFileNames, *(FPaths::GameSourceDir() / TEXT("*client.target.cs")), true, false);

		if (ClientTargetFileNames.Num() > 0)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DebugClientConfiguration", "DebugGame Client"),
				LOCTEXT("DebugClientConfigurationTooltip", "Package the project for debugging as a client"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfiguration, PPBC_DebugGameClient),
					FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CanPackageBuildConfiguration, PPBC_DebugGameClient),
					FIsActionChecked::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfigurationIsChecked, PPBC_DebugGameClient)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DevelopmentConfiguration", "Development"),
			LOCTEXT("DevelopmentConfigurationTooltip", "Package the project for development"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfiguration, PPBC_Development),
				FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CanPackageBuildConfiguration, PPBC_Development),
				FIsActionChecked::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfigurationIsChecked, PPBC_Development)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		if (ClientTargetFileNames.Num() > 0)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DevelopmentClientConfiguration", "Development Client"),
				LOCTEXT("DevelopmentClientConfigurationTooltip", "Package the project for development as a client"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfiguration, PPBC_DevelopmentClient),
					FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CanPackageBuildConfiguration, PPBC_DevelopmentClient),
					FIsActionChecked::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfigurationIsChecked, PPBC_DevelopmentClient)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShippingConfiguration", "Shipping"),
			LOCTEXT("ShippingConfigurationTooltip", "Package the project for shipping"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfiguration, PPBC_Shipping),
				FCanExecuteAction(),
				FIsActionChecked::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfigurationIsChecked, PPBC_Shipping)
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		if (ClientTargetFileNames.Num() > 0)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShippingClientConfiguration", "Shipping Client"),
				LOCTEXT("ShippingClientConfigurationTooltip", "Package the project for shipping as a client"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfiguration, PPBC_ShippingClient),
					FCanExecuteAction(),
					FIsActionChecked::CreateStatic(&FMainFrameActionCallbacks::PackageBuildConfigurationIsChecked, PPBC_ShippingClient)
				),
				NAME_None,
				EUserInterfaceActionType::RadioButton
			);
		}

	}
};


#undef LOCTEXT_NAMESPACE
