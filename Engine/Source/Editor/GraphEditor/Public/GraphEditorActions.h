// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FGraphEditorCommandsImpl : public TCommands<FGraphEditorCommandsImpl>
{
public:

	FGraphEditorCommandsImpl()
		: TCommands<FGraphEditorCommandsImpl>( TEXT("GraphEditor"), NSLOCTEXT("Contexts", "GraphEditor", "Graph Editor"), NAME_None, FEditorStyle::GetStyleSetName() )
	{
	}	

	virtual ~FGraphEditorCommandsImpl()
	{
	}

	GRAPHEDITOR_API virtual void RegisterCommands() override;

	TSharedPtr< FUICommandInfo > ReconstructNodes;
	TSharedPtr< FUICommandInfo > BreakNodeLinks;

	// Execution sequence specific commands
	TSharedPtr< FUICommandInfo > AddExecutionPin;
	TSharedPtr< FUICommandInfo > RemoveExecutionPin;

	// SetFieldsInStruct specific commands
	TSharedPtr< FUICommandInfo > RemoveThisStructVarPin;
	TSharedPtr< FUICommandInfo > RemoveOtherStructVarPins;
	TSharedPtr< FUICommandInfo > RestoreAllStructVarPins;

	// Select node specific commands
	TSharedPtr< FUICommandInfo > AddOptionPin;
	TSharedPtr< FUICommandInfo > RemoveOptionPin;
	TSharedPtr< FUICommandInfo > ChangePinType;

	// Pin visibility modes
	TSharedPtr< FUICommandInfo > ShowAllPins;
	TSharedPtr< FUICommandInfo > HideNoConnectionPins;
	TSharedPtr< FUICommandInfo > HideNoConnectionNoDefaultPins;

	// Event / Function Entry commands
	TSharedPtr< FUICommandInfo > AddParentNode;

	// Debugging commands
	TSharedPtr< FUICommandInfo > RemoveBreakpoint;
	TSharedPtr< FUICommandInfo > AddBreakpoint;
	TSharedPtr< FUICommandInfo > EnableBreakpoint;
	TSharedPtr< FUICommandInfo > DisableBreakpoint;
	TSharedPtr< FUICommandInfo > ToggleBreakpoint;

	// Encapsulation commands
	TSharedPtr< FUICommandInfo > CollapseNodes;
	TSharedPtr< FUICommandInfo > PromoteSelectionToFunction;
	TSharedPtr< FUICommandInfo > PromoteSelectionToMacro;
	TSharedPtr< FUICommandInfo > ExpandNodes;
	TSharedPtr< FUICommandInfo > CollapseSelectionToFunction;
	TSharedPtr< FUICommandInfo > CollapseSelectionToMacro;

	// Alignment commands
	TSharedPtr< FUICommandInfo > AlignNodesTop;
	TSharedPtr< FUICommandInfo > AlignNodesMiddle;
	TSharedPtr< FUICommandInfo > AlignNodesBottom;

	TSharedPtr< FUICommandInfo > AlignNodesLeft;
	TSharedPtr< FUICommandInfo > AlignNodesCenter;
	TSharedPtr< FUICommandInfo > AlignNodesRight;

	TSharedPtr< FUICommandInfo > StraightenConnections;

	TSharedPtr< FUICommandInfo > DistributeNodesHorizontally;
	TSharedPtr< FUICommandInfo > DistributeNodesVertically;
	
	// Enable/disable commands
	TSharedPtr< FUICommandInfo > EnableNodes;
	TSharedPtr< FUICommandInfo > DisableNodes;
	TSharedPtr< FUICommandInfo > EnableNodes_Always;
	TSharedPtr< FUICommandInfo > EnableNodes_DevelopmentOnly;

	//
	TSharedPtr< FUICommandInfo > SelectReferenceInLevel;
	TSharedPtr< FUICommandInfo > AssignReferencedActor;

	// Find references
	TSharedPtr< FUICommandInfo > FindReferences;
	TSharedPtr< FUICommandInfo > FindAndReplaceReferences;

	// Jumps to the definition of the selected node (or otherwise focuses something interesting about that node, e.g., the inner graph for a collapsed graph)
	TSharedPtr< FUICommandInfo > GoToDefinition;

	// Pin-specific actions
	TSharedPtr< FUICommandInfo > BreakPinLinks;
	TSharedPtr< FUICommandInfo > PromoteToVariable;
	TSharedPtr< FUICommandInfo > PromoteToLocalVariable;
	TSharedPtr< FUICommandInfo > SplitStructPin;
	TSharedPtr< FUICommandInfo > RecombineStructPin;
	TSharedPtr< FUICommandInfo > StartWatchingPin;
	TSharedPtr< FUICommandInfo > StopWatchingPin;
	TSharedPtr< FUICommandInfo > ResetPinToDefaultValue;

	// SkeletalControl specific commands
	TSharedPtr< FUICommandInfo > SelectBone;
	// Blend list options
	TSharedPtr< FUICommandInfo > AddBlendListPin;
	TSharedPtr< FUICommandInfo > RemoveBlendListPin;

	// options for sequence/evaluator converter
	TSharedPtr< FUICommandInfo > ConvertToSeqEvaluator;
	TSharedPtr< FUICommandInfo > ConvertToSeqPlayer;

	// options for blendspace sequence/evaluator converter
	TSharedPtr< FUICommandInfo > ConvertToBSEvaluator;
	TSharedPtr< FUICommandInfo > ConvertToBSPlayer;

	// options for aimoffset converter
	TSharedPtr< FUICommandInfo > ConvertToAimOffsetLookAt;
	TSharedPtr< FUICommandInfo > ConvertToAimOffsetSimple;

	// options for sequence/evaluator converter
	TSharedPtr< FUICommandInfo > ConvertToPoseBlender;
	TSharedPtr< FUICommandInfo > ConvertToPoseByName;

	// option for opening the asset related to the graph node
	TSharedPtr< FUICommandInfo > OpenRelatedAsset;

	//create a comment node
	TSharedPtr< FUICommandInfo > CreateComment;

	// Zoom in and out on the graph editor
	TSharedPtr< FUICommandInfo > ZoomIn;
	TSharedPtr< FUICommandInfo > ZoomOut;

	// Go to node documentation
	TSharedPtr< FUICommandInfo > GoToDocumentation;
};

class GRAPHEDITOR_API FGraphEditorCommands
{
public:
	static void Register();

	static const FGraphEditorCommandsImpl& Get();

	static void Unregister();
};


