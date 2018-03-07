// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// GAMEPLAY DEBUGGER EXTENSION
// 
// Extensions allows creating additional key bindings for gameplay debugger.
// For example, you can use them to add another way of selecting actor to Debug.
//
// Replication is limited only to handling input events and tool state events,
// it's not possible to send variables or RPC calls
//
// It should be compiled and used only when module is included, so every extension class
// needs be placed in #if WITH_GAMEPLAY_DEBUGGER block.
// 
// Extensions needs to be manually registered and unregistered with GameplayDebugger.
// It's best to do it in owning module's Startup / Shutdown, similar to detail view customizations.

#pragma once

#include "CoreMinimal.h"
#include "GameplayDebuggerAddonBase.h"

class APlayerController;

class GAMEPLAYDEBUGGER_API FGameplayDebuggerExtension : public FGameplayDebuggerAddonBase
{
public:

	virtual ~FGameplayDebuggerExtension() {}
	virtual void OnGameplayDebuggerActivated() override;
	virtual void OnGameplayDebuggerDeactivated() override;

	/** [LOCAL] description for gameplay debugger's header row, newline character is ignored */
	virtual FString GetDescription() const;

	/** [LOCAL] called when added to debugger tool or tool is activated */
	virtual void OnActivated();

	/** [LOCAL] called when removed from debugger tool or tool is deactivated */
	virtual void OnDeactivated();

	/** check if extension is created for local player */
	bool IsLocal() const;

protected:

	/** get player controller owning gameplay debugger tool */
	APlayerController* GetPlayerController() const;
};
