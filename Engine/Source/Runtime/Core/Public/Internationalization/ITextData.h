// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Internationalization/TextLocalizationManager.h"

class FTextHistory;

/** 
 * Interface to the internal data for an FText.
 * Various derived types are optimized to reduce memory allocation overhead.
 */
class ITextData
{
public:
	virtual ~ITextData()
	{
	}

	/**
	 * True if this text data owns its localized string pointer, and allows you to call GetMutableLocalizedString on it
	 */
	virtual bool OwnsLocalizedString() const = 0;

	/**
	 * Get the string to use for display purposes.
	 * This may have come from the localization manager, or may been generated at runtime (eg, via FText::AsNumber).
	 */
	virtual const FString& GetDisplayString() const = 0;

	/** 
	 * Get the string pointer that was retrieved from the text localization manager.
	 * Text that was generated at runtime by default will not have one of these, and you must call Persist() to generate one.
	 */
	virtual FTextDisplayStringPtr GetLocalizedString() const = 0;

	/** 
	 * Get a mutable reference to the localized string associated with this text (used when loading/saving text).
	 */
	virtual FTextDisplayStringPtr& GetMutableLocalizedString() = 0;

	/**
	 * Get the history associated with this text.
	 */
	virtual const FTextHistory& GetTextHistory() const = 0;

	/**
	 * Get a mutable reference to the history associated with this text (used when loading/saving text).
	 */
	virtual FTextHistory& GetMutableTextHistory() = 0;

	/**
	 * Persist this text so that it can be stored in the localization manager.
	 */
	virtual void PersistText() = 0;

	/**
	 * Get the global history revision associated with this text instance.
	 */
	virtual uint16 GetGlobalHistoryRevision() const = 0;

	/**
	 * Get the local history revision associated with this text instance.
	 */
	virtual uint16 GetLocalHistoryRevision() const = 0;

	/**
	 * Assign a new history object to this instance.
	 * @note: There is no RTTI on these types, so it is your responsibility to make sure that the history object you're assigning is of the correct type!
	 */
	template <typename THistoryType>
	void SetTextHistory(THistoryType&& InHistory)
	{
		// Need to cast to avoid slicing data
		static_cast<THistoryType&>(GetMutableTextHistory()) = Forward<THistoryType>(InHistory);
	}
};
