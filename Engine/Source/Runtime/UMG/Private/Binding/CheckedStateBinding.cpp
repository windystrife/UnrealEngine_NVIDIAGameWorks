// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Binding/CheckedStateBinding.h"
#include "Styling/SlateTypes.h"

#define LOCTEXT_NAMESPACE "UMG"

UCheckedStateBinding::UCheckedStateBinding()
{
}

bool UCheckedStateBinding::IsSupportedSource(UProperty* Property) const
{
	return IsSupportedDestination(Property) || IsConcreteTypeCompatibleWithReflectedType<bool>(Property);
}

bool UCheckedStateBinding::IsSupportedDestination(UProperty* Property) const
{
	static const FName CheckBoxStateEnum(TEXT("ECheckBoxState"));

	if ( UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property) )
	{
		return EnumProperty->GetEnum()->GetFName() == CheckBoxStateEnum;
	}
	else if ( UByteProperty* ByteProperty = Cast<UByteProperty>(Property) )
	{
		if ( ByteProperty->IsEnum() )
		{
			return ByteProperty->Enum->GetFName() == CheckBoxStateEnum;
		}
	}

	return false;
}

ECheckBoxState UCheckedStateBinding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		if ( bConversion.Get(EConversion::None) == EConversion::None )
		{
			uint8 Value = 0;
			if ( SourcePath.GetValue<uint8>(Source, Value) )
			{
				bConversion = EConversion::None;
				return static_cast<ECheckBoxState>(Value);
			}
		}

		if ( bConversion.Get(EConversion::Bool) == EConversion::Bool )
		{
			bool Value = false;
			if ( SourcePath.GetValue<bool>(Source, Value) )
			{
				bConversion = EConversion::Bool;
				return Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}

#undef LOCTEXT_NAMESPACE
