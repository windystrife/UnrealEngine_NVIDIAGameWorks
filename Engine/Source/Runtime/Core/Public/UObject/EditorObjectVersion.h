// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"

// Custom serialization version for changes made in Dev-Editor stream
struct CORE_API FEditorObjectVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,
		// Localizable text gathered and stored in packages is now flagged with a localizable text gathering process version
		GatheredTextProcessVersionFlagging,
		// Fixed several issues with the gathered text cache stored in package headers
		GatheredTextPackageCacheFixesV1,
		// Added support for "root" meta-data (meta-data not associated with a particular object in a package)
		RootMetaDataSupport,
		// Fixed issues with how Blueprint bytecode was cached
		GatheredTextPackageCacheFixesV2,
		// Updated FFormatArgumentData to allow variant data to be marshaled from a BP into C++
		TextFormatArgumentDataIsVariant,
		// Changes to SplineComponent
		SplineComponentCurvesInStruct,
		// Updated ComboBox to support toggling the menu open, better controller support
		ComboBoxControllerSupportUpdate,
		// Refactor mesh editor materials
		RefactorMeshEditorMaterials,
		// Added UFontFace assets
		AddedFontFaceAssets,
		// Add UPROPERTY for TMap of Mesh section, so the serialize will be done normally (and export to text will work correctly)
		UPropertryForMeshSection,
		// Update the schema of all widget blueprints to use the WidgetGraphSchema
		WidgetGraphSchema,
		// Added a specialized content slot to the background blur widget
		AddedBackgroundBlurContentSlot,
		// Updated UserDefinedEnums to have stable keyed display names
		StableUserDefinedEnumDisplayNames,
		// Added "Inline" option to UFontFace assets
		AddedInlineFontFaceAssets,
		// Fix a serialization issue with static mesh FMeshSectionInfoMap UProperty
		UPropertryForMeshSectionSerialize,
		// Adding a version bump for the new fast widget construction in case of problems.
		FastWidgetTemplates,
		// Update material thumbnails to be more intelligent on default primitive shape for certain material types
		MaterialThumbnailRenderingChanges,
		// Introducing a new clipping system for Slate/UMG
		NewSlateClippingSystem,
		// MovieScene Meta Data added as native Serialization
		MovieSceneMetaDataSerialization,
		// Text gathered from properties now adds two variants: a version without the package localization ID (for use at runtime), and a version with it (which is editor-only)
		GatheredTextEditorOnlyPackageLocId,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FEditorObjectVersion() {}
};
