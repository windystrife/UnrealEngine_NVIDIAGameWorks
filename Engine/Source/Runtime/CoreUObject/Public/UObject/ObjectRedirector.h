// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnRedirector.h: Object redirector definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

/**
 * This class will redirect an object load to another object, so if an object is renamed
 * to a different package or group, external references to the object can be found
 */
class UObjectRedirector : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UObjectRedirector, UObject, 0, TEXT("/Script/CoreUObject"), CASTCLASS_None, COREUOBJECT_API)

	// Variables.
	UObject*		DestinationObject;
	// UObject interface.
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	void Serialize( FArchive& Ar ) override;
	virtual bool NeedsLoadForEditorGame() const override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	return true if property values were added to the map.
	 */
	virtual bool GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, uint32 ExportFlags=0 ) const override;
};
