// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Models/TextureEditorCommands.h"

#define LOCTEXT_NAMESPACE "TextureEditorViewOptionsMenu"

/**
 * Static helper class for populating the "View Options" menu in the texture editor's view port.
 */
class FTextureEditorViewOptionsMenu
{
public:

	/**
	 * Creates the menu.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void MakeMenu( FMenuBuilder& MenuBuilder )
	{
		// color channel options
		MenuBuilder.BeginSection("ChannelSection", LOCTEXT("ChannelsSectionHeader", "Color Channels"));
		{
			MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().RedChannel);
			MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().GreenChannel);
			MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().BlueChannel);
			MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().AlphaChannel);
			MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().Desaturation);
		}
		MenuBuilder.EndSection();

		// view port options
		MenuBuilder.BeginSection("ViewportSection", LOCTEXT("ViewportSectionHeader", "Viewport Options"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("Background", "Background"),
				LOCTEXT("BackgroundTooltip", "Set the viewport's background"),
				FNewMenuDelegate::CreateStatic(&FTextureEditorViewOptionsMenu::GenerateBackgroundMenuContent)
			);

			MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().TextureBorder);
			MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().FitToViewport);
		}
		MenuBuilder.EndSection();

		MenuBuilder.AddMenuSeparator();
		MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().Settings);
	}

protected:

	/**
	 * Creates the 'Background' sub-menu.
	 *
	 * @param MenuBuilder The builder for the menu that owns this menu.
	 */
	static void GenerateBackgroundMenuContent( FMenuBuilder& MenuBuilder )
	{
		MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().CheckeredBackground);
		MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().CheckeredBackgroundFill);
		MenuBuilder.AddMenuEntry(FTextureEditorCommands::Get().SolidBackground);
	}
};


#undef LOCTEXT_NAMESPACE
