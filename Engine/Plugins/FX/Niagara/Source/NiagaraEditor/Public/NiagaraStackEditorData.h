// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEditorData.generated.h"

/** Editor only UI data for emitters. */
UCLASS()
class UNiagaraStackEditorData : public UObject
{
	GENERATED_BODY()

public:
	/*
	 * Gets whether or not a module has been pinned.
	 * @param ModuleInputKey A unique key for the module. 
	 */
	bool GetModuleInputIsPinned(const FString& ModuleInputKey) const;

	/*
	 * Sets whether or not a module is pinned.
	 * @param ModuleInputKey A unique key for the module.
	 * @param bIsPinned Whether or not the module is pinned.
	 */
	void SetModuleInputIsPinned(const FString& ModuleInputKey, bool bIsPinned);

	/*
	* Gets whether or not a module has a rename pending.
	* @param ModuleInputKey A unique key for the module.
	*/
	bool GetModuleInputIsRenamePending(const FString& ModuleInputKey) const;

	/*
	* Sets whether or not a module is pinned.
	* @param ModuleInputKey A unique key for the module.
	* @param bIsRenamePending Whether or not the module is pinned.
	*/
	void SetModuleInputIsRenamePending(const FString& ModuleInputKey, bool bIsRenamePending);

	/*
	 * Gets whether or not a stack entry is Expanded.
	 * @param bIsExpandedDefault The default value to return if the expanded state hasn't been set for the stack entry.
	 * @param StackItemKey A unique key for the entry.
	 */
	bool GetStackEntryIsExpanded(const FString& StackEntryKey, bool bIsExpandedDefault) const;

	/*
	 * Sets whether or not a stack entry is Expanded.
	 * @param StackEntryKey A unique key for the entry.
	 * @param bIsExpanded Whether or not the entry is expanded.
	 */
	void SetStackEntryIsExpanded(const FString& StackEntryKey, bool bIsExpanded);

private:

	UPROPERTY()
	TMap<FString, bool> ModuleInputKeyToPinnedMap;

	UPROPERTY()
	TMap<FString, bool> ModuleInputKeyToRenamePendingMap;

	UPROPERTY()
	TMap<FString, bool> StackEntryKeyToExpandedMap;
};