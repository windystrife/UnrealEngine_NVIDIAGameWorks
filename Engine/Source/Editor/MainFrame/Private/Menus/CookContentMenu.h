// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AssertionMacros.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Modules/ModuleManager.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameProjectGenerationModule.h"
#include "PlatformInfo.h"
#include "Frame/MainFrameActions.h"
#include "Interfaces/IProjectTargetPlatformEditorModule.h"
#include "Interfaces/IProjectManager.h"
#include "InstalledPlatformInfo.h"


#define LOCTEXT_NAMESPACE "FCookContentMenu"

class FMenuBuilder;

/**
 * Static helper class for populating the "Cook Content" menu.
 */
class FCookContentMenu
{
public:

	/**
	 * Creates the menu.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void MakeMenu( FMenuBuilder& MenuBuilder )
	{
		TArray<PlatformInfo::FVanillaPlatformEntry> VanillaPlatforms = PlatformInfo::BuildPlatformHierarchy(PlatformInfo::EPlatformFilter::CookFlavor);
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

			// We now make sure we're able to run this platform at the command stage so we can fire off SDK tutorials
/*			ITargetPlatform* const Platform = GetTargetPlatformManager()->FindTargetPlatform(VanillaPlatform.PlatformInfo->TargetPlatformName.ToString());
			if (!Platform)
			{
				continue;
			}*/

			if(VanillaPlatform.PlatformFlavors.Num())
			{
				MenuBuilder.AddSubMenu(
					ProjectTargetPlatformEditorModule.MakePlatformMenuItemWidget(*VanillaPlatform.PlatformInfo), 
					FNewMenuDelegate::CreateStatic(&FCookContentMenu::AddPlatformSubPlatformsToMenu, VanillaPlatform.PlatformFlavors),
					false
					);
			}
			else
			{
				AddPlatformToMenu(MenuBuilder, *VanillaPlatform.PlatformInfo);
			}
		}
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
			FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CookContent, PlatformInfo.PlatformInfoName),
			FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CookContentCanExecute, PlatformInfo.PlatformInfoName)
		);

		// ... generate tooltip text
		FFormatNamedArguments TooltipArguments;
		TooltipArguments.Add(TEXT("DisplayName"), PlatformInfo.DisplayName);
		FText Tooltip = FText::Format(LOCTEXT("CookContentForPlatformTooltip", "Cook your game content for the {DisplayName} platform"), TooltipArguments);

		FProjectStatus ProjectStatus;
		if (IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus) && !ProjectStatus.IsTargetPlatformSupported(PlatformInfo.VanillaPlatformName))
		{
			FText TooltipLine2 = FText::Format(LOCTEXT("CookUnsupportedPlatformWarning", "{DisplayName} is not listed as a target platform for this project, so may not run as expected."), TooltipArguments);
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
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 * @param SubPlatformInfos The Sub-platform information
	 */
	static void AddPlatformSubPlatformsToMenu(FMenuBuilder& MenuBuilder, TArray<const PlatformInfo::FPlatformInfo*> SubPlatformInfos)
	{
		for (const PlatformInfo::FPlatformInfo* SubPlatformInfo : SubPlatformInfos)
		{
			AddPlatformToMenu(MenuBuilder, *SubPlatformInfo);
		}
	}
};


#undef LOCTEXT_NAMESPACE
