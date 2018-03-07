// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Interface for types that provide caching for package localization. */
class IPackageLocalizationCache
{
public:
	virtual ~IPackageLocalizationCache() {}

	/**
	 * Update this cache, but only if it is dirty.
	 */
	virtual void ConditionalUpdateCache() = 0;

	/**
	 * Try and find the localized package name for the given source package for the active culture.
	 *
	 * @param InSourcePackageName	The name of the source package to find.
	 *
	 * @return The localized package name, or NAME_None if there is no localized package.
	 */
	virtual FName FindLocalizedPackageName(const FName InSourcePackageName) = 0;

	/**
	 * Try and find the localized package name for the given source package for the given culture.
	 *
	 * @param InSourcePackageName	The name of the source package to find.
	 * @param InCultureName			The name of the culture to find the package for.
	 *
	 * @return The localized package name, or NAME_None if there is no localized package.
	 */
	virtual FName FindLocalizedPackageNameForCulture(const FName InSourcePackageName, const FString& InCultureName) = 0;
};
