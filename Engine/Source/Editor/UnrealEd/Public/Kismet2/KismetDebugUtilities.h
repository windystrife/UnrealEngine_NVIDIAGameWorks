// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Engine/Selection.h"

class UBlueprint;
class UBreakpoint;
template<typename ElementType> class TSimpleRingBuffer;

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintDebug, Log, All);

//////////////////////////////////////////////////////////////////////////
// FKismetTraceSample

struct FKismetTraceSample
{
	TWeakObjectPtr<class UObject> Context;
	TWeakObjectPtr<class UFunction> Function;
	int32 Offset;
	double ObservationTime;
};

//////////////////////////////////////////////////////////////////////////
// FObjectsBeingDebuggedIterator

// Helper class to iterate over all objects that should be visible in the debugger
struct UNREALED_API FObjectsBeingDebuggedIterator
{
public:
	FObjectsBeingDebuggedIterator();

	/** @name Element access */
	//@{
	UObject* operator* () const;
	UObject* operator-> () const;
	//@}

	/** Advances iterator to the next element in the container. */
	FObjectsBeingDebuggedIterator& operator++();

	/** conversion to "bool" returning true if the iterator has not reached the last element. */
	FORCEINLINE explicit operator bool() const
	{ 
		return IsValid(); 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

private:
	FSelectionIterator SelectedActorsIter;
	int32 LevelScriptActorIndex;
private:
	void FindNextLevelScriptActor();
	bool IsValid() const;
	UWorld* GetWorld() const;
};



//////////////////////////////////////////////////////////////////////////
// FObjectsBeingDebuggedIterator

// Helper class to iterate over all objects that should be visible in the debugger
struct UNREALED_API FBlueprintObjectsBeingDebuggedIterator
{
public:
	FBlueprintObjectsBeingDebuggedIterator(UBlueprint* InBlueprint);

	/** @name Element access */
	//@{
	UObject* operator* () const;
	UObject* operator-> () const;
	//@}

	/** Advances iterator to the next element in the container. */
	FBlueprintObjectsBeingDebuggedIterator& operator++();

	/** conversion to "bool" returning true if the iterator has not reached the last element. */
	FORCEINLINE explicit operator bool() const
	{ 
		return IsValid(); 
	}
	/** inverse of the "bool" operator */
	FORCEINLINE bool operator !() const 
	{
		return !(bool)*this;
	}

private:
	UBlueprint* Blueprint;
private:
	bool IsValid() const;
};

//////////////////////////////////////////////////////////////////////////
// FKismetDebugUtilities

class UNREALED_API FKismetDebugUtilities
{
public:
	static void OnScriptException(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info);

	/** Returns the current instruction; if a PIE/SIE session is started but paused; otherwise NULL */
	static class UEdGraphNode* GetCurrentInstruction();

	/** Returns the most recent hit breakpoint; if a PIE/SIE session is started but paused; otherwise NULL */
	static class UEdGraphNode* GetMostRecentBreakpointHit();

	/** Request an attempt to single-step to the next node, with parameter to control step into sub graphs */
	static void RequestSingleStepping(bool bInAllowStepIn);

	/** Request an attempt to step out of the current graph */
	static void RequestStepOut();

	/** Called on terminatation of the current script execution so we can reset any break conditions */
	static void EndOfScriptExecution();

	// The maximum number of trace samples to gather before overwriting old ones
	enum { MAX_TRACE_STACK_SAMPLES = 1024 };

	// Get the trace stack
	static const TSimpleRingBuffer<FKismetTraceSample>& GetTraceStack();

	// Find the node that resulted in code at the specified location in the Object, or NULL if there was a problem (e.g., no debugging information was generated)
	static class UEdGraphNode* FindSourceNodeForCodeLocation(const UObject* Object, UFunction* Function, int32 DebugOpcodeOffset, bool bAllowImpreciseHit = false);

	// Find the macro node that resulted in code at the specified location in the Object, or NULL if there was a problem (e.g., no debugging information was generated)
	static class UEdGraphNode* FindMacroSourceNodeForCodeLocation(const UObject* Object, UFunction* Function, int32 DebugOpcodeOffset);

	// Return proper class for breakpoint
	static UClass* FindClassForNode(const UObject* Object, UFunction* Function);

	// Notify the debugger of the start of the game frame
	static void NotifyDebuggerOfStartOfGameFrame(UWorld* CurrentWorld);

	// Notify the debugger of the end of the game frame
	static void NotifyDebuggerOfEndOfGameFrame(UWorld* CurrentWorld);

	// Whether or not we are single stepping
	static bool IsSingleStepping();

	// Breakpoint utils

	/** Is the node a valid breakpoint target? (i.e., the node is impure and ended up generating code) */
	static bool IsBreakpointValid(UBreakpoint* Breakpoint);

	/** Set the node that the breakpoint should focus on */
	static void SetBreakpointLocation(UBreakpoint* Breakpoint, UEdGraphNode* NewNode);

	/** Set or clear the enabled flag for the breakpoint */
	static void SetBreakpointEnabled(UBreakpoint* Breakpoint, bool bIsEnabled);

	/** Sets this breakpoint up as a single-step breakpoint (will disable or delete itself after one go if the breakpoint wasn't already enabled) */
	static void SetBreakpointEnabledForSingleStep(UBreakpoint* Breakpoint, bool bDeleteAfterStep);

	/** Reapplies the breakpoint (used after recompiling to ensure it is set if needed) */
	static void ReapplyBreakpoint(UBreakpoint* Breakpoint);

	/** Start the process of deleting this breakpoint */
	static void StartDeletingBreakpoint(UBreakpoint* Breakpoint, UBlueprint* OwnerBlueprint);

	/** Update the internal state of the breakpoint when it got hit */
	static void UpdateBreakpointStateWhenHit(UBreakpoint* Breakpoint, UBlueprint* OwnerBlueprint);

	/** Returns the installation site(s); don't cache these pointers! */
	static void GetBreakpointInstallationSites(UBreakpoint* Breakpoint, TArray<uint8*>& InstallSites);

	/** Install/uninstall the breakpoint into/from the script code for the generated class that contains the node */
	static void SetBreakpointInternal(UBreakpoint* Breakpoint, bool bShouldBeEnabled);

	/** Returns the set of valid macro source node breakpoint location(s) for the given macro instance node. The set may be empty. */
	static void GetValidBreakpointLocations(const class UK2Node_MacroInstance* MacroInstanceNode, TArray<const UEdGraphNode*>& BreakpointLocations);

	// Blueprint utils 

	// Looks thru the debugging data for any class variables associated with the pin
	static class UProperty* FindClassPropertyForPin(UBlueprint* Blueprint, const UEdGraphPin* Pin);

	// Looks thru the debugging data for any class variables associated with the node (e.g., temporary variables or timelines)
	static class UProperty* FindClassPropertyForNode(UBlueprint* Blueprint, const UEdGraphNode* Node);

	// Is there debugging data available for this blueprint?
	static bool HasDebuggingData(const UBlueprint* Blueprint);

	/** Returns the breakpoint associated with a node, or NULL */
	static UBreakpoint* FindBreakpointForNode(UBlueprint* Blueprint, const UEdGraphNode* Node, bool bCheckSubLocations = false);

	/** Deletes all breakpoints in this blueprint */
	static void ClearBreakpoints(UBlueprint* Blueprint);

	static bool CanWatchPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin);
	static bool IsPinBeingWatched(const UBlueprint* Blueprint, const UEdGraphPin* Pin);
	static void TogglePinWatch(UBlueprint* Blueprint, const UEdGraphPin* Pin);
	static void RemovePinWatch(UBlueprint* Blueprint, const UEdGraphPin* Pin);
	static void ClearPinWatches(UBlueprint* Blueprint);

	enum EWatchTextResult
	{
		// The property was valid and the value has been returned as a string
		EWTR_Valid,

		// The property is a local of a function that is not on the current stack
		EWTR_NotInScope,

		// There is no debug object selected
		EWTR_NoDebugObject,

		// There is no property related to the pin
		EWTR_NoProperty
	};

	// Gets the watched tooltip for a specified site
	static EWatchTextResult GetWatchText(FString& OutWatchText, UBlueprint* Blueprint, UObject* ActiveObject, const UEdGraphPin* WatchPin);

	//@TODO: Pretty lame way to handle this messaging, ideally the entire Info object gets pushed into the editor when intraframe debugging is triggered!
	// This doesn't work properly if there is more than one blueprint editor open at once either (one will consume it, the others will be left in the cold)
	static FText GetAndClearLastExceptionMessage();
protected:
	static void CheckBreakConditions(UEdGraphNode* NodeStoppedAt, bool& InOutBreakExecution);
	static void AttemptToBreakExecution(UBlueprint* BlueprintObj, const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info, UEdGraphNode* NodeStoppedAt, int32 DebugOpcodeOffset);

private:
	FKismetDebugUtilities() {}
};

//////////////////////////////////////////////////////////////////////////
