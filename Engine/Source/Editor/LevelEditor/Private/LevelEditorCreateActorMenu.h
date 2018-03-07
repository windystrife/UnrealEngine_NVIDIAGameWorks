// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;

/**
 * The mode to use when creating an actor
 */
namespace EActorCreateMode
{
	enum Type
	{
		/** Add the actor at the last click location */
		Add,

		/** Replace the actor that was last clicked on */
		Replace,

		/** Temp actor is attached to cursor & placed on click */
		Placement,
	};
}

namespace LevelEditorCreateActorMenu
{
	/**
	 * Fill the context menu section(s) for adding or replacing an actor in the viewport
	 * @param	MenuBuilder		The menu builder used to generate the context menu
	 */
	void FillAddReplaceViewportContextMenuSections( FMenuBuilder& MenuBuilder );

	/**
	 * Fill the context menu for adding or replacing an actor. Used for in-viewport and level editor toolbar menus.
	 * @param	MenuBuilder		The menu builder used to generate the context menu
	 */
	void FillAddReplaceActorMenu( FMenuBuilder& MenuBuilder, EActorCreateMode::Type CreateMode );
};
