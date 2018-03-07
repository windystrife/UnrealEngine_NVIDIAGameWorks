// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiBoxExtender.h"
#include "SlateDelegates.h"
#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Styling/CoreStyle.h"
#include "Framework/SlateDelegates.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "MultiBox.h"

class FUICommandInfo;
class FUICommandList;
struct FSlateIcon;
struct FUIAction;

/** Delegate used by multi-box to call a user function to populate a new menu.  Used for spawning sub-menus and pull-down menus. */
DECLARE_DELEGATE_OneParam( FNewMenuDelegate, class FMenuBuilder& );


/**
 * MultiBox builder
 */
class SLATE_API FMultiBoxBuilder
{

public:

	/**
	 * Constructor
	 *
	 * @param	InType	Type of MultiBox
	 * @param	bInShouldCloseWindowAfterMenuSelection	Sets whether or not the window that contains this multibox should be destroyed after the user clicks on a menu item in this box
	 * @param	InCommandList	The action list that maps command infos to delegates that should be called for each command associated with a multiblock widget.  This can be modified after the MultiBox is created by calling the PushCommandList() and PopCommandList() methods.
	 * @param	InTutorialHighlightName	Optional name to identify this widget and highlight during tutorials
	 */
	FMultiBoxBuilder( const EMultiBoxType::Type InType, FMultiBoxCustomization InCustomization, const bool bInShouldCloseWindowAfterMenuSelection, const TSharedPtr< const FUICommandList >& InCommandList, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), FName InTutorialHighlightName = NAME_None );

	virtual ~FMultiBoxBuilder() {}

	/**
	 * Adds an editable text entry
	 *
	 * @param	InLabel				The label to display in the menu
	 * @param	InToolTip			The tool tip to display when the menu entry is hovered over
	 * @param	InIcon				The icon to display to the left of the label
	 * @param	InTextAttribute		The text string we're editing (often, a delegate will be bound to the attribute)
	 * @param	InOnTextCommitted	Called when the user commits their change to the editable text control
	 * @param	InOnTextChanged		Called when the text is changed interactively
	 * @param	bInReadOnly			Whether or not the text block is read only
	 */
	void AddEditableText( const FText& InLabel, const FText& InToolTip, const FSlateIcon& InIcon, const TAttribute< FText >& InTextAttribute, const FOnTextCommitted& InOnTextCommitted = FOnTextCommitted(), const FOnTextChanged& InOnTextChanged = FOnTextChanged(), bool bInReadOnly = false );


	/**
	 * Creates a widget for this MultiBox
	 *
	 * @return  New widget object
	 */
	virtual TSharedRef< class SWidget > MakeWidget( FMultiBox::FOnMakeMultiBoxBuilderOverride* InMakeMultiBoxBuilderOverride = nullptr );
	

	/** 
	 * Get the multi-box being built.
	 *
	 * @return The multi-box being built.
	 */
	TSharedRef< class FMultiBox > GetMultiBox();


	/**
	 * Pushes a new command list onto the stack.  This command list will be used for all subsequently-added multiblocks, until the command-list is popped.
	 *
	 * @param	CommandList		The new command list to use
	 */
	void PushCommandList( const TSharedRef< const FUICommandList > CommandList );


	/**
	 * Pops the current command list.
	 */
	void PopCommandList();
	
	/**
	 * @return The top command list
	 */
	TSharedPtr<const FUICommandList> GetTopCommandList();

	/**
	 * Pushes a new extender onto the stack. This extender will be used for all subsequently-added multiblocks, until the extender is popped.
	 *
	 * @param	InExtender	The new extender to use
	 */
	void PushExtender( TSharedRef< FExtender > InExtender );


	/**
	 * Pops the current extender.
	 */
	void PopExtender();

	/** @return The style set used by the multibox widgets */
	const ISlateStyle* GetStyleSet() const;

	/** @return The style name used by the multibox widgets */
	const FName& GetStyleName() const;

	/** Sets the style to use on the multibox widgets */
	void SetStyle( const ISlateStyle* InStyleSet, const FName& InStyleName );

	/** @return  The customization settings for the box being built */
	FMultiBoxCustomization GetCustomization() const;

protected:
	/** Applies any potential extension hooks at the current place */
	virtual void ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition) = 0;
	
	/** Applies the beginning of a section, including the header and relevant separator */
	virtual void ApplySectionBeginning() {}

protected:

	/** The MultiBox we're building up */
	TSharedRef< class FMultiBox > MultiBox;

	/** A stack of command lists which map command infos to delegates that should be called.  New multi blocks will always use
		the command-list at the top of the stack at the time they are added. */
	TArray< TSharedPtr< const FUICommandList > > CommandListStack;

	/** The extender stack holding all the possible extensions for this menu builder */
	TArray< TSharedPtr<class FExtender> > ExtenderStack;

	/** Name to identify this widget and highlight during tutorials */
	FName TutorialHighlightName;
};



/**
 * Base menu builder
 */
class SLATE_API FBaseMenuBuilder : public FMultiBoxBuilder
{

public:

	/**
	 * Constructor
	 *
	 * @param	InType	Type of MultiBox
	 * @param	bInShouldCloseWindowAfterMenuSelection	Sets whether or not the window that contains this multibox should be destroyed after the user clicks on a menu item in this box
	 * @param	InCommandList	The action list that maps command infos to delegates that should be called for each command associated with a multiblock widget
	 * @param	bInCloseSelfOnly	True if clicking on a menu entry closes itself only and its children but not the entire stack 
	 * @param	InTutorialHighlightName	Optional name to identify this widget and highlight during tutorials
	 */
	FBaseMenuBuilder( const EMultiBoxType::Type InType, const bool bInShouldCloseWindowAfterMenuSelection, TSharedPtr< const FUICommandList > InCommandList, bool bInCloseSelfOnly, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), const ISlateStyle* InStyleSet = &FCoreStyle::Get(), FName InTutorialHighlightName = NAME_None );

	/**
	 * Adds a menu entry
	 *
	 * @param	InCommand			The command associated with this menu entry
	 * @param	InExtensionHook		The section hook. Can be NAME_None
	 * @param	InLabelOverride		Optional label override.  If omitted, then the action's label will be used instead.
	 * @param	InToolTipOverride	Optional tool tip override.	 If omitted, then the action's label will be used instead.
	 * @param	InIconOverride		Optional name of the slate brush to use for the tool bar image.  If omitted, then the command's icon will be used instead.
	 * @param	InTutorialHighlightName	Optional name to identify this widget and highlight during tutorials
	 */
	void AddMenuEntry( const TSharedPtr< const FUICommandInfo > InCommand, FName InExtensionHook = NAME_None, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const FSlateIcon& InIconOverride = FSlateIcon(), FName InTutorialHighlightName = NAME_None );

	/**
	 * Adds a menu entry without the use of a command
	 *
	 * @param	InLabel		Label to show in the menu entry
	 * @param	InToolTip	Tool tip used when hovering over the menu entry
	 * @param	InIcon		The icon to use		
	 * @param	UIAction	Actions to execute on this menu item.
	 * @param	InExtensionHook			The section hook. Can be NAME_None
	 * @param	UserInterfaceActionType	Type of interface action
	 * @param	InTutorialHighlightName	Optional name to identify this widget and highlight during tutorials
	 */
	void AddMenuEntry( const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const FSlateIcon& InIcon, const FUIAction& UIAction, FName InExtensionHook = NAME_None, const EUserInterfaceActionType::Type UserInterfaceActionType = EUserInterfaceActionType::Button, FName InTutorialHighlightName = NAME_None );

	void AddMenuEntry( const FUIAction& UIAction, const TSharedRef< SWidget > Contents, const FName& InExtensionHook = NAME_None, const TAttribute<FText>& InToolTip = TAttribute<FText>(), const EUserInterfaceActionType::Type UserInterfaceActionType = EUserInterfaceActionType::Button, FName InTutorialHighlightName = NAME_None );

protected:
	/** True if clicking on a menu entry closes itself only and its children and not the entire stack */
	bool bCloseSelfOnly;
};


/**
 * Vertical menu builder
 */
class SLATE_API FMenuBuilder : public FBaseMenuBuilder
{
public:

	/**
	 * Constructor
	 *
	 * @param	bInShouldCloseWindowAfterMenuSelection	Sets whether or not the window that contains this multibox should be destroyed after the user clicks on a menu item in this box
	 * @param	InCommandList	The action list that maps command infos to delegates that should be called for each command associated with a multiblock widget
	 * @param	bInCloseSelfOnly	True if clicking on a menu entry closes itself only and its children but not the entire stack
	 */
	FMenuBuilder( const bool bInShouldCloseWindowAfterMenuSelection, TSharedPtr< const FUICommandList > InCommandList, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), const bool bCloseSelfOnly = false, const ISlateStyle* InStyleSet = &FCoreStyle::Get() )
		: FBaseMenuBuilder( EMultiBoxType::Menu, bInShouldCloseWindowAfterMenuSelection, InCommandList, bCloseSelfOnly, InExtender, InStyleSet )
		, bSectionNeedsToBeApplied(false)
	{
		AddSearchWidget();
	}

	/**
	* Creates a widget for this MultiBox
	*
	* @return  New widget object
	*/
	virtual TSharedRef< class SWidget > MakeWidget( FMultiBox::FOnMakeMultiBoxBuilderOverride* InMakeMultiBoxBuilderOverride = nullptr ) override;

	/**
	 * Adds a menu separator
	 */
	void AddMenuSeparator(FName InExtensionHook = NAME_None);
	

	/**
	 * Starts a section on to the extender section hook stack
	 * 
	 * @param InExtensionHook	The section hook. Can be NAME_None
	 * @param InHeadingText		The heading text to use. If none, only a separator is used
	 */
	void BeginSection( FName InExtensionHook, const TAttribute< FText >& InHeadingText = TAttribute<FText>() );

	/**
	 * Ends the current section
	 */
	void EndSection();


	/**
	 * Adds a sub-menu which is a menu within a menu
	 * 
	 * @param	InMenuLabel			The text that should be shown for the menu
	 * @param	InToolTip			The tooltip that should be shown when the menu is hovered over
	 * @param	InSubMenu			Sub-Menu object which creates menu entries for the sub-menu
	 * @param	InUIAction			Actions to execute on this menu item.
	 * @param	InExtensionHook		The section hook. Can be NAME_None
	 * @param	InUserInterfaceActionType	Type of interface action
	 * @param bInOpenSubMenuOnClick Sub-menu will open only if the sub-menu entry is clicked
	 * @param	InIcon				The icon to use
	 * @param	bInShouldCloseWindowAfterMenuSelection	Whether the submenu should close after an item is selected
	 */
	void AddSubMenu( const TAttribute<FText>& InMenuLabel, const TAttribute<FText>& InToolTip, const FNewMenuDelegate& InSubMenu, const FUIAction& InUIAction, FName InExtensionHook, const EUserInterfaceActionType::Type InUserInterfaceActionType, const bool bInOpenSubMenuOnClick = false, const FSlateIcon& InIcon = FSlateIcon(), const bool bInShouldCloseWindowAfterMenuSelection = true );

	void AddSubMenu( const TAttribute<FText>& InMenuLabel, const TAttribute<FText>& InToolTip, const FNewMenuDelegate& InSubMenu, const bool bInOpenSubMenuOnClick = false, const FSlateIcon& InIcon = FSlateIcon(), const bool bInShouldCloseWindowAfterMenuSelection = true );

	void AddSubMenu( const TSharedRef< SWidget > Contents, const FNewMenuDelegate& InSubMenu, const bool bInOpenSubMenuOnClick = false, const bool bInShouldCloseWindowAfterMenuSelection = true );

	void AddSubMenu( const FUIAction& UIAction, const TSharedRef< SWidget > Contents, const FNewMenuDelegate& InSubMenu, const bool bInShouldCloseWindowAfterMenuSelection = true );

	/**
	 * Adds any widget to the menu
	 * 
	 * @param	InWidget			The widget that should be shown in the menu
	 * @param	InLabel				Optional label text to be added to the left of the content
	 * @param	bNoIndent			If true, removes the padding from the left of the widget that lines it up with other menu items (default == false)
	 * @param	bSearchable			If true, widget will be searchable (default == true)
	 */
	void AddWidget( TSharedRef<SWidget> InWidget, const FText& Label, bool bNoIndent = false, bool bSearchable = true );

	/**
	* Adds the widget the multibox will use for searching
	*/
	void AddSearchWidget();

protected:
	/** FMultiBoxBuilder interface */
	virtual void ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition) override;
	virtual void ApplySectionBeginning() override;

public:
	// These classes need access to the AddWrapperSubMenu() methods
	//friend class FWidgetBlock;
	//friend class FToolBarComboButtonBlock;

	/**
	 * Adds a sub-menu which is a menu within a menu
	 * 
	 * @param	InMenuLabel			The text that should be shown for the menu
	 * @param	InToolTip			The tooltip that should be shown when the menu is hovered over
	 * @param	InSubMenu			Sub-Menu object which creates the sub-menu
	 */
	void AddWrapperSubMenu( const FText& InMenuLabel, const FText& InToolTip, const FOnGetContent& InSubMenu, const FSlateIcon& InIcon );

	/**
	 * Adds a sub-menu which is a menu within a menu
	 * 
	 * @param	InMenuLabel			The text that should be shown for the menu
	 * @param	InToolTip			The tooltip that should be shown when the menu is hovered over
	 * @param	InSubMenu			Sub-Menu object
	 */
	void AddWrapperSubMenu( const FText& InMenuLabel, const FText& InToolTip, const TSharedPtr<SWidget>& InSubMenu, const FSlateIcon& InIcon );

private:
	/** Current extension hook name for sections to determine where sections begin and end */
	FName CurrentSectionExtensionHook;
	
	/** Any pending section's heading text */
	FText CurrentSectionHeadingText;
	
	/** True if there is a pending section that needs to be applied */
	bool bSectionNeedsToBeApplied;
};



/**
 * Menu bar builder
 */
class SLATE_API FMenuBarBuilder : public FBaseMenuBuilder
{

public:

	/**
	 * Constructor
	 *
	 * @param	InCommandList	The action list that maps command infos to delegates that should be called for each command associated with a multiblock widget
	 */
	FMenuBarBuilder( TSharedPtr< const FUICommandList > InCommandList, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), const ISlateStyle* InStyleSet = &FCoreStyle::Get())
		: FBaseMenuBuilder( EMultiBoxType::MenuBar, false, InCommandList, false, InExtender, InStyleSet )
	{
	}


	/**
	 * Adds a pull down menu
	 *
	 * @param	InMenuLabel			The text that should be shown for the menu
	 * @param	InToolTip			The tooltip that should be shown when the menu is hovered over
	 * @param	InPullDownMenu		Pull down menu object for the menu to add.
	 */
	void AddPullDownMenu( const FText& InMenuLabel, const FText& InToolTip, const FNewMenuDelegate& InPullDownMenu, FName InExtensionHook = NAME_None, FName InTutorialHighlightName = NAME_None);
	
protected:
	/** FMultiBoxBuilder interface */
	virtual void ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition) override;
};



/**
 * Tool bar builder
 */
class SLATE_API FToolBarBuilder : public FMultiBoxBuilder
{

public:

	/**
	 * Constructor
	 *
	 * @param	InCommandList	The action list that maps command infos to delegates that should be called for each command associated with a multiblock widget
	 */
	FToolBarBuilder( TSharedPtr< const FUICommandList > InCommandList, FMultiBoxCustomization InCustomization, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), EOrientation Orientation = Orient_Horizontal, const bool InForceSmallIcons = false )
		: FMultiBoxBuilder( (Orientation == Orient_Horizontal) ? EMultiBoxType::ToolBar : EMultiBoxType::VerticalToolBar, InCustomization, false, InCommandList, InExtender )
		, bSectionNeedsToBeApplied(false)
		, bIsFocusable(false)
		, bForceSmallIcons(InForceSmallIcons)
	{
	}

	void SetLabelVisibility( EVisibility InLabelVisibility ) { LabelVisibility  = InLabelVisibility ; }

	void SetIsFocusable( bool bInIsFocusable ) { bIsFocusable = bInIsFocusable; }

	/**
	 * Adds a tool bar button
	 *
	 * @param	InCommand				The command associated with this tool bar button
	 * @param	InLabelOverride			Optional label override.  If omitted, then the action's label will be used instead.
	 * @param	InToolTipOverride		Optional tool tip override.	 If omitted, then the action's label will be used instead.
	 * @param	InIconOverride			Optional name of the slate brush to use for the tool bar image.  If omitted, then the action's icon will be used instead.
	 * @param	InTutorialHighlightName	Name to identify this widget and highlight during tutorials
	 */
	void AddToolBarButton(const TSharedPtr< const FUICommandInfo > InCommand, FName InExtensionHook = NAME_None, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), FName InTutorialHighlightName = NAME_None );
	
	/**
	 * Adds a tool bar button
	 *
	 * @param	UIAction	Actions to execute on this menu item.
	 * @param	InLabel		Label to show in the menu entry
	 * @param	InToolTip	Tool tip used when hovering over the menu entry
	 * @param	InIcon		The icon to use		
	 * @param	UserInterfaceActionType	Type of interface action
	 * @param	InTutorialHighlightName	Name to identify this widget and highlight during tutorials
	 */
	void AddToolBarButton(const FUIAction& InAction, FName InExtensionHook = NAME_None, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), const EUserInterfaceActionType::Type UserInterfaceActionType = EUserInterfaceActionType::Button, FName InTutorialHighlightName = NAME_None );

	/**
	 * Adds a combo button
	 *
	 * @param	InAction					UI action that sets the enabled state for this combo button
	 * @param	InMenuContentGenerator		Delegate that generates a widget for this combo button's menu content.  Called when the menu is summoned.
	 * @param	InLabelOverride				Optional label override.  If omitted, then the action's label will be used instead.
	 * @param	InToolTipOverride			Optional tool tip override.	 If omitted, then the action's label will be used instead.
	 * @param	InIconOverride				Optional icon to use for the tool bar image.  If omitted, then the action's icon will be used instead.
	 * @param	bInSimpleComboBox			If true, the icon and label won't be displayed
	 * @param	InTutorialHighlightName		Name to identify this widget and highlight during tutorials
	 */
	void AddComboButton( const FUIAction& InAction, const FOnGetContent& InMenuContentGenerator, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const TAttribute<FSlateIcon>& InIconOverride = TAttribute<FSlateIcon>(), bool bInSimpleComboBox = false, FName InTutorialHighlightName = NAME_None );

	/**
	 * Adds any widget to the toolbar
	 * 
	 * @param	InWidget				The widget that should be shown in the toolbar
	 * @param	InTutorialHighlightName	Name to identify this widget and highlight during tutorials
	 * @param	bSearchable			If true, widget will be searchable (default == true)
	 */
	void AddWidget(TSharedRef<SWidget> InWidget, FName InTutorialHighlightName = NAME_None, bool bSearchable = true);
	
	/**
	 * Adds a toolbar separator
	 */
	void AddSeparator(FName InExtensionHook = NAME_None);
	
	/**
	 * Starts a section on to the extender section hook stack
	 * 
	 * @param InExtensionHook	The section hook. Can be NAME_None
	 */
	void BeginSection( FName InExtensionHook );

	/**
	 * Ends the current section
	 */
	void EndSection();

	/** 
	 * Starts a new Group block, must be used in conjunction with EndBlockGroup
	 */
	void BeginBlockGroup();
	
	/** 
	 * End a group block, must be used in conjunction with BeginBlockGroup.
	 */
	void EndBlockGroup();

protected:
	/** FMultiBoxBuilder interface */
	virtual void ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition) override;
	virtual void ApplySectionBeginning() override;

private:
	/** Current extension hook name for sections to determine where sections begin and end */
	FName CurrentSectionExtensionHook;

	TOptional< EVisibility > LabelVisibility;

	/** True if there is a pending section that needs to be applied */
	bool bSectionNeedsToBeApplied;

	/** Whether the buttons created can receive keyboard focus */
	bool bIsFocusable;

	/** Whether this toolbar should always use small icons, regardless of the current settings */
	bool bForceSmallIcons;
};


/**
 * Button grid builder
 */
class SLATE_API FButtonRowBuilder : public FMultiBoxBuilder
{
public:
	/**
	 * Constructor
	 *
	 * @param	InCommandList	The action list that maps command infos to delegates that should be called for each command associated with a multiblock widget
	 */
	FButtonRowBuilder( TSharedPtr< const FUICommandList > InCommandList, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>() )
		: FMultiBoxBuilder( EMultiBoxType::ButtonRow, FMultiBoxCustomization::None, false, InCommandList, InExtender )
	{
	}


	/**
	 * Adds a button to a row
	 *
	 * @param	InCommand				The command associated with this tool bar button
	 * @param	InLabelOverride			Optional label override.  If omitted, then the action's label will be used instead.
	 * @param	InToolTipOverride		Optional tool tip override.	 If omitted, then the action's label will be used instead.
	 * @param	InIconOverride			Optional icon to use for the tool bar image.  If omitted, then the action's icon will be used instead.
	 */
	void AddButton( const TSharedPtr< const FUICommandInfo > InCommand, const TAttribute<FText>& InLabelOverride = TAttribute<FText>(), const TAttribute<FText>& InToolTipOverride = TAttribute<FText>(), const FSlateIcon& InIconOverride = FSlateIcon() );

	/**
	 * Adds a button to a row
	 *
	 * @param	InLabel					The button label to display
	 * @param	InToolTipOverride		The tooltip for the button
	 * @param	UIAction				Action to execute when the button is clicked or when state should be checked
	 * @param	InIcon					The icon for the button
	 * @param	UserInterfaceActionType	The style of button to display
	 */
	void AddButton( const FText& InLabel, const FText& InToolTip, const FUIAction& UIAction, const FSlateIcon& InIcon = FSlateIcon(), const EUserInterfaceActionType::Type UserInterfaceActionType = EUserInterfaceActionType::Button );
	
protected:
	/** FMultiBoxBuilder interface */
	virtual void ApplyHook(FName InExtensionHook, EExtensionHook::Position HookPosition) override {}
};
