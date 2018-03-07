// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Layout/Children.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SPanel.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class FArrangedChildren;
class SEditableTextBox;
class SErrorText;
class STextBlock;
class SThemeColorBlocksBar;

/**
 * A Color Theme is a name and an array of Colors.
 * It also holds and array of refresh callbacks which it calls every time it changes at all.
 */
class FColorTheme
{
public:

	FColorTheme(const FString& InName = TEXT(""), const TArray< TSharedPtr<FLinearColor> >& InColors = TArray< TSharedPtr<FLinearColor> >());

	/** Get a list of all the colors in the theme */
	const TArray< TSharedPtr<FLinearColor> >& GetColors() const
	{
		return Colors;
	}

	/** Insert a color at a specific point in the list and broadcast change */
	void InsertNewColor(TSharedPtr<FLinearColor> InColor, int32 InsertPosition);

	/** Check to see if a color is already present in the list */
	int32 FindApproxColor(const FLinearColor& InColor, float Tolerance = KINDA_SMALL_NUMBER) const;

	/** Remove all colors from the list, broadcast change */
	void RemoveAll();

	/** Remove color at index from the list, broadcast change */
	void RemoveColor(int32 ColorIndex);

	/** Remove specific color from the list, broadcast change */
	int32 RemoveColor(const TSharedPtr<FLinearColor> InColor);
	
	FString Name;

	DECLARE_EVENT( FColorTheme, FRefreshEvent );
	FRefreshEvent& OnRefresh()
	{
		return RefreshEvent;
	}

private:

	TArray< TSharedPtr<FLinearColor> > Colors;

	FRefreshEvent RefreshEvent;
};


/**
 * The SColorTrash is a multipurpose widget which allows FColorDragDrops
 * to be dropped on to to be 
 */
class SColorTrash
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SColorTrash)
		: _UsesSmallIcon(false)
	{ }

		SLATE_ATTRIBUTE(bool, UsesSmallIcon)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

protected:	

	/**
	 * Called during drag and drop when the drag enters a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	/**
	 * Called during drag and drop when the drag leaves a widget.
	 *
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;

	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	
	const FSlateBrush* GetBorderStyle() const;

private:

	/** Determines whether to draw the border to show activation */
	bool bBorderActivated;
};


/**
 * SThemeColorBlocks are Color Blocks which point to a Color in a ColorTheme.
 * They can be dragged and dropped, and clicking on one in the Color Picker will
 * give the color that they point to.
 */
class SThemeColorBlock
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SThemeColorBlock)
		: _Color()
		, _OnSelectColor()
		, _Parent()
		, _ShowTrashCallback()
		, _HideTrashCallback()
		, _UseSRGB()
		, _UseAlpha()
	{ }

		/** A pointer to the color this block uses */
		SLATE_ATTRIBUTE(TSharedPtr<FLinearColor>, Color)
		
		/** Event called when this block is clicked */
		SLATE_EVENT(FOnLinearColorValueChanged, OnSelectColor)

		/** A pointer to the theme color blocks bar that is this block's origin */
		SLATE_ATTRIBUTE(TSharedPtr<SThemeColorBlocksBar>, Parent)
		
		/** Callback to pass down to the FColorDragDrop for it to show the trash */
		SLATE_EVENT(FSimpleDelegate, ShowTrashCallback)
		/** Callback to pass down to the FColorDragDrop for it to hide the trash */
		SLATE_EVENT(FSimpleDelegate, HideTrashCallback)
		
		/** Whether to display sRGB color */
		SLATE_ATTRIBUTE(bool, UseSRGB)

		/** Whether the ability to pick the alpha value is enabled */
		SLATE_ATTRIBUTE(bool, UseAlpha)

	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param InArgs   Declaration from which to construct the widget.
	 */
	void Construct(const FArguments& InArgs );
	
private:

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	
	FLinearColor GetColor() const;

	FSlateColor HandleBorderColor() const;
	const FSlateBrush* HandleBorderImage() const;

	FText GetRedText() const;
	FText GetGreenText() const;
	FText GetBlueText() const;
	FText GetAlphaText() const;
	FText GetHueText() const;
	FText GetSaturationText() const;
	FText GetValueText() const;

	/**
	 * Function for formatting text for the tooltip which has limited space.
	 */
	FText FormatToolTipText(const FText& ColorIdentifier, float Value) const;
	
	bool OnReadIgnoreAlpha() const;
	bool OnReadShowBackgroundForAlpha() const;
	EVisibility OnGetAlphaVisibility() const;

	/** A pointer to the color this block uses */
	TWeakPtr<FLinearColor> ColorPtr;
	
	/** A pointer to the theme color blocks bar that is this block's origin */
	TWeakPtr<SThemeColorBlocksBar> ParentPtr;
	
	/** Event called when this block is clicked */
	FOnLinearColorValueChanged OnSelectColor;
	
	/** Callback to pass down to the FColorDragDrop for it to show the trash */
	FSimpleDelegate ShowTrashCallback;
	/** Callback to pass down to the FColorDragDrop for it to hide the trash */
	FSimpleDelegate HideTrashCallback;
	
	/** Whether to use display sRGB color */
	TAttribute<bool> bUseSRGB;

	/** Whether or not the color uses Alpha or not */
	TAttribute<bool> bUseAlpha;

	float DistanceDragged;
};


/**
 * SThemeColorBlocksBars are panels for dragging and dropping SColorThemeBlocks onto and off of.
 */
class SThemeColorBlocksBar
	: public SPanel
{
public:

	SLATE_BEGIN_ARGS(SThemeColorBlocksBar)
		: _ColorTheme()
		, _OnSelectColor()
		, _ShowTrashCallback()
		, _HideTrashCallback()
		, _EmptyText()
		, _UseSRGB()
		, _UseAlpha()
	{ }

		/** A pointer to the color theme that this bar should display */
		SLATE_ATTRIBUTE(TSharedPtr<FColorTheme>, ColorTheme)

		/** Event called when a color block is clicked */
		SLATE_EVENT(FOnLinearColorValueChanged, OnSelectColor)

		/** Callback to pass down to the FColorDragDrop for it to show the trash */
		SLATE_EVENT(FSimpleDelegate, ShowTrashCallback)
		/** Callback to pass down to the FColorDragDrop for it to hide the trash */
		SLATE_EVENT(FSimpleDelegate, HideTrashCallback)
		
		/** Specify what the bar should display when no colors are present */
		SLATE_ARGUMENT(FText, EmptyText)

		/** Whether to display sRGB color */
		SLATE_ATTRIBUTE(bool, UseSRGB)

		/** Whether the ability to pick the alpha value is enabled */
		SLATE_ATTRIBUTE(bool, UseAlpha)

	SLATE_END_ARGS()

	SThemeColorBlocksBar();

	/**
	 * Most panels do not create widgets as part of their implementation, so
	 * they do not need to implement a Construct()
	 */
	void Construct(const FArguments& InArgs);
	
	/**
	 * Panels arrange their children in a space described by the AllottedGeometry parameter. The results of the arrangement
	 * should be returned by appending a FArrangedWidget pair for every child widget.
	 *
	 * @param AllottedGeometry    The geometry allotted for this widget by its parent.
	 * @param ArrangedChildren    The array to which to add the WidgetGeometries that represent the arranged children.
	 */
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

	/**
	 * A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
	 * the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
	 *
	 * @return The desired size.
	 */
	virtual FVector2D ComputeDesiredSize(float) const override;

	/**
	 * All widgets must provide a way to access their children in a layout-agnostic way.
	 * Panels store their children in Slots, which creates a dilemma. Most panels
	 * can store their children in a TPanelChildren<Slot>, where the Slot class
	 * provides layout information about the child it stores. In that case
	 * GetChildren should simply return the TPanelChildren<Slot>.
	 */
	virtual FChildren* GetChildren() override;
	
	/**
	 * Called during drag and drop when the drag enters a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	
	/**
	 * Called during drag and drop when the drag leaves a widget.
	 *
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	/**
	 * Adds a new color block to the Bar
	 * @param Color				The	color the new block will be
	 * @param InsertPosition	The location the block will be at
	 */
	void AddNewColorBlock(FLinearColor Color, int32 InsertPosition);

	/**
	 * @param ColorToRemove		The pointer to the color that should be removed
	 *
	 * @return					The index of the removed color block, INDEX_NONE if it can't be found
	 */
	int32 RemoveColorBlock(TSharedPtr<FLinearColor> ColorToRemove);

	/**
	 * Refresh Callbacks are used by Color Themes to notify widgets to refresh
	 * This removes a callback from the Color Theme that this bar has a pointer to
	 */
	void RemoveRefreshCallback();

	/** Adds a callback to the Color Theme that this bar has a pointer to. */
	void AddRefreshCallback();

	/**
	 * Rebuilds the entire bar, regenerating all the constituent color blocks
	 */
	void Refresh();

	void SetPlaceholderGrabOffset(FVector2D GrabOffset);
	
private:

	/** Destroys the placeholder block in this widget. */
	void DestroyPlaceholders();

	/** The children blocks of this panel */
	TSlotlessChildren<SThemeColorBlock> ColorBlocks;

	/**
	 * A placeholder child, which exists as a "preview" to show what would happen when dropping the current
	 * FColorDragDrop onto this widget. It "replaces" the FColorDragDrop widget with a preview SThemeColorBlock
	 */
	TSharedPtr<SThemeColorBlock> NewColorBlockPlaceholder;

	/** The placeholder's color, also used to determine whether the placeholder is real or not (by IsValid) */
	TSharedPtr<FLinearColor> NewColorPlaceholder;

	/** Current x offset of the placeholder block */
	float PlaceholderBlockOffset;

	/** The initial grab offset when grabbing the placeholder */
	FVector2D PlaceholderInitialGrabOffset;

	/** A help text widget which appears when there are no children in this panel */
	TSharedPtr<SWidget> EmptyHintTextBlock;

	/**
	 * The Color Theme that this SThemeColorBlockBar is displaying.
	 * This is a TAttribute so it can re-get the theme when it changes rather than rely on a delegate to refresh it
	 */
	TAttribute< TSharedPtr<FColorTheme> > ColorTheme;
	
	/** Event called when a color block is clicked */
	FOnLinearColorValueChanged OnSelectColor;
	
	/** Callback to pass to the Color Theme. Holds a handle to this bar's Refresh method */
	FSimpleDelegate RefreshCallback;

	/** Handle to the registered RefreshCallback delegate */
	FDelegateHandle RefreshCallbackHandle;

	/** Callback to pass down to the FColorDragDrop for it to show the trash */
	FSimpleDelegate ShowTrashCallback;

	/** Callback to pass down to the FColorDragDrop for it to hide the trash */
	FSimpleDelegate HideTrashCallback;

	/** Whether to use display sRGB color */
	TAttribute<bool> bUseSRGB;

	/** Whether or not the color uses Alpha or not */
	TAttribute<bool> bUseAlpha;
};


DECLARE_DELEGATE_OneParam(FOnCurrentThemeChanged, TSharedPtr<FColorTheme>)


/**
 * SColorThemeBars include a ThemeColorBlocksBar in addition to a label.
 * Clicking on one will select it and set the currently used color theme to it
 */
class SColorThemeBar
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SColorThemeBar)
		: _ColorTheme()
		, _OnCurrentThemeChanged()
		, _ShowTrashCallback()
		, _HideTrashCallback()
		, _UseSRGB()
		, _UseAlpha()
	{ }

		/** The color theme that this bar is displaying */
		SLATE_ATTRIBUTE(TSharedPtr<FColorTheme>, ColorTheme)

		/** Event to be called when the current theme changes */
		SLATE_EVENT(FOnCurrentThemeChanged, OnCurrentThemeChanged)

		/** Callback to pass down to the FColorDragDrop for it to show the trash */
		SLATE_EVENT(FSimpleDelegate, ShowTrashCallback)
		/** Callback to pass down to the FColorDragDrop for it to hide the trash */
		SLATE_EVENT(FSimpleDelegate, HideTrashCallback)
		
		/** Whether to display sRGB color */
		SLATE_ATTRIBUTE(bool, UseSRGB)

		/** Whether the ability to pick the alpha value is enabled */
		SLATE_ATTRIBUTE(bool, UseAlpha)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	/**
	 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param MouseEvent Information about the input event
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );
	
private:

	FText GetThemeName() const;

	/** Text Block which shows the Color Theme's name */
	TSharedPtr<STextBlock> ThemeNameText;

	/** Color Theme that this bar is displaying */
	TWeakPtr<FColorTheme> ColorTheme;

	/** Callback to execute when the global current theme has changed */
	FOnCurrentThemeChanged OnCurrentThemeChanged;

	/** Callback to pass down to the FColorDragDrop for it to show the trash */
	FSimpleDelegate ShowTrashCallback;

	/** Callback to pass down to the FColorDragDrop for it to hide the trash */
	FSimpleDelegate HideTrashCallback;
	
	/** Whether to use display sRGB color */
	TAttribute<bool> bUseSRGB;

	/** Whether or not the color uses Alpha or not */
	TAttribute<bool> bUseAlpha;
};


/**
 * There should only ever be a single SColorThemesViewer. It is the widget which
 * manages all color themes and displays in a nice list format.
 */
class SColorThemesViewer
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SColorThemesViewer)
		: _UseAlpha()
	{ }

		/** Whether the ability to pick the alpha value is enabled */
		SLATE_ATTRIBUTE(bool, UseAlpha)

	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	
	/** Gets the current color theme */
	TSharedPtr<FColorTheme> GetCurrentColorTheme() const;

	/** Sets the UseAlpha attribute */
	void SetUseAlpha( const TAttribute<bool>& InUseAlpha );

	/** Load the color theme settings from the config */
	static void LoadColorThemesFromIni();

	/** Save the color theme settings to the config */
	static void SaveColorThemesToIni();
	
	/** Callbacks to execute whenever we change the global current theme */
	DECLARE_EVENT( SColorThemesViewer, FCurrentThemeChangedEvent );
	FCurrentThemeChangedEvent& OnCurrentThemeChanged()
	{
		return CurrentThemeChangedEvent;
	}

	/** Whether to use display sRGB color */
	static bool bSRGBEnabled;

	void MenuToStandardNoReturn();

private:

	FReply NewColorTheme();
	FReply DuplicateColorTheme();
	FReply DeleteColorTheme();
	FReply AcceptThemeName();
	/** Rename theme if the user has pressed Enter in the RenameTextBox */
	void CommitThemeName(const FText& InText, ETextCommit::Type InCommitType);
	void UpdateThemeNameFromTextBox();
	
	bool CanAcceptThemeName() const;
	void ChangeThemeName(const FText& InText);
	EVisibility OnGetErrorTextVisibility() const;

	/** Sets the current color theme to the existing theme */
	void SetCurrentColorTheme(TSharedPtr<FColorTheme> NewTheme);

	FReply MenuToStandard();
	FReply MenuToRename();
	FReply MenuToDelete();
	void MenuToTrash();
	
	bool OnReadUseSRGB() const;
	bool OnReadUseAlpha() const;
	
	/** Refreshes the list, saves colors to ini, and sends the menu to standard */
	void RefreshThemes();

	TSharedRef<ITableRow> OnGenerateColorThemeBars(TSharedPtr<FColorTheme> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets the default color theme, optionally creates it if not present */
	static TSharedPtr<FColorTheme> GetDefaultColorTheme(bool bCreateNew = false);
	/** Gets the color theme, creates it if not present */
	static TSharedPtr<FColorTheme> GetColorTheme(const FString& ThemeName);
	/** Checks to see if this is a color theme, returns success */
	static TSharedPtr<FColorTheme> IsColorTheme(const FString& ThemeName);
	/** Makes the passed theme name unique so it doesn't clash with pre-existing themes */
	static FString MakeUniqueThemeName(const FString& ThemeName);
	/** Creates a new theme, ensuring the name is unique */
	static TSharedPtr<FColorTheme> NewColorTheme(const FString& ThemeName, const TArray< TSharedPtr<FLinearColor> >& ThemeColors = TArray< TSharedPtr<FLinearColor> >());
	
	/** A list of all the color themes for the list view to use */
	TSharedPtr< SListView <TSharedPtr <FColorTheme> > > ColorThemeList;

	/** The menu is a widget at the bottom of the SColorThemesViewer with variable content */
	TSharedPtr<SBorder> Menu;
	/** The menu alternates between a standard menu, a rename menu, a confirm delete menu, and a color trash */
	TSharedPtr<SWidget> MenuStandard;
	TSharedPtr<SWidget> MenuRename;
	TSharedPtr<SWidget> MenuConfirmDelete;
	TSharedPtr<SWidget> MenuTrashColor;
	/** If any error occurs, it needs to be displayed here as SPopupErrorText won't work with nested combo buttons */
	TSharedPtr<SErrorText> ErrorText;

	/** The text box for renaming themes */
	TSharedPtr<SEditableTextBox> RenameTextBox;

	/** Callbacks to execute whenever we change the global current theme */
	FCurrentThemeChangedEvent CurrentThemeChangedEvent;

	/** Whether or not the color uses Alpha or not */
	TAttribute<bool> bUseAlpha;

	/** A static holder of the color themes for the entire program */
	static TArray<TSharedPtr<FColorTheme>> ColorThemes;

	/** A static pointer to the color theme that is currently selected for the entire program */
	static TWeakPtr<FColorTheme> CurrentlySelectedThemePtr;
};


/**
 * This operation is a color which can be dragged and dropped between widgets.
 * Represents a SThemeColorBlock that is dragged around, and can be dropped into a color trash.
 */
class FColorDragDrop
	: public FDragDropOperation
{
public:

	DRAG_DROP_OPERATOR_TYPE(FColorDragDrop, FDragDropOperation)

	/**
	 * Invoked when the drag and drop operation has ended.
	 * 
	 * @param bDropWasHandled   true when the drop was handled by some widget; false otherwise
	 * @param MouseEvent        The mouse event which caused the on drop to be called.
	 */
	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override;

	/** 
	 * Called when the mouse was moved during a drag and drop operation
	 *
	 * @param DragDropEvent    The event that describes this drag drop operation.
	 */
	virtual void OnDragged( const class FDragDropEvent& DragDropEvent ) override;
	
	/** Gets the widget that will serve as the decorator unless overridden. If you do not override, you will have no decorator */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
	
	/**
	 * Makes a new FColorDragDrop to hold on to
	 * @param InColor			The color to be dragged and dropped
	 * @param bSRGB				Whether the color is sRGB
	 * @param bUseAlpha			Whether the colors alpha is important
	 * @param TrashShowCallback	Called when this operation is created
	 * @param TrashHideCallback Called when this operation is dropped
	 * @param Origin			The SThemeColorBlockBar that this operation is from
	 * @param OriginPosition	The position in it's origin that it is from
	 */
	static TSharedRef<FColorDragDrop> New( FLinearColor InColor, bool bSRGB, bool bUseAlpha,
		FSimpleDelegate TrashShowCallback = FSimpleDelegate(), FSimpleDelegate TrashHideCallback = FSimpleDelegate(),
		TSharedPtr<SThemeColorBlocksBar> Origin = TSharedPtr<SThemeColorBlocksBar>(), int32 OriginPosition = 0 );

	/** The color currently held onto by this drag drop operation */
	FLinearColor Color;

	/** Whether or not the color uses SRGB or not */
	bool bUseSRGB;

	/** Whether or not the color uses Alpha or not */
	bool bUseAlpha;

	/**
	 * The origin is the SThemeColorBlockBar where this operation is from.
	 * Upon dropping, this operation jumps back to it's origin if possible
	 */
	TWeakPtr<SThemeColorBlocksBar> OriginBar;

	/** The origin position */
	int32 OriginBarPosition;

	/** Callback to show the trash when this widget is created */
	FSimpleDelegate ShowTrash;

	/** Callback to hide the trash when this widget is dropped */
	FSimpleDelegate HideTrash;

	/** Flag which ensures that OnDrop will not replace this block in it's origin */
	bool bSetForDeletion;

	/** The size of the drag and drop color block */
	FVector2D BlockSize;
};
