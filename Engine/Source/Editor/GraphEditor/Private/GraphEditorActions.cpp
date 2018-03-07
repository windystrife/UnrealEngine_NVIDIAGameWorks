// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorActions.h"

#define LOCTEXT_NAMESPACE "GraphEditorCommands"

void FGraphEditorCommandsImpl::RegisterCommands()
{
	UI_COMMAND( ReconstructNodes, "Refresh Nodes", "Refreshes nodes", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( BreakNodeLinks, "Break Link(s)", "Breaks links", EUserInterfaceActionType::Button, FInputChord() )
	
	UI_COMMAND( AddExecutionPin, "Add execution pin", "Adds another execution output pin to an execution sequence or switch node", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( RemoveExecutionPin, "Remove execution pin", "Removes an execution output pin from an execution sequence or switch node", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( RemoveThisStructVarPin, "Remove this struct variable pin", "Removes the selected input pin", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( RemoveOtherStructVarPins, "Remove all other pins", "Removes all variable input pins, except for the selected one", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( RestoreAllStructVarPins, "Restore all structure pins", "Restore all structure pins", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( AddOptionPin, "Add Option Pin", "Adds another option input pin to the node", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( RemoveOptionPin, "Remove Option Pin", "Removes the last option input pin from the node", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( ChangePinType, "Change Pin Type", "Changes the type of this pin (boolean, int, etc.)", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( ShowAllPins, "Show All Pins", "Shows all pins", EUserInterfaceActionType::RadioButton, FInputChord() )
	UI_COMMAND( HideNoConnectionPins, "Hide Unconnected Pins", "Hides all pins with no connections", EUserInterfaceActionType::RadioButton, FInputChord() )
	UI_COMMAND( HideNoConnectionNoDefaultPins, "Hide Unused Pins", "Hides all pins with no connections and no default value", EUserInterfaceActionType::RadioButton, FInputChord() )

	UI_COMMAND( AddParentNode, "Add call to parent function", "Adds a node that calls this function's parent", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( ToggleBreakpoint, "Toggle breakpoint", "Adds or removes a breakpoint on each selected node", EUserInterfaceActionType::Button, FInputChord(EKeys::F9) )
	UI_COMMAND( AddBreakpoint, "Add breakpoint", "Adds a breakpoint to each selected node", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( RemoveBreakpoint, "Remove breakpoint", "Removes any breakpoints on each selected node", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( EnableBreakpoint, "Enable breakpoint", "Enables any breakpoints on each selected node", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( DisableBreakpoint, "Disable breakpoint", "Disables any breakpoints on each selected node", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( CollapseNodes, "Collapse Nodes", "Collapses selected nodes into a single node", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( PromoteSelectionToFunction, "Promote to Function", "Promotes selected collapsed graphs to functions.", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( PromoteSelectionToMacro, "Promote to Macro", "Promotes selected collapsed graphs to macros.", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( ExpandNodes, "Expand Node", "Expands the node's internal graph into the current graph and removes this node.", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( CollapseSelectionToFunction, "Collapse to Function", "Collapses selected nodes into a single function node.", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( CollapseSelectionToMacro, "Collapse to Macro", "Collapses selected nodes into a single macro node.", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( AlignNodesTop, "Align Top", "Aligns the top edges of the selected nodes", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( AlignNodesMiddle, "Align Middle", "Aligns the vertical middles of the selected nodes", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( AlignNodesBottom, "Align Bottom", "Aligns the bottom edges of the selected nodes", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( AlignNodesLeft, "Align Left", "Aligns the left edges of the selected nodes", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( AlignNodesCenter, "Align Center", "Aligns the horizontal centers of the selected nodes", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( AlignNodesRight, "Align Right", "Aligns the right edges of the selected nodes", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( StraightenConnections, "Straighten Connection(s)", "Straightens connections between the selected nodes.", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( DistributeNodesHorizontally, "Distribute Horizontally", "Evenly distributes the selected nodes horizontally", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( DistributeNodesVertically, "Distribute Vertically", "Evenly distributes the selected nodes vertically", EUserInterfaceActionType::Button, FInputChord() )
	
	UI_COMMAND( EnableNodes, "Enable Nodes", "Selected node(s) will be enabled.", EUserInterfaceActionType::Check, FInputChord() )
	UI_COMMAND( DisableNodes, "Disable Nodes", "Selected node(s) will be disabled.", EUserInterfaceActionType::Check, FInputChord() )
	UI_COMMAND( EnableNodes_Always, "Enable Nodes (Always)", "Selected node(s) will always be enabled.", EUserInterfaceActionType::RadioButton, FInputChord() )
	UI_COMMAND( EnableNodes_DevelopmentOnly, "Enable Nodes (Development Only)", "Selected node(s) will be enabled in development mode only.", EUserInterfaceActionType::RadioButton, FInputChord() )

	UI_COMMAND( SelectReferenceInLevel, "Find Actor in Level", "Select the actor referenced by this node in the level", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( AssignReferencedActor, "Assign selected Actor", "Assign the selected actor to be this node's referenced object", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( FindReferences, "Find References", "Find references of this item", EUserInterfaceActionType::Button, FInputChord(EKeys::F, EModifierKey::Shift | EModifierKey::Alt) )
	UI_COMMAND( FindAndReplaceReferences, "Find and Replace References", "Brings up a window to help find and replace all instances of this item", EUserInterfaceActionType::Button, FInputChord() )
	
	UI_COMMAND( GoToDefinition, "Goto Definition", "Jumps to the defintion of the selected node if available, e.g., C++ code for a native function or the graph for a Blueprint function.", EUserInterfaceActionType::Button, FInputChord(EKeys::G, EModifierKey::Alt) )

	UI_COMMAND( BreakPinLinks, "Break Link(s)", "Breaks pin links", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( PromoteToVariable, "Promote to Variable", "Promotes something to a variable", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( PromoteToLocalVariable, "Promote to Local Variable", "Promotes something to a local variable of the current function", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( SplitStructPin, "Split Struct Pin", "Breaks a struct pin in to a separate pin per element", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( RecombineStructPin, "Recombine Struct Pin", "Takes struct pins that have been broken in to composite elements and combines them back to a single struct pin", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( StartWatchingPin, "Watch this value", "Adds this pin or variable to the watch list", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( StopWatchingPin, "Stop watching this value", "Removes this pin or variable from the watch list ", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( ResetPinToDefaultValue, "Reset to Default Value", "Reset value of this pin to the default", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( SelectBone, "Select Bone", "Assign or change the bone for SkeletalControls", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( AddBlendListPin, "Add Blend Pin", "Add Blend Pin to BlendList", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( RemoveBlendListPin, "Remove Blend Pin", "Remove Blend Pin", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( ConvertToSeqEvaluator, "Convert To Single Frame Animation", "Convert to one frame animation that requires position", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( ConvertToSeqPlayer, "Convert to Sequence Player", "Convert back to sequence player without manual position set up", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( ConvertToBSEvaluator, "Convert To Single Frame BlendSpace", "Convert to one frame BlendSpace that requires position", EUserInterfaceActionType::Button, FInputChord() )
	UI_COMMAND( ConvertToBSPlayer, "Convert to BlendSpace Player", "Convert back to BlendSpace player without manual position set up", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( ConvertToAimOffsetLookAt, "Convert To LookAt AimOffset", "Convert to one AimOffset that automatically tracks a Target", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND( ConvertToAimOffsetSimple, "Convert to Simple AimOffset", "Convert to a manual AimOffets", EUserInterfaceActionType::Button, FInputChord())

	UI_COMMAND(ConvertToPoseBlender, "Convert To Pose Blender", "Convert to pose blender that can blend by source curves", EUserInterfaceActionType::Button, FInputChord())
	UI_COMMAND(ConvertToPoseByName, "Convert to Pose By Name", "Convert to pose node that returns by name", EUserInterfaceActionType::Button, FInputChord())

	UI_COMMAND( OpenRelatedAsset, "Open Asset", "Opens the asset related to this node", EUserInterfaceActionType::Button, FInputChord() )

	UI_COMMAND( CreateComment, "Create Comment", "Create a comment box", EUserInterfaceActionType::Button, FInputChord(EKeys::C))

	UI_COMMAND( ZoomIn, "Zoom In", "Zoom in on the graph editor", EUserInterfaceActionType::Button, FInputChord(EKeys::Add))
	UI_COMMAND( ZoomOut, "Zoom Out", "Zoom out from the graph editor", EUserInterfaceActionType::Button, FInputChord(EKeys::Subtract))

	UI_COMMAND( GoToDocumentation, "View Documentation", "View documentation for this node.", EUserInterfaceActionType::Button, FInputChord());
}



void FGraphEditorCommands::Register()
{
	return FGraphEditorCommandsImpl::Register();
}

const FGraphEditorCommandsImpl& FGraphEditorCommands::Get()
{
	return FGraphEditorCommandsImpl::Get();
}

void FGraphEditorCommands::Unregister()
{
	return FGraphEditorCommandsImpl::Unregister();
}

#undef LOCTEXT_NAMESPACE
