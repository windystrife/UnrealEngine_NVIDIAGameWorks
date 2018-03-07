// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Binding/VisibilityBinding.h"
#include "Components/SlateWrapperTypes.h"

#define LOCTEXT_NAMESPACE "UMG"

UVisibilityBinding::UVisibilityBinding()
{
}

bool UVisibilityBinding::IsSupportedSource(UProperty* Property) const
{
	return IsSupportedDestination(Property);
}

bool UVisibilityBinding::IsSupportedDestination(UProperty* Property) const
{
	static const FName VisibilityEnum(TEXT("ESlateVisibility"));

	if ( UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property) )
	{
		return EnumProperty->GetEnum()->GetFName() == VisibilityEnum;
	}
	else if ( UByteProperty* ByteProperty = Cast<UByteProperty>(Property) )
	{
		if ( ByteProperty->IsEnum() )
		{
			return ByteProperty->Enum->GetFName() == VisibilityEnum;
		}
	}

	return false;
}

ESlateVisibility UVisibilityBinding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		uint8 Value = 0;
		if ( SourcePath.GetValue<uint8>(Source, Value) )
		{
			return static_cast<ESlateVisibility>( Value );
		}
	}

	return ESlateVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE
