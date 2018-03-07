// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Extension position enumeration */
enum class ELayoutExtensionPosition
{
	/** Extend the layout before the specified element */
	Before,
	/** Extend the layout after the specified element */
	After
};

/** Class used for extending default layouts */
class FLayoutExtender : public TSharedFromThis<FLayoutExtender>
{
public:

	/**
	 * Extend the layout by defining a tab before or after the specified predicate tab ID
	 *
	 * @param PredicateTabId		The existing tab to place the extended tab before/after
	 * @param Position				Whether to place the new tab before or after this tab
	 * @param TabToAdd				The new tab definition
	 */
	void ExtendLayout(FTabId PredicateTabId, ELayoutExtensionPosition Position, FTabManager::FTab TabToAdd)
	{
		TabExtensions.Add(PredicateTabId, FExtendedTab(Position, TabToAdd));
	}


	/**
	 * Populate the specified container with the tabs that relate to the specified tab ID
	 *
	 * @param TabId					The existing tab that may be extended
	 * @param Position				The position to acquire extensions for (before/after)
	 * @param OutValues				The container to populate with extended tabs
	 */
	template<typename T>
	void FindExtensions(FTabId TabId, ELayoutExtensionPosition Position, T& OutValues) const
	{
		OutValues.Reset();

		TArray<FExtendedTab, TInlineAllocator<2>> Extensions;
		TabExtensions.MultiFind(TabId, Extensions, true);

		for (FExtendedTab& Extension : Extensions)
		{
			if (Extension.Position == Position)
			{
				OutValues.Add(Extension.TabToAdd);
			}
		}
	}

private:
	
	/** Extended tab information */
	struct FExtendedTab
	{
		FExtendedTab(ELayoutExtensionPosition InPosition, const FTabManager::FTab& InTabToAdd)
			: Position(InPosition), TabToAdd(InTabToAdd)
		{}

		/** The tab position */
		ELayoutExtensionPosition Position;
		/** The tab definition */
		FTabManager::FTab TabToAdd;
	};

	/** Map of extensions */
	TMultiMap<FTabId, FExtendedTab> TabExtensions;
};