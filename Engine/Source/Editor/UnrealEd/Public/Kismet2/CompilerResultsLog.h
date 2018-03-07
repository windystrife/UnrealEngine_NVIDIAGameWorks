// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/Object.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "ObjectKey.h"
#if WITH_EDITOR
#include "Logging/TokenizedMessage.h"
#include "Misc/CompilationResult.h"
#include "EdGraphToken.h"
#endif

class FTokenizedMessage;
class IMessageLogListing;

#if WITH_EDITOR

/** This class maps from final objects to their original source object, across cloning, autoexpansion, etc... */
class UNREALED_API FBacktrackMap
{
protected:
	// Maps from transient object created during compiling to original 'source code' object
	TMap<UObject const*, UObject*> SourceBacktrackMap;
	// Maps from transient pins created during compiling to original 'source pin' object
	TMap<UEdGraphPin*, UEdGraphPin*> PinSourceBacktrackMap;

public:
	/** Update the source backtrack map to note that NewObject was most closely generated/caused by the SourceObject */
	void NotifyIntermediateObjectCreation(UObject* NewObject, UObject* SourceObject);

	/** Update the pin source backtrack map to note that NewPin was most closely generated/caused by the SourcePin */
	void NotifyIntermediatePinCreation(UEdGraphPin* NewPin, UEdGraphPin* SourcePin);

	/** Returns the true source object for the passed in object */
	UObject* FindSourceObject(UObject* PossiblyDuplicatedObject);
	UObject const* FindSourceObject(UObject const* PossiblyDuplicatedObject) const;
	UEdGraphPin* FindSourcePin(UEdGraphPin* PossiblyDuplicatedPin);
	UEdGraphPin const* FindSourcePin(UEdGraphPin const* PossiblyDuplicatedPin) const;
};

/** This class represents a log of compiler output lines (errors, warnings, and information notes), each of which can be a rich tokenized message */
class UNREALED_API FCompilerResultsLog
{
	// Compiler event
	struct FCompilerEvent
	{
		FString Name;
		uint32 Counter;
		double StartTime;
		double FinishTime;
		TSharedPtr<FCompilerEvent> ParentEventScope;
		TArray< TSharedRef<FCompilerEvent> > ChildEvents;

		FCompilerEvent(TSharedPtr<FCompilerEvent> InParentEventScope = nullptr)
			:Name(TEXT(""))
			,Counter(0)
			,StartTime(0.0)
			,FinishTime(0.0)
			,ParentEventScope(InParentEventScope)
		{
		}

		void Start(const TCHAR* InName)
		{
			Name = InName;
			StartTime = FPlatformTime::Seconds();
		}

		void Finish()
		{
			FinishTime = FPlatformTime::Seconds();
		}
	};

	// Current compiler event scope
	TSharedPtr<FCompilerEvent> CurrentEventScope;

public:
	// List of all tokenized messages
	TArray< TSharedRef<FTokenizedMessage> > Messages;

	// Number of error messages
	int32 NumErrors;

	// Number of warnings
	int32 NumWarnings;

	// Should we be silent?
	bool bSilentMode;

	// Should we log only Info messages, or all messages?
	bool bLogInfoOnly;

	// Should nodes mentioned in messages be annotated for display with that message?
	bool bAnnotateMentionedNodes;

	// Should detailed results be appended to the final summary log?
	bool bLogDetailedResults;

	// Minimum event time (ms) for inclusion into the final summary log
	int EventDisplayThresholdMs;

	/** Tracks nodes that produced errors/warnings */
	TSet< TWeakObjectPtr<UEdGraphNode> > AnnotatedNodes;

protected:
	// Maps from transient object created during compiling to original 'source code' object
	FBacktrackMap SourceBacktrackMap;
	
	// Maps immediately back to source uobject, which may itself by an intermediate object:
	TMap<UEdGraphNode const*, UEdGraphNode*> FullSourceBacktrackMap;

	// Name of the source object being compiled
	FString SourcePath;

	// Backtrack map for intermendiate tunnel/macro nodes
	FBacktrackMap FinalNodeBackToTunnelSourceMap;
	
	// Map to track intermediate tunnel nodes back to the intermediate expansion tunnel instance.
	TMap<TWeakObjectPtr<UEdGraphNode>, TWeakObjectPtr<UEdGraphNode>> IntermediateTunnelNodeToTunnelInstanceMap;

	// Map to track active nested tunnels for intermediate tunnel instances.
	TMultiMap<TWeakObjectPtr<UEdGraphNode>, TWeakObjectPtr<UEdGraphNode>> IntermediateTunnelInstanceHierarchyMap;

public:
	FCompilerResultsLog(bool bIsCompatibleWithEvents = true);
	virtual ~FCompilerResultsLog();

	/** Register this log with the MessageLog module */
	static void Register();

	/** Unregister this log from the MessageLog module */
	static void Unregister();

	/** Accessor for the LogName, so it can be opened elsewhere */
	static FName GetLogName(){ return Name; }

	/** Set the source name for the final log summary */
	void SetSourcePath(const FString& InSourcePath)
	{
		SourcePath = InSourcePath;
	}

	/**
	 * Write an error in to the compiler log.
	 * Note: @@ will be replaced by node or pin links for nodes/pins passed via varargs 
	 */
	template<typename... Args>
	TSharedRef<FTokenizedMessage> Error(const TCHAR* Format, Args... args)
	{
		++NumErrors;
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Error);
		InternalLogMessage(Format, Line, args...);
		return Line;
	}

	/**
	 * Write a warning in to the compiler log.
	 * Note: @@ will be replaced by node or pin links for nodes/pins passed via varargs 
	 */
	template<typename... Args>
	TSharedRef<FTokenizedMessage> Warning(const TCHAR* Format, Args... args)
	{
		++NumWarnings;
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Warning);
		InternalLogMessage(Format, Line, args...);
		return Line;
	}

	/**
	 * Write a note in to the compiler log.
	 * Note: @@ will be replaced by node or pin links for nodes/pins passed via varargs 
	 */
	template<typename... Args>
	TSharedRef<FTokenizedMessage> Note(const TCHAR* Format, Args... args)
	{
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Info);
		InternalLogMessage(Format, Line, args...);
		return Line;
	}

	/**
	 * Store a potential error for a given node in the compiler log. All messages for the node can be committed to the log later by calling CommitPotentialMessages
	 * Note: @@ will be replaced by node or pin links for nodes/pins passed via varargs 
	 */
	template<typename... Args>
	TSharedRef<FTokenizedMessage> StorePotentialError(const UEdGraphNode* Source, const TCHAR* Format, Args... args)
	{
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Error);
		UEdGraphNode* SourceNode = nullptr;
		Tokenize(Format, *Line, SourceNode, args...);
		PotentialMessages.FindOrAdd(Source).Add(Line);
		return Line;
	}

	/**
	 * Store a potential warning for a given node in the compiler log. All messages for the node can be committed to the log later by calling CommitPotentialMessages
	 * Note: @@ will be replaced by node or pin links for nodes/pins passed via varargs 
	 */
	template<typename... Args>
	TSharedRef<FTokenizedMessage> StorePotentialWarning(const UEdGraphNode* Source, const TCHAR* Format, Args... args)
	{
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Warning);
		UEdGraphNode* SourceNode = nullptr;
		Tokenize(Format, *Line, SourceNode, args...);
		PotentialMessages.FindOrAdd(Source).Add(Line);
		return Line;
	}

	/**
	 * Store a potential note for a given node in the compiler log. All messages for the node can be committed to the log later by calling CommitPotentialMessages
	 * Note: @@ will be replaced by node or pin links for nodes/pins passed via varargs 
	 */
	template<typename... Args>
	TSharedRef<FTokenizedMessage> StorePotentialNote(const UEdGraphNode* Source, const TCHAR* Format, Args... args)
	{
		TSharedRef<FTokenizedMessage> Line = FTokenizedMessage::Create(EMessageSeverity::Info);
		UEdGraphNode* SourceNode = nullptr;
		Tokenize(Format, *Line, SourceNode, args...);
		PotentialMessages.FindOrAdd(Source).Add(Line);
		return Line;
	}

	/**
	 * Commit all stored potential messages for a given node. Returns true if any messages were written.
	 */
	bool CommitPotentialMessages(UEdGraphNode* Source)
	{
		TArray<TSharedRef<FTokenizedMessage>> FoundMessages;
		if (PotentialMessages.RemoveAndCopyValue(Source, FoundMessages))
		{
			for (const TSharedRef<FTokenizedMessage>& Message : FoundMessages)
			{
				switch (Message->GetSeverity())
				{
				case EMessageSeverity::Error:
					++NumErrors;
					break;

				case EMessageSeverity::Warning:
					++NumWarnings;
					break;

				default:
					break;
				}

				InternalLogMessage(Message, Source);
			}
			return true;
		}
		return false;
	}

	/** Update the source backtrack map to note that NewObject was most closely generated/caused by the SourceObject */
	void NotifyIntermediateObjectCreation(UObject* NewObject, UObject* SourceObject);
	void NotifyIntermediatePinCreation(UEdGraphPin* NewObject, UEdGraphPin* SourceObject);

	/** Registers intermediate tunnel nodes, both the node and tunnel instance should be intermediate nodes */
	void RegisterIntermediateTunnelNode(UEdGraphNode* Node, UEdGraphNode* OwningTunnelInstance);

	/** Registers an intermediate tunnel instance node, the active tunnels should not be intermediate nodes. */
	void RegisterIntermediateTunnelInstance(UEdGraphNode* IntermediateTunnel, TArray<TWeakObjectPtr<UEdGraphNode>>& ActiveTunnels);

	/** Returns the source tunnel instance or the source tunnel node depending on what created the intermediate node */
	UEdGraphNode* GetSourceNode(const UEdGraphNode* IntermediateNode);

	/** Returns the intermediate tunnel instance that generated the node */
	UEdGraphNode* GetIntermediateTunnelInstance(const UEdGraphNode* IntermediateNode) const;

	/** Returns the source tunnel node for the intermediate node */
	UEdGraphNode* GetSourceTunnelNode(const UEdGraphNode* IntermediateNode);

	/** Returns the source tunnel instance that generated the intermediate node */
	UEdGraphNode* GetSourceTunnelInstance(const UEdGraphNode* IntermediateNode);

	/** Returns the active tunnel instances for the intermediate node */
	void GetTunnelsActiveForNode(const UEdGraphNode* IntermediateNode, TArray<TWeakObjectPtr<UEdGraphNode>>& ActiveTunnelsOut);

	/** Returns the true source object for the passed in object */
	UObject* FindSourceObject(UObject* PossiblyDuplicatedObject);
	UObject const* FindSourceObject(UObject const* PossiblyDuplicatedObject) const;

	/** Returns a int32 used to uniquely identify an action for the latent action manager */
	int32 CalculateStableIdentifierForLatentActionManager( const UEdGraphNode* Node );

	/** Returns the true source object for the passed in object; does type checking on the result */
	template <typename T>
	T* FindSourceObjectTypeChecked(UObject* PossiblyDuplicatedObject)
	{
		return CastChecked<T>(FindSourceObject(PossiblyDuplicatedObject));
	}
	
	template <typename T>
	T const* FindSourceObjectTypeChecked(UObject const* PossiblyDuplicatedObject) const
	{
		return CastChecked<T const>(FindSourceObject(PossiblyDuplicatedObject));
	}

	UEdGraphPin* FindSourcePin(UEdGraphPin* PossiblyDuplicatedPin);
	const UEdGraphPin* FindSourcePin(const UEdGraphPin* PossiblyDuplicatedPin) const;
	
	void Append(FCompilerResultsLog const& Other);

	/** Begin a new compiler event */
	void BeginEvent(const TCHAR* InName);

	/** End the current compiler event */
	void EndEvent();

	/** Access the current event target log */
	static FCompilerResultsLog* GetEventTarget()
	{
		return CurrentEventTarget;
	}

	/** Get the message log listing for this blueprint */
	static TSharedRef<IMessageLogListing> GetBlueprintMessageLog(UBlueprint* InBlueprint);

protected:
	/** Helper method to add a child event to the given parent event scope */
	void AddChildEvent(TSharedPtr<FCompilerEvent>& ParentEventScope, TSharedRef<FCompilerEvent>& ChildEventScope);

	void InternalLogMessage(const TSharedRef<FTokenizedMessage>& Message, UEdGraphNode* SourceNode );

	void Tokenize(const TCHAR* Text, FTokenizedMessage &OutMessage, UEdGraphNode*& OutSourceNode)
	{
		OutMessage.AddToken(FTextToken::Create(FText::FromString(Text)));
	}

	template<typename T, typename... Args>
	void Tokenize(const TCHAR* Format, FTokenizedMessage &OutMessage, UEdGraphNode*& OutSourceNode, T First, Args... Rest)
	{
		// read to next "@@":
		if (const TCHAR* DelimiterStr = FCString::Strstr(Format, TEXT("@@")))
		{
			OutMessage.AddToken(FTextToken::Create(FText::FromString(FString(DelimiterStr - Format, Format))));
			OutMessage.AddToken(FEdGraphToken::Create(First, this, OutSourceNode));
			const TCHAR* NextChunk = DelimiterStr + FCString::Strlen(TEXT("@@"));
			if (*NextChunk)
			{
				Tokenize(NextChunk, OutMessage, OutSourceNode, Rest...);
			}
		}
		else
		{
			Tokenize(Format, OutMessage, OutSourceNode);
		}
	}

	template<typename... Args>
	void InternalLogMessage(const TCHAR* Format, const TSharedRef<FTokenizedMessage>& Message, Args... args)
	{
		// Convention for SourceNode established by the original version of the compiler results log
		// was to annotate the error on the first node we can find. I am preserving that behavior
		// for this type safe, variadic version:
		UEdGraphNode* SourceNode = nullptr;
		Tokenize(Format, *Message, SourceNode, args...);
		InternalLogMessage(Message, SourceNode);
	}

	/** */
	void AnnotateNode(class UEdGraphNode* Node, TSharedRef<FTokenizedMessage> LogLine);

	/** Internal method to append the final compiler results summary to the MessageLog */
	void InternalLogSummary();

	/** Internal helper method to recursively append event details into the MessageLog */
	void InternalLogEvent(const FCompilerEvent& InEvent, int32 InDepth = 0);

private:

	/** Map of stored potential messages indexed by a node. Can be committed to the results log by calling CommitPotentialMessages for that node. */
	TMap< FObjectKey, TArray< TSharedRef<FTokenizedMessage> > > PotentialMessages;

	/** Parses a compiler log dump to generate tokenized output */
	static TArray< TSharedRef<FTokenizedMessage> > ParseCompilerLogDump(const FString& LogDump);

	/** Goes to an error given a Message Token */
	static void OnGotoError(const class TSharedRef<IMessageToken>& Token);

	/** Callback function for binding the global compiler dump to open the static compiler log */
	static void GetGlobalModuleCompilerDump(const FString& LogDump, ECompilationResult::Type CompilationResult, bool bShowLog);

	/** The log's name, for easy re-use */
	static const FName Name;
	
	/** The log target for compile events */
	static FCompilerResultsLog* CurrentEventTarget;

	/** Handle to the registered GetGlobalModuleCompilerDump delegate. */
	static FDelegateHandle GetGlobalModuleCompilerDumpDelegateHandle;
};

/** This class will begin a new compile event on construction, and automatically end it when the instance goes out of scope */
class UNREALED_API FScopedCompilerEvent
{
public:
	/** Constructor; automatically begins a new event */
	FScopedCompilerEvent(const TCHAR* InName)
	{
		FCompilerResultsLog* ResultsLog = FCompilerResultsLog::GetEventTarget();
		if(ResultsLog != nullptr)
		{
			ResultsLog->BeginEvent(InName);
		}
	}

	/** Destructor; automatically ends the event */
	~FScopedCompilerEvent()
	{
		FCompilerResultsLog* ResultsLog = FCompilerResultsLog::GetEventTarget();
		if(ResultsLog != nullptr)
		{
			ResultsLog->EndEvent();
		}
	}
};

/** Scope wrapper for the blueprint message log. Ensures we dont leak logs that we dont need (i.e. those that have no messages) */
class UNREALED_API FScopedBlueprintMessageLog
{
public:
	FScopedBlueprintMessageLog(UBlueprint* InBlueprint);
	~FScopedBlueprintMessageLog();

public:
	/** The listing we wrap */
	TSharedRef<IMessageLogListing> Log;

	/** The generated name of the log */
	FName LogName;
};

#if STATS
#define BP_SCOPED_COMPILER_EVENT_STAT(Stat) \
	SCOPE_CYCLE_COUNTER(Stat); \
	FScopedCompilerEvent PREPROCESSOR_JOIN(ScopedCompilerEvent,__LINE__)(GET_STATDESCRIPTION(Stat))
#else
#define BP_SCOPED_COMPILER_EVENT_STAT(Stat) \
	FScopedCompilerEvent PREPROCESSOR_JOIN(ScopedCompilerEvent,__LINE__)(ANSI_TO_TCHAR(#Stat))
#endif
#endif	//#if WITH_EDITOR
