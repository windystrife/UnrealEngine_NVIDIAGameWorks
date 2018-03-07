// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintCompilationManager.h"
#include "UObject/Interface.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Components/ActorComponent.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Stats/StatsMisc.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StructOnScope.h"
#include "UObject/MetaData.h"
#include "UObject/TextProperty.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/CoreStyle.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"
#include "Exporters/Exporter.h"
#include "Animation/AnimInstance.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/MemberReference.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ThumbnailRendering/BlueprintThumbnailRenderer.h"
#include "Engine/LevelScriptActor.h"
#include "Components/TimelineComponent.h"
#include "Engine/TimelineTemplate.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/PropertyPortFlags.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "EngineUtils.h"
#include "EdMode.h"
#include "Dialogs/Dialogs.h"
#include "UnrealEdGlobals.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Matinee/MatineeActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "BlueprintsObjectVersion.h"
#include "Kismet2/CompilerResultsLog.h"

#include "Editor/KismetCompiler/Public/KismetCompilerModule.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_Event.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_AddComponent.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_Variable.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Composite.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_GetClassDefaults.h"
#include "K2Node_Literal.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MathExpression.h"
#include "K2Node_MatineeController.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_Timeline.h"
#include "K2Node_Knot.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimStateNodeBase.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationTransitionSchema.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimStateConduitNode.h"
#include "AnimGraphNode_StateMachine.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/StructureEditorUtils.h"
#include "ScopedTransaction.h"
#include "ClassViewerFilter.h"
#include "InstancedReferenceSubobjectHelper.h"
#include "Toolkits/AssetEditorManager.h"
#include "BlueprintEditorModule.h"
#include "BlueprintEditor.h"
#include "Kismet2/Kismet2NameValidators.h"

#include "Misc/DefaultValueHelper.h"
#include "ObjectEditorUtils.h"
#include "Toolkits/ToolkitManager.h"
#include "UnrealExporter.h"
#include "BlueprintEditorSettings.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Editor/Blutility/Public/IBlutilityModule.h"

#include "Engine/InheritableComponentHandler.h"
#include "LevelEditor.h"

#include "EditorCategoryUtils.h"
#include "Styling/SlateIconFinder.h"

extern COREUOBJECT_API bool GBlueprintUseCompilationManager;

#define LOCTEXT_NAMESPACE "Blueprint"

DEFINE_LOG_CATEGORY(LogBlueprintDebug);

DEFINE_STAT(EKismetCompilerStats_NotifyBlueprintChanged);
DECLARE_CYCLE_STAT(TEXT("Mark Blueprint as Structurally Modified"), EKismetCompilerStats_MarkBlueprintasStructurallyModified, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Refresh External DependencyNodes"), EKismetCompilerStats_RefreshExternalDependencyNodes, STATGROUP_KismetCompiler);

struct FCompareNodePriority
{
	FORCEINLINE bool operator()( const UK2Node& A, const UK2Node& B ) const
	{
		const bool NodeAChangesStructure = A.NodeCausesStructuralBlueprintChange();
		const bool NodeBChangesStructure = B.NodeCausesStructuralBlueprintChange();

		if (NodeAChangesStructure != NodeBChangesStructure)
		{
			return NodeAChangesStructure;
		}
		
		return A.GetNodeRefreshPriority() > B.GetNodeRefreshPriority();
	}
};

/**
 * This helper does a depth first search, looking for the highest parent class that
 * implements the specified interface.
 * 
 * @param  Class		The class whose inheritance tree you want to check (NOTE: this class is checked itself as well).
 * @param  Interface	The interface that you're looking for.
 * @return NULL if the interface wasn't found, otherwise the highest parent class that implements the interface.
 */
static UClass* FindInheritedInterface(UClass* const Class, FBPInterfaceDescription const& Interface)
{
	UClass* ClassWithInterface = nullptr;

	if (Class != nullptr)
	{
		UClass* const ParentClass = Class->GetSuperClass();
		// search depth first so that we may find the highest parent in the chain that implements this interface
		ClassWithInterface = FindInheritedInterface(ParentClass, Interface);

		if (ClassWithInterface == nullptr)
		{
			for (const FImplementedInterface& ImplementedInterface : Class->Interfaces)
		{
				if (ImplementedInterface.Class == Interface.Interface)
			{
				ClassWithInterface = Class;
				break;
			}
		}
	}
	}

	return ClassWithInterface;
}

/**
 * This helper can be used to find a duplicate interface that is implemented higher
 * up the inheritance hierarchy (which can happen when you change parents or add an 
 * interface to a parent that's already implemented by a child).
 * 
 * @param  Interface	The interface you wish to find a duplicate of.
 * @param  Blueprint	The blueprint you wish to search.
 * @return True if one of the blueprint's super classes implements the specified interface, false if the child is free to implement it.
 */
static bool IsInterfaceImplementedByParent(FBPInterfaceDescription const& Interface, UBlueprint const* const Blueprint)
{
	check(Blueprint != nullptr);
	return (FindInheritedInterface(Blueprint->ParentClass, Interface) != nullptr);
}

/**
 * A helper function that takes two nodes belonging to the same graph and deletes 
 * one, replacing it with the other (moving over pin connections, etc.).
 * 
 * @param  OldNode	The node you want deleted.
 * @param  NewNode	The new replacement node that should take OldNode's place.
 */
static void ReplaceNode(UK2Node* OldNode, UK2Node* NewNode)
{
	check(OldNode->GetClass() == NewNode->GetClass());
	check(OldNode->GetOuter() == NewNode->GetOuter());

	UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
	K2Schema->BreakNodeLinks(*NewNode);

	for (UEdGraphPin* OldPin : OldNode->Pins)
	{
		UEdGraphPin* NewPin = NewNode->FindPinChecked(OldPin->PinName);
		NewPin->MovePersistentDataFromOldPin(*OldPin);
	}

	NewNode->NodePosX = OldNode->NodePosX;
	NewNode->NodePosY = OldNode->NodePosY;

	FBlueprintEditorUtils::RemoveNode(OldNode->GetBlueprint(), OldNode, /* bDontRecompile =*/ true);
}

/**
 * Promotes any graphs that belong to the specified interface, and repurposes them
 * as parent overrides (function graphs that implement a parent's interface).
 * 
 * @param  Interface	The interface you're looking to promote.
 * @param  BlueprintObj	The blueprint that you want this applied to.
 */
static void PromoteInterfaceImplementationToOverride(FBPInterfaceDescription const& Interface, UBlueprint* const BlueprintObj)
{
	check(BlueprintObj != nullptr);
	// find the parent whose interface we're overriding 
	UClass* ParentClass = FindInheritedInterface(BlueprintObj->ParentClass, Interface);

	if (ParentClass != nullptr)
	{
		for (UEdGraph* InterfaceGraph : Interface.Graphs)
		{
			check(InterfaceGraph != nullptr);

			// The graph can be deleted now that it is a simple function override
			InterfaceGraph->bAllowDeletion = true;

			// Interface functions are ready to be a function graph outside the box, 
			// there will be no auto-call to parent though to maintain current functionality
			// in the graph
			BlueprintObj->FunctionGraphs.Add(InterfaceGraph);

			// No validation should be necessary here. Child blueprints will have interfaces conformed during their own compilation. 
		}

		// if any graphs were moved
		if (Interface.Graphs.Num() > 0)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BlueprintObj);
		}
	}
}

/**
 * Looks through the specified graph for any references to the specified 
 * variable, and renames them accordingly.
 * 
 * @param  InBlueprint		The blueprint that you want to search through.
 * @param  InVariableClass	The class that owns the variable that we're renaming
 * @param  InGraph			Graph to scope the rename to
 * @param  InOldVarName		The current name of the variable we want to replace
 * @param  InNewVarName		The name that we wish to change all references to
 */
static void RenameVariableReferencesInGraph(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName)
{
	for(UEdGraphNode* GraphNode : InGraph->Nodes)
	{
		// Allow node to handle variable renaming
		if (UK2Node* const K2Node = Cast<UK2Node>(GraphNode))
		{
			K2Node->HandleVariableRenamed(InBlueprint, InVariableClass, InGraph, InOldVarName, InNewVarName);
		}
	}
}

/**
 * Looks through the specified blueprint for any references to the specified 
 * variable, and renames them accordingly.
 * 
 * @param  Blueprint		The blueprint that you want to search through.
 * @param  VariableClass	The class that owns the variable that we're renaming
 * @param  OldVarName		The current name of the variable we want to replace
 * @param  NewVarName		The name that we wish to change all references to
 */
static void RenameVariableReferences(UBlueprint* Blueprint, UClass* VariableClass, const FName& OldVarName, const FName& NewVarName)
{
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	// Update any graph nodes that reference the old variable name to instead reference the new name
	for(UEdGraph* CurrentGraph : AllGraphs)
	{
		RenameVariableReferencesInGraph(Blueprint, VariableClass, CurrentGraph, OldVarName, NewVarName);
	}
}

//////////////////////////////////////
// FBasePinChangeHelper

void FBasePinChangeHelper::Broadcast(UBlueprint* InBlueprint, UK2Node_EditablePinBase* InTargetNode, UEdGraph* Graph)
{
	UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(InTargetNode);
	const UK2Node_FunctionTerminator* FunctionDefNode = Cast<const UK2Node_FunctionTerminator>(InTargetNode);
	const UK2Node_Event* EventNode = Cast<const UK2Node_Event>(InTargetNode);
	if (TunnelNode)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);

		const bool bIsTopLevelFunctionGraph = Blueprint->MacroGraphs.Contains(Graph);

		if (bIsTopLevelFunctionGraph)
		{
			// Editing a macro, hit all loaded instances (in open blueprints)
			for (TObjectIterator<UK2Node_MacroInstance> It(RF_Transient); It; ++It)
			{
				UK2Node_MacroInstance* MacroInstance = *It;
				if (NodeIsNotTransient(MacroInstance) && (MacroInstance->GetMacroGraph() == Graph))
				{
					EditMacroInstance(MacroInstance, FBlueprintEditorUtils::FindBlueprintForNode(MacroInstance));
				}
			}
		}
		else if(NodeIsNotTransient(TunnelNode))
		{
			// Editing a composite node, hit the single instance in the parent graph		
			EditCompositeTunnelNode(TunnelNode);
		}
	}
	else if (FunctionDefNode || EventNode)
	{
		UFunction* Func = FFunctionFromNodeHelper::FunctionFromNode(FunctionDefNode ? static_cast<const UK2Node*>(FunctionDefNode) : static_cast<const UK2Node*>(EventNode));
		const FName FuncName = Func 
			? Func->GetFName() 
			: (FunctionDefNode ? FunctionDefNode->SignatureName : EventNode->GetFunctionName());
		const UClass* SignatureClass = Func
			? Func->GetOwnerClass()
			: (UClass*)(FunctionDefNode ? FunctionDefNode->SignatureClass : nullptr);

		const bool bIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(InBlueprint);

		// Reconstruct all function call sites that call this function (in open blueprints)
		for (TObjectIterator<UK2Node_CallFunction> It(RF_Transient); It; ++It)
		{
			UK2Node_CallFunction* CallSite = *It;
			if (NodeIsNotTransient(CallSite))
			{
				UBlueprint* CallSiteBlueprint = FBlueprintEditorUtils::FindBlueprintForNode(CallSite);
				if (!CallSiteBlueprint)
				{
					// the node doesn't have a Blueprint in its outer chain, 
					// probably signifying that it is part of a graph that has 
					// been removed by the user (and moved off the Blueprint)
					continue;
				}
				
				const bool bValidSchema = CallSite->GetSchema() != nullptr;
				const bool bNameMatches = (CallSite->FunctionReference.GetMemberName() == FuncName);
				if (bNameMatches && bValidSchema)
				{
					if (bIsInterface)
					{
						if (FBlueprintEditorUtils::FindFunctionInImplementedInterfaces(CallSiteBlueprint, FuncName))
						{
							EditCallSite(CallSite, CallSiteBlueprint);
						}
					}
					else
					{
						const UClass* MemberParentClass = CallSite->FunctionReference.GetMemberParentClass(CallSite->GetBlueprintClassFromNode());
						const bool bClassMatchesEasy = (MemberParentClass != nullptr)
							&& (MemberParentClass->IsChildOf(SignatureClass) || MemberParentClass->IsChildOf(InBlueprint->GeneratedClass));
						const bool bClassMatchesHard = !bClassMatchesEasy && CallSite->FunctionReference.IsSelfContext() && (SignatureClass == nullptr)
							&& (CallSiteBlueprint == InBlueprint || CallSiteBlueprint->SkeletonGeneratedClass->IsChildOf(InBlueprint->SkeletonGeneratedClass));

						if (bClassMatchesEasy || bClassMatchesHard)
						{
							EditCallSite(CallSite, CallSiteBlueprint);
						}
					}
				}
			}
		}

		if(FBlueprintEditorUtils::IsDelegateSignatureGraph(Graph))
		{
			FName GraphName = Graph->GetFName();
			for (TObjectIterator<UK2Node_BaseMCDelegate> It(RF_Transient); It; ++It)
			{
				if(NodeIsNotTransient(*It) && (GraphName == It->GetPropertyName()))
				{
					UBlueprint* CallSiteBlueprint = FBlueprintEditorUtils::FindBlueprintForNode(*It);
					EditDelegates(*It, CallSiteBlueprint);
				}
			}
		}

		for (TObjectIterator<UK2Node_CreateDelegate> It(RF_Transient); It; ++It)
		{
			if(NodeIsNotTransient(*It))
			{
				EditCreateDelegates(*It);
			}
		}
	}
}

//////////////////////////////////////
// FParamsChangedHelper

void FParamsChangedHelper::EditCompositeTunnelNode(UK2Node_Tunnel* TunnelNode)
{
	if (TunnelNode->InputSinkNode != nullptr)
	{
		TunnelNode->InputSinkNode->ReconstructNode();
	}

	if (TunnelNode->OutputSourceNode != nullptr)
	{
		TunnelNode->OutputSourceNode->ReconstructNode();
	}
}

void FParamsChangedHelper::EditMacroInstance(UK2Node_MacroInstance* MacroInstance, UBlueprint* Blueprint)
{
	MacroInstance->ReconstructNode();
	if (Blueprint)
	{
		ModifiedBlueprints.Add(Blueprint);
	}
}

void FParamsChangedHelper::EditCallSite(UK2Node_CallFunction* CallSite, UBlueprint* Blueprint)
{
	CallSite->Modify();
	CallSite->ReconstructNode();
	if (Blueprint != nullptr)
	{
		ModifiedBlueprints.Add(Blueprint);
	}
}

void FParamsChangedHelper::EditDelegates(UK2Node_BaseMCDelegate* CallSite, UBlueprint* Blueprint)
{
	CallSite->Modify();
	CallSite->ReconstructNode();
	if (UK2Node_AddDelegate* AssignNode = Cast<UK2Node_AddDelegate>(CallSite))
	{
		if (UEdGraphPin* DelegateInPin = AssignNode->GetDelegatePin())
		{
			for (UEdGraphPin* DelegateOutPin : DelegateInPin->LinkedTo)
			{
				if (UK2Node_CustomEvent* CustomEventNode = (DelegateOutPin ? Cast<UK2Node_CustomEvent>(DelegateOutPin->GetOwningNode()) : nullptr))
				{
					CustomEventNode->ReconstructNode();
				}
			}
		}
	}
	if (Blueprint != nullptr)
	{
		ModifiedBlueprints.Add(Blueprint);
	}
}

void FParamsChangedHelper::EditCreateDelegates(UK2Node_CreateDelegate* CallSite)
{
	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;
	CallSite->HandleAnyChange(Graph, Blueprint);
	if(Blueprint)
	{
		ModifiedBlueprints.Add(Blueprint);
	}
	if(Graph)
	{
		ModifiedGraphs.Add(Graph);
	}
}

//////////////////////////////////////
// FUCSComponentId

FUCSComponentId::FUCSComponentId(const UK2Node_AddComponent* UCSNode)
	: GraphNodeGuid(UCSNode->NodeGuid)
{
}

//////////////////////////////////////
// FBlueprintEditorUtils

void FBlueprintEditorUtils::RefreshAllNodes(UBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->HasAllFlags(RF_LoadCompleted))
	{
		UE_LOG(LogBlueprint, Warning, 
			TEXT("RefreshAllNodes called on incompletly loaded blueprint '%s'"), 
			Blueprint ? *Blueprint->GetFullName() : TEXT("NULL"));
		return;
	}

	TArray<UK2Node*> AllNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);

	const bool bIsMacro = (Blueprint->BlueprintType == BPTYPE_MacroLibrary);
	if( AllNodes.Num() > 1 )
	{
		AllNodes.Sort(FCompareNodePriority());
	}

	bool bLastChangesStructure = (AllNodes.Num() > 0) ? AllNodes[0]->NodeCausesStructuralBlueprintChange() : true;
	for( TArray<UK2Node*>::TIterator NodeIt(AllNodes); NodeIt; ++NodeIt )
	{
		UK2Node* CurrentNode = *NodeIt;

		// See if we've finished the batch of nodes that affect structure, and recompile the skeleton if needed
		const bool bCurrentChangesStructure = CurrentNode->NodeCausesStructuralBlueprintChange();
		if( bLastChangesStructure != bCurrentChangesStructure )
		{
			// Make sure sorting was valid!
			check(bLastChangesStructure && !bCurrentChangesStructure);

			// Recompile the skeleton class, now that all changes to entry point structure has taken place
			// Ignore this for macros
			if (!bIsMacro)
			{
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}
			bLastChangesStructure = bCurrentChangesStructure;
		}

		//@todo:  Do we really need per-schema refreshing?
		const UEdGraphSchema* Schema = CurrentNode->GetGraph()->GetSchema();
		Schema->ReconstructNode(*CurrentNode, true);
	}

	// If all nodes change structure, catch that case and recompile now
	if( bLastChangesStructure )
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}


void FBlueprintEditorUtils::ReconstructAllNodes(UBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->HasAllFlags(RF_LoadCompleted))
	{
		UE_LOG(LogBlueprint, Warning,
			TEXT("ReconstructAllNodes called on incompletly loaded blueprint '%s'"),
			Blueprint ? *Blueprint->GetFullName() : TEXT("NULL"));
		return;
	}

	TArray<UK2Node*> AllNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);

	const bool bIsMacro = (Blueprint->BlueprintType == BPTYPE_MacroLibrary);
	if (AllNodes.Num() > 1)
	{
		AllNodes.Sort(FCompareNodePriority());
	}

	for (TArray<UK2Node*>::TIterator NodeIt(AllNodes); NodeIt; ++NodeIt)
	{
		UK2Node* CurrentNode = *NodeIt;
		//@todo:  Do we really need per-schema refreshing?
		const UEdGraphSchema* Schema = CurrentNode->GetGraph()->GetSchema();
		Schema->ReconstructNode(*CurrentNode, true);
	}
}

void FBlueprintEditorUtils::ReplaceDeprecatedNodes(UBlueprint* Blueprint)
{
	Blueprint->ReplaceDeprecatedNodes();
}

void FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(UBlueprint* Blueprint, UStruct* RefreshOnlyChild)
{
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RefreshExternalDependencyNodes);

	if (!Blueprint || !Blueprint->HasAllFlags(RF_LoadCompleted))
	{
		UE_LOG(LogBlueprint, Warning,
			TEXT("RefreshAllNodes called on incompletly loaded blueprint '%s'"),
			Blueprint ? *Blueprint->GetFullName() : TEXT("NULL"));
		return;
	}

	TArray<UK2Node*> AllNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);

	if (!RefreshOnlyChild)
	{
		for (UK2Node* Node : AllNodes)
		{
			if (Node->HasExternalDependencies())
			{
				//@todo:  Do we really need per-schema refreshing?
				const UEdGraphSchema* Schema = Node->GetGraph()->GetSchema();
				Schema->ReconstructNode(*Node, true);
			}
		}
	}
	else
	{
		for (UK2Node* Node : AllNodes)
		{
			TArray<UStruct*> Dependencies;
			if (Node->HasExternalDependencies(&Dependencies))
			{
				for (UStruct* Struct : Dependencies)
				{
					bool bShouldRefresh = Struct->IsChildOf(RefreshOnlyChild);
					if (!bShouldRefresh)
					{
						UClass* OwnerClass = Struct->GetOwnerClass();
						if (ensureMsgf(!OwnerClass || !OwnerClass->GetClass()->IsChildOf<UBlueprintGeneratedClass>() || OwnerClass->ClassGeneratedBy
							, TEXT("Malformed Blueprint class (%s) - bad node dependency, unable to determine if the %s node (%s) should be refreshed or not. Currently compiling: %s")
							, *OwnerClass->GetName()
							, *Node->GetClass()->GetName()
							, *Node->GetPathName()
							, *Blueprint->GetName()) )
						{
							bShouldRefresh |= OwnerClass &&
								(OwnerClass->IsChildOf(RefreshOnlyChild) || OwnerClass->GetAuthoritativeClass()->IsChildOf(RefreshOnlyChild));
						}						
					}
					if (bShouldRefresh)
					{
						//@todo:  Do we really need per-schema refreshing?
						const UEdGraphSchema* Schema = Node->GetGraph()->GetSchema();
						Schema->ReconstructNode(*Node, true);

						break;
					}
				}
			}
		}
	}
}

void FBlueprintEditorUtils::RefreshGraphNodes(const UEdGraph* Graph)
{
	TArray<UK2Node*> AllNodes;
	Graph->GetNodesOfClass(AllNodes);

	for (UK2Node* Node : AllNodes)
	{
		const UEdGraphSchema* Schema = Node->GetGraph()->GetSchema();
		Schema->ReconstructNode(*Node, true);
	}
}

void FBlueprintEditorUtils::PreloadMembers(UObject* InObject)
{
	// Collect a list of all things this element owns
	TArray<UObject*> BPMemberReferences;
	FReferenceFinder ComponentCollector(BPMemberReferences, InObject, false, true, true, true);
	ComponentCollector.FindReferences(InObject);

	// Iterate over the list, and preload everything so it is valid for refreshing
	for( TArray<UObject*>::TIterator it(BPMemberReferences); it; ++it )
	{
		UObject* CurrentObject = *it;
		if( CurrentObject->HasAnyFlags(RF_NeedLoad) )
		{
			FLinkerLoad* Linker = CurrentObject->GetLinker();
			if (Linker)
			{
				Linker->Preload(CurrentObject);
			}
			PreloadMembers(CurrentObject);
		}
	}
}

void FBlueprintEditorUtils::PreloadConstructionScript(UBlueprint* Blueprint)
{
	if ( Blueprint )
	{
		FLinkerLoad* TargetLinker = Blueprint->SimpleConstructionScript ? Blueprint->SimpleConstructionScript->GetLinker() : nullptr;
		if (TargetLinker)
		{
			TargetLinker->Preload(Blueprint->SimpleConstructionScript);

			if (USCS_Node* DefaultSceneRootNode = Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode())
			{
				DefaultSceneRootNode->PreloadChain();
			}

			const TArray<USCS_Node*>& RootNodes = Blueprint->SimpleConstructionScript->GetRootNodes();
			for (int32 NodeIndex = 0; NodeIndex < RootNodes.Num(); ++NodeIndex)
			{
				RootNodes[NodeIndex]->PreloadChain();
			}
		}

		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* SCSNode : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (SCSNode)
				{
					SCSNode->ValidateGuid();
				}
			}
		}
	}
}

void FBlueprintEditorUtils::PatchNewCDOIntoLinker(UObject* CDO, FLinkerLoad* Linker, int32 ExportIndex, TArray<UObject*>& ObjLoaded)
{
	if( (CDO != nullptr) && (Linker != nullptr) && (ExportIndex != INDEX_NONE) )
	{
		// Get rid of the old thing that was in its place
		UObject* OldCDO = Linker->ExportMap[ExportIndex].Object;
		if( OldCDO != nullptr )
		{
			EObjectFlags OldObjectFlags = OldCDO->GetFlags();
			OldCDO->ClearFlags(RF_NeedLoad|RF_NeedPostLoad);
			OldCDO->SetLinker(nullptr, INDEX_NONE);
			
			// Copy flags from the old CDO.
			CDO->SetFlags(OldObjectFlags);

			// Make sure the new CDO gets PostLoad called on it, so either add it to ObjLoaded list, or replace it if already present.
			int32 ObjLoadedIdx = ObjLoaded.Find(OldCDO);
			if (ObjLoadedIdx != INDEX_NONE)
			{
				ObjLoaded[ObjLoadedIdx] = CDO;
			}
			else if (OldObjectFlags & RF_NeedPostLoad)
			{
				ObjLoaded.Add(CDO);
			}
		}

		// Patch the new CDO in, and update the Export.Object
		CDO->SetLinker(Linker, ExportIndex);
		Linker->ExportMap[ExportIndex].Object = CDO;

		PatchCDOSubobjectsIntoExport(OldCDO, CDO);

		// This was set to true when the trash class was invalidated, but now we have a valid object
		Linker->ExportMap[ExportIndex].bExportLoadFailed = false;
	}
}

UClass* FBlueprintEditorUtils::FindFirstNativeClass(UClass* Class)
{
	for(; Class; Class = Class->GetSuperClass() )
	{
		if( 0 != (Class->ClassFlags & CLASS_Native))
		{
			break;
		}
	}
	return Class;
}

void FBlueprintEditorUtils::GetAllGraphNames(const UBlueprint* Blueprint, TSet<FName>& GraphNames)
{
	TArray< UEdGraph* > GraphList;
	Blueprint->GetAllGraphs(GraphList);

	for(int32 GraphIdx = 0; GraphIdx < GraphList.Num(); ++GraphIdx)
	{
		GraphNames.Add(GraphList[GraphIdx]->GetFName());
	}

	// Include all functions from parents because they should never conflict
	TArray<UBlueprint*> ParentBPStack;
	UBlueprint::GetBlueprintHierarchyFromClass(Blueprint->SkeletonGeneratedClass, ParentBPStack);
	for (int32 StackIndex = ParentBPStack.Num() - 1; StackIndex >= 0; --StackIndex)
	{
		UBlueprint* ParentBP = ParentBPStack[StackIndex];
		check(ParentBP != nullptr);

		for(int32 FunctionIndex = 0; FunctionIndex < ParentBP->FunctionGraphs.Num(); ++FunctionIndex)
		{
			GraphNames.Add(ParentBP->FunctionGraphs[FunctionIndex]->GetFName());
		}
	}
}

void FBlueprintEditorUtils::GetCompilerRelevantNodeLinks(UEdGraphPin* FromPin, FCompilerRelevantNodeLinkArray& OutNodeLinks)
{
	if(FromPin)
	{
		// Start with the given pin's owning node
		UK2Node* OwningNode = Cast<UK2Node>(FromPin->GetOwningNode());
		if(OwningNode)
		{
			// If this node is not compiler relevant
			if(!OwningNode->IsCompilerRelevant())
			{
				// And if this node has a matching "pass-through" pin
				FromPin = OwningNode->GetPassThroughPin(FromPin);
				if(FromPin)
				{
					// Recursively check each link for a compiler-relevant node that will "pass through" this node at compile time
					for (UEdGraphPin* LinkedPin : FromPin->LinkedTo)
					{
						GetCompilerRelevantNodeLinks(LinkedPin, OutNodeLinks);
					}
				}
			}
			else
			{
				OutNodeLinks.Add(FCompilerRelevantNodeLink(OwningNode, FromPin));
			}
		}		
	}
}

UK2Node* FBlueprintEditorUtils::FindFirstCompilerRelevantNode(UEdGraphPin* FromPin)
{
	FCompilerRelevantNodeLinkArray RelevantNodeLinks;
	GetCompilerRelevantNodeLinks(FromPin, RelevantNodeLinks);
	
	return RelevantNodeLinks.Num() > 0 ? RelevantNodeLinks[0].Node : nullptr;
}

UEdGraphPin* FBlueprintEditorUtils::FindFirstCompilerRelevantLinkedPin(UEdGraphPin* FromPin)
{
	FCompilerRelevantNodeLinkArray RelevantNodeLinks;
	GetCompilerRelevantNodeLinks(FromPin, RelevantNodeLinks);

	return RelevantNodeLinks.Num() > 0 ? RelevantNodeLinks[0].LinkedPin : nullptr;
}

/** 
 * Check FKismetCompilerContext::SetCanEverTickForActor
 */
struct FSaveActorFlagsHelper
{
	bool bOverride;
	bool bCanEverTick;
	UClass * Class;

	FSaveActorFlagsHelper(UClass * InClass) : Class(InClass)
	{
		bOverride = (AActor::StaticClass() == FBlueprintEditorUtils::FindFirstNativeClass(Class));
		if(Class && bOverride)
		{
			AActor* CDActor = Cast<AActor>(Class->GetDefaultObject());
			if(CDActor)
			{
				bCanEverTick = CDActor->PrimaryActorTick.bCanEverTick;
			}
		}
	}

	~FSaveActorFlagsHelper()
	{
		if(Class && bOverride)
		{
			AActor* CDActor = Cast<AActor>(Class->GetDefaultObject());
			if(CDActor)
			{
				CDActor->PrimaryActorTick.bCanEverTick = bCanEverTick;
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////

/**
 * Archive built to go through and find any references to objects in the transient package, and then NULL those references
 */
class FArchiveMoveSkeletalRefs : public FArchiveUObject
{
public:
	FArchiveMoveSkeletalRefs(UBlueprint* TargetBP)
		: TargetBlueprint(TargetBP)
	{
		ArIsObjectReferenceCollector = true;
		ArIsPersistent = false;
		ArIgnoreArchetypeRef = false;
	}

	void UpdateReferences()
	{
		if( TargetBlueprint != nullptr && (TargetBlueprint->BlueprintType != BPTYPE_MacroLibrary) )
		{
			if( ensureMsgf(TargetBlueprint->SkeletonGeneratedClass, TEXT("Blueprint %s is missing its skeleton generated class - known possible for assets on revision 1 older than 2088505"), *TargetBlueprint->GetName() ) )
			{
				TargetBlueprint->SkeletonGeneratedClass->GetDefaultObject()->Serialize(*this);
			}
			check(TargetBlueprint->GeneratedClass);
			TargetBlueprint->GeneratedClass->GetDefaultObject()->Serialize(*this);

			TArray<UObject*> SubObjs;
			GetObjectsWithOuter(TargetBlueprint, SubObjs, true);

			for (UObject* SubObj : SubObjs)
			{
				SubObj->Serialize(*this);
			}

			TargetBlueprint->bLegacyNeedToPurgeSkelRefs = false;
		}
	}

protected:
	UBlueprint* TargetBlueprint;

	/** 
	 * UObject serialize operator implementation
	 *
	 * @param Object	reference to Object reference
	 * @return reference to instance of this class
	 */
	FArchive& operator<<( UObject*& Object )
	{
		// Check if this is a reference to an object existing in the transient package, and if so, NULL it.
		if (Object != nullptr )
		{
			if( UClass* RefClass = Cast<UClass>(Object) )
			{
				const bool bIsValidBPGeneratedClass = RefClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint) && RefClass->ClassGeneratedBy;
				if (bIsValidBPGeneratedClass)
				{
					UClass* AuthClass = RefClass->GetAuthoritativeClass();
					if (RefClass != AuthClass)
					{
						Object = AuthClass;
					}
				}
			}
		}

		return *this;
	}

private:
	// Want to make them HAVE to use a blueprint, so we can control what we replace refs on
	FArchiveMoveSkeletalRefs()
	{
	}
};

//////////////////////////////////////////////////////////////////////////

struct FRegenerationHelper
{
	static void PreloadAndLinkIfNecessary(UStruct* Struct)
	{
		bool bChanged = false;
		if (Struct->HasAnyFlags(RF_NeedLoad))
		{
			if (FLinkerLoad* Linker = Struct->GetLinker())
			{
				Linker->Preload(Struct);
				bChanged = true;
			}
		}

		UBlueprint::ForceLoadMetaData(Struct);

		const int32 OldPropertiesSize = Struct->GetPropertiesSize();
		for (UField* Field = Struct->Children; Field; Field = Field->Next)
		{
			bChanged |= UBlueprint::ForceLoad(Field);
		}

		if (bChanged)
		{
			Struct->StaticLink(true);
			ensure(Struct->IsA<UFunction>() || (OldPropertiesSize == Struct->GetPropertiesSize()) || !Struct->HasAnyFlags(RF_LoadCompleted));
		}
	}

	static UBlueprint* GetGeneratingBlueprint(const UObject* Obj)
	{
		const UBlueprintGeneratedClass* BPGC = nullptr;
		while (!BPGC && Obj)
		{
			BPGC = Cast<const UBlueprintGeneratedClass>(Obj);
			Obj = Obj->GetOuter();
		}

		return UBlueprint::GetBlueprintFromClass(BPGC);
	}

	static void ProcessHierarchy(UStruct* Struct, TSet<UStruct*>& Dependencies)
	{
		if (Struct)
		{
			bool bAlreadyProcessed = false;
			Dependencies.Add(Struct, &bAlreadyProcessed);
			if (!bAlreadyProcessed)
			{
				ProcessHierarchy(Struct->GetSuperStruct(), Dependencies);

				const UBlueprint* BP = GetGeneratingBlueprint(Struct);
				const bool bProcessBPGClass = BP && !BP->bHasBeenRegenerated;
				const bool bProcessUserDefinedStruct = Struct->IsA<UUserDefinedStruct>();
				if (bProcessBPGClass || bProcessUserDefinedStruct)
				{
					PreloadAndLinkIfNecessary(Struct);
				}
			}
		}
	}

	static void PreloadMacroSources(TSet<UBlueprint*>& MacroSources)
	{
		for (UBlueprint* BP : MacroSources)
		{
			if (!BP->bHasBeenRegenerated)
			{
				if (BP->HasAnyFlags(RF_NeedLoad))
				{
					if (FLinkerLoad* Linker = BP->GetLinker())
					{
						Linker->Preload(BP);
					}
				}
				// at the point of blueprint regeneration (on load), we are guaranteed that blueprint dependencies (like this macro) have
				// fully formed classes (meaning the blueprint class and all its direct dependencies have been loaded)... however, we do not 
				// get the guarantee that all of that blueprint's graph dependencies are loaded (hence, why we have to force load 
				// everything here); in the case of cyclic dependencies, macro dependencies could already be loaded, but in the midst of 
				// resolving thier own dependency placeholders (why a ForceLoad() call is not enough); this ensures that 
				// placeholder objects are properly resolved on nodes that will be injected by macro expansion
				FLinkerLoad::PRIVATE_ForceLoadAllDependencies(BP->GetOutermost());
				
				UBlueprint::ForceLoadMembers(BP);
			}
		}
	}

	/**
	 * A helper function that loads (and regenerates) interface dependencies.
	 * Accounts for circular dependencies by following how we handle parent 
	 * classes in FLinkerLoad::RegenerateBlueprintClass() (that is, to complete 
	 * the interface's compilation/regeneration before we utilize it for the
	 * specified blueprint).
	 * 
	 * @param  Blueprint	The blueprint whose implemented interfaces you want loaded.
	 */
	static void PreloadInterfaces(UBlueprint* Blueprint, TArray<UObject*>& ObjLoaded)
	{
#if WITH_EDITORONLY_DATA // ImplementedInterfaces is wrapped WITH_EDITORONLY_DATA 
		for (FBPInterfaceDescription const& InterfaceDesc : Blueprint->ImplementedInterfaces)
		{
			UClass* InterfaceClass = InterfaceDesc.Interface;
			UBlueprint* InterfaceBlueprint = InterfaceClass ? Cast<UBlueprint>(InterfaceClass->ClassGeneratedBy) : nullptr;
			if (InterfaceBlueprint)
			{
				UBlueprint::ForceLoadMembers(InterfaceBlueprint);
				if (InterfaceBlueprint->HasAnyFlags(RF_BeingRegenerated))
				{
					InterfaceBlueprint->RegenerateClass(InterfaceClass, InterfaceClass->ClassDefaultObject, ObjLoaded);
				}
			}
		}
#endif // #if WITH_EDITORONLY_DATA
	}

	static void LinkExternalDependencies(UBlueprint* Blueprint, TArray<UObject*>& ObjLoaded)
	{
		check(Blueprint);
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		TSet<UStruct*> Dependencies;
		ProcessHierarchy(Blueprint->ParentClass, Dependencies);
		
		for (const FBPVariableDescription& NewVar : Blueprint->NewVariables)
		{
			if (UObject* TypeObject = NewVar.VarType.PinSubCategoryObject.Get())
			{
				FLinkerLoad* Linker = TypeObject->GetLinker();
				if (Linker && TypeObject->HasAnyFlags(RF_NeedLoad))
				{
					Linker->Preload(TypeObject);
				}
			}

			if (UClass* TypeClass = NewVar.VarType.PinSubCategoryMemberReference.GetMemberParentClass())
			{
				FLinkerLoad* Linker = TypeClass->GetLinker();
				if (Linker && TypeClass->HasAnyFlags(RF_NeedLoad))
				{
					Linker->Preload(TypeClass);
				}
			}
		}

		TSet<UBlueprint*> MacroSources;
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && !FBlueprintEditorUtils::IsGraphIntermediate(Graph))
			{
				const bool bIsDelegateSignatureGraph = FBlueprintEditorUtils::IsDelegateSignatureGraph(Graph);

				TArray<UK2Node*> Nodes;
				Graph->GetNodesOfClass(Nodes);
				for (UK2Node* Node : Nodes)
				{
					if (Node)
					{
						TArray<UStruct*> LocalDependentStructures;
						if (Node->HasExternalDependencies(&LocalDependentStructures))
						{
							for (UStruct* Struct : LocalDependentStructures)
							{
								ProcessHierarchy(Struct, Dependencies);
							}

							if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
							{
								if (UBlueprint* MacroSource = MacroNode->GetSourceBlueprint())
								{
									MacroSources.Add(MacroSource);
								}
							}

							// If a variable node has an external dependency, then its BP class will differ from ours. For array properties,
							// the external BP class (and thus the array property itself) will have been loaded/processed via the above
							// ProcessHierarchy() call. However, the array's 'Inner' property may not have been preloaded as part of that path.
							// Thus, we handle that here in order to ensure that all 'Inner' fields are also valid before class regeneration.
							if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
							{
								UArrayProperty* ArrayProperty = Cast<UArrayProperty>(VariableNode->VariableReference.ResolveMember<UProperty>(Node->GetBlueprintClassFromNode()));
								if (ArrayProperty != nullptr && ArrayProperty->Inner != nullptr && ArrayProperty->Inner->HasAnyFlags(RF_NeedLoad|RF_WasLoaded))
								{
									UBlueprint::ForceLoad(ArrayProperty->Inner);
								}
							}
						}

						UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node);
						if (FunctionEntry && !bIsDelegateSignatureGraph)
						{
							const FName FunctionName = (FunctionEntry->CustomGeneratedFunctionName != NAME_None) 
								? FunctionEntry->CustomGeneratedFunctionName 
								: FunctionEntry->SignatureName;
							UFunction* ParentFunction = Blueprint->ParentClass ? Blueprint->ParentClass->FindFunctionByName(FunctionName) : nullptr;
							if (ParentFunction && (Schema->FN_UserConstructionScript != FunctionName))
							{
								ProcessHierarchy(ParentFunction, Dependencies);
							}
						}

						// load Enums
						for (UEdGraphPin* Pin : Node->Pins)
						{
							UObject* SubCategoryObject = Pin ? Pin->PinType.PinSubCategoryObject.Get() : nullptr;
							if (SubCategoryObject && SubCategoryObject->IsA<UEnum>())
							{
								UBlueprint::ForceLoad(SubCategoryObject);
							}
						}
					}
				}
			}
		}
		PreloadMacroSources(MacroSources);

		PreloadInterfaces(Blueprint, ObjLoaded);
	}
};

/**
	Procedure used to remove old function implementations and child properties from data only blueprints.
	These blueprints have a 'fast path' compilation path but we need to make sure that any data regenerated 
	by normal blueprint compilation is cleared here. If we don't then these functions and properties will
	hang around when a class is converted from a real blueprint to a data only blueprint.
*/
void FBlueprintEditorUtils::RemoveStaleFunctions(UBlueprintGeneratedClass* Class, UBlueprint* Blueprint)
{
	if (Class == nullptr)
	{
		return;
	}

	// Removes all existing functions from the class, currently used 
	TFieldIterator<UFunction> Fn(Class, EFieldIteratorFlags::ExcludeSuper);
	if (Fn)
	{
		FString OrphanedClassString = FString::Printf(TEXT("ORPHANED_DATA_ONLY_%s"), *Class->GetName());
		FName OrphanedClassName = MakeUniqueObjectName(GetTransientPackage(), UBlueprintGeneratedClass::StaticClass(), FName(*OrphanedClassString));
		UClass* OrphanedClass = NewObject<UBlueprintGeneratedClass>(GetTransientPackage(), OrphanedClassName, RF_Public | RF_Transient);
		OrphanedClass->ClassAddReferencedObjects = Class->AddReferencedObjects;
		OrphanedClass->ClassFlags |= CLASS_CompiledFromBlueprint;
		OrphanedClass->ClassGeneratedBy = Class->ClassGeneratedBy;

		const ERenameFlags RenFlags = REN_DontCreateRedirectors | (Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_NonTransactional | REN_DoNotDirty;

		while (Fn)
		{
			UFunction* Function = *Fn;
			Class->RemoveFunctionFromFunctionMap(Function);
			Function->Rename(nullptr, OrphanedClass, RenFlags);

			// invalidate this package's reference to this function, so 
			// subsequent packages that import it will treat it as if it didn't 
			// exist (because data-only blueprints shouldn't have functions)
			FLinkerLoad::InvalidateExport(Function); 
			++Fn;
		}
	}

	// Clear function map caches which will be rebuilt the next time functions are searched by name
	Class->ClearFunctionMapsCaches();

	Blueprint->GeneratedClass->Children = nullptr;
	Blueprint->GeneratedClass->Bind();
	Blueprint->GeneratedClass->StaticLink(true);
}

void FBlueprintEditorUtils::RefreshVariables(UBlueprint* Blueprint)
{
	// module punchthrough:
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	Compiler.RefreshVariables(Blueprint);
}

void FBlueprintEditorUtils::PreloadBlueprintSpecificData(UBlueprint* Blueprint)
{
	TArray<UK2Node*> AllNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);

	for( UK2Node* K2Node : AllNodes )
	{
		K2Node->PreloadRequiredAssets();
	}
}

UClass* FBlueprintEditorUtils::RegenerateBlueprintClass(UBlueprint* Blueprint, UClass* ClassToRegenerate, UObject* PreviousCDO, TArray<UObject*>& ObjLoaded)
{
	bool bRegenerated = false;

	// Cache off the dirty flag for the package, so we can restore it later
	UPackage* Package = Blueprint->GetOutermost();
	bool bIsPackageDirty = Package ? Package->IsDirty() : false;

	// Preload the blueprint and all its parts before refreshing nodes. 
	// Otherwise, the nodes might not maintain their proper linkages... 
	//
	// This all should also happen here, first thing, before 
	// bIsRegeneratingOnLoad is set, so that we can re-enter this function for 
	// the same class further down the callstack (presumably from 
	// PreloadInterfaces() or some other dependency load). This is here to 
	// handle circular dependencies, where pre-loading a member here sets off a  
	// subsequent load that in turn, relies on this class and requires this  
	// class to be fully generated... A second call to this function with the 
	// same class will continue to preload all it's members (from where it left
	// off, since they're gated by a RF_NeedLoad check) and then fall through to
	// finish compiling the class (while it's still technically pre-loading a
	// member further up the stack).
	if (!Blueprint->bHasBeenRegenerated)
	{
		UBlueprint::ForceLoadMetaData(Blueprint);
		if (ensure(PreviousCDO))
		{
			UBlueprint::ForceLoadMembers(PreviousCDO);
		}
		UBlueprint::ForceLoadMembers(Blueprint);
	}

	if( ShouldRegenerateBlueprint(Blueprint) && !Blueprint->bHasBeenRegenerated )
	{
		Blueprint->bCachedDependenciesUpToDate = false;
		Blueprint->bIsRegeneratingOnLoad = true;

		// Cache off the linker index, if needed
		FName GeneratedName, SkeletonName;
		Blueprint->GetBlueprintCDONames(GeneratedName, SkeletonName);
		int32 OldSkelLinkerIdx = INDEX_NONE;
		int32 OldGenLinkerIdx = INDEX_NONE;
		FLinkerLoad* OldLinker = Blueprint->GetLinker();
		for( int32 i = 0; i < OldLinker->ExportMap.Num(); i++ )
		{
			FObjectExport& ThisExport = OldLinker->ExportMap[i];
			if( ThisExport.ObjectName == SkeletonName )
			{
				OldSkelLinkerIdx = i;
			}
			else if( ThisExport.ObjectName == GeneratedName )
			{
				OldGenLinkerIdx = i;
			}

			if( OldSkelLinkerIdx != INDEX_NONE && OldGenLinkerIdx != INDEX_NONE )
			{
				break;
			}
		}

		// Make sure the simple construction script is loaded, since the outer hierarchy isn't compatible with PreloadMembers past the root node
		FBlueprintEditorUtils::PreloadConstructionScript(Blueprint);

		// Preload Overridden Components
		if (Blueprint->InheritableComponentHandler)
		{
			Blueprint->InheritableComponentHandler->PreloadAll();
		}

		// Purge any NULL graphs
		FBlueprintEditorUtils::PurgeNullGraphs(Blueprint);

		// Now that things have been preloaded, see what work needs to be done to refresh this blueprint
		const bool bIsMacro = (Blueprint->BlueprintType == BPTYPE_MacroLibrary);
		const bool bHasCode = !FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint) && !bIsMacro;

		// Make sure all used external classes/functions/structures/macros/etc are loaded and linked
		FRegenerationHelper::LinkExternalDependencies(Blueprint, ObjLoaded);

		bool bSkeletonUpToDate = FKismetEditorUtilities::GenerateBlueprintSkeleton(Blueprint);

		const bool bDataOnlyClassThatMustBeRecompiled = !bHasCode && !bIsMacro
			&& (!ClassToRegenerate || (Blueprint->ParentClass != ClassToRegenerate->GetSuperClass()));

		UBlueprintGeneratedClass* BPGClassToRegenerate = Cast<UBlueprintGeneratedClass>(ClassToRegenerate);
		const bool bHasPendingUberGraphFrame = BPGClassToRegenerate
			&& (BPGClassToRegenerate->UberGraphFramePointerProperty || BPGClassToRegenerate->UberGraphFunction);

		const bool bDefaultComponentMustBeAdded = !bHasCode 
			&& BPGClassToRegenerate
			&& SupportsConstructionScript(Blueprint) 
			&& BPGClassToRegenerate->SimpleConstructionScript
			&& (nullptr == BPGClassToRegenerate->SimpleConstructionScript->GetSceneRootComponentTemplate());
		const bool bShouldBeRecompiled = bHasCode || bDataOnlyClassThatMustBeRecompiled || bHasPendingUberGraphFrame || bDefaultComponentMustBeAdded;

		if (bShouldBeRecompiled)
		{
			// Make sure parent function calls are up to date
			FBlueprintEditorUtils::ConformCallsToParentFunctions(Blueprint);

			// Make sure events are up to date
			FBlueprintEditorUtils::ConformImplementedEvents(Blueprint);
			
			// Make sure interfaces are up to date
			FBlueprintEditorUtils::ConformImplementedInterfaces(Blueprint);

			// Reconstruct all nodes, this will call AllocateDefaultPins, which ensures
			// that nodes have a chance to create all the pins they'll expect when they compile.
			// A good example of why this is necessary is UK2Node_BaseAsyncTask::AllocateDefaultPins
			// and it's companion function UK2Node_BaseAsyncTask::ExpandNode.
			FBlueprintEditorUtils::ReconstructAllNodes(Blueprint);

			FBlueprintEditorUtils::ReplaceDeprecatedNodes(Blueprint);

			// Compile the actual blueprint
			EBlueprintCompileOptions Options = EBlueprintCompileOptions::IsRegeneratingOnLoad;
			if(bSkeletonUpToDate)
			{
				Options |= EBlueprintCompileOptions::SkeletonUpToDate;
			}
			FKismetEditorUtilities::CompileBlueprint(Blueprint, Options, nullptr);
		}
		else if( bIsMacro )
		{
			// Just refresh all nodes in macro blueprints, but don't recompile
			FBlueprintEditorUtils::RefreshAllNodes(Blueprint);

			FBlueprintEditorUtils::ReplaceDeprecatedNodes(Blueprint);

			if (ClassToRegenerate != nullptr)
			{
				UClass* OldSuperClass = ClassToRegenerate->GetSuperClass();
				if ((OldSuperClass != nullptr) && OldSuperClass->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					UClass* NewSuperClass = OldSuperClass->GetAuthoritativeClass();
					ensure(NewSuperClass == Blueprint->ParentClass);

					// in case the macro's super class was re-instanced (it 
					// would have re-parented this to a REINST_ class), for non-
					// macro blueprints this would normally be reset in 
					// CompileBlueprint (but since we don't compile macros, we 
					// need to fix this up here)
					ClassToRegenerate->SetSuperStruct(NewSuperClass);
				}
			}

			// Flag macro blueprints as being up-to-date
			Blueprint->Status = BS_UpToDate;
		}
		else
		{
			if (Blueprint->IsGeneratedClassAuthoritative() && (Blueprint->GeneratedClass != nullptr))
			{
				RemoveStaleFunctions(Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass), Blueprint);

				check(PreviousCDO != nullptr);
				check(Blueprint->SkeletonGeneratedClass != nullptr);

				// We now know we're a data-only blueprint on the outer pass (generate class is valid), where generated class is authoritative
				// If the PreviousCDO is to the skeleton, then it will corrupt data when copied over the AuthoriativeClass later on in this function
				if (PreviousCDO == Blueprint->SkeletonGeneratedClass->GetDefaultObject())
				{
					check(Blueprint->PRIVATE_InnermostPreviousCDO == nullptr);
					Blueprint->PRIVATE_InnermostPreviousCDO = Blueprint->GeneratedClass->GetDefaultObject();
				}
			}

			// No actual compilation work to be done, but try to conform the class and fix up anything that might need to be updated if the native base class has changed in any way
			FKismetEditorUtilities::ConformBlueprintFlagsAndComponents(Blueprint);

			if (Blueprint->GeneratedClass)
			{
				FBlueprintEditorUtils::RecreateClassMetaData(Blueprint, Blueprint->GeneratedClass, true);
				
				extern COREUOBJECT_API void SetUpRuntimeReplicationData(UClass* Class);
				SetUpRuntimeReplicationData(Blueprint->GeneratedClass);
			}

			// Flag data only blueprints as being up-to-date
			Blueprint->Status = BS_UpToDate;
		}
		
		// Patch the new CDOs to the old indices in the linker
		if( Blueprint->SkeletonGeneratedClass )
		{
			PatchNewCDOIntoLinker(Blueprint->SkeletonGeneratedClass->GetDefaultObject(), OldLinker, OldSkelLinkerIdx, ObjLoaded);
		}
		if( Blueprint->GeneratedClass )
		{
			PatchNewCDOIntoLinker(Blueprint->GeneratedClass->GetDefaultObject(), OldLinker, OldGenLinkerIdx, ObjLoaded);
		}

		// Success or failure, there's no point in trying to recompile this class again when other objects reference it
		// redo data only blueprints later, when we actually have a generated class
		Blueprint->bHasBeenRegenerated = !FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint) || Blueprint->GeneratedClass != nullptr; 

		Blueprint->bIsRegeneratingOnLoad = false;

		bRegenerated = bShouldBeRecompiled;

		if (!FKismetEditorUtilities::IsClassABlueprintSkeleton(ClassToRegenerate))
		{
			if (Blueprint->bRecompileOnLoad)
			{
				// Verify that we had a skeleton generated class if we had a previous CDO, to make sure we have something to copy into
				check((Blueprint->BlueprintType == BPTYPE_MacroLibrary) || Blueprint->SkeletonGeneratedClass);

				const bool bPreviousMatchesGenerated = (PreviousCDO == Blueprint->GeneratedClass->GetDefaultObject());

				if (Blueprint->BlueprintType != BPTYPE_MacroLibrary)
				{
					UObject* CDOThatKickedOffCOL = PreviousCDO;
					if (Blueprint->IsGeneratedClassAuthoritative() && !bPreviousMatchesGenerated && Blueprint->PRIVATE_InnermostPreviousCDO)
					{
						PreviousCDO = Blueprint->PRIVATE_InnermostPreviousCDO;
					}
				}

				// If this is the top of the compile-on-load stack for this object, copy the old CDO properties to the newly created one unless they are the same
				UClass* AuthoritativeClass = (Blueprint->IsGeneratedClassAuthoritative() ? Blueprint->GeneratedClass : Blueprint->SkeletonGeneratedClass);
				if (AuthoritativeClass != nullptr && PreviousCDO != AuthoritativeClass->GetDefaultObject())
				{
					TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);

					// Make sure the previous CDO has been fully loaded before we use it
					FBlueprintEditorUtils::PreloadMembers(PreviousCDO);

					// Copy over the properties from the old CDO to the new
					PropagateParentBlueprintDefaults(AuthoritativeClass);
					UObject* NewCDO = AuthoritativeClass->GetDefaultObject();
					{
						FSaveActorFlagsHelper SaveActorFlags(AuthoritativeClass);
						UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams CopyDetails;
						CopyDetails.bAggressiveDefaultSubobjectReplacement = true;
						CopyDetails.bDoDelta = false;
						CopyDetails.bCopyDeprecatedProperties = true;
						CopyDetails.bSkipCompilerGeneratedDefaults = true;
						UEditorEngine::CopyPropertiesForUnrelatedObjects(PreviousCDO, NewCDO, CopyDetails);
					}

					if (bRegenerated)
					{
						PatchCDOSubobjectsIntoExport(PreviousCDO, NewCDO);
						// We purposefully do not call post load here, it happens later on in the normal flow
					}

					// Update the custom property list used in post construction logic to include native class properties for which the regenerated Blueprint CDO now differs from the native CDO.
					if (UBlueprintGeneratedClass* BPGClass = Cast<UBlueprintGeneratedClass>(AuthoritativeClass))
					{
						BPGClass->UpdateCustomPropertyListForPostConstruction();
					}
				}

				Blueprint->PRIVATE_InnermostPreviousCDO = nullptr;
			}
			else
			{
				// If we didn't recompile, we still need to propagate flags, and instance components
				FKismetEditorUtilities::ConformBlueprintFlagsAndComponents(Blueprint);
			}

			// If this is the top of the compile-on-load stack for this object, copy the old CDO properties to the newly created one
			if (!Blueprint->IsGeneratedClassAuthoritative() && Blueprint->GeneratedClass != nullptr)
			{
				TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);

				UObject* SkeletonCDO = Blueprint->SkeletonGeneratedClass->GetDefaultObject();
				UObject* GeneratedCDO = Blueprint->GeneratedClass->GetDefaultObject();

				UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams CopyDetails;
				CopyDetails.bAggressiveDefaultSubobjectReplacement = false;
				CopyDetails.bDoDelta = false;
				UEditorEngine::CopyPropertiesForUnrelatedObjects(SkeletonCDO, GeneratedCDO, CopyDetails);

				Blueprint->SetLegacyGeneratedClassIsAuthoritative();
			}

			// Now that the CDO is valid, update the OwnedComponents, in case we've added or removed native components
			if (AActor* MyActor = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
			{
				MyActor->ResetOwnedComponents();
			}
		}
	}
	else
	{
		if (Blueprint->GeneratedClass && !Blueprint->bHasBeenRegenerated && !Blueprint->bIsRegeneratingOnLoad)
		{
			FObjectDuplicationParameters Params(Blueprint->GeneratedClass, Blueprint->GeneratedClass->GetOuter());
			Params.ApplyFlags = RF_Transient;
			Params.DestName = *(FString("SKEL_COPY_") + Blueprint->GeneratedClass->GetName());
			Blueprint->SkeletonGeneratedClass = (UClass*)StaticDuplicateObjectEx(Params);
		}
	}

	if ( bRegenerated )
	{		
		// Fix any invalid metadata
		UPackage* GeneratedClassPackage = Blueprint->GeneratedClass->GetOuterUPackage();
		GeneratedClassPackage->GetMetaData()->RemoveMetaDataOutsidePackage();
	}

	bool const bNeedsSkelRefRemoval = !FKismetEditorUtilities::IsClassABlueprintSkeleton(ClassToRegenerate) && (Blueprint->SkeletonGeneratedClass != nullptr);
	if (bNeedsSkelRefRemoval && Blueprint->bLegacyNeedToPurgeSkelRefs)
	{
		// Remove any references to the skeleton class, replacing them with refs to the generated class instead
		FArchiveMoveSkeletalRefs SkelRefArchiver(Blueprint);
		SkelRefArchiver.UpdateReferences();
	}

	// Restore the dirty flag
	if( Package )
	{
		Package->SetDirtyFlag(bIsPackageDirty);
	}

	return bRegenerated ? Blueprint->GeneratedClass : nullptr;
}

void FBlueprintEditorUtils::LinkExternalDependencies(UBlueprint* Blueprint)
{
	TArray<UObject*> Unused;
	FRegenerationHelper::LinkExternalDependencies(Blueprint, Unused);
}

void FBlueprintEditorUtils::RecreateClassMetaData(UBlueprint* Blueprint, UClass* Class, bool bRemoveExistingMetaData)
{
	if (!ensure(Blueprint && Class))
	{
		return;
	}

	UClass* ParentClass = Class->GetSuperClass();
	TArray<FString> AllHideCategories;

	if (bRemoveExistingMetaData)
	{
		Class->RemoveMetaData("HideCategories");
		Class->RemoveMetaData("ShowCategories");
		Class->RemoveMetaData("HideFunctions");
		Class->RemoveMetaData("AutoExpandCategories");
		Class->RemoveMetaData("AutoCollapseCategories");
		Class->RemoveMetaData("ClassGroupNames");
		Class->RemoveMetaData("Category");
		Class->RemoveMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType);
	}

	if (ensure(ParentClass != nullptr))
	{
		if (!ParentClass->HasMetaData(FBlueprintMetadata::MD_IgnoreCategoryKeywordsInSubclasses))
		{
			// we want the categories just as they appear in the parent class 
			// (set bHomogenize to false) - especially since homogenization 
			// could inject spaces
			FEditorCategoryUtils::GetClassHideCategories(ParentClass, AllHideCategories, /*bHomogenize =*/false);
			if (ParentClass->HasMetaData(TEXT("ShowCategories")))
			{
				Class->SetMetaData(TEXT("ShowCategories"), *ParentClass->GetMetaData("ShowCategories"));
			}
			if (ParentClass->HasMetaData(TEXT("AutoExpandCategories")))
			{
				Class->SetMetaData(TEXT("AutoExpandCategories"), *ParentClass->GetMetaData("AutoExpandCategories"));
			}
			if (ParentClass->HasMetaData(TEXT("AutoCollapseCategories")))
			{
				Class->SetMetaData(TEXT("AutoCollapseCategories"), *ParentClass->GetMetaData("AutoCollapseCategories"));
			}
		}

		if (ParentClass->HasMetaData(TEXT("HideFunctions")))
		{
			Class->SetMetaData(TEXT("HideFunctions"), *ParentClass->GetMetaData("HideFunctions"));
		}

		if (ParentClass->IsChildOf(UActorComponent::StaticClass()))
		{
			static const FName NAME_ClassGroupNames(TEXT("ClassGroupNames"));
			Class->SetMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent, TEXT("true"));

			FString ClassGroupCategory = NSLOCTEXT("BlueprintableComponents", "CategoryName", "Custom").ToString();
			if (!Blueprint->BlueprintCategory.IsEmpty())
			{
				ClassGroupCategory = Blueprint->BlueprintCategory;
			}

			Class->SetMetaData(NAME_ClassGroupNames, *ClassGroupCategory);
		}
	}

	// Add a category if one has been specified
	if (Blueprint->BlueprintCategory.Len() > 0)
	{
		Class->SetMetaData(TEXT("Category"), *Blueprint->BlueprintCategory);
	}
	else
	{
		Class->RemoveMetaData(TEXT("Category"));
	}

	if ((Blueprint->BlueprintType == BPTYPE_Normal) ||
		(Blueprint->BlueprintType == BPTYPE_Const) ||
		(Blueprint->BlueprintType == BPTYPE_Interface))
	{
		Class->SetMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType, TEXT("true"));
	}

	for (FString HideCategory : Blueprint->HideCategories)
	{
		TArray<TCHAR>& CharArray = HideCategory.GetCharArray();

		int32 SpaceIndex = CharArray.Find(TEXT(' '));
		while (SpaceIndex != INDEX_NONE)
		{
			CharArray.RemoveAt(SpaceIndex);
			if (SpaceIndex >= CharArray.Num())
			{
				break;
			}

			TCHAR& WordStartingChar = CharArray[SpaceIndex];
			WordStartingChar = FChar::ToUpper(WordStartingChar);

			CharArray.Find(TEXT(' '), SpaceIndex);
		}
		AllHideCategories.Add(HideCategory);
	}

	if (AllHideCategories.Num() > 0)
	{
		Class->SetMetaData(TEXT("HideCategories"), *FString::Join(AllHideCategories, TEXT(" ")));
	}
	else
	{
		Class->RemoveMetaData(TEXT("HideCategories"));
	}
}


void FBlueprintEditorUtils::PatchCDOSubobjectsIntoExport(UObject* PreviousCDO, UObject* NewCDO)
{
	if (PreviousCDO && NewCDO)
	{
		struct PatchCDOSubobjectsIntoExport_Impl
		{
			static void PatchSubObjects(UObject* OldObj, UObject* NewObj)
			{
				TMap<FName, UObject*> SubObjLookupTable;
				ForEachObjectWithOuter(NewObj, [&SubObjLookupTable](UObject* NewSubObj)
				{
					if (NewSubObj != nullptr)
					{
						SubObjLookupTable.Add(NewSubObj->GetFName(), NewSubObj);
					}
				}, /*bIncludeNestedSubObjects =*/false);

				TArray<UObject*> OldSubObjects;
				GetObjectsWithOuter(OldObj, OldSubObjects, /*bIncludeNestedSubObjects =*/false);

				for (UObject* OldSubObj : OldSubObjects)
				{
					if (UObject** NewSubObjPtr = SubObjLookupTable.Find(OldSubObj->GetFName()))
					{
						UObject* NewSubObj = *NewSubObjPtr;
						if (NewSubObj->IsDefaultSubobject() && OldSubObj->IsDefaultSubobject())
						{
							FLinkerLoad::PRIVATE_PatchNewObjectIntoExport(OldSubObj, NewSubObj);

							UClass* SubObjClass = OldSubObj->GetClass();
							if (SubObjClass && SubObjClass->HasAnyClassFlags(CLASS_HasInstancedReference))
							{
								TSet<FInstancedSubObjRef> OldInstancedValues;
								FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects(OldSubObj, OldInstancedValues);

								for (const FInstancedSubObjRef& OldInstancedObj : OldInstancedValues)
								{
									if (UObject* NewInstancedObj = OldInstancedObj.PropertyPath.Resolve(NewSubObj))
									{
										FLinkerLoad::PRIVATE_PatchNewObjectIntoExport(OldInstancedObj, NewInstancedObj);
									}
								}
							}
						}

						PatchSubObjects(OldSubObj, NewSubObj);
					}
				}
			}
		};
		PatchCDOSubobjectsIntoExport_Impl::PatchSubObjects(PreviousCDO, NewCDO);
		NewCDO->CheckDefaultSubobjects();
	}
}

void FBlueprintEditorUtils::PropagateParentBlueprintDefaults(UClass* ClassToPropagate)
{
	check(ClassToPropagate);

	UObject* NewCDO = ClassToPropagate->GetDefaultObject();
	
	check(NewCDO);

	// Get the blueprint's BP derived lineage
	TArray<UBlueprint*> ParentBP;
	UBlueprint::GetBlueprintHierarchyFromClass(ClassToPropagate, ParentBP);

	// Starting from the least derived BP class, copy the properties into the new CDO
	for(int32 i = ParentBP.Num() - 1; i > 0; i--)
	{
		checkf(ParentBP[i]->GeneratedClass != nullptr, TEXT("Parent classes for class %s have not yet been generated.  Compile-on-load must be processed for the parent class first."), *ClassToPropagate->GetName());
		UObject* LayerCDO = ParentBP[i]->GeneratedClass->GetDefaultObject();

		UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams CopyDetails;
		CopyDetails.bReplaceObjectClassReferences = false;
		UEditorEngine::CopyPropertiesForUnrelatedObjects(LayerCDO, NewCDO, CopyDetails);
	}
}

UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;

uint32 FBlueprintDuplicationScopeFlags::bStaticFlags = FBlueprintDuplicationScopeFlags::NoFlags;

void FBlueprintEditorUtils::PostDuplicateBlueprint(UBlueprint* Blueprint, bool bDuplicateForPIE)
{
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData); 
	
	// Only recompile after duplication if this isn't PIE
	if (!bDuplicateForPIE)
	{
		check(Blueprint->GeneratedClass != nullptr);
		{
			// Grab the old CDO, which contains the class defaults
			UClass* OldBPGCAsClass = Blueprint->GeneratedClass;
			UBlueprintGeneratedClass* OldBPGC = (UBlueprintGeneratedClass*)(OldBPGCAsClass);
			UObject* OldCDO = OldBPGC->GetDefaultObject();
			check(OldCDO != nullptr);

			if (FBlueprintDuplicationScopeFlags::HasAnyFlag(FBlueprintDuplicationScopeFlags::ValidatePinsUsingSourceClass))
			{
				Blueprint->OriginalClass = OldBPGC;
			}

			// Grab the old class templates, which needs to be moved to the new class
			USimpleConstructionScript* SCSRootNode = Blueprint->SimpleConstructionScript;
			Blueprint->SimpleConstructionScript = nullptr;

			UInheritableComponentHandler* InheritableComponentHandler = Blueprint->InheritableComponentHandler;
			Blueprint->InheritableComponentHandler = nullptr;

			TArray<UActorComponent*> Templates = Blueprint->ComponentTemplates;
			Blueprint->ComponentTemplates.Empty();

			TArray<UTimelineTemplate*> Timelines = Blueprint->Timelines;
			Blueprint->Timelines.Empty();

			Blueprint->GeneratedClass = nullptr;
			Blueprint->SkeletonGeneratedClass = nullptr;

			// Make sure the new blueprint has a shiny new class
			IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
			FCompilerResultsLog Results;
			FKismetCompilerOptions CompileOptions;
			CompileOptions.bIsDuplicationInstigated = true;

			FName NewSkelClassName, NewGenClassName;
			Blueprint->GetBlueprintClassNames(NewGenClassName, NewSkelClassName);

			UClass* NewClass = NewObject<UClass>(
				Blueprint->GetOutermost(), Blueprint->GetBlueprintClass(), NewGenClassName, RF_Public | RF_Transactional);

			Blueprint->GeneratedClass = NewClass;
			NewClass->ClassGeneratedBy = Blueprint;
			NewClass->SetSuperStruct(Blueprint->ParentClass);
			Blueprint->bHasBeenRegenerated = true;		// Set to true, similar to CreateBlueprint, since we've regerated the class by duplicating it

			// Since we just duplicated the generated class above, we don't need to do a full compile below
			CompileOptions.CompileType = EKismetCompileType::SkeletonOnly;

			TMap<UObject*, UObject*> OldToNewMap;

			UClass* NewBPGCAsClass = Blueprint->GeneratedClass;
			UBlueprintGeneratedClass* NewBPGC = (UBlueprintGeneratedClass*)(NewBPGCAsClass);
			if( SCSRootNode )
			{
				NewBPGC->SimpleConstructionScript = Cast<USimpleConstructionScript>(StaticDuplicateObject(SCSRootNode, NewBPGC, SCSRootNode->GetFName()));
				Blueprint->SimpleConstructionScript = NewBPGC->SimpleConstructionScript;				
				const TArray<USCS_Node*>& AllNodes = NewBPGC->SimpleConstructionScript->GetAllNodes();

				// Duplicate all component templates
				for (USCS_Node* CurrentNode : AllNodes)
				{
					if (CurrentNode && CurrentNode->ComponentTemplate)
					{
						UActorComponent* DuplicatedComponent = CastChecked<UActorComponent>(StaticDuplicateObject(CurrentNode->ComponentTemplate, NewBPGC, CurrentNode->ComponentTemplate->GetFName()));
						OldToNewMap.Add(CurrentNode->ComponentTemplate, DuplicatedComponent);
						CurrentNode->ComponentTemplate = DuplicatedComponent;
					}
				}

				if (USCS_Node* DefaultSceneRootNode = NewBPGC->SimpleConstructionScript->GetDefaultSceneRootNode())
				{
					if (!AllNodes.Contains(DefaultSceneRootNode) && DefaultSceneRootNode->ComponentTemplate)
					{
						UActorComponent* DuplicatedComponent =  Cast<UActorComponent>(OldToNewMap.FindRef(DefaultSceneRootNode->ComponentTemplate));
						if (!DuplicatedComponent)
						{
							DuplicatedComponent = CastChecked<UActorComponent>(StaticDuplicateObject(DefaultSceneRootNode->ComponentTemplate, NewBPGC, DefaultSceneRootNode->ComponentTemplate->GetFName()));
							OldToNewMap.Add(DefaultSceneRootNode->ComponentTemplate, DuplicatedComponent);
						}
						DefaultSceneRootNode->ComponentTemplate = DuplicatedComponent;
					}
				}
			}

			for (UActorComponent* OldComponent : Templates)
			{
				UActorComponent* NewComponent = CastChecked<UActorComponent>(StaticDuplicateObject(OldComponent, NewBPGC, OldComponent->GetFName()));

				NewBPGC->ComponentTemplates.Add(NewComponent);
				OldToNewMap.Add(OldComponent, NewComponent);
			}

			for (UTimelineTemplate* OldTimeline : Timelines)
			{
				UTimelineTemplate* NewTimeline = CastChecked<UTimelineTemplate>(StaticDuplicateObject(OldTimeline, NewBPGC, OldTimeline->GetFName()));

				if (FBlueprintDuplicationScopeFlags::HasAnyFlag(FBlueprintDuplicationScopeFlags::TheSameTimelineGuid))
				{
					NewTimeline->TimelineGuid = OldTimeline->TimelineGuid;
				}

				NewBPGC->Timelines.Add(NewTimeline);
				OldToNewMap.Add(OldTimeline, NewTimeline);
			}

			if (InheritableComponentHandler)
			{
				NewBPGC->InheritableComponentHandler = Cast<UInheritableComponentHandler>(StaticDuplicateObject(InheritableComponentHandler, NewBPGC, InheritableComponentHandler->GetFName()));
				if (NewBPGC->InheritableComponentHandler)
				{
					NewBPGC->InheritableComponentHandler->UpdateOwnerClass(NewBPGC);
				}
			}

			Blueprint->ComponentTemplates = NewBPGC->ComponentTemplates;
			Blueprint->Timelines = NewBPGC->Timelines;
			Blueprint->InheritableComponentHandler = NewBPGC->InheritableComponentHandler;

			Compiler.CompileBlueprint(Blueprint, CompileOptions, Results);

			// Create a new blueprint guid
			Blueprint->GenerateNewGuid();

			// Give all nodes a new Guid
			TArray< UEdGraphNode* > AllGraphNodes;
			GetAllNodesOfClass(Blueprint, AllGraphNodes);
			for(UEdGraphNode* Node : AllGraphNodes)
			{
				if (!FBlueprintDuplicationScopeFlags::HasAnyFlag(FBlueprintDuplicationScopeFlags::TheSameNodeGuid))
				{
					Node->CreateNewGuid();
				}

				// Some variable nodes must be fixed up on duplicate, this cannot wait for individual 
				// node calls to PostDuplicate because it happens after compilation and will still result in errors
				if(UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
				{
					// Self context variable nodes need to be updated with the new Blueprint class
					if(VariableNode->VariableReference.IsSelfContext())
					{
						const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
						if(UEdGraphPin* SelfPin = K2Schema->FindSelfPin(*VariableNode, EGPD_Input))
						{
							UClass* TargetClass = nullptr;

							if(UProperty* Property = VariableNode->VariableReference.ResolveMember<UProperty>(VariableNode->GetBlueprintClassFromNode()))
							{
								TargetClass = Property->GetOwnerClass()->GetAuthoritativeClass();
							}
							else
							{
								TargetClass = Blueprint->SkeletonGeneratedClass->GetAuthoritativeClass();
							}

							SelfPin->PinType.PinSubCategoryObject = TargetClass;
						}
					}
				}
			}

			// Needs a full compile to handle the ArchiveReplaceObjectRef
			CompileOptions.CompileType = EKismetCompileType::Full;
			Compiler.CompileBlueprint(Blueprint, CompileOptions, Results);

			FArchiveReplaceObjectRef<UObject> ReplaceTemplateRefs(NewBPGC, OldToNewMap, /*bNullPrivateRefs=*/ false, /*bIgnoreOuterRef=*/ false, /*bIgnoreArchetypeRef=*/ false);

			// Now propagate the values from the old CDO to the new one
			check(Blueprint->SkeletonGeneratedClass != nullptr);

			UObject* NewCDO = Blueprint->GeneratedClass->GetDefaultObject();
			check(NewCDO != nullptr);
			UEditorEngine::CopyPropertiesForUnrelatedObjects(OldCDO, NewCDO);
		}

		if (!FBlueprintDuplicationScopeFlags::HasAnyFlag(FBlueprintDuplicationScopeFlags::NoExtraCompilation))
		{
			// And compile again to make sure they go into the generated class, get cleaned up, etc...
			FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
		}

		// it can still keeps references to some external objects
		Blueprint->LastEditedDocuments.Empty();
	}

	// Should be no instances of this new blueprint, so no need to replace any
}

void FBlueprintEditorUtils::RemoveGeneratedClasses(UBlueprint* Blueprint)
{
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	Compiler.RemoveBlueprintGeneratedClasses(Blueprint);
}

void FBlueprintEditorUtils::UpdateDelegatesInBlueprint(UBlueprint* Blueprint)
{
	check(Blueprint);
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if(!IsGraphIntermediate(Graph))
		{
			TArray<UK2Node_CreateDelegate*> CreateDelegateNodes;
			Graph->GetNodesOfClass(CreateDelegateNodes);
			for (UK2Node_CreateDelegate* DelegateNode: CreateDelegateNodes)
			{
				DelegateNode->HandleAnyChangeWithoutNotifying();
			}

			TArray<UK2Node_Event*> EventNodes;
			Graph->GetNodesOfClass(EventNodes);
			for (UK2Node_Event* EventNode : EventNodes)
			{
				EventNode->UpdateDelegatePin();
			}

			TArray<UK2Node_Knot*> Knots;
			Graph->GetNodesOfClass(Knots);
			for (UK2Node_Knot* Knot : Knots)
			{
				// Indiscriminate reuse of UK2Node_Knot::PostReconstructNode() is the convention established
				// by UEdGraphSchema_K2::OnPinConnectionDoubleCicked. This forces the pin type data to be
				// refreshed (e.g. due to changes in UpdateDelegatePin())
				Knot->PostReconstructNode();
			}
		}
	}
}

// Blueprint has materially changed.  Recompile the skeleton, notify observers, and mark the package as dirty.
void FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(UBlueprint* Blueprint)
{
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData);
	
	struct FRefreshHelper
	{
		static void SkeletalRecompileChildren(TArray<UClass*> SkelClassesToRecompile, bool bIsCompilingOnLoad)
		{
			FSecondsCounterScope SkeletalRecompileTimer(BlueprintCompileAndLoadTimerData);
			
			for (UClass* SkelClass : SkelClassesToRecompile)
			{
				if (SkelClass->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					continue;
				}

				UBlueprint* SkelBlueprint = Cast<UBlueprint>(SkelClass->ClassGeneratedBy);
				if (SkelBlueprint
					&& SkelBlueprint->Status != BS_BeingCreated
					&& !SkelBlueprint->bBeingCompiled
					&& !SkelBlueprint->bIsRegeneratingOnLoad)
				{
					TArray<UClass*> ChildrenOfClass;
					GetDerivedClasses(SkelClass, ChildrenOfClass, false);

					IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

					FCompilerResultsLog Results;
					Results.bSilentMode = true;
					Results.bLogInfoOnly = true;

					{
						bool const bWasRegenerating = SkelBlueprint->bIsRegeneratingOnLoad;
						SkelBlueprint->bIsRegeneratingOnLoad |= bIsCompilingOnLoad;

						FKismetCompilerOptions CompileOptions;
						CompileOptions.CompileType = EKismetCompileType::SkeletonOnly;
						Compiler.CompileBlueprint(SkelBlueprint, CompileOptions, Results);
						SkelBlueprint->Status = BS_Dirty;

						SkelBlueprint->BroadcastCompiled();

						SkelBlueprint->MarkPackageDirty();

						SkeletalRecompileChildren(ChildrenOfClass, bIsCompilingOnLoad);
						SkelBlueprint->bIsRegeneratingOnLoad = bWasRegenerating;
					}
				}
			}
		}
	};

	// The Blueprint has been structurally modified and this means that some node titles will need to be refreshed
	GetDefault<UEdGraphSchema_K2>()->ForceVisualizationCacheClear();

	Blueprint->bCachedDependenciesUpToDate = false;
	if (Blueprint->Status != BS_BeingCreated && !Blueprint->bBeingCompiled)
	{
		FCompilerResultsLog Results;
		Results.bLogInfoOnly = Blueprint->bIsRegeneratingOnLoad;

		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_MarkBlueprintasStructurallyModified);

		TArray<UClass*> ChildrenOfClass;
		if (UClass* SkelClass = Blueprint->SkeletonGeneratedClass)
		{
			if (!Blueprint->bIsRegeneratingOnLoad)
			{
				if (IsInterfaceBlueprint(Blueprint))
				{
					// Find all dependent Blueprints that implement the interface. Note: Using
					// GetDependentBlueprints() here as the result is cached and thus it should
					// generally be a faster path than iterating through all loaded Blueprints.
					TArray<UBlueprint*> DependentBlueprints;
					GetDependentBlueprints(Blueprint, DependentBlueprints, true);
					for (UBlueprint* DependentBlueprint : DependentBlueprints)
					{
						const bool bDependentBPImplementsInterface = DependentBlueprint->ImplementedInterfaces.ContainsByPredicate([&Blueprint](const FBPInterfaceDescription& InterfaceDesc)
						{
							return InterfaceDesc.Interface == Blueprint->GeneratedClass;
						});

						if (bDependentBPImplementsInterface)
						{
							ChildrenOfClass.Add(DependentBlueprint->SkeletonGeneratedClass);
						}
					}
				}
				else
				{
					GetDerivedClasses(SkelClass, ChildrenOfClass, false);
				}
			}
		}

		{
			// Invoke the compiler to update the skeleton class definition
			IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

			FKismetCompilerOptions CompileOptions;
			CompileOptions.CompileType = EKismetCompileType::SkeletonOnly;
			Compiler.CompileBlueprint(Blueprint, CompileOptions, Results);
		}
		UpdateDelegatesInBlueprint(Blueprint);

		FRefreshHelper::SkeletalRecompileChildren(ChildrenOfClass, Blueprint->bIsRegeneratingOnLoad);

		// Call general modification callback as well
		MarkBlueprintAsModified(Blueprint);

		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_NotifyBlueprintChanged);

			// Notify any interested parties that the blueprint has changed
			Blueprint->BroadcastChanged();
		}
	}
}

// Blueprint has changed in some manner that invalidates the compiled data (link made/broken, default value changed, etc...)
void FBlueprintEditorUtils::MarkBlueprintAsModified(UBlueprint* Blueprint, FPropertyChangedEvent PropertyChangedEvent)
{
	if(Blueprint->bBeingCompiled && GBlueprintUseCompilationManager)
	{
		return;
	}

	Blueprint->bCachedDependenciesUpToDate = false;
	if (Blueprint->Status != BS_BeingCreated)
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for (int32 i = 0; i < AllGraphs.Num(); i++)
		{
			UK2Node_EditablePinBase* EntryNode = GetEntryNode(AllGraphs[i]);
			if(UK2Node_Tunnel* TunnelNode = ExactCast<UK2Node_Tunnel>(EntryNode))
			{
				// Remove data marking graphs as latent, this will be re-cache'd as needed
				TunnelNode->MetaData.HasLatentFunctions = INDEX_NONE;
			}
		}

		// If this was called the CDO was probably modified. Regenerate the post construct property list
		UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
		if (!Blueprint->bBeingCompiled && BPGC)
		{
			BPGC->UpdateCustomPropertyListForPostConstruction();
		}

		Blueprint->Status = BS_Dirty;
		Blueprint->MarkPackageDirty();
		// Previously, PostEditChange() was called on the Blueprint which creates an empty FPropertyChangedEvent. In
		// certain cases, we needed to be able to pass along specific FPropertyChangedEvent that initially triggered
		// this call so that we could keep the Blueprint from refreshing under certain conditions.
		Blueprint->PostEditChangeProperty(PropertyChangedEvent);

		// Clear out the cache as the user may have added or removed a latent action to a macro graph
		FBlueprintEditorUtils::ClearMacroCosmeticInfoCache(Blueprint);
	}
}

bool FBlueprintEditorUtils::ShouldRegenerateBlueprint(UBlueprint* Blueprint)
{
	return !IsCompileOnLoadDisabled(Blueprint)
		&& Blueprint->bRecompileOnLoad
		&& !Blueprint->bIsRegeneratingOnLoad;
}

bool FBlueprintEditorUtils::IsCompileOnLoadDisabled(UBlueprint* Blueprint)
{
	bool bCompilationDisabled = false;
	if(Blueprint->GetLinker())
	{
		bCompilationDisabled = (Blueprint->GetLinker()->LoadFlags & LOAD_DisableCompileOnLoad) != LOAD_None;
	}
	return bCompilationDisabled;
}

// Helper function to get the blueprint that ultimately owns a node.
UBlueprint* FBlueprintEditorUtils::FindBlueprintForNode(const UEdGraphNode* Node)
{
	UEdGraph* Graph = Node ? Cast<UEdGraph>(Node->GetOuter()) : nullptr;
	return FindBlueprintForGraph(Graph);
}

// Helper function to get the blueprint that ultimately owns a node.  Cannot fail.
UBlueprint* FBlueprintEditorUtils::FindBlueprintForNodeChecked(const UEdGraphNode* Node)
{
	return FindBlueprintForGraphChecked(Node->GetGraph());
}


// Helper function to get the blueprint that ultimately owns a graph.
UBlueprint* FBlueprintEditorUtils::FindBlueprintForGraph(const UEdGraph* Graph)
{
	for (UObject* TestOuter = Graph ? Graph->GetOuter() : nullptr; TestOuter; TestOuter = TestOuter->GetOuter())
	{
		if (UBlueprint* Result = Cast<UBlueprint>(TestOuter))
		{
			return Result;
		}
	}

	return nullptr;
}

// Helper function to get the blueprint that ultimately owns a graph.  Cannot fail.
UBlueprint* FBlueprintEditorUtils::FindBlueprintForGraphChecked(const UEdGraph* Graph)
{
	UBlueprint* Result = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	check(Result);
	return Result;
}

UClass* FBlueprintEditorUtils::GetSkeletonClass(UClass* FromClass)
{
	if (FromClass)
	{
		if (UBlueprint* Generator = Cast<UBlueprint>(FromClass->ClassGeneratedBy))
		{
			return Generator->SkeletonGeneratedClass;
		}
	}
	return nullptr;
}

UClass* FBlueprintEditorUtils::GetMostUpToDateClass(UClass* FromClass)
{
	if(!FromClass || FromClass->HasAnyClassFlags(CLASS_Native))
	{
		return FromClass;
	}

	if(GBlueprintUseCompilationManager)
	{
		// It's really not safe/coherent to try and dig out the 'right' class. Things that need the 'most up to date'
		// version of a class should always be looking at the skeleton:
		return GetSkeletonClass(FromClass);
	}
	else
	{
		return FromClass;
	}
}

const UClass* FBlueprintEditorUtils::GetMostUpToDateClass(const UClass* FromClass)
{
	return GetMostUpToDateClass(const_cast<UClass*>(FromClass));
}

bool FBlueprintEditorUtils::PropertyStillExists(UProperty* Property)
{
	if(GBlueprintUseCompilationManager)
	{
		return GetMostUpToDateProperty(Property) != nullptr;
	}

	// We can't reliably know if the property still exists, but assume that it does:
	return true;
}

UProperty* FBlueprintEditorUtils::GetMostUpToDateProperty(UProperty* Property)
{
	if(const UClass* OwningClass = Property->GetTypedOuter<UClass>())
	{
		const UClass* UpToDateClass = GetMostUpToDateClass(OwningClass);
		if (UpToDateClass && UpToDateClass != OwningClass)
		{
			Property = UpToDateClass->FindPropertyByName(Property->GetFName());
		}
	}

	return Property;
}

const UProperty* FBlueprintEditorUtils::GetMostUpToDateProperty(const UProperty* Property)
{
	return GetMostUpToDateProperty(const_cast<UProperty*>(Property));
}

UFunction* FBlueprintEditorUtils::GetMostUpToDateFunction(UFunction* Function)
{
	if(const UClass* OwningClass = Function->GetTypedOuter<UClass>())
	{
		const UClass* UpToDateClass = GetMostUpToDateClass(OwningClass);
		if (UpToDateClass && UpToDateClass != OwningClass)
		{
			Function = UpToDateClass->FindFunctionByName(Function->GetFName());
		}
	}
	return Function;
}

const UFunction* FBlueprintEditorUtils::GetMostUpToDateFunction(const UFunction* Function)
{
	return GetMostUpToDateFunction(const_cast<UFunction*>(Function));
}

bool FBlueprintEditorUtils::IsGraphNameUnique(UBlueprint* Blueprint, const FName& InName)
{
	// Check for any object directly created in the blueprint
	if( !FindObject<UObject>(Blueprint, *InName.ToString()) )
	{
		// Next, check for functions with that name in the blueprint's class scope
		if( !FindField<UField>(Blueprint->SkeletonGeneratedClass, InName) )
		{
			// Finally, check function entry points
			TArray<UK2Node_Event*> AllEvents;
			FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, AllEvents);

			for(int32 i=0; i < AllEvents.Num(); i++)
			{
				UK2Node_Event* EventNode = AllEvents[i];
				check(EventNode);

				if( EventNode->CustomFunctionName == InName
					|| EventNode->EventReference.GetMemberName() == InName )
				{
					return false;
				}
			}

			// All good!
			return true;
		}
	}

	return false;
}

UEdGraph* FBlueprintEditorUtils::CreateNewGraph(UObject* ParentScope, const FName& GraphName, TSubclassOf<class UEdGraph> GraphClass, TSubclassOf<class UEdGraphSchema> SchemaClass)
{
	UEdGraph* NewGraph = nullptr;
	bool bRename = false;

	// Ensure this name isn't already being used for a graph
	if (GraphName != NAME_None)
	{
		UEdGraph* ExistingGraph = FindObject<UEdGraph>(ParentScope, *(GraphName.ToString()));
		ensureMsgf(!ExistingGraph, TEXT("Graph %s already exists: %s"), *GraphName.ToString(), *ExistingGraph->GetFullName());

		// Rename the old graph out of the way; but we have already failed at this point
		if (ExistingGraph)
		{
			ExistingGraph->Rename(nullptr, ExistingGraph->GetOuter(), REN_DoNotDirty | REN_ForceNoResetLoaders);
		}

		// Construct new graph with the supplied name
		NewGraph = NewObject<UEdGraph>(ParentScope, GraphClass, NAME_None, RF_Transactional);
		bRename = true;
	}
	else
	{
		// Construct a new graph with a default name
		NewGraph = NewObject<UEdGraph>(ParentScope, GraphClass, NAME_None, RF_Transactional);
	}

	NewGraph->Schema = SchemaClass;

	// Now move to where we want it to. Workaround to ensure transaction buffer is correctly utilized
	if (bRename)
	{
		NewGraph->Rename(*(GraphName.ToString()), ParentScope, REN_DoNotDirty | REN_ForceNoResetLoaders);
	}
	return NewGraph;
}

UFunction* FBlueprintEditorUtils::FindFunctionInImplementedInterfaces(const UBlueprint* Blueprint, const FName& FunctionName, bool * bOutInvalidInterface, bool bGetAllInterfaces)
{
	if(Blueprint)
	{
		TArray<UClass*> InterfaceClasses;
		FindImplementedInterfaces(Blueprint, bGetAllInterfaces, InterfaceClasses);

		if( bOutInvalidInterface )
		{
			*bOutInvalidInterface = false;
		}

		// Now loop through the interface classes and try and find the function
		for (UClass* SearchClass : InterfaceClasses)
		{
			if( SearchClass )
			{
				// Use the skeleton class if possible, as the generated class may not always be up-to-date (e.g. if the compile state is dirty).
				UBlueprint* InterfaceBlueprint = Cast<UBlueprint>(SearchClass->ClassGeneratedBy);
				if (InterfaceBlueprint && InterfaceBlueprint->SkeletonGeneratedClass)
				{
					SearchClass = InterfaceBlueprint->SkeletonGeneratedClass;
				}

				do
				{
					if( UFunction* OverriddenFunction = SearchClass->FindFunctionByName(FunctionName, EIncludeSuperFlag::ExcludeSuper) )
					{
						return OverriddenFunction;
					}
					SearchClass = SearchClass->GetSuperClass();
				} while (SearchClass);
			}
			else if( bOutInvalidInterface )
			{
				*bOutInvalidInterface = true;
			}
		}
	}

	return nullptr;
}

void FBlueprintEditorUtils::FindImplementedInterfaces(const UBlueprint* Blueprint, bool bGetAllInterfaces, TArray<UClass*>& ImplementedInterfaces)
{
	// First get the ones this blueprint implemented
	for (const FBPInterfaceDescription& ImplementedInterface : Blueprint->ImplementedInterfaces)
	{
		ImplementedInterfaces.AddUnique(ImplementedInterface.Interface);
	}

	if (bGetAllInterfaces)
	{
		// Now get all the ones the blueprint's parents implemented
		UClass* BlueprintParent =  Blueprint->ParentClass;
		while (BlueprintParent)
		{
			for (const FImplementedInterface& ImplementedInterface : BlueprintParent->Interfaces)
			{
				ImplementedInterfaces.AddUnique(ImplementedInterface.Class);
			}
			BlueprintParent = BlueprintParent->GetSuperClass();
		}
	}
}

void FBlueprintEditorUtils::AddMacroGraph( UBlueprint* Blueprint, class UEdGraph* Graph, bool bIsUserCreated, UClass* SignatureFromClass )
{
	// Give the schema a chance to fill out any required nodes (like the entry node or results node)
	const UEdGraphSchema* Schema = Graph->GetSchema();
	const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());

	Schema->CreateDefaultNodesForGraph(*Graph);

	if (K2Schema != nullptr)
	{
		K2Schema->CreateMacroGraphTerminators(*Graph, SignatureFromClass);

		if (bIsUserCreated)
		{
			// We need to flag the entry node to make sure that the compiled function is callable from Kismet2
			K2Schema->AddExtraFunctionFlags(Graph, (FUNC_BlueprintCallable|FUNC_BlueprintEvent));
			K2Schema->MarkFunctionEntryAsEditable(Graph, true);
		}
	}

	// Mark the graph as public if it's going to be referenced directly from other blueprints
	if (Blueprint->BlueprintType == BPTYPE_MacroLibrary)
	{
		Graph->SetFlags(RF_Public);
	}

	Blueprint->MacroGraphs.Add(Graph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void FBlueprintEditorUtils::AddInterfaceGraph(UBlueprint* Blueprint, class UEdGraph* Graph, UClass* InterfaceClass)
{
	const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());
	if (K2Schema != nullptr)
	{
		K2Schema->CreateFunctionGraphTerminators(*Graph, InterfaceClass);
	}
}

void FBlueprintEditorUtils::AddUbergraphPage(UBlueprint* Blueprint, class UEdGraph* Graph)
{
#if WITH_EDITORONLY_DATA
	Blueprint->UbergraphPages.Add(Graph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
#endif	//#if WITH_EDITORONLY_DATA
}

void FBlueprintEditorUtils::AddDomainSpecificGraph(UBlueprint* Blueprint, class UEdGraph* Graph)
{
	// Give the schema a chance to fill out any required nodes (like the entry node or results node)
	const UEdGraphSchema* Schema = Graph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*Graph);

	check(Blueprint->BlueprintType != BPTYPE_MacroLibrary);

#if WITH_EDITORONLY_DATA
	Blueprint->FunctionGraphs.Add(Graph);
#endif	//#if WITH_EDITORONLY_DATA
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

// Remove the supplied set of graphs from the Blueprint.
void FBlueprintEditorUtils::RemoveGraphs( UBlueprint* Blueprint, const TArray<class UEdGraph*>& GraphsToRemove )
{
	for (int32 ItemIndex=0; ItemIndex < GraphsToRemove.Num(); ++ItemIndex)
	{
		UEdGraph* Graph = GraphsToRemove[ItemIndex];
		FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::MarkTransient);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

// Removes the supplied graph from the Blueprint.
void FBlueprintEditorUtils::RemoveGraph(UBlueprint* Blueprint, class UEdGraph* GraphToRemove, EGraphRemoveFlags::Type Flags /*= Transient | Recompile */)
{
	struct Local
	{
		static bool IsASubGraph(UEdGraph* Graph)
		{
			UObject* Outer = Graph->GetOuter();
			return ( Outer && Outer->IsA( UK2Node_Composite::StaticClass() ) );
		}
	};

	GraphToRemove->Modify();

	for (UObject* TestOuter = GraphToRemove->GetOuter(); TestOuter; TestOuter = TestOuter->GetOuter())
	{
		if (TestOuter == Blueprint)
		{
			Blueprint->DelegateSignatureGraphs.Remove( GraphToRemove );
			Blueprint->FunctionGraphs.Remove( GraphToRemove );
			Blueprint->UbergraphPages.Remove( GraphToRemove );

			// Can't just call Remove, the object is wrapped in a struct
			for(int EditedDocIdx = 0; EditedDocIdx < Blueprint->LastEditedDocuments.Num(); ++EditedDocIdx)
			{
				if(Blueprint->LastEditedDocuments[EditedDocIdx].EditedObject == GraphToRemove)
				{
					Blueprint->LastEditedDocuments.RemoveAt(EditedDocIdx);
					break;
				}
			}

			if(Blueprint->MacroGraphs.Remove( GraphToRemove ) > 0 ) 
			{
				//removes all macro nodes using this macro graph
				TArray<UK2Node_MacroInstance*> MacroNodes;
				FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, MacroNodes);
				for (UK2Node_MacroInstance* Node : MacroNodes)
				{
					if(Node->GetMacroGraph() == GraphToRemove)
					{
						FBlueprintEditorUtils::RemoveNode(Blueprint, Node);
					}
				}

				// Clear the cache since it's indexed by graph and one of the graphs is going away
				FBlueprintEditorUtils::ClearMacroCosmeticInfoCache(Blueprint);
			}

			for (FBPInterfaceDescription& CurrInterface : Blueprint->ImplementedInterfaces)
			{
				CurrInterface.Graphs.Remove( GraphToRemove );
			}
		}
		else if (UEdGraph* OuterGraph = Cast<UEdGraph>(TestOuter))
		{
			// remove ourselves
			OuterGraph->Modify();
			OuterGraph->SubGraphs.Remove(GraphToRemove);
		}
		else if (! (Cast<UK2Node_Composite>(TestOuter)	|| 
					Cast<UAnimStateNodeBase>(TestOuter)	||
					Cast<UAnimStateTransitionNode>(TestOuter)	||
					Cast<UAnimGraphNode_StateMachineBase>(TestOuter)) )
		{
			break;
		}
	}

	// Remove timelines held in the graph
	TArray<UK2Node_Timeline*> AllTimelineNodes;
	GraphToRemove->GetNodesOfClass<UK2Node_Timeline>(AllTimelineNodes);
	for (UK2Node_Timeline* TimelineNode : AllTimelineNodes)
	{
		TimelineNode->DestroyNode();
	}

	// Handle subgraphs held in graph
	TArray<UK2Node_Composite*> AllCompositeNodes;
	GraphToRemove->GetNodesOfClass<UK2Node_Composite>(AllCompositeNodes);

	const bool bDontRecompile = true;
	for (UK2Node_Composite* CompNode : AllCompositeNodes)
	{
		if (CompNode->BoundGraph && Local::IsASubGraph(CompNode->BoundGraph))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, CompNode->BoundGraph, EGraphRemoveFlags::None);
		}
	}

	// Animation nodes can contain subgraphs but are not composite nodes, handle their graphs
	TArray<UAnimStateNodeBase*> AllAnimCompositeNodes;
	GraphToRemove->GetNodesOfClassEx<UAnimStateNode>(AllAnimCompositeNodes);
	GraphToRemove->GetNodesOfClassEx<UAnimStateConduitNode>(AllAnimCompositeNodes);
	GraphToRemove->GetNodesOfClassEx<UAnimStateTransitionNode>(AllAnimCompositeNodes);

	for(UAnimStateNodeBase* Node : AllAnimCompositeNodes)
	{
		UEdGraph* BoundGraph = Node->GetBoundGraph();
		if(BoundGraph && BoundGraph->GetOuter()->IsA(UAnimStateNodeBase::StaticClass()))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, BoundGraph, EGraphRemoveFlags::None);
		}
	}

	// Handle sub anim state machines
	TArray<UAnimGraphNode_StateMachineBase*> AllStateMachines;
	GraphToRemove->GetNodesOfClassEx<UAnimGraphNode_StateMachine>(AllStateMachines);

	for(UAnimGraphNode_StateMachineBase* Node : AllStateMachines)
	{
		UEdGraph* BoundGraph = Node->EditorStateMachineGraph;
		if(BoundGraph && BoundGraph->GetOuter()->IsA(UAnimGraphNode_StateMachineBase::StaticClass()))
		{
			FBlueprintEditorUtils::RemoveGraph(Blueprint, BoundGraph, EGraphRemoveFlags::None);
		}
	}

	GraphToRemove->GetSchema()->HandleGraphBeingDeleted(*GraphToRemove);

	GraphToRemove->Rename(nullptr, Blueprint->GetOuter(), REN_DoNotDirty | REN_DontCreateRedirectors);
	GraphToRemove->ClearFlags(RF_Standalone | RF_Public);
	GraphToRemove->RemoveFromRoot();

	if (Flags & EGraphRemoveFlags::MarkTransient)
	{
		GraphToRemove->SetFlags(RF_Transient);
	}

	GraphToRemove->MarkPendingKill();

	if (Flags & EGraphRemoveFlags::Recompile )
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

/** Rename a graph and mark objects for modified */
void FBlueprintEditorUtils::RenameGraph(UEdGraph* Graph, const FString& NewNameStr)
{
	if (Graph && Graph->Rename(*NewNameStr, Graph->GetOuter(), REN_Test))
	{
		// Cache old name
		FName OldGraphName = Graph->GetFName();
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);

		// Ensure we have undo records
		Graph->Modify();
		Graph->Rename(*NewNameStr, Graph->GetOuter(), (Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_DontCreateRedirectors);

		const FName NewName(*NewNameStr);

		// Clean function entry & result nodes if they exist
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				if (EntryNode->SignatureName == OldGraphName)
				{
					EntryNode->Modify();
					EntryNode->SignatureName = NewName;
				}
				else if (EntryNode->CustomGeneratedFunctionName == OldGraphName)
				{
					EntryNode->Modify();
					EntryNode->CustomGeneratedFunctionName = NewName;
				}
			}
			else if(UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
			{
				if (ResultNode->SignatureName == OldGraphName)
				{
					ResultNode->Modify();
					ResultNode->SignatureName = NewName;
				}
			}
		}
	
		// Rename any function call points
		for (TObjectIterator<UK2Node_CallFunction> It(RF_Transient); It; ++It)
		{
			UK2Node_CallFunction* FunctionNode = *It;
			if (FunctionNode && !FunctionNode->HasAnyFlags(RF_Transient) && Cast<UEdGraph>(FunctionNode->GetOuter()))
			{
				if (UBlueprint* FunctionNodeBlueprint = FBlueprintEditorUtils::FindBlueprintForNode(FunctionNode))
				{
					if (FunctionNode->FunctionReference.GetMemberName() == OldGraphName)
					{
						if (FunctionNode->FunctionReference.IsSelfContext())
						{
							FunctionNode->Modify();
							FunctionNode->FunctionReference.SetSelfMember(NewName);
						}
						else if (FunctionNode->FunctionReference.GetMemberParentClass() == Blueprint->GeneratedClass)
						{
							FunctionNode->Modify();
							FunctionNode->FunctionReference.SetExternalMember(NewName, Blueprint->GeneratedClass);
						}
					}
				}
			}
		}

		// Potentially adjust variable names for any child blueprints
		ValidateBlueprintChildVariables(Blueprint, Graph->GetFName());

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

void FBlueprintEditorUtils::RenameGraphWithSuggestion(class UEdGraph* Graph, TSharedPtr<class INameValidatorInterface> NameValidator, const FString& DesiredName )
{
	FString NewName = DesiredName;
	NameValidator->FindValidString(NewName);
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);
	Graph->Rename(*NewName, Graph->GetOuter(), (BP->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_DontCreateRedirectors);
}

/** 
 * Cleans up a Node in the blueprint
 */
void FBlueprintEditorUtils::RemoveNode(UBlueprint* Blueprint, UEdGraphNode* Node, bool bDontRecompile)
{
	check(Node);

	const UEdGraphSchema* Schema = nullptr;

	// Ensure we mark parent graph modified
	if (UEdGraph* GraphObj = Node->GetGraph())
	{
		GraphObj->Modify();
		Schema = GraphObj->GetSchema();
	}

	if (Blueprint != nullptr)
	{
		// Remove any breakpoints set on the node
		if (UBreakpoint* Breakpoint = FKismetDebugUtilities::FindBreakpointForNode(Blueprint, Node))
		{
			FKismetDebugUtilities::StartDeletingBreakpoint(Breakpoint, Blueprint);
		}

		// Remove any watches set on the node's pins
		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
		{
			FKismetDebugUtilities::RemovePinWatch(Blueprint, Node->Pins[PinIndex]);
		}
	}

	Node->Modify();

	// Timelines will be removed from the blueprint if the node is a UK2Node_Timeline
	if (Schema)
	{
		Schema->BreakNodeLinks(*Node);
	}

	Node->DestroyNode(); 

	if (!bDontRecompile && (Blueprint != nullptr))
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

UEdGraph* FBlueprintEditorUtils::GetTopLevelGraph(const UEdGraph* InGraph)
{
	UEdGraph* GraphToTest = const_cast<UEdGraph*>(InGraph);

	for (UObject* TestOuter = GraphToTest; TestOuter; TestOuter = TestOuter->GetOuter())
	{
		// reached up to the blueprint for the graph
		if (UBlueprint* Blueprint = Cast<UBlueprint>(TestOuter))
		{
			break;
		}
		else
		{
			GraphToTest = Cast<UEdGraph>(TestOuter);
		}
	}
	return GraphToTest;
}

bool FBlueprintEditorUtils::IsGraphReadOnly(UEdGraph* InGraph)
{
	bool bGraphReadOnly = true;
	if (InGraph)
	{
		bGraphReadOnly = !InGraph->bEditable;

		if (!bGraphReadOnly)
		{
			const UBlueprint* BlueprintForGraph = FBlueprintEditorUtils::FindBlueprintForGraph(InGraph);
			bool const bIsInterface = ((BlueprintForGraph != nullptr) && (BlueprintForGraph->BlueprintType == BPTYPE_Interface));
			bool const bIsDelegate  = FBlueprintEditorUtils::IsDelegateSignatureGraph(InGraph);
			bool const bIsMathExpression = FBlueprintEditorUtils::IsMathExpressionGraph(InGraph);

			bGraphReadOnly = bIsInterface || bIsDelegate || bIsMathExpression;
		}
	}
	return bGraphReadOnly;
}

UK2Node_Event* FBlueprintEditorUtils::FindOverrideForFunction(const UBlueprint* Blueprint, const UClass* SignatureClass, FName SignatureName)
{
	TArray<UK2Node_Event*> AllEvents;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, AllEvents);

	for(int32 i=0; i<AllEvents.Num(); i++)
	{
		UK2Node_Event* EventNode = AllEvents[i];
		check(EventNode);
		if(	EventNode->bOverrideFunction == true &&
			EventNode->EventReference.GetMemberParentClass(EventNode->GetBlueprintClassFromNode())->IsChildOf(SignatureClass) &&
			EventNode->EventReference.GetMemberName() == SignatureName )
		{
			return EventNode;
		}
	}

	return nullptr;
}

UK2Node_Event* FBlueprintEditorUtils::FindCustomEventNode(const UBlueprint* Blueprint, FName const CustomName)
{
	UK2Node_Event* FoundNode = nullptr;

	if (CustomName != NAME_None)
	{
		TArray<UK2Node_Event*> AllEvents;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, AllEvents);

		for (UK2Node_Event* EventNode : AllEvents)
		{
			if (EventNode->CustomFunctionName == CustomName)
			{
				FoundNode = EventNode;
				break;
			}
		}
	}
	return FoundNode;
}


void FBlueprintEditorUtils::GatherDependencies(const UBlueprint* InBlueprint, TSet<TWeakObjectPtr<UBlueprint>>& Dependencies, TSet<TWeakObjectPtr<UStruct>>& OutUDSDependencies)
{
	struct FGatherDependenciesHelper
	{
		static UBlueprint* GetGeneratingBlueprint(const UObject* Obj)
		{
			const UBlueprintGeneratedClass* BPGC = nullptr;
			while (!BPGC && Obj)
			{
				BPGC = Cast<const UBlueprintGeneratedClass>(Obj);
				Obj = Obj->GetOuter();
			}

			return UBlueprint::GetBlueprintFromClass(BPGC);
		}

		static void ProcessHierarchy(const UStruct* Struct, TSet<TWeakObjectPtr<UBlueprint>>& InDependencies)
		{
			for (UBlueprint* Blueprint = GetGeneratingBlueprint(Struct);
				Blueprint;
				Blueprint = UBlueprint::GetBlueprintFromClass(Cast<UBlueprintGeneratedClass>(Blueprint->ParentClass)))
			{
				bool bAlreadyProcessed = false;
				InDependencies.Add(Blueprint, &bAlreadyProcessed);
				if (bAlreadyProcessed)
				{
					return;
				}

				Blueprint->GatherDependencies(InDependencies);
			}
		}
	};

	check(InBlueprint);
	Dependencies.Empty();
	OutUDSDependencies.Empty();

	// If the Blueprint's GeneratedClass was not generated by the Blueprint, it's either corrupt or a PIE version of the BP
	if (InBlueprint->GeneratedClass && InBlueprint->GeneratedClass->ClassGeneratedBy != InBlueprint)
	{
		// Dependencies do not matter for PIE duplicated Blueprints
		return;
	}

	InBlueprint->GatherDependencies(Dependencies);

	FGatherDependenciesHelper::ProcessHierarchy(InBlueprint->ParentClass, Dependencies);

	for (const FBPInterfaceDescription& InterfaceDesc : InBlueprint->ImplementedInterfaces)
	{
		UBlueprint* InterfaceBP = InterfaceDesc.Interface ? Cast<UBlueprint>(InterfaceDesc.Interface->ClassGeneratedBy) : nullptr;
		if (InterfaceBP)
		{
			Dependencies.Add(InterfaceBP);
		}
	}

	TArray<UEdGraph*> Graphs;
	InBlueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && !FBlueprintEditorUtils::IsGraphIntermediate(Graph))
		{
			TArray<UK2Node*> Nodes;
			Graph->GetNodesOfClass(Nodes);
			for (UK2Node* Node : Nodes)
			{
				TArray<UStruct*> LocalDependentStructures;
				if (Node && Node->HasExternalDependencies(&LocalDependentStructures))
				{
					for (UStruct* Struct : LocalDependentStructures)
					{
						if (UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(Struct))
						{
							OutUDSDependencies.Add(UDS);
						}
						else
						{
							FGatherDependenciesHelper::ProcessHierarchy(Struct, Dependencies);
						}
					}
				}
			}
		}
	}

	Dependencies.Remove(InBlueprint);
}

void FBlueprintEditorUtils::EnsureCachedDependenciesUpToDate(UBlueprint* Blueprint)
{
	if (Blueprint && !Blueprint->bCachedDependenciesUpToDate)
	{
		GatherDependencies(Blueprint, Blueprint->CachedDependencies, Blueprint->CachedUDSDependencies);
		Blueprint->bCachedDependenciesUpToDate = true;
	}
}

void FBlueprintEditorUtils::GetDependentBlueprints(UBlueprint* Blueprint, TArray<UBlueprint*>& DependentBlueprints, bool bRemoveSelf)
{
	TArray<UObject*> AllBlueprints;
	bool const bIncludeDerivedClasses = true;
	GetObjectsOfClass(UBlueprint::StaticClass(), AllBlueprints, bIncludeDerivedClasses );

	for (UObject* Obj : AllBlueprints)
	{
		// we know the class is correct so a fast cast is ok here
		UBlueprint* TestBP = static_cast<UBlueprint*>(Obj);
		if (TestBP && !TestBP->IsPendingKill())
		{
			EnsureCachedDependenciesUpToDate(TestBP);

			if (TestBP->CachedDependencies.Contains(Blueprint))
			{
				if (!DependentBlueprints.Contains(TestBP))
				{
					DependentBlueprints.Add(TestBP);

					// When a Macro Library depends on this Blueprint, then any Blueprint that 
					// depends on it must also depend on this Blueprint for re-compiling (bytecode, skeleton, full) purposes
					if (TestBP->BlueprintType == BPTYPE_MacroLibrary)
					{
						GetDependentBlueprints(TestBP, DependentBlueprints, false);
					}
				}
			}
		}
	}

	if (bRemoveSelf)
	{
		DependentBlueprints.RemoveSwap(Blueprint);
	}
}


bool FBlueprintEditorUtils::IsGraphIntermediate(const UEdGraph* Graph)
{
	if (Graph)
	{
		return Graph->HasAllFlags(RF_Transient);
	}
	return false;
}

bool FBlueprintEditorUtils::IsDataOnlyBlueprint(const UBlueprint* Blueprint)
{
	// Blueprint interfaces are always compiled
	if (Blueprint->BlueprintType == BPTYPE_Interface)
	{
		return false;
	}

	if (Blueprint->AlwaysCompileOnLoad())
	{
		return false;
	}

	// Note that the current implementation of IsChildOf will not crash when called on a nullptr, but
	// I'm explicitly null checking because it seems unwise to rely on this behavior:
	if (Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return false;
	}

	// No new variables defined
	if (Blueprint->NewVariables.Num() > 0)
	{
		return false;
	}
	
	// No extra functions, other than the user construction script(only AActor and subclasses of AActor have)
	const int32 DefaultFunctionNum = (Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(AActor::StaticClass())) ? 1 : 0;
	if ((Blueprint->FunctionGraphs.Num() > DefaultFunctionNum) || (Blueprint->MacroGraphs.Num() > 0))
	{
		return false;
	}

	if (Blueprint->DelegateSignatureGraphs.Num())
	{
		return false;
	}

	if (Blueprint->ComponentTemplates.Num() || Blueprint->Timelines.Num())
	{
		return false;
	}

	if (USimpleConstructionScript* SimpleConstructionScript = Blueprint->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& Nodes = SimpleConstructionScript->GetAllNodes();
		if (Nodes.Num() > 1)
		{
			return false;
		}
		if ((1 == Nodes.Num()) && (Nodes[0] != SimpleConstructionScript->GetDefaultSceneRootNode()))
		{
			return false;
		}
	}

	// Make sure there's nothing in the user construction script, other than an entry node
	UEdGraph* UserConstructionScript = (Blueprint->FunctionGraphs.Num() == 1) ? Blueprint->FunctionGraphs[0] : nullptr;
	if (UserConstructionScript && Blueprint->ParentClass)
	{
		//Call parent construction script may be added automatically
		UBlueprint* BlueprintParent = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
		// just 1 entry node or just one entry node and a call to our super, which is DataOnly:
		if ( !BlueprintParent && UserConstructionScript->Nodes.Num() > 1 )
		{
			return false;
		}
		else if (BlueprintParent)
		{
			// More than two nodes.. one of them must do something (same logic as above, but we have a call to super as well)
			if (UserConstructionScript->Nodes.Num() > 2)
			{
				return false;
			}
			else
			{
				// Just make sure the nodes are trivial, if they aren't then we're not data only:
				for (UEdGraphNode* Node : UserConstructionScript->Nodes)
				{
					if (!Cast<UK2Node_FunctionEntry>(Node) &&
						!Cast<UK2Node_CallParentFunction>(Node))
					{
						return false;
					}
				}
			}
		}
	}

	// All EventGraphs are empty (at least of non-ghost, non-disabled nodes)
	for (UEdGraph* EventGraph : Blueprint->UbergraphPages)
	{
		for (UEdGraphNode* GraphNode : EventGraph->Nodes)
		{
			// If there is an enabled node in the event graph, the Blueprint is not data only
			if (GraphNode && (GraphNode->GetDesiredEnabledState() != ENodeEnabledState::Disabled))
			{
				return false;
			}
		}
	}

	// No implemented interfaces
	if (Blueprint->ImplementedInterfaces.Num() > 0)
	{
		return false;
	}

	return true;
}

bool FBlueprintEditorUtils::IsBlueprintConst(const UBlueprint* Blueprint)
{
	// Macros aren't marked as const because they can modify variables when instanced into a non const class
	// and will be caught at compile time if they're modifying variables on a const class.
	return Blueprint->BlueprintType == BPTYPE_Const;
}

bool FBlueprintEditorUtils::IsBlutility(const UBlueprint* Blueprint)
{
	IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility");

	if (BlutilityModule)
	{
		return BlutilityModule->IsBlutility( Blueprint );
	}
	return false;
}

bool FBlueprintEditorUtils::IsActorBased(const UBlueprint* Blueprint)
{
	return Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(AActor::StaticClass());
}

bool FBlueprintEditorUtils::IsDelegateSignatureGraph(const UEdGraph* Graph)
{
	if(Graph)
	{
		if(const UBlueprint* Blueprint = FindBlueprintForGraph(Graph))
		{
			return (nullptr != Blueprint->DelegateSignatureGraphs.FindByKey(Graph));
		}
	}
	return false;
}

bool FBlueprintEditorUtils::IsMathExpressionGraph(const UEdGraph* InGraph)
{
	if(InGraph)
	{
		return InGraph->GetOuter()->GetClass() == UK2Node_MathExpression::StaticClass();
	}
	return false;
}

bool FBlueprintEditorUtils::IsInterfaceBlueprint(const UBlueprint* Blueprint)
{
	return (Blueprint->BlueprintType == BPTYPE_Interface);
}

bool FBlueprintEditorUtils::IsLevelScriptBlueprint(const UBlueprint* Blueprint)
{
	return (Blueprint->BlueprintType == BPTYPE_LevelScript);
}

bool FBlueprintEditorUtils::IsAnonymousBlueprintClass(const UClass* Class)
{
	return (Class->GetOutermost()->ContainsMap());
}

ULevel* FBlueprintEditorUtils::GetLevelFromBlueprint(const UBlueprint* Blueprint)
{
	return Cast<ULevel>(Blueprint->GetOuter());
}

bool FBlueprintEditorUtils::SupportsConstructionScript(const UBlueprint* Blueprint)
{
	return(	!FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint) && 
			!FBlueprintEditorUtils::IsBlueprintConst(Blueprint) && 
			!FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint) && 
			FBlueprintEditorUtils::IsActorBased(Blueprint)) &&
			!(Blueprint->BlueprintType == BPTYPE_MacroLibrary) &&
			!(Blueprint->BlueprintType == BPTYPE_FunctionLibrary);
}

bool FBlueprintEditorUtils::CanClassGenerateEvents(const UClass* InClass)
{
	if( InClass )
	{
		for( TFieldIterator<UMulticastDelegateProperty> PropertyIt( InClass, EFieldIteratorFlags::IncludeSuper ); PropertyIt; ++PropertyIt )
		{
			UProperty* Property = *PropertyIt;
			if( !Property->HasAnyPropertyFlags( CPF_Parm ) && Property->HasAllPropertyFlags( CPF_BlueprintAssignable ))
			{
				return true;
			}
		}
	}
	return false;
}

UEdGraph* FBlueprintEditorUtils::FindUserConstructionScript(const UBlueprint* Blueprint)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	for (UEdGraph* CurrentGraph : Blueprint->FunctionGraphs)
	{
		if( CurrentGraph->GetFName() == Schema->FN_UserConstructionScript )
		{
			return CurrentGraph;
		}
	}

	return nullptr;
}

UEdGraph* FBlueprintEditorUtils::FindEventGraph(const UBlueprint* Blueprint)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	for (UEdGraph* CurrentGraph : Blueprint->UbergraphPages)
	{
		if( CurrentGraph->GetFName() == Schema->GN_EventGraph )
		{
			return CurrentGraph;
		}
	}

	return nullptr;
}

bool FBlueprintEditorUtils::IsEventGraph(const UEdGraph* InGraph)
{
	if (InGraph)
	{
		if (const UBlueprint* Blueprint = FindBlueprintForGraph(InGraph))
		{
			return (nullptr != Blueprint->UbergraphPages.FindByKey(InGraph));
		}
	}
	return false;
}

bool FBlueprintEditorUtils::IsTunnelInstanceNode(const UEdGraphNode* InGraphNode)
{
	if (InGraphNode)
	{
		return InGraphNode->IsA<UK2Node_MacroInstance>() || InGraphNode->IsA<UK2Node_Composite>();
	}
	return false;
}

bool FBlueprintEditorUtils::DoesBlueprintDeriveFrom(const UBlueprint* Blueprint, UClass* TestClass)
{
	check(Blueprint->SkeletonGeneratedClass != nullptr);
	return	TestClass != nullptr && 
		Blueprint->SkeletonGeneratedClass->IsChildOf(TestClass);
}

bool FBlueprintEditorUtils::DoesBlueprintContainField(const UBlueprint* Blueprint, UField* TestField)
{
	// Get the class of the field
	if(TestField)
	{
		// Local properties do not have a UClass outer but are also not a part of the Blueprint
		UClass* TestClass = Cast<UClass>(TestField->GetOuter());
		if(TestClass)
		{
			return FBlueprintEditorUtils::DoesBlueprintDeriveFrom(Blueprint, TestClass);
		}
	}
	return false;
}

bool FBlueprintEditorUtils::DoesSupportOverridingFunctions(const UBlueprint* Blueprint)
{
	return Blueprint->BlueprintType != BPTYPE_MacroLibrary 
		&& Blueprint->BlueprintType != BPTYPE_Interface
		&& Blueprint->BlueprintType != BPTYPE_FunctionLibrary;
}

bool FBlueprintEditorUtils::DoesSupportTimelines(const UBlueprint* Blueprint)
{
	// Right now, just assume actor based blueprints support timelines
	return FBlueprintEditorUtils::IsActorBased(Blueprint) && FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
}

bool FBlueprintEditorUtils::DoesSupportEventGraphs(const UBlueprint* Blueprint)
{
	return Blueprint->BlueprintType == BPTYPE_Normal 
		|| Blueprint->BlueprintType == BPTYPE_LevelScript;
}

/** Returns whether or not the blueprint supports implementing interfaces */
bool FBlueprintEditorUtils::DoesSupportImplementingInterfaces(const UBlueprint* Blueprint)
{
	return Blueprint->BlueprintType != BPTYPE_MacroLibrary
		&& Blueprint->BlueprintType != BPTYPE_Interface
		&& Blueprint->BlueprintType != BPTYPE_LevelScript
		&& Blueprint->BlueprintType != BPTYPE_FunctionLibrary;
}

bool FBlueprintEditorUtils::DoesSupportComponents(UBlueprint const* Blueprint)
{
	return (Blueprint->SimpleConstructionScript != nullptr)      // An SCS must be present (otherwise there is nothing valid to edit)
		&& FBlueprintEditorUtils::IsActorBased(Blueprint)        // Must be parented to an AActor-derived class (some older BPs may have an SCS but may not be Actor-based)
		&& (Blueprint->BlueprintType != BPTYPE_MacroLibrary)     // Must not be a macro-type Blueprint
		&& (Blueprint->BlueprintType != BPTYPE_FunctionLibrary); // Must not be a function library
}

bool FBlueprintEditorUtils::DoesSupportDefaults(UBlueprint const* Blueprint)
{
	return Blueprint->BlueprintType != BPTYPE_MacroLibrary
		&& Blueprint->BlueprintType != BPTYPE_FunctionLibrary;
}

bool FBlueprintEditorUtils::DoesSupportLocalVariables(UEdGraph const* InGraph)
{
	if(InGraph)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(InGraph);
		return Blueprint
			&& Blueprint->BlueprintType != BPTYPE_Interface
			&& InGraph->GetSchema()->GetGraphType(InGraph) == EGraphType::GT_Function
			&& !InGraph->IsA(UAnimationTransitionGraph::StaticClass());
	}
	return false;
}

// Returns a descriptive name of the type of blueprint passed in
FString FBlueprintEditorUtils::GetBlueprintTypeDescription(const UBlueprint* Blueprint)
{
	FString BlueprintTypeString;
	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_LevelScript:
		BlueprintTypeString = LOCTEXT("BlueprintType_LevelScript", "Level Blueprint").ToString();
		break;
	case BPTYPE_MacroLibrary:
		BlueprintTypeString = LOCTEXT("BlueprintType_MacroLibrary", "Macro Library").ToString();
		break;
	case BPTYPE_Interface:
		BlueprintTypeString = LOCTEXT("BlueprintType_Interface", "Interface").ToString();
		break;
	case BPTYPE_FunctionLibrary:
	case BPTYPE_Normal:
	case BPTYPE_Const:
		BlueprintTypeString = Blueprint->GetClass()->GetName();
		break;
	default:
		BlueprintTypeString = TEXT("Unknown blueprint type");
	}

	return BlueprintTypeString;
}

//////////////////////////////////////////////////////////////////////////
// Variables

bool FBlueprintEditorUtils::IsVariableCreatedByBlueprint(UBlueprint* InBlueprint, UProperty* InVariableProperty)
{
	bool bIsVariableCreatedByBlueprint = false;
	if (UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(InVariableProperty->GetOwnerClass()))
	{
		UBlueprint* OwnerBlueprint = Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy);
		bIsVariableCreatedByBlueprint = (OwnerBlueprint == InBlueprint && FBlueprintEditorUtils::FindNewVariableIndex(InBlueprint, InVariableProperty->GetFName()) != INDEX_NONE);
	}
	return bIsVariableCreatedByBlueprint;
}

// Find the index of a variable first declared in this blueprint. Returns INDEX_NONE if not found.
int32 FBlueprintEditorUtils::FindNewVariableIndex(const UBlueprint* Blueprint, const FName& InName) 
{
	if(InName != NAME_None)
	{
		for(int32 i=0; i<Blueprint->NewVariables.Num(); i++)
		{
			if(Blueprint->NewVariables[i].VarName == InName)
			{
				return i;
			}
		}
	}

	return INDEX_NONE;
}

bool FBlueprintEditorUtils::MoveVariableBeforeVariable(UBlueprint* Blueprint, FName VarNameToMove, FName TargetVarName, bool bDontRecompile)
{
	bool bMoved = false;
	int32 VarIndexToMove = FindNewVariableIndex(Blueprint, VarNameToMove);
	int32 TargetVarIndex = FindNewVariableIndex(Blueprint, TargetVarName);
	if(VarIndexToMove != INDEX_NONE && TargetVarIndex != INDEX_NONE)
	{
		// Copy var we want to move
		FBPVariableDescription MoveVar = Blueprint->NewVariables[VarIndexToMove];
		// When we remove item, will back all items after it. If your target is after it, need to adjust
		if(TargetVarIndex > VarIndexToMove)
		{
			TargetVarIndex--;
		}
		// Remove var we are moving
		Blueprint->NewVariables.RemoveAt(VarIndexToMove);
		// Add in before target variable
		Blueprint->NewVariables.Insert(MoveVar, TargetVarIndex);

		if(!bDontRecompile)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		bMoved = true;
	}
	return bMoved;
}

int32 FBlueprintEditorUtils::FindTimelineIndex(const UBlueprint* Blueprint, const FName& InName) 
{
	const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(InName);
	for(int32 i=0; i<Blueprint->Timelines.Num(); i++)
	{
		if(Blueprint->Timelines[i]->GetFName() == TimelineTemplateName)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void FBlueprintEditorUtils::GetSCSVariableNameList(const UBlueprint* Blueprint, TSet<FName>& VariableNames)
{
	if(Blueprint != nullptr && Blueprint->SimpleConstructionScript != nullptr)
	{
		for (USCS_Node* SCS_Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (SCS_Node != nullptr)
			{
				const FName VariableName = SCS_Node->GetVariableName();
				if(VariableName != NAME_None)
				{
					VariableNames.Add(VariableName);
				}
			}
		}
	}
}

void FBlueprintEditorUtils::GetImplementingBlueprintsFunctionNameList(const UBlueprint* Blueprint, TSet<FName>& FunctionNames)
{
	if(Blueprint != nullptr && FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint))
	{
		for(TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
		{
			const UBlueprint* ChildBlueprint = *BlueprintIt;
			if(ChildBlueprint != nullptr)
			{
				for (int32 InterfaceIndex = 0; InterfaceIndex < ChildBlueprint->ImplementedInterfaces.Num(); InterfaceIndex++)
				{
					const FBPInterfaceDescription& CurrentInterface = ChildBlueprint->ImplementedInterfaces[InterfaceIndex];
					const UBlueprint* BlueprintInterfaceClass = UBlueprint::GetBlueprintFromClass(CurrentInterface.Interface);
					if(BlueprintInterfaceClass != nullptr && BlueprintInterfaceClass == Blueprint)
					{
						FBlueprintEditorUtils::GetAllGraphNames(ChildBlueprint, FunctionNames);
					}
				}
			}
		}
	}
}

int32 FBlueprintEditorUtils::FindSCS_Node(const UBlueprint* Blueprint, const FName& InName) 
{
	if (Blueprint->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& AllSCS_Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();
	
		for(int32 i=0; i<AllSCS_Nodes.Num(); i++)
		{
			if(AllSCS_Nodes[i]->GetVariableName() == InName)
			{
				return i;
			}
		}
	}

	return INDEX_NONE;
}

void FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(UBlueprint* Blueprint, const FName& VarName, const bool bNewBlueprintOnly)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);

	if (bNewBlueprintOnly)
	{
		FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(Blueprint, VarName, nullptr, FEdMode::MD_MakeEditWidget);
	}

	if (VarIndex != INDEX_NONE)
	{
		if( bNewBlueprintOnly )
		{
			Blueprint->NewVariables[VarIndex].PropertyFlags |= CPF_DisableEditOnInstance;
		}
		else
		{
			Blueprint->NewVariables[VarIndex].PropertyFlags &= ~CPF_DisableEditOnInstance;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void FBlueprintEditorUtils::SetBlueprintPropertyReadOnlyFlag(UBlueprint* Blueprint, const FName& VarName, const bool bVariableReadOnly)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);

	if (bVariableReadOnly)
	{
		FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(Blueprint, VarName, nullptr, FEdMode::MD_MakeEditWidget);
	}

	if (VarIndex != INDEX_NONE)
	{
		if (bVariableReadOnly)
		{
			Blueprint->NewVariables[VarIndex].PropertyFlags |= CPF_BlueprintReadOnly;
		}
		else
		{
			Blueprint->NewVariables[VarIndex].PropertyFlags &= ~CPF_BlueprintReadOnly;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void FBlueprintEditorUtils::SetInterpFlag(UBlueprint* Blueprint, const FName& VarName, const bool bInterp)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
	if (VarIndex != INDEX_NONE)
	{
		if( bInterp )
		{
			Blueprint->NewVariables[VarIndex].PropertyFlags |= (CPF_Interp);
		}
		else
		{
			Blueprint->NewVariables[VarIndex].PropertyFlags &= ~(CPF_Interp);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void FBlueprintEditorUtils::SetVariableTransientFlag(UBlueprint* InBlueprint, const FName& InVarName, const bool bInIsTransient)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(InBlueprint, InVarName);

	if (VarIndex != INDEX_NONE)
	{
		if( bInIsTransient )
		{
			InBlueprint->NewVariables[VarIndex].PropertyFlags |= CPF_Transient;
		}
		else
		{
			InBlueprint->NewVariables[VarIndex].PropertyFlags &= ~CPF_Transient;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
}

void FBlueprintEditorUtils::SetVariableSaveGameFlag(UBlueprint* InBlueprint, const FName& InVarName, const bool bInIsSaveGame)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(InBlueprint, InVarName);

	if (VarIndex != INDEX_NONE)
	{
		if( bInIsSaveGame )
		{
			InBlueprint->NewVariables[VarIndex].PropertyFlags |= CPF_SaveGame;
		}
		else
		{
			InBlueprint->NewVariables[VarIndex].PropertyFlags &= ~CPF_SaveGame;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
}

void FBlueprintEditorUtils::SetVariableAdvancedDisplayFlag(UBlueprint* InBlueprint, const FName& InVarName, const bool bInIsAdvancedDisplay)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(InBlueprint, InVarName);

	if (VarIndex != INDEX_NONE)
	{
		if( bInIsAdvancedDisplay )
		{
			InBlueprint->NewVariables[VarIndex].PropertyFlags |= CPF_AdvancedDisplay;
		}
		else
		{
			InBlueprint->NewVariables[VarIndex].PropertyFlags &= ~CPF_AdvancedDisplay;
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
}

struct FMetaDataDependencyHelper
{
	static void OnChange(UBlueprint* Blueprint, FName MetaDataKey)
	{
		if (Blueprint && (FBlueprintMetadata::MD_ExposeOnSpawn == MetaDataKey))
		{
			TArray<UEdGraph*> AllGraphs;
			Blueprint->GetAllGraphs(AllGraphs);
			for (UEdGraph* Graph : AllGraphs)
			{
				if (Graph)
				{
					const UEdGraphSchema* Schema = Graph->GetSchema();
					TArray<UK2Node_SpawnActorFromClass*> LocalSpawnNodes;
					Graph->GetNodesOfClass(LocalSpawnNodes);
					for (UK2Node_SpawnActorFromClass* Node : LocalSpawnNodes)
					{
						UClass* ClassToSpawn = Node ? Node->GetClassToSpawn() : nullptr;
						if (ClassToSpawn && ClassToSpawn->IsChildOf(Blueprint->GeneratedClass))
						{
							Schema->ReconstructNode(*Node, true);
						}
					}
				}
			}
		}
	}
};

void FBlueprintEditorUtils::SetBlueprintVariableMetaData(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FName& MetaDataKey, const FString& MetaDataValue)
{
	// If there is a local var scope, we know we are looking at a local variable
	if(InLocalVarScope)
	{
		if(FBPVariableDescription* LocalVariable = FindLocalVariable(Blueprint, InLocalVarScope, VarName))
		{
			LocalVariable->SetMetaData(MetaDataKey, *MetaDataValue);
		}
	}
	else
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
		if (VarIndex == INDEX_NONE)
		{
			//Not a NewVariable is the VarName from a Timeline?
			const int32 TimelineIndex = FBlueprintEditorUtils::FindTimelineIndex(Blueprint,VarName);

			if (TimelineIndex == INDEX_NONE)
			{
				//Not a Timeline is this a SCS Node?
				const int32 SCS_NodeIndex = FBlueprintEditorUtils::FindSCS_Node(Blueprint,VarName);

				if (SCS_NodeIndex != INDEX_NONE)
				{
					Blueprint->SimpleConstructionScript->GetAllNodes()[SCS_NodeIndex]->SetMetaData(MetaDataKey, MetaDataValue);
				}

			}
			else
			{
				Blueprint->Timelines[TimelineIndex]->SetMetaData(MetaDataKey, MetaDataValue);
			}
		}
		else
		{
			Blueprint->NewVariables[VarIndex].SetMetaData(MetaDataKey, MetaDataValue);
			UProperty* Property = FindField<UProperty>(Blueprint->SkeletonGeneratedClass, VarName);
			if (Property)
			{
				Property->SetMetaData(MetaDataKey, *MetaDataValue);
			}
			Property = FindField<UProperty>(Blueprint->GeneratedClass, VarName);
			if (Property)
			{
				Property->SetMetaData(MetaDataKey, *MetaDataValue);
			}
		}
	}

	FMetaDataDependencyHelper::OnChange(Blueprint, MetaDataKey);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

bool FBlueprintEditorUtils::GetBlueprintVariableMetaData(const UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FName& MetaDataKey, FString& OutMetaDataValue)
{
	// If there is a local var scope, we know we are looking at a local variable
	if(InLocalVarScope)
	{
		if(FBPVariableDescription* LocalVariable = FindLocalVariable(Blueprint, InLocalVarScope, VarName))
		{
			int32 EntryIndex = LocalVariable->FindMetaDataEntryIndexForKey(MetaDataKey);
			if (EntryIndex != INDEX_NONE)
			{
				OutMetaDataValue = LocalVariable->GetMetaData(MetaDataKey);
				return true;
			}
		}
	}
	else
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
		if (VarIndex == INDEX_NONE)
		{
			//Not a NewVariable is the VarName from a Timeline?
			const int32 TimelineIndex = FBlueprintEditorUtils::FindTimelineIndex(Blueprint,VarName);

			if (TimelineIndex == INDEX_NONE)
			{
				//Not a Timeline is this a SCS Node?
				const int32 SCS_NodeIndex = FBlueprintEditorUtils::FindSCS_Node(Blueprint,VarName);

				if (SCS_NodeIndex != INDEX_NONE)
				{
					USCS_Node& Desc = *Blueprint->SimpleConstructionScript->GetAllNodes()[SCS_NodeIndex];

					int32 EntryIndex = Desc.FindMetaDataEntryIndexForKey(MetaDataKey);
					if (EntryIndex != INDEX_NONE)
					{
						OutMetaDataValue = Desc.GetMetaData(MetaDataKey);
						return true;
					}
				}
			}
			else
			{
				UTimelineTemplate& Desc = *Blueprint->Timelines[TimelineIndex];

				int32 EntryIndex = Desc.FindMetaDataEntryIndexForKey(MetaDataKey);
				if (EntryIndex != INDEX_NONE)
				{
					OutMetaDataValue = Desc.GetMetaData(MetaDataKey);
					return true;
				}
			}
		}
		else
		{
			const FBPVariableDescription& Desc = Blueprint->NewVariables[VarIndex];

			int32 EntryIndex = Desc.FindMetaDataEntryIndexForKey(MetaDataKey);
			if (EntryIndex != INDEX_NONE)
			{
				OutMetaDataValue = Desc.GetMetaData(MetaDataKey);
				return true;
			}
		}
	}

	OutMetaDataValue.Empty();
	return false;
}

void FBlueprintEditorUtils::RemoveBlueprintVariableMetaData(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FName& MetaDataKey)
{
	// If there is a local var scope, we know we are looking at a local variable
	if(InLocalVarScope)
	{
		if(FBPVariableDescription* LocalVariable = FindLocalVariable(Blueprint, InLocalVarScope, VarName))
		{
			LocalVariable->RemoveMetaData(MetaDataKey);
		}
	}
	else
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
		if (VarIndex == INDEX_NONE)
		{
			//Not a NewVariable is the VarName from a Timeline?
			const int32 TimelineIndex = FBlueprintEditorUtils::FindTimelineIndex(Blueprint,VarName);

			if (TimelineIndex == INDEX_NONE)
			{
				//Not a Timeline is this a SCS Node?
				const int32 SCS_NodeIndex = FBlueprintEditorUtils::FindSCS_Node(Blueprint,VarName);

				if (SCS_NodeIndex != INDEX_NONE)
				{
					Blueprint->SimpleConstructionScript->GetAllNodes()[SCS_NodeIndex]->RemoveMetaData(MetaDataKey);
				}

			}
			else
			{
				Blueprint->Timelines[TimelineIndex]->RemoveMetaData(MetaDataKey);
			}
		}
		else
		{
			Blueprint->NewVariables[VarIndex].RemoveMetaData(MetaDataKey);
			UProperty* Property = FindField<UProperty>(Blueprint->SkeletonGeneratedClass, VarName);
			if (Property)
			{
				Property->RemoveMetaData(MetaDataKey);
			}
			Property = FindField<UProperty>(Blueprint->GeneratedClass, VarName);
			if (Property)
			{
				Property->RemoveMetaData(MetaDataKey);
			}
		}
	}

	FMetaDataDependencyHelper::OnChange(Blueprint, MetaDataKey);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void FBlueprintEditorUtils::SetBlueprintVariableCategory(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FText& NewCategory, bool bDontRecompile)
{
	if (Blueprint == nullptr)
	{
		return;
	}

	// Ensure we always set a category
	FText SetCategory = NewCategory;
	if (SetCategory.IsEmpty())
	{
		SetCategory = UEdGraphSchema_K2::VR_DefaultCategory;
	}
	
	const FText OldCategory = GetBlueprintVariableCategory(Blueprint, VarName, InLocalVarScope);
	if (OldCategory.EqualTo(SetCategory))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ChangeVariableCategory", "Change Variable Category"));
	Blueprint->Modify();

	UClass* SkeletonGeneratedClass = Blueprint->SkeletonGeneratedClass;
	if (UProperty* TargetProperty = FindField<UProperty>(SkeletonGeneratedClass, VarName))
	{
		UClass* OuterClass = CastChecked<UClass>(TargetProperty->GetOuter());
		const bool bIsNativeVar = (OuterClass->ClassGeneratedBy == nullptr);

		// If the category does not change, we will not recompile the Blueprint
		bool bIsCategoryChanged = false;
		if (!bIsNativeVar)
		{
			TargetProperty->SetMetaData(TEXT("Category"), *SetCategory.ToString());
			const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
			if (VarIndex != INDEX_NONE)
			{
				Blueprint->NewVariables[VarIndex].Category = SetCategory;
			}
			else
			{
				const int32 SCS_NodeIndex = FBlueprintEditorUtils::FindSCS_Node(Blueprint, VarName);
				if (SCS_NodeIndex != INDEX_NONE)
				{
					Blueprint->SimpleConstructionScript->GetAllNodes()[SCS_NodeIndex]->Modify();
					Blueprint->SimpleConstructionScript->GetAllNodes()[SCS_NodeIndex]->CategoryName = SetCategory;
				}
			}
		}
	}
	else if (InLocalVarScope)
	{
		UK2Node_FunctionEntry* OutFunctionEntryNode;
		if (FBPVariableDescription* LocalVariable = FindLocalVariable(Blueprint, InLocalVarScope, VarName, &OutFunctionEntryNode))
		{
			OutFunctionEntryNode->Modify();
			LocalVariable->SetMetaData(TEXT("Category"), *SetCategory.ToString());
			LocalVariable->Category = SetCategory;
		}
	}

	if (bDontRecompile == false)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

void FBlueprintEditorUtils::SetBlueprintFunctionOrMacroCategory(UEdGraph* Graph, const FText& InCategoryName, bool bDontRecompile)
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);
	if (FKismetUserDeclaredFunctionMetadata* MetaData = FBlueprintEditorUtils::GetGraphFunctionMetaData(Graph))
	{
		UFunction* Function = nullptr;
		for (TFieldIterator<UFunction> FunctionIt(Blueprint->SkeletonGeneratedClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			if (FunctionIt->GetName() == Graph->GetName())
			{
				Function = *FunctionIt;
				break;
			}
		}

		const FText& NewCategory = InCategoryName.IsEmpty() ? UEdGraphSchema_K2::VR_DefaultCategory : InCategoryName;
		MetaData->Category = NewCategory;

		if (Function)
		{
			check(!Function->IsNative()); // Should never get here with a native function, as we wouldn't have been able to find metadata for it
			Function->Modify();
			Function->SetMetaData(FBlueprintMetadata::MD_FunctionCategory, *NewCategory.ToString());
		}

		if (!bDontRecompile)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
}

int32 FBlueprintEditorUtils::FindIndexOfGraphInParent(UEdGraph* Graph)
{
	int32 Result = INDEX_NONE;

	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
	{
		Result = Blueprint->FunctionGraphs.IndexOfByKey(Graph);
		if (Result == INDEX_NONE)
		{
			Result = Blueprint->MacroGraphs.IndexOfByKey(Graph);
		}
	}

	return Result;
}

bool FBlueprintEditorUtils::MoveGraphBeforeOtherGraph(UEdGraph* Graph, int32 NewIndex, bool bDontRecompile)
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
	{
		bool bModified = false;

		const int32 OldFunctionIndex = Blueprint->FunctionGraphs.IndexOfByKey(Graph);
		if (OldFunctionIndex != INDEX_NONE)
		{
			if ((OldFunctionIndex != NewIndex) && Blueprint->FunctionGraphs.IsValidIndex(NewIndex))
			{
				Blueprint->Modify();
				Blueprint->FunctionGraphs.Insert(Graph, NewIndex);
				Blueprint->FunctionGraphs.RemoveAt((OldFunctionIndex < NewIndex) ? OldFunctionIndex : (OldFunctionIndex + 1));
				bModified = true;
			}
		}

		const int32 OldMacroIndex = Blueprint->MacroGraphs.IndexOfByKey(Graph);
		if (OldMacroIndex != INDEX_NONE)
		{
			if ((OldMacroIndex != NewIndex) && Blueprint->MacroGraphs.IsValidIndex(NewIndex))
			{
				Blueprint->Modify();
				Blueprint->MacroGraphs.Insert(Graph, NewIndex);
				Blueprint->MacroGraphs.RemoveAt((OldMacroIndex < NewIndex) ? OldMacroIndex : (OldMacroIndex + 1));
				bModified = true;
			}
		}

		if (bModified && !bDontRecompile)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}

		return bModified;
	}

	return false;
}


FText FBlueprintEditorUtils::GetBlueprintVariableCategory(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope)
{
	FText CategoryName;
	UClass* SkeletonGeneratedClass = Blueprint->SkeletonGeneratedClass;
	UProperty* TargetProperty = FindField<UProperty>(SkeletonGeneratedClass, VarName);
	if(TargetProperty != nullptr)
	{
		CategoryName = FObjectEditorUtils::GetCategoryText(TargetProperty);
	}
	else if(InLocalVarScope)
	{
		// Check to see if it is a local variable
		if(FBPVariableDescription* LocalVariable = FindLocalVariable(Blueprint, InLocalVarScope, VarName))
		{
			CategoryName = LocalVariable->Category;
		}
	}

	if(CategoryName.IsEmpty() && Blueprint->SimpleConstructionScript != nullptr)
	{
		// Look for the variable in the SCS (in case the Blueprint has not been compiled yet)
		const int32 SCS_NodeIndex = FBlueprintEditorUtils::FindSCS_Node(Blueprint, VarName);
		if (SCS_NodeIndex != INDEX_NONE)
		{
			CategoryName = Blueprint->SimpleConstructionScript->GetAllNodes()[SCS_NodeIndex]->CategoryName;
		}
	}

	return CategoryName;
}

uint64* FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(UBlueprint* Blueprint, const FName& VarName)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
	if (VarIndex != INDEX_NONE)
	{
		return &Blueprint->NewVariables[VarIndex].PropertyFlags;
	}
	return nullptr;
}

FName FBlueprintEditorUtils::GetBlueprintVariableRepNotifyFunc(UBlueprint* Blueprint, const FName& VarName)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
	if (VarIndex != INDEX_NONE)
	{
		return Blueprint->NewVariables[VarIndex].RepNotifyFunc;
	}
	return NAME_None;
}

void FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(UBlueprint* Blueprint, const FName& VarName, const FName& RepNotifyFunc)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
	if (VarIndex != INDEX_NONE)
	{
		Blueprint->NewVariables[VarIndex].RepNotifyFunc = RepNotifyFunc;
	}
}

void FBlueprintEditorUtils::GetFunctionNameList(const UBlueprint* Blueprint, TSet<FName>& FunctionNames)
{
	if( UClass* SkeletonClass = Blueprint->SkeletonGeneratedClass )
	{
		for( TFieldIterator<UFunction> FuncIt(SkeletonClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt )
		{
			FunctionNames.Add( (*FuncIt)->GetFName() );
		}
	}
}

void FBlueprintEditorUtils::GetDelegateNameList(const UBlueprint* Blueprint, TSet<FName>& FunctionNames)
{
	check(Blueprint);
	for (int32 It = 0; It < Blueprint->DelegateSignatureGraphs.Num(); It++)
	{
		if (UEdGraph* Graph = Blueprint->DelegateSignatureGraphs[It])
		{
			FunctionNames.Add(Graph->GetFName());
		}
	}
}

UEdGraph* FBlueprintEditorUtils::GetDelegateSignatureGraphByName(UBlueprint* Blueprint, FName FunctionName)
{
	if ((nullptr != Blueprint) && (FunctionName != NAME_None))
	{
		for (int32 It = 0; It < Blueprint->DelegateSignatureGraphs.Num(); It++)
		{
			if (UEdGraph* Graph = Blueprint->DelegateSignatureGraphs[It])
			{
				if(FunctionName == Graph->GetFName())
				{
					return Graph;
				}
			}
		}
	}
	return nullptr;
}

// Gets a list of pins that should hidden for a given function
void FBlueprintEditorUtils::GetHiddenPinsForFunction(UEdGraph const* Graph, UFunction const* Function, TSet<FString>& HiddenPins, TSet<FString>* OutInternalPins)
{
	check(Function != nullptr);
	TMap<FName, FString>* MetaData = UMetaData::GetMapForObject(Function);	
	if (MetaData != nullptr)
	{
		for (TMap<FName, FString>::TConstIterator It(*MetaData); It; ++It)
		{
			static const FName NAME_LatentInfo = TEXT("LatentInfo");
			static const FName NAME_HidePin = TEXT("HidePin");

			const FName& Key = It.Key();

			if (Key == NAME_LatentInfo)
			{
				HiddenPins.Add(It.Value());
			}
			else if (Key == NAME_HidePin)
			{
				HiddenPins.Add(It.Value());
			}
			else if (Key == FBlueprintMetadata::MD_InternalUseParam)
			{
				HiddenPins.Add(It.Value());

				if (OutInternalPins != nullptr)
				{
					OutInternalPins->Add(It.Value());
				}
			}
			else if(Key == FBlueprintMetadata::MD_ExpandEnumAsExecs)
			{
				HiddenPins.Add(It.Value());
			}
			else if (Key == FBlueprintMetadata::MD_WorldContext)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
				if(!K2Schema->IsStaticFunctionGraph(Graph))
				{
					bool bHasIntrinsicWorldContext = false;

					UBlueprint const* CallingContext = FindBlueprintForGraph(Graph);
					if (CallingContext && CallingContext->ParentClass)
					{
						UClass* NativeOwner = CallingContext->ParentClass;
						while(NativeOwner && !NativeOwner->IsNative())
						{
							NativeOwner = NativeOwner->GetSuperClass();
						}

						if(NativeOwner)
						{
							bHasIntrinsicWorldContext = NativeOwner->GetDefaultObject()->ImplementsGetWorld();
						}
					}

					// if the blueprint has world context that we can lookup with "self", 
					// then we can hide this pin (and default it to self)
					if (bHasIntrinsicWorldContext)
					{
						HiddenPins.Add(It.Value());
					}
				}
			}
		}
	}
}

bool FBlueprintEditorUtils::IsPinTypeValid(const FEdGraphPinType& Type)
{
	if (const UUserDefinedStruct* UDStruct = Cast<const UUserDefinedStruct>(Type.PinSubCategoryObject.Get()))
	{
		if (EUserDefinedStructureStatus::UDSS_UpToDate != UDStruct->Status)
		{
			return false;
		}
	}
	return true;
}

void FBlueprintEditorUtils::GetClassVariableList(const UBlueprint* Blueprint, TSet<FName>& VisibleVariables, bool bIncludePrivateVars) 
{
	// Existing variables in the parent class and above, when using the compilation manager the previous SkeletonGeneratedClass will have been cleared when
	// we're regenerating the SkeletonGeneratedClass. Using this function in the skeleton pass at all is highly dubious, but I am leaving it until the 
	// compilation manager is on full time:
	check(!Blueprint->bHasBeenRegenerated || Blueprint->bIsRegeneratingOnLoad || (Blueprint->SkeletonGeneratedClass != nullptr) || GBlueprintUseCompilationManager);
	if (Blueprint->SkeletonGeneratedClass != nullptr)
	{
		for (TFieldIterator<UProperty> PropertyIt(Blueprint->SkeletonGeneratedClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;

			if ((!Property->HasAnyPropertyFlags(CPF_Parm) && (bIncludePrivateVars || Property->HasAllPropertyFlags(CPF_BlueprintVisible))))
			{
				VisibleVariables.Add(Property->GetFName());
			}
		}

		if (bIncludePrivateVars)
		{
			// Include SCS node variable names, timelines, and other member variables that may be pending compilation. Consider them to be "private" as they're not technically accessible for editing just yet.
			TArray<UBlueprint*> ParentBPStack;
			UBlueprint::GetBlueprintHierarchyFromClass(Blueprint->SkeletonGeneratedClass, ParentBPStack);
			for (int32 StackIndex = ParentBPStack.Num() - 1; StackIndex >= 0; --StackIndex)
			{
				UBlueprint* ParentBP = ParentBPStack[StackIndex];
				check(ParentBP != nullptr);

				GetSCSVariableNameList(ParentBP, VisibleVariables);

				for (int32 VariableIndex = 0; VariableIndex < ParentBP->NewVariables.Num(); ++VariableIndex)
				{
					VisibleVariables.Add(ParentBP->NewVariables[VariableIndex].VarName);
				}

				for (UTimelineTemplate* Timeline : ParentBP->Timelines)
				{
					if (Timeline)
					{
						VisibleVariables.Add(Timeline->GetFName());
					}
				}
			}
		}
	}

	// "self" is reserved for all classes
	VisibleVariables.Add(NAME_Self);
}

void FBlueprintEditorUtils::GetNewVariablesOfType( const UBlueprint* Blueprint, const FEdGraphPinType& Type, TArray<FName>& OutVars )
{
	for(int32 i=0; i<Blueprint->NewVariables.Num(); i++)
	{
		const FBPVariableDescription& Var = Blueprint->NewVariables[i];
		if(Type == Var.VarType)
		{
			OutVars.Add(Var.VarName);
		}
	}
}

void FBlueprintEditorUtils::GetLocalVariablesOfType( const UEdGraph* Graph, const FEdGraphPinType& Type, TArray<FName>& OutVars)
{
	if (DoesSupportLocalVariables(Graph))
	{
		// Grab the function graph, so we can find the function entry node for local variables
		UEdGraph* FunctionGraph = FBlueprintEditorUtils::GetTopLevelGraph(Graph);

		TArray<UK2Node_FunctionEntry*> GraphNodes;
		FunctionGraph->GetNodesOfClass<UK2Node_FunctionEntry>(GraphNodes);

		// There should only be one entry node
		check(GraphNodes.Num() == 1);

		for (const FBPVariableDescription& LocalVar : GraphNodes[0]->LocalVariables)
		{
			if (LocalVar.VarType == Type)
			{
				OutVars.Add(LocalVar.VarName);
			}
		}
	}
}

// Adds a member variable to the blueprint.  It cannot mask a variable in any superclass.
bool FBlueprintEditorUtils::AddMemberVariable(UBlueprint* Blueprint, const FName& NewVarName, const FEdGraphPinType& NewVarType, const FString& DefaultValue/* = FString()*/)
{
	// Don't allow vars with empty names
	if(NewVarName == NAME_None)
	{
		return false;
	}

	// First we need to see if there is already a variable with that name, in this blueprint or parent class
	TSet<FName> CurrentVars;
	FBlueprintEditorUtils::GetClassVariableList(Blueprint, CurrentVars);
	if(CurrentVars.Contains(NewVarName))
	{
		return false; // fail
	}

	Blueprint->Modify();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Now create new variable
	FBPVariableDescription NewVar;

	NewVar.VarName = NewVarName;
	NewVar.VarGuid = FGuid::NewGuid();
	NewVar.FriendlyName = FName::NameToDisplayString( NewVarName.ToString(), (NewVarType.PinCategory == K2Schema->PC_Boolean) ? true : false );
	NewVar.VarType = NewVarType;
	// default new vars to 'kismet read/write' and 'only editable on owning CDO' 
	NewVar.PropertyFlags |= (CPF_Edit | CPF_BlueprintVisible | CPF_DisableEditOnInstance);
	if(NewVarType.PinCategory == K2Schema->PC_MCDelegate)
	{
		NewVar.PropertyFlags |= CPF_BlueprintAssignable | CPF_BlueprintCallable;
	}
	else
	{
		PostSetupObjectPinType(Blueprint, NewVar);
	}
	NewVar.ReplicationCondition = COND_None;
	NewVar.Category = UEdGraphSchema_K2::VR_DefaultCategory;
	NewVar.DefaultValue = DefaultValue;

	// user created variables should be none of these things
	NewVar.VarType.bIsConst       = false;
	NewVar.VarType.bIsWeakPointer = false;
	NewVar.VarType.bIsReference   = false;

	Blueprint->NewVariables.Add(NewVar);

	// Potentially adjust variable names for any child blueprints
	FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, NewVarName);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	return true;
}

// Removes a member variable if it was declared in this blueprint and not in a base class.
void FBlueprintEditorUtils::RemoveMemberVariable(UBlueprint* Blueprint, const FName VarName)
{
	const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarName);
	if (VarIndex != INDEX_NONE)
	{
		Blueprint->NewVariables.RemoveAt(VarIndex);
		FBlueprintEditorUtils::RemoveVariableNodes(Blueprint, VarName);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

void FBlueprintEditorUtils::BulkRemoveMemberVariables(UBlueprint* Blueprint, const TArray<FName>& VarNames)
{
	const FScopedTransaction Transaction( LOCTEXT("DeleteUnusedVariables", "Delete Unused Variables") );
	Blueprint->Modify();

	bool bModified = false;
	for (int32 i = 0; i < VarNames.Num(); ++i)
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VarNames[i]);
		if (VarIndex != INDEX_NONE)
		{
			Blueprint->NewVariables.RemoveAt(VarIndex);
			FBlueprintEditorUtils::RemoveVariableNodes(Blueprint, VarNames[i]);
			bModified = true;
		}
	}

	if (bModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

FGuid FBlueprintEditorUtils::FindMemberVariableGuidByName(UBlueprint* InBlueprint, const FName InVariableName)
{
	while(InBlueprint)
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(InBlueprint, InVariableName);
		if (VarIndex != INDEX_NONE)
		{
			return InBlueprint->NewVariables[VarIndex].VarGuid;
		}
		InBlueprint = Cast<UBlueprint>(InBlueprint->ParentClass->ClassGeneratedBy);
	}
	return FGuid(); 
}

FName FBlueprintEditorUtils::FindMemberVariableNameByGuid(UBlueprint* InBlueprint, const FGuid& InVariableGuid)
{
	while(InBlueprint)
	{
		for(FBPVariableDescription& Variable : InBlueprint->NewVariables)
		{
			if(Variable.VarGuid == InVariableGuid)
			{
				return Variable.VarName;
			}
		}

		InBlueprint = Cast<UBlueprint>(InBlueprint->ParentClass->ClassGeneratedBy);
	}
	return NAME_None;
}

void FBlueprintEditorUtils::RemoveVariableNodes(UBlueprint* Blueprint, const FName VarName, bool const bForSelfOnly, UEdGraph* LocalGraphScope)
{
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for(TArray<UEdGraph*>::TConstIterator it(AllGraphs); it; ++it)
	{
		const UEdGraph* CurrentGraph = *it;

		TArray<UK2Node_Variable*> GraphNodes;
		CurrentGraph->GetNodesOfClass(GraphNodes);

		for( TArray<UK2Node_Variable*>::TConstIterator NodeIt(GraphNodes); NodeIt; ++NodeIt )
		{
			UK2Node_Variable* CurrentNode = *NodeIt;

			UClass* SelfClass = Blueprint->GeneratedClass;
			UClass* VariableParent = CurrentNode->VariableReference.GetMemberParentClass(SelfClass);

			if ((SelfClass == VariableParent) || !bForSelfOnly)
			{
				if(LocalGraphScope == CurrentNode->GetGraph() || LocalGraphScope == nullptr)
				{
					if(VarName == CurrentNode->GetVarName())
					{
						CurrentNode->DestroyNode();
					}
				}
			}
		}
	}
}

void FBlueprintEditorUtils::RenameComponentMemberVariable(UBlueprint* Blueprint, USCS_Node* Node, const FName NewName)
{
	// Should not allow renaming to "none" (UI should prevent this)
	check(!NewName.IsNone());

	if (!NewName.IsEqual(Node->GetVariableName(), ENameCase::CaseSensitive))
	{
		Blueprint->Modify();

		// Validate child blueprints and adjust variable names to avoid a potential name collision
		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, NewName);

		// Update the name
		const FName OldName = Node->GetVariableName();
		Node->Modify();
		Node->SetVariableName(NewName);

		// Rename Inheritable Component Templates
		{
			const FComponentKey Key(Node);
			TArray<UBlueprint*> Dependents;
			GetDependentBlueprints(Blueprint, Dependents);
			for (UBlueprint* DepBP : Dependents)
			{
				UInheritableComponentHandler* InheritableComponentHandler = DepBP ? DepBP->GetInheritableComponentHandler(false) : nullptr;
				if (InheritableComponentHandler && InheritableComponentHandler->GetOverridenComponentTemplate(Key))
				{
					InheritableComponentHandler->Modify();
					InheritableComponentHandler->RefreshTemplateName(Key);
					InheritableComponentHandler->MarkPackageDirty();
				}
			}
		}

		Node->NameWasModified();

		// Update any existing references to the old name
		if (OldName != NAME_None)
		{
			FBlueprintEditorUtils::ReplaceVariableReferences(Blueprint, OldName, NewName);
		}

		// And recompile
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

void FBlueprintEditorUtils::RenameMemberVariable(UBlueprint* Blueprint, const FName OldName, const FName NewName)
{
	if (!NewName.IsNone() && !NewName.IsEqual(OldName, ENameCase::CaseSensitive))
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, OldName);
		if (VarIndex != INDEX_NONE)
		{
			const FScopedTransaction Transaction( LOCTEXT("RenameVariable", "Rename Variable") );
			Blueprint->Modify();

			// Update the name
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			FBPVariableDescription& Variable = Blueprint->NewVariables[VarIndex];
			Variable.VarName = NewName;
			Variable.FriendlyName = FName::NameToDisplayString( NewName.ToString(), (Variable.VarType.PinCategory == K2Schema->PC_Boolean) ? true : false );

			// Update any existing references to the old name
			FBlueprintEditorUtils::ReplaceVariableReferences(Blueprint, OldName, NewName);

			{
				//Grab property of blueprint's current CDO
				UClass* GeneratedClass = Blueprint->GeneratedClass;
				UObject* GeneratedCDO = GeneratedClass ? GeneratedClass->GetDefaultObject(false) : nullptr;
				if (GeneratedCDO)
				{
					UProperty* TargetProperty = FindField<UProperty>(GeneratedCDO->GetClass(), OldName); // GeneratedCDO->GetClass() is used instead of GeneratedClass, because CDO could use REINST class.
					// Grab the address of where the property is actually stored (UObject* base, plus the offset defined in the property)
					void* OldPropertyAddr = TargetProperty ? TargetProperty->ContainerPtrToValuePtr<void>(GeneratedCDO) : nullptr;
					if (OldPropertyAddr)
					{
						// if there is a property for variable, it means the original default value was already copied, so it can be safely overridden
						Variable.DefaultValue.Empty();
						PropertyValueToString(TargetProperty, reinterpret_cast<const uint8*>(GeneratedCDO), Variable.DefaultValue);
					}
				}
				else
				{
					UE_LOG(LogBlueprint, Warning, TEXT("Could not find default value of renamed variable '%s' (previously '%s') in %s"), *NewName.ToString(), *OldName.ToString(), *GetPathNameSafe(Blueprint));
				}

				// Validate child blueprints and adjust variable names to avoid a potential name collision
				FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, NewName);

				// And recompile
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			}

			{
				const bool bIsDelegateVar = (Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate);
				if (UEdGraph* DelegateSignatureGraph = bIsDelegateVar ? GetDelegateSignatureGraphByName(Blueprint, OldName) : nullptr)
				{
					FBlueprintEditorUtils::RenameGraph(DelegateSignatureGraph, NewName.ToString());

					// this code should not be necessary, because the GUID remains valid, but let it be for backward compatibility.
					TArray<UK2Node_BaseMCDelegate*> NodeUsingDelegate;
					FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_BaseMCDelegate>(Blueprint, NodeUsingDelegate);
					for (UK2Node_BaseMCDelegate* FunctionNode : NodeUsingDelegate)
					{
						if (FunctionNode->DelegateReference.IsSelfContext() && (FunctionNode->DelegateReference.GetMemberName() == OldName))
						{
							FunctionNode->Modify();
							FunctionNode->DelegateReference.SetSelfMember(NewName);
						}
					}
				}
			}
		}
		else if (Blueprint && Blueprint->SimpleConstructionScript)
		{
			// Wasn't in the introduced variable list; try to find the associated SCS node
			//@TODO: The SCS-generated variables should be in the variable list and have a link back;
			// As it stands, you cannot do any metadata operations on a SCS variable, and you have to do icky code like the following
			TArray<USCS_Node*> Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();
			for (TArray<USCS_Node*>::TConstIterator NodeIt(Nodes); NodeIt; ++NodeIt)
			{
				USCS_Node* CurrentNode = *NodeIt;
				if (CurrentNode->GetVariableName() == OldName)
				{
					RenameComponentMemberVariable(Blueprint, CurrentNode, NewName);
					break;
				}
			}
		}
	}
}

TArray<UK2Node*> FBlueprintEditorUtils::GetNodesForVariable(const FName& InVarName, const UBlueprint* InBlueprint, const UStruct* InScope/* = nullptr*/)
{
	TArray<UK2Node*> ReturnNodes;
	TArray<UK2Node*> Nodes;
	GetAllNodesOfClass<UK2Node>(InBlueprint, Nodes);

	bool bNodesPendingDeletion = false;
	for( TArray<UK2Node*>::TConstIterator NodeIt(Nodes); NodeIt; ++NodeIt )
	{
		UK2Node* CurrentNode = *NodeIt;
		if (CurrentNode->ReferencesVariable(InVarName, InScope))
		{
			ReturnNodes.Add(CurrentNode);
		}
	}
	return ReturnNodes;
}

bool FBlueprintEditorUtils::VerifyUserWantsVariableTypeChanged(const FName& InVarName)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("VariableName"), FText::FromName(InVarName));

	FText ConfirmDelete = FText::Format(LOCTEXT( "ConfirmChangeVarType",
		"This could break connections, do you want to search all Variable '{VariableName}' instances, change its type, and recompile?"), Args );

	// Warn the user that this may result in data loss
	FSuppressableWarningDialog::FSetupInfo Info( ConfirmDelete, LOCTEXT("ChangeVariableType", "Change Variable Type"), "ChangeVariableType_Warning" );
	Info.ConfirmText = LOCTEXT( "ChangeVariableType_Yes", "Change Variable Type");
	Info.CancelText = LOCTEXT( "ChangeVariableType_No", "Do Nothing");	

	FSuppressableWarningDialog ChangeVariableType( Info );

	FSuppressableWarningDialog::EResult RetCode = ChangeVariableType.ShowModal();
	return RetCode == FSuppressableWarningDialog::Confirm || RetCode == FSuppressableWarningDialog::Suppressed;
}

void FBlueprintEditorUtils::GetLoadedChildBlueprints(UBlueprint* InBlueprint, TArray<UBlueprint*>& OutBlueprints)
{
	// Iterate over currently-loaded Blueprints and potentially adjust their variable names if they conflict with the parent
	for(TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
	{
		UBlueprint* ChildBP = *BlueprintIt;
		if(ChildBP != nullptr && ChildBP->ParentClass != nullptr)
		{
			TArray<UBlueprint*> ParentBPArray;
			// Get the parent hierarchy
			UBlueprint::GetBlueprintHierarchyFromClass(ChildBP->ParentClass, ParentBPArray);

			// Also get any BP interfaces we use
			TArray<UClass*> ImplementedInterfaces;
			FindImplementedInterfaces(ChildBP, true, ImplementedInterfaces);
			for (UClass* ImplementedInterface : ImplementedInterfaces)
			{
				UBlueprint* BlueprintInterfaceClass = UBlueprint::GetBlueprintFromClass(ImplementedInterface);
				if(BlueprintInterfaceClass != nullptr)
				{
					ParentBPArray.Add(BlueprintInterfaceClass);
				}
			}

			if(ParentBPArray.Contains(InBlueprint))
			{
				OutBlueprints.Add(ChildBP);
			}
		}
	}
}

void FBlueprintEditorUtils::ChangeMemberVariableType(UBlueprint* Blueprint, const FName VariableName, const FEdGraphPinType& NewPinType)
{
	if (VariableName != NAME_None)
	{
		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName);
		if (VarIndex != INDEX_NONE)
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			FBPVariableDescription& Variable = Blueprint->NewVariables[VarIndex];
			
			// Update the variable type only if it is different
			if (Variable.VarType != NewPinType)
			{
				TArray<UBlueprint*> ChildBPs;
				GetLoadedChildBlueprints(Blueprint, ChildBPs);

				TArray<UK2Node*> AllVariableNodes = GetNodesForVariable(VariableName, Blueprint);
				for(UBlueprint* ChildBP : ChildBPs)
				{
					TArray<UK2Node*> VariableNodes = GetNodesForVariable(VariableName, ChildBP);
					AllVariableNodes.Append(VariableNodes);
				}

				// TRUE if the user might be breaking variable connections
				bool bBreakingVariableConnections = false;

				// If there are variable nodes in place, warn the user of the consequences using a suppressible dialog
				if(AllVariableNodes.Num())
				{
					if(!VerifyUserWantsVariableTypeChanged(VariableName))
					{
						// User has decided to cancel changing the variable member type
						return;
					}
					bBreakingVariableConnections = true;
				}

				const FScopedTransaction Transaction( LOCTEXT("ChangeVariableType", "Change Variable Type") );
				Blueprint->Modify();

				/** Only change the variable type if type selection is valid, some unloaded Blueprints will turn out to be bad */
				bool bChangeVariableType = true;

				if ((NewPinType.PinCategory == K2Schema->PC_Object) || (NewPinType.PinCategory == K2Schema->PC_Interface))
				{
					// if it's a PC_Object, then it should have an associated UClass object
					if(NewPinType.PinSubCategoryObject.IsValid())
					{
						const UClass* ClassObject = Cast<UClass>(NewPinType.PinSubCategoryObject.Get());
						check(ClassObject != nullptr);

						if (ClassObject->IsChildOf(AActor::StaticClass()))
						{
							// prevent Actor variables from having default values (because Blueprint templates are library elements that can 
							// bridge multiple levels and different levels might not have the actor that the default is referencing).
							Variable.PropertyFlags |= CPF_DisableEditOnTemplate;
						}
						else 
						{
							// clear the disable-default-value flag that might have been present (if this was an AActor variable before)
							Variable.PropertyFlags &= ~(CPF_DisableEditOnTemplate);
						}
					}
					else
					{
						bChangeVariableType = false;

						// Display a notification to inform the user that the variable type was invalid (likely due to corruption), it should no longer appear in the list.
						FNotificationInfo Info( LOCTEXT("InvalidUnloadedBP", "The selected type was invalid once loaded, it has been removed from the list!") );
						Info.ExpireDuration = 3.0f;
						Info.bUseLargeFont = false;
						TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
						if ( Notification.IsValid() )
						{
							Notification->SetCompletionState( SNotificationItem::CS_Fail );
						}
					}
				}
				else 
				{
					// clear the disable-default-value flag that might have been present (if this was an AActor variable before)
					Variable.PropertyFlags &= ~(CPF_DisableEditOnTemplate);
				}

				if(bChangeVariableType)
				{
					Variable.VarType = NewPinType;

					if(Variable.VarType.IsSet() || Variable.VarType.IsMap())
					{
						// Make sure that the variable is no longer tagged for replication, and warn the user if the variable is no
						// longer goign to be replicated:
						if(Variable.RepNotifyFunc != NAME_None || Variable.PropertyFlags & CPF_Net || Variable.PropertyFlags & CPF_RepNotify)
						{
							FNotificationInfo Warning( 
								FText::Format(
									LOCTEXT("InvalidReplicationSettings", "Maps and sets cannot be replicated - {0} has had its replication settings cleared"),
									FText::FromName(Variable.VarName) 
								) 
							);
							Warning.ExpireDuration = 5.0f;
							Warning.bFireAndForget = true;
							Warning.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
							FSlateNotificationManager::Get().AddNotification(Warning);

							Variable.PropertyFlags &= ~CPF_Net;
							Variable.PropertyFlags &= ~CPF_RepNotify;
							Variable.RepNotifyFunc = NAME_None;
							Variable.ReplicationCondition = COND_None;
						}
					}

					UClass* ParentClass = nullptr;
					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

					if(bBreakingVariableConnections)
					{
						for(UBlueprint* ChildBP : ChildBPs)
						{
							// Mark the Blueprint as structurally modified so we can reconstruct the node successfully
							FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ChildBP);
						}

						// Reconstruct all variable nodes that reference the changing variable
						for(UK2Node* VariableNode : AllVariableNodes)
						{
							K2Schema->ReconstructNode(*VariableNode, true);
						}

						TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(Blueprint);
						if (FoundAssetEditor.IsValid())
						{
							TSharedRef<IBlueprintEditor> BlueprintEditor = StaticCastSharedRef<IBlueprintEditor>(FoundAssetEditor.ToSharedRef());

							UK2Node* FirstVariableNode = nullptr;
							for (UK2Node* VariableNode : AllVariableNodes)
							{
								if (VariableNode->IsA<UK2Node_Variable>())
								{
									FirstVariableNode = VariableNode;
									break;
								}
							}

							if (FirstVariableNode)
							{
								const bool bSetFindWithinBlueprint = false;
								const bool bSelectFirstResult = false;
								BlueprintEditor->SummonSearchUI(bSetFindWithinBlueprint, FirstVariableNode->GetFindReferenceSearchString(), bSelectFirstResult);
							}
						}
					}
				}
			}
		}
	}
}

FName FBlueprintEditorUtils::DuplicateVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName& InVariableToDuplicate)
{
	FName DuplicatedVariableName = NAME_None;

	if (InVariableToDuplicate != NAME_None)
	{
		const FScopedTransaction Transaction( LOCTEXT( "DuplicateVariable", "Duplicate Variable" ) );
		InBlueprint->Modify();

		FBPVariableDescription NewVar;

		const int32 VarIndex = FBlueprintEditorUtils::FindNewVariableIndex(InBlueprint, InVariableToDuplicate);
		if (VarIndex != INDEX_NONE)
		{
			FBPVariableDescription& Variable = InBlueprint->NewVariables[VarIndex];

			NewVar = DuplicateVariableDescription(InBlueprint, Variable);

			// We need to manually pull the DefaultValue from the UProperty to set it
			void* OldPropertyAddr = nullptr;

			//Grab property of blueprint's current CDO
			UClass* GeneratedClass = InBlueprint->GeneratedClass;
			UObject* GeneratedCDO = GeneratedClass->GetDefaultObject();
			UProperty* TargetProperty = FindField<UProperty>(GeneratedClass, Variable.VarName);

			if( TargetProperty )
			{
				// Grab the address of where the property is actually stored (UObject* base, plus the offset defined in the property)
				OldPropertyAddr = TargetProperty->ContainerPtrToValuePtr<void>(GeneratedCDO);
				if(OldPropertyAddr)
				{
					// if there is a property for variable, it means the original default value was already copied, so it can be safely overridden
					Variable.DefaultValue.Empty();
					TargetProperty->ExportTextItem(NewVar.DefaultValue, OldPropertyAddr, OldPropertyAddr, nullptr, PPF_SerializedAsImportText);
				}
			}

			// Add the new variable
			InBlueprint->NewVariables.Add(NewVar);
		}
		else
		{
			// It's probably a local variable

			UK2Node_FunctionEntry* FunctionEntry = nullptr;
			FBPVariableDescription* LocalVariable = FBlueprintEditorUtils::FindLocalVariable(InBlueprint, InScope, InVariableToDuplicate, &FunctionEntry);

			if (LocalVariable)
			{
				FunctionEntry->Modify();

				NewVar = DuplicateVariableDescription(InBlueprint, *LocalVariable);

				// Add the new variable
				FunctionEntry->LocalVariables.Add(NewVar);
			}
		}

		if(NewVar.VarGuid.IsValid())
		{
			DuplicatedVariableName = NewVar.VarName;

			// Potentially adjust variable names for any child blueprints
			FBlueprintEditorUtils::ValidateBlueprintChildVariables(InBlueprint, NewVar.VarName);

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
		}
	}

	return DuplicatedVariableName;
}

FBPVariableDescription FBlueprintEditorUtils::DuplicateVariableDescription(UBlueprint* InBlueprint, FBPVariableDescription& InVariableDescription)
{
	FName DuplicatedVariableName = FindUniqueKismetName(InBlueprint, InVariableDescription.VarName.GetPlainNameString());

	// Now create new variable
	FBPVariableDescription NewVar = InVariableDescription;
	NewVar.VarName = DuplicatedVariableName;
	NewVar.FriendlyName = FName::NameToDisplayString( NewVar.VarName.ToString(), NewVar.VarType.PinCategory == GetDefault<UEdGraphSchema_K2>()->PC_Boolean);
	NewVar.VarGuid = FGuid::NewGuid();

	return NewVar;
}

bool FBlueprintEditorUtils::AddLocalVariable(UBlueprint* Blueprint, UEdGraph* InTargetGraph, const FName InNewVarName, const FEdGraphPinType& InNewVarType, const FString& DefaultValue/*= FString()*/)
{
	if(InTargetGraph != nullptr && InTargetGraph->GetSchema()->GetGraphType(InTargetGraph) == GT_Function)
	{
		// Ensure we have the top level graph for the function, in-case we are in a child graph
		UEdGraph* TargetGraph = FBlueprintEditorUtils::GetTopLevelGraph(InTargetGraph);

		const FScopedTransaction Transaction( LOCTEXT("AddLocalVariable", "Add Local Variable") );
		Blueprint->Modify();

		TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
		TargetGraph->GetNodesOfClass(FunctionEntryNodes);
		check(FunctionEntryNodes.Num());

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		// Now create new variable
		FBPVariableDescription NewVar;

		NewVar.VarName = InNewVarName;
		NewVar.VarGuid = FGuid::NewGuid();
		NewVar.VarType = InNewVarType;
		NewVar.PropertyFlags |= CPF_BlueprintVisible;
		NewVar.FriendlyName = FName::NameToDisplayString( NewVar.VarName.ToString(), (NewVar.VarType.PinCategory == K2Schema->PC_Boolean) ? true : false );
		NewVar.Category = UEdGraphSchema_K2::VR_DefaultCategory;
		NewVar.DefaultValue = DefaultValue;

		PostSetupObjectPinType(Blueprint, NewVar);

		FunctionEntryNodes[0]->Modify();
		FunctionEntryNodes[0]->LocalVariables.Add(NewVar);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		return true;
	}
	return false;
}

void FBlueprintEditorUtils::RemoveLocalVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVarName)
{
	UEdGraph* ScopeGraph = FindScopeGraph(InBlueprint, InScope);

	if(ScopeGraph)
	{
		TArray<UK2Node_FunctionEntry*> GraphNodes;
		ScopeGraph->GetNodesOfClass<UK2Node_FunctionEntry>(GraphNodes);

		bool bFoundLocalVariable = false;

		// There is only ever 1 function entry
		check(GraphNodes.Num() == 1)
		for( int32 VarIdx = 0; VarIdx < GraphNodes[0]->LocalVariables.Num(); ++VarIdx )
		{
			if(GraphNodes[0]->LocalVariables[VarIdx].VarName == InVarName)
			{
				GraphNodes[0]->LocalVariables.RemoveAt(VarIdx);
				FBlueprintEditorUtils::RemoveVariableNodes(InBlueprint, InVarName, true, ScopeGraph);
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);

				bFoundLocalVariable = true;

				// No other local variables will match, we are done
				break;
			}
		}

		// Check if we found the local variable, it is a problem if we do not.
		if(!bFoundLocalVariable)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Could not find local variable '%s'!"), *InVarName.ToString());
		}
	}
}

FFunctionFromNodeHelper::FFunctionFromNodeHelper(const UObject* Obj) : Function(FunctionFromNode(Cast<UK2Node>(Obj))), Node(Cast<UK2Node>(Obj))
{

}

UFunction* FFunctionFromNodeHelper::FunctionFromNode(const UK2Node* Node)
{
	UFunction* Function = nullptr;
	UBlueprint* Blueprint = Node ? Node->GetBlueprint() : nullptr;
	const UClass* SearchScope = Blueprint ? Blueprint->SkeletonGeneratedClass : nullptr;
	if (SearchScope)
	{
		if (const UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node))
		{
			// Function result nodes cannot resolve the UFunction, so find the entry node and use that for finding the UFunction
			TArray<UK2Node_FunctionEntry*> EntryNodes;
			ResultNode->GetGraph()->GetNodesOfClass(EntryNodes);

			check(EntryNodes.Num() == 1);
			Node = EntryNodes[0];
		}
		
		if (const UK2Node_FunctionEntry* FunctionNode = Cast<UK2Node_FunctionEntry>(Node))
		{
			const FName FunctionName = (FunctionNode->CustomGeneratedFunctionName != NAME_None) ? FunctionNode->CustomGeneratedFunctionName : FunctionNode->GetGraph()->GetFName();
			Function = SearchScope->FindFunctionByName(FunctionName);
		}
		else if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			// We need to search up the class hierachy by name or functions like CanAddParentNode will fail:
			Function = SearchScope->FindFunctionByName(EventNode->EventReference.GetMemberName());
		}
	}

	return Function;
}

UEdGraph* FBlueprintEditorUtils::FindScopeGraph(const UBlueprint* InBlueprint, const UStruct* InScope)
{
	UEdGraph* ScopeGraph = nullptr;

	TArray<UEdGraph*> AllGraphs;
	InBlueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		check(Graph != nullptr);
		if(Graph->GetFName() == InScope->GetFName())
		{
			// This graph should always be a function graph
			check(Graph->GetSchema()->GetGraphType(Graph) == GT_Function);
			
			ScopeGraph = Graph;
			break;
		}
	}
	return ScopeGraph;
}

void FBlueprintEditorUtils::RenameLocalVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName InOldName, const FName InNewName)
{
	if (!InNewName.IsNone() && !InNewName.IsEqual(InOldName, ENameCase::CaseSensitive))
	{
		UK2Node_FunctionEntry* FunctionEntry = nullptr;
		FBPVariableDescription* LocalVariable = FindLocalVariable(InBlueprint, InScope, InOldName, &FunctionEntry);
		const UProperty* OldProperty = FindField<const UProperty>(InScope, InOldName);
		const UProperty* ExistingProperty = FindField<const UProperty>(InScope, InNewName);
		const bool bHasExistingProperty = ExistingProperty && ExistingProperty != OldProperty;
		if (bHasExistingProperty)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Cannot name local variable '%s'. The name is already used."), *InNewName.ToString());
		}

		if (LocalVariable && !bHasExistingProperty)
		{
			const FScopedTransaction Transaction( LOCTEXT("RenameLocalVariable", "Rename Local Variable") );
			InBlueprint->Modify();
			FunctionEntry->Modify();

			// Update the name
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			LocalVariable->VarName = InNewName;
			LocalVariable->FriendlyName = FName::NameToDisplayString( InNewName.ToString(), LocalVariable->VarType.PinCategory == K2Schema->PC_Boolean );

			// Update any existing references to the old name
			RenameVariableReferencesInGraph(InBlueprint, InBlueprint->GeneratedClass, FindScopeGraph(InBlueprint, InScope), InOldName, InNewName);

			// Validate child blueprints and adjust variable names to avoid a potential name collision
			FBlueprintEditorUtils::ValidateBlueprintChildVariables(InBlueprint, InNewName);
			
			// And recompile
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);
		}
	}
}

FBPVariableDescription* FBlueprintEditorUtils::FindLocalVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName)
{
	UK2Node_FunctionEntry* DummyFunctionEntry = nullptr;
	return FindLocalVariable(InBlueprint, InScope, InVariableName, &DummyFunctionEntry);
}

FBPVariableDescription* FBlueprintEditorUtils::FindLocalVariable(const UBlueprint* InBlueprint, const UEdGraph* InScopeGraph, const FName InVariableName, class UK2Node_FunctionEntry** OutFunctionEntry)
{
	FBPVariableDescription* ReturnVariable = nullptr;
	if (DoesSupportLocalVariables(InScopeGraph))
	{
		UEdGraph* FunctionGraph = GetTopLevelGraph(InScopeGraph);
		TArray<UK2Node_FunctionEntry*> GraphNodes;
		FunctionGraph->GetNodesOfClass<UK2Node_FunctionEntry>(GraphNodes);

		bool bFoundLocalVariable = false;

		if (GraphNodes.Num() > 0)
		{
			// If there is an entry node, there should only be one
			check(GraphNodes.Num() == 1);

			for (int32 VarIdx = 0; VarIdx < GraphNodes[0]->LocalVariables.Num(); ++VarIdx)
			{
				if (GraphNodes[0]->LocalVariables[VarIdx].VarName == InVariableName)
				{
					if (OutFunctionEntry)
					{
						*OutFunctionEntry = GraphNodes[0];
					}
					ReturnVariable = &GraphNodes[0]->LocalVariables[VarIdx];
					break;
				}
			}
		}
	}

	return ReturnVariable;
}

FBPVariableDescription* FBlueprintEditorUtils::FindLocalVariable(const UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName, class UK2Node_FunctionEntry** OutFunctionEntry)
{
	UEdGraph* ScopeGraph = FindScopeGraph(InBlueprint, InScope);

	return FindLocalVariable(InBlueprint, ScopeGraph, InVariableName, OutFunctionEntry);
}

FName FBlueprintEditorUtils::FindLocalVariableNameByGuid(UBlueprint* InBlueprint, const FGuid& InVariableGuid)
{
	// Search through all function entry nodes for a local variable with the passed Guid
	TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
	GetAllNodesOfClass<UK2Node_FunctionEntry>(InBlueprint, FunctionEntryNodes);

	FName ReturnVariableName = NAME_None;
	for (UK2Node_FunctionEntry* const FunctionEntry : FunctionEntryNodes)
	{
		for( FBPVariableDescription& Variable : FunctionEntry->LocalVariables )
		{
			if(Variable.VarGuid == InVariableGuid)
			{
				ReturnVariableName = Variable.VarName;
				break;
			}
		}

		if(ReturnVariableName != NAME_None)
		{
			break;
		}
	}

	return ReturnVariableName;
}

FGuid FBlueprintEditorUtils::FindLocalVariableGuidByName(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName)
{
	FGuid ReturnGuid;
	if(FBPVariableDescription* LocalVariable = FindLocalVariable(InBlueprint, InScope, InVariableName))
	{
		ReturnGuid = LocalVariable->VarGuid;
	}

	return ReturnGuid;
}

FGuid FBlueprintEditorUtils::FindLocalVariableGuidByName(UBlueprint* InBlueprint, const UEdGraph* InScopeGraph, const FName InVariableName)
{
	FGuid ReturnGuid;
	if(FBPVariableDescription* LocalVariable = FindLocalVariable(InBlueprint, InScopeGraph, InVariableName))
	{
		ReturnGuid = LocalVariable->VarGuid;
	}

	return ReturnGuid;
}

void FBlueprintEditorUtils::ChangeLocalVariableType(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName, const FEdGraphPinType& NewPinType)
{
	if (InVariableName != NAME_None)
	{
		FString ActionCategory;
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		UK2Node_FunctionEntry* FunctionEntry = nullptr;
		FBPVariableDescription* VariablePtr = FindLocalVariable(InBlueprint, InScope, InVariableName, &FunctionEntry);

		if(VariablePtr)
		{
			FBPVariableDescription& Variable = *VariablePtr;

			// Update the variable type only if it is different
			if (Variable.VarName == InVariableName && Variable.VarType != NewPinType)
			{
				TArray<UK2Node*> VariableNodes = GetNodesForVariable(InVariableName, InBlueprint, InScope);

				// If there are variable nodes in place, warn the user of the consequences using a suppressible dialog
				if(VariableNodes.Num())
				{
					if(!VerifyUserWantsVariableTypeChanged(InVariableName))
					{
						// User has decided to cancel changing the variable member type
						return;
					}
				}

				const FScopedTransaction Transaction( LOCTEXT("ChangeLocalVariableType", "Change Local Variable Type") );
				InBlueprint->Modify();
				FunctionEntry->Modify();

				Variable.VarType = NewPinType;

				// Reset the default value
				Variable.DefaultValue.Empty();

				// Mark the Blueprint as structurally modified so we can reconstruct the node successfully
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InBlueprint);

				if ((NewPinType.PinCategory == K2Schema->PC_Object) || (NewPinType.PinCategory == K2Schema->PC_Interface))
				{
					// if it's a PC_Object, then it should have an associated UClass object
					if(NewPinType.PinSubCategoryObject.IsValid())
					{
						const UClass* ClassObject = Cast<UClass>(NewPinType.PinSubCategoryObject.Get());
						check(ClassObject != nullptr);

						if (ClassObject->IsChildOf(AActor::StaticClass()))
						{
							// prevent Actor variables from having default values (because Blueprint templates are library elements that can 
							// bridge multiple levels and different levels might not have the actor that the default is referencing).
							Variable.PropertyFlags |= CPF_DisableEditOnTemplate;
						}
						else 
						{
							// clear the disable-default-value flag that might have been present (if this was an AActor variable before)
							Variable.PropertyFlags &= ~(CPF_DisableEditOnTemplate);
						}
					}
				}

				// Reconstruct all local variables referencing the modified one
				for(UK2Node* VariableNode : VariableNodes)
				{
					K2Schema->ReconstructNode(*VariableNode, true);
				}

				TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(InBlueprint);

				// No need to submit a search query if there are no nodes.
				if (FoundAssetEditor.IsValid() && VariableNodes.Num())
				{
					TSharedRef<IBlueprintEditor> BlueprintEditor = StaticCastSharedRef<IBlueprintEditor>(FoundAssetEditor.ToSharedRef());

					UK2Node* FirstVariableNode = nullptr;
					for (UK2Node* VariableNode : VariableNodes)
					{
						if (VariableNode->IsA<UK2Node_Variable>())
						{
							FirstVariableNode = VariableNode;
							break;
						}
					}

					if (FirstVariableNode)
					{
						const bool bSetFindWithinBlueprint = true;
						const bool bSelectFirstResult = false;
						BlueprintEditor->SummonSearchUI(bSetFindWithinBlueprint, VariableNodes[0]->GetFindReferenceSearchString(), bSelectFirstResult);
					}
				}
			}
		}
	}
}

void FBlueprintEditorUtils::ReplaceVariableReferences(UBlueprint* Blueprint, const FName OldName, const FName NewName)
{
	check((OldName != NAME_None) && (NewName != NAME_None));

	::RenameVariableReferences(Blueprint, Blueprint->GeneratedClass, OldName, NewName);

	TArray<UBlueprint*> Dependents;
	GetDependentBlueprints(Blueprint, Dependents);

	for (UBlueprint* DependentBp : Dependents)
	{
		::RenameVariableReferences(DependentBp, Blueprint->GeneratedClass, OldName, NewName);
	}
}

void FBlueprintEditorUtils::ReplaceVariableReferences(UBlueprint* Blueprint, const UProperty* OldVariable, const UProperty* NewVariable)
{
	check((OldVariable != nullptr) && (NewVariable != nullptr));
	ReplaceVariableReferences(Blueprint, OldVariable->GetFName(), NewVariable->GetFName());
}

bool FBlueprintEditorUtils::IsVariableComponent(const FBPVariableDescription& Variable)
{
	// Find the variable in the list
	if( Variable.VarType.PinCategory == FString(TEXT("object")) )
	{
		const UClass* VarClass = Cast<const UClass>(Variable.VarType.PinSubCategoryObject.Get());
		return (VarClass && VarClass->HasAnyClassFlags(CLASS_DefaultToInstanced));
	}

	return false;
}

bool FBlueprintEditorUtils::IsVariableUsed(const UBlueprint* Blueprint, const FName& Name, UEdGraph* LocalGraphScope/* = nullptr*/ )
{
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for(TArray<UEdGraph*>::TConstIterator it(AllGraphs); it; ++it)
	{
		const UEdGraph* CurrentGraph = *it;

		if(CurrentGraph == LocalGraphScope || LocalGraphScope == nullptr)
		{
			TArray<UK2Node_Variable*> GraphNodes;
			CurrentGraph->GetNodesOfClass(GraphNodes);

			for (const UK2Node_Variable* CurrentNode : GraphNodes )
			{
				if(Name == CurrentNode->GetVarName())
				{
					return true;
				}
			}

			// Also consider "used" if there's a GetClassDefaults node that exposes the variable as an output pin that's connected to something.
			TArray<UK2Node_GetClassDefaults*> ClassDefaultsNodes;
			CurrentGraph->GetNodesOfClass(ClassDefaultsNodes);
			for (const UK2Node_GetClassDefaults* ClassDefaultsNode : ClassDefaultsNodes)
			{
				if (ClassDefaultsNode->GetInputClass() == Blueprint->SkeletonGeneratedClass)
				{
					const UEdGraphPin* VarPin = ClassDefaultsNode->FindPin(Name.ToString());
					if (VarPin && VarPin->Direction == EGPD_Output && VarPin->LinkedTo.Num() > 0)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool FBlueprintEditorUtils::ValidateAllMemberVariables(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName InVariableName)
{
	for(int32 VariableIdx = 0; VariableIdx < InBlueprint->NewVariables.Num(); ++VariableIdx)
	{
		if(InBlueprint->NewVariables[VariableIdx].VarName == InVariableName)
		{
			FName NewChildName = FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint, InVariableName.ToString(), InParentBlueprint ? InParentBlueprint->SkeletonGeneratedClass : InBlueprint->ParentClass);

			UE_LOG(LogBlueprint, Warning, TEXT("Blueprint %s (child of/implements %s) has a member variable with a conflicting name (%s). Changing to %s."), *InBlueprint->GetName(), *GetNameSafe(InParentBlueprint), *InVariableName.ToString(), *NewChildName.ToString());

			FBlueprintEditorUtils::RenameMemberVariable(InBlueprint, InBlueprint->NewVariables[VariableIdx].VarName, NewChildName);
			return true;
		}
	}

	return false;
}

bool FBlueprintEditorUtils::ValidateAllComponentMemberVariables(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName& InVariableName)
{
	if(InBlueprint->SimpleConstructionScript != nullptr)
	{
		TArray<USCS_Node*> ChildSCSNodes = InBlueprint->SimpleConstructionScript->GetAllNodes();
		for(int32 NodeIndex = 0; NodeIndex < ChildSCSNodes.Num(); ++NodeIndex)
		{
			USCS_Node* SCS_Node = ChildSCSNodes[NodeIndex];
			if(SCS_Node != nullptr && SCS_Node->GetVariableName() == InVariableName)
			{
				FName NewChildName = FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint, InVariableName.ToString());

				UE_LOG(LogBlueprint, Warning, TEXT("Blueprint %s (child of/implements %s) has a component variable with a conflicting name (%s). Changing to %s."), *InBlueprint->GetName(), *InParentBlueprint->GetName(), *InVariableName.ToString(), *NewChildName.ToString());

				FBlueprintEditorUtils::RenameComponentMemberVariable(InBlueprint, SCS_Node, NewChildName);
				return true;
			}
		}
	}
	return false;
}

bool FBlueprintEditorUtils::ValidateAllTimelines(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName& InVariableName)
{
	for (int32 TimelineIndex=0; TimelineIndex < InBlueprint->Timelines.Num(); ++TimelineIndex)
	{
		UTimelineTemplate* TimelineTemplate = InBlueprint->Timelines[TimelineIndex];
		if( TimelineTemplate )
		{
			if( TimelineTemplate->GetFName() == InVariableName )
			{
				FName NewName = FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint, TimelineTemplate->GetName());
				FBlueprintEditorUtils::RenameTimeline(InBlueprint, TimelineTemplate->GetFName(), NewName);

				UE_LOG(LogBlueprint, Warning, TEXT("Blueprint %s (child of/implements %s) has a timeline with a conflicting name (%s). Changing to %s."), *InBlueprint->GetName(), *InParentBlueprint->GetName(), *InVariableName.ToString(), *NewName.ToString());
				return true;
			}
		}
	}
	return false;
}

bool FBlueprintEditorUtils::ValidateAllFunctionGraphs(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName& InVariableName)
{
	for (int32 FunctionIndex=0; FunctionIndex < InBlueprint->FunctionGraphs.Num(); ++FunctionIndex)
	{
		UEdGraph* FunctionGraph = InBlueprint->FunctionGraphs[FunctionIndex];

		if( FunctionGraph->GetFName() == InVariableName )
		{
			FName NewName = FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint, FunctionGraph->GetName());
			FBlueprintEditorUtils::RenameGraph(FunctionGraph, NewName.ToString());

			UE_LOG(LogBlueprint, Warning, TEXT("Blueprint %s (child of/implements %s) has a function graph with a conflicting name (%s). Changing to %s."), *InBlueprint->GetName(), *InParentBlueprint->GetName(), *InVariableName.ToString(), *NewName.ToString());
			return true;
		}
	}
	return false;
}

void FBlueprintEditorUtils::FixupVariableDescription(UBlueprint* Blueprint, FBPVariableDescription& VarDesc)
{
	if ((VarDesc.PropertyFlags & CPF_Config) != 0 && Blueprint->GetLinkerCustomVersion(FBlueprintsObjectVersion::GUID) < FBlueprintsObjectVersion::DisallowObjectConfigVars)
	{
		// Synchronized with FBlueprintVarActionDetails::IsConfigCheckBoxEnabled
		const FEdGraphPinType& VarType = VarDesc.VarType;
		if (VarType.PinCategory == UEdGraphSchema_K2::PC_Object ||
			VarType.PinCategory == UEdGraphSchema_K2::PC_Interface)
		{
			VarDesc.PropertyFlags &= ~CPF_Config;
		}
	}

	// Remove bitflag enum type metadata if the enum type name is missing or if the enum type is no longer a bitflags type.
	if (VarDesc.HasMetaData(FBlueprintMetadata::MD_BitmaskEnum))
	{
		FString BitmaskEnumTypeName = VarDesc.GetMetaData(FBlueprintMetadata::MD_BitmaskEnum);
		if (!BitmaskEnumTypeName.IsEmpty())
		{
			UEnum* BitflagsEnum = FindObject<UEnum>(ANY_PACKAGE, *BitmaskEnumTypeName);
			if (BitflagsEnum == nullptr || !BitflagsEnum->HasMetaData(*FBlueprintMetadata::MD_Bitflags.ToString()))
			{
				VarDesc.RemoveMetaData(FBlueprintMetadata::MD_BitmaskEnum);
			}
		}
		else
		{
			VarDesc.RemoveMetaData(FBlueprintMetadata::MD_BitmaskEnum);
		}
	}
}

void FBlueprintEditorUtils::ValidateBlueprintChildVariables(UBlueprint* InBlueprint, const FName InVariableName)
{
	// Iterate over currently-loaded Blueprints and potentially adjust their variable names if they conflict with the parent
	for(TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
	{
		UBlueprint* ChildBP = *BlueprintIt;
		if(ChildBP != nullptr && ChildBP->ParentClass != nullptr)
		{
			TArray<UBlueprint*> ParentBPArray;
			// Get the parent hierarchy
			UBlueprint::GetBlueprintHierarchyFromClass(ChildBP->ParentClass, ParentBPArray);

			// Also get any BP interfaces we use
			TArray<UClass*> ImplementedInterfaces;
			FindImplementedInterfaces(ChildBP, true, ImplementedInterfaces);
			for (UClass* ImplementedInterface : ImplementedInterfaces)
			{
				UBlueprint* BlueprintInterfaceClass = UBlueprint::GetBlueprintFromClass(ImplementedInterface);
				if(BlueprintInterfaceClass != nullptr)
				{
					ParentBPArray.Add(BlueprintInterfaceClass);
				}
			}

			if(ParentBPArray.Contains(InBlueprint))
			{
				bool bValidatedVariable = false;

				bValidatedVariable = ValidateAllMemberVariables(ChildBP, InBlueprint, InVariableName);

				if(!bValidatedVariable)
				{
					bValidatedVariable = ValidateAllComponentMemberVariables(ChildBP, InBlueprint, InVariableName);
				}

				if(!bValidatedVariable)
				{
					bValidatedVariable = ValidateAllTimelines(ChildBP, InBlueprint, InVariableName);
				}

				if(!bValidatedVariable)
				{
					bValidatedVariable = ValidateAllFunctionGraphs(ChildBP, InBlueprint, InVariableName);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void FBlueprintEditorUtils::FindNativizationDependencies(UBlueprint* Blueprint, TArray<UClass*>& NativizeDependenciesOut)
{
	FBlueprintEditorUtils::FindImplementedInterfaces(Blueprint, /*bGetAllInterfaces =*/false, NativizeDependenciesOut);
	NativizeDependenciesOut.AddUnique(Blueprint->ParentClass);
}

/** Shared function for posting notification toasts (utilized by the nativization property system) */
static void PostNativizationWarning(const FText& Message)
{
	FNotificationInfo Warning(Message);
	Warning.ExpireDuration = 5.0f;
	Warning.bFireAndForget = true;
	Warning.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
	FSlateNotificationManager::Get().AddNotification(Warning);
}

bool FBlueprintEditorUtils::PropagateNativizationSetting(UBlueprint* Blueprint)
{
	bool bSettingsChanged = false;
	UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

	switch (Blueprint->NativizationFlag)
	{
	case EBlueprintNativizationFlag::Disabled:
		bSettingsChanged |= PackagingSettings->RemoveBlueprintAssetFromNativizationList(Blueprint);
		break;

	case EBlueprintNativizationFlag::ExplicitlyEnabled:
		{
			bSettingsChanged |= PackagingSettings->AddBlueprintAssetToNativizationList(Blueprint);

			TArray<UClass*> NativizationDependencies;
			FindNativizationDependencies(Blueprint, NativizationDependencies);

			bool bAddedDependencies = false;

			for (UClass* Dependency : NativizationDependencies)
			{
				if (UBlueprint* DependencyBp = UBlueprint::GetBlueprintFromClass(Dependency))
				{
					// if the user hasn't manually altered the setting (chosen 
					// for themselves), then let's apply the auto-setting
					if (DependencyBp->NativizationFlag == EBlueprintNativizationFlag::Dependency)
					{
						DependencyBp->NativizationFlag = EBlueprintNativizationFlag::ExplicitlyEnabled;
						// recurse and propagate this setting to dependencies once removed
						bAddedDependencies |= PropagateNativizationSetting(DependencyBp);
					}
					else if (DependencyBp->NativizationFlag == EBlueprintNativizationFlag::ExplicitlyEnabled &&
						!PackagingSettings->IsBlueprintAssetInNativizationList(DependencyBp))
					{
						bAddedDependencies |= PropagateNativizationSetting(DependencyBp);
						// this is a hairy case, because the user could have changes pending to the DependencyBp
						// that they will end up discarding... is it their intension to discard the "nativize"  
						// setting as well? was it set before or after this dependent? maybe they set it before,
						// and want to discard the change, but didn't realize it was a dependency?
						// here we'll favor correctness, and save it to the config now
					}
				}
			}
			bSettingsChanged |= bAddedDependencies;
			if (bAddedDependencies)
			{
				PostNativizationWarning(LOCTEXT("DependenciesSavedForNativization", "Saved extra (required dependency) Blueprints for nativization."));
			}
		}
		break;

	default:
	case EBlueprintNativizationFlag::Dependency:
		// the Blueprint which set this flag is responsible for applying this change
		break;
	}

	return bSettingsChanged;
}

bool FBlueprintEditorUtils::ShouldNativizeImplicitly(const UBlueprint* Blueprint)
{
	if (Blueprint)
	{
		TArray<UK2Node_Event*> AllEventNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Event>(Blueprint, AllEventNodes);

		// Add all events overridden by this Blueprint.
		TArray<FName> CheckFunctionNames;
		for (const UK2Node_Event* EventNode : AllEventNodes)
		{
			if (EventNode->bOverrideFunction)
			{
				CheckFunctionNames.Add(EventNode->EventReference.GetMemberName());
			}
		}

		// Add all function graphs implemented by this Blueprint.
		for (const UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
		{
			CheckFunctionNames.Add(FunctionGraph->GetFName());
		}

		// Check each overridable/callable function defined by all ancestors to see if any names match an implementation found in this Blueprint.
		UClass* ParentClass = Blueprint->SkeletonGeneratedClass ? Blueprint->SkeletonGeneratedClass->GetSuperClass() : *Blueprint->ParentClass;
		for (TFieldIterator<UFunction> FunctionIt(ParentClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			const UFunction* Function = *FunctionIt;
			if (UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && UEdGraphSchema_K2::CanUserKismetCallFunction(Function) && CheckFunctionNames.Contains(Function->GetFName()))
			{
				// This Blueprint overrides a callable event/function. If the function is defined in a parent BP that is flagged for nativization, OR if
				// the parent BP has itself been implicitly flagged for nativization, then this Blueprint will also be implicitly flagged for nativization.
				// Currently, any calls to such a function within a nativized parent hierarchy are not able to invoke an override in a non-nativized child,
				// so the current solution is to implicitly force the child BP to also be nativized along with its parent hierarchy in this particular case.
				const UClass* SignatureClass = CastChecked<UClass>(Function->GetOuter());
				const UBlueprint* ParentBP = UBlueprint::GetBlueprintFromClass(SignatureClass);
				if (ParentBP != nullptr
					&& (ParentBP->NativizationFlag == EBlueprintNativizationFlag::ExplicitlyEnabled || ShouldNativizeImplicitly(ParentBP)))
				{
					return true;
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// Interfaces

FGuid FBlueprintEditorUtils::FindInterfaceFunctionGuid(const UFunction* Function, const UClass* InterfaceClass)
{
	// check if this is a blueprint - only blueprint interfaces can have Guids
	check(Function);
	check(InterfaceClass);
	const UBlueprint* InterfaceBlueprint = Cast<UBlueprint>(InterfaceClass->ClassGeneratedBy);
	if(InterfaceBlueprint != nullptr)
	{
		// find the graph for this function
		TArray<UEdGraph*> InterfaceGraphs;
		InterfaceBlueprint->GetAllGraphs(InterfaceGraphs);

		for (const UEdGraph* InterfaceGraph : InterfaceGraphs)
		{
			if(InterfaceGraph != nullptr && InterfaceGraph->GetFName() == Function->GetFName())
			{
				return InterfaceGraph->GraphGuid;
			}
		}
	}

	return FGuid();
}

// Add a new interface, and member function graphs to the blueprint
bool FBlueprintEditorUtils::ImplementNewInterface(UBlueprint* Blueprint, const FName& InterfaceClassName)
{
	check(InterfaceClassName != NAME_None);

	// Attempt to find the class we want to implement
	UClass* InterfaceClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *InterfaceClassName.ToString());

	// Make sure the class is found, and isn't native (since Blueprints don't necessarily generate native classes.
	check(InterfaceClass);

	// Check to make sure we haven't already implemented it
	for( int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++ )
	{
		if( Blueprint->ImplementedInterfaces[i].Interface == InterfaceClass )
		{
			Blueprint->Message_Warn( FString::Printf (*LOCTEXT("InterfaceAlreadyImplemented", "ImplementNewInterface: Blueprint '%s' already implements the interface called '%s'").ToString(), 
				*Blueprint->GetPathName(), 
				*InterfaceClassName.ToString())
				);
			return false;
		}
	}

	// Make a new entry for this interface
	FBPInterfaceDescription NewInterface;
	NewInterface.Interface = InterfaceClass;

	bool bAllFunctionsAdded = true;

	// Add the graphs for the functions required by this interface
	for( TFieldIterator<UFunction> FunctionIter(InterfaceClass, EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter )
	{
		UFunction* Function = *FunctionIter;
		if( UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function) )
		{
			FName FunctionName = Function->GetFName();
			UEdGraph* FuncGraph = FindObject<UEdGraph>( Blueprint, *(FunctionName.ToString()) );
			if (FuncGraph != nullptr)
			{
				bAllFunctionsAdded = false;

				Blueprint->Message_Error( FString::Printf (*LOCTEXT("InterfaceFunctionConflicts", "ImplementNewInterface: Blueprint '%s' has a function or graph which conflicts with the function %s in the interface called '%s'").ToString(), 
					*Blueprint->GetPathName(), 
					*FunctionName.ToString(), 
					*InterfaceClassName.ToString())
					);
				break;
			}

			UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			NewGraph->bAllowDeletion = false;
			NewGraph->InterfaceGuid = FindInterfaceFunctionGuid(Function, InterfaceClass);

			NewInterface.Graphs.Add(NewGraph);

			FBlueprintEditorUtils::AddInterfaceGraph(Blueprint, NewGraph, InterfaceClass);
		}
	}

	if (bAllFunctionsAdded)
	{
		Blueprint->ImplementedInterfaces.Add(NewInterface);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		if (Blueprint->NativizationFlag != EBlueprintNativizationFlag::Disabled)
		{
			UBlueprint* InterfaceBlueprint = UBlueprint::GetBlueprintFromClass(InterfaceClass);
			if (InterfaceBlueprint && InterfaceBlueprint->NativizationFlag == EBlueprintNativizationFlag::Disabled)
			{
				InterfaceBlueprint->NativizationFlag = EBlueprintNativizationFlag::Dependency;
				PostNativizationWarning(FText::Format(
					LOCTEXT("InterfaceFlaggedForNativization", "{0} flagged for nativization (as a required dependency)."),
					FText::FromName(InterfaceBlueprint->GetFName())
					)
				);
			}
		}
	}
	return bAllFunctionsAdded;
}

// Gets the graphs currently in the blueprint associated with the specified interface
void FBlueprintEditorUtils::GetInterfaceGraphs(UBlueprint* Blueprint, const FName& InterfaceClassName, TArray<UEdGraph*>& ChildGraphs)
{
	ChildGraphs.Empty();

	if( InterfaceClassName == NAME_None )
	{
		return;
	}

	// Find the implemented interface
	for( int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++ )
	{
		if( Blueprint->ImplementedInterfaces[i].Interface->GetFName() == InterfaceClassName )
		{
			ChildGraphs = Blueprint->ImplementedInterfaces[i].Graphs;
			return;			
		}
	}
}

// Remove an implemented interface, and its associated member function graphs
void FBlueprintEditorUtils::RemoveInterface(UBlueprint* Blueprint, const FName& InterfaceClassName, bool bPreserveFunctions /*= false*/)
{
	if( InterfaceClassName == NAME_None )
	{
		return;
	}

	// Find the implemented interface
	int32 Idx = INDEX_NONE;
	for( int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); i++ )
	{
		if( Blueprint->ImplementedInterfaces[i].Interface->GetFName() == InterfaceClassName )
		{
			Idx = i;
			break;
		}
	}

	if( Idx != INDEX_NONE )
	{
		FBPInterfaceDescription& CurrentInterface = Blueprint->ImplementedInterfaces[Idx];

		// Remove all the graphs that we implemented
		for(TArray<UEdGraph*>::TIterator it(CurrentInterface.Graphs); it; ++it)
		{
			UEdGraph* CurrentGraph = *it;
			if(bPreserveFunctions)
			{
				PromoteGraphFromInterfaceOverride(Blueprint, CurrentGraph);
				Blueprint->FunctionGraphs.Add(CurrentGraph);
			}
			else
			{
				FBlueprintEditorUtils::RemoveGraph(Blueprint, CurrentGraph, EGraphRemoveFlags::MarkTransient);	// Do not recompile, yet*
			}
		}

		// Find all events placed in the event graph, and remove them
		TArray<UK2Node_Event*> AllEvents;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllEvents);
		const UClass* InterfaceClass = Blueprint->ImplementedInterfaces[Idx].Interface;
		for(TArray<UK2Node_Event*>::TIterator NodeIt(AllEvents); NodeIt; ++NodeIt)
		{
			UK2Node_Event* EventNode = *NodeIt;
			if( EventNode->EventReference.GetMemberParentClass(EventNode->GetBlueprintClassFromNode()) == InterfaceClass )
			{
				if(bPreserveFunctions)
				{
					// Create a custom event with the same name and signature
					const FVector2D PreviousNodePos = FVector2D(EventNode->NodePosX, EventNode->NodePosY);
					const FString PreviousNodeName = EventNode->EventReference.GetMemberName().ToString();
					const UFunction* PreviousSignatureFunction = EventNode->FindEventSignatureFunction();
					check(PreviousSignatureFunction);
					
					UK2Node_CustomEvent* NewEvent = UK2Node_CustomEvent::CreateFromFunction(PreviousNodePos, EventNode->GetGraph(), PreviousNodeName, PreviousSignatureFunction, false);

					// Move the pin links from the old pin to the new pin to preserve connections
					for (UEdGraphPin* CurrentPin : EventNode->Pins)
					{
						UEdGraphPin* TargetPin = NewEvent->FindPinChecked(CurrentPin->PinName);
						const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
						Schema->MovePinLinks(*CurrentPin, *TargetPin);
					}
				}
			
				EventNode->GetGraph()->RemoveNode(EventNode);
			}
		}

		// Then remove the interface from the list
		Blueprint->ImplementedInterfaces.RemoveAt(Idx, 1);
	
		// *Now recompile the blueprint (this needs to be done outside of RemoveGraph, after it's been removed from ImplementedInterfaces - otherwise it'll re-add it)
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

void FBlueprintEditorUtils::PromoteGraphFromInterfaceOverride(UBlueprint* InBlueprint, UEdGraph* InInterfaceGraph)
{
	InInterfaceGraph->bAllowDeletion = true;
	InInterfaceGraph->bAllowRenaming = true;
	InInterfaceGraph->bEditable = true;
	InInterfaceGraph->InterfaceGuid.Invalidate();

	// We need to flag the entry node to make sure that the compiled function is callable
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	Schema->AddExtraFunctionFlags(InInterfaceGraph, (FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public));
	Schema->MarkFunctionEntryAsEditable(InInterfaceGraph, true);

	// Move all non-exec pins from the function entry node to being user defined pins
	TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
	InInterfaceGraph->GetNodesOfClass(FunctionEntryNodes);
	if (FunctionEntryNodes.Num() > 0)
	{
		UK2Node_FunctionEntry* FunctionEntry = FunctionEntryNodes[0];
		FunctionEntry->PromoteFromInterfaceOverride();
	}

	// Move all non-exec pins from the function result node to being user defined pins
	TArray<UK2Node_FunctionResult*> FunctionResultNodes;
	InInterfaceGraph->GetNodesOfClass(FunctionResultNodes);
	if (FunctionResultNodes.Num() > 0)
	{
		UK2Node_FunctionResult* PrimaryFunctionResult = FunctionResultNodes[0];
		PrimaryFunctionResult->PromoteFromInterfaceOverride();

		// Reconstruct all result nodes so they update their pins accordingly
		for (UK2Node_FunctionResult* FunctionResult : FunctionResultNodes)
		{
			if (PrimaryFunctionResult != FunctionResult)
			{
				FunctionResult->PromoteFromInterfaceOverride(false);
			}
		}
	}
}

void FBlueprintEditorUtils::CleanNullGraphReferencesRecursive(UEdGraph* Graph)
{
	for (int32 GraphIndex = 0; GraphIndex < Graph->SubGraphs.Num(); )
	{
		if (UEdGraph* ChildGraph = Graph->SubGraphs[GraphIndex])
		{
			CleanNullGraphReferencesRecursive(ChildGraph);
			++GraphIndex;
		}
		else
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Found NULL graph reference in children of '%s', removing it!"), *Graph->GetPathName());
			Graph->SubGraphs.RemoveAt(GraphIndex);
		}
	}
}

void FBlueprintEditorUtils::CleanNullGraphReferencesInArray(UBlueprint* Blueprint, TArray<UEdGraph*>& GraphArray)
{
	for (int32 GraphIndex = 0; GraphIndex < GraphArray.Num(); )
	{
		if (UEdGraph* Graph = GraphArray[GraphIndex])
		{
			CleanNullGraphReferencesRecursive(Graph);
			++GraphIndex;
		}
		else
		{
			UE_LOG(LogBlueprint, Warning, TEXT("Found NULL graph reference in '%s', removing it!"), *Blueprint->GetPathName());
			GraphArray.RemoveAt(GraphIndex);
		}
	}
}

void FBlueprintEditorUtils::PurgeNullGraphs(UBlueprint* Blueprint)
{
	CleanNullGraphReferencesInArray(Blueprint, Blueprint->UbergraphPages);
	CleanNullGraphReferencesInArray(Blueprint, Blueprint->FunctionGraphs);
	CleanNullGraphReferencesInArray(Blueprint, Blueprint->DelegateSignatureGraphs);
	CleanNullGraphReferencesInArray(Blueprint, Blueprint->MacroGraphs);

	Blueprint->LastEditedDocuments.RemoveAll([](const FEditedDocumentInfo& TestDoc) { return TestDoc.EditedObject == nullptr; });
}

// Makes sure that calls to parent functions are valid, and removes them if not
void FBlueprintEditorUtils::ConformCallsToParentFunctions(UBlueprint* Blueprint)
{
	check(nullptr != Blueprint);

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for(int GraphIndex = 0; GraphIndex < AllGraphs.Num(); ++GraphIndex)
	{
		UEdGraph* CurrentGraph = AllGraphs[GraphIndex];
		check(nullptr != CurrentGraph);

		// Make sure the graph is loaded
		if(!CurrentGraph->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad))
		{
			TArray<UK2Node_CallParentFunction*> CallFunctionNodes;
			CurrentGraph->GetNodesOfClass<UK2Node_CallParentFunction>(CallFunctionNodes);

			// For each parent function call node in the graph
			for(int CallFunctionNodeIndex = 0; CallFunctionNodeIndex < CallFunctionNodes.Num(); ++CallFunctionNodeIndex)
			{
				UK2Node_CallParentFunction* CallFunctionNode = CallFunctionNodes[CallFunctionNodeIndex];
				check(nullptr != CallFunctionNode);

				// Make sure the node has already been loaded
				if(!CallFunctionNode->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad))
				{
					// Attempt to locate the function within the parent class
					const UFunction* TargetFunction = CallFunctionNode->GetTargetFunction();
					TargetFunction = TargetFunction && Blueprint->ParentClass ? Blueprint->ParentClass->FindFunctionByName(TargetFunction->GetFName()) : nullptr;
					if(TargetFunction)
					{
						// If the function signature does not match the parent class
						if(TargetFunction->GetOwnerClass() != CallFunctionNode->FunctionReference.GetMemberParentClass(Blueprint->ParentClass))
						{
							// Emit something to the log to indicate that we're making a change
							FFormatNamedArguments Args;
							Args.Add(TEXT("NodeTitle"), CallFunctionNode->GetNodeTitle(ENodeTitleType::ListView));
							Args.Add(TEXT("FunctionNodeName"), FText::FromString(CallFunctionNode->GetName()));
							Blueprint->Message_Note(FText::Format(LOCTEXT("CallParentFunctionSignatureFixed_Note", "{NodeTitle} ({FunctionNodeName}) had an invalid function signature - it has now been fixed."), Args).ToString() );

							// Redirect to the correct parent function
							CallFunctionNode->SetFromFunction(TargetFunction);
						}
					}
					else
					{
						// Cache a reference to the output exec pin
						UEdGraphPin* OutputPin = CallFunctionNode->GetThenPin();

						// We're going to destroy the existing parent function call node, but first we need to persist any existing connections
						for(int PinIndex = 0; PinIndex < CallFunctionNode->Pins.Num(); ++PinIndex)
						{
							UEdGraphPin* InputPin = CallFunctionNode->Pins[PinIndex];
							check(nullptr != InputPin);

							// If this is an input exec pin
							const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
							if(K2Schema && K2Schema->IsExecPin(*InputPin) && InputPin->Direction == EGPD_Input)
							{
								// Redirect any existing connections to the input exec pin to whatever pin(s) the output exec pin is connected to
								for(int InputLinkedToPinIndex = 0; InputLinkedToPinIndex < InputPin->LinkedTo.Num(); ++InputLinkedToPinIndex)
								{
									UEdGraphPin* InputLinkedToPin = InputPin->LinkedTo[InputLinkedToPinIndex];
									check(nullptr != InputLinkedToPin);

									// Break the existing link to the node we're about to remove
									InputLinkedToPin->BreakLinkTo(InputPin);

									// Redirect the input connection to the output connection(s)
									for(int OutputLinkedToPinIndex = 0; OutputLinkedToPinIndex < OutputPin->LinkedTo.Num(); ++OutputLinkedToPinIndex)
									{
										UEdGraphPin* OutputLinkedToPin = OutputPin->LinkedTo[OutputLinkedToPinIndex];
										check(nullptr != OutputLinkedToPin);

										// Make sure the output connection isn't linked to the node we're about to remove
										if(OutputLinkedToPin->LinkedTo.Contains(OutputPin))
										{
											OutputLinkedToPin->BreakLinkTo(OutputPin);
										}
										
										// Fix up the connection
										InputLinkedToPin->MakeLinkTo(OutputLinkedToPin);
									}
								}
							}
						}

						// Emit something to the log to indicate that we're making a change
						FFormatNamedArguments Args;
						Args.Add(TEXT("NodeTitle"), CallFunctionNode->GetNodeTitle(ENodeTitleType::ListView));
						Args.Add(TEXT("FunctionNodeName"), FText::FromString(CallFunctionNode->GetName()));
						Blueprint->Message_Note( FText::Format(LOCTEXT("CallParentNodeRemoved_Note", "{NodeTitle} ({FunctionNodeName}) was not valid for this Blueprint - it has been removed."), Args).ToString() );

						// Destroy the existing parent function call node (this will also break pin links and remove it from the graph)
						CallFunctionNode->DestroyNode();
					}
				}
			}
		}
	}
}

namespace
{
	static bool ExtendedIsParent(const UClass* Parent, const UClass* Child)
	{
		if (Parent && Child)
		{
			if (Child->IsChildOf(Parent))
			{
				return true;
			}

			if (Parent->ClassGeneratedBy)
			{
				if (Parent->ClassGeneratedBy == Child->ClassGeneratedBy)
				{
					return true;
				}

				if (const UBlueprint* ParentBP = Cast<UBlueprint>(Parent->ClassGeneratedBy))
				{
					if (ParentBP->SkeletonGeneratedClass && Child->IsChildOf(ParentBP->SkeletonGeneratedClass))
					{
						return true;
					}

					if (ParentBP->GeneratedClass && Child->IsChildOf(ParentBP->GeneratedClass))
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	static void FixOverriddenEventSignature(UK2Node_Event* EventNode, UBlueprint* Blueprint, UEdGraph* CurrentGraph)
	{
		check(EventNode && Blueprint && CurrentGraph);
		UClass* CurrentClass = EventNode->GetBlueprintClassFromNode();
		FMemberReference& FuncRef = EventNode->EventReference;
		const FName EventFuncName = FuncRef.GetMemberName();
		ensure(EventFuncName != NAME_None);
		ensure(!EventNode->IsA<UK2Node_CustomEvent>());

		const UFunction* TargetFunction = FuncRef.ResolveMember<UFunction>(CurrentClass);
		const UClass* FuncOwnerClass = FuncRef.GetMemberParentClass(CurrentClass);
		const bool bFunctionOwnerIsNotParentOfClass = !FuncOwnerClass || !ExtendedIsParent(FuncOwnerClass, CurrentClass);
		const bool bNeedsToBeFixed = !TargetFunction || bFunctionOwnerIsNotParentOfClass;
		if (bNeedsToBeFixed)
		{
			const UClass* SuperClass = CurrentClass->GetSuperClass();
			const UFunction* ActualTargetFunction = SuperClass ? SuperClass->FindFunctionByName(EventFuncName) : nullptr;
			if (ActualTargetFunction)
			{
				ensure(TargetFunction != ActualTargetFunction);
				if (!ensure(!TargetFunction || TargetFunction->IsSignatureCompatibleWith(ActualTargetFunction)))
				{
					UE_LOG(LogBlueprint, Error
						, TEXT("FixOverriddenEventSignature function \"%s\" is not compatible with \"%s\" node \"%s\"")
						, *GetPathNameSafe(ActualTargetFunction), *GetPathNameSafe(TargetFunction), *GetPathNameSafe(EventNode));
				}

				ensure(GetDefault<UEdGraphSchema_K2>()->FunctionCanBePlacedAsEvent(ActualTargetFunction));
				FuncRef.SetFromField<UFunction>(ActualTargetFunction, false);

				// Emit something to the log to indicate that we've made a change
				FFormatNamedArguments Args;
				Args.Add(TEXT("NodeTitle"), EventNode->GetNodeTitle(ENodeTitleType::ListView));
				Args.Add(TEXT("EventNodeName"), FText::FromString(EventNode->GetName()));
				Blueprint->Message_Note(FText::Format(LOCTEXT("EventSignatureFixed_Note", "{NodeTitle} ({EventNodeName}) had an invalid function signature - it has now been fixed."), Args).ToString());
			}
			else
			{
				TSet<FName> DummyExtraNameList;
				UEdGraphNode* CustomEventNode = CurrentGraph->GetSchema()->CreateSubstituteNode(EventNode, CurrentGraph, nullptr, DummyExtraNameList);
				if (ensure(CustomEventNode))
				{
					// Destroy the old event node (this will also break all pin links and remove it from the graph)
					EventNode->DestroyNode();
					// Add the new custom event node to the graph
					CurrentGraph->Nodes.Add(CustomEventNode);
					// Emit something to the log to indicate that we've made a change
					FFormatNamedArguments Args;
					Args.Add(TEXT("NodeTitle"), EventNode->GetNodeTitle(ENodeTitleType::ListView));
					Args.Add(TEXT("EventNodeName"), FText::FromString(EventNode->GetName()));
					Blueprint->Message_Note(FText::Format(LOCTEXT("EventNodeReplaced_Note", "{NodeTitle} ({EventNodeName}) was not valid for this Blueprint - it has been converted to a custom event."), Args).ToString());
				}
			}
		}
	}
}

// Makes sure that all events we handle exist, and replace with custom events if not
void FBlueprintEditorUtils::ConformImplementedEvents(UBlueprint* Blueprint)
{
	check(nullptr != Blueprint);

	// Collect all implemented interface classes
	TArray<UClass*> ImplementedInterfaceClasses;
	FindImplementedInterfaces(Blueprint, true, ImplementedInterfaceClasses);

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for(int GraphIndex = 0; GraphIndex < AllGraphs.Num(); ++GraphIndex)
	{
		UEdGraph* CurrentGraph = AllGraphs[GraphIndex];
		check(nullptr != CurrentGraph);

		// Make sure the graph is loaded
		if(!CurrentGraph->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad))
		{
			TArray<UK2Node_Event*> EventNodes;
			CurrentGraph->GetNodesOfClass<UK2Node_Event>(EventNodes);

			// For each event node in the graph
			for(int EventNodeIndex = 0; EventNodeIndex < EventNodes.Num(); ++EventNodeIndex)
			{
				UK2Node_Event* EventNode = EventNodes[EventNodeIndex];
				check(nullptr != EventNode);

				// If the event is loaded and is not a custom event
				if(!EventNode->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad) && EventNode->bOverrideFunction)
				{
					UClass* EventClass = EventNode->EventReference.GetMemberParentClass(EventNode->GetBlueprintClassFromNode());
					bool bEventNodeUsedByInterface = false;
					int32 Idx = 0;
					while (Idx != ImplementedInterfaceClasses.Num() && bEventNodeUsedByInterface == false)
					{
						const UClass* CurrentInterface = ImplementedInterfaceClasses[Idx];
						while (CurrentInterface)
						{
							if (EventClass == CurrentInterface )
							{
								bEventNodeUsedByInterface = true;
								break;
							}
							CurrentInterface = CurrentInterface->GetSuperClass();
						}
						++Idx;
					}
					if (Blueprint->GeneratedClass && !bEventNodeUsedByInterface)
					{
						FixOverriddenEventSignature(EventNode, Blueprint, CurrentGraph);
					}
				}
			}
		}
	}
}

/** Helper function for ConformImplementedInterfaces */
static void ConformInterfaceByGUID(const UBlueprint* Blueprint, const FBPInterfaceDescription& CurrentInterfaceDesc)
{
	// Attempt to conform by GUID if we have a blueprint interface
	// This just make sure that GUID-linked functions preserve their names
	const UBlueprint* InterfaceBlueprint = CastChecked<UBlueprint>(CurrentInterfaceDesc.Interface->ClassGeneratedBy);

	TArray<UEdGraph*> InterfaceGraphs;
	InterfaceBlueprint->GetAllGraphs(InterfaceGraphs);

	TArray<UEdGraph*> BlueprintGraphs;
	Blueprint->GetAllGraphs(BlueprintGraphs);
		
	for (UEdGraph* BlueprintGraph : BlueprintGraphs)
	{
		if(BlueprintGraph != nullptr && BlueprintGraph->InterfaceGuid.IsValid())
		{
			// valid interface Guid found, so fixup name if it is different
			for (const UEdGraph* InterfaceGraph : InterfaceGraphs)
			{
				if(InterfaceGraph != nullptr && InterfaceGraph->GraphGuid == BlueprintGraph->InterfaceGuid && InterfaceGraph->GetFName() != BlueprintGraph->GetFName())
				{
					FBlueprintEditorUtils::RenameGraph(BlueprintGraph, InterfaceGraph->GetFName().ToString());
					FBlueprintEditorUtils::RefreshGraphNodes(BlueprintGraph);
					break;
				}
			}
		}
	}
}

/** Helper function for ConformImplementedInterfaces */
static void ConformInterfaceByName(UBlueprint* Blueprint, FBPInterfaceDescription& CurrentInterfaceDesc, int32 InterfaceIndex, TArray<UK2Node_Event*>& ImplementedEvents, const TArray<FName>& VariableNamesUsedInBlueprint)
{
	// Iterate over all the functions in the interface, and create graphs that are in the interface, but missing in the blueprint
	if (CurrentInterfaceDesc.Interface)
	{
		// a interface could have since been added by the parent (or this blueprint could have been re-parented)
		if (IsInterfaceImplementedByParent(CurrentInterfaceDesc, Blueprint))
		{			
			// have to remove the interface before we promote it (in case this method is reentrant)
			FBPInterfaceDescription LocalInterfaceCopy = CurrentInterfaceDesc;
			Blueprint->ImplementedInterfaces.RemoveAt(InterfaceIndex, 1);

			// in this case, the interface needs to belong to the parent and not this
			// blueprint (we would have been prevented from getting in this state if we
			// had started with a parent that implemented this interface initially)
			PromoteInterfaceImplementationToOverride(LocalInterfaceCopy, Blueprint);
			return;
		}

		// check to make sure that there aren't any interface methods that we originally 
		// implemented as events, but have since switched to functions 
		TSet<FName> ExtraNameList;
		for (UK2Node_Event* EventNode : ImplementedEvents)
		{
			// if this event belongs to something other than this interface
			if (EventNode->EventReference.GetMemberParentClass(EventNode->GetBlueprintClassFromNode()) != CurrentInterfaceDesc.Interface)
			{
				continue;
			}

			UFunction* InterfaceFunction = EventNode->EventReference.ResolveMember<UFunction>(CurrentInterfaceDesc.Interface);
			// if the function is still ok as an event, no need to try and fix it up
			if (UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(InterfaceFunction))
			{
				continue;
			}

			UEdGraph* EventGraph = EventNode->GetGraph();
			// we've already implemented this interface function as an event (which we need to replace)
			UK2Node_CustomEvent* CustomEventNode = Cast<UK2Node_CustomEvent>(EventGraph->GetSchema()->CreateSubstituteNode(EventNode, EventGraph, nullptr, ExtraNameList));
			if (CustomEventNode == nullptr)
			{
				continue;
			}

			// grab the function's name before we delete the node
			FName const FunctionName = EventNode->EventReference.GetMemberName();
			// destroy the old event node (this will also break all pin links and remove it from the graph)
			EventNode->DestroyNode();

			if (InterfaceFunction)
			{
				// have to rename so it doesn't conflict with the graph we're about to add
				CustomEventNode->RenameCustomEventCloseToName();
			}
			EventGraph->Nodes.Add(CustomEventNode);

			// warn the user that their old functionality won't work (it's now connected 
			// to a custom node that isn't triggered anywhere)
			FText WarningMessageText = InterfaceFunction ?
				LOCTEXT("InterfaceEventNodeReplaced_Warn", "'%s' was promoted from an event to a function - it has been replaced by a custom event, which won't trigger unless you call it manually.") :
				LOCTEXT("InterfaceEventRemovedNodeReplaced_Warn", "'%s' was removed from its interface - it has been replaced by a custom event, which won't trigger unless you call it manually.");

			Blueprint->Message_Warn(FString::Printf(*WarningMessageText.ToString(), *FunctionName.ToString()));
		}

		// Cache off the graph names for this interface, for easier searching
		TMap<FName, const UEdGraph*> InterfaceFunctionGraphs;
		for (int32 GraphIndex = 0; GraphIndex < CurrentInterfaceDesc.Graphs.Num(); GraphIndex++)
		{
			const UEdGraph* CurrentGraph = CurrentInterfaceDesc.Graphs[GraphIndex];
			if( CurrentGraph )
			{
				InterfaceFunctionGraphs.Add(CurrentGraph->GetFName()) = CurrentGraph;
			}
		}

		// If this is a Blueprint interface, redirect to the skeleton class for function iteration
		const UClass* InterfaceClass = CurrentInterfaceDesc.Interface;
		if (InterfaceClass && InterfaceClass->ClassGeneratedBy)
		{
			InterfaceClass = CastChecked<UBlueprint>(InterfaceClass->ClassGeneratedBy)->SkeletonGeneratedClass;
		}

		// Iterate over all the functions in the interface, and create graphs that are in the interface, but missing in the blueprint
		for (TFieldIterator<UFunction> FunctionIter(InterfaceClass, EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter)
		{
			UFunction* Function = *FunctionIter;
			const FName FunctionName = Function->GetFName();
			if(!VariableNamesUsedInBlueprint.Contains(FunctionName))
			{
				if( UEdGraphSchema_K2::CanKismetOverrideFunction(Function) && !UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(Function) )
				{
					if (const UEdGraph** FunctionGraphPtr = InterfaceFunctionGraphs.Find(FunctionName))
					{
						const bool bIsConstInterfaceFunction = (Function->FunctionFlags & FUNC_Const) != 0;

						// Sync the 'const' attribute of the implementation with the interface function, in case it has been changed
						TArray<UK2Node_FunctionEntry*> FunctionEntryNodes;
						(*FunctionGraphPtr)->GetNodesOfClass<UK2Node_FunctionEntry>(FunctionEntryNodes);
						for (UK2Node_FunctionEntry* FunctionEntryNode : FunctionEntryNodes)
						{
							const bool bIsImplementedAsConstFunction = (FunctionEntryNode->GetExtraFlags() & FUNC_Const) != 0;
							if (bIsImplementedAsConstFunction != bIsConstInterfaceFunction)
							{
								FunctionEntryNode->Modify();
								if (bIsConstInterfaceFunction)
								{
									FunctionEntryNode->AddExtraFlags(FUNC_Const);
								}
								else
								{
									FunctionEntryNode->ClearExtraFlags(FUNC_Const);
								}
							}
						}
					}
					else
					{
						// interface methods initially create EventGraph stubs, so we need
						// to make sure we remove that entry so the new graph doesn't conflict (don't
						// worry, these are regenerated towards the end of a compile)
						for (UEdGraph* GraphStub : Blueprint->EventGraphs)
						{
							if (GraphStub->GetFName() == FunctionName)
							{
								FBlueprintEditorUtils::RemoveGraph(Blueprint, GraphStub, EGraphRemoveFlags::MarkTransient);
							}
						}

						// Check to see if we already have implemented 
						UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
						NewGraph->bAllowDeletion = false;
						NewGraph->InterfaceGuid = FBlueprintEditorUtils::FindInterfaceFunctionGuid(Function, CurrentInterfaceDesc.Interface);
						CurrentInterfaceDesc.Graphs.Add(NewGraph);

						FBlueprintEditorUtils::AddInterfaceGraph(Blueprint, NewGraph, CurrentInterfaceDesc.Interface);
					}
				}
			}
			else
			{
				Blueprint->Status = BS_Error;
				const FString NewError = FString::Printf( *LOCTEXT("InterfaceNameCollision_Error", "Interface name collision in blueprint: %s, interface: %s, name: %s").ToString(), 
					*Blueprint->GetFullName(), *CurrentInterfaceDesc.Interface->GetFullName(), *FunctionName.ToString());
				Blueprint->Message_Error(NewError);
			}
		}

		// Iterate over all the graphs in the blueprint interface, and remove ones that no longer have functions 
		for (int32 GraphIndex = 0; GraphIndex < CurrentInterfaceDesc.Graphs.Num(); GraphIndex++)
		{
			// If we can't find the function associated with the graph, delete it
			const UEdGraph* CurrentGraph = CurrentInterfaceDesc.Graphs[GraphIndex];
			if (!CurrentGraph || !FindField<UFunction>(CurrentInterfaceDesc.Interface, CurrentGraph->GetFName()))
			{
				CurrentInterfaceDesc.Graphs.RemoveAt(GraphIndex, 1);
				GraphIndex--;
			}
		}
	}
}

// Makes sure that all graphs for all interfaces we implement exist, and add if not
void FBlueprintEditorUtils::ConformImplementedInterfaces(UBlueprint* Blueprint)
{
	check(nullptr != Blueprint);
	FString ErrorStr;

	// Collect all variables names in current blueprint 
	TArray<FName> VariableNamesUsedInBlueprint;
	for (TFieldIterator<UProperty> VariablesIter(Blueprint->GeneratedClass); VariablesIter; ++VariablesIter)
	{
		VariableNamesUsedInBlueprint.Add(VariablesIter->GetFName());
	}
	for (const FBPVariableDescription& NewVariable : Blueprint->NewVariables)
	{
		VariableNamesUsedInBlueprint.AddUnique(NewVariable.VarName);
	}

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	// collect all existing event nodes, so we can find interface events that 
	// need to be converted to function graphs
	TArray<UK2Node_Event*> PotentialInterfaceEvents;
	for (UEdGraph const* Graph : AllGraphs)
	{
		UClass* InterfaceEventClass = UK2Node_Event::StaticClass();
		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			// interface event nodes are only ever going to be implemented as 
			// explicit UK2Node_Events... using == instead of IsChildOf<> 
			// guarantees that we won't be catching any special node types that
			// users might have made (that maybe reference interface functions too)
			if (GraphNode && (GraphNode->GetClass() == InterfaceEventClass))
			{
				PotentialInterfaceEvents.Add(CastChecked<UK2Node_Event>(GraphNode));
			}
		}
	}

	for (int32 InterfaceIndex = 0; InterfaceIndex < Blueprint->ImplementedInterfaces.Num(); )
	{
		FBPInterfaceDescription& CurrentInterface = Blueprint->ImplementedInterfaces[InterfaceIndex];
		if (!CurrentInterface.Interface)
		{
			Blueprint->Status = BS_Error;
			Blueprint->ImplementedInterfaces.RemoveAt(InterfaceIndex, 1);
			continue;
		}

		// conform functions linked by Guids first
		if(CurrentInterface.Interface->ClassGeneratedBy != nullptr && CurrentInterface.Interface->ClassGeneratedBy->IsA(UBlueprint::StaticClass()))
		{
			ConformInterfaceByGUID(Blueprint, CurrentInterface);
		}

		// now try to conform by name/signature
		ConformInterfaceByName(Blueprint, CurrentInterface, InterfaceIndex, PotentialInterfaceEvents, VariableNamesUsedInBlueprint);

		// not going to remove this interface, so let's continue forward
		++InterfaceIndex;
	}
}

void FBlueprintEditorUtils::ConformAllowDeletionFlag(UBlueprint* Blueprint)
{
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph->GetFName() != UEdGraphSchema_K2::FN_UserConstructionScript && Graph->GetFName() != UEdGraphSchema_K2::GN_AnimGraph)
		{
			Graph->bAllowDeletion = true;
		}
	}
}

/** Handle old Anim Blueprints (state machines in the wrong position, transition graphs with the wrong schema, etc...) */
void FBlueprintEditorUtils::UpdateOutOfDateAnimBlueprints(UBlueprint* InBlueprint)
{
	if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(InBlueprint))
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		// Ensure all transition graphs have the correct schema
		TArray<UAnimStateTransitionNode*> TransitionNodes;
		GetAllNodesOfClass<UAnimStateTransitionNode>(AnimBlueprint, /*out*/ TransitionNodes);
		for (UAnimStateTransitionNode* Node : TransitionNodes)
		{
			UEdGraph* TestGraph = Node->BoundGraph;
			if (TestGraph->Schema == UAnimationGraphSchema::StaticClass())
			{
				TestGraph->Schema = UAnimationTransitionSchema::StaticClass();
			}
		}

		// Handle a reparented anim blueprint that either needs or no longer needs an anim graph
		if (UAnimBlueprint::FindRootAnimBlueprint(AnimBlueprint) == nullptr)
		{
			// Add an anim graph if not present
			if (FindObject<UEdGraph>(AnimBlueprint, *(K2Schema->GN_AnimGraph.ToString())) == nullptr)
			{
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(AnimBlueprint, K2Schema->GN_AnimGraph, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
				FBlueprintEditorUtils::AddDomainSpecificGraph(AnimBlueprint, NewGraph);
				AnimBlueprint->LastEditedDocuments.Add(NewGraph);
				NewGraph->bAllowDeletion = false;
			}
		}
		else
		{
			// Remove an anim graph if present
			for (int32 i = 0; i < AnimBlueprint->FunctionGraphs.Num(); ++i)
			{
				UEdGraph* FuncGraph = AnimBlueprint->FunctionGraphs[i];
				if ((FuncGraph != nullptr) && (FuncGraph->GetFName() == K2Schema->GN_AnimGraph))
				{
					UE_LOG(LogBlueprint, Log, TEXT("!!! Removing AnimGraph from %s, because it has a parent anim blueprint that defines the AnimGraph"), *AnimBlueprint->GetPathName());
					AnimBlueprint->FunctionGraphs.RemoveAt(i);
					break;
				}
			}
		}
	}
}

void FBlueprintEditorUtils::UpdateOutOfDateCompositeNodes(UBlueprint* Blueprint)
{
	for (UEdGraph* UbergraphPage : Blueprint->UbergraphPages)
	{
		UpdateOutOfDateCompositeWithOuter(Blueprint, UbergraphPage);
	}
	for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
	{
		UpdateOutOfDateCompositeWithOuter(Blueprint, FunctionGraph);
	}
}

void FBlueprintEditorUtils::UpdateOutOfDateCompositeWithOuter(UBlueprint* Blueprint, UEdGraph* OuterGraph )
{
	check(OuterGraph != nullptr);
	check(FindBlueprintForGraphChecked(OuterGraph) == Blueprint);

	for (UEdGraphNode* Node : OuterGraph->Nodes)
	{
		//Is this node of a type that has a BoundGraph to update
		UEdGraph* BoundGraph = nullptr;
		if (UK2Node_Composite* Composite = Cast<UK2Node_Composite>(Node))
		{
			BoundGraph = Composite->BoundGraph;
		}
		else if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			BoundGraph = StateNode->BoundGraph;
		}
		else if (UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node))
		{
			BoundGraph = TransitionNode->BoundGraph;
		}
		else if (UAnimGraphNode_StateMachineBase* StateMachineNode = Cast<UAnimGraphNode_StateMachineBase>(Node))
		{
			BoundGraph = StateMachineNode->EditorStateMachineGraph;
		}

		if (BoundGraph)
		{
			// Check for out of date BoundGraph where outer is not the composite node
			if (BoundGraph->GetOuter() != Node)
			{
				// change the outer of the BoundGraph to be the composite node instead of the OuterGraph
				if (false == BoundGraph->Rename(*BoundGraph->GetName(), Node, ((BoundGraph->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad) ? REN_ForceNoResetLoaders : 0) | REN_DontCreateRedirectors)))
				{
					UE_LOG(LogBlueprintDebug, Log, TEXT("CompositeNode: On Blueprint '%s' could not fix Outer() for BoundGraph of composite node '%s'"), *Blueprint->GetPathName(), *Node->GetName());
				}
			}
		}
	}

	for (UEdGraph* SubGraph : OuterGraph->SubGraphs)
	{
		UpdateOutOfDateCompositeWithOuter(Blueprint, SubGraph);
	}
}

/** Ensure all component templates are in use */
void FBlueprintEditorUtils::UpdateComponentTemplates(UBlueprint* Blueprint)
{
	TArray<UActorComponent*> ReferencedTemplates;

	TArray<UK2Node_AddComponent*> AllComponents;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllComponents);

	for (UK2Node_AddComponent* ComponentNode : AllComponents)
	{
		check(ComponentNode);

		UActorComponent* ActorComp = ComponentNode->GetTemplateFromNode();
		if (ActorComp)
		{
			ensure(Blueprint->ComponentTemplates.Contains(ActorComp));

			// fix up AddComponent nodes that don't have their own unique template objects
			if (ReferencedTemplates.Contains(ActorComp))
			{
				UE_LOG(LogBlueprint, Warning,
					TEXT("Blueprint '%s' has an AddComponent node '%s' with a non-unique component template name (%s). Moving it to a new template object with a unique name. Re-save the Blueprint to remove this warning on the next load."),
					*Blueprint->GetPathName(), *ComponentNode->GetPathName(), *ActorComp->GetName());

				ComponentNode->MakeNewComponentTemplate();
				ActorComp = ComponentNode->GetTemplateFromNode();
			}

			// fix up existing content to be sure these are flagged as archetypes and are transactional
			ActorComp->SetFlags(RF_ArchetypeObject|RF_Transactional);	
			ReferencedTemplates.Add(ActorComp);
		}
	}
	Blueprint->ComponentTemplates.Empty();
	Blueprint->ComponentTemplates.Append(ReferencedTemplates);
}

/** Ensures that the CDO root component reference is valid for Actor-based Blueprints */
void FBlueprintEditorUtils::UpdateRootComponentReference(UBlueprint* Blueprint)
{
	// The CDO's root component reference should match that of its parent class
	if(Blueprint && Blueprint->ParentClass && Blueprint->GeneratedClass)
	{
		AActor* ParentActorCDO = Cast<AActor>(Blueprint->ParentClass->GetDefaultObject(false));
		AActor* BlueprintActorCDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject(false));
		if(ParentActorCDO && BlueprintActorCDO)
		{
			// If both CDOs are valid, check for a valid scene root component
			USceneComponent* ParentSceneRootComponent = ParentActorCDO->GetRootComponent();
			USceneComponent* BlueprintSceneRootComponent = BlueprintActorCDO->GetRootComponent();
			if((ParentSceneRootComponent == nullptr && BlueprintSceneRootComponent != nullptr)
				|| (ParentSceneRootComponent != nullptr && BlueprintSceneRootComponent == nullptr)
				|| (ParentSceneRootComponent != nullptr && BlueprintSceneRootComponent != nullptr && ParentSceneRootComponent->GetFName() != BlueprintSceneRootComponent->GetFName()))
			{
				// If the parent CDO has a valid scene root component
				if(ParentSceneRootComponent != nullptr)
				{
					// Search for a scene component with the same name in the Blueprint CDO's Components list
					TInlineComponentArray<USceneComponent*> SceneComponents;
					BlueprintActorCDO->GetComponents(SceneComponents);
					for(int i = 0; i < SceneComponents.Num(); ++i)
					{
						USceneComponent* SceneComp = SceneComponents[i];
						if(SceneComp && SceneComp->GetFName() == ParentSceneRootComponent->GetFName())
						{
							// We found a match, so make this the new scene root component
							BlueprintActorCDO->SetRootComponent(SceneComp);
							break;
						}
					}
				}
				else if(BlueprintSceneRootComponent != nullptr)
				{
					// The parent CDO does not have a valid scene root, so NULL out the Blueprint CDO reference to match
					BlueprintActorCDO->SetRootComponent(nullptr);
				}
			}
		}
	}
}

bool FBlueprintEditorUtils::IsSCSComponentProperty(UObjectProperty* MemberProperty)
{
	if (!MemberProperty->PropertyClass->IsChildOf<UActorComponent>())
	{
		return false;
	}


	UClass* OwnerClass = MemberProperty->GetOwnerClass();
	UBlueprintGeneratedClass* BpClassOwner = Cast<UBlueprintGeneratedClass>(OwnerClass);

	if (BpClassOwner == nullptr)
	{
		// if this isn't directly a blueprint property, then we check if it is a 
		// associated with a natively added component (which would still be  
		// accessible through the SCS tree)

		if ((OwnerClass == nullptr) || !OwnerClass->IsChildOf<AActor>())
		{
			return false;
		}
		else if (const AActor* ActorCDO = GetDefault<AActor>(OwnerClass))
		{
			TInlineComponentArray<UActorComponent*> CDOComponents;
			ActorCDO->GetComponents(CDOComponents);

			const void* PropertyAddress = MemberProperty->ContainerPtrToValuePtr<void>(ActorCDO);
			UObject* PropertyValue = MemberProperty->GetObjectPropertyValue(PropertyAddress);

			for (UActorComponent* Component : CDOComponents)
			{
				if (!Component->GetClass()->IsChildOf(MemberProperty->PropertyClass))
				{
					continue;
				}

				if (PropertyValue == Component)
				{
					return true;
				}
			}
		}
		return false;
	}

	FMemberReference MemberRef;
	MemberRef.SetFromField<UProperty>(MemberProperty, /*bIsConsideredSelfContext =*/false);
	bool const bIsGuidValid = MemberRef.GetMemberGuid().IsValid();

	if (BpClassOwner->SimpleConstructionScript != nullptr)
	{
		TArray<USCS_Node*> SCSNodes = BpClassOwner->SimpleConstructionScript->GetAllNodes();
		for (USCS_Node* ScsNode : SCSNodes)
		{
			if (bIsGuidValid && ScsNode->VariableGuid.IsValid())
			{
				if (ScsNode->VariableGuid == MemberRef.GetMemberGuid())
				{
					return true;
				}
			}
			else if (ScsNode->GetVariableName() == MemberRef.GetMemberName())
			{
				return true;
			}
		}
	}
	return false;
}

UActorComponent* FBlueprintEditorUtils::FindUCSComponentTemplate(const FComponentKey& ComponentKey)
{
	UActorComponent* FoundTemplate = nullptr;
	if (ComponentKey.IsValid() && ComponentKey.IsUCSKey())
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(ComponentKey.GetComponentOwner()->ClassGeneratedBy);
		check(Blueprint != nullptr);

		if (UEdGraph* UCSGraph = FBlueprintEditorUtils::FindUserConstructionScript(Blueprint))
		{
			TArray<UK2Node_AddComponent*> ComponentNodes;
			UCSGraph->GetNodesOfClass<UK2Node_AddComponent>(ComponentNodes);

			for (UK2Node_AddComponent* UCSNode : ComponentNodes)
			{
				if (UCSNode->NodeGuid == ComponentKey.GetAssociatedGuid())
				{
					FoundTemplate = UCSNode->GetTemplateFromNode();
					break;
				}
			}
		}
	}
	return FoundTemplate;
}

/** Temporary fix for cut-n-paste error that failed to carry transactional flags */
void FBlueprintEditorUtils::UpdateTransactionalFlags(UBlueprint* Blueprint)
{
	TArray<UK2Node*> AllNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);

	for (UK2Node* K2Node : AllNodes)
	{
		check(K2Node);

		if (!K2Node->HasAnyFlags(RF_Transactional))
		{
			K2Node->SetFlags(RF_Transactional);
			Blueprint->Status = BS_Dirty;
		}
	}
}

void FBlueprintEditorUtils::UpdateStalePinWatches( UBlueprint* Blueprint )
{
	TSet<UEdGraphPin*> AllPins;

	// Find all unique pins being watched
	for (const FEdGraphPinReference& PinRef : Blueprint->WatchedPins)
	{
		UEdGraphPin* Pin = PinRef.Get();
		if (Pin == nullptr)
		{
			continue;
		}

		UEdGraphNode* OwningNode = Pin->GetOwningNode();
		// during node reconstruction, dead pins get moved to the transient 
		// package (so just in case this blueprint got saved with dead pin watches)
		if (OwningNode == nullptr)
		{
			continue;
		}

		if (!OwningNode->Pins.Contains(Pin))
		{
			continue;
		}
		
		AllPins.Add(Pin);
	}

	// Refresh watched pins with unique pins (throw away null or duplicate watches)
	if (Blueprint->WatchedPins.Num() != AllPins.Num())
	{
		Blueprint->WatchedPins.Empty();
		for (UEdGraphPin* Pin : AllPins)
		{
			Blueprint->WatchedPins.Add(Pin);
		}

		Blueprint->Status = BS_Dirty;
	}
}

void FBlueprintEditorUtils::ClearMacroCosmeticInfoCache(UBlueprint* Blueprint)
{
	Blueprint->PRIVATE_CachedMacroInfo.Reset();
}

FBlueprintMacroCosmeticInfo FBlueprintEditorUtils::GetCosmeticInfoForMacro(UEdGraph* MacroGraph)
{
	if (UBlueprint* MacroOwnerBP = FBlueprintEditorUtils::FindBlueprintForGraph(MacroGraph))
	{
		checkSlow(MacroGraph->GetSchema()->GetGraphType(MacroGraph) == GT_Macro);
		
		// See if it's in the cache
		if (FBlueprintMacroCosmeticInfo* pCosmeticInfo = MacroOwnerBP->PRIVATE_CachedMacroInfo.Find(MacroGraph))
		{
			return *pCosmeticInfo;
		}
		else
		{
			FBlueprintMacroCosmeticInfo& CosmeticInfo = MacroOwnerBP->PRIVATE_CachedMacroInfo.Add(MacroGraph);
			CosmeticInfo.bContainsLatentNodes = FBlueprintEditorUtils::CheckIfGraphHasLatentFunctions(MacroGraph);

			return CosmeticInfo;
		}
	}

	return FBlueprintMacroCosmeticInfo();
}

FName FBlueprintEditorUtils::FindUniqueKismetName(const UBlueprint* InBlueprint, const FString& InBaseName, UStruct* InScope/* = nullptr*/)
{
	int32 Count = 0;
	FString KismetName;
	FString BaseName = InBaseName;
	TSharedPtr<FKismetNameValidator> NameValidator = MakeShareable(new FKismetNameValidator(InBlueprint, NAME_None, InScope));

	// Clean up BaseName to not contain any invalid characters, which will mean we can never find a legal name no matter how many numbers we add
	if (NameValidator->IsValid(BaseName) == EValidatorResult::ContainsInvalidCharacters)
	{
		for (TCHAR& TestChar : BaseName)
		{
			for (TCHAR BadChar : UE_BLUEPRINT_INVALID_NAME_CHARACTERS)
			{
				if (TestChar == BadChar)
				{
					TestChar = TEXT('_');
					break;
				}
			}
		}
	}

	while(NameValidator->IsValid(KismetName) != EValidatorResult::Ok)
	{
		// Calculate the number of digits in the number, adding 2 (1 extra to correctly count digits, another to account for the '_' that will be added to the name
		int32 CountLength = Count > 0? (int32)log((double)Count) + 2 : 2;

		// If the length of the final string will be too long, cut off the end so we can fit the number
		if(CountLength + BaseName.Len() > NameValidator->GetMaximumNameLength())
		{
			BaseName = BaseName.Left(NameValidator->GetMaximumNameLength() - CountLength);
		}
		KismetName = FString::Printf(TEXT("%s_%d"), *BaseName, Count);
		Count++;
	}

	return FName(*KismetName);
}

FName FBlueprintEditorUtils::FindUniqueCustomEventName(const UBlueprint* Blueprint)
{
	return FindUniqueKismetName(Blueprint, LOCTEXT("DefaultCustomEventName", "CustomEvent").ToString());
}

//////////////////////////////////////////////////////////////////////////
// Timeline

FName FBlueprintEditorUtils::FindUniqueTimelineName(const UBlueprint* Blueprint)
{
	return FindUniqueKismetName(Blueprint, LOCTEXT("DefaultTimelineName", "Timeline").ToString());
}

UTimelineTemplate* FBlueprintEditorUtils::AddNewTimeline(UBlueprint* Blueprint, const FName& TimelineVarName)
{
	// Early out if we don't support timelines in this class
	if( !FBlueprintEditorUtils::DoesSupportTimelines(Blueprint) )
	{
		return nullptr;
	}

	// First look to see if we already have a timeline with that name
	UTimelineTemplate* Timeline = Blueprint->FindTimelineTemplateByVariableName(TimelineVarName);
	if (Timeline != nullptr)
	{
		UE_LOG(LogBlueprint, Log, TEXT("AddNewTimeline: Blueprint '%s' already contains a timeline called '%s'"), *Blueprint->GetPathName(), *TimelineVarName.ToString());
		return nullptr;
	}
	else
	{
		Blueprint->Modify();
		check(nullptr != Blueprint->GeneratedClass);
		// Construct new graph with the supplied name
		const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineVarName);
		Timeline = NewObject<UTimelineTemplate>(Blueprint->GeneratedClass, TimelineTemplateName, RF_Transactional);
		Blueprint->Timelines.Add(Timeline);

		// Potentially adjust variable names for any child blueprints
		FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, TimelineVarName);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return Timeline;
	}
}

void FBlueprintEditorUtils::RemoveTimeline(UBlueprint* Blueprint, UTimelineTemplate* Timeline, bool bDontRecompile)
{
	// Ensure objects are marked modified
	Timeline->Modify();
	Blueprint->Modify();

	Blueprint->Timelines.Remove(Timeline);
	Timeline->MarkPendingKill();

	if( !bDontRecompile )
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

UK2Node_Timeline* FBlueprintEditorUtils::FindNodeForTimeline(UBlueprint* Blueprint, UTimelineTemplate* Timeline)
{
	check(Timeline);
	const FName TimelineVarName = *UTimelineTemplate::TimelineTemplateNameToVariableName(Timeline->GetFName());

	TArray<UK2Node_Timeline*> TimelineNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Timeline>(Blueprint, TimelineNodes);

	for(int32 i=0; i<TimelineNodes.Num(); i++)
	{
		UK2Node_Timeline* TestNode = TimelineNodes[i];
		if(TestNode->TimelineName == TimelineVarName)
		{
			return TestNode;
		}
	}

	return nullptr; // no node found
}

bool FBlueprintEditorUtils::RenameTimeline(UBlueprint* Blueprint, const FName OldNameRef, const FName NewName)
{
	check(Blueprint);

	bool bRenamed = false;

	// make a copy, in case we alter the value of what is referenced by 
	// OldNameRef through the course of this function
	FName OldName = OldNameRef;

	TSharedPtr<INameValidatorInterface> NameValidator = MakeShareable(new FKismetNameValidator(Blueprint));
	const FString NewTemplateName = UTimelineTemplate::TimelineVariableNameToTemplateName(NewName);
	// NewName should be already validated. But one must make sure that NewTemplateName is also unique.
	const bool bUniqueNameForTemplate = (EValidatorResult::Ok == NameValidator->IsValid(NewTemplateName));

	UTimelineTemplate* Template = Blueprint->FindTimelineTemplateByVariableName(OldName);
	UTimelineTemplate* NewTemplate = Blueprint->FindTimelineTemplateByVariableName(NewName);
	if ((!NewTemplate && bUniqueNameForTemplate) || NewTemplate == Template)
	{
		if (Template)
		{
			Blueprint->Modify();
			Template->Modify();

			if (UK2Node_Timeline* TimelineNode = FindNodeForTimeline(Blueprint, Template))
			{
				TimelineNode->Modify();
				TimelineNode->TimelineName = NewName;
			}

			const FString NewNameStr = NewName.ToString();
			const FString OldNameStr = OldName.ToString();

			TArray<UK2Node_Variable*> TimelineVarNodes;
			FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Variable>(Blueprint, TimelineVarNodes);
			for(int32 It = 0; It < TimelineVarNodes.Num(); It++)
			{
				UK2Node_Variable* TestNode = TimelineVarNodes[It];
				if(TestNode && (OldName == TestNode->GetVarName()))
				{
					UEdGraphPin* TestPin = TestNode->FindPin(OldNameStr);
					if(TestPin && (UTimelineComponent::StaticClass() == TestPin->PinType.PinSubCategoryObject.Get()))
					{
						TestNode->Modify();
						if(TestNode->VariableReference.IsSelfContext())
						{
							TestNode->VariableReference.SetSelfMember(NewName);
						}
						else
						{
							//TODO:
							UClass* ParentClass = TestNode->VariableReference.GetMemberParentClass((UClass*)nullptr);
							TestNode->VariableReference.SetExternalMember(NewName, ParentClass);
						}
						TestPin->Modify();
						TestPin->PinName = NewNameStr;
					}
				}
			}

			Blueprint->Timelines.Remove(Template);
			
			UObject* ExistingObject = StaticFindObject(nullptr, Template->GetOuter(), *NewTemplateName, true);
			if (ExistingObject != Template && ExistingObject != nullptr)
			{
				ExistingObject->Rename(*MakeUniqueObjectName(ExistingObject->GetOuter(), ExistingObject->GetClass(), ExistingObject->GetFName()).ToString());
			}
			Template->Rename(*NewTemplateName, Template->GetOuter(), (Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : REN_None));
			Blueprint->Timelines.Add(Template);

			// Validate child blueprints and adjust variable names to avoid a potential name collision
			FBlueprintEditorUtils::ValidateBlueprintChildVariables(Blueprint, NewName);

			// Refresh references and flush editors
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			bRenamed = true;
		}
	}
	return bRenamed;
}

//////////////////////////////////////////////////////////////////////////
// LevelScriptBlueprint

bool FBlueprintEditorUtils::FindReferencesToActorFromLevelScript(ULevelScriptBlueprint* LevelScriptBlueprint, AActor* InActor, TArray<UK2Node*>& ReferencedToActors)
{
	if (LevelScriptBlueprint == nullptr)
	{
		return false;
	}

	TArray<UEdGraph*> AllGraphs;
	LevelScriptBlueprint->GetAllGraphs(AllGraphs);

	for(TArray<UEdGraph*>::TConstIterator it(AllGraphs); it; ++it)
	{
		const UEdGraph* CurrentGraph = *it;

		TArray<UK2Node*> GraphNodes;
		CurrentGraph->GetNodesOfClass(GraphNodes);

		for(UK2Node* Node : GraphNodes)
		{
			if(Node != nullptr && Node->GetReferencedLevelActor() == InActor )
			{
				ReferencedToActors.Add(Node);
			}
		}
	}

	return ReferencedToActors.Num() > 0;
}

void FBlueprintEditorUtils::ReplaceAllActorRefrences(ULevelScriptBlueprint* InLevelScriptBlueprint, AActor* InOldActor, AActor* InNewActor)
{
	InLevelScriptBlueprint->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsModified(InLevelScriptBlueprint);

	// Literal nodes are the common "get" type nodes and need to be updated with the new object reference
	TArray< UK2Node_Literal* > LiteralNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(InLevelScriptBlueprint, LiteralNodes);

	for( UK2Node_Literal* LiteralNode : LiteralNodes )
	{
		if(LiteralNode->GetObjectRef() == InOldActor)
		{
			LiteralNode->Modify();
			LiteralNode->SetObjectRef(InNewActor);
			LiteralNode->ReconstructNode();
		}
	}

	// Actor Bound Events reference the actors as well and need to be updated
	TArray< UK2Node_ActorBoundEvent* > ActorEventNodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(InLevelScriptBlueprint, ActorEventNodes);

	for( UK2Node_ActorBoundEvent* ActorEventNode : ActorEventNodes )
	{
		if(ActorEventNode->GetReferencedLevelActor() == InOldActor)
		{
			ActorEventNode->Modify();
			ActorEventNode->EventOwner = InNewActor;
			ActorEventNode->ReconstructNode();
		}
	}
}

void  FBlueprintEditorUtils::ModifyActorReferencedGraphNodes(ULevelScriptBlueprint* LevelScriptBlueprint, const AActor* InActor)
{
	TArray<UEdGraph*> AllGraphs;
	LevelScriptBlueprint->GetAllGraphs(AllGraphs);

	for(TArray<UEdGraph*>::TConstIterator it(AllGraphs); it; ++it)
	{
		const UEdGraph* CurrentGraph = *it;

		TArray<UK2Node*> GraphNodes;
		CurrentGraph->GetNodesOfClass(GraphNodes);

		for( TArray<UK2Node*>::TIterator NodeIt(GraphNodes); NodeIt; ++NodeIt )
		{
			UK2Node* CurrentNode = *NodeIt;
			if( CurrentNode != nullptr && CurrentNode->GetReferencedLevelActor() == InActor )
			{
				CurrentNode->Modify();
			}
		}
	}
}

void FBlueprintEditorUtils::FindActorsThatReferenceActor( AActor* InActor, TArray<UClass*>& InClassesToIgnore, TArray<AActor*>& OutReferencingActors )
{
	// Iterate all actors in the same world as InActor
	for ( FActorIterator ActorIt(InActor->GetWorld()); ActorIt; ++ActorIt )
	{
		AActor* CurrentActor = *ActorIt;
		if (( CurrentActor ) && ( CurrentActor != InActor ))
		{
			bool bShouldIgnore = false;
			// Ignore Actors that aren't in the same level as InActor - cross level references are not allowed, so it's safe to ignore.
			if ( !CurrentActor->IsInLevel(InActor->GetLevel() ) )
			{
				bShouldIgnore = true;
			}
			// Ignore Actors if they are of a type we were instructed to ignore.
			for ( int32 IgnoreIndex = 0; IgnoreIndex < InClassesToIgnore.Num() && !bShouldIgnore; IgnoreIndex++ )
			{
				if ( CurrentActor->IsA( InClassesToIgnore[IgnoreIndex] ) )
				{
					bShouldIgnore = true;
				}
			}

			if ( !bShouldIgnore )
			{
				// Get all references from CurrentActor and see if any are the Actor we're searching for
				TArray<UObject*> References;
				FReferenceFinder Finder( References );
				Finder.FindReferences( CurrentActor );

				if ( References.Contains( InActor ) )
				{
					OutReferencingActors.Add( CurrentActor );
				}
			}
		}
	}
}

void FBlueprintEditorUtils::GetActorReferenceMap(UWorld* InWorld, TArray<UClass*>& InClassesToIgnore, TMap<AActor*, TArray<AActor*> >& OutReferencingActors)
{
	// Iterate all actors in the same world as InActor
	for (FActorIterator ActorIt(InWorld); ActorIt; ++ActorIt)
	{
		AActor* CurrentActor = *ActorIt;
		if (CurrentActor)
		{
			bool bShouldIgnore = false;

			// Ignore Actors if they are of a type we were instructed to ignore.
			for (int32 IgnoreIndex = 0; IgnoreIndex < InClassesToIgnore.Num() && !bShouldIgnore; IgnoreIndex++)
			{
				if (CurrentActor->IsA(InClassesToIgnore[IgnoreIndex]))
				{
					bShouldIgnore = true;
				}
			}

			if (!bShouldIgnore)
			{
				// Get all references from CurrentActor and see if any Actors
				TArray<UObject*> References;
				FReferenceFinder Finder(References);
				Finder.FindReferences(CurrentActor);

				for (int32 RefIdx = 0; RefIdx < References.Num(); RefIdx++)
				{
					if (References[RefIdx] && References[RefIdx]->IsA(AActor::StaticClass()))
					{
						OutReferencingActors.FindOrAdd(Cast<AActor>(References[RefIdx])).Add(CurrentActor);
					}
				}
			}
		}
	}
}

void FBlueprintEditorUtils::FixLevelScriptActorBindings(ALevelScriptActor* LevelScriptActor, const ULevelScriptBlueprint* ScriptBlueprint)
{
	if( ScriptBlueprint->BlueprintType != BPTYPE_LevelScript )
	{
		return;
	}

	UPackage* ActorPackage = LevelScriptActor->GetOutermost();
	UPackage* BlueprintPkg = ScriptBlueprint->GetOutermost();
	// the nodes in the Blueprint are going to be bound to actors within the same
	// (level) package, they're the actors in the editor; if LevelScriptActor 
	// doesn't belong to that package, then it is likely a copy (for PIE), this guard 
	// prevents us from cross-binding instantiated (PIE) actors to editor objects
	if (ActorPackage != BlueprintPkg)
	{
		return;
	}

	TArray<UEdGraph*> AllGraphs;
	ScriptBlueprint->GetAllGraphs(AllGraphs);

	// Iterate over all graphs, and find all bound event nodes
	for( TArray<UEdGraph*>::TConstIterator GraphIt(AllGraphs); GraphIt; ++GraphIt )
	{
		TArray<UK2Node_ActorBoundEvent*> BoundEvents;
		(*GraphIt)->GetNodesOfClass(BoundEvents);

		for( TArray<UK2Node_ActorBoundEvent*>::TConstIterator NodeIt(BoundEvents); NodeIt; ++NodeIt )
		{
			UK2Node_ActorBoundEvent* EventNode = *NodeIt;

			// For each bound event node, verify that we have an entry point in the LSA, and add a delegate to the target
			if( EventNode && EventNode->EventOwner )
			{
				const FName TargetFunction = EventNode->CustomFunctionName;

				// Check to make sure the level scripting actor actually has the function defined
				if( LevelScriptActor->FindFunction(TargetFunction) )
				{
					// Grab the MC delegate we need to add to
					FMulticastScriptDelegate* TargetDelegate = EventNode->GetTargetDelegate();
					if( TargetDelegate != nullptr )
					{
						// Create the delegate, and add it if it doesn't already exist
						FScriptDelegate Delegate;
						Delegate.BindUFunction(LevelScriptActor, TargetFunction);
						TargetDelegate->AddUnique(Delegate);
					}
				}
			}
		}

		// Find matinee controller nodes and update node name
		TArray<UK2Node_MatineeController*> MatineeControllers;
		(*GraphIt)->GetNodesOfClass(MatineeControllers);

		for( TArray<UK2Node_MatineeController*>::TConstIterator NodeIt(MatineeControllers); NodeIt; ++NodeIt )
		{
			const UK2Node_MatineeController* MatController = *NodeIt;

			if(MatController->MatineeActor != nullptr)
			{
				MatController->MatineeActor->MatineeControllerName = MatController->GetFName();
			}
		}
	}
}

void FBlueprintEditorUtils::ListPackageContents(UPackage* Package, FOutputDevice& Ar)
{
	Ar.Logf(TEXT("Package %s contains:"), *Package->GetName());
	for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
	{
		if (ObjIt->GetOuter() == Package)
		{
			Ar.Logf(TEXT("  %s (flags 0x%X)"), *ObjIt->GetFullName(), (int32)ObjIt->GetFlags());
		}
	}
}

bool FBlueprintEditorUtils::KismetDiagnosticExec(const TCHAR* InStream, FOutputDevice& Ar)
{
	const TCHAR* Str = InStream;

	if (FParse::Command(&Str, TEXT("FindBadBlueprintReferences")))
	{
		// Collect garbage first to remove any false positives
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		UPackage* TransientPackage = GetTransientPackage();

		// Unique blueprints/classes that contain badness
		TSet<UObject*> ObjectsContainingBadness;
		TSet<UPackage*> BadPackages;

		// Run thru every object in the world
		for (FObjectIterator ObjectIt; ObjectIt; ++ObjectIt)
		{
			UObject* TestObj = *ObjectIt;

			// If the test object is itself transient, there is no concern
			CA_SUPPRESS(28182); // https://connect.microsoft.com/VisualStudio/feedback/details/3082622
			if (TestObj->HasAnyFlags(RF_Transient))
			{
				continue;
			}


			// Look for a containing scope (either a blueprint or a class)
			UObject* OuterScope = nullptr;
			for (UObject* TestOuter = TestObj; (TestOuter != nullptr) && (OuterScope == nullptr); TestOuter = TestOuter->GetOuter())
			{
				if (UClass* OuterClass = Cast<UClass>(TestOuter))
				{
					if (OuterClass->ClassGeneratedBy != nullptr)
					{
						OuterScope = OuterClass;
					}
				}
				else if (UBlueprint* OuterBlueprint = Cast<UBlueprint>(TestOuter))
				{
					OuterScope = OuterBlueprint;
				}
			}

			if ((OuterScope != nullptr) && !OuterScope->IsIn(TransientPackage))
			{
				// Find all references
				TArray<UObject*> ReferencedObjects;
				FReferenceFinder ObjectReferenceCollector(ReferencedObjects, nullptr, false, false, false, false);
				ObjectReferenceCollector.FindReferences(TestObj);

				for (UObject* ReferencedObj : ReferencedObjects)
				{
					// Ignore any references inside the outer blueprint or class; they're intrinsically safe
					if (!ReferencedObj->IsIn(OuterScope))
					{
						// If it's a public reference, that's fine
						if (!ReferencedObj->HasAnyFlags(RF_Public))
						{
							// It's a private reference outside of the parent object; not good!
							Ar.Logf(TEXT("%s has a reference to %s outside of it's container %s"),
								*TestObj->GetFullName(),
								*ReferencedObj->GetFullName(),
								*OuterScope->GetFullName()
								);
							ObjectsContainingBadness.Add(OuterScope);
							BadPackages.Add(OuterScope->GetOutermost());
						}
					}
				}
			}
		}

		// Report all the bad outers as text dumps so the exact property can be identified
		Ar.Logf(TEXT("Summary of assets containing objects that have bad references"));
		for (UObject* BadObj : ObjectsContainingBadness)
		{
			Ar.Logf(TEXT("\n\nObject %s referenced private objects outside of it's container asset inappropriately"), *BadObj->GetFullName());

			UBlueprint* Blueprint = Cast<UBlueprint>(BadObj);
			if (Blueprint == nullptr)
			{
				if (UClass* Class = Cast<UClass>(BadObj))
				{
					Blueprint = CastChecked<UBlueprint>(Class->ClassGeneratedBy);

					if (Blueprint->GeneratedClass == Class)
					{
						Ar.Logf(TEXT("  => GeneratedClass of %s"), *Blueprint->GetFullName());
					}
					else if (Blueprint->SkeletonGeneratedClass == Class)
					{
						Ar.Logf(TEXT("  => SkeletonGeneratedClass of %s"), *Blueprint->GetFullName());
					}
					else
					{
						Ar.Logf(TEXT("  => ***FALLEN BEHIND*** class generated by %s"), *Blueprint->GetFullName());
					}
					Ar.Logf(TEXT("  Has an associated CDO named %s"), *Class->GetDefaultObject()->GetFullName());
				}
			}

			// Export the asset to text
			if (true)
			{
				UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));
				FStringOutputDevice Archive;
				const FExportObjectInnerContext Context;
				UExporter::ExportToOutputDevice(&Context, BadObj, nullptr, Archive, TEXT("copy"), 0, /*PPF_IncludeTransient*/ /*PPF_ExportsNotFullyQualified|*/PPF_Copy, false, nullptr);
				FString ExportedText = Archive;

				Ar.Logf(TEXT("%s"), *ExportedText);
			}
		}

		// Report the contents of the bad packages
		for (UPackage* BadPackage : BadPackages)
		{
			Ar.Logf(TEXT("\nBad package %s contains:"), *BadPackage->GetName());
			for (FObjectIterator ObjIt; ObjIt; ++ObjIt)
			{
				if (ObjIt->GetOuter() == BadPackage)
				{
					Ar.Logf(TEXT("  %s"), *ObjIt->GetFullName());
				}
			}
		}

		Ar.Logf(TEXT("\nFinished listing illegal private references"));
	}
	else if (FParse::Command(&Str, TEXT("ListPackageContents")))
	{
		if (UPackage* Package = FindPackage(nullptr, Str))
		{
			FBlueprintEditorUtils::ListPackageContents(Package, Ar);
		}
		else
		{
			Ar.Logf(TEXT("Failed to find package %s"), Str);
		}
	}
	else if (FParse::Command(&Str, TEXT("RepairBlueprint")))
	{
		if (UBlueprint* Blueprint = FindObject<UBlueprint>(ANY_PACKAGE, Str))
		{
			IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
			Compiler.RecoverCorruptedBlueprint(Blueprint);
		}
		else
		{
			Ar.Logf(TEXT("Failed to find blueprint %s"), Str);
		}
	}
	else if (FParse::Command(&Str, TEXT("ListOrphanClasses")))
	{
		UE_LOG(LogBlueprintDebug, Log, TEXT("--- LISTING ORPHANED CLASSES ---"));
		for( TObjectIterator<UClass> it; it; ++it )
		{
			UClass* CurrClass = *it;
			if( CurrClass->ClassGeneratedBy != nullptr && CurrClass->GetOutermost() != GetTransientPackage() )
			{
				if( UBlueprint* GeneratingBP = Cast<UBlueprint>(CurrClass->ClassGeneratedBy) )
				{
					if( CurrClass != GeneratingBP->GeneratedClass && CurrClass != GeneratingBP->SkeletonGeneratedClass )
					{
						UE_LOG(LogBlueprintDebug, Log, TEXT(" - %s"), *CurrClass->GetFullName());				
					}
				}	
			}
		}

		return true;
	}
	else if (FParse::Command(&Str, TEXT("ListRootSetObjects")))
	{
		UE_LOG(LogBlueprintDebug, Log, TEXT("--- LISTING ROOTSET OBJ ---"));
		for( FObjectIterator it; it; ++it )
		{
			UObject* CurrObj = *it;
			if( CurrObj->IsRooted() )
			{
				UE_LOG(LogBlueprintDebug, Log, TEXT(" - %s"), *CurrObj->GetFullName());
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}


void FBlueprintEditorUtils::OpenReparentBlueprintMenu( UBlueprint* Blueprint, const TSharedRef<SWidget>& ParentContent, const FOnClassPicked& OnPicked)
{
	TArray< UBlueprint* > Blueprints;
	Blueprints.Add( Blueprint );
	OpenReparentBlueprintMenu( Blueprints, ParentContent, OnPicked );
}

class FBlueprintReparentFilter : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet< const UClass* > AllowedChildrenOfClasses;

	/** Classes to not allow any children of into the Class Viewer/Picker. */
	TSet< const UClass* > DisallowedChildrenOfClasses;

	/** Classes to never show in this class viewer. */
	TSet< const UClass* > DisallowedClasses;

	/** Will limit the results to only native classes */
	bool bShowNativeOnly;

	FBlueprintReparentFilter()
		: bShowNativeOnly(false)
	{}

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		// If it appears on the allowed child-of classes list (or there is nothing on that list)
		//		AND it is NOT on the disallowed child-of classes list
		//		AND it is NOT on the disallowed classes list
		return InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed && 
			InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InClass) != EFilterReturn::Passed && 
			InFilterFuncs->IfInClassesSet(DisallowedClasses, InClass) != EFilterReturn::Passed &&
			!InClass->HasAnyClassFlags(CLASS_Deprecated) &&
			((bShowNativeOnly && InClass->HasAnyClassFlags(CLASS_Native)) || !bShowNativeOnly);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		// If it appears on the allowed child-of classes list (or there is nothing on that list)
		//		AND it is NOT on the disallowed child-of classes list
		//		AND it is NOT on the disallowed classes list
		return InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed && 
			InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Passed && 
			InFilterFuncs->IfInClassesSet(DisallowedClasses, InUnloadedClassData) != EFilterReturn::Passed &&
			!InUnloadedClassData->HasAnyClassFlags(CLASS_Deprecated)  &&
			((bShowNativeOnly && InUnloadedClassData->HasAnyClassFlags(CLASS_Native)) || !bShowNativeOnly);
	}
};

TSharedRef<SWidget> FBlueprintEditorUtils::ConstructBlueprintParentClassPicker( const TArray< UBlueprint* >& Blueprints, const FOnClassPicked& OnPicked)
{
	bool bIsActor = false;
	bool bIsAnimBlueprint = false;
	bool bIsLevelScriptActor = false;
	bool bIsComponentBlueprint = false;
	TArray<UClass*> BlueprintClasses;
	for( auto BlueprintIter = Blueprints.CreateConstIterator(); (!bIsActor && !bIsAnimBlueprint) && BlueprintIter; ++BlueprintIter )
	{
		const UBlueprint* Blueprint = *BlueprintIter;
		bIsActor |= Blueprint->ParentClass->IsChildOf( AActor::StaticClass() );
		bIsAnimBlueprint |= Blueprint->IsA(UAnimBlueprint::StaticClass());
		bIsLevelScriptActor |= Blueprint->ParentClass->IsChildOf( ALevelScriptActor::StaticClass() );
		bIsComponentBlueprint |= Blueprint->ParentClass->IsChildOf( UActorComponent::StaticClass() );
		if(Blueprint->GeneratedClass)
		{
			BlueprintClasses.Add(Blueprint->GeneratedClass);
		}
	}

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<FBlueprintReparentFilter> Filter = MakeShareable(new FBlueprintReparentFilter);
	Options.ClassFilter = Filter;
	Options.ViewerTitleString = LOCTEXT("ReparentBlueprint", "Reparent blueprint");

	// Only allow parenting to base blueprints.
	Options.bIsBlueprintBaseOnly = true;

	// never allow parenting to Interface
	Filter->DisallowedChildrenOfClasses.Add( UInterface::StaticClass() );

	// never allow parenting to children of itself
	for (UClass* BPClass : BlueprintClasses)
	{
		Filter->DisallowedChildrenOfClasses.Add(BPClass);
	}

	for ( UBlueprint* Blueprint : Blueprints )
	{
		Blueprint->GetReparentingRules(Filter->AllowedChildrenOfClasses, Filter->DisallowedChildrenOfClasses);
	}

	if(bIsActor)
	{
		if(bIsLevelScriptActor)
		{
			// Don't allow conversion outside of the LevelScriptActor hierarchy
			Filter->AllowedChildrenOfClasses.Add( ALevelScriptActor::StaticClass() );
			Filter->bShowNativeOnly = true;
		}
		else
		{
			// Don't allow conversion outside of the Actor hierarchy
			Filter->AllowedChildrenOfClasses.Add( AActor::StaticClass() );

			// Don't allow non-LevelScriptActor->LevelScriptActor conversion
			Filter->DisallowedChildrenOfClasses.Add( ALevelScriptActor::StaticClass() );
		}
	}
	else if (bIsAnimBlueprint)
	{
		// If it's an anim blueprint, do not allow conversion to non anim
		Filter->AllowedChildrenOfClasses.Add( UAnimInstance::StaticClass() );
	}
	else if(bIsComponentBlueprint)
	{
		// If it is a component blueprint, only allow classes under and including UActorComponent
		Filter->AllowedChildrenOfClasses.Add( UActorComponent::StaticClass() );
	}
	else
	{
		Filter->DisallowedChildrenOfClasses.Add( AActor::StaticClass() );
	}


	for( const UBlueprint* Blueprint : Blueprints)
	{
		// don't allow making me my own parent!
		if(Blueprint->GeneratedClass)
		{
			Filter->DisallowedClasses.Add(Blueprint->GeneratedClass);
		}
	}

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

void FBlueprintEditorUtils::OpenReparentBlueprintMenu( const TArray< UBlueprint* >& Blueprints, const TSharedRef<SWidget>& ParentContent, const FOnClassPicked& OnPicked)
{
	if ( Blueprints.Num() == 0 )
	{
		return;
	}

	TSharedRef<SWidget> ClassPicker = ConstructBlueprintParentClassPicker(Blueprints, OnPicked);

	TSharedRef<SBox> ClassPickerBox = 
		SNew(SBox)
		.WidthOverride(280)
		.HeightOverride(400)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
			[
				ClassPicker
			]
		];

	// Show dialog to choose new parent class
	FSlateApplication::Get().PushMenu(
		ParentContent,
		FWidgetPath(),
		ClassPickerBox,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu),
		true);
}

/** Filter class for ClassPicker handling allowed interfaces for a Blueprint */
class FBlueprintInterfaceFilter : public IClassViewerFilter
{
public:
	/** All children of these classes will be included unless filtered out by another setting. */
	TSet< const UClass* > AllowedChildrenOfClasses;

	/** Classes to not allow any children of into the Class Viewer/Picker. */
	TSet< const UClass* > DisallowedChildrenOfClasses;

	/** Classes to never show in this class viewer. */
	TSet< const UClass* > DisallowedClasses;

	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs ) override
	{
		// If it appears on the allowed child-of classes list (or there is nothing on that list)
		//		AND it is NOT on the disallowed child-of classes list
		//		AND it is NOT on the disallowed classes list
		return InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed && 
			InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InClass) != EFilterReturn::Passed && 
			InFilterFuncs->IfInClassesSet(DisallowedClasses, InClass) != EFilterReturn::Passed &&
			!InClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) &&
			InClass->HasAnyClassFlags(CLASS_Interface) &&
			// Here is some loaded classes only logic, Blueprints will never have this info
			!InClass->HasMetaData(FBlueprintMetadata::MD_CannotImplementInterfaceInBlueprint);
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		// Unloaded interfaces mean they must be Blueprint Interfaces


		// If it appears on the allowed child-of classes list (or there is nothing on that list)
		//		AND it is NOT on the disallowed child-of classes list
		//		AND it is NOT on the disallowed classes list
		return InFilterFuncs->IfInChildOfClassesSet( AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed && 
			InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Passed && 
			InFilterFuncs->IfInClassesSet(DisallowedClasses, InUnloadedClassData) != EFilterReturn::Passed &&
			!InUnloadedClassData->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) &&
			InUnloadedClassData->HasAnyClassFlags(CLASS_Interface);
	}
};

TSharedRef<SWidget> FBlueprintEditorUtils::ConstructBlueprintInterfaceClassPicker( const TArray< UBlueprint* >& Blueprints, const FOnClassPicked& OnPicked)
{
	TArray<UClass*> BlueprintClasses;
	for (const UBlueprint* Blueprint : Blueprints)
	{
		if(Blueprint->GeneratedClass)
		{
			BlueprintClasses.Add(Blueprint->GeneratedClass);
		}
	}

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	TSharedPtr<FBlueprintInterfaceFilter> Filter = MakeShareable(new FBlueprintInterfaceFilter);
	Options.ClassFilter = Filter;
	Options.ViewerTitleString = LOCTEXT("ImplementInterfaceBlueprint", "Implement Interface");

	for (const UBlueprint* Blueprint : Blueprints)
	{
		// don't allow making me my own parent!
		if(Blueprint->GeneratedClass)
		{
			Filter->DisallowedClasses.Add(Blueprint->GeneratedClass);
		}
		
		UClass const* const ParentClass = Blueprint->ParentClass;
		// see if the parent class has any prohibited interfaces
		if ((ParentClass != nullptr) && ParentClass->HasMetaData(FBlueprintMetadata::MD_ProhibitedInterfaces))
		{
			FString const& ProhibitedList = Blueprint->ParentClass->GetMetaData(FBlueprintMetadata::MD_ProhibitedInterfaces);

			TArray<FString> ProhibitedInterfaceNames;
			ProhibitedList.ParseIntoArray(ProhibitedInterfaceNames, TEXT(","), true);

			// loop over all the prohibited interfaces
			for (int32 ExclusionIndex = 0; ExclusionIndex < ProhibitedInterfaceNames.Num(); ++ExclusionIndex)
			{
				ProhibitedInterfaceNames[ExclusionIndex].TrimStartInline();
				FString const& ProhibitedInterfaceName = ProhibitedInterfaceNames[ExclusionIndex].RightChop(1);
				UClass* ProhibitedInterface = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ProhibitedInterfaceName);
				if(ProhibitedInterface)
				{
					Filter->DisallowedClasses.Add(ProhibitedInterface);
					Filter->DisallowedChildrenOfClasses.Add(ProhibitedInterface);
				}
			}
		}

		// Do not allow adding interfaces that are already added to the Blueprint
		for(TArray<FBPInterfaceDescription>::TConstIterator it(Blueprint->ImplementedInterfaces); it; ++it)
		{
			const FBPInterfaceDescription& CurrentInterface = *it;
			Filter->DisallowedClasses.Add(CurrentInterface.Interface);
		}
	}

	// never allow parenting to children of itself
	for (UClass*  BPClass : BlueprintClasses)
	{
		Filter->DisallowedChildrenOfClasses.Add(BPClass);
	}

	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, OnPicked);
}

/** Call PostEditChange() on any Actors that are based on this Blueprint */
void FBlueprintEditorUtils::PostEditChangeBlueprintActors(UBlueprint* Blueprint, bool bComponentEditChange)
{
	if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
	{
		// Get the selected Actor set in the level editor context
		bool bEditorSelectionChanged = false;
		const USelection* CurrentEditorActorSelection = GEditor ? GEditor->GetSelectedActors() : nullptr;
		const bool bIncludeDerivedClasses = false;

		TArray<UObject*> MatchingBlueprintObjects;
		GetObjectsOfClass(Blueprint->GeneratedClass, MatchingBlueprintObjects, bIncludeDerivedClasses, RF_ClassDefaultObject, EInternalObjectFlags::PendingKill);

		for (UObject* MatchingObj : MatchingBlueprintObjects)
		{
			// We know the class was derived from AActor because we checked the Blueprint->GeneratedClass.
			AActor* Actor = static_cast<AActor*>(MatchingObj);
			Actor->PostEditChange();

			// Broadcast edit notification if necessary so that the level editor's detail panel is refreshed
			bEditorSelectionChanged |= CurrentEditorActorSelection && CurrentEditorActorSelection->IsSelected(Actor);
		}

		// Broadcast edit notifications if necessary so that level editor details are refreshed (e.g. components tree)
		if(bEditorSelectionChanged)
		{
			if(bComponentEditChange)
			{
				FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
				LevelEditor.BroadcastComponentsEdited();
			}
		}
	}	

	// Let the blueprint thumbnail renderer know that a blueprint has been modified so it knows to reinstance components for visualization
	FThumbnailRenderingInfo* RenderInfo = GUnrealEd->GetThumbnailManager()->GetRenderingInfo( Blueprint );
	if ( RenderInfo != nullptr )
	{
		UBlueprintThumbnailRenderer* BlueprintThumbnailRenderer = Cast<UBlueprintThumbnailRenderer>(RenderInfo->Renderer);
		if ( BlueprintThumbnailRenderer != nullptr )
		{
			BlueprintThumbnailRenderer->BlueprintChanged(Blueprint);
		}
	}
}

FBlueprintEditorUtils::EPropertyWritableState FBlueprintEditorUtils::IsPropertyWritableInBlueprint(const UBlueprint* Blueprint, const UProperty* Property)
{
	if (Property)
	{
		if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
		{
			return EPropertyWritableState::NotBlueprintVisible;
		}
		if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
		{
			return EPropertyWritableState::BlueprintReadOnly;
		}
		if (Property->GetBoolMetaData(FBlueprintMetadata::MD_Private))
		{
			const UClass* OwningClass = CastChecked<UClass>(Property->GetOuter());
			if (OwningClass->ClassGeneratedBy != Blueprint)
			{
				return EPropertyWritableState::Private;
			}
		}
	}
	return EPropertyWritableState::Writable;
}

FBlueprintEditorUtils::EPropertyReadableState FBlueprintEditorUtils::IsPropertyReadableInBlueprint(const UBlueprint* Blueprint, const UProperty* Property)
{
	if (Property)
	{
		if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
		{
			return EPropertyReadableState::NotBlueprintVisible;
		}
		if (Property->GetBoolMetaData(FBlueprintMetadata::MD_Private))
		{
			const UClass* OwningClass = CastChecked<UClass>(Property->GetOuter());
			if (OwningClass->ClassGeneratedBy != Blueprint)
			{
				return EPropertyReadableState::Private;
			}
		}
	}
	return EPropertyReadableState::Readable;
}

bool FBlueprintEditorUtils::IsPropertyReadOnlyInCurrentBlueprint(const UBlueprint* Blueprint, const UProperty* Property)
{
	return (IsPropertyWritableInBlueprint(Blueprint, Property) != EPropertyWritableState::Writable);
}

void FBlueprintEditorUtils::FindAndSetDebuggableBlueprintInstances()
{
	TMap< UBlueprint*, TArray< AActor* > > BlueprintsNeedingInstancesToDebug;

	// Find open blueprint editors that have no debug instances
	FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();	
	TArray<UObject*> EditedAssets = AssetEditorManager.GetAllEditedAssets();		
	for(int32 i=0; i<EditedAssets.Num(); i++)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>( EditedAssets[i] );
		if( Blueprint != nullptr )
		{
			if( Blueprint->GetObjectBeingDebugged() == nullptr )
			{
				BlueprintsNeedingInstancesToDebug.FindOrAdd( Blueprint );
			}			
		}
	}

	// If we have blueprints with no debug objects selected try to find a suitable on to debug
	if( BlueprintsNeedingInstancesToDebug.Num() != 0 )
	{	
		// Priority is in the following order.
		// 1. Selected objects with the exact same type as the blueprint being debugged
		// 2. UnSelected objects with the exact same type as the blueprint being debugged
		// 3. Selected objects based on the type of blueprint being debugged
		// 4. UnSelected objects based on the type of blueprint being debugged
		USelection* Selected = GEditor->GetSelectedActors();
		const bool bDisAllowDerivedTypes = false;
		TArray< UBlueprint* > BlueprintsToRefresh;
		for (TMap< UBlueprint*, TArray< AActor* > >::TIterator ObjIt(BlueprintsNeedingInstancesToDebug); ObjIt; ++ObjIt)
		{	
			UBlueprint* EachBlueprint = ObjIt.Key();
			bool bFoundItemToDebug = false;
			AActor* SimilarInstanceSelected = nullptr;
			AActor* SimilarInstanceUnselected = nullptr;

			// First check selected objects.
			if( Selected->Num() != 0 )
			{
				for (int32 iSelected = 0; iSelected < Selected->Num() ; iSelected++)
				{
					AActor* ObjectAsActor = Cast<AActor>( Selected->GetSelectedObject( iSelected ) );
					UWorld* ActorWorld = ObjectAsActor ? ObjectAsActor->GetWorld() : nullptr;
					if ((ActorWorld != nullptr) && (ActorWorld->WorldType != EWorldType::EditorPreview) && (ActorWorld->WorldType != EWorldType::Inactive))
					{
						if( IsObjectADebugCandidate(ObjectAsActor, EachBlueprint, true/*bInDisallowDerivedBlueprints*/ ) == true )
						{
							EachBlueprint->SetObjectBeingDebugged( ObjectAsActor );
							bFoundItemToDebug = true;
							BlueprintsToRefresh.Add( EachBlueprint );
							break;
						}
						else if( SimilarInstanceSelected == nullptr)
						{
							// If we haven't found a similar selected instance already check for one now
							if( IsObjectADebugCandidate(ObjectAsActor, EachBlueprint, false/*bInDisallowDerivedBlueprints*/ ) == true )
							{
								SimilarInstanceSelected = ObjectAsActor;
							}
						}
					}
				}
			}
			// Nothing of this type selected, just find any instance of one of these objects.
			if (!bFoundItemToDebug)
			{
				for (TObjectIterator<UObject> It; It; ++It)
				{
					AActor* ObjectAsActor = Cast<AActor>( *It );
					UWorld* ActorWorld = ObjectAsActor ? ObjectAsActor->GetWorld() : nullptr;
					if( ActorWorld && ( ActorWorld->WorldType != EWorldType::EditorPreview) && ActorWorld->WorldType != EWorldType::Inactive )
					{
						if( IsObjectADebugCandidate(ObjectAsActor, EachBlueprint, true/*bInDisallowDerivedBlueprints*/ ) == true )
						{
							EachBlueprint->SetObjectBeingDebugged( ObjectAsActor );
							bFoundItemToDebug = true;
							BlueprintsToRefresh.Add( EachBlueprint );
							break;
						}
						else if( SimilarInstanceUnselected == nullptr)
						{
							// If we haven't found a similar unselected instance already check for one now
							if( IsObjectADebugCandidate(ObjectAsActor, EachBlueprint, false/*bInDisallowDerivedBlueprints*/ ) == true )
							{
								SimilarInstanceUnselected = ObjectAsActor;
							}						
						}
					}
				}
			}

			// If we didn't find and exact type match, but we did find a related type use that.
			if( bFoundItemToDebug == false )
			{
				if( ( SimilarInstanceSelected != nullptr ) || ( SimilarInstanceUnselected != nullptr ) )
				{
					EachBlueprint->SetObjectBeingDebugged( SimilarInstanceSelected != nullptr ? SimilarInstanceSelected : SimilarInstanceUnselected );
					BlueprintsToRefresh.Add( EachBlueprint );
				}
			}
		}

		// Refresh all blueprint windows that we have made a change to the debugging selection of
		for (int32 iRefresh = 0; iRefresh < BlueprintsToRefresh.Num() ; iRefresh++)
		{
			// Ensure its a blueprint editor !
			TSharedPtr< IToolkit > FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(BlueprintsToRefresh[ iRefresh]);
			if (FoundAssetEditor.IsValid() && FoundAssetEditor->IsBlueprintEditor())
			{
				TSharedPtr<IBlueprintEditor> BlueprintEditor = StaticCastSharedPtr<IBlueprintEditor>(FoundAssetEditor);
				BlueprintEditor->RefreshEditors();
			}
		}
	}
}

void FBlueprintEditorUtils::AnalyticsTrackNewNode( UEdGraphNode *NewNode )
{
	UBlueprint* Blueprint = FindBlueprintForNodeChecked(NewNode);
	TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(Blueprint);
	if (FoundAssetEditor.IsValid() && FoundAssetEditor->IsBlueprintEditor()) 
	{ 
		TSharedPtr<IBlueprintEditor> BlueprintEditor = StaticCastSharedPtr<IBlueprintEditor>(FoundAssetEditor);
		BlueprintEditor->AnalyticsTrackNodeEvent(Blueprint, NewNode, false);
	}
}

bool FBlueprintEditorUtils::IsObjectADebugCandidate( AActor* InActorObject, UBlueprint* InBlueprint, bool bInDisallowDerivedBlueprints )
{
	const bool bPassesFlags = !InActorObject->HasAnyFlags(RF_ClassDefaultObject) && !InActorObject->IsPendingKill();
	bool bCanDebugThisObject = false;
	if( bInDisallowDerivedBlueprints == true )
	{
		bCanDebugThisObject = InActorObject->GetClass()->ClassGeneratedBy == InBlueprint;
	}
	else if(InBlueprint->GeneratedClass)
	{
		bCanDebugThisObject = InActorObject->IsA( InBlueprint->GeneratedClass );
	}
	
	return bPassesFlags && bCanDebugThisObject;
}

bool FBlueprintEditorUtils::PropertyValueFromString(const UProperty* Property, const FString& StrValue, uint8* Container)
{
	return PropertyValueFromString_Direct(Property, StrValue, Property->ContainerPtrToValuePtr<uint8>(Container));
}

bool FBlueprintEditorUtils::PropertyValueFromString_Direct(const UProperty* Property, const FString& StrValue, uint8* DirectValue)
{
	bool bParseSucceeded = true;
	if (!Property->IsA(UStructProperty::StaticClass()))
	{
		if (Property->IsA(UIntProperty::StaticClass()))
		{
			int32 IntValue = 0;
			bParseSucceeded = FDefaultValueHelper::ParseInt(StrValue, IntValue);
			CastChecked<UIntProperty>(Property)->SetPropertyValue(DirectValue, IntValue);
		}
		else if (Property->IsA(UFloatProperty::StaticClass()))
		{
			float FloatValue = 0.0f;
			bParseSucceeded = FDefaultValueHelper::ParseFloat(StrValue, FloatValue);
			CastChecked<UFloatProperty>(Property)->SetPropertyValue(DirectValue, FloatValue);
		}
		else if (const UByteProperty* ByteProperty = Cast<const UByteProperty>(Property))
		{
			int32 IntValue = 0;
			if (const UEnum* Enum = ByteProperty->Enum)
			{
				IntValue = Enum->GetValueByName(FName(*StrValue));
				bParseSucceeded = (INDEX_NONE != IntValue);

				// If the parse did not succeed, clear out the int to keep the enum value valid
				if (!bParseSucceeded)
				{
					IntValue = 0;
				}
			}
			else
			{
				bParseSucceeded = FDefaultValueHelper::ParseInt(StrValue, IntValue);
			}
			bParseSucceeded = bParseSucceeded && (IntValue <= 255) && (IntValue >= 0);
			ByteProperty->SetPropertyValue(DirectValue, IntValue);
		}
		else if (const UEnumProperty* EnumProperty = Cast<const UEnumProperty>(Property))
		{
			int64 IntValue = EnumProperty->GetEnum()->GetValueByName(FName(*StrValue));
			bParseSucceeded = (INDEX_NONE != IntValue);

			// If the parse did not succeed, clear out the int to keep the enum value valid
			if (!bParseSucceeded)
			{
				IntValue = 0;
			}
			bParseSucceeded = bParseSucceeded && (IntValue <= 255) && (IntValue >= 0);
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(DirectValue, IntValue);
		}
		else if (Property->IsA(UStrProperty::StaticClass()))
		{
			CastChecked<UStrProperty>(Property)->SetPropertyValue(DirectValue, StrValue);
		}
		else if (Property->IsA(UBoolProperty::StaticClass()))
		{
			CastChecked<UBoolProperty>(Property)->SetPropertyValue(DirectValue, StrValue.ToBool());
		}
		else if (Property->IsA(UNameProperty::StaticClass()))
		{
			CastChecked<UNameProperty>(Property)->SetPropertyValue(DirectValue, FName(*StrValue));
		}
		else if (Property->IsA(UTextProperty::StaticClass()))
		{
			FStringOutputDevice ImportError;
			const TCHAR* EndOfParsedBuff = Property->ImportText(*StrValue, DirectValue, PPF_SerializedAsImportText, nullptr, &ImportError);
			bParseSucceeded = EndOfParsedBuff && ImportError.IsEmpty();
		}
		else
		{
			// Empty array-like properties need to use "()" in order to import correctly (as array properties export comma separated within a set of brackets)
			const TCHAR* const ValueToImport = (StrValue.IsEmpty() && (Property->IsA(UArrayProperty::StaticClass()) || Property->IsA(UMulticastDelegateProperty::StaticClass())))
				? TEXT("()")
				: *StrValue;

			FStringOutputDevice ImportError;
			const TCHAR* EndOfParsedBuff = Property->ImportText(*StrValue, DirectValue, PPF_SerializedAsImportText, nullptr, &ImportError);
			bParseSucceeded = EndOfParsedBuff && ImportError.IsEmpty();
		}
	}
	else
	{
		static UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
		static UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();
		static UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();
		static UScriptStruct* LinearColorStruct = TBaseStructure<FLinearColor>::Get();

		const UStructProperty* StructProperty = CastChecked<UStructProperty>(Property);

		// Struct properties must be handled differently, unfortunately.  We only support FVector, FRotator, and FTransform
		if (StructProperty->Struct == VectorStruct)
		{
			FVector V = FVector::ZeroVector;
			bParseSucceeded = FDefaultValueHelper::ParseVector(StrValue, V);
			Property->CopyCompleteValue(DirectValue, &V);
		}
		else if (StructProperty->Struct == RotatorStruct)
		{
			FRotator R = FRotator::ZeroRotator;
			bParseSucceeded = FDefaultValueHelper::ParseRotator(StrValue, R);
			Property->CopyCompleteValue(DirectValue, &R);
		}
		else if (StructProperty->Struct == TransformStruct)
		{
			FTransform T = FTransform::Identity;
			bParseSucceeded = T.InitFromString(StrValue);
			Property->CopyCompleteValue(DirectValue, &T);
		}
		else if (StructProperty->Struct == LinearColorStruct)
		{
			FLinearColor Color;
			// Color form: "(R=%f,G=%f,B=%f,A=%f)"
			bParseSucceeded = Color.InitFromString(StrValue);
			Property->CopyCompleteValue(DirectValue, &Color);
		}
		else if (StructProperty->Struct)
		{
			const UScriptStruct* Struct = StructProperty->Struct;
			const int32 StructSize = Struct->GetStructureSize() * StructProperty->ArrayDim;
			StructProperty->InitializeValue(DirectValue);
			ensure(1 == StructProperty->ArrayDim);
			bParseSucceeded = FStructureEditorUtils::Fill_MakeStructureDefaultValue(Cast<const UUserDefinedStruct>(Struct), DirectValue);

			FStringOutputDevice ImportError;
			const TCHAR* EndOfParsedBuff = StructProperty->ImportText(StrValue.IsEmpty() ? TEXT("()") : *StrValue, DirectValue, PPF_SerializedAsImportText, nullptr, &ImportError);
			bParseSucceeded &= EndOfParsedBuff && ImportError.IsEmpty();
		}
	}

	return bParseSucceeded;
}

bool FBlueprintEditorUtils::PropertyValueToString(const UProperty* Property, const uint8* Container, FString& OutForm)
{
	return PropertyValueToString_Direct(Property, Property->ContainerPtrToValuePtr<const uint8>(Container), OutForm);
}

bool FBlueprintEditorUtils::PropertyValueToString_Direct(const UProperty* Property, const uint8* DirectValue, FString& OutForm)
{
	check(Property && DirectValue);
	OutForm.Reset();

	const UStructProperty* StructProperty = Cast<UStructProperty>(Property);
	if (StructProperty)
	{
		static UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
		static UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();
		static UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();
		static UScriptStruct* LinearColorStruct = TBaseStructure<FLinearColor>::Get();

		// Struct properties must be handled differently, unfortunately.  We only support FVector, FRotator, and FTransform
		if (StructProperty->Struct == VectorStruct)
		{
			FVector Vector;
			Property->CopyCompleteValue(&Vector, DirectValue);
			OutForm = FString::Printf(TEXT("%f,%f,%f"), Vector.X, Vector.Y, Vector.Z);
		}
		else if (StructProperty->Struct == RotatorStruct)
		{
			FRotator Rotator;
			Property->CopyCompleteValue(&Rotator, DirectValue);
			OutForm = FString::Printf(TEXT("%f,%f,%f"), Rotator.Pitch, Rotator.Yaw, Rotator.Roll);
		}
		else if (StructProperty->Struct == TransformStruct)
		{
			FTransform Transform;
			Property->CopyCompleteValue(&Transform, DirectValue);
			OutForm = Transform.ToString();
		}
		else if (StructProperty->Struct == LinearColorStruct)
		{
			FLinearColor Color;
			Property->CopyCompleteValue(&Color, DirectValue);
			OutForm = Color.ToString();
		}
	}

	bool bSucceeded = true;
	if (OutForm.IsEmpty())
	{
		const uint8* DefaultValue = DirectValue;

		UUserDefinedStruct* UserDefinedStruct = StructProperty ? Cast<UUserDefinedStruct>(StructProperty->Struct) : nullptr;
		FStructOnScope StructOnScope(UserDefinedStruct);
		if (UserDefinedStruct && StructOnScope.IsValid())
		{
			UserDefinedStruct->InitializeDefaultValue(StructOnScope.GetStructMemory());
			DefaultValue = StructOnScope.GetStructMemory();
		}
		
		bSucceeded = Property->ExportText_Direct(OutForm, DirectValue, DefaultValue, nullptr, PPF_SerializedAsImportText);
	}
	return bSucceeded;
}

FName FBlueprintEditorUtils::GenerateUniqueGraphName(UBlueprint* const BlueprintOuter, FString const& ProposedName)
{
	FName UniqueGraphName(*ProposedName);

	int32 CountPostfix = 1;
	while (!FBlueprintEditorUtils::IsGraphNameUnique(BlueprintOuter, UniqueGraphName))
	{
		UniqueGraphName = FName(*FString::Printf(TEXT("%s%i"), *ProposedName, CountPostfix));
		++CountPostfix;
	}

	return UniqueGraphName;
}

bool FBlueprintEditorUtils::CheckIfNodeConnectsToSelection(UEdGraphNode* InNode, const TSet<UEdGraphNode*>& InSelectionSet)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	for (int32 PinIndex = 0; PinIndex < InNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = InNode->Pins[PinIndex];
		if(Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != Schema->PC_Exec)
		{
			for (int32 LinkIndex = 0; LinkIndex < Pin->LinkedTo.Num(); ++LinkIndex)
			{
				UEdGraphPin* LinkedToPin = Pin->LinkedTo[LinkIndex];

				// The InNode, which is NOT in the new function, is checking if one of it's pins IS in the function, return true if it is. If not, check the node.
				if(InSelectionSet.Contains(LinkedToPin->GetOwningNode()))
				{
					return true;
				}

				// Check the node recursively to see if it is connected back with selection.
				if(CheckIfNodeConnectsToSelection(LinkedToPin->GetOwningNode(), InSelectionSet))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FBlueprintEditorUtils::CheckIfSelectionIsCycling(const TSet<UEdGraphNode*>& InSelectionSet, FCompilerResultsLog& InMessageLog)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	for (UEdGraphNode* Node : InSelectionSet)
	{
		if (Node)
		{
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* Pin = Node->Pins[PinIndex];
				if(Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != Schema->PC_Exec)
				{
					for (int32 LinkIndex = 0; LinkIndex < Pin->LinkedTo.Num(); ++LinkIndex)
					{
						UEdGraphPin* LinkedToPin = Pin->LinkedTo[LinkIndex];

						// Check to see if this node, which is IN the selection, has any connections OUTSIDE the selection
						// If it does, check to see if those nodes have any connections IN the selection
						if(!InSelectionSet.Contains(LinkedToPin->GetOwningNode()))
						{
							if(CheckIfNodeConnectsToSelection(LinkedToPin->GetOwningNode(), InSelectionSet))
							{
								InMessageLog.Error(*LOCTEXT("DependencyCyleDetected_Error", "Dependency cycle detected, preventing node @@ from being scheduled").ToString(), LinkedToPin->GetOwningNode());
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool FBlueprintEditorUtils::IsPaletteActionReadOnly(TSharedPtr<FEdGraphSchemaAction> ActionIn, TSharedPtr<FBlueprintEditor> const BlueprintEditorIn)
{
	check(BlueprintEditorIn.IsValid());
	bool bIsReadOnly = false;
	if(!BlueprintEditorIn->InEditingMode())
	{
		bIsReadOnly = true;
	}
	else
	{
		UBlueprint const* const BlueprintObj = BlueprintEditorIn->GetBlueprintObj();	
		if(ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Graph* GraphAction = (FEdGraphSchemaAction_K2Graph*)ActionIn.Get();
			// No graph is evidence of an overridable function, don't let the user modify it
			if(GraphAction->EdGraph == nullptr)
			{
				bIsReadOnly = true;
			}
			else
			{
				// Graphs that cannot be deleted or re-named are read-only
				if ( !(GraphAction->EdGraph->bAllowDeletion || GraphAction->EdGraph->bAllowRenaming) )
				{
					bIsReadOnly = true;
				}
				else
				{
					if(GraphAction->GraphType == EEdGraphSchemaAction_K2Graph::Function)
					{
						// Check if the function is an override
						UFunction* OverrideFunc = FindField<UFunction>(BlueprintObj->ParentClass, GraphAction->FuncName);
						if ( OverrideFunc != nullptr )
						{
							bIsReadOnly = true;
						}
					}
					else if(GraphAction->GraphType == EEdGraphSchemaAction_K2Graph::Interface)
					{
						// Interfaces cannot be renamed
						bIsReadOnly = true;
					}
				}
			}
		}
		else if(ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Var* VarAction = (FEdGraphSchemaAction_K2Var*)ActionIn.Get();

			bIsReadOnly = true;

			if( FBlueprintEditorUtils::FindNewVariableIndex(BlueprintObj, VarAction->GetVariableName()) != INDEX_NONE)
			{
				bIsReadOnly = false;
			}
			else if(BlueprintObj->FindTimelineTemplateByVariableName(VarAction->GetVariableName()))
			{
				bIsReadOnly = false;
			}
			else if(BlueprintEditorIn->CanAccessComponentsMode())
			{
				// Wasn't in the introduced variable list; try to find the associated SCS node
				//@TODO: The SCS-generated variables should be in the variable list and have a link back;
				// As it stands, you cannot do any metadata operations on a SCS variable, and you have to do icky code like the following
				TArray<USCS_Node*> Nodes = BlueprintObj->SimpleConstructionScript->GetAllNodes();
				for (TArray<USCS_Node*>::TConstIterator NodeIt(Nodes); NodeIt; ++NodeIt)
				{
					USCS_Node* CurrentNode = *NodeIt;
					if (CurrentNode->GetVariableName() == VarAction->GetVariableName())
					{
						bIsReadOnly = false;
						break;
					}
				}
			}
		}
		else if(ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Delegate* DelegateAction = (FEdGraphSchemaAction_K2Delegate*)ActionIn.Get();

			if( FBlueprintEditorUtils::FindNewVariableIndex(BlueprintObj, DelegateAction->GetDelegateName()) == INDEX_NONE)
			{
				bIsReadOnly = true;
			}
		}
		else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2Event::StaticGetTypeId())
		{
			FEdGraphSchemaAction_K2Event* EventAction = (FEdGraphSchemaAction_K2Event*)ActionIn.Get();
			UK2Node* AssociatedNode = EventAction->NodeTemplate;

			bIsReadOnly = (AssociatedNode == nullptr) || (!AssociatedNode->bCanRenameNode);	
		}
		else if (ActionIn->GetTypeId() == FEdGraphSchemaAction_K2InputAction::StaticGetTypeId())
		{
			bIsReadOnly = true;
		}
	}

	return bIsReadOnly;
}

struct FUberGraphHelper
{
	static void GetAll(const UBlueprint* Blueprint, TArray<UEdGraph*>& OutGraphs)
	{
		for (UEdGraph* UberGraph : Blueprint->UbergraphPages)
		{
			OutGraphs.Add(UberGraph);
			UberGraph->GetAllChildrenGraphs(OutGraphs);
		}
	}
};

FName FBlueprintEditorUtils::GetFunctionNameFromClassByGuid(const UClass* InClass, const FGuid FunctionGuid)
{
	TArray<UBlueprint*> Blueprints;
	UBlueprint::GetBlueprintHierarchyFromClass(InClass, Blueprints);

	for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
	{
		UBlueprint* Blueprint = Blueprints[BPIndex];
		for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
		{
			if (FunctionGraph && FunctionGraph->GraphGuid == FunctionGuid)
			{
				return FunctionGraph->GetFName();
			}
		}

		for (UEdGraph* FunctionGraph : Blueprint->DelegateSignatureGraphs)
		{
			if (FunctionGraph && FunctionGraph->GraphGuid == FunctionGuid)
			{
				const FString Name = FunctionGraph->GetName() + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX;
				return FName(*Name);
			}
		}

		//FUNCTIONS BASED ON CUSTOM EVENTS:
		TArray<UEdGraph*> UberGraphs;
		FUberGraphHelper::GetAll(Blueprint, UberGraphs);
		for (const UEdGraph* UberGraph : UberGraphs)
		{
			TArray<UK2Node_CustomEvent*> CustomEvents;
			UberGraph->GetNodesOfClass(CustomEvents);
			for (const UK2Node_CustomEvent* CustomEvent : CustomEvents)
			{
				if (!CustomEvent->bOverrideFunction && (CustomEvent->NodeGuid == FunctionGuid))
				{
					ensure(CustomEvent->CustomFunctionName != NAME_None);
					return CustomEvent->CustomFunctionName;
				}
			}
		}
	}

	return NAME_None;
}

bool FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(const UClass* InClass, const FName FunctionName, FGuid& FunctionGuid)
{
	if (FunctionName != NAME_None)
	{
		TArray<UBlueprint*> Blueprints;
		UBlueprint::GetBlueprintHierarchyFromClass(InClass, Blueprints);

		for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
		{
			UBlueprint* Blueprint = Blueprints[BPIndex];
			for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
			{
				if (FunctionGraph && FunctionGraph->GetFName() == FunctionName)
				{
					FunctionGuid = FunctionGraph->GraphGuid;
					return true;
				}
			}

			FString BaseDelegateSignatureName = FunctionName.ToString();
			if (BaseDelegateSignatureName.RemoveFromEnd(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX))
			{
				const FName GraphName(*BaseDelegateSignatureName);
				for (UEdGraph* FunctionGraph : Blueprint->DelegateSignatureGraphs)
				{
					if (FunctionGraph && FunctionGraph->GetFName() == GraphName)
					{
						FunctionGuid = FunctionGraph->GraphGuid;
						return true;
					}
				}
			}

			TArray<UEdGraph*> UberGraphs;
			FUberGraphHelper::GetAll(Blueprint, UberGraphs);
			for (const UEdGraph* UberGraph : UberGraphs)
			{
				TArray<UK2Node_CustomEvent*> CustomEvents;
				UberGraph->GetNodesOfClass(CustomEvents);
				for (const UK2Node_CustomEvent* CustomEvent : CustomEvents)
				{
					if (!CustomEvent->bOverrideFunction
						&& (CustomEvent->CustomFunctionName == FunctionName)
						&& CustomEvent->NodeGuid.IsValid())
					{
						FunctionGuid = CustomEvent->NodeGuid;
						return true;
					}
				}
			}
		}
	}

	FunctionGuid.Invalidate();

	return false;
}

UK2Node_EditablePinBase* FBlueprintEditorUtils::GetEntryNode(const UEdGraph* InGraph)
{
	UK2Node_EditablePinBase* Result = nullptr;
	if (InGraph)
	{
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		InGraph->GetNodesOfClass(EntryNodes);
		if (EntryNodes.Num() > 0)
		{
			if (EntryNodes[0]->IsEditable())
			{
				Result = EntryNodes[0];
			}
		}
		else
		{
			TArray<UK2Node_Tunnel*> TunnelNodes;
			InGraph->GetNodesOfClass(TunnelNodes);

			if (TunnelNodes.Num() > 0)
			{
				// Iterate over the tunnel nodes, and try to find an entry and exit
				for (int32 i = 0; i < TunnelNodes.Num(); i++)
				{
					UK2Node_Tunnel* Node = TunnelNodes[i];
					// Composite nodes should never be considered for function entry / exit, since we're searching for a graph's terminals
					if (Node->IsEditable() && !Node->IsA(UK2Node_Composite::StaticClass()))
					{
						if (Node->bCanHaveOutputs)
						{
							Result = Node;
							break;
						}
					}
				}
			}
		}
	}
	return Result;
}

void FBlueprintEditorUtils::GetEntryAndResultNodes(const UEdGraph* InGraph, TWeakObjectPtr<class UK2Node_EditablePinBase>& OutEntryNode, TWeakObjectPtr<class UK2Node_EditablePinBase>& OutResultNode)
{
	if (InGraph)
	{
		// There are a few different potential configurations for editable graphs (FunctionEntry/Result, Tunnel Pairs, etc).
		// Step through each case until we find one that matches what appears to be in the graph.  This could be improved if
		// we want to add more robust typing to the graphs themselves

		// Case 1:  Function Entry / Result Pair ------------------
		TArray<UK2Node_FunctionEntry*> EntryNodes;
		InGraph->GetNodesOfClass(EntryNodes);

		if (EntryNodes.Num() > 0)
		{
			if (EntryNodes[0]->IsEditable())
			{
				OutEntryNode = EntryNodes[0];

				// Find a result node
				TArray<UK2Node_FunctionResult*> ResultNodes;
				InGraph->GetNodesOfClass(ResultNodes);

				UK2Node_FunctionResult* ResultNode = ResultNodes.Num() ? ResultNodes[0] : nullptr;
				// Note:  we assume that if the entry is editable, the result is too (since the entry node is guaranteed to be there on graph creation, but the result isn't)
				if( ResultNode )
				{
					OutResultNode = ResultNode;
				}
			}
		}
		else
		{
			// Case 2:  Tunnel Pair -----------------------------------
			TArray<UK2Node_Tunnel*> TunnelNodes;
			InGraph->GetNodesOfClass(TunnelNodes);

			if (TunnelNodes.Num() > 0)
			{
				// Iterate over the tunnel nodes, and try to find an entry and exit
				for (int32 i = 0; i < TunnelNodes.Num(); i++)
				{
					UK2Node_Tunnel* Node = TunnelNodes[i];
					// Composite nodes should never be considered for function entry / exit, since we're searching for a graph's terminals
					if (Node->IsEditable() && !Node->IsA(UK2Node_Composite::StaticClass()))
					{
						if (Node->bCanHaveOutputs)
						{
							ensure(!OutEntryNode.IsValid());
							OutEntryNode = Node;
						}
						else if (Node->bCanHaveInputs)
						{
							ensure(!OutResultNode.IsValid());
							OutResultNode = Node;
						}
					}
				}
			}
		}
	}
}

FKismetUserDeclaredFunctionMetadata* FBlueprintEditorUtils::GetGraphFunctionMetaData(const UEdGraph* InGraph)
{
	if (InGraph)
	{
		UK2Node_EditablePinBase* FunctionEntryNode = GetEntryNode(InGraph);
		if (UK2Node_FunctionEntry* TypedEntryNode = Cast<UK2Node_FunctionEntry>(FunctionEntryNode))
		{
			return &(TypedEntryNode->MetaData);
		}
		else if (UK2Node_Tunnel* TunnelNode = ExactCast<UK2Node_Tunnel>(FunctionEntryNode))
		{
			// Must be exactly a tunnel, not a macro instance
			return &(TunnelNode->MetaData);
		}
	}

	return nullptr;
}

FText FBlueprintEditorUtils::GetGraphDescription(const UEdGraph* InGraph)
{
	if (FKismetUserDeclaredFunctionMetadata* MetaData = GetGraphFunctionMetaData(InGraph))
	{
		 return MetaData->ToolTip;
	}

	return LOCTEXT( "NoGraphTooltip", "(None)" );
}

bool FBlueprintEditorUtils::CheckIfGraphHasLatentFunctions(UEdGraph* InGraph)
{
	struct Local
	{
		static bool CheckIfGraphHasLatentFunctions(UEdGraph* InGraphToCheck, TArray<UEdGraph*>& InspectedGraphList)
		{
			UK2Node_EditablePinBase* EntryNode = GetEntryNode(InGraphToCheck);

			UK2Node_Tunnel* TunnelNode = ExactCast<UK2Node_Tunnel>(EntryNode);
			if(!TunnelNode)
			{
				// No tunnel, no metadata.
				return false;
			}

			if(TunnelNode->MetaData.HasLatentFunctions != INDEX_NONE)
			{
				return TunnelNode->MetaData.HasLatentFunctions > 0;
			}
			else
			{
				// Add all graphs to the list of already inspected, this prevents circular inclusion issues.
				InspectedGraphList.Add(InGraphToCheck);

				for( const UEdGraphNode* Node : InGraphToCheck->Nodes )
				{
					if(const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
					{
						// Check any function call nodes to see if they are latent.
						UFunction* TargetFunction = CallFunctionNode->GetTargetFunction();
						if (TargetFunction && TargetFunction->HasMetaData(FBlueprintMetadata::MD_Latent))
						{
							TunnelNode->MetaData.HasLatentFunctions = 1;
							return true;
						}
					}
					else if(const UK2Node_BaseAsyncTask* BaseAsyncNode = Cast<UK2Node_BaseAsyncTask>(Node))
					{
						// Async tasks are latent nodes
						TunnelNode->MetaData.HasLatentFunctions = 1;
						return true;
					}
					else if(const UK2Node_MacroInstance* MacroInstanceNode = Cast<UK2Node_MacroInstance>(Node))
					{
						// Any macro graphs that haven't already been checked need to be checked for latent function calls
						if(InspectedGraphList.Find(MacroInstanceNode->GetMacroGraph()) == INDEX_NONE)
						{
							if(CheckIfGraphHasLatentFunctions(MacroInstanceNode->GetMacroGraph(), InspectedGraphList))
							{
								TunnelNode->MetaData.HasLatentFunctions = 1;
								return true;
							}
						}
					}
					else if(const UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(Node))
					{
						// Any collapsed graphs that haven't already been checked need to be checked for latent function calls
						if(InspectedGraphList.Find(CompositeNode->BoundGraph) == INDEX_NONE)
						{
							if(CheckIfGraphHasLatentFunctions(CompositeNode->BoundGraph, InspectedGraphList))
							{
								TunnelNode->MetaData.HasLatentFunctions = 1;
								return true;
							}
						}
					}
				}

				TunnelNode->MetaData.HasLatentFunctions = 0;
				return false;
			}
		}
	};

	TArray<UEdGraph*> InspectedGraphList;
	return Local::CheckIfGraphHasLatentFunctions(InGraph, InspectedGraphList);
}

void FBlueprintEditorUtils::PostSetupObjectPinType(UBlueprint* InBlueprint, FBPVariableDescription& InOutVarDesc)
{
	UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if ((InOutVarDesc.VarType.PinCategory == K2Schema->PC_Object) || (InOutVarDesc.VarType.PinCategory == K2Schema->PC_Interface))
	{
		if (InOutVarDesc.VarType.PinSubCategory == K2Schema->PSC_Self)
		{
			InOutVarDesc.VarType.PinSubCategory.Empty();
			InOutVarDesc.VarType.PinSubCategoryObject = *InBlueprint->GeneratedClass;
		}
		else if (!InOutVarDesc.VarType.PinSubCategoryObject.IsValid())
		{
			// Fall back to UObject if the given type is not valid. This can happen for example if a variable is removed from
			// a Blueprint parent class along with the variable's type and the user then attempts to recreate the missing variable
			// through a stale variable node's context menu in a child Blueprint graph.
			InOutVarDesc.VarType.PinSubCategory.Empty();
			InOutVarDesc.VarType.PinSubCategoryObject = UObject::StaticClass();
		}

		// if it's a PC_Object, then it should have an associated UClass object
		check(InOutVarDesc.VarType.PinSubCategoryObject.IsValid());
		const UClass* ClassObject = Cast<UClass>(InOutVarDesc.VarType.PinSubCategoryObject.Get());
		check(ClassObject != nullptr);

		if (ClassObject->IsChildOf(AActor::StaticClass()))
		{
			// prevent Actor variables from having default values (because Blueprint templates are library elements that can 
			// bridge multiple levels and different levels might not have the actor that the default is referencing).
			InOutVarDesc.PropertyFlags |= CPF_DisableEditOnTemplate;
		}
	}
}

const FSlateBrush* FBlueprintEditorUtils::GetIconFromPin( const FEdGraphPinType& PinType, bool bIsLarge )
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	
	const FSlateBrush* IconBrush = FEditorStyle::GetBrush(TEXT("Kismet.VariableList.TypeIcon"));
	const UObject* PinSubObject = PinType.PinSubCategoryObject.Get();
	if( PinType.IsArray() && PinType.PinCategory != K2Schema->PC_Exec )
	{
		IconBrush = FEditorStyle::GetBrush(TEXT("Kismet.VariableList.ArrayTypeIcon"));
	}
	else if (PinType.IsMap() && PinType.PinCategory != K2Schema->PC_Exec)
	{
		IconBrush = FEditorStyle::GetBrush(TEXT("Kismet.VariableList.MapKeyTypeIcon"));
	}
	else if (PinType.IsSet() && PinType.PinCategory != K2Schema->PC_Exec)
	{
		if( bIsLarge )
		{
			IconBrush = FEditorStyle::GetBrush(TEXT("Kismet.VariableList.SetTypeIconLarge"));
		}
		else
		{
			IconBrush = FEditorStyle::GetBrush(TEXT("Kismet.VariableList.SetTypeIcon"));
		}
	}
	else if( PinSubObject )
	{
		UClass* VarClass = FindObject<UClass>(ANY_PACKAGE, *PinSubObject->GetName());
		if( VarClass )
		{
			IconBrush = FSlateIconFinder::FindIconBrushForClass( VarClass );
		}
	}
	return IconBrush;
}

const FSlateBrush* FBlueprintEditorUtils::GetSecondaryIconFromPin(const FEdGraphPinType& PinType)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (PinType.IsMap() && PinType.PinCategory != K2Schema->PC_Exec)
	{
		return FEditorStyle::GetBrush(TEXT("Kismet.VariableList.MapValueTypeIcon"));
	}
	return nullptr;
}

bool FBlueprintEditorUtils::HasGetTypeHash(const FEdGraphPinType& PinType)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if(PinType.PinCategory == K2Schema->PC_Boolean)
	{
		return false;
	}

	if (PinType.PinCategory == K2Schema->PC_Text)
	{
		return false;
	}

	if (PinType.PinCategory != K2Schema->PC_Struct)
	{
		// even object or class types can be hashed, no reason to investigate further
		return true;
	}

	const UScriptStruct* StructType = Cast<const UScriptStruct>(PinType.PinSubCategoryObject.Get());
	if( StructType )
	{
		return StructHasGetTypeHash(StructType);
	}
	return false;
}

bool FBlueprintEditorUtils::PropertyHasGetTypeHash(const UProperty* PropertyType)
{
	return PropertyType->HasAllPropertyFlags(CPF_HasGetValueTypeHash);
} 

bool FBlueprintEditorUtils::StructHasGetTypeHash(const UScriptStruct* StructType)
{
	if (StructType->IsNative())
	{
		return StructType->GetCppStructOps() && StructType->GetCppStructOps()->HasGetTypeHash();
	}
	else
	{
		// if every member can be hashed (or is a UBoolProperty, which is specially 
		// handled by UScriptStruct::GetStructTypeHash) then we can hash the struct:
		for (TFieldIterator<UProperty> It(StructType); It; ++It)
		{
			if (Cast<UBoolProperty>(*It))
			{
				continue;
			}
			else
			{
				if (!FBlueprintEditorUtils::PropertyHasGetTypeHash(*It) )
				{
					return false;
				}
			}
		}
		return true;
	}
}

FText FBlueprintEditorUtils::GetFriendlyClassDisplayName(const UClass* Class)
{
	if (Class != nullptr)
	{
		return Class->GetDisplayNameText();
	}
	else
	{
		return LOCTEXT("ClassIsNull", "None");
	}
}

FString FBlueprintEditorUtils::GetClassNameWithoutSuffix(const UClass* Class)
{
	if (Class != nullptr)
	{
		FString Result = Class->GetName();
		if (Class->ClassGeneratedBy != nullptr)
		{
			Result.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);
		}

		return Result;
	}
	else
	{
		return LOCTEXT("ClassIsNull", "None").ToString();
	}
}

UK2Node_FunctionResult* FBlueprintEditorUtils::FindOrCreateFunctionResultNode(UK2Node_EditablePinBase* InFunctionEntryNode)
{
	UK2Node_FunctionResult* FunctionResult = nullptr;

	if (InFunctionEntryNode)
	{
		UEdGraph* Graph = InFunctionEntryNode->GetGraph();

		TArray<UK2Node_FunctionResult*> ResultNode;
		if (Graph)
		{
			Graph->GetNodesOfClass(ResultNode);
		}

		if (Graph && ResultNode.Num() == 0)
		{
			FGraphNodeCreator<UK2Node_FunctionResult> ResultNodeCreator(*Graph);
			FunctionResult = ResultNodeCreator.CreateNode();

			const UEdGraphSchema_K2* Schema = Cast<const UEdGraphSchema_K2>(FunctionResult->GetSchema());
			FunctionResult->NodePosX = InFunctionEntryNode->NodePosX + InFunctionEntryNode->NodeWidth + 256;
			FunctionResult->NodePosY = InFunctionEntryNode->NodePosY;
			FunctionResult->bIsEditable = true;
			UEdGraphSchema_K2::SetNodeMetaData(FunctionResult, FNodeMetadata::DefaultGraphNode);
			ResultNodeCreator.Finalize();

			// Connect the function entry to the result node, if applicable
			UEdGraphPin* ThenPin = Schema->FindExecutionPin(*InFunctionEntryNode, EGPD_Output);
			UEdGraphPin* ReturnPin = Schema->FindExecutionPin(*FunctionResult, EGPD_Input);

			if(ThenPin->LinkedTo.Num() == 0) 
			{
				ThenPin->MakeLinkTo(ReturnPin);
			}
			else
			{
				// Bump the result node up a bit, so it's less likely to fall behind the node the entry is already connected to
				FunctionResult->NodePosY -= 100;
			}
		}
		else
		{
			FunctionResult = ResultNode[0];
		}
	}

	return FunctionResult;
}

void FBlueprintEditorUtils::HandleDisableEditableWhenInherited(UObject* ModifiedObject, TArray<UObject*>& ArchetypeInstances)
{
	for (int32 Index = ArchetypeInstances.Num() - 1; Index >= 0; --Index)
	{
		UObject* ArchetypeInstance = ArchetypeInstances[Index];
		if (ArchetypeInstance != ModifiedObject)
		{
			UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ArchetypeInstance->GetOuter());
			if (BPGC)
			{
				UInheritableComponentHandler* ICH = BPGC->GetInheritableComponentHandler(false);
				check(ICH);

				ICH->RemoveOverridenComponentTemplate(ICH->FindKey(CastChecked<UActorComponent>(ArchetypeInstance)));
			}
		}
	}
}

UClass* FBlueprintEditorUtils::GetNativeParent(const UBlueprint* BP)
{
	UClass* Ret = BP->ParentClass;
	while(Ret && !Ret->HasAnyClassFlags(CLASS_Native))
	{
		Ret = Ret->GetSuperClass();
	}
	return Ret;
}

bool FBlueprintEditorUtils::ImplentsGetWorld(const UBlueprint* BP)
{
	if(UClass* NativeParent = GetNativeParent(BP))
	{
		return NativeParent->GetDefaultObject()->ImplementsGetWorld();
	}
	return false;
}

struct FComponentInstancingDataUtils
{
	// Recursively gathers properties that differ from class/struct defaults, and fills out the cooked property list structure.
	static void RecursivePropertyGather(UStruct* InStruct, const uint8* DataPtr, const uint8* DefaultDataPtr, FBlueprintCookedComponentInstancingData& OutData)
	{
		for (UProperty* Property = InStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			// Skip editor-only properties since they won't be compiled in a non-editor configuration. Also skip transient and deprecated properties since they won't be serialized on save/duplicate. 
			if (!Property->IsEditorOnlyProperty()
				&& !Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient | CPF_Deprecated))
			{
				for (int32 Idx = 0; Idx < Property->ArrayDim; Idx++)
				{
					const uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(DataPtr, Idx);
					const uint8* DefaultPropertyValue = Property->ContainerPtrToValuePtrForDefaults<uint8>(InStruct, DefaultDataPtr, Idx);

					FBlueprintComponentChangedPropertyInfo ChangedPropertyInfo;
					ChangedPropertyInfo.PropertyName = Property->GetFName();
					ChangedPropertyInfo.ArrayIndex = Idx;
					ChangedPropertyInfo.PropertyScope = InStruct;

					if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
					{
						int32 NumChangedProperties = OutData.ChangedPropertyList.Num();

						RecursivePropertyGather(StructProperty->Struct, PropertyValue, DefaultPropertyValue, OutData);

						// Prepend the struct property only if there is at least one changed sub-property.
						if (NumChangedProperties < OutData.ChangedPropertyList.Num())
						{
							OutData.ChangedPropertyList.Insert(ChangedPropertyInfo, NumChangedProperties);
						}
					}
					else if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property))
					{
						FScriptArrayHelper ArrayValueHelper(ArrayProperty, PropertyValue);
						FScriptArrayHelper DefaultArrayValueHelper(ArrayProperty, DefaultPropertyValue);

						int32 NumChangedProperties = OutData.ChangedPropertyList.Num();
						FBlueprintComponentChangedPropertyInfo ChangedArrayPropertyInfo = ChangedPropertyInfo;

						for (int32 ArrayValueIndex = 0; ArrayValueIndex < ArrayValueHelper.Num(); ++ArrayValueIndex)
						{
							ChangedArrayPropertyInfo.ArrayIndex = ArrayValueIndex;
							const uint8* ArrayPropertyValue = ArrayValueHelper.GetRawPtr(ArrayValueIndex);

							if (ArrayValueIndex < DefaultArrayValueHelper.Num())
							{
								const uint8* DefaultArrayPropertyValue = DefaultArrayValueHelper.GetRawPtr(ArrayValueIndex);

								if (UStructProperty* InnerStructProperty = Cast<UStructProperty>(ArrayProperty->Inner))
								{
									int32 NumChangedArrayProperties = OutData.ChangedPropertyList.Num();

									RecursivePropertyGather(InnerStructProperty->Struct, ArrayPropertyValue, DefaultArrayPropertyValue, OutData);

									// Prepend the struct property only if there is at least one changed sub-property.
									if (NumChangedArrayProperties < OutData.ChangedPropertyList.Num())
									{
										OutData.ChangedPropertyList.Insert(ChangedArrayPropertyInfo, NumChangedArrayProperties);
									}
								}
								else if (!ArrayProperty->Inner->Identical(ArrayPropertyValue, DefaultArrayPropertyValue, PPF_None))
								{
									// Emit the index of the individual array value that differs from the default value
									OutData.ChangedPropertyList.Add(ChangedArrayPropertyInfo);
								}
							}
							else
							{
								// Emit the "end" of differences with the default value (signals that remaining values should be copied in full)
								ChangedArrayPropertyInfo.PropertyName = NAME_None;
								OutData.ChangedPropertyList.Add(ChangedArrayPropertyInfo);

								// Don't need to record anything else.
								break;
							}
						}

						// Prepend the array property as changed only if the sizes differ and/or if we also wrote out any of the inner value as changed.
						if (ArrayValueHelper.Num() != DefaultArrayValueHelper.Num() || NumChangedProperties < OutData.ChangedPropertyList.Num())
						{
							OutData.ChangedPropertyList.Insert(ChangedPropertyInfo, NumChangedProperties);
						}
					}
					else if (!Property->Identical(PropertyValue, DefaultPropertyValue, PPF_None))
					{
						OutData.ChangedPropertyList.Add(ChangedPropertyInfo);
					}
				}
			}
		}
	}
};

void FBlueprintEditorUtils::BuildComponentInstancingData(UActorComponent* ComponentTemplate, FBlueprintCookedComponentInstancingData& OutData, bool bUseTemplateArchetype)
{
	if (ComponentTemplate)
	{
		UClass* ComponentTemplateClass = ComponentTemplate->GetClass();
		const UObject* ComponentDefaults = bUseTemplateArchetype ? ComponentTemplate->GetArchetype() : ComponentTemplateClass->GetDefaultObject(false);

		// Gather the set of properties that differ from the defaults.
		OutData.ChangedPropertyList.Empty();
		FComponentInstancingDataUtils::RecursivePropertyGather(ComponentTemplateClass, (uint8*)ComponentTemplate, (uint8*)ComponentDefaults, OutData);

		// Flag that cooked data has been built and is now considered to be valid.
		OutData.bIsValid = true;
	}
}

#undef LOCTEXT_NAMESPACE

