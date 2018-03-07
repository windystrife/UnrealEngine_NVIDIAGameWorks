// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace TextNamespaceUtil
{

#if USE_STABLE_LOCALIZATION_KEYS

/**
 * Given a package, try and get the namespace it should use for localization.
 *
 * @param InPackage			The package to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace.
 */
COREUOBJECT_API FString GetPackageNamespace(const UPackage* InPackage);

/**
 * Given an object, try and get the namespace it should use for localization (from its owner package).
 *
 * @param InObject			The object to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace.
 */
COREUOBJECT_API FString GetPackageNamespace(const UObject* InObject);

/**
 * Given a package, try and ensure it has a namespace it should use for localization.
 *
 * @param InPackage			The package to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace and one could not be added.
 */
COREUOBJECT_API FString EnsurePackageNamespace(UPackage* InPackage);

/**
 * Given an object, try and ensure it has a namespace it should use for localization (from its owner package).
 *
 * @param InObject			The object to try and get the namespace for.
 *
 * @return The package namespace, or an empty string if the package has no namespace and one could not be added.
 */
COREUOBJECT_API FString EnsurePackageNamespace(UObject* InObject);

/**
 * Given a package, clear any namespace it has set for localization.
 *
 * @param InPackage			The package to clear the namespace for.
 */
COREUOBJECT_API void ClearPackageNamespace(UPackage* InPackage);

/**
 * Given an object, clear any namespace it has set for localization (from its owner package).
 *
 * @param InObject			The object to clear the namespace for.
 */
COREUOBJECT_API void ClearPackageNamespace(UObject* InObject);

/**
 * Given a package, force it to have the given namespace for localization (even if a transient package!).
 *
 * @param InPackage			The package to set the namespace for.
 * @param InNamespace		The namespace to set.
 */
COREUOBJECT_API void ForcePackageNamespace(UPackage* InPackage, const FString& InNamespace);

/**
 * Given an object, force it to have the given namespace for localization (from its owner package, even if a transient package!).
 *
 * @param InObject			The object to set the namespace for.
 * @param InNamespace		The namespace to set.
 */
COREUOBJECT_API void ForcePackageNamespace(UObject* InObject, const FString& InNamespace);

#endif // USE_STABLE_LOCALIZATION_KEYS

}
