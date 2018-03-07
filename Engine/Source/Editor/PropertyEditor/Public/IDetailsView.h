// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "PropertyEditorDelegates.h"
#include "Framework/Commands/UICommandList.h"

class AActor;
class FNotifyHook;
class FPropertyPath;
class FTabManager;
class IDetailKeyframeHandler;
class IDetailPropertyExtensionHandler;
class IDetailRootObjectCustomization;
class IPropertyTypeIdentifier;


enum class EEditDefaultsOnlyNodeVisibility : uint8
{
	/** Always show nodes that have the EditDefaultsOnly aka CPF_DisableEditOnInstance flag. */
	Show,
	/** Always hide nodes that have the EditDefaultsOnly aka CPF_DisableEditOnInstance flag. */
	Hide,
	/** Let the details panel control it. If the CDO is selected EditDefaultsOnly nodes will be visible, otherwise false. */
	Automatic,
};


/**
 * Init params for a details view widget
 */
struct FDetailsViewArgs
{
	enum ENameAreaSettings
	{
		/** The name area should never be displayed */
		HideNameArea,
		/** All object types use name area */
		ObjectsUseNameArea,
		/** Only Actors use name area */
		ActorsUseNameArea,
		/** Components and actors use the name area. Components will display their actor owner as the name */
		ComponentsAndActorsUseNameArea,
	};

	/** Controls how CPF_DisableEditOnInstance nodes will be treated */
	EEditDefaultsOnlyNodeVisibility DefaultsOnlyVisibility;
	/** The command list from the host of the details view, allowing child widgets to bind actions with a bound chord */
	TSharedPtr<class FUICommandList> HostCommandList;
	/** The tab manager from the host of the details view, allowing child widgets to spawn tabs */
	TSharedPtr<FTabManager> HostTabManager;

	/** Identifier for this details view; NAME_None if this view is anonymous */
	FName ViewIdentifier;
	/** Notify hook to call when properties are changed */
	FNotifyHook* NotifyHook;
	/** Settings for displaying the name area */
	ENameAreaSettings NameAreaSettings;
	/** True if the viewed objects updates from editor selection */
	uint32 bUpdatesFromSelection : 1;
	/** True if this property view can be locked */
	uint32 bLockable : 1;
	/** True if we allow searching */
	uint32 bAllowSearch : 1;
	/** True if you want to not show the tip when no objects are selected (should only be used if viewing actors properties or bObjectsUseNameArea is true ) */
	uint32 bHideSelectionTip : 1;
	/** True if you want the search box to have initial keyboard focus */
	uint32 bSearchInitialKeyFocus : 1;
	/** True if the 'Open Selection in Property Matrix' button should be shown */
	uint32 bShowPropertyMatrixButton : 1;
	/** Allow options to be changed */
	uint32 bShowOptions : 1;
	/** True if you want to show the 'Show Only Modified Properties'. Only valid in conjunction with bShowOptions */
	uint32 bShowModifiedPropertiesOption : 1;
	/** True if you want to show the actor label */
	uint32 bShowActorLabel : 1;
	/** Bind this delegate to hide differing properties */
	uint32 bShowDifferingPropertiesOption : 1;
	/** If true, the name area will be created but will not be displayed so it can be placed in a custom location.  */
	uint32 bCustomNameAreaLocation : 1;
	/** If true, the filter area will be created but will not be displayed so it can be placed in a custom location.  */
	uint32 bCustomFilterAreaLocation : 1;
	/** If false, the current properties editor will never display the favorite system */
	uint32 bAllowFavoriteSystem : 1;
	/** If true the details panel will assume each object passed in through SetObjects will be a unique object shown in the tree and not combined with other objects */
	uint32 bAllowMultipleTopLevelObjects : 1;
	/** If false, the details panel's scrollbar will always be hidden. Useful when embedding details panels in widgets that either grow to accommodate them, or with scrollbars of their own. */
	uint32 bShowScrollBar : 1;

public:
	/** Default constructor */
	FDetailsViewArgs( const bool InUpdateFromSelection = false
					, const bool InLockable = false
					, const bool InAllowSearch = true
					, const ENameAreaSettings InNameAreaSettings = ActorsUseNameArea
					, const bool InHideSelectionTip = false
					, FNotifyHook* InNotifyHook = NULL
					, const bool InSearchInitialKeyFocus = false
					, FName InViewIdentifier = NAME_None )
		: DefaultsOnlyVisibility(EEditDefaultsOnlyNodeVisibility::Show)
		, ViewIdentifier( InViewIdentifier )
		, NotifyHook( InNotifyHook )
		, NameAreaSettings( InNameAreaSettings )
		, bUpdatesFromSelection( InUpdateFromSelection )
		, bLockable(InLockable)
		, bAllowSearch( InAllowSearch )
		, bHideSelectionTip( InHideSelectionTip )
		, bSearchInitialKeyFocus( InSearchInitialKeyFocus )
		, bShowPropertyMatrixButton(true)
		, bShowOptions( true )
		, bShowModifiedPropertiesOption(true)
		, bShowActorLabel(true)
		, bShowDifferingPropertiesOption(false)
		, bCustomNameAreaLocation(false)
		, bCustomFilterAreaLocation(false)
		, bAllowFavoriteSystem(true)
		, bAllowMultipleTopLevelObjects(false)
		, bShowScrollBar(true)
	{
	}
};


/**
 * Interface class for all detail views
 */
class IDetailsView : public SCompoundWidget
{
public:
	/** Sets the callback for when the property view changes */
	virtual void SetOnObjectArrayChanged(FOnObjectArrayChanged OnObjectArrayChangedDelegate) = 0;

	/** List of all selected objects we are inspecting */
	virtual const TArray< TWeakObjectPtr<UObject> >& GetSelectedObjects() const = 0;

	/** @return	Returns list of selected actors we are inspecting */
	virtual const TArray< TWeakObjectPtr<AActor> >& GetSelectedActors() const = 0;

	/** @return Returns information about the selected set of actors */
	virtual const struct FSelectedActorInfo& GetSelectedActorInfo() const = 0;

	/** @return Whether or not the details view is viewing a CDO */
	virtual bool HasClassDefaultObject() const = 0;

	/**
	 * Registers a custom detail layout delegate for a specific class in this instance of the details view only
	 *
	 * @param Class	The class the custom detail layout is for
	 * @param DetailLayoutDelegate	The delegate to call when querying for custom detail layouts for the classes properties
	 */
	virtual void RegisterInstancedCustomPropertyLayout(UStruct* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate) = 0;
	virtual void RegisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr) = 0; 

	/**
	 * Unregisters a custom detail layout delegate for a specific class in this instance of the details view only
	 *
	 * @param Class	The class with the custom detail layout delegate to remove
	 */
	virtual void UnregisterInstancedCustomPropertyLayout(UStruct* Class) = 0;
	virtual void UnregisterInstancedCustomPropertyTypeLayout(FName PropertyTypeName, TSharedPtr<IPropertyTypeIdentifier> Identifier = nullptr) = 0;

	/**
	 * Registers a customization that will be used only if this details panel contains multiple top level objects.
	 * I.E it was created with bAllowMultipleTopLevelObjects = true.	This interface will be used to customize the header for each top level object in the details panel
	 *
	 * @param InRootObjectCustomization	If null is passed in, the customization will be removed
	 */
	virtual void SetRootObjectCustomizationInstance(TSharedPtr<class IDetailRootObjectCustomization> InRootObjectCustomization) = 0;

	/**
	 * Sets the objects this details view is viewing
	 *
	 * @param InObjects		The list of objects to observe
	 * @param bForceRefresh	If true, doesn't check if new objects are being set
	 * @param bOverrideLock	If true, will set the objects even if the details view is locked
	 */
	virtual void SetObjects( const TArray<UObject*>& InObjects, bool bForceRefresh = false, bool bOverrideLock = false ) = 0;
	virtual void SetObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects, bool bForceRefresh = false, bool bOverrideLock = false ) = 0;

	/**
	 * Sets a single object that details view is viewing
	 *
	 * @param InObject		The object to view
	 * @param bForceRefresh	If true, doesn't check if new objects are being set
	 */
	virtual void SetObject( UObject* InObject, bool bForceRefresh = false ) = 0;

	/** Removes all invalid objects being observed by this details panel */
	virtual void RemoveInvalidObjects() = 0;

	/** Set overrides that should be used when looking for packages that contain the given object (used when editing a transient copy of an object, but you need access to th real package) */
	virtual void SetObjectPackageOverrides(const TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UPackage>>& InMapping) = 0;

	/**
	 * Returns true if the details view is locked and cant have its observed objects changed 
	 */
	virtual bool IsLocked() const = 0;

	/**
	 * @return true if the details view can be updated from editor selection
	 */
	virtual bool IsUpdatable() const = 0;

	/**
	 * @return True if there is any filter of properties active in this details panel
	 */
	virtual bool HasActiveSearch() const = 0;
	
	/** 
	 * Clears any search terms in the current filter
	 */
	virtual void ClearSearch() = 0;

	/**
	 * @return The number of visible top level objects. This value is affected by filtering. 
	 * Note: this value will always be 1 if this details panel was not created with bAllowMultipleTopLevelObjects=true
	 */
	virtual int32 GetNumVisibleTopLevelObjects() const = 0;

	/** @return The identifier for this details view, or NAME_None is this view is anonymous */
	virtual FName GetIdentifier() const = 0;

	/**
	 * Sets a delegate to call to determine if a specific property should be visible in this instance of the details view
	 */ 
	virtual void SetIsPropertyVisibleDelegate( FIsPropertyVisible InIsPropertyVisible ) = 0;
	virtual FIsPropertyVisible& GetIsPropertyVisibleDelegate() = 0;

	/**
	 * Sets a delegate to call to determine if a specific property should be read-only in this instance of the details view
	 */ 
	virtual void SetIsPropertyReadOnlyDelegate( FIsPropertyReadOnly InIsPropertyReadOnly ) = 0;
	virtual FIsPropertyReadOnly& GetIsPropertyReadOnlyDelegate() = 0;

	/**
	 * Sets a delegate to call to layout generic details not specific to an object being viewed
	 */ 
	virtual void SetGenericLayoutDetailsDelegate( FOnGetDetailCustomizationInstance OnGetGenericDetails ) = 0;
	virtual FOnGetDetailCustomizationInstance& GetGenericLayoutDetailsDelegate() = 0;

	/**
	 * Sets a delegate to call to determine if the properties  editing is enabled
	 */ 
	virtual void SetIsPropertyEditingEnabledDelegate( FIsPropertyEditingEnabled IsPropertyEditingEnabled ) = 0;
	virtual FIsPropertyEditingEnabled& GetIsPropertyEditingEnabledDelegate() = 0;

	virtual void SetKeyframeHandler( TSharedPtr<class IDetailKeyframeHandler> InKeyframeHandler ) = 0;

	virtual TSharedPtr<IDetailKeyframeHandler> GetKeyframeHandler() = 0;

	virtual void SetExtensionHandler(TSharedPtr<class IDetailPropertyExtensionHandler> InExtensionandler) = 0;

	/**
	 * @return true if property editing is enabled (based on the FIsPropertyEditingEnabled delegate)
	 */ 
	virtual bool IsPropertyEditingEnabled() const = 0;

	/**
	 * A delegate which is called after properties have been edited and PostEditChange has been called on all objects.
	 * This can be used to safely make changes to data that the details panel is observing instead of during PostEditChange (which is
	 * unsafe)
	 */
	virtual FOnFinishedChangingProperties& OnFinishedChangingProperties() const = 0;

	/** 
	 * Sets the visible state of the filter box/property grid area
	 */
	virtual void HideFilterArea(bool bIsVisible) = 0;

	/**
	 * Returns a list of all the properties displayed (via full path), order in list corresponds to draw order:
	 */
	virtual TArray< FPropertyPath > GetPropertiesInOrderDisplayed() const = 0;

	/**
	 * Creates a box around the treenode corresponding to Property and scrolls the treenode into view
	 */
	virtual void HighlightProperty(const FPropertyPath& Property) = 0;
	
	/**
	 * Forces all advanced property sections to be in expanded state:
	 */
	virtual void ShowAllAdvancedProperties() = 0;
	
	/**
	 * Refreshes the visibility of root objects in this details view. 
	 * Note: This method has no effect if the details panel is viewing a single top-level object set only
	 */
	virtual void RefreshRootObjectVisibility() = 0;
	/**
	 * Assigns delegate called when view is filtered, useful for updating external control logic:
	 */
	virtual void SetOnDisplayedPropertiesChanged(FOnDisplayedPropertiesChanged InOnDisplayedPropertiesChangedDelegate) = 0;
	virtual FOnDisplayedPropertiesChanged& GetOnDisplayedPropertiesChanged() = 0;

	/**
	 * Disables or enables customization of the details view:
	 */
	virtual void SetDisableCustomDetailLayouts(bool bInDisableCustomDetailLayouts) = 0;

	/**
	 * Sets the set of properties that are considered differing, used when filtering out identical properties
	 */
	virtual void UpdatePropertiesWhitelist(const TSet<FPropertyPath> InWhitelistedProperties) = 0;

	/** Returns the name area widget used to display object naming functionality so it can be placed in a custom location.  Note FDetailsViewArgs.bCustomNameAreaLocation must be true */
	virtual TSharedPtr<SWidget> GetNameAreaWidget() = 0;

	/** Returns the search area widget used to display search and view options so it can be placed in a custom location.  Note FDetailsViewArgs.bCustomFilterAreaLocation must be true */
	virtual TSharedPtr<SWidget> GetFilterAreaWidget() = 0;

	/** Returns the command list of the hosting toolkit (can be nullptr if the widget that contains the details panel didn't route a command list in) */
	virtual TSharedPtr<class FUICommandList> GetHostCommandList() const = 0;

	/** Returns the tab manager of the hosting toolkit (can be nullptr if the details panel is not hosted within a tab) */
	virtual TSharedPtr<FTabManager> GetHostTabManager() const = 0;

	/** Sets the tab manager of the hosting toolkit (can be nullptr if the details panel is not hosted within a tab) */
	virtual void SetHostTabManager(TSharedPtr<FTabManager> InTabManager) = 0;

	/** Force refresh */
	virtual void ForceRefresh() = 0;
};
