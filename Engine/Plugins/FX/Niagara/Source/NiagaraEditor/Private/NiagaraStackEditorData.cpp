// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackEditorData.h"

bool UNiagaraStackEditorData::GetModuleInputIsPinned(const FString& ModuleInputKey) const
{
	const bool* bIsPinnedPtr = ModuleInputKeyToPinnedMap.Find(ModuleInputKey);
	return bIsPinnedPtr != nullptr && *bIsPinnedPtr;
}

void UNiagaraStackEditorData::SetModuleInputIsPinned(const FString& ModuleInputKey, bool bIsPinned)
{
	ModuleInputKeyToPinnedMap.FindOrAdd(ModuleInputKey) = bIsPinned;
}

bool UNiagaraStackEditorData::GetModuleInputIsRenamePending(const FString& ModuleInputKey) const
{
	const bool* bIsRenamePendingPtr = ModuleInputKeyToRenamePendingMap.Find(ModuleInputKey);
	return bIsRenamePendingPtr != nullptr && *bIsRenamePendingPtr;
}

void UNiagaraStackEditorData::SetModuleInputIsRenamePending(const FString& ModuleInputKey, bool bIsRenamePending)
{
	ModuleInputKeyToRenamePendingMap.FindOrAdd(ModuleInputKey) = bIsRenamePending;
}

bool UNiagaraStackEditorData::GetStackEntryIsExpanded(const FString& StackEntryKey, bool bIsExpandedDefault) const
{
	const bool* bIsExpandedPtr = StackEntryKeyToExpandedMap.Find(StackEntryKey);
	return bIsExpandedPtr != nullptr ? *bIsExpandedPtr : bIsExpandedDefault;
}

void UNiagaraStackEditorData::SetStackEntryIsExpanded(const FString& StackEntryKey, bool bIsExpanded)
{
	StackEntryKeyToExpandedMap.FindOrAdd(StackEntryKey) = bIsExpanded;
}