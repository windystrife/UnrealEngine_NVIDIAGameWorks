// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayDebuggerTypes.h"

class AActor;
class AGameplayDebuggerCategoryReplicator;

class GAMEPLAYDEBUGGER_API FGameplayDebuggerAddonBase : public TSharedFromThis<FGameplayDebuggerAddonBase>
{
public:

	virtual ~FGameplayDebuggerAddonBase() {}

	int32 GetNumInputHandlers() const { return InputHandlers.Num(); }
	FGameplayDebuggerInputHandler& GetInputHandler(int32 HandlerId) { return InputHandlers[HandlerId]; }
	FString GetInputHandlerDescription(int32 HandlerId) const;

	/** [ALL] called when gameplay debugger is activated */
	virtual void OnGameplayDebuggerActivated();

	/** [ALL] called when gameplay debugger is deactivated */
	virtual void OnGameplayDebuggerDeactivated();

	/** check if simulate in editor mode is active */
	static bool IsSimulateInEditor();

protected:

	/** tries to find selected actor in local world */
	AActor* FindLocalDebugActor() const;

	/** returns replicator actor */
	AGameplayDebuggerCategoryReplicator* GetReplicator() const;

	/** creates new key binding handler: single key press */
	template<class UserClass>
	bool BindKeyPress(FName KeyName, UserClass* KeyHandlerObject, typename FGameplayDebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc, EGameplayDebuggerInputMode InputMode = EGameplayDebuggerInputMode::Local)
	{
		FGameplayDebuggerInputHandler NewHandler;
		NewHandler.KeyName = KeyName;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);
		NewHandler.Mode = InputMode;

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

	/** creates new key binding handler: key press with modifiers */
	template<class UserClass>
	bool BindKeyPress(FName KeyName, FGameplayDebuggerInputModifier KeyModifer, UserClass* KeyHandlerObject, typename FGameplayDebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc, EGameplayDebuggerInputMode InputMode = EGameplayDebuggerInputMode::Local)
	{
		FGameplayDebuggerInputHandler NewHandler;
		NewHandler.KeyName = KeyName;
		NewHandler.Modifier = KeyModifer;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);
		NewHandler.Mode = InputMode;

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

	/** creates new key binding handler: customizable key press, stored in config files */
	template<class UserClass>
	bool BindKeyPress(const FGameplayDebuggerInputHandlerConfig& InputConfig, UserClass* KeyHandlerObject, typename FGameplayDebuggerInputHandler::FHandler::TRawMethodDelegate< UserClass >::FMethodPtr KeyHandlerFunc, EGameplayDebuggerInputMode InputMode = EGameplayDebuggerInputMode::Local)
	{
		FGameplayDebuggerInputHandler NewHandler;
		NewHandler.KeyName = InputConfig.KeyName;
		NewHandler.Modifier = InputConfig.Modifier;
		NewHandler.Delegate.BindRaw(KeyHandlerObject, KeyHandlerFunc);
		NewHandler.Mode = InputMode;

		if (NewHandler.IsValid())
		{
			InputHandlers.Add(NewHandler);
			return true;
		}

		return false;
	}

private:

	friend class FGameplayDebuggerAddonManager;

	/** list of input handlers */
	TArray<FGameplayDebuggerInputHandler> InputHandlers;

	/** replicator actor */
	TWeakObjectPtr<AGameplayDebuggerCategoryReplicator> RepOwner;
};
