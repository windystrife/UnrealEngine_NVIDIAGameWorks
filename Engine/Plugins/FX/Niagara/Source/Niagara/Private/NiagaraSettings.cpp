// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSettings.h"

UNiagaraSettings::UNiagaraSettings(const FObjectInitializer& ObjectInitlaizer)
	: Super(ObjectInitlaizer)
{

}

FName UNiagaraSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText UNiagaraSettings::GetSectionText() const
{
	return NSLOCTEXT("NiagaraPlugin", "NiagaraSettingsSection", "Niagara");
}
#endif

#if WITH_EDITOR
void UNiagaraSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		SettingsChangedDelegate.Broadcast(PropertyChangedEvent.Property->GetName(), this);
	}
}

UNiagaraSettings::FOnNiagaraSettingsChanged& UNiagaraSettings::OnSettingsChanged()
{
	return SettingsChangedDelegate;
}

UNiagaraSettings::FOnNiagaraSettingsChanged UNiagaraSettings::SettingsChangedDelegate;
#endif


