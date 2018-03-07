// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterHandleViewModel.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraObjectSelection.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeInput.h"
#include "NiagaraScriptOutputCollectionViewModel.h"
#include "NotificationManager.h"
#include "SNotificationList.h"

#include "ScopedTransaction.h"
#include "AssetEditorManager.h"

#define LOCTEXT_NAMESPACE "EmitterHandleViewModel"

FNiagaraEmitterHandleViewModel::FNiagaraEmitterHandleViewModel(FNiagaraEmitterHandle* InEmitterHandle, TWeakPtr<FNiagaraEmitterInstance> InSimulation, UNiagaraSystem& InOwningSystem)
	: EmitterHandle(InEmitterHandle)
	, OwningSystem(InOwningSystem)
	, EmitterViewModel(MakeShareable(new FNiagaraEmitterViewModel((InEmitterHandle ? InEmitterHandle->GetInstance() : nullptr), InSimulation)))
{
}

FNiagaraEmitterHandleViewModel::~FNiagaraEmitterHandleViewModel()
{
}

bool FNiagaraEmitterHandleViewModel::Set(FNiagaraEmitterHandle* InEmitterHandle, TWeakPtr<FNiagaraEmitterInstance> InSimulation, UNiagaraSystem& InOwningSystem)
{
	if (&OwningSystem != &InOwningSystem)
	{
		return false;
	}

	SetEmitterHandle(InEmitterHandle);
	SetSimulation(InSimulation);
	
	UNiagaraEmitter* EmitterProperties = nullptr;
	if (InEmitterHandle != nullptr)
	{
		EmitterProperties = InEmitterHandle->GetInstance();
	}
	return EmitterViewModel->Set(EmitterProperties, InSimulation);
}


void FNiagaraEmitterHandleViewModel::SetEmitterHandle(FNiagaraEmitterHandle* InEmitterHandle)
{
	EmitterHandle = InEmitterHandle;
}

void FNiagaraEmitterHandleViewModel::SetSimulation(TWeakPtr<FNiagaraEmitterInstance> InSimulation)
{
	EmitterViewModel->SetSimulation(InSimulation);
}

FGuid FNiagaraEmitterHandleViewModel::GetId() const
{
	if (EmitterHandle)
	{
		return EmitterHandle->GetId();
	}
	return FGuid();
}

FText FNiagaraEmitterHandleViewModel::GetIdText() const
{
	return FText::FromString( GetId().ToString() );
}


FText FNiagaraEmitterHandleViewModel::GetErrorText() const
{
	switch (EmitterViewModel->GetLatestCompileStatus())
	{
	case ENiagaraScriptCompileStatus::NCS_Unknown:
	case ENiagaraScriptCompileStatus::NCS_BeingCreated:
		return LOCTEXT("NiagaraEmitterHandleCompileStatusUnknown", "Needs compilation & refresh.");
	case ENiagaraScriptCompileStatus::NCS_UpToDate:
		return LOCTEXT("NiagaraEmitterHandleCompileStatusUpToDate", "Compiled");
	default:
		return LOCTEXT("NiagaraEmitterHandleCompileStatusError", "Error! Needs compilation & refresh.");
	}
}

FSlateColor FNiagaraEmitterHandleViewModel::GetErrorTextColor() const
{
	switch (EmitterViewModel->GetLatestCompileStatus())
	{
	case ENiagaraScriptCompileStatus::NCS_Unknown:
	case ENiagaraScriptCompileStatus::NCS_BeingCreated:
		return FSlateColor(FLinearColor::Yellow);
	case ENiagaraScriptCompileStatus::NCS_UpToDate:
		return FSlateColor(FLinearColor::Green);
	default:
		return FSlateColor(FLinearColor::Red);
	}
}

EVisibility FNiagaraEmitterHandleViewModel::GetErrorTextVisibility() const
{
	return EmitterViewModel->GetLatestCompileStatus() != ENiagaraScriptCompileStatus::NCS_UpToDate ? EVisibility::Visible : EVisibility::Collapsed;
}


FText FNiagaraEmitterHandleViewModel::GetSourceSynchronizationText() const
{
	
	if (EmitterHandle->IsSynchronizedWithSource())
	{
		return LOCTEXT("SynchronizedWithSource", "Up-To-Date");
	}
	else
	{
		return LOCTEXT("NotSynchronizedWithSource", "Not Up-To-Date");
	}
}

EVisibility FNiagaraEmitterHandleViewModel::GetSourceSynchronizationTextVisibility() const
{
	if (OwningSystem.GetAutoImportChangedEmitters())
	{
		return EVisibility::Collapsed;
	}
	else
	{
		return EVisibility::Visible;
	}
}

FSlateColor FNiagaraEmitterHandleViewModel::GetSourceSynchronizationTextColor() const
{
	if (EmitterHandle->IsSynchronizedWithSource())
	{
		return FSlateColor::UseForeground();
	}
	else
	{
		return FSlateColor(FLinearColor::Yellow);
	}
}

FName FNiagaraEmitterHandleViewModel::GetName() const
{
	if (EmitterHandle)
	{
		return EmitterHandle->GetName();
	}
	return FName();
}

void FNiagaraEmitterHandleViewModel::SetName(FName InName)
{
	if (EmitterHandle && EmitterHandle->GetName() == InName)
	{
		return;
	}

	if (EmitterHandle)
	{
		TSet<FName> OtherEmitterNames;
		for (const FNiagaraEmitterHandle& OtherEmitterHandle : OwningSystem.GetEmitterHandles())
		{
			if (OtherEmitterHandle.GetId() != EmitterHandle->GetId())
			{
				OtherEmitterNames.Add(OtherEmitterHandle.GetName());
			}
		}
		FName UniqueName = FNiagaraEditorUtilities::GetUniqueName(InName, OtherEmitterNames);

		FScopedTransaction ScopedTransaction(NSLOCTEXT("NiagaraEmitterEditor", "EditEmitterNameTransaction", "Edit emitter name"));
		OwningSystem.Modify();
		EmitterHandle->SetName(UniqueName);
		OnPropertyChangedDelegate.Broadcast();
	}
}

FText FNiagaraEmitterHandleViewModel::GetNameText() const
{
	if (EmitterHandle)
	{
		return FText::FromName(EmitterHandle->GetName());
	}
	return FText();
}

void FNiagaraEmitterHandleViewModel::OnNameTextComitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	SetName(*InText.ToString());
}

bool FNiagaraEmitterHandleViewModel::VerifyNameTextChanged(const FText& NewText, FText& OutErrorMessage)
{
	FName NewName = *NewText.ToString();
	if (NewName == FName())
	{
		OutErrorMessage = NSLOCTEXT("NiagaraEmitterEditor", "NiagaraInputNameEmptyWarn", "Cannot have empty name!");
		return false;
	}
	return true;
}

bool FNiagaraEmitterHandleViewModel::GetIsEnabled() const
{
	if (EmitterHandle)
	{
		return EmitterHandle->GetIsEnabled();
	}
	return false;
}

void FNiagaraEmitterHandleViewModel::SetIsEnabled(bool bInIsEnabled)
{
	if (EmitterHandle && EmitterHandle->GetIsEnabled() != bInIsEnabled)
	{
		FScopedTransaction ScopedTransaction(NSLOCTEXT("NiagaraEmitterEditor", "EditEmitterEnabled", "Change emitter enabled state"));
		OwningSystem.Modify();
		EmitterHandle->SetIsEnabled(bInIsEnabled);
		OnPropertyChangedDelegate.Broadcast();
	}
}

ECheckBoxState FNiagaraEmitterHandleViewModel::GetIsEnabledCheckState() const
{
	if (EmitterHandle)
	{
		return EmitterHandle->GetIsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void FNiagaraEmitterHandleViewModel::OnIsEnabledCheckStateChanged(ECheckBoxState InCheckState)
{
	SetIsEnabled(InCheckState == ECheckBoxState::Checked);
}

FNiagaraEmitterHandle* FNiagaraEmitterHandleViewModel::GetEmitterHandle()
{
	return EmitterHandle;
}

TSharedRef<FNiagaraEmitterViewModel> FNiagaraEmitterHandleViewModel::GetEmitterViewModel()
{
	return EmitterViewModel;
}

void FNiagaraEmitterHandleViewModel::CompileScripts()
{
	EmitterViewModel->CompileScripts();
}



void FNiagaraEmitterHandleViewModel::RefreshFromSource()
{
	FScopedTransaction ScopedTransaction(NSLOCTEXT("NiagaraEmitterEditor", "RefreshFromSource", "Reset emitter from source."));
	OwningSystem.Modify();

	if (EmitterHandle)
	{
		{
			// Pull in changes to the emitter asset by copying the source scripts, compiling and then copying over parameter values
			// where relevant.
			if (EmitterHandle->RefreshFromSource() == false)
			{
				UE_LOG(LogNiagaraEditor, Error, TEXT("Failed to compile during refresh. Refresh cancelled. %s"), *EmitterHandle->GetSource()->GetPathName());

				const FText NotificationText = FText::Format(LOCTEXT("FailedScriptRefresh", "'{0}' failed to refresh due to compile errors. Please see log."), FText::FromString(EmitterHandle->GetSource()->GetName()));

				FNotificationInfo Info(NotificationText);
				Info.bFireAndForget = true;
				Info.bUseThrobber = true;
				Info.bUseSuccessFailIcons = true;
				Info.ExpireDuration = 10.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
		}

		EmitterViewModel->SetEmitter(EmitterHandle->GetInstance());
	}
	OnPropertyChangedDelegate.Broadcast();
}

void FNiagaraEmitterHandleViewModel::ResetToSource()
{
	FScopedTransaction ScopedTransaction(NSLOCTEXT("NiagaraEmitterEditor", "ResetToSource", "Reset emitter to source."));
	OwningSystem.Modify();
	
	if (EmitterHandle)
	{
		EmitterHandle->ResetToSource();
		EmitterViewModel->SetEmitter(EmitterHandle->GetInstance());
	}
	OnPropertyChangedDelegate.Broadcast();
}

void FNiagaraEmitterHandleViewModel::OpenSourceEmitter()
{
	if (EmitterHandle)
	{
		FAssetEditorManager::Get().OpenEditorForAsset(const_cast<UNiagaraEmitter*>(EmitterHandle->GetSource()));
	}
}

FNiagaraEmitterHandleViewModel::FOnPropertyChanged& FNiagaraEmitterHandleViewModel::OnPropertyChanged()
{
	return OnPropertyChangedDelegate;
}

#undef LOCTEXT_NAMESPACE