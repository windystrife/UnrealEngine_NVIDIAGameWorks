// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "AbilitySystemGlobals.h"
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

struct FGameplayAbilitiesExec : public FSelfRegisteringExec
{
	FGameplayAbilitiesExec()
	{
	}

	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface
};

FGameplayAbilitiesExec GameplayAbilitiesExecInstance;

bool FGameplayAbilitiesExec::Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (Inworld == NULL)
	{
		return false;
	}

	bool bHandled = false;

	if (FParse::Command(&Cmd, TEXT("ToggleIgnoreAbilitySystemCooldowns")))
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
		AbilitySystemGlobals.ToggleIgnoreAbilitySystemCooldowns();
		bHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ToggleIgnoreAbilitySystemCosts")))
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
		AbilitySystemGlobals.ToggleIgnoreAbilitySystemCosts();
		bHandled = true;
	}

	return bHandled;
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
