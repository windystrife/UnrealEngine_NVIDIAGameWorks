// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PortableObjectPipeline.generated.h"

class FLocTextHelper;

UENUM()
enum class ELocalizedTextCollapseMode : uint8
{
	/** Collapse texts with the same text identity (namespace + key) and source text (default 4.15+ behavior). */
	IdenticalTextIdAndSource			UMETA(DisplayName = "Identical Text Identity (Namespace + Key) and Source Text"),
	/** Collapse texts with the same package ID, text identity (namespace + key), and source text (deprecated 4.14 behavior, removed in 4.17). */
	IdenticalPackageIdTextIdAndSource	UMETA(DisplayName = "Identical Package ID, Text Identity (Namespace + Key) and Source Text", Hidden),
	/** Collapse texts with the same namespace and source text (legacy pre-4.14 behavior). */
	IdenticalNamespaceAndSource			UMETA(DisplayName = "Identical Namespace and Source Text"),
};

namespace PortableObjectPipeline
{
	/** Update the given LocTextHelper with the translation data imported from the PO file for the given culture */
	LOCALIZATION_API bool Import(FLocTextHelper& InLocTextHelper, const FString& InCulture, const FString& InPOFilePath, const ELocalizedTextCollapseMode InTextCollapseMode);

	/** Update the given LocTextHelper with the translation data imported from the PO file for all cultures */
	LOCALIZATION_API bool ImportAll(FLocTextHelper& InLocTextHelper, const FString& InPOCultureRootPath, const FString& InPOFilename, const ELocalizedTextCollapseMode InTextCollapseMode, const bool bUseCultureDirectory);

	/** Use the given LocTextHelper to generate a new PO file using the translation data for the given culture */
	LOCALIZATION_API bool Export(FLocTextHelper& InLocTextHelper, const FString& InCulture, const FString& InPOFilePath, const ELocalizedTextCollapseMode InTextCollapseMode, const bool bShouldPersistComments);

	/** Use the given LocTextHelper to generate a new PO file using the translation data for all cultures */
	LOCALIZATION_API bool ExportAll(FLocTextHelper& InLocTextHelper, const FString& InPOCultureRootPath, const FString& InPOFilename, const ELocalizedTextCollapseMode InTextCollapseMode, const bool bShouldPersistComments, const bool bUseCultureDirectory);
}
