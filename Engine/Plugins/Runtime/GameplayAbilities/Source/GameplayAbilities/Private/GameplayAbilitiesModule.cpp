// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilitiesModule.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/HUD.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "GameplayDebuggerCategory_Abilities.h"
#endif // WITH_GAMEPLAY_DEBUGGER

class FGameplayAbilitiesModule : public IGameplayAbilitiesModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	virtual UAbilitySystemGlobals* GetAbilitySystemGlobals() override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IGameplayAbilitiesModule_GetAbilitySystemGlobals);
		// Defer loading of globals to the first time it is requested
		if (!AbilitySystemGlobals)
		{
			FSoftClassPath AbilitySystemClassName = (UAbilitySystemGlobals::StaticClass()->GetDefaultObject<UAbilitySystemGlobals>())->AbilitySystemGlobalsClassName;

			UClass* SingletonClass = AbilitySystemClassName.TryLoadClass<UObject>();
			checkf(SingletonClass != nullptr, TEXT("Ability config value AbilitySystemGlobalsClassName is not a valid class name."));

			AbilitySystemGlobals = NewObject<UAbilitySystemGlobals>(GetTransientPackage(), SingletonClass, NAME_None);
			AbilitySystemGlobals->AddToRoot();
			AbilitySystemGlobalsReadyCallback.Broadcast();
		}

		check(AbilitySystemGlobals);
		return AbilitySystemGlobals;
	}

	virtual bool IsAbilitySystemGlobalsAvailable() override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_IGameplayAbilitiesModule_IsAbilitySystemGlobalsAvailable);
		return AbilitySystemGlobals != nullptr;
	}

	void CallOrRegister_OnAbilitySystemGlobalsReady(FSimpleMulticastDelegate::FDelegate Delegate)
	{
		if (AbilitySystemGlobals)
		{
			Delegate.Execute();
		}
		else
		{
			AbilitySystemGlobalsReadyCallback.Add(Delegate);
		}
	}

	FSimpleMulticastDelegate AbilitySystemGlobalsReadyCallback;
	UAbilitySystemGlobals* AbilitySystemGlobals;
	
};

IMPLEMENT_MODULE(FGameplayAbilitiesModule, GameplayAbilities)

void FGameplayAbilitiesModule::StartupModule()
{	
	// This is loaded upon first request
	AbilitySystemGlobals = nullptr;

#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory("Abilities", IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_Abilities::MakeInstance));
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif // WITH_GAMEPLAY_DEBUGGER

	if (!IsRunningDedicatedServer())
	{
		AHUD::OnShowDebugInfo.AddStatic(&UAbilitySystemComponent::OnShowDebugInfo);
	}
}

void FGameplayAbilitiesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	AbilitySystemGlobals = NULL;

#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory("Abilities");
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif // WITH_GAMEPLAY_DEBUGGER
}
