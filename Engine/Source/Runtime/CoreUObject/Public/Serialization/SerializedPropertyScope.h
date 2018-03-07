// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncPackage.h: Unreal async loading definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/** Helper class to set and restore serialized property on an archive */
class COREUOBJECT_API FSerializedPropertyScope
{
	FArchive& Ar;
	UProperty* Property;
	UProperty* OldProperty;
#if WITH_EDITORONLY_DATA
	void PushEditorOnlyProperty();
	void PopEditorOnlyProperty();
#endif
public:
	FSerializedPropertyScope(FArchive& InAr, UProperty* InProperty, const UProperty* OnlyIfOldProperty = nullptr)
		: Ar(InAr)
		, Property(InProperty)
	{
		OldProperty = Ar.GetSerializedProperty();
		if (!OnlyIfOldProperty || OldProperty == OnlyIfOldProperty)
		{
			Ar.SetSerializedProperty(Property);
		}
		
#if WITH_EDITORONLY_DATA
		PushEditorOnlyProperty();
#endif
	}
	~FSerializedPropertyScope()
	{
#if WITH_EDITORONLY_DATA
		PopEditorOnlyProperty();
#endif
		Ar.SetSerializedProperty(OldProperty);
	}
};
