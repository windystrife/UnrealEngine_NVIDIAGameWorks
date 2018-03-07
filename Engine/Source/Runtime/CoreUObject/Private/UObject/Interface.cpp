// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/Interface.h"
#include "UObject/Class.h"

#if WITH_EDITOR
IMPLEMENT_CORE_INTRINSIC_CLASS(UInterface, UObject,
	{
		Class->ClassAddReferencedObjects = &UInterface::AddReferencedObjects;
		Class->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true"));
		Class->SetMetaData(TEXT("CannotImplementInterfaceInBlueprint"), TEXT(""));
	}
);

#else

IMPLEMENT_CORE_INTRINSIC_CLASS(UInterface, UObject,
{
	Class->ClassAddReferencedObjects = &UInterface::AddReferencedObjects;
}
);

#endif
