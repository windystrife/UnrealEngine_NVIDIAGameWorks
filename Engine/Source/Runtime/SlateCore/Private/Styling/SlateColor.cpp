// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Styling/SlateColor.h"
#include "UObject/PropertyTag.h"

bool FSlateColor::SerializeFromMismatchedTag( const FPropertyTag& Tag, FArchive& Ar )
{
	if (Tag.Type == NAME_StructProperty)
	{
		if (Tag.StructName == NAME_Color)
		{
			FColor OldColor;
			Ar << OldColor;
			*this = FSlateColor(FLinearColor(OldColor));

			return true;
		}
		else if(Tag.StructName == NAME_LinearColor)
		{
			FLinearColor OldColor;
			Ar << OldColor;
			*this = FSlateColor(OldColor);

			return true;
		}
	}

	return false;
}
