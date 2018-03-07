// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/TextFilter.h"

class ITableRow;
class STableViewBase;
class SToolTip;
class SVerticalBox;
struct FProjectCategory;
struct FProjectItem;
struct FSlateBrush;
enum class ECheckBoxState : uint8;

/**
 * A list of known projects with the option to add a new one
 */
class SProjectBrowser
	: public SCompoundWidget
{
public:

	DECLARE_DELEGATE(FNewProjectScreenRequested)

	SLATE_BEGIN_ARGS(SProjectBrowser) { }

	SLATE_END_ARGS()

public:

	/** Constructor */
	SProjectBrowser();

	/**
	 * Constructs this widget with InArgs.
	 *
	 * @param InArgs - The construction arguments.
	 */
	void Construct( const FArguments& InArgs );

	bool HasProjects() const;

protected:

	void ConstructCategory( const TSharedRef<SVerticalBox>& CategoriesBox, const TSharedRef<FProjectCategory>& Category ) const;

	/** Creates a row in the template list */
	TSharedRef<ITableRow> MakeProjectViewWidget( TSharedPtr<FProjectItem> ProjectItem, const TSharedRef<STableViewBase>& OwnerTable );

	/** Create a tooltip for the given project item */
	TSharedRef<SToolTip> MakeProjectToolTip( TSharedPtr<FProjectItem> ProjectItem ) const;

	/** Add information to the tooltip for this project item */
	void AddToToolTipInfoBox(const TSharedRef<SVerticalBox>& InfoBox, const FText& Key, const FText& Value) const;

	/** Get the context menu to use for the selected project item */
	TSharedPtr<SWidget> OnGetContextMenuContent() const;

	/** Handler for when find in explorer is selected */
	void ExecuteFindInExplorer() const;

	/** Handler to check to see if a find in explorer command is allowed */
	bool CanExecuteFindInExplorer() const;	

	/** Gets the image to display for the specified template */
	const FSlateBrush* GetProjectItemImage( TWeakPtr<FProjectItem> ProjectItem ) const;

	/** Gets the currently selected template item */
	TSharedPtr<FProjectItem> GetSelectedProjectItem( ) const;

	/** Gets the label to show the currently selected template */
	FText GetSelectedProjectName( ) const;

	/** Populates ProjectItemsSource with projects found on disk */
	FReply FindProjects( );

	/** Adds the specified project to the specified category. Creates a new category if necessary. */
	void AddProjectToCategory( const TSharedRef<FProjectItem>& ProjectItem, const FText& ProjectCategory );

	/** Opens the specified project file */
	bool OpenProject( const FString& ProjectFile );

	/** Begins the opening process for the selected project */
	void OpenSelectedProject( );

	/** Populate the list of filtered project categories */
	void PopulateFilteredProjectCategories();
	
	/**
	 * Called after a key is pressed when this widget has focus (this event bubbles if not handled)
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

private:

	FReply OnBrowseToProjectClicked();

	/** Handler for when the Open button is clicked */
	FReply HandleOpenProjectButtonClicked( );

	/** Returns true if the user is allowed to open a project with the supplied name and path */
	bool HandleOpenProjectButtonIsEnabled( ) const;

	/** Called when a user double-clicks a project item in the project view */
	void HandleProjectItemDoubleClick( TSharedPtr<FProjectItem> ProjectItem );

	/** Handler for when the selection changes in the project view */
	void HandleProjectViewSelectionChanged( TSharedPtr<FProjectItem> ProjectItem, ESelectInfo::Type SelectInfo, FText CategoryName );

	/** Callback for clicking the 'Marketplace' button. */
	FReply HandleMarketplaceTabButtonClicked( );

	/** Called when the text in the filter box is changed */
	void OnFilterTextChanged(const FText& InText);

	/** Whether to autoload the last project. */
	void OnAutoloadLastProjectChanged(ECheckBoxState NewState);

	/** Get the visibility of the specified category */
	EVisibility GetProjectCategoryVisibility(const TSharedRef<FProjectCategory> InCategory) const;

	EVisibility GetNoProjectsErrorVisibility() const;

	/** Get the visibility of the "no projects" error */
	EVisibility GetNoProjectsAfterFilterErrorVisibility() const;
	
	/** Get the visibility of the active search overlay */
	EVisibility GetFilterActiveOverlayVisibility() const;

	/** Get the filter text to highlight on items in the list */
	FText GetItemHighlightText() const;

private:
	// Holds the collection of project categories.
	TArray<TSharedRef<FProjectCategory> > ProjectCategories;

	/** Search box used to set the filter text */
	TSharedPtr<class SSearchBox> SearchBoxPtr;

	/** Filter that is used to test for the visibility of projects */
	typedef TTextFilter<const TSharedPtr<FProjectItem>> ProjectItemTextFilter;
	ProjectItemTextFilter ProjectItemFilter;
	int32 NumFilteredProjects;

	int32 ThumbnailBorderPadding;
	int32 ThumbnailSize;
	bool bPreventSelectionChangeEvent;

	TSharedPtr<FProjectItem> CurrentlySelectedItem;
	FText CurrentSelectedProjectPath;

	bool IsOnlineContentFinished;
	TSharedPtr<SVerticalBox> CategoriesBox;

	bool bHasProjectFiles;
private:

	// Holds a delegate that is executed when the new project screen is being requested.
	FNewProjectScreenRequested NewProjectScreenRequestedDelegate;
};
