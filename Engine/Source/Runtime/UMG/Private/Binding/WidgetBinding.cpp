// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Binding/WidgetBinding.h"
#include "Components/Widget.h"

#define LOCTEXT_NAMESPACE "UMG"

UWidgetBinding::UWidgetBinding()
{
}

bool UWidgetBinding::IsSupportedDestination(UProperty* Property) const
{
	return IsSupportedSource(Property);
}

bool UWidgetBinding::IsSupportedSource(UProperty* Property) const
{
	if ( IsConcreteTypeCompatibleWithReflectedType<UObject*>(Property) )
	{
		if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property) )
		{
			return ObjectProperty->PropertyClass->IsChildOf(UWidget::StaticClass());
		}
	}

	return false;
}

UWidget* UWidgetBinding::GetValue() const
{
	//SCOPE_CYCLE_COUNTER(STAT_UMGBinding);

	if ( UObject* Source = SourceObject.Get() )
	{
		UObject* Value = nullptr;
		if ( SourcePath.GetValue<UObject*>(Source, Value) )
		{
			if ( UWidget* Widget = Cast<UWidget>(Value) )
			{
				return Widget;
			}
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
