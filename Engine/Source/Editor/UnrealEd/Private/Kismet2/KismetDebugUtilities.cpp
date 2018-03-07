// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet2/KismetDebugUtilities.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "UObject/PropertyPortFlags.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "UnrealEdGlobals.h"
#include "Engine/Breakpoint.h"
#include "ActorEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_Tunnel.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "AnimGraphNode_Base.h"

#define LOCTEXT_NAMESPACE "BlueprintDebugging"

/** Per-thread data for use by FKismetDebugUtilities functions */
class FKismetDebugUtilitiesData : public TThreadSingleton<FKismetDebugUtilitiesData>
{
public:
	FKismetDebugUtilitiesData()
		: CurrentInstructionPointer(nullptr)
		, MostRecentBreakpointInstructionPointer(nullptr)
		, TargetGraphStackDepth(INDEX_NONE)
		, StackFrameAtIntraframeDebugging(nullptr)
		, TraceStackSamples(FKismetDebugUtilities::MAX_TRACE_STACK_SAMPLES)
		, bIsSingleStepping(false)
	{
	}

	TWeakObjectPtr< class UEdGraphNode > CurrentInstructionPointer;

	// The current instruction encountered if we are stopped at a breakpoint; NULL otherwise
	TWeakObjectPtr< class UEdGraphNode > MostRecentBreakpointInstructionPointer;

	// The current function call graph stack.
	TArray<TWeakObjectPtr<class UEdGraph>> GraphStack;

	// The target graph call stack depth. INDEX_NONE if not active.
	int32 TargetGraphStackDepth;

	// The last message that an exception delivered
	FText LastExceptionMessage;

	// Only valid inside intraframe debugging
	const FFrame* StackFrameAtIntraframeDebugging;

	TSimpleRingBuffer<FKismetTraceSample> TraceStackSamples;

	bool bIsSingleStepping;
};

//////////////////////////////////////////////////////////////////////////
// FKismetDebugUtilities

void FKismetDebugUtilities::EndOfScriptExecution()
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	Data.bIsSingleStepping = false;
	Data.TargetGraphStackDepth = INDEX_NONE;
	Data.GraphStack.SetNum(0, false);
}

void FKismetDebugUtilities::RequestSingleStepping(bool bInAllowStepIn)
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	Data.bIsSingleStepping = bInAllowStepIn;
	if (!bInAllowStepIn)
	{
		Data.TargetGraphStackDepth = Data.GraphStack.Num();
	}
}

void FKismetDebugUtilities::RequestStepOut()
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	Data.bIsSingleStepping = false;
	if (Data.GraphStack.Num() > 1)
	{
		Data.TargetGraphStackDepth = Data.GraphStack.Num() - 1;
	}
}

void FKismetDebugUtilities::OnScriptException(const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info)
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	struct Local
	{
		static void OnMessageLogLinkActivated(const class TSharedRef<IMessageToken>& Token)
		{
			if( Token->GetType() == EMessageToken::Object )
			{
				const TSharedRef<FUObjectToken> UObjectToken = StaticCastSharedRef<FUObjectToken>(Token);
				if(UObjectToken->GetObject().IsValid())
				{
					FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(UObjectToken->GetObject().Get());
				}	
			}
		}
	};

	checkSlow(ActiveObject != NULL);

	// Ignore script exceptions for preview actors
	if(FActorEditorUtils::IsAPreviewOrInactiveActor(Cast<const AActor>(ActiveObject)))
	{
		return;
	}
	
	UClass* ClassContainingCode = FindClassForNode(ActiveObject, StackFrame.Node);
	UBlueprint* BlueprintObj = (ClassContainingCode ? Cast<UBlueprint>(ClassContainingCode->ClassGeneratedBy) : NULL);
	if (BlueprintObj)
	{
		const FBlueprintExceptionInfo* ExceptionInfo = &Info;
		bool bResetObjectBeingDebuggedWhenFinished = false;
		UObject* ObjectBeingDebugged = BlueprintObj->GetObjectBeingDebugged();
		UObject* SavedObjectBeingDebugged = ObjectBeingDebugged;
		UWorld* WorldBeingDebugged = BlueprintObj->GetWorldBeingDebugged();

		const int32 BreakpointOffset = StackFrame.Code - StackFrame.Node->Script.GetData() - 1;

		bool bShouldBreakExecution = false;
		bool bForceToCurrentObject = false;

		switch (Info.GetType())
		{
		case EBlueprintExceptionType::Breakpoint:
			bShouldBreakExecution = true;
			break;
		case EBlueprintExceptionType::Tracepoint:
			bShouldBreakExecution = Data.bIsSingleStepping;
			break;
		case EBlueprintExceptionType::WireTracepoint:
			break;
		case EBlueprintExceptionType::AccessViolation:
			if ( GIsEditor && GIsPlayInEditorWorld )
			{
				// declared as its own variable since it's flushed (logs pushed
				// to std output) on destruction - we want the full message 
				// constructed before it's logged
				FMessageLog PIEMessageLog("PIE");
				TSharedRef<FTokenizedMessage> ErrorMessage = PIEMessageLog.Error(LOCTEXT("RuntimeErrorMessage", "Blueprint Runtime Error:"));
				ErrorMessage->AddToken(FTextToken::Create(Info.GetDescription()));
				ErrorMessage->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintFunction", "from function:")));
				// NOTE: StackFrame.Node is not a blueprint node like you may think ("Node" has some legacy meaning)
				FString GeneratedFuncName = FString::Printf(TEXT("'%s'"), *StackFrame.Node->GetName());
				// a log token, telling us specifically where the exception is coming from (here
				// it's not helpful to link to a generated-function, so we just provide the plain name)
				ErrorMessage->AddToken(FTextToken::Create(FText::FromString(GeneratedFuncName)));

#if WITH_EDITORONLY_DATA // to protect access to GeneratedClass->DebugData
				UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(ClassContainingCode);
				if ((GeneratedClass != NULL) && GeneratedClass->DebugData.IsValid())
				{
					UEdGraphNode* BlueprintNode = GeneratedClass->DebugData.FindSourceNodeFromCodeLocation(StackFrame.Node, BreakpointOffset, true);
					// if instead, there is a node we can point to...
					if (BlueprintNode != NULL)
					{
						ErrorMessage->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintNode", "from node:")));

						FText NodeTitle = BlueprintNode->GetNodeTitle(ENodeTitleType::ListView); // a more user friendly name
						// link to the last executed node (the one throwing the exception, presumably)
						ErrorMessage->AddToken(
							FUObjectToken::Create(
								BlueprintNode, 
								NodeTitle
							)->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&Local::OnMessageLogLinkActivated))
						);

						ErrorMessage->AddToken( FTextToken::Create( LOCTEXT("RuntimeErrorBlueprintGraph", "in graph:") ) );
						ErrorMessage->AddToken(
							FUObjectToken::Create(
								BlueprintNode->GetGraph(), 
								FText::FromString(GetNameSafe(BlueprintNode->GetGraph()))
							)->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&Local::OnMessageLogLinkActivated)));
					}
				}
#endif // WITH_EDITORONLY_DATA

				ErrorMessage->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintObject", "in object:")));
				ErrorMessage
					->AddToken(FUObjectToken::Create(BlueprintObj, FText::FromString(BlueprintObj->GetName()))->OnMessageTokenActivated(FOnMessageTokenActivated::CreateStatic(&Local::OnMessageLogLinkActivated)));

				ErrorMessage->AddToken(FTextToken::Create(LOCTEXT("RuntimeErrorBlueprintDescription", "with description:")));
				ErrorMessage->AddToken(FTextToken::Create(Info.GetDescription()));
			}
			bForceToCurrentObject = true;
			bShouldBreakExecution = GetDefault<UEditorExperimentalSettings>()->bBreakOnExceptions;
			break;
		case EBlueprintExceptionType::InfiniteLoop:
			bForceToCurrentObject = true;
			bShouldBreakExecution = GetDefault<UEditorExperimentalSettings>()->bBreakOnExceptions;
			break;
		default:
			bForceToCurrentObject = true;
			bShouldBreakExecution = GetDefault<UEditorExperimentalSettings>()->bBreakOnExceptions;
			break;
		}

		// If we are debugging a specific world, the object needs to be in it
		if (WorldBeingDebugged != NULL && !ActiveObject->IsIn(WorldBeingDebugged))
		{
			// Might be a streaming level case, so find the real world to see
			const UObject *ObjOuter = ActiveObject;
			const UWorld *ObjWorld = NULL;
			bool FailedWorldCheck = true;
			while(ObjWorld == NULL && ObjOuter != NULL)
			{
				ObjOuter = ObjOuter->GetOuter();
				ObjWorld = Cast<const UWorld>(ObjOuter);
			}
			if (ObjWorld && ObjWorld->PersistentLevel)
			{
				if (ObjWorld->PersistentLevel->OwningWorld == WorldBeingDebugged)
				{
					// Its ok, the owning world is the world being debugged
					FailedWorldCheck = false;
				}				
			}

			if (FailedWorldCheck)
			{
				bForceToCurrentObject = false;
				bShouldBreakExecution = false;
			}
		}

		if (bShouldBreakExecution)
		{
			if ((ObjectBeingDebugged == NULL) || (bForceToCurrentObject))
			{
				// If there was nothing being debugged, treat this as a one-shot, temporarily set this object as being debugged,
				// and continue allowing any breakpoint to hit later on
				bResetObjectBeingDebuggedWhenFinished = true;
				BlueprintObj->SetObjectBeingDebugged(const_cast<UObject*>(ActiveObject));
			}
		}

		// Can't do intraframe debugging when the editor is actively stopping
		if (GEditor->ShouldEndPlayMap()) 
		{
			bShouldBreakExecution = false;
		}

		if (BlueprintObj->GetObjectBeingDebugged() == ActiveObject)
		{
			// Record into the trace log
			FKismetTraceSample& Tracer = Data.TraceStackSamples.WriteNewElementUninitialized();
			Tracer.Context = ActiveObject;
			Tracer.Function = StackFrame.Node;
			Tracer.Offset = BreakpointOffset; //@TODO: Might want to make this a parameter of Info
			Tracer.ObservationTime = FPlatformTime::Seconds();

			// Find the node that generated the code which we hit
			UEdGraphNode* NodeStoppedAt = FindSourceNodeForCodeLocation(ActiveObject, StackFrame.Node, BreakpointOffset, /*bAllowImpreciseHit=*/ true);
			if (NodeStoppedAt)
			{
				// Handle Node stepping and update the stack
				if (Info.GetType() == EBlueprintExceptionType::Tracepoint || Info.GetType() == EBlueprintExceptionType::Breakpoint)
				{
					CheckBreakConditions(NodeStoppedAt, bShouldBreakExecution);
				}
				// If the code which we hit was generated by a macro node expansion
				UK2Node_MacroInstance* MacroInstanceNode = Cast<UK2Node_MacroInstance>(NodeStoppedAt);
				if (MacroInstanceNode)
				{
					// Attempt to find the associated macro source node
					UEdGraphNode* MacroSourceNode = FindMacroSourceNodeForCodeLocation(ActiveObject, StackFrame.Node, BreakpointOffset);
					if (MacroSourceNode)
					{
						// If the macro source graph is valid
						UEdGraph* MacroSourceGraph = MacroSourceNode->GetGraph();
						if (MacroSourceGraph)
						{
							// If the macro source blueprint is valid
							UBlueprint* MacroBlueprint = MacroSourceGraph->GetTypedOuter<UBlueprint>();
							if (MacroBlueprint)
							{
								// If we're not already going to break execution
								if (!bShouldBreakExecution && !GEditor->ShouldEndPlayMap())
								{
									// Check the source graph to see if any breakpoints are set in the actual macro
									UBreakpoint* MacroBreakpoint = FindBreakpointForNode(MacroBlueprint, MacroSourceNode, true);
									if (MacroBreakpoint && MacroBreakpoint->IsEnabledByUser())
									{
										// Found one; break at this node in the macro source graph
										bShouldBreakExecution = true;

										// Redirect breakpoint exception info
										static FBlueprintExceptionInfo MacroBreakpointExceptionInfo(EBlueprintExceptionType::Breakpoint);
										ExceptionInfo = &MacroBreakpointExceptionInfo;
									}
								}

								// If we're going to break execution on a macro source node
								if (bShouldBreakExecution)
								{
									// Switch to the macro source node
									NodeStoppedAt = MacroSourceNode;

									// Restore the debug object on the original blueprint if the flag was set
									if (bResetObjectBeingDebuggedWhenFinished)
									{
										BlueprintObj->SetObjectBeingDebugged(SavedObjectBeingDebugged);
									}

									// Now switch to the macro source blueprint
									BlueprintObj = MacroBlueprint;

									// Ensure that the macro source blueprint's current debug object is set to the active object, and set the flag to restore it when finished
									bResetObjectBeingDebuggedWhenFinished = true;
									SavedObjectBeingDebugged = BlueprintObj->GetObjectBeingDebugged();
									BlueprintObj->SetObjectBeingDebugged(const_cast<UObject*>(ActiveObject));
								}
							}
						}
					}
				}
			}

			// Handle a breakpoint or single-step
			if (bShouldBreakExecution)
			{
				AttemptToBreakExecution(BlueprintObj, ActiveObject, StackFrame, *ExceptionInfo, NodeStoppedAt, BreakpointOffset);
			}
		}

		// Reset the object being debugged if we forced it to be something different
		if (bResetObjectBeingDebuggedWhenFinished)
		{
			BlueprintObj->SetObjectBeingDebugged(SavedObjectBeingDebugged);
		}

		const auto DisplayErrorLambda = [&](const FText ErrorTypeName, const TCHAR* Description)
		{
			if (GUnrealEd->PlayWorld != NULL)
			{
				GEditor->RequestEndPlayMap();
				FSlateApplication::Get().LeaveDebuggingMode();
			}

			// Launch a message box notifying the user why they have been booted
			{
				// Callback to display a pop-up showing the Callstack, the user can highlight and copy this if needed
				auto DisplayCallStackLambda = [](const FText CallStack)
				{
					TSharedPtr<SMultiLineEditableText> TextBlock;
					TSharedRef<SWidget> DisplayWidget =
						SNew(SBox)
						.MaxDesiredHeight(512)
						.MaxDesiredWidth(512)
						.Content()
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(TextBlock, SMultiLineEditableText)
									.AutoWrapText(true)
									.IsReadOnly(true)
									.Text(CallStack)
								]
							]
						];

					FSlateApplication::Get().PushMenu(
						FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),
						FWidgetPath(),
						DisplayWidget,
						FSlateApplication::Get().GetCursorPos(),
						FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
						);

					FSlateApplication::Get().SetKeyboardFocus(TextBlock);
				};

				TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);

				// Display a UObject link to the Blueprint that is the source of the failure
				Message->AddToken(FTextToken::Create(ErrorTypeName));
				Message->AddToken(FTextToken::Create(LOCTEXT("DisplayErrorLambda_Blueprint", "detected in ")));
				FString BlueprintName;
				BlueprintObj->GetName(BlueprintName);
				Message->AddToken(FUObjectToken::Create(BlueprintObj, FText::FromString(BlueprintName)));

				// Display a UObject link to the UFunction that is crashing. Will open the Blueprint if able and focus on the function's graph
				Message->AddToken(FTextToken::Create(LOCTEXT("DisplayErrorLambda_Function", ", asserted during ")));
				const int32 BreakpointOpCodeOffset = StackFrame.Code - StackFrame.Node->Script.GetData() - 1; //@TODO: Might want to make this a parameter of Info
				UEdGraphNode* SourceNode = FindSourceNodeForCodeLocation(ActiveObject, StackFrame.Node, BreakpointOpCodeOffset, /*bAllowImpreciseHit=*/ true);

				// If a source node is found, that's the token we want to link, otherwise settle with the UFunction
				if (SourceNode)
				{
					Message->AddToken(FUObjectToken::Create(SourceNode, SourceNode->GetNodeTitle(ENodeTitleType::ListView)));
				}
				else
				{
					Message->AddToken(FUObjectToken::Create(StackFrame.Node, StackFrame.Node->GetDisplayNameText()));
				}

				if (!Description)
				{
					Message->AddToken(FTextToken::Create(LOCTEXT("DisplayErrorLambda_CallStackNoDescription", " with the following ")));
				}
				else
				{
					Message->AddToken(FTextToken::Create(LOCTEXT("DisplayErrorLambda_CallStackWithDescription", " with the following message")));
					Message->AddToken(FTextToken::Create(FText::FromString(FString::Printf(TEXT("\"%s\""), Description))));
					Message->AddToken(FTextToken::Create(LOCTEXT("DisplayErrorLambda_CallStackWithDescriptionAnd", "and ")));
				}

				// Add an action token to display a pop-up that will display the complete script callstack
				const FText CallStackAsText = FText::FromString(StackFrame.GetStackTrace());
				Message->AddToken(FActionToken::Create(LOCTEXT("DisplayErrorLambda_CallStackLink", "Call Stack"), LOCTEXT("DisplayErrorLambda_CallStackDesc", "Displays the underlying callstack, tracing what function calls led to the assert occuring."), FOnActionTokenExecuted::CreateStatic(DisplayCallStackLambda, CallStackAsText)));
				FMessageLog("PIE").AddMessage(Message);
			}
		};

		// Extra cleanup after potential interactive handling
		switch (Info.GetType())
		{
		case EBlueprintExceptionType::FatalError:
			DisplayErrorLambda(LOCTEXT("FatalErrorType", "Fatal Error"), *(Info.GetDescription().ToString()));
			break;
		case EBlueprintExceptionType::InfiniteLoop:
			DisplayErrorLambda(LOCTEXT("InfiniteLoopErrorType", "Infinite Loop"), nullptr);
			break;
		default:
			// Left empty intentionally
			break;
		}
	}
}

UClass* FKismetDebugUtilities::FindClassForNode(const UObject* Object, UFunction* Function)
{
	if (NULL != Function)
	{
		UClass* FunctionOwner = Function->GetOwnerClass();
		return FunctionOwner;
	}
	if(NULL != Object)
	{
		UClass* ObjClass = Object->GetClass();
		return ObjClass;
	}
	return NULL;
}	

const TSimpleRingBuffer<FKismetTraceSample>& FKismetDebugUtilities::GetTraceStack()
{
	return FKismetDebugUtilitiesData::Get().TraceStackSamples; 
}

UEdGraphNode* FKismetDebugUtilities::FindSourceNodeForCodeLocation(const UObject* Object, UFunction* Function, int32 DebugOpcodeOffset, bool bAllowImpreciseHit)
{
	if (Object != NULL)
	{
		// Find the blueprint that corresponds to the object
		if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(FindClassForNode(Object, Function)))
		{
			return Class->GetDebugData().FindSourceNodeFromCodeLocation(Function, DebugOpcodeOffset, bAllowImpreciseHit);
		}
	}

	return NULL;
}

UEdGraphNode* FKismetDebugUtilities::FindMacroSourceNodeForCodeLocation(const UObject* Object, UFunction* Function, int32 DebugOpcodeOffset)
{
	if (Object != NULL)
	{
		// Find the blueprint that corresponds to the object
		if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(FindClassForNode(Object, Function)))
		{
			return Class->GetDebugData().FindMacroSourceNodeFromCodeLocation(Function, DebugOpcodeOffset);
		}
	}

	return NULL;
}

void FKismetDebugUtilities::CheckBreakConditions(UEdGraphNode* NodeStoppedAt, bool& InOutBreakExecution)
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	if (NodeStoppedAt)
	{
		// Update tracked graph stack.
		if (!Data.GraphStack.Num())
		{
			Data.GraphStack.Push(NodeStoppedAt->GetTypedOuter<UEdGraph>());
		}
		else if (UK2Node_FunctionEntry* FunctionNode = Cast<UK2Node_FunctionEntry>(NodeStoppedAt))
		{
			UEdGraph* NewGraph = FunctionNode->GetTypedOuter<UEdGraph>();
			const bool bAddFunction = Data.GraphStack.Num() && NewGraph ? NewGraph != Data.GraphStack.Last() : false;

			if (bAddFunction)
			{
				Data.GraphStack.Push(NewGraph);
			}
		}
		else if (Data.GraphStack.Num())
		{
			UEdGraph* CurrGraph = NodeStoppedAt->GetTypedOuter<UEdGraph>();
			if (Data.GraphStack.Last() != CurrGraph)
			{
				Data.GraphStack.Pop();
			}
		}
		// Figure out if we have a break condition.
		if (!Data.bIsSingleStepping)
		{
			if (Data.TargetGraphStackDepth != INDEX_NONE)
			{
				InOutBreakExecution = Data.TargetGraphStackDepth >= Data.GraphStack.Num();
			}
		}
	}
}

void FKismetDebugUtilities::AttemptToBreakExecution(UBlueprint* BlueprintObj, const UObject* ActiveObject, const FFrame& StackFrame, const FBlueprintExceptionInfo& Info, UEdGraphNode* NodeStoppedAt, int32 DebugOpcodeOffset)
{
	checkSlow(BlueprintObj->GetObjectBeingDebugged() == ActiveObject);

	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	// Cannot have re-entrancy while processing a breakpoint; return from this call stack before resuming execution!
	check( !GIntraFrameDebuggingGameThread );
	
	TGuardValue<bool> SignalGameThreadBeingDebugged(GIntraFrameDebuggingGameThread, true);
	TGuardValue<const FFrame*> ResetStackFramePointer(Data.StackFrameAtIntraframeDebugging, &StackFrame);

	// Should we pump Slate messages from this callstack, allowing intra-frame debugging?
	bool bShouldInStackDebug = false;

	if (NodeStoppedAt != NULL)
	{
		bShouldInStackDebug = true;

		Data.CurrentInstructionPointer = NodeStoppedAt;

		Data.MostRecentBreakpointInstructionPointer = NULL;

		// Find the breakpoint object for the node, assuming we hit one
		if (Info.GetType() == EBlueprintExceptionType::Breakpoint)
		{
			UBreakpoint* Breakpoint = FKismetDebugUtilities::FindBreakpointForNode(BlueprintObj, NodeStoppedAt);

			if (Breakpoint != NULL)
			{
				Data.MostRecentBreakpointInstructionPointer = NodeStoppedAt;
				FKismetDebugUtilities::UpdateBreakpointStateWhenHit(Breakpoint, BlueprintObj);
					
				//@TODO: K2: DEBUGGING: Debug print text can go eventually
				UE_LOG(LogBlueprintDebug, Warning, TEXT("Hit breakpoint on node '%s', from offset %d"), *(NodeStoppedAt->GetDescriptiveCompiledName()), DebugOpcodeOffset);
				UE_LOG(LogBlueprintDebug, Log, TEXT("\n%s"), *StackFrame.GetStackTrace());
			}
			else
			{
				UE_LOG(LogBlueprintDebug, Warning, TEXT("Unknown breakpoint hit at node %s in object %s:%04X"), *NodeStoppedAt->GetDescriptiveCompiledName(), *StackFrame.Node->GetFullName(), DebugOpcodeOffset);
			}
		}

		// Turn off single stepping; we've hit a node
		if (Data.bIsSingleStepping)
		{
			Data.bIsSingleStepping = false;
		}
	}
	else
	{
		UE_LOG(LogBlueprintDebug, Warning, TEXT("Tried to break execution in an unknown spot at object %s:%04X"), *StackFrame.Node->GetFullName(), StackFrame.Code - StackFrame.Node->Script.GetData());
	}

	// A check to !GIsAutomationTesting was removed from here as it seemed redundant.
	// Breakpoints have to be explicitly enabled by the user which shouldn't happen 
	// under automation and this was preventing debugging on automation test bp's.
	if ((GUnrealEd->PlayWorld != NULL))
	{
		// Pause the simulation
		GUnrealEd->PlayWorld->bDebugPauseExecution = true;
		GUnrealEd->PlayWorld->bDebugFrameStepExecution = false;
		bShouldInStackDebug = true;
	}
	else
	{
		bShouldInStackDebug = false;
		//@TODO: Determine exactly what behavior we want for breakpoints hit when not in PIE/SIE
		//ensureMsgf(false, TEXT("Breakpoints placed in a function instead of the event graph are not supported yet"));
	}

	// Now enter within-the-frame debugging mode
	if (bShouldInStackDebug)
	{
		Data.LastExceptionMessage = Info.GetDescription();
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(NodeStoppedAt);

		FSlateApplication::Get().EnterDebuggingMode();
	}
}

UEdGraphNode* FKismetDebugUtilities::GetCurrentInstruction()
{
	// If paused at the end of the frame, or while not paused, there is no 'current instruction' to speak of
	// It only has meaning during intraframe debugging.
	if (GIntraFrameDebuggingGameThread)
	{
		return FKismetDebugUtilitiesData::Get().CurrentInstructionPointer.Get();
	}
	else
	{
		return NULL;
	}
}

UEdGraphNode* FKismetDebugUtilities::GetMostRecentBreakpointHit()
{
	// If paused at the end of the frame, or while not paused, there is no 'current instruction' to speak of
	// It only has meaning during intraframe debugging.
	if (GIntraFrameDebuggingGameThread)
	{
		return FKismetDebugUtilitiesData::Get().MostRecentBreakpointInstructionPointer.Get();
	}
	else
	{
		return NULL;
	}
}

// Notify the debugger of the start of the game frame
void FKismetDebugUtilities::NotifyDebuggerOfStartOfGameFrame(UWorld* CurrentWorld)
{
}

// Notify the debugger of the end of the game frame
void FKismetDebugUtilities::NotifyDebuggerOfEndOfGameFrame(UWorld* CurrentWorld)
{
	FKismetDebugUtilitiesData::Get().bIsSingleStepping = false;
}

bool FKismetDebugUtilities::IsSingleStepping()
{ 
	return FKismetDebugUtilitiesData::Get().bIsSingleStepping; 
}

//////////////////////////////////////////////////////////////////////////
// Breakpoint

// Is the node a valid breakpoint target? (i.e., the node is impure and ended up generating code)
bool FKismetDebugUtilities::IsBreakpointValid(UBreakpoint* Breakpoint)
{
	check(Breakpoint);

	// Breakpoints on impure nodes in a macro graph are always considered valid
	UBlueprint* Blueprint = Cast<UBlueprint>(Breakpoint->GetOuter());
	if (Blueprint && Blueprint->BlueprintType == BPTYPE_MacroLibrary)
	{
		UK2Node* K2Node = Cast<UK2Node>(Breakpoint->Node);
		if (K2Node)
		{
			return K2Node->IsA<UK2Node_MacroInstance>()
				|| (!K2Node->IsNodePure() && !K2Node->IsA<UK2Node_Tunnel>());
		}
	}

	TArray<uint8*> InstallSites;
	FKismetDebugUtilities::GetBreakpointInstallationSites(Breakpoint, InstallSites);
	return InstallSites.Num() > 0;
}

// Set the node that the breakpoint should focus on
void FKismetDebugUtilities::SetBreakpointLocation(UBreakpoint* Breakpoint, UEdGraphNode* NewNode)
{
	if (NewNode != Breakpoint->Node)
	{
		// Uninstall it from the old site if needed
		FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, false);

		// Make the new site accurate
		Breakpoint->Node = NewNode;
		FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, Breakpoint->bEnabled);
	}
}

// Set or clear the enabled flag for the breakpoint
void FKismetDebugUtilities::SetBreakpointEnabled(UBreakpoint* Breakpoint, bool bIsEnabled)
{
	if (Breakpoint->bStepOnce && !bIsEnabled)
	{
		// Want to be disabled, but the single-stepping is keeping it enabled
		bIsEnabled = true;
		Breakpoint->bStepOnce_WasPreviouslyDisabled = true;
	}

	Breakpoint->bEnabled = bIsEnabled;
	FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, Breakpoint->bEnabled);
}

// Sets this breakpoint up as a single-step breakpoint (will disable or delete itself after one go if the breakpoint wasn't already enabled)
void FKismetDebugUtilities::SetBreakpointEnabledForSingleStep(UBreakpoint* Breakpoint, bool bDeleteAfterStep)
{
	Breakpoint->bStepOnce = true;
	Breakpoint->bStepOnce_RemoveAfterHit = bDeleteAfterStep;
	Breakpoint->bStepOnce_WasPreviouslyDisabled = !Breakpoint->bEnabled;

	FKismetDebugUtilities::SetBreakpointEnabled(Breakpoint, true);
}

void FKismetDebugUtilities::ReapplyBreakpoint(UBreakpoint* Breakpoint)
{
	FKismetDebugUtilities::SetBreakpointInternal(Breakpoint, Breakpoint->IsEnabled());
}

void FKismetDebugUtilities::StartDeletingBreakpoint(UBreakpoint* Breakpoint, UBlueprint* OwnerBlueprint)
{
#if WITH_EDITORONLY_DATA
	checkSlow(OwnerBlueprint->Breakpoints.Contains(Breakpoint));
	OwnerBlueprint->Breakpoints.Remove(Breakpoint);
	OwnerBlueprint->MarkPackageDirty();

	FKismetDebugUtilities::SetBreakpointLocation(Breakpoint, NULL);
#endif	//#if WITH_EDITORONLY_DATA
}

// Update the internal state of the breakpoint when it got hit
void FKismetDebugUtilities::UpdateBreakpointStateWhenHit(UBreakpoint* Breakpoint, UBlueprint* OwnerBlueprint)
{
	// Handle single-step breakpoints
	if (Breakpoint->bStepOnce)
	{
		Breakpoint->bStepOnce = false;

		if (Breakpoint->bStepOnce_RemoveAfterHit)
		{
			FKismetDebugUtilities::StartDeletingBreakpoint(Breakpoint, OwnerBlueprint);
		}
		else if (Breakpoint->bStepOnce_WasPreviouslyDisabled)
		{
			FKismetDebugUtilities::SetBreakpointEnabled(Breakpoint, false);
		}
	}
}

// Install/uninstall the breakpoint into/from the script code for the generated class that contains the node
void FKismetDebugUtilities::SetBreakpointInternal(UBreakpoint* Breakpoint, bool bShouldBeEnabled)
{
	TArray<uint8*> InstallSites;
	FKismetDebugUtilities::GetBreakpointInstallationSites(Breakpoint, InstallSites);

	for (int i = 0; i < InstallSites.Num(); ++i)
	{
		if (uint8* InstallSite = InstallSites[i])
		{
			*InstallSite = bShouldBeEnabled ? EX_Breakpoint : EX_Tracepoint;
		}
	}
}

// Returns the installation site(s); don't cache these pointers!
void FKismetDebugUtilities::GetBreakpointInstallationSites(UBreakpoint* Breakpoint, TArray<uint8*>& InstallSites)
{
	InstallSites.Empty();

#if WITH_EDITORONLY_DATA
	if (Breakpoint->Node != NULL)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(Breakpoint->Node);

		if ((Blueprint != NULL) && (Blueprint->GeneratedClass != NULL))
		{
			if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(*Blueprint->GeneratedClass))
			{
				// Find the insertion point from the debugging data
				Class->GetDebugData().FindBreakpointInjectionSites(Breakpoint->Node, InstallSites);
			}
		}
	}
#endif	//#if WITH_EDITORONLY_DATA
}

// Returns the set of valid breakpoint locations for the given macro instance node
void FKismetDebugUtilities::GetValidBreakpointLocations(const UK2Node_MacroInstance* MacroInstanceNode, TArray<const UEdGraphNode*>& BreakpointLocations)
{
	check(MacroInstanceNode);
	BreakpointLocations.Empty();

	// Gather information from the macro graph associated with the macro instance node
	bool bIsMacroPure = false;
	UK2Node_Tunnel* MacroEntryNode = NULL;
	UK2Node_Tunnel* MacroResultNode = NULL;
	UEdGraph* InstanceNodeMacroGraph = MacroInstanceNode->GetMacroGraph();
	if (ensure(InstanceNodeMacroGraph != nullptr))
	{
		FKismetEditorUtilities::GetInformationOnMacro(InstanceNodeMacroGraph, MacroEntryNode, MacroResultNode, bIsMacroPure);
	}
	if (!bIsMacroPure && MacroEntryNode)
	{
		// Get the execute pin outputs on the entry node
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		for (auto PinIt = MacroEntryNode->Pins.CreateConstIterator(); PinIt; ++PinIt)
		{
			const UEdGraphPin* ExecPin = *PinIt;
			if (ExecPin && ExecPin->Direction == EGPD_Output
				&& ExecPin->PinType.PinCategory == K2Schema->PC_Exec)
			{
				// For each pin linked to each execute pin, collect the node that owns it
				for (auto LinkedToPinIt = ExecPin->LinkedTo.CreateConstIterator(); LinkedToPinIt; ++LinkedToPinIt)
				{
					const UEdGraphPin* LinkedToPin = *LinkedToPinIt;
					check(LinkedToPin);

					const UEdGraphNode* LinkedToNode = LinkedToPin->GetOwningNode();
					check(LinkedToNode);

					if (LinkedToNode->IsA<UK2Node_MacroInstance>())
					{
						// Recursively descend into macro instance nodes encountered in a macro graph
						TArray<const UEdGraphNode*> SubLocations;
						GetValidBreakpointLocations(Cast<const UK2Node_MacroInstance>(LinkedToNode), SubLocations);
						BreakpointLocations.Append(SubLocations);
					}
					else
					{
						BreakpointLocations.AddUnique(LinkedToNode);
					}
				}
			}
		}
	}
}

// Finds a breakpoint for a given node if it exists, or returns NULL
UBreakpoint* FKismetDebugUtilities::FindBreakpointForNode(UBlueprint* Blueprint, const UEdGraphNode* Node, bool bCheckSubLocations)
{
	// iterate backwards so we can remove invalid breakpoints as we go
	for (int32 Index = Blueprint->Breakpoints.Num()-1; Index >= 0; --Index)
	{
		UBreakpoint* Breakpoint = Blueprint->Breakpoints[Index];
		if (Breakpoint == nullptr)
		{
			Blueprint->Breakpoints.RemoveAtSwap(Index);
			Blueprint->MarkPackageDirty();
			UE_LOG(LogBlueprintDebug, Warning, TEXT("Encountered an invalid blueprint breakpoint in %s (this should not happen... if you know how your blueprint got in this state, then please notify the Engine-Blueprints team)"), *Blueprint->GetPathName());
			continue;
		}

		const UEdGraphNode* BreakpointLocation = Breakpoint->GetLocation();
		if (BreakpointLocation == nullptr)
		{
			Blueprint->Breakpoints.RemoveAtSwap(Index);
			Blueprint->MarkPackageDirty();
			UE_LOG(LogBlueprintDebug, Display, TEXT("Encountered a blueprint breakpoint in %s without an associated node. The blueprint breakpoint has been removed"), *Blueprint->GetPathName());
			continue;
		}

		// Return this breakpoint if the location matches the given node
		if (BreakpointLocation == Node)
		{
			return Breakpoint;
		}
		else if (bCheckSubLocations)
		{
			// If this breakpoint is set on a macro instance node, check the set of valid breakpoint locations. If we find a
			// match in the returned set, return the breakpoint that's set on the macro instance node. This allows breakpoints
			// to be set and hit on macro instance nodes contained in a macro graph that will be expanded during compile.
			const UK2Node_MacroInstance* MacroInstanceNode = Cast<UK2Node_MacroInstance>(BreakpointLocation);
			if (MacroInstanceNode)
			{
				TArray<const UEdGraphNode*> ValidBreakpointLocations;
				GetValidBreakpointLocations(MacroInstanceNode, ValidBreakpointLocations);
				if (ValidBreakpointLocations.Contains(Node))
				{
					return Breakpoint;
				}
			}
		}
	}

	return NULL;
}

bool FKismetDebugUtilities::HasDebuggingData(const UBlueprint* Blueprint)
{
	return Cast<UBlueprintGeneratedClass>(*Blueprint->GeneratedClass)->GetDebugData().IsValid();
}

//////////////////////////////////////////////////////////////////////////
// Blueprint utils

// Looks thru the debugging data for any class variables associated with the node
UProperty* FKismetDebugUtilities::FindClassPropertyForPin(UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	UProperty* FoundProperty = nullptr;

	UClass* Class = Blueprint->GeneratedClass;
	while (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class))
	{
		FoundProperty = BlueprintClass->GetDebugData().FindClassPropertyForPin(Pin);
		if (FoundProperty != nullptr)
		{
			break;
		}

		Class = BlueprintClass->GetSuperClass();
	}

	return FoundProperty;
}

// Looks thru the debugging data for any class variables associated with the node (e.g., temporary variables or timelines)
UProperty* FKismetDebugUtilities::FindClassPropertyForNode(UBlueprint* Blueprint, const UEdGraphNode* Node)
{
	if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(*Blueprint->GeneratedClass))
	{
		return Class->GetDebugData().FindClassPropertyForNode(Node);
	}

	return NULL;
}


void FKismetDebugUtilities::ClearBreakpoints(UBlueprint* Blueprint)
{
	for (int32 BreakpointIndex = 0; BreakpointIndex < Blueprint->Breakpoints.Num(); ++BreakpointIndex)
	{
		UBreakpoint* Breakpoint = Blueprint->Breakpoints[BreakpointIndex];
		FKismetDebugUtilities::SetBreakpointLocation(Breakpoint, NULL);
	}

	Blueprint->Breakpoints.Empty();
	Blueprint->MarkPackageDirty();
}

bool FKismetDebugUtilities::CanWatchPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	//@TODO: This function belongs in the schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraph* Graph = Pin->GetOwningNode()->GetGraph();

	// Inputs should always be followed to their corresponding output in the world above
	const bool bNotAnInput = (Pin->Direction != EGPD_Input);

	//@TODO: Make watching a schema-allowable/denyable thing
	const bool bCanWatchThisGraph = true;

	return bCanWatchThisGraph && !K2Schema->IsMetaPin(*Pin) && bNotAnInput && !IsPinBeingWatched(Blueprint, Pin);
}

bool FKismetDebugUtilities::IsPinBeingWatched(const UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	return Blueprint->WatchedPins.Contains(const_cast<UEdGraphPin*>(Pin));
}

void FKismetDebugUtilities::RemovePinWatch(UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	UEdGraphPin* NonConstPin = const_cast<UEdGraphPin*>(Pin);
	Blueprint->WatchedPins.Remove(NonConstPin);
	Blueprint->MarkPackageDirty();
	Blueprint->PostEditChange();
}

void FKismetDebugUtilities::TogglePinWatch(UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	int32 ExistingWatchIndex = Blueprint->WatchedPins.Find(const_cast<UEdGraphPin*>(Pin));

	if (ExistingWatchIndex != INDEX_NONE)
	{
		FKismetDebugUtilities::RemovePinWatch(Blueprint, Pin);
	}
	else
	{
		Blueprint->WatchedPins.Add(const_cast<UEdGraphPin*>(Pin));
		Blueprint->MarkPackageDirty();
		Blueprint->PostEditChange();
	}
}

void FKismetDebugUtilities::ClearPinWatches(UBlueprint* Blueprint)
{
	Blueprint->WatchedPins.Empty();
	Blueprint->MarkPackageDirty();
	Blueprint->PostEditChange();
}

// Gets the watched tooltip for a specified site
FKismetDebugUtilities::EWatchTextResult FKismetDebugUtilities::GetWatchText(FString& OutWatchText, UBlueprint* Blueprint, UObject* ActiveObject, const UEdGraphPin* WatchPin)
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();

	if (UProperty* Property = FKismetDebugUtilities::FindClassPropertyForPin(Blueprint, WatchPin))
	{
		if (!Property->IsValidLowLevel())
		{
			//@TODO: Temporary checks to attempt to determine intermittent unreproducable crashes in this function
			static bool bErrorOnce = true;
			if (bErrorOnce)
			{
				ensureMsgf(false, TEXT("Error: Invalid (but non-null) property associated with pin; cannot get variable value"));
				bErrorOnce = false;
			}
			return EWTR_NoProperty;
		}

		if (ActiveObject != nullptr)
		{
			if (!ActiveObject->IsValidLowLevel())
			{
				//@TODO: Temporary checks to attempt to determine intermittent unreproducable crashes in this function
				static bool bErrorOnce = true;
				if (bErrorOnce)
				{
					ensureMsgf(false, TEXT("Error: Invalid (but non-null) active object being debugged; cannot get variable value for property %s"), *Property->GetPathName());
					bErrorOnce = false;
				}
				return EWTR_NoDebugObject;
			}

			void* PropertyBase = NULL;

			// Walk up the stack frame to see if we can find a function scope that contains the property as a local
			for (const FFrame* TestFrame = Data.StackFrameAtIntraframeDebugging; TestFrame != NULL; TestFrame = TestFrame->PreviousFrame)
			{
				if (Property->IsIn(TestFrame->Node))
				{
					PropertyBase = TestFrame->Locals;
					break;
				}
			}

			// Try at member scope if it wasn't part of a current function scope
			UClass* PropertyClass = Cast<UClass>(Property->GetOuter());
			if (!PropertyBase && PropertyClass)
			{
				if (ActiveObject->GetClass()->IsChildOf(PropertyClass))
				{
					PropertyBase = ActiveObject;
				}
				else if (AActor* Actor = Cast<AActor>(ActiveObject))
				{
					// Try and locate the propertybase in the actor components
					for (auto ComponentIter : Actor->GetComponents())
					{
						if (ComponentIter->GetClass()->IsChildOf(PropertyClass))
						{
							PropertyBase = ComponentIter;
							break;
						}
					}
				}
			}
#if USE_UBER_GRAPH_PERSISTENT_FRAME
			// Try find the propertybase in the persistent ubergraph frame
			UFunction* OuterFunction = Cast<UFunction>(Property->GetOuter());
			if(!PropertyBase && OuterFunction)
			{
				UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
				if (BPGC && ActiveObject->IsA(BPGC))
				{
					PropertyBase = BPGC->GetPersistentUberGraphFrame(ActiveObject, OuterFunction);
				}
			}
#endif // USE_UBER_GRAPH_PERSISTENT_FRAME
			
			// see if our WatchPin is on a animation node & if so try to get its property info
			UAnimBlueprintGeneratedClass* AnimBlueprintGeneratedClass = Cast<UAnimBlueprintGeneratedClass>(Blueprint->GeneratedClass);
			if(!PropertyBase && AnimBlueprintGeneratedClass)
			{
				// are we linked to an anim graph node?
				UProperty* LinkedProperty = Property;
				const UAnimGraphNode_Base* Node = Cast<UAnimGraphNode_Base>(WatchPin->GetOuter());
				if(Node == nullptr && WatchPin->LinkedTo.Num() > 0)
				{
					const UEdGraphPin* LinkedPin = WatchPin->LinkedTo[0];
					// When we change Node we *must* change Property, so it's still a sub-element of that.
					LinkedProperty = FKismetDebugUtilities::FindClassPropertyForPin(Blueprint, LinkedPin);
					Node = Cast<UAnimGraphNode_Base>(LinkedPin->GetOuter());
				}

				if(Node && LinkedProperty)
				{
					UStructProperty* NodeStructProperty = Cast<UStructProperty>(FKismetDebugUtilities::FindClassPropertyForNode(Blueprint, Node));
					if (NodeStructProperty)
					{
						for (UStructProperty* NodeProperty : AnimBlueprintGeneratedClass->AnimNodeProperties)
						{
							if (NodeProperty == NodeStructProperty)
							{
								void* NodePtr = NodeProperty->ContainerPtrToValuePtr<void>(ActiveObject);
								LinkedProperty->ExportText_InContainer(/*ArrayElement=*/ 0, /*inout*/ OutWatchText, NodePtr, NodePtr, /*Parent=*/ ActiveObject, PPF_PropertyWindow|PPF_BlueprintDebugView);
								return EWTR_Valid;
							}
						}
					}
				}
			}

			// Now either print out the variable value, or that it was out-of-scope
			if (PropertyBase != nullptr)
			{
				Property->ExportText_InContainer(/*ArrayElement=*/ 0, /*inout*/ OutWatchText, PropertyBase, PropertyBase, /*Parent=*/ ActiveObject, PPF_PropertyWindow|PPF_BlueprintDebugView);
				return EWTR_Valid;
			}
			else
			{
				return EWTR_NotInScope;
			}
		}
		else
		{
			return EWTR_NoDebugObject;
		}
	}
	else
	{
		return EWTR_NoProperty;
	}
}

FText FKismetDebugUtilities::GetAndClearLastExceptionMessage()
{
	FKismetDebugUtilitiesData& Data = FKismetDebugUtilitiesData::Get();
	const FText Result = Data.LastExceptionMessage;
	Data.LastExceptionMessage = FText();
	return Result;
}

#undef LOCTEXT_NAMESPACE
