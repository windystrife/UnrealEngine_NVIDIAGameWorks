// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Internationalization/TextNamespaceFwd.h"
#include "Containers/UnrealString.h"

namespace TextNamespaceUtil
{

static const TCHAR PackageNamespaceStartMarker = TEXT('[');
static const TCHAR PackageNamespaceEndMarker = TEXT(']');

/**
 * Given a text and package namespace, build the full version that should be used by the localization system.
 * This can also be used to "zero-out" the package namespace used by a text namespace (by passing an empty package namespace) while still leaving the package namespace markers in place.
 *
 * @param InTextNamespace				The namespace currently used by the FText instance.
 * @param InPackageNamespace			The namespace of the package owning the FText instance.
 * @param bAlwaysApplyPackageNamespace	If true, this will always apply the package namespace to the text namespace.
 *										If false, this will only apply the package namespace if the text namespace already contains package namespace makers.
 *
 * @return The full namespace that should be used by the localization system.
 */
CORE_API FString BuildFullNamespace(const FString& InTextNamespace, const FString& InPackageNamespace, const bool bAlwaysApplyPackageNamespace = false);

/**
 * Given a text namespace, extract any package namespace that may currently be present.
 *
 * @param InTextNamespace				The namespace currently used by the FText instance.
 *
 * @return The extracted package namespace component, or an empty string if there was no package namespace component.
 */
CORE_API FString ExtractPackageNamespace(const FString& InTextNamespace);

/**
 * Given a text namespace, strip any package namespace that may currently be present.
 * This is similar to calling BuildFullNamespace with an empty package namespace, however this version will also remove the package namespace markers.
 *
 * @param InTextNamespace				The namespace currently used by the FText instance.
 *
 * @return The namespace stripped of any package namespace component.
 */
CORE_API FString StripPackageNamespace(const FString& InTextNamespace);

#if USE_STABLE_LOCALIZATION_KEYS

/**
 * Given an archive, try and get the package namespace it should use for localization.
 *
 * @param InArchive						The archive to try and get the package namespace for.
 *
 * @return The package namespace, or an empty string if the archive has no package namespace set.
 */
CORE_API FString GetPackageNamespace(FArchive& InArchive);

#endif // USE_STABLE_LOCALIZATION_KEYS

}
