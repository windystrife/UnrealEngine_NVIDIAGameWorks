// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Binding/TextBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

UTextBinding::UTextBinding()
{
}

bool UTextBinding::IsSupportedDestination(UProperty* Property) const
{
	return IsConcreteTypeCompatibleWithReflectedType<FText>(Property);
}

bool UTextBinding::IsSupportedSource(UProperty* Property) const
{
	return
		IsConcreteTypeCompatibleWithReflectedType<FText>(Property) ||
		IsConcreteTypeCompatibleWithReflectedType<FString>(Property);
}

void UTextBinding::Bind(UProperty* Property, FScriptDelegate* Delegate)
{
	if ( IsConcreteTypeCompatibleWithReflectedType<FText>(Property) )
	{
		static const FName BinderFunction(TEXT("GetTextValue"));
		Delegate->BindUFunction(this, BinderFunction);
	}
	else if ( IsConcreteTypeCompatibleWithReflectedType<FString>(Property) )
	{
		static const FName BinderFunction(TEXT("GetStringValue"));
		Delegate->BindUFunction(this, BinderFunction);
	}
}

FText UTextBinding::GetTextValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		if ( !bNeedsConversion.Get(false) )
		{
			FText TextValue = FText::GetEmpty();
			if ( SourcePath.GetValue<FText>(Source, TextValue) )
			{
				bNeedsConversion = false;
				return TextValue;
			}
		}

		if ( bNeedsConversion.Get(true) )
		{
			FString StringValue;
			if ( SourcePath.GetValue<FString>(Source, StringValue) )
			{
				bNeedsConversion = true;
				return FText::FromString(StringValue);
			}
		}
	}

	return FText::GetEmpty();
}

FString UTextBinding::GetStringValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		if ( !bNeedsConversion.Get(false) )
		{
			FString StringValue;
			if ( SourcePath.GetValue<FString>(Source, StringValue) )
			{
				bNeedsConversion = false;
				return StringValue;
			}
		}

		if ( bNeedsConversion.Get(true) )
		{
			FText TextValue = FText::GetEmpty();
			if ( SourcePath.GetValue<FText>(Source, TextValue) )
			{
				bNeedsConversion = true;
				return TextValue.ToString();
			}
		}
	}

	return FString();
}

#undef LOCTEXT_NAMESPACE
