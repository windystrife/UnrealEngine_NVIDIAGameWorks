// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class PROPERTYEDITOR_API FPropertyRestriction
{
public:

	FPropertyRestriction(const FText& InReason)
		: Reason(InReason)
	{
	}

	const FText& GetReason() const { return Reason; }

	bool IsValueHidden(const FString& Value) const;
	bool IsValueDisabled(const FString& Value) const;
	void AddHiddenValue(FString Value);
	void AddDisabledValue(FString Value);

	void RemoveHiddenValue(FString Value);
	void RemoveDisabledValue(FString Value);
	void RemoveAll();

private:

	TArray<FString> HiddenValues;
	TArray<FString> DisabledValues;
	FText Reason;
};
