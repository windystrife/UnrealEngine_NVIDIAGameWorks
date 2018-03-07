// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/NotifyHook.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "ISettingsCategory.h"
#include "ISettingsContainer.h"
#include "ISettingsEditorModel.h"
#include "ISettingsSection.h"

class IDetailsView;
class SVerticalBox;
struct FPropertyChangedEvent;

/**
 * Implements an editor widget for settings.
 */
class SSettingsEditor
	: public SCompoundWidget
	, public FNotifyHook
{
public:

	SLATE_BEGIN_ARGS(SSettingsEditor) { }
		SLATE_EVENT( FSimpleDelegate, OnApplicationRestartRequired )
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SSettingsEditor();

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The view model.
	 */
	void Construct( const FArguments& InArgs, const ISettingsEditorModelRef& InModel );

public:

	// FNotifyHook interface

	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged ) override;

protected:


	/**
	 * Gets the settings object of the selected section, if any.
	 *
	 * @return The settings object.
	 */
	TWeakObjectPtr<UObject> GetSelectedSettingsObject() const;

	/**
	 * Creates a widget for the given settings category.
	 *
	 * @param Category The category to create the widget for.
	 * @return The created widget.
	 */
	TSharedRef<SWidget> MakeCategoryWidget( const ISettingsCategoryRef& Category, const TArray<ISettingsSectionPtr>& InSections );


	/**
	 * Reports a preference changed event to the analytics system.
	 *
	 * @param SelectedSection The currently selected settings section.
	 * @param PropertyChangedEvent The event for the changed property.
	 */
	void RecordPreferenceChangedAnalytics( ISettingsSectionPtr SelectedSection, const FPropertyChangedEvent& PropertyChangedEvent ) const;

	/** Reloads the settings categories. */
	void ReloadCategories();


private:

	/** Callback for when the user's culture has changed. */
	void HandleCultureChanged();

	/** Callback for changing the selected settings section. */
	void HandleModelSelectionChanged();

	/** Callback for navigating a settings section link. */
	void HandleSectionLinkNavigate( ISettingsSectionPtr Section );

	/** Callback for navigating the all settings link. */
	void HandleAllSectionsLinkNavigate();

	/** Callback for getting the visibility of a section link image. */
	EVisibility HandleSectionLinkImageVisibility( ISettingsSectionPtr Section ) const;

	/** Callback for getting the visibility of a the all sections link image. */
	EVisibility HandleAllSectionsLinkImageVisibility() const;

	/** Callback for determining the visibility of the settings box. */
	EVisibility HandleSettingsBoxVisibility() const;

	/** Callback for the modification of categories in the settings container. */
	void HandleSettingsContainerCategoryModified( const FName& CategoryName );

	/** Callback for checking whether the settings view is enabled. */
	bool HandleSettingsViewEnabled() const;

	/** Callback for determining the visibility of the settings view. */
	EVisibility HandleSettingsViewVisibility() const;

	/** Callback when the timer has been ticked to refresh the categories latently. */
	EActiveTimerReturnType UpdateCategoriesCallback(double InCurrentTime, float InDeltaTime);
private:

	/** Holds the vertical box for settings categories. */
	TSharedPtr<SVerticalBox> CategoriesBox;

	/** Holds the overlay slot for custom widgets. */
	SOverlay::FOverlaySlot* CustomWidgetSlot;
	/** Holds a pointer to the view model. */
	ISettingsEditorModelPtr Model;

	/** Holds the settings container. */
	ISettingsContainerPtr SettingsContainer;

	/** Holds the details view. */
	TSharedPtr<IDetailsView> SettingsView;

	/** Delegate called when this settings editor requests that the user be notified that the application needs to be restarted for some setting changes to take effect */
	FSimpleDelegate OnApplicationRestartRequiredDelegate;

	/** Is the active timer registered to refresh categories after the settings changed. */
	bool bIsActiveTimerRegistered;
	
	/** Are we showing all settings at once */
	bool bShowingAllSettings;
};
