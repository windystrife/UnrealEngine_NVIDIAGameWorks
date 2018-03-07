// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "InputCoreTypes.h"
#include "Layout/Visibility.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/InputChord.h"

class FBindingContext;
class FUICommandInfo;

/** Types of user interfaces that can be associated with a user interface action */
namespace EUserInterfaceActionType
{
	enum Type
	{
		/** An action which should not be associated with a user interface action */
		None,

		/** Momentary buttons or menu items.  These support enable state, and execute a delegate when clicked. */
		Button,

		/** Toggleable buttons or menu items that store on/off state.  These support enable state, and execute a delegate when toggled. */
		ToggleButton,
		
		/** Radio buttons are similar to toggle buttons in that they are for menu items that store on/off state.  However they should be used to indicate that menu items in a group can only be in one state */
		RadioButton,

		/** Similar to Button but will display a readonly checkbox next to the item. */
		Check,

		/** Similar to Button but has the checkbox area collapsed */
		CollapsedButton
	};
};

UENUM()
enum class EMultipleKeyBindingIndex : uint8
{
	Primary = 0,
	Secondary,
	NumChords
};

class FUICommandInfo;


/**
 *
 */
class SLATE_API FUICommandInfoDecl
{
	friend class FBindingContext;

public:

	FUICommandInfoDecl& DefaultChord( const FInputChord& InDefaultChord, const EMultipleKeyBindingIndex InChordIndex = EMultipleKeyBindingIndex::Primary);
	FUICommandInfoDecl& UserInterfaceType( EUserInterfaceActionType::Type InType );
	FUICommandInfoDecl& Icon( const FSlateIcon& InIcon );
	FUICommandInfoDecl& Description( const FText& InDesc );

	operator TSharedPtr<FUICommandInfo>() const;
	operator TSharedRef<FUICommandInfo>() const;

public:

	FUICommandInfoDecl( const TSharedRef<class FBindingContext>& InContext, const FName InCommandName, const FText& InLabel, const FText& InDesc  );

private:

	TSharedPtr<class FUICommandInfo> Info;
	const TSharedRef<FBindingContext>& Context;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBindingContextChanged, const FBindingContext&);

/**
 * Represents a context in which input bindings are valid
 */
class SLATE_API FBindingContext
	: public TSharedFromThis<FBindingContext>
{
public:
	/**
	 * Constructor
	 *
	 * @param InContextName		The name of the context
	 * @param InContextDesc		The localized description of the context
	 * @param InContextParent	Optional parent context.  Bindings are not allowed to be the same between parent and child contexts
	 * @param InStyleSetName	The style set to find the icons in, eg) FCoreStyle::Get().GetStyleSetName()
	 */
	FBindingContext( const FName InContextName, const FText& InContextDesc, const FName InContextParent, const FName InStyleSetName )
		: ContextName( InContextName )
		, ContextParent( InContextParent )
		, ContextDesc( InContextDesc )
		, StyleSetName( InStyleSetName )
	{
		check(!InStyleSetName.IsNone());
	}

	FBindingContext( const FBindingContext &Other )
		: ContextName( Other.ContextName )
		, ContextParent( Other.ContextParent )
		, ContextDesc( Other.ContextDesc )
		, StyleSetName( Other.StyleSetName )
	{}

	/**
	 * Creates a new command declaration used to populate commands with data
	 */
	FUICommandInfoDecl NewCommand( const FName InCommandName, const FText& InCommandLabel, const FText& InCommandDesc );

	/**
	 * @return The name of the context
	 */
	FName GetContextName() const { return ContextName; }

	/**
	 * @return The name of the parent context (or NAME_None if there isnt one)
	 */
	FName GetContextParent() const { return ContextParent; }

	/**
	 * @return The name of the style set to find the icons in
	 */
	FName GetStyleSetName() const { return StyleSetName; }

	/**
	 * @return The localized description of this context
	 */
	const FText& GetContextDesc() const { return ContextDesc; }

	friend uint32 GetTypeHash( const FBindingContext& Context )
	{
		return GetTypeHash( Context.ContextName );
	}

	bool operator==( const FBindingContext& Other ) const
	{
		return ContextName == Other.ContextName;
	}

	/** A delegate that is called when commands are registered or unregistered with a binding context */
	static FOnBindingContextChanged CommandsChanged;

private:

	/** The name of the context */
	FName ContextName;

	/** The name of the parent context */
	FName ContextParent;

	/** The description of the context */
	FText ContextDesc;

	/** The style set to find the icons in */
	FName StyleSetName;
};


class SLATE_API FUICommandInfo
{
	friend class FInputBindingManager;
	friend class FUICommandInfoDecl;

public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InBindingContext The name of the binding context to use.
	 */
	FUICommandInfo( const FName InBindingContext )
		: BindingContext( InBindingContext )
		, UserInterfaceType( EUserInterfaceActionType::Button )
	{
		ActiveChords.Empty(2);
		ActiveChords.Add(TSharedRef<FInputChord>(new FInputChord));
		ActiveChords.Add(TSharedRef<FInputChord>(new FInputChord));

		DefaultChords.Init(FInputChord(EKeys::Invalid, EModifierKey::None), 2);
	}

	/**
	 * Returns the friendly, localized string name of the first valid chord in the key bindings list that is required to perform the command
	 *
	 * @return	Localized friendly text for the chord
	 */
	const FText GetInputText() const;

	/**
	 * @return	Returns the active chord at the specified index for this command
	 */
	const TSharedRef<const FInputChord> GetActiveChord(const EMultipleKeyBindingIndex InChordIndex) const { return ActiveChords[static_cast<uint8>(InChordIndex)]; }
	
	/**
	* @return	Checks if there is an active chord for this command matching the input chord
	*/
	const bool HasActiveChord(const FInputChord InChord) const { return *ActiveChords[static_cast<uint8>(EMultipleKeyBindingIndex::Primary)] == InChord ||
																	*ActiveChords[static_cast<uint8>(EMultipleKeyBindingIndex::Secondary)] == InChord; }

	/**
	* @return	Checks if there is an active chord for this command matching the input chord
	*/
	const TSharedRef<const FInputChord> GetFirstValidChord() const {
		return ActiveChords[static_cast<uint8>(EMultipleKeyBindingIndex::Primary)]->IsValidChord()
				? ActiveChords[static_cast<uint8>(EMultipleKeyBindingIndex::Primary)]
				: ActiveChords[static_cast<uint8>(EMultipleKeyBindingIndex::Secondary)];
	}

	/**
	* @return	Checks if there is an active chord for this command matching the input chord
	*/
	const bool HasDefaultChord(const FInputChord InChord) const {
		return (DefaultChords[static_cast<uint8>(EMultipleKeyBindingIndex::Primary)] == InChord) ||
			(DefaultChords[static_cast<uint8>(EMultipleKeyBindingIndex::Secondary)] == InChord);
	}

	const FInputChord& GetDefaultChord(const EMultipleKeyBindingIndex InChordIndex) const { return DefaultChords[static_cast<uint8>(InChordIndex)]; }

	/** Utility function to make an FUICommandInfo */
	static void MakeCommandInfo( const TSharedRef<class FBindingContext>& InContext, TSharedPtr< FUICommandInfo >& OutCommand, const FName InCommandName, const FText& InCommandLabel, const FText& InCommandDesc, const FSlateIcon& InIcon, const EUserInterfaceActionType::Type InUserInterfaceType, const FInputChord& InDefaultChord, const FInputChord& InAlternateDefaultChord = FInputChord());

	/** Utility function to unregister an FUICommandInfo */
	static void UnregisterCommandInfo(const TSharedRef<class FBindingContext>& InContext, const TSharedRef<FUICommandInfo>& InCommand);

	/** @return The display label for this command */
	const FText& GetLabel() const { return Label; }

	/** @return The description of this command */
	const FText& GetDescription() const { return Description; }

	/** @return The icon to used when this command is displayed in UI that shows icons */
	const FSlateIcon& GetIcon() const { return Icon; }

	/** @return The type of command this is.  Used to determine what UI to create for it */
	EUserInterfaceActionType::Type GetUserInterfaceType() const { return UserInterfaceType; }
	
	/** @return The name of the command */
	FName GetCommandName() const { return CommandName; }

	/** @return The name of the context where the command is valid */
	FName GetBindingContext() const { return BindingContext; }

	/** Sets the new active chord for this command */
	void SetActiveChord( const FInputChord& NewChord, const EMultipleKeyBindingIndex InChordIndex );

	/** Removes the active chord from this command */
	void RemoveActiveChord(const EMultipleKeyBindingIndex InChordIndex);

	/** 
	 * Makes a tooltip for this command.
	 * @param	InText	Optional dynamic text to be displayed in the tooltip.
	 * @return	The tooltip widget
	 */
	TSharedRef<class SToolTip> MakeTooltip( const TAttribute<FText>& InText = TAttribute<FText>() , const TAttribute< EVisibility >& InToolTipVisibility = TAttribute<EVisibility>()) const;

private:

	/** Input commands that executes this action */
	TArray<TSharedRef<FInputChord>> ActiveChords;

	/** Default display name of the command */
	FText Label;

	/** Localized help text for this command */
	FText Description;

	/** The default input chords for this command (can be invalid) */
	TArray<FInputChord> DefaultChords;

	/** Brush name for icon to use in tool bars and menu items to represent this command */
	FSlateIcon Icon;

	/** Brush name for icon to use in tool bars and menu items to represent this command in its toggled on (checked) state*/
	FName UIStyle;

	/** Name of the command */
	FName CommandName;

	/** The context in which this command is active */
	FName BindingContext;

	/** The type of user interface to associated with this action */
	EUserInterfaceActionType::Type UserInterfaceType;
};
