// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IClassViewerFilter;
class IPropertyHandle;

/** Delegate used with the Class Viewer in 'class picking' mode.  You'll bind a delegate when the
    class viewer widget is created, which will be fired off when a class is selected in the list */
DECLARE_DELEGATE_OneParam( FOnClassPicked, UClass* );

namespace EClassViewerMode
{
	enum Type
	{
		/** Allows all classes to be browsed and selected; syncs selection with the editor; drag and drop attachment, etc. */
		ClassBrowsing,

		/** Sets the class viewer to operate as a class 'picker'. */
		ClassPicker,
	};
}

namespace EClassViewerDisplayMode
{
	enum Type
	{
		/** Default will choose what view mode based on if in Viewer or Picker mode. */
		DefaultView,

		/** Displays all classes as a tree. */
		TreeView,

		/** Displays all classes as a list. */
		ListView,
	};
}

/**
 * Settings for the Class Viewer set by the programmer before spawning an instance of the widget.  This
 * is used to modify the class viewer's behavior in various ways, such as filtering in or out specific classes.
 */
class FClassViewerInitializationOptions
{

public:
	/** The filter to use on classes in this instance. */
	TSharedPtr<class IClassViewerFilter> ClassFilter;

	/** Mode to operate in */
	EClassViewerMode::Type Mode;

	/** Mode to display the classes using. */
	EClassViewerDisplayMode::Type DisplayMode;

	/** Filters so only actors will be displayed. */
	bool bIsActorsOnly;

	/** Filters so only placeable actors will be displayed. Forces bIsActorsOnly to true. */
	bool bIsPlaceableOnly;

	/** Filters so only base blueprints will be displayed. */
	bool bIsBlueprintBaseOnly;

	/** Shows unloaded blueprints. Will not be filtered out based on non-bool filter options. */
	bool bShowUnloadedBlueprints;

	/** Shows a "None" option, only available in Picker mode. */
	bool bShowNoneOption;

	/** true will show the UObject root class. */
	bool bShowObjectRootClass;

	/** If true, root nodes will be expanded by default. */
	bool bExpandRootNodes;

	/** true allows class dynamic loading on selection */
	bool bEnableClassDynamicLoading;

	/** true shows display names of classes rather than full class names */
	bool bShowDisplayNames;

	/** the title string of the class viewer if required. */
	FText ViewerTitleString;

	/** The property this class viewer be working on. */
	TSharedPtr<class IPropertyHandle> PropertyHandle;

	/** true (the default) shows the view options at the bottom of the class picker*/
	bool bAllowViewOptions;
public:

	/** Constructor */
	FClassViewerInitializationOptions()	
		: Mode( EClassViewerMode::ClassPicker )
		, DisplayMode(EClassViewerDisplayMode::DefaultView)
		, bIsActorsOnly(false)
		, bIsPlaceableOnly(false)
		, bIsBlueprintBaseOnly(false)
		, bShowUnloadedBlueprints(true)
		, bShowNoneOption(false)
		, bShowObjectRootClass(false)
		, bExpandRootNodes(true)
		, bEnableClassDynamicLoading(true)
		, bShowDisplayNames(false)
		, bAllowViewOptions(true)
	{
	}
};

/**
 * Class Viewer module
 */
class FClassViewerModule : public IModuleInterface
{

public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();

	/**
	 * Creates a class viewer widget
	 *
	 * @param	InitOptions						Programmer-driven configuration for this widget instance
	 * @param	OnClassPickedDelegate			Optional callback when a class is selected in 'class picking' mode
	 *
	 * @return	New class viewer widget
	 */
	virtual TSharedRef<class SWidget> CreateClassViewer(const FClassViewerInitializationOptions& InitOptions,
		const FOnClassPicked& OnClassPickedDelegate );

};
