// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Engine/Blueprint.h"
#include "Widgets/SWidget.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_EditablePinBase.h"
#include "Editor/ClassViewer/Public/ClassViewerModule.h"
#include "EdGraphSchema_K2.h"

class AActor;
class ALevelScriptActor;
class FBlueprintEditor;
class FCompilerResultsLog;
class INameValidatorInterface;
class UActorComponent;
class UBlueprintGeneratedClass;
class UK2Node_Variable;
class ULevelScriptBlueprint;
class USCS_Node;
class UTimelineTemplate;
struct FBlueprintCookedComponentInstancingData;
struct FComponentKey;

/** 
  * Flags describing how to handle graph removal
  */
namespace EGraphRemoveFlags
{
	enum Type
	{
		/** No options */
		None = 0x00000000,

		/** If true recompile the blueprint after removing the graph, false if operations are being batched */
		Recompile = 0x00000001,

		/** If true mark the graph as transient, false otherwise */
		MarkTransient = 0x00000002,

		/** Helper enum for most callers */
		Default = Recompile | MarkTransient
	};
};

struct UNREALED_API FFunctionFromNodeHelper
{
	UFunction* const Function;
	const UK2Node* const Node;

	static UFunction* FunctionFromNode(const UK2Node* Node);

	FFunctionFromNodeHelper(const UObject* Obj);
};

class UNREALED_API FBasePinChangeHelper
{
public:
	static bool NodeIsNotTransient(const UK2Node* Node)
	{
		return (NULL != Node)
			&& !Node->HasAnyFlags(RF_Transient) 
			&& (NULL != Cast<UEdGraph>(Node->GetOuter()));
	}

	virtual ~FBasePinChangeHelper() { }

	virtual void EditCompositeTunnelNode(class UK2Node_Tunnel* TunnelNode) {}

	virtual void EditMacroInstance(class UK2Node_MacroInstance* MacroInstance, UBlueprint* Blueprint) {}

	virtual void EditCallSite(class UK2Node_CallFunction* CallSite, UBlueprint* Blueprint) {}

	virtual void EditDelegates(class UK2Node_BaseMCDelegate* CallSite, UBlueprint* Blueprint) {}

	virtual void EditCreateDelegates(class UK2Node_CreateDelegate* CallSite) {}

	void Broadcast(UBlueprint* InBlueprint, class UK2Node_EditablePinBase* InTargetNode, UEdGraph* Graph);
};

class UNREALED_API FParamsChangedHelper : public FBasePinChangeHelper
{
public:
	TSet<UBlueprint*> ModifiedBlueprints;
	TSet<UEdGraph*> ModifiedGraphs;

	virtual void EditCompositeTunnelNode(class UK2Node_Tunnel* TunnelNode) override;

	virtual void EditMacroInstance(class UK2Node_MacroInstance* MacroInstance, UBlueprint* Blueprint) override;

	virtual void EditCallSite(class UK2Node_CallFunction* CallSite, UBlueprint* Blueprint) override;

	virtual void EditDelegates(class UK2Node_BaseMCDelegate* CallSite, UBlueprint* Blueprint) override;

	virtual void EditCreateDelegates(class UK2Node_CreateDelegate* CallSite) override;

};

struct UNREALED_API FUCSComponentId
{
public:
	FUCSComponentId(const class UK2Node_AddComponent* UCSNode);
	FGuid GetAssociatedGuid() const { return GraphNodeGuid; }

private:
	FGuid GraphNodeGuid;
};

DECLARE_CYCLE_STAT_EXTERN(TEXT("Notify Blueprint Changed"), EKismetCompilerStats_NotifyBlueprintChanged, STATGROUP_KismetCompiler, );

struct UNREALED_API FCompilerRelevantNodeLink
{
	UK2Node* Node;
	UEdGraphPin* LinkedPin;

	FCompilerRelevantNodeLink(UK2Node* InNode, UEdGraphPin* InLinkedPin)
		: Node(InNode)
		, LinkedPin(InLinkedPin)
	{
	}
};

/** Array type for GetCompilerRelevantNodeLinks() */
typedef TArray<FCompilerRelevantNodeLink, TInlineAllocator<4> > FCompilerRelevantNodeLinkArray;

class UNREALED_API FBlueprintEditorUtils
{
public:

	/**
	 * Schedules and refreshes all nodes in the blueprint, making sure that nodes that affect function signatures get regenerated first
	 */
	static void RefreshAllNodes(UBlueprint* Blueprint);

	/**
	 * Reconstructs all nodes in the blueprint, node reconstruction order determined by FCompareNodePriority.
	 */
	static void ReconstructAllNodes(UBlueprint* Blueprint);

	/**
	 * Optimized refresh of nodes that depend on external blueprints.  Refreshes the nodes, but does not recompile the skeleton class
	 */
	static void RefreshExternalBlueprintDependencyNodes(UBlueprint* Blueprint, UStruct* RefreshOnlyChild = NULL);

	/**
	 * Refresh the nodes of an individual graph.
	 * 
	 * @param	Graph	The graph to refresh.
	 */
	static void RefreshGraphNodes(const UEdGraph* Graph);

	/**
	 * Replaces any deprecated nodes with new ones
	 */
	static void ReplaceDeprecatedNodes(UBlueprint* Blueprint);

	/**
	 * Preloads the object and all the members it owns (nodes, pins, etc)
	 */
	static void PreloadMembers(UObject* InObject);

	/**
	 * Preloads the construction script, and all templates therein
	 */
	static void PreloadConstructionScript(UBlueprint* Blueprint);

	/** 
	 * Helper function to patch the new CDO into the linker where the old one existed 
	 */
	static void PatchNewCDOIntoLinker(UObject* CDO, FLinkerLoad* Linker, int32 ExportIndex, TArray<UObject*>& ObjLoaded);

	/** 
	 * Procedure used to remove old function implementations and child properties from data only blueprints.
	 */
	static void RemoveStaleFunctions(UBlueprintGeneratedClass* Class, UBlueprint* Blueprint);

	/**
	 *  Synchronizes Blueprint's GeneratedClass's properties with the NewVariable declarations in the blueprint
	 */
	static void RefreshVariables(UBlueprint* Blueprint);

	/** Helper function to punch through and honor UAnimGraphNode_Base::PreloadRequiredAssets, which formerly relied on loading assets during compile */
	static void PreloadBlueprintSpecificData(UBlueprint* Blueprint);

	/**
	 * Regenerates the class at class load time, and refreshes the blueprint
	 */
	static UClass* RegenerateBlueprintClass(UBlueprint* Blueprint, UClass* ClassToRegenerate, UObject* PreviousCDO, TArray<UObject*>& ObjLoaded);
	
	/**
	 * Links external dependencies
	 */
	static void LinkExternalDependencies(UBlueprint* Blueprint);

	/**
	 * Replace subobjects of CDO in linker
	 */
	static void PatchCDOSubobjectsIntoExport(UObject* PreviousCDO, UObject* NewCDO);

	/** Recreates class meta data */
	static void RecreateClassMetaData(UBlueprint* Blueprint, UClass* Class, bool bRemoveExistingMetaData);

	/**
	 * Copies the default properties of all parent blueprint classes in the chain to the specified blueprint's skeleton CDO
	 */
	static void PropagateParentBlueprintDefaults(UClass* ClassToPropagate);

	/** Called on a Blueprint after it has been duplicated */
	static void PostDuplicateBlueprint(UBlueprint* Blueprint, bool bDuplicateForPIE);

	/** Consigns the blueprint's generated classes to oblivion */
	static void RemoveGeneratedClasses(UBlueprint* Blueprint);

	/**
	 * Helper function to get the blueprint that ultimately owns a node.
	 *
	 * @param	InNode	Node to find the blueprint for.
	 * @return	The corresponding blueprint or NULL.
	 */
	static UBlueprint* FindBlueprintForNode(const UEdGraphNode* Node);

	/**
	 * Helper function to get the blueprint that ultimately owns a node.  Cannot fail.
	 *
	 * @param	InNode	Node to find the blueprint for.
	 * @return	The corresponding blueprint or NULL.
	 */
	static UBlueprint* FindBlueprintForNodeChecked(const UEdGraphNode* Node);

	/**
	 * Helper function to get the blueprint that ultimately owns a graph.
	 *
	 * @param	InGraph	Graph to find the blueprint for.
	 * @return	The corresponding blueprint or NULL.
	 */
	static UBlueprint* FindBlueprintForGraph(const UEdGraph* Graph);

	/**
	 * Helper function to get the blueprint that ultimately owns a graph.  Cannot fail.
	 *
	 * @param	InGraph	Graph to find the blueprint for.
	 * @return	The corresponding blueprint or NULL.
	 */
	static UBlueprint* FindBlueprintForGraphChecked(const UEdGraph* Graph);

	/** Helper function to get the SkeletonClass, returns nullptr for UClasses that are not generated by a UBlueprint */
	static UClass* GetSkeletonClass(UClass* FromClass);

	/** Helper function to get the most up to date class , returns FromClass for native types, SkeletonClass for UBlueprint generated classes */
	static UClass* GetMostUpToDateClass(UClass* FromClass);
	static const UClass* GetMostUpToDateClass(const UClass* FromClass);
	
	/** Looks at the most up to data class and returns whether the given property exists in it as well */
	static bool PropertyStillExists(UProperty* Property);

	/** Returns the skeleton version of the property, skeleton classes are often more up to date than the authoritative GeneratedClass */
	static UProperty* GetMostUpToDateProperty(UProperty* Property);
	static const UProperty* GetMostUpToDateProperty(const UProperty* Property);

	static UFunction* GetMostUpToDateFunction(UFunction* Function);
	static const UFunction* GetMostUpToDateFunction(const UFunction* Function);

	/**
	 * Updates sources of delegates.
	 */
	static void UpdateDelegatesInBlueprint(UBlueprint* Blueprint);

	/**
	 * Whether or not the blueprint should regenerate its class on load or not.  This prevents macros and other BP types not marked for reconstruction from being recompiled all the time
	 */
	static bool ShouldRegenerateBlueprint(UBlueprint* Blueprint);

	/** Returns true if compilation for the given blueprint has been disabled */
	static bool IsCompileOnLoadDisabled(UBlueprint* Blueprint);

	/**
	 * Blueprint has structurally changed (added/removed functions, graphs, etc...). Performs the following actions:
	 *  - Recompiles the skeleton class.
	 *  - Notifies any observers.
	 *  - Marks the package as dirty.
	 */
	static void MarkBlueprintAsStructurallyModified(UBlueprint* Blueprint);

	/**
	 * Blueprint has changed in some manner that invalidates the compiled data (link made/broken, default value changed, etc...)
	 *  - Marks the blueprint as status unknown
	 *  - Marks the package as dirty
	 *
	 * @param	Blueprint				The Blueprint to mark as modified
	 * @param	PropertyChangedEvent	Used when marking the blueprint as modified due to a changed property (optional)
	 */
	static void MarkBlueprintAsModified(UBlueprint* Blueprint, FPropertyChangedEvent PropertyChangedEvent = FPropertyChangedEvent(nullptr));

	/** See whether or not the specified graph name / entry point name is unique */
	static bool IsGraphNameUnique(UBlueprint* Blueprint, const FName& InName);

	/**
	 * Creates a new empty graph.
	 *
	 * @param	ParentScope		The outer of the new graph (typically a blueprint).
	 * @param	GraphName		Name of the graph to add.
	 * @param	SchemaClass		Schema to use for the new graph.
	 *
	 * @return	null if it fails, else.
	 */
	static class UEdGraph* CreateNewGraph(UObject* ParentScope, const FName& GraphName, TSubclassOf<class UEdGraph> GraphClass, TSubclassOf<class UEdGraphSchema> SchemaClass);

	/**
	 * Creates a function graph, but does not add it to the blueprint.  If bIsUserCreated is true, the entry/exit nodes will be editable. 
	 * SignatureFromObject is used to find signature for entry/exit nodes if using an existing signature.
	 * The template argument SignatureType should be UClass or UFunction.
	 */
	template <typename SignatureType>
	static void CreateFunctionGraph(UBlueprint* Blueprint, class UEdGraph* Graph, bool bIsUserCreated, SignatureType* SignatureFromObject)
	{
		// Give the schema a chance to fill out any required nodes (like the entry node or results node)
		const UEdGraphSchema* Schema = Graph->GetSchema();
		const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(Graph->GetSchema());

		Schema->CreateDefaultNodesForGraph(*Graph);

		if ( K2Schema != NULL )
		{
			K2Schema->CreateFunctionGraphTerminators(*Graph, SignatureFromObject);

			if ( bIsUserCreated )
			{
				// We need to flag the entry node to make sure that the compiled function is callable from Kismet2
				int32 ExtraFunctionFlags = ( FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_Public );
				if ( BPTYPE_FunctionLibrary == Blueprint->BlueprintType )
				{
					ExtraFunctionFlags |= FUNC_Static;
				}
				// We need to mark the function entry as editable so that we can
				// set metadata on it if it is a blutility:
				K2Schema->MarkFunctionEntryAsEditable(Graph, true);
				if( IsBlutility( Blueprint ))
				{
					if( FKismetUserDeclaredFunctionMetadata* MetaData = GetGraphFunctionMetaData( Graph ))
					{
						MetaData->bCallInEditor = true;
					}
				}
				K2Schema->AddExtraFunctionFlags(Graph, ExtraFunctionFlags);
			}
		}
	}

	/** 
	 * Adds a function graph to this blueprint.  If bIsUserCreated is true, the entry/exit nodes will be editable. 
	 * SignatureFromObject is used to find signature for entry/exit nodes if using an existing signature.
	 * The template argument SignatureType should be UClass or UFunction.
	 */
	template <typename SignatureType>
	static void AddFunctionGraph(UBlueprint* Blueprint, class UEdGraph* Graph, bool bIsUserCreated, SignatureType* SignatureFromObject)
	{
		CreateFunctionGraph(Blueprint, Graph, bIsUserCreated, SignatureFromObject);

		Blueprint->FunctionGraphs.Add(Graph);

		// Potentially adjust variable names for any child blueprints
		ValidateBlueprintChildVariables(Blueprint, Graph->GetFName());

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	/** Adds a macro graph to this blueprint.  If bIsUserCreated is true, the entry/exit nodes will be editable. SignatureFromClass is used to find signature for entry/exit nodes if using an existing signature. */
	static void AddMacroGraph(UBlueprint* Blueprint, class UEdGraph* Graph,  bool bIsUserCreated, UClass* SignatureFromClass);

	/** Adds an interface graph to this blueprint */
	static void AddInterfaceGraph(UBlueprint* Blueprint, class UEdGraph* Graph, UClass* InterfaceClass);

	/** Adds an ubergraph page to this blueprint */
	static void AddUbergraphPage(UBlueprint* Blueprint, class UEdGraph* Graph);

	/** Adds a domain-specific graph to this blueprint */
	static void AddDomainSpecificGraph(UBlueprint* Blueprint, class UEdGraph* Graph);

	/**
	 * Remove the supplied set of graphs from the Blueprint.
	 *
	 * @param	GraphsToRemove	The graphs to remove.
	 */
	static void RemoveGraphs( UBlueprint* Blueprint, const TArray<class UEdGraph*>& GraphsToRemove );

	/**
	 * Removes the supplied graph from the Blueprint.
	 *
	 * @param Blueprint			The blueprint containing the graph
	 * @param GraphToRemove		The graph to remove.
	 * @param Flags				Options to control the removal process
	 */
	static void RemoveGraph( UBlueprint* Blueprint, class UEdGraph* GraphToRemove, EGraphRemoveFlags::Type Flags = EGraphRemoveFlags::Default );

	/**
	 * Tries to rename the supplied graph.
	 * Cleans up function entry node if one exists and marks objects for modification
	 *
	 * @param Graph				The graph to rename.
	 * @param NewName			The new name for the graph
	 */
	static void RenameGraph(class UEdGraph* Graph, const FString& NewName );

	/**
	 * Renames the graph of the supplied node with a valid name based off of the suggestion.
	 *
	 * @param GraphNode			The node of the graph to rename.
	 * @param DesiredName		The initial form of the name to try
	 */
	static void RenameGraphWithSuggestion(class UEdGraph* Graph, TSharedPtr<class INameValidatorInterface> NameValidator, const FString& DesiredName );

	/**
	 * Removes the supplied node from the Blueprint.
	 *
	 * @param Node				The node to remove.
	 * @param bDontRecompile	If true, the blueprint will not be marked as modified, and will not be recompiled.  Useful for if you are removing several node at once, and don't want to recompile each time
	 */
	static void RemoveNode (UBlueprint* Blueprint, UEdGraphNode* Node, bool bDontRecompile=false);

	/**
	 * Returns the graph's top level graph (climbing up the hierarchy until there are no more graphs)
	 *
	 * @param InGraph		The graph to find the parent of
	 *
	 * @return				The top level graph
	 */
	static UEdGraph* GetTopLevelGraph(const UEdGraph* InGraph);

	/** Determines if the graph is ReadOnly, this differs from editable in that it is never expected to be edited and is in a read-only state */
	static bool IsGraphReadOnly(UEdGraph* InGraph);

	/** Look to see if an event already exists to override a particular function */
	static class UK2Node_Event* FindOverrideForFunction(const UBlueprint* Blueprint, const UClass* SignatureClass, FName SignatureName);

	/** Find the Custom Event if it already exists in the Blueprint */
	static class UK2Node_Event* FindCustomEventNode(const UBlueprint* Blueprint, FName const CustomName);


	/** Returns all nodes in all graphs of the specified class */
	template< class T > 
	static inline void GetAllNodesOfClass( const UBlueprint* Blueprint, TArray<T*>& OutNodes )
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for(int32 i=0; i<AllGraphs.Num(); i++)
		{
			check(AllGraphs[i] != NULL);
			TArray<T*> GraphNodes;
			AllGraphs[i]->GetNodesOfClass<T>(GraphNodes);
			OutNodes.Append(GraphNodes);
		}
	}

	/** Returns all nodes in all graphs of at least the minimum node type */
	template< class MinNodeType, class ArrayClassType>
	static inline void GetAllNodesOfClassEx(const UBlueprint* Blueprint, TArray<ArrayClassType*>& OutNodes)
	{
		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);
		for(UEdGraph* Graph : AllGraphs)
		{
			check(Graph != nullptr);
			Graph->GetNodesOfClassEx<MinNodeType, ArrayClassType>(OutNodes);
		}
	}

	/**
	 * Searches all nodes in a Blueprint and checks for a matching Guid
	 *
	 * @param InBlueprint			The Blueprint to search
	 * @param InNodeGuid			The Guid to check Blueprints against
	 *
	 * @return						Returns a Node with a matching Guid
	 */
	static UEdGraphNode* GetNodeByGUID(const UBlueprint* InBlueprint, const FGuid& InNodeGuid)
	{
		TArray<UEdGraphNode*> GraphNodes;
		GetAllNodesOfClass(InBlueprint, GraphNodes);

		for(UEdGraphNode* GraphNode : GraphNodes)
		{
			if(GraphNode->NodeGuid == InNodeGuid)
			{
				return GraphNode;
			}
		}
		return nullptr;
	}

	/** Gather all bps that Blueprint depends on */
	static void GatherDependencies(const UBlueprint* Blueprint, TSet<TWeakObjectPtr<UBlueprint>>& OutDependencies, TSet<TWeakObjectPtr<UStruct>>& OutUDSDependencies);

	/** Returns a list of loaded Blueprints that are dependent on the given Blueprint. */
	static void GetDependentBlueprints(UBlueprint* Blueprint, TArray<UBlueprint*>& DependentBlueprints, bool bRemoveSelf = true);

	/** Ensures, that CachedDependencies in BP are up to date */
	static void EnsureCachedDependenciesUpToDate(UBlueprint* Blueprint);

	/** returns if a graph is an intermediate build product */
	static bool IsGraphIntermediate(const UEdGraph* Graph);

	/** @return true if the blueprint does not contain any special logic or variables or other elements that require a full compile. */
	static bool IsDataOnlyBlueprint(const UBlueprint* Blueprint);

	/** Returns whether or not the blueprint is const during execution */
	static bool IsBlueprintConst(const UBlueprint* Blueprint);

	/** Returns whether or not the blueprint is a blutility */
	static bool IsBlutility(const UBlueprint* Blueprint);

	/**
	 * Whether or not this is an actor-based blueprint, and supports features like the uber-graph, components, etc
	 *
	 * @return	Whether or not this is an actor based blueprint
	 */
	static bool IsActorBased(const UBlueprint* Blueprint);

	/**
	 * Whether or not this blueprint is an interface, used only for defining functions to implement
	 *
	 * @return	Whether or not this is an interface blueprint
	 */
	static bool IsInterfaceBlueprint(const UBlueprint* Blueprint);

	/**
	 * Whether or not this blueprint is an interface, used only for defining functions to implement
	 *
	 * @return	Whether or not this is a level script blueprint
	 */
	static bool IsLevelScriptBlueprint(const UBlueprint* Blueprint);

	/**
	 * Whether or not this class represents a class generated by an anonymous actor class stored in a level 
	 *
	 * @return	Whether or not this is an anonymous blueprint
	 */
	static bool IsAnonymousBlueprintClass(const UClass* Class);

	/**
	 * Checks for events in the argument class
	 * @param Class	The class to check for events.
	 */
	static bool CanClassGenerateEvents(const UClass* Class);

	/**
	 * If a blueprint is directly tied to a level (level script and anonymous blueprints), this will return a pointer to that level
	 *
	 * @return	The level, if any, tied to this blueprint
	 */
	static class ULevel* GetLevelFromBlueprint(const UBlueprint* Blueprint);

	/** Do we support construction scripts */
	static bool SupportsConstructionScript(const UBlueprint* Blueprint);

	/** Returns the user construction script, if any */
	static UEdGraph* FindUserConstructionScript(const UBlueprint* Blueprint);

	/** Returns the event graph, if any */
	static UEdGraph* FindEventGraph(const UBlueprint* Blueprint);

	/** Checks if given graph is an event graph */
	static bool IsEventGraph(const UEdGraph* InGraph);

	/** Checks if given node is a tunnel instance node */
	static bool IsTunnelInstanceNode(const UEdGraphNode* InGraphNode);

	/** See if a class is the one generated by this blueprint */
	static bool DoesBlueprintDeriveFrom(const UBlueprint* Blueprint, UClass* TestClass);

	/** See if a field (property, function etc) is part of the blueprint chain, or  */
	static bool DoesBlueprintContainField(const UBlueprint* Blueprint, UField* TestField);

	/** Returns whether or not the blueprint supports overriding functions */
	static bool DoesSupportOverridingFunctions(const UBlueprint* Blueprint);

	/** Returns whether or not the blueprint supports timelines */
	static bool DoesSupportTimelines(const UBlueprint* Blueprint);

	/** Returns whether or not the blueprint supports event graphs*/
	static bool DoesSupportEventGraphs(const UBlueprint* Blueprint);

	/** Returns whether or not the blueprint supports implementing interfaces */
	static bool DoesSupportImplementingInterfaces(const UBlueprint* Blueprint);

	/** Returns whether or not the blueprint supports components */
	static bool DoesSupportComponents(UBlueprint const* Blueprint);

	/** Returns whether or not the blueprint supports default values (IE has a CDO) */
	static bool DoesSupportDefaults(UBlueprint const* Blueprint);

	/** Returns whether or not the blueprint graph supports local variables */
	static bool DoesSupportLocalVariables(UEdGraph const* InGraph);

	// Returns a descriptive name of the type of blueprint passed in
	static FString GetBlueprintTypeDescription(const UBlueprint* Blueprint);

	/** Constructs a class picker widget for reparenting the specified blueprint(s) */
	static TSharedRef<SWidget> ConstructBlueprintParentClassPicker( const TArray< UBlueprint* >& Blueprints, const FOnClassPicked& OnPicked);

	/** Try to open reparent menu for specified blueprint */
	static void OpenReparentBlueprintMenu( UBlueprint* Blueprint, const TSharedRef<SWidget>& ParentContent, const FOnClassPicked& OnPicked);
	static void OpenReparentBlueprintMenu( const TArray< UBlueprint* >& Blueprints, const TSharedRef<SWidget>& ParentContent, const FOnClassPicked& OnPicked);

	/** Constructs a class picker widget for adding interfaces for the specified blueprint(s) */
	static TSharedRef<SWidget> ConstructBlueprintInterfaceClassPicker( const TArray< UBlueprint* >& Blueprints, const FOnClassPicked& OnPicked);

	/** return find first native class in the hierarchy */
	static UClass* FindFirstNativeClass(UClass* Class);

	/**
	 * Gets the names of all graphs in the Blueprint
	 *
	 * @param [in,out]	GraphNames	The graph names will be appended to this array.
	 */
	static void GetAllGraphNames(const UBlueprint* Blueprint, TSet<FName>& GraphNames);

	/**
	 * Gets the compiler-relevant (i.e. non-ignorable) node links from the given pin.
	 *
	 * @param			FromPin			The pin to start searching from.
	 * @param			OutNodeLinks	Will contain the given pin + owning node if compiler-relevant, or all nodes linked to the owning node at the matching "pass-through" pin that are compiler-relevant. Empty if no compiler-relevant node links can be found from the given pin.
	 */
	static void GetCompilerRelevantNodeLinks(UEdGraphPin* FromPin, FCompilerRelevantNodeLinkArray& OutNodeLinks);

	/**
	 * Finds the first compiler-relevant (i.e. non-ignorable) node from the given pin.
	 *
	 * @param			FromPin			The pin to start searching from.
	 *
	 * @return			The given pin's owning node if compiler-relevant, or the first node linked to the owning node at the matching "pass-through" pin that is compiler-relevant. May be NULL if no compiler-relevant nodes can be found from the given pin.
	 */
	static UK2Node* FindFirstCompilerRelevantNode(UEdGraphPin* FromPin);

	/**
	 * Finds the first compiler-relevant (i.e. non-ignorable) node from the given pin and returns the owned pin that's linked.
	 *
	 * @param			FromPin			The pin to start searching from.
	 *
	 * @return			The given pin if its owning node is compiler-relevant, or the first pin linked to the owning node at the matching "pass-through" pin that is owned by a compiler-relevant node. May be NULL if no compiler-relevant nodes can be found from the given pin.
	 */
	static UEdGraphPin* FindFirstCompilerRelevantLinkedPin(UEdGraphPin* FromPin);

	//////////////////////////////////////////////////////////////////////////
	// Functions

	/**
	 * Gets a list of function names currently in use in the blueprint, based on the skeleton class
	 *
	 * @param			Blueprint		The blueprint to check
	 * @param [in,out]	FunctionNames	List of function names currently in use
	 */
	static void GetFunctionNameList(const UBlueprint* Blueprint, TSet<FName>& FunctionNames);

	/**
	 * Gets a list of delegates names in the blueprint, based on the skeleton class
	 *
	 * @param			Blueprint		The blueprint to check
	 * @param [in,out]	DelegatesNames	List of function names currently in use
	 */
	static void GetDelegateNameList(const UBlueprint* Blueprint, TSet<FName>& DelegatesNames);

	/** 
	 * Get a graph for delegate signature with given name, from given blueprint.
	 * 
	 * @param			Blueprint		Blueprint owning the delegate signature graph
	 * @param			DelegateName	Name of delegate.
	 * @return			Graph of delegate-signature function.
	 */
	static UEdGraph* GetDelegateSignatureGraphByName(UBlueprint* Blueprint, FName DelegateName);

	/** Checks if given graph contains a delegate signature */
	static bool IsDelegateSignatureGraph(const UEdGraph* Graph);

	/** Checks if given graph is owned by a Math Expression node */
	static bool IsMathExpressionGraph(const UEdGraph* InGraph);

	/**
	 * Gets a list of pins that should hidden for a given function in a given graph
	 *
	 * @param			Graph			The graph that you're looking to call the function from (some functions hide different pins depending on the graph they're in)
	 * @param			Function		The function to consider
	 * @param [out]		HiddenPins		Set of pins that should be hidden
	 * @param [out]		OutInternalPins	Subset of hidden pins that are marked for internal use only rather than marked as hidden (optional)
	 */
	static void GetHiddenPinsForFunction(UEdGraph const* Graph, UFunction const* Function, TSet<FString>& HiddenPins, TSet<FString>* OutInternalPins = nullptr);

	/** Makes sure that calls to parent functions are valid, and removes them if not */
	static void ConformCallsToParentFunctions(UBlueprint* Blueprint);

	//////////////////////////////////////////////////////////////////////////
	// Events

	/** Makes sure that all events we handle exist, and replace with custom events if not */
	static void ConformImplementedEvents(UBlueprint* Blueprint);

	//////////////////////////////////////////////////////////////////////////
	// Variables

	/**
	 * Checks if pin type stores proper type for a variable or parameter. Especially if the UDStruct is valid.
	 *
	 * @param		Type	Checked pin type.
	 *
	 * @return				if type is valid
	 */
	static bool IsPinTypeValid(const FEdGraphPinType& Type);

	/**
	 * Gets the visible class variable list.  This includes both variables introduced here and in all superclasses.
	 *
	 * @param [in,out]	VisibleVariables	The visible variables will be appended to this array.
	 */
	static void GetClassVariableList(const UBlueprint* Blueprint, TSet<FName>& VisibleVariables, bool bIncludePrivateVars=false);

	/**
	 * Gets variables of specified type
	 *
	 * @param 			FEdGraphPinType	 			Type of variables to look for
	 * @param [in,out]	VisibleVariables			The visible variables will be appended to this array.
	 */
	static void GetNewVariablesOfType( const UBlueprint* Blueprint, const FEdGraphPinType& Type, TArray<FName>& OutVars);

	/**
	 * Gets local variables of specified type
	 *
	 * @param 			FEdGraphPinType	 			Type of variables to look for
	 * @param [in,out]	VisibleVariables			The visible variables will be appended to this array.
	 */
	static void GetLocalVariablesOfType( const UEdGraph* Graph, const FEdGraphPinType& Type, TArray<FName>& OutVars);

	/**
	 * Adds a member variable to the blueprint.  It cannot mask a variable in any superclass.
	 *
	 * @param	NewVarName	Name of the new variable.
	 * @param	NewVarType	Type of the new variable.
	 * @param	DefaultValue	Default value stored as string
	 *
	 * @return	true if it succeeds, false if it fails.
	 */
	static bool AddMemberVariable(UBlueprint* Blueprint, const FName& NewVarName, const FEdGraphPinType& NewVarType, const FString& DefaultValue = FString());

	/**
	 * Duplicates a variable given it's name and Blueprint
	 *
	 * @param InBlueprint					The Blueprint the variable can be found in
	 * @paramInScope						Local variable's scope
	 * @param InVariableToDuplicate			Variable name to be found and duplicated
	 *
	 * @return								Returns the name of the new variable or NAME_None is failed to duplicate
	 */
	static FName DuplicateVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName& InVariableToDuplicate);

	/**
	 * Internal function that deep copies a variable description
	 *
	 * @param InBlueprint					The blueprint to ensure a uniquely named variable in
	 * @param InVariableToDuplicate			Variable description to duplicate
	 *
	 * @return								Returns a copy of the passed in variable description.
	 */
	static FBPVariableDescription DuplicateVariableDescription(UBlueprint* InBlueprint, FBPVariableDescription& InVariableDescription);

	/**
	 * Removes a member variable if it was declared in this blueprint and not in a base class.
	 *
	 * @param	VarName	Name of the variable to be removed.
	 */
	static void RemoveMemberVariable(UBlueprint* Blueprint, const FName VarName);
	
	/**
	 * Removes member variables if they were declared in this blueprint and not in a base class.
	 *
	 * @param	VarNames	Names of the variable to be removed.
	 */
	static void BulkRemoveMemberVariables(UBlueprint* Blueprint, const TArray<FName>& VarNames);

	/**
	 * Finds a member variable Guid using the variable's name
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InVariableGuid	Local variable's name to search for
	 * @return					The Guid associated with the local variable
	 */
	static FGuid FindMemberVariableGuidByName(UBlueprint* InBlueprint, const FName InVariableName);

	/**
	 * Finds a member variable name using the variable's Guid
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InVariableGuid	Guid to identify the local variable with
	 * @return					Local variable's name
	 */
	static FName FindMemberVariableNameByGuid(UBlueprint* InBlueprint, const FGuid& InVariableGuid);

	/**
	 * Removes the variable nodes associated with the specified var name
	 *
	 * @param	Blueprint			The blueprint you want variable nodes removed from.
	 * @param	VarName				Name of the variable to be removed.
	 * @param	bForSelfOnly		True if you only want to delete variable nodes that represent ones owned by this blueprint,
	 * @param	LocalGraphScope		Local scope graph of variables
	 *								false if you just want everything with the specified name removed (variables from other classes too).
	 */
	static void RemoveVariableNodes(UBlueprint* Blueprint, const FName VarName, bool const bForSelfOnly = true, UEdGraph* LocalGraphScope = nullptr);

	/**Rename a member variable*/
	static void RenameMemberVariable(UBlueprint* Blueprint, const FName OldName, const FName NewName);

	/** Rename a member variable created by a SCS entry */
	static void RenameComponentMemberVariable(UBlueprint* Blueprint, USCS_Node* Node, const FName NewName);
	
	/** Changes the type of a member variable */
	static void ChangeMemberVariableType(UBlueprint* Blueprint, const FName VariableName, const FEdGraphPinType& NewPinType);

	/**
	 * Finds the scope's associated graph for local variables (or any passed UFunction)
	 *
	 * @param	InBlueprint			The Blueprint the local variable can be found in
	 * @param	InScope				Local variable's scope
	 */
	static UEdGraph* FindScopeGraph(const UBlueprint* InBlueprint, const UStruct* InScope);

	/**
	 * Adds a local variable to the function graph.  It cannot mask a member variable or a variable in any superclass.
	 *
	 * @param	NewVarName	Name of the new variable.
	 * @param	NewVarType	Type of the new variable.
	 * @param	DefaultValue	Default value stored as string
	 *
	 * @return	true if it succeeds, false if it fails.
	 */
	static bool AddLocalVariable(UBlueprint* Blueprint, UEdGraph* InTargetGraph, const FName InNewVarName, const FEdGraphPinType& InNewVarType, const FString& DefaultValue = FString());

	/**
	 * Removes a member variable if it was declared in this blueprint and not in a base class.
	 *
	 * @param	InBlueprint			The Blueprint the local variable can be found in
	 * @param	InScope				Local variable's scope
	 * @param	InVarName			Name of the variable to be removed.
	 */
	static void RemoveLocalVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVarName);

	/**
	 * Returns a local variable with the function entry it was found in
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InVariableName	Name of the variable to search for
	 * @return					The local variable description
	 */
	static FBPVariableDescription* FindLocalVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName);

	/**
	 * Returns a local variable
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InScopeGraph		Local variable's graph
	 * @param InVariableName	Name of the variable to search for
	 * @param OutFunctionEntry	Optional output parameter. If not null, the found function entry is returned.
	 * @return					The local variable description
	 */
	static FBPVariableDescription* FindLocalVariable(const UBlueprint* InBlueprint, const UEdGraph* InScopeGraph, const FName InVariableName, class UK2Node_FunctionEntry** OutFunctionEntry = NULL);

	/**
	 * Returns a local variable
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InScope			Local variable's scope
	 * @param InVariableName	Name of the variable to search for
	 * @param OutFunctionEntry	Optional output parameter. If not null, the found function entry is returned.
	 * @return					The local variable description
	 */
	static FBPVariableDescription* FindLocalVariable(const UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName, class UK2Node_FunctionEntry** OutFunctionEntry = NULL);

	/**
	 * Finds a local variable name using the variable's Guid
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InVariableGuid	Guid to identify the local variable with
	 * @return					Local variable's name
	 */
	static FName FindLocalVariableNameByGuid(UBlueprint* InBlueprint, const FGuid& InVariableGuid);

	/**
	 * Finds a local variable Guid using the variable's name
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InScope			Local variable's scope
	 * @param InVariableGuid	Local variable's name to search for
	 * @return					The Guid associated with the local variable
	 */
	static FGuid FindLocalVariableGuidByName(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName);

	/**
	 * Finds a local variable Guid using the variable's name
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InScopeGraph		Local variable's graph
	 * @param InVariableGuid	Local variable's name to search for
	 * @return					The Guid associated with the local variable
	 */
	static FGuid FindLocalVariableGuidByName(UBlueprint* InBlueprint, const UEdGraph* InScopeGraph, const FName InVariableName);

	/**
	 * Rename a local variable
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InScope			Local variable's scope
	 * @param InOldName			The name of the local variable to change
	 * @param InNewName			The new name of the local variable
	 */
	static void RenameLocalVariable(UBlueprint* InBlueprint, const UStruct* InScope, const FName InOldName, const FName InNewName);

	/**
	 * Changes the type of a local variable
	 *
	 * @param InBlueprint		Blueprint to search for the local variable
	 * @param InScope			Local variable's scope
	 * @param InVariableName	Name of the local variable to change the type of
	 * @param InNewPinType		The pin type to change the local variable type to
	 */
	static void ChangeLocalVariableType(UBlueprint* InBlueprint, const UStruct* InScope, const FName InVariableName, const FEdGraphPinType& InNewPinType);

	/** Replaces all variable references in the specified blueprint */
	static void ReplaceVariableReferences(UBlueprint* Blueprint, const FName OldName, const FName NewName);

	/** Replaces all variable references in the specified blueprint */
	static void ReplaceVariableReferences(UBlueprint* Blueprint, const UProperty* OldVariable, const UProperty* NewVariable);

	/** Check blueprint variable metadata keys/values for validity and make adjustments if needed */
	static void FixupVariableDescription(UBlueprint* Blueprint, FBPVariableDescription& VarDesc);

	/** Validate child blueprint component member variables, member variables, and timelines, and function graphs against the given variable name */
	static void ValidateBlueprintChildVariables(UBlueprint* InBlueprint, const FName InVariableName);

	/** Rename a Timeline. If bRenameNodes is true, will also rename any timeline nodes associated with this timeline */
	static bool RenameTimeline (UBlueprint* Blueprint, const FName OldVarName, const FName NewVarName);

	/**
	 * Sets the Blueprint edit-only flag on the variable with the specified name
	 *
	 * @param	VarName				Name of the var to set the flag on
	 * @param	bNewBlueprintOnly	The new value to set the bitflag to
	 */
	static void SetBlueprintOnlyEditableFlag(UBlueprint* Blueprint, const FName& VarName, const bool bNewBlueprintOnly);

	/**
	 * Sets the Blueprint read-only flag on the variable with the specified name
	 *
	 * @param	VarName				Name of the var to set the flag on
	 * @param	bVariableReadOnly	The new value to set the bitflag to
	 */
	static void SetBlueprintPropertyReadOnlyFlag(UBlueprint* Blueprint, const FName& VarName, const bool bVariableReadOnly);

	/**
	 * Sets the Interp flag on the variable with the specified name to make available to matinee
	 *
	 * @param	VarName				Name of the var to set the flag on
	 * @param	bInterp	true to make variable available to Matinee, false otherwise
	 */
	static void SetInterpFlag(UBlueprint* Blueprint, const FName& VarName, const bool bInterp);

	/**
	 * Sets the Transient flag on the variable with the specified name
	 *
	 * @param	InVarName				Name of the var to set the flag on
	 * @param	bInIsTransient			The new value to set the bitflag to
	 */
	static void SetVariableTransientFlag(UBlueprint* InBlueprint, const FName& InVarName, const bool bInIsTransient);

	/**
	 * Sets the Save Game flag on the variable with the specified name
	 *
	 * @param	InVarName				Name of the var to set the flag on
	 * @param	bInIsSaveGame			The new value to set the bitflag to
	 */
	static void SetVariableSaveGameFlag(UBlueprint* InBlueprint, const FName& InVarName, const bool bInIsSaveGame);

	/**
	 * Sets the Advanced Display flag on the variable with the specified name
	 *
	 * @param	InVarName				Name of the var to set the flag on
	 * @param	bInIsAdvancedDisplay	The new value to set the bitflag to
	 */
	static void SetVariableAdvancedDisplayFlag(UBlueprint* InBlueprint, const FName& InVarName, const bool bInIsAdvancedDisplay);

	/** Sets a metadata key/value on the specified variable
	 *
	 * @param Blueprint				The Blueprint to find the variable in
	 * @param VarName				Name of the variable
	 * @param InLocalVarScope		Local variable's scope, if looking to modify a local variable
	 * @param MetaDataKey			Key name for the metadata to change
	 * @param MetaDataValue			Value to change the metadata to
	 */
	static void SetBlueprintVariableMetaData(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FName& MetaDataKey, const FString& MetaDataValue);

	/** Get a metadata key/value on the specified variable, or timeline if it exists, returning false if it does not exist
	 *
	 * @param Blueprint				The Blueprint to find the variable in
	 * @param VarName				Name of the variable
	 * @param InLocalVarScope		Local variable's scope, if looking to modify a local variable
	 * @param MetaDataKey			Key name for the metadata to change
	 * @param OutMetaDataValue		Value of the metadata
	 * @return						TRUE if finding the metadata was successful
	 */
	static bool GetBlueprintVariableMetaData(const UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FName& MetaDataKey, FString& OutMetaDataValue);

	/** Clear metadata key on specified variable, or timeline
	 * @param Blueprint				The Blueprint to find the variable in
	 * @param VarName				Name of the variable
	 * @param InLocalVarScope		Local variable's scope, if looking to modify a local variable
	 * @param MetaDataKey			Key name for the metadata to change
	 */
	static void RemoveBlueprintVariableMetaData(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FName& MetaDataKey);

	/**
	 * Sets the custom category on the variable with the specified name.
	 * @note: Will not change the category for variables defined via native classes.
	 *
	 * @param	VarName				Name of the variable
	 * @param	InLocalVarScope		Local variable's scope, if looking to modify a local variable
	 * @param	VarCategory			The new value of the custom category for the variable
	 * @param	bDontRecompile		If true, the blueprint will not be marked as modified, and will not be recompiled.  
	 */
	static void SetBlueprintVariableCategory(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope, const FText& NewCategory, bool bDontRecompile=false);


	/**
	 * Sets the custom category on the function or macro
	 * @note: Will not change the category for functions defined via native classes.
	 *
	 * @param	Graph				Graph associated with the function or macro
	 * @param	NewCategory			The new value of the custom category for the function
	 * @param	bDontRecompile		If true, the blueprint will not be marked as modified, and will not be recompiled.  
	 */
	static void SetBlueprintFunctionOrMacroCategory(UEdGraph* Graph, const FText& NewCategory, bool bDontRecompile=false);

	/** Finds the index of the specified graph (function or macro) in the parent (if it is not reorderable, then we will return INDEX_NONE) */
	static int32 FindIndexOfGraphInParent(UEdGraph* Graph);

	/** Reorders the specified graph (function or macro) to be at the new index in the parent (moving whatever was there to be after it), assuming it is reorderable and that is a valid index */
	static bool MoveGraphBeforeOtherGraph(UEdGraph* Graph, int32 NewIndex, bool bDontRecompile);

	/**
	 * Gets the custom category on the variable with the specified name.
	 *
	 * @param	VarName				Name of the variable
	 * @param	InLocalVarScope		Local variable's scope, if looking to modify a local variable
	 * @return						The custom category (None indicates the name will be the same as the blueprint)
	 */
	static FText GetBlueprintVariableCategory(UBlueprint* Blueprint, const FName& VarName, const UStruct* InLocalVarScope);

	/** Gets pointer to PropertyFlags of variable */
	static uint64* GetBlueprintVariablePropertyFlags(UBlueprint* Blueprint, const FName& VarName);

	/** Get RepNotify function name of variable */
	static FName GetBlueprintVariableRepNotifyFunc(UBlueprint* Blueprint, const FName& VarName);

	/** Set RepNotify function of variable */
	static void SetBlueprintVariableRepNotifyFunc(UBlueprint* Blueprint, const FName& VarName, const FName& RepNotifyFunc);

	/** Returns TRUE if the variable was created by the Blueprint */
	static bool IsVariableCreatedByBlueprint(UBlueprint* InBlueprint, UProperty* InVariableProperty);

	/**
	 * Find the index of a variable first declared in this blueprint. Returns INDEX_NONE if not found.
	 *
	 * @param	InName	Name of the variable to find.
	 *
	 * @return	The index of the variable, or INDEX_NONE if it wasn't introduced in this blueprint.
	 */
	static int32 FindNewVariableIndex(const UBlueprint* Blueprint, const FName& InName);

	/** Change the order of variables in the Blueprint */
	static bool MoveVariableBeforeVariable(UBlueprint* Blueprint, FName VarNameToMove, FName TargetVarName, bool bDontRecompile);

	/**
	 * Find the index of a timeline first declared in this blueprint. Returns INDEX_NONE if not found.
	 *
	 * @param	InName	Name of the variable to find.
	 *
	 * @return	The index of the variable, or INDEX_NONE if it wasn't introduced in this blueprint.
	 */
	static int32 FindTimelineIndex(const UBlueprint* Blueprint, const FName& InName);

	/** 
	 * Gets a list of SCS node variable names for the given blueprint.
	 *
	 * @param [in,out]	VariableNames		The list of variable names for the SCS node array.
	 */
	static void GetSCSVariableNameList(const UBlueprint* Blueprint, TSet<FName>& VariableNames);

	/** 
	 * Gets a list of function names in blueprints that implement the interface defined by the given blueprint.
	 *
	 * @param [in,out]	Blueprint			The interface blueprint to check.
	 * @param [in,out]	VariableNames		The list of function names for implementing blueprints.
	 */
	static void GetImplementingBlueprintsFunctionNameList(const UBlueprint* Blueprint, TSet<FName>& FunctionNames);

	/**
	 * Find the index of a SCS_Node first declared in this blueprint. Returns INDEX_NONE if not found.
	 *
	 * @param	InName	Name of the variable to find.
	 *
	 * @return	The index of the variable, or INDEX_NONE if it wasn't introduced in this blueprint.
	 */
	static int32 FindSCS_Node(const UBlueprint* Blueprint, const FName& InName);
	
	/** Returns whether or not the specified member var is a component */
	static bool IsVariableComponent(const FBPVariableDescription& Variable);

	/** Indicates if the variable is used on any graphs in this Blueprint*/
	static bool IsVariableUsed(const UBlueprint* Blueprint, const FName& Name, UEdGraph* LocalGraphScope = nullptr);

	/** Copies the value from the passed in string into a property. ContainerMem points to the Struct or Class containing Property */
	static bool PropertyValueFromString(const UProperty* Property, const FString& StrValue, uint8* Container);

	/** Copies the value from the passed in string into a property. DirectValue is the raw memory address of the property value */
	static bool PropertyValueFromString_Direct(const UProperty* Property, const FString& StrValue, uint8* DirectValue);

	/** Copies the value from a property into the string OutForm. ContainerMem points to the Struct or Class containing Property */
	static bool PropertyValueToString(const UProperty* Property, const uint8* Container, FString& OutForm);

	/** Copies the value from a property into the string OutForm. DirectValue is the raw memory address of the property value */
	static bool PropertyValueToString_Direct(const UProperty* Property, const uint8* DirectValue, FString& OutForm);

	/** Call PostEditChange() on all Actors based on the given Blueprint */
	static void PostEditChangeBlueprintActors(UBlueprint* Blueprint, bool bComponentEditChange = false);

	/** Checks if the property can be modified in given blueprint */
	DEPRECATED(4.17, "Use IsPropertyWritableInBlueprint instead.")
	static bool IsPropertyReadOnlyInCurrentBlueprint(const UBlueprint* Blueprint, const UProperty* Property);

	/** Enumeration of whether a property is writable or if not, why. */
	enum class EPropertyWritableState : uint8
	{
		Writable,
		Private,
		NotBlueprintVisible,
		BlueprintReadOnly
	};

	/** Returns an enumeration indicating if the property can be written to by the given Blueprint */
	static EPropertyWritableState IsPropertyWritableInBlueprint(const UBlueprint* Blueprint, const UProperty* Property);

	/** Enumeration of whether a property is readable or if not, why. */
	enum class EPropertyReadableState : uint8
	{
		Readable,
		Private,
		NotBlueprintVisible
	};

	/** Returns an enumeration indicating if the property can be read by the given Blueprint */
	static EPropertyReadableState IsPropertyReadableInBlueprint(const UBlueprint* Blueprint, const UProperty* Property);

	/** Ensures that the CDO root component reference is valid for Actor-based Blueprints */
	static void UpdateRootComponentReference(UBlueprint* Blueprint);

	/** Determines if this property is associated with a component that would be displayed in the SCS editor */
	static bool IsSCSComponentProperty(UObjectProperty* MemberProperty);

	/** Attempts to match up the FComponentKey with a ComponentTemplate from the Blueprint's UCS */
	static UActorComponent* FindUCSComponentTemplate(const FComponentKey& ComponentKey);

	/** Takes the Blueprint's NativizedFlag property and applies it to the authoritative config (does the same for flagged dependencies) */
	static bool PropagateNativizationSetting(UBlueprint* Blueprint);

	/** Retrieves all dependencies that need to be nativized for this to work as a nativized Blueprint */
	static void FindNativizationDependencies(UBlueprint* Blueprint, TArray<UClass*>& NativizeDependenciesOut);

	/** Returns whether or not the given Blueprint should be nativized implicitly, regardless of whether or not the user has explicitly enabled it */
	static bool ShouldNativizeImplicitly(const UBlueprint* Blueprint);

	//////////////////////////////////////////////////////////////////////////
	// Interface

	/** 
	 * Find the interface Guid for a function if it exists.
	 * 
	 * @param	Function		The function to find a graph for.
	 * @param	InterfaceClass	The interface's generated class.
	 */
	static FGuid FindInterfaceFunctionGuid(const UFunction* Function, const UClass* InterfaceClass);

	/** Add a new interface, and member function graphs to the blueprint */
	static bool ImplementNewInterface(UBlueprint* Blueprint, const FName& InterfaceClassName);

	/** Remove an implemented interface, and its associated member function graphs.  If bPreserveFunctions is true, then the interface will move its functions to be normal implemented blueprint functions */
	static void RemoveInterface(UBlueprint* Blueprint, const FName& InterfaceClassName, bool bPreserveFunctions = false);

	/**
	* Promotes a Graph from being an Interface Override to a full member function
	*
	* @param InBlueprint			Blueprint the graph is contained within
	* @param InInterfaceGraph		The graph to promote
	*/
	static void PromoteGraphFromInterfaceOverride(UBlueprint* InBlueprint, UEdGraph* InInterfaceGraph);

	/** Gets the graphs currently in the blueprint associated with the specified interface */
	static void GetInterfaceGraphs(UBlueprint* Blueprint, const FName& InterfaceClassName, TArray<UEdGraph*>& ChildGraphs);

	/** Makes sure that all graphs for all interfaces we implement exist, and add if not */
	static void ConformImplementedInterfaces(UBlueprint* Blueprint);

	/** Makes sure that all function graphs are flagged as bAllowDeletion=true, except for construction script and animgraph: */
	static void ConformAllowDeletionFlag(UBlueprint* Blueprint);
	
	/** Makes sure that all NULL graph references are removed from SubGraphs and top-level graph arrays */
	static void PurgeNullGraphs(UBlueprint* Blueprint);

	/** Handle old AnimBlueprints (state machines in the wrong position, transition graphs with the wrong schema, etc...) */
	static void UpdateOutOfDateAnimBlueprints(UBlueprint* Blueprint);

	/** Handle fixing up composite nodes within the blueprint*/
	static void UpdateOutOfDateCompositeNodes(UBlueprint* Blueprint);

	/** Handle fixing up composite nodes within the specified Graph of Blueprint, and correctly setting the Outer */
	static void UpdateOutOfDateCompositeWithOuter(UBlueprint* Blueprint, UEdGraph* Outer );

	/** Handle stale components and ensure correct flags are set */
	static void UpdateComponentTemplates(UBlueprint* Blueprint);

	/** Handle stale transactional flags on blueprints */
	static void UpdateTransactionalFlags(UBlueprint* Blueprint);

	/** Handle stale pin watches */
	static void UpdateStalePinWatches( UBlueprint* Blueprint );

	/** Updates the cosmetic information cache for macros */
	static void ClearMacroCosmeticInfoCache(UBlueprint* Blueprint);

	/** Returns the cosmetic information for the specified macro graph, caching it if necessary */
	static FBlueprintMacroCosmeticInfo GetCosmeticInfoForMacro(UEdGraph* MacroGraph);

	/** Return the first function from implemented interface with given name */
	static UFunction* FindFunctionInImplementedInterfaces(const UBlueprint* Blueprint, const FName& FunctionName, bool* bOutInvalidInterface = nullptr, bool bGetAllInterfaces = false);

	/** 
	 * Build a list of all interface classes either implemented by this blueprint or through inheritance
	 * @param		Blueprint				The blueprint to find interfaces for
	 * @param		bGetAllInterfaces		If true, get all the implemented and inherited. False, just gets the interfaces implemented directly.
	 * @param [out]	ImplementedInterfaces	The interfaces implemented by this blueprint
	 */
	static void FindImplementedInterfaces(const UBlueprint* Blueprint, bool bGetAllInterfaces, TArray<UClass*>& ImplementedInterfaces);

	/**
	 * Finds a unique name with a base of the passed in string, appending numbers as needed
	 *
	 * @param InBlueprint		The blueprint the kismet object's name needs to be unique in
	 * @param InBaseName		The base name to use
	 * @param InScope			Scope, if any, of the unique kismet name to generate, used for locals
	 *
	 * @return					A unique name that will not conflict in the Blueprint
	 */
	static FName FindUniqueKismetName(const UBlueprint* InBlueprint, const FString& InBaseName, UStruct* InScope = NULL);

	/** Finds a unique and valid name for a custom event */
	static FName FindUniqueCustomEventName(const UBlueprint* Blueprint);

	//////////////////////////////////////////////////////////////////////////
	// Timeline

	/** Finds a name for a timeline that is not already in use */
	static FName FindUniqueTimelineName(const UBlueprint* Blueprint);

	/** Add a new timeline with the supplied name to the blueprint */
	static class UTimelineTemplate* AddNewTimeline(UBlueprint* Blueprint, const FName& TimelineVarName);

	/** Remove the timeline from the blueprint 
	 * @note Just removes the timeline from the associated timelist in the Blueprint. Does not remove the node graph
	 * object representing the timeline itself.
	 * @param Timeline			The timeline to remove
	 * @param bDontRecompile	If true, the blueprint will not be marked as modified, and will not be recompiled.  
	 */
	static void RemoveTimeline(UBlueprint* Blueprint, class UTimelineTemplate* Timeline, bool bDontRecompile=false);

	/** Find the node that owns the supplied timeline template */
	static class UK2Node_Timeline* FindNodeForTimeline(UBlueprint* Blueprint, UTimelineTemplate* Timeline);

	//////////////////////////////////////////////////////////////////////////
	// LevelScriptBlueprint

	/** Find how many nodes reference the supplied actor */
	static bool FindReferencesToActorFromLevelScript(ULevelScriptBlueprint* LevelScriptBlueprint, AActor* InActor, TArray<UK2Node*>& ReferencedToActors);

	/** Replace all references of the old actor with the new actor */
	static void ReplaceAllActorRefrences(ULevelScriptBlueprint* InLevelScriptBlueprint, AActor* InOldActor, AActor* InNewActor);

	/** Function to call modify() on all graph nodes which reference this actor */
	static void  ModifyActorReferencedGraphNodes(ULevelScriptBlueprint* LevelScriptBlueprint, const AActor* InActor);

	/**
	 * Called after a level script blueprint is changed and nodes should be refreshed for it's new level script actor
	 *
	 * @param	LevelScriptActor	The newly-created level script actor that should be (re-)bound to
	 * @param	ScriptBlueprint		The level scripting blueprint that contains the bound events to try and bind delegates to this actor for
	 */
	static void FixLevelScriptActorBindings(ALevelScriptActor* LevelScriptActor, const class ULevelScriptBlueprint* ScriptBlueprint);

	/**
	 * Find how many actors reference the supplied actor
	 *
	 * @param InActor The Actor to count references to.
	 * @param InClassesToIgnore An array of class types to ignore, even if there is an instance of one that references the InActor
	 * @param OutReferencingActors An array of actors found that reference the specified InActor
	 */
	static void FindActorsThatReferenceActor( AActor* InActor, TArray<UClass*>& InClassesToIgnore, TArray<AActor*>& OutReferencingActors );

	/**
	 * Go through the world and build a map of all actors that are referenced by other actors.
	 * @param InWorld The world to scan for Actors.
	 * @param InClassesToIgnore  An array of class types to ignore, even if there is an instance of one that references another Actor
	 * @param OutReferencingActors A map of Actors that are referenced by a list of other Actors.
	*/
	static void GetActorReferenceMap(UWorld* InWorld, TArray<UClass*>& InClassesToIgnore, TMap<AActor*, TArray<AActor*> >& OutReferencingActors);

	//////////////////////////////////////////////////////////////////////////
	// Diagnostics

	// Diagnostic use only: Lists all of the objects have a direct outer of Package
	static void ListPackageContents(UPackage* Package, FOutputDevice& Ar);

	// Diagnostic exec commands
	static bool KismetDiagnosticExec(const TCHAR* Stream, FOutputDevice& Ar);

	/**
	 * Searches the world for any blueprints that are open and do not have a debug instances set and sets one if possible.
	 * It will favor a selected instance over a non selected one
	 */
	static void FindAndSetDebuggableBlueprintInstances();

	/**
	 * Records node create events for analytics
	 */
	static void AnalyticsTrackNewNode( UEdGraphNode* NewNode );

	/**
	 * Generates a unique graph name for the supplied blueprint (guaranteed to not 
	 * cause a naming conflict at the time of the call).
	 * 
	 * @param  BlueprintOuter	The blueprint you want a unique graph name for.
	 * @param  ProposedName		The name you want to give the graph (the result will be some permutation of this string).
	 * @return A unique name intended for a new graph.
	 */
	static FName GenerateUniqueGraphName(UBlueprint* const BlueprintOuter, FString const& ProposedName);

	/* Checks if the passed in selection set causes cycling on compile
	 *
	 * @param InSelectionSet		The selection set to check for a cycle within
	 * @param InMessageLog			Log to report cycling errors to
	 *
	 * @return						Returns TRUE if the selection does cause cycling
	 */
	static bool CheckIfSelectionIsCycling(const TSet<UEdGraphNode*>& InSelectionSet, FCompilerResultsLog& InMessageLog);
	
	/**
	 * A utility function intended to aid the construction of a specific blueprint 
	 * palette item. Some items can be renamed, so this method determines if that is 
	 * allowed.
	 * 
	 * @param  ActionIn				The action associated with the palette item you're querying for.
	 * @param  BlueprintEditorIn	The blueprint editor owning the palette item you're querying for.
	 * @return True is the item CANNOT be renamed, false if it can.
	 */
	static bool IsPaletteActionReadOnly(TSharedPtr<FEdGraphSchemaAction> ActionIn, TSharedPtr<class FBlueprintEditor> const BlueprintEditorIn);

	/**
	 * Finds the entry and result nodes for a function or macro graph
	 *
	 * @param InGraph			The graph to search through
	 * @param OutEntryNode		The found entry node for the graph
	 * @param OutResultNode		The found result node for the graph
	 */
	static void GetEntryAndResultNodes(const UEdGraph* InGraph, TWeakObjectPtr<class UK2Node_EditablePinBase>& OutEntryNode, TWeakObjectPtr<class UK2Node_EditablePinBase>& OutResultNode);


	/**
	 * Finds the entry node for a function or macro graph
	 *
	 * @param InGraph			The graph to search through
	 * @return		The found entry node for the graph
	 */
	static class UK2Node_EditablePinBase* GetEntryNode(const UEdGraph* InGraph);

	/**
	 * Returns the function meta data block for the graph entry node.
	 *
	 * @param InGraph			The graph to search through
	 * @return					If valid a pointer to the user declared function meta data structure otherwise nullptr.
	 */
	static FKismetUserDeclaredFunctionMetadata* GetGraphFunctionMetaData(const UEdGraph* InGraph);

	/**
	 * Returns the description of the graph from the metadata
	 *
	 * @param InGraph			Graph to find the description of
	 * @return					The description of the graph
	 */
	static FText GetGraphDescription(const UEdGraph* InGraph);

	/** Checks if a graph (or any sub-graphs or referenced graphs) have latent function nodes */
	static bool CheckIfGraphHasLatentFunctions(UEdGraph* InGraph);

	/**
	 * Creates a function result node or returns the current one if one exists
	 *
	 * @param InFunctionEntryNode		The function entry node to spawn the result node for
	 * @return							Spawned result node
	 */
	static class UK2Node_FunctionResult* FindOrCreateFunctionResultNode(class UK2Node_EditablePinBase* InFunctionEntryNode);

	/** 
	 * Determine the best icon to represent the given pin.
	 *
	 * @param PinType		The pin get the icon for.
	 * @param returns a brush that best represents the icon (or Kismet.VariableList.TypeIcon if none is available )
	 */
	static const struct FSlateBrush* GetIconFromPin(const FEdGraphPinType& PinType, bool bIsLarge = false);

	/**
	 * Determine the best secondary icon icon to represent the given pin.
	 */
	static const struct FSlateBrush* GetSecondaryIconFromPin(const FEdGraphPinType& PinType);

	/**
	 * Returns true if this terminal type can be hashed (native types need GetTypeHash, script types are always hashable).
	 */
	static bool HasGetTypeHash(const FEdGraphPinType& PinType);

	/**
	 * Returns true if this type of UProperty can be hashed. Matches native constructors of UNumericProperty, etc.
	 */
	static bool PropertyHasGetTypeHash(const UProperty* PropertyType);

	/**
	 * Returns true if the StructType is native and has a GetTypeHash or is non-native and all of its member types are handled by UScriptStruct::GetStructTypeHash
	 */
	static bool StructHasGetTypeHash(const UScriptStruct* StructType);

	/**
	 * Generate component instancing data (for cooked builds).
	 *
	 * @param ComponentTemplate		The component template to generate instancing data for.
	 * @param OutData				The generated component instancing data.
	 * @param bUseTemplateArchetype	Whether or not to use the template archetype or the template CDO for delta serialization (default is to use the template CDO).
	 * @return						TRUE if component instancing data was built, FALSE otherwise.
	 */
	static void BuildComponentInstancingData(UActorComponent* ComponentTemplate, FBlueprintCookedComponentInstancingData& OutData, bool bUseTemplateArchetype = false);

protected:
	// Removes all NULL graph references from the SubGraphs array and recurses thru the non-NULL ones
	static void CleanNullGraphReferencesRecursive(UEdGraph* Graph);

	// Removes all NULL graph references in the specified array
	static void CleanNullGraphReferencesInArray(UBlueprint* Blueprint, TArray<UEdGraph*>& GraphArray);

	/**
	 * Checks that the actor type matches the blueprint type (or optionally is BASED on the same type. 
	 *
	 * @param InActorObject						The object to check
	 * @param InBlueprint						The blueprint to check against
	 * @param bInDisallowDerivedBlueprints		if true will only allow exact type matches, if false derived types are allowed.
	 */
	static bool IsObjectADebugCandidate( AActor* InActorObject, UBlueprint* InBlueprint , bool bInDisallowDerivedBlueprints );

	/** Validate child blueprint member variables against the given variable name */
	static bool ValidateAllMemberVariables(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName InVariableName);

	/** Validate child blueprint component member variables against the given variable name */
	static bool ValidateAllComponentMemberVariables(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName& InVariableName);

	/** Validates all timelines of the passed blueprint against the given variable name */
	static bool ValidateAllTimelines(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName& InVariableName);

	/** Validates all function graphs of the passed blueprint against the given variable name */
	static bool ValidateAllFunctionGraphs(UBlueprint* InBlueprint, UBlueprint* InParentBlueprint, const FName& InVariableName);

	/**
	 * Checks if the passed node connects to the selection set
	 *
	 * @param InNode				The node to check
	 * @param InSelectionSet		The selection set to check for a connection to
	 *
	 * @return						Returns TRUE if the node does connect to the selection set
	 */
	static bool CheckIfNodeConnectsToSelection(UEdGraphNode* InNode, const TSet<UEdGraphNode*>& InSelectionSet);

	/**
	 * Returns an array of variables Get/Set nodes of the current variable
	 *
	 * @param InVarName		Variable to check for being in use
	 * @param InBlueprint	Blueprint to check within
	 * @param InScope		Option scope for local variables
	 * @return				Array of variable nodes
	 */
	static TArray<UK2Node*> GetNodesForVariable(const FName& InVarName, const UBlueprint* InBlueprint, const UStruct* InScope = nullptr);

	/**
	 * Helper function to warn user of the results of changing var type by displaying a suppressible dialog
	 *
	 * @param InVarName		Variable name to display in the dialog message
	 * @return				TRUE if the user wants to change the variable type
	 */
	static bool VerifyUserWantsVariableTypeChanged(const FName& InVarName);

	/**
	 * Helper function to get all loaded Blueprints that are children (or using as an interface) the passed Blueprint
	 *
	 * @param InBlueprint		Blueprint to find children of
	 * @param OutBlueprints		Filled out with child Blueprints
	 */
	static void GetLoadedChildBlueprints(UBlueprint* InBlueprint, TArray<UBlueprint*>& OutBlueprints);

	/**
	 * Validates flags and settings on object pins, keeping them from being given default values and from being in invalid states
	 *
	 * @param InOutVarDesc		The variable description to validate
	 */
	static void PostSetupObjectPinType(UBlueprint* InBlueprint, FBPVariableDescription& InOutVarDesc);

public:
	static FName GetFunctionNameFromClassByGuid(const UClass* InClass, const FGuid FunctionGuid);
	static bool GetFunctionGuidFromClassByFieldName(const UClass* InClass, const FName FunctionName, FGuid& FunctionGuid);

	/**
	 * Returns a friendly class display name for the specified class (removing things like _C from the end, may localize the class name).  Class can be nullptr.
	 */
	static FText GetFriendlyClassDisplayName(const UClass* Class);

	/**
	 * Returns a class name for the specified class that has no automatic suffixes, but is otherwise unmodified.  Class can be nullptr.
	 */
	static FString GetClassNameWithoutSuffix(const UClass* Class);

	/**
	 * Remove overridden component templates from instance component handlers when a parent class disables editable when inherited boolean.
	 */
	static void HandleDisableEditableWhenInherited(UObject* ModifiedObject, TArray<UObject*>& ArchetypeInstances);

	/**
	 * Returns the BPs most derived native parent type:
	 */
	static UClass* GetNativeParent(const UBlueprint* BP);

	/**
	 * Returns true if this BP is currently based on a type that returns true for the UObject::ImplementsGetWorld() call:
	 */
	static bool ImplentsGetWorld(const UBlueprint* BP);
};

struct UNREALED_API FBlueprintDuplicationScopeFlags
{
	enum EFlags : uint32
	{
		NoFlags = 0,
		NoExtraCompilation = 1 << 0,
		TheSameTimelineGuid = 1 << 1,
		// This flag is needed for C++ backend (while compiler validates graphs). The actual BPGC type is compatible with the original BPGC.
		ValidatePinsUsingSourceClass = 1 << 2,
		TheSameNodeGuid = 1 << 3,
	};

	static uint32 bStaticFlags;
	static bool HasAnyFlag(uint32 InFlags)
	{
		return 0 != (bStaticFlags & InFlags);
	}

	TGuardValue<uint32> Guard;
	FBlueprintDuplicationScopeFlags(uint32 InFlags) : Guard(bStaticFlags, InFlags) {}
};
struct UNREALED_API FMakeClassSpawnableOnScope
{
	UClass* Class;
	bool bIsDeprecated;
	bool bIsAbstract;
	FMakeClassSpawnableOnScope(UClass* InClass)
		: Class(InClass), bIsDeprecated(false), bIsAbstract(false)
	{
		if (Class)
		{
			bIsDeprecated = Class->HasAnyClassFlags(CLASS_Deprecated);
			Class->ClassFlags &= ~CLASS_Deprecated;
			bIsAbstract = Class->HasAnyClassFlags(CLASS_Abstract);
			Class->ClassFlags &= ~CLASS_Abstract;
		}
	}
	~FMakeClassSpawnableOnScope()
	{
		if (Class)
		{
			if (bIsAbstract)
			{
				Class->ClassFlags |= CLASS_Abstract;
			}

			if (bIsDeprecated)
			{
				Class->ClassFlags |= CLASS_Deprecated;
			}
		}
	}
};
