// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"


/**
 * Interface for media options.
 */
class IMediaOptions
{
public:

/**
	 * Get the name of the desired native player.
	 *
	 * @return Native player name, or NAME_None to auto select.
	 */
	virtual FName GetDesiredPlayerName() const = 0;

	/**
	 * Get a Boolean media option.
	 *
	 * @param Key The name of the option to get.
	 * @param DefaultValue The default value to return if the option is not set.
	 * @return The option value.
	 */
	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const = 0;

	/**
	 * Get a double precision floating point media option.
	 *
	 * @param Key The name of the option to get.
	 * @param DefaultValue The default value to return if the option is not set.
	 * @return The option value.
	 */
	virtual double GetMediaOption(const FName& Key, double DefaultValue) const = 0;

	/**
	 * Get a signed integer media option.
	 *
	 * @param Key The name of the option to get.
	 * @param DefaultValue The default value to return if the option is not set.
	 * @return The option value.
	 */
	virtual int64 GetMediaOption(const FName& Key, int64 DefaultValue) const = 0;

	/**
	 * Get a string media option.
	 *
	 * @param Key The name of the option to get.
	 * @param DefaultValue The default value to return if the option is not set.
	 * @return The option value.
	 */
	virtual FString GetMediaOption(const FName& Key, const FString& DefaultValue) const = 0;

	/**
	 * Get a localized text media option.
	 *
	 * @param Key The name of the option to get.
	 * @param DefaultValue The default value to return if the option is not set.
	 * @return The option value.
	 */
	virtual FText GetMediaOption(const FName& Key, const FText& DefaultValue) const = 0;

	/**
	 * Check whether the specified option is set.
	 *
	 * @param Key The name of the option to check.
	 * @return true if the option is set, false otherwise.
	 */
	virtual bool HasMediaOption(const FName& Key) const = 0;
};
