// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorSettings.h"

UNiagaraEditorSettings::UNiagaraEditorSettings(const FObjectInitializer& ObjectInitlaizer)
	: Super(ObjectInitlaizer)
{
	bAutoCompile = true;
}

FName UNiagaraEditorSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText UNiagaraEditorSettings::GetSectionText() const
{
	return NSLOCTEXT("NiagaraEditorPlugin", "NiagaraEditorSettingsSection", "Niagara Editor");
}
#endif

#if WITH_EDITOR
void UNiagaraEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		SettingsChangedDelegate.Broadcast(PropertyChangedEvent.Property->GetName(), this);
	}
}

UNiagaraEditorSettings::FOnNiagaraEditorSettingsChanged& UNiagaraEditorSettings::OnSettingsChanged()
{
	return SettingsChangedDelegate;
}

UNiagaraEditorSettings::FOnNiagaraEditorSettingsChanged UNiagaraEditorSettings::SettingsChangedDelegate;
#endif


