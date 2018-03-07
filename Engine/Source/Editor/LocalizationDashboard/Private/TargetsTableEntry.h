// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

class ULocalizationTarget;

struct FTargetsTableEntry
{
	TSharedPtr<IPropertyHandle> TargetObjectPropertyHandle;
	int32 CultureIndex;

	FTargetsTableEntry()
		: CultureIndex(INDEX_NONE)
	{}

	FTargetsTableEntry(const TSharedPtr<IPropertyHandle>& InTargetObjectPropertyHandle, const int32 InCultureIndex = INDEX_NONE)
		: TargetObjectPropertyHandle(InTargetObjectPropertyHandle)
		, CultureIndex(InCultureIndex)
	{}

	ULocalizationTarget* GetTarget()
	{
		if (TargetObjectPropertyHandle.IsValid() && TargetObjectPropertyHandle->IsValidHandle())
		{
			UObject* Value = nullptr;
			TargetObjectPropertyHandle->GetValue(Value);
			return Cast<ULocalizationTarget>(Value);
		}
		return nullptr;
	}
};