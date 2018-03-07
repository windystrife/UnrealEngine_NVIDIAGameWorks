// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UWidget;

/**
 * Handle to a widget material for easy getting and setting of the material.  Not designed to be stored
 */
class UMG_API FWidgetMaterialHandle
{
public:
	FWidgetMaterialHandle()
		: TypeName(NAME_None)
		, Data(nullptr)
	{}

	FWidgetMaterialHandle(FName InTypeName, void* InData)
		: TypeName(InTypeName)
		, Data(InData)
	{}

	/** @return true if this handle points to valid data */
	bool IsValid() const { return TypeName != NAME_None && Data != nullptr; }

	/** Gets the currently bound material */
	UMaterialInterface* GetMaterial() const;

	/** Sets the new bound material */
	void SetMaterial(UMaterialInterface* InMaterial);
private:
	/** Struct typename for that the data is pointing to */
	FName TypeName;
	/** Pointer to the struct data holding the material */
	void* Data;
};

namespace WidgetMaterialTrackUtilities
{
	/** Gets a material handle from a property on a widget by the properties FName path. */
	UMG_API FWidgetMaterialHandle GetMaterialHandle(UWidget* Widget, const TArray<FName>& BrushPropertyNamePath);

	/** Gets the property paths on a widget which are slate brush properties, and who's slate brush has a valid material. */
	UMG_API void GetMaterialBrushPropertyPaths( UWidget* Widget, TArray<TArray<UProperty*>>& MaterialBrushPropertyPaths ); 

	/** Converts a property name path into a single name which is appropriate for a track name. */
	UMG_API FName GetTrackNameFromPropertyNamePath( const  TArray<FName>& PropertyNamePath );
}
