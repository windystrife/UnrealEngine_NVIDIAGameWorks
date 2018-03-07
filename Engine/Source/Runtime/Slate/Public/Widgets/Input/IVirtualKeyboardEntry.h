// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"

// @todo - hook up keyboard types
enum EKeyboardType
{
	Keyboard_Default,
	Keyboard_Number,
	Keyboard_Web,
	Keyboard_Email,
	Keyboard_Password,
	Keyboard_AlphaNumeric,
};

enum class ETextEntryType : uint8
{
	/** The keyboard entry was canceled, some valid text may still be available. */
	TextEntryCanceled,
	/** The keyboard entry was accepted, full text is available. */
	TextEntryAccepted,
	/** They keyboard is providing a periodic update of the text entered so far. */
	TextEntryUpdated
};

class SLATE_API IVirtualKeyboardEntry
{

public:
	virtual ~IVirtualKeyboardEntry() {}

	/**
	* Sets the text to that entered by the virtual keyboard
	*
	* @param InNewText  The new text
	* @param TextEntryType What type of text update is being provided
	* @param CommitType If we are sending a TextCommitted event, what commit type is it
	*/
	virtual void SetTextFromVirtualKeyboard(const FText& InNewText, ETextEntryType TextEntryType) = 0;
	
	/**
	* Returns the text.
	*
	* @return  Text
	*/
	virtual FText GetText() const = 0;

	/**
	* Returns the hint text.
	*
	* @return  HintText
	*/
	virtual FText GetHintText() const = 0;

	/**
	* Returns the virtual keyboard type.
	*
	* @return  VirtualKeyboardType
	*/
	virtual EKeyboardType GetVirtualKeyboardType() const = 0;

	/**
	* Returns whether the entry is multi-line
	*
	* @return Whether the entry is multi-line
	*/
	virtual bool IsMultilineEntry() const = 0;
};
