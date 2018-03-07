// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_TunnelBoundary.generated.h"

class FCompilerResultsLog;
class UEdGraph;
class UK2Node_Tunnel;

enum ETunnelBoundaryType
{
	TBT_Unknown = 0,
	TBT_EntrySite,
	TBT_ExitSite,
	TBT_EndOfThread
};

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_TunnelBoundary : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** Base Name */
	UPROPERTY()
	FName BaseName;

	/** Final exit site */
	UPROPERTY()
	FEdGraphPinReference FinalExitSite;

	/** Wether or not this is an exit point */
	ETunnelBoundaryType TunnelBoundaryType;

public:

	//~ Begin of UK2Node Interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//~ End UK2Node Interface

	/** Creates Boundary nodes for a tunnel graph */
	static void CreateBoundaryNodesForGraph(UEdGraph* TunnelGraph, class FCompilerResultsLog& MessageLog);

	/** Creates Boundary nodes for a tunnel instance */
	static void CreateBoundaryNodesForTunnelInstance(UK2Node_Tunnel* TunnelInstance, UEdGraph* TunnelGraph, class FCompilerResultsLog& MessageLog);

	/** Creates Boundaries for expansion nodes */
	static void CreateBoundariesForExpansionNodes(UEdGraphNode* SourceNode, TArray<UEdGraphNode*>& ExpansionNodes, TMap<UEdGraphPin*, UEdGraphPin*>& LinkedPinMap, FCompilerResultsLog& MessageLog);

	/** Checks if the tunnel node is a pure tunnel rather than a tunnel instance */
	static bool IsPureTunnel(UK2Node_Tunnel* Tunnel);

protected:

	/** Wires up the tunnel entry boundries */
	void WireUpTunnelEntry(UK2Node_Tunnel* TunnelInstance, UEdGraphPin* TunnelPin, FCompilerResultsLog& MessageLog);

	/** Wires up the tunnel exit boundries */
	void WireUpTunnelExit(UK2Node_Tunnel* TunnelInstance, UEdGraphPin* TunnelPin, FCompilerResultsLog& MessageLog);

	/** Wires up the entry boundries */
	void WireUpEntry(UEdGraphNode* SourceNode, UEdGraphPin* SourcePin, const TArray<UEdGraphPin*>& EntryPins, FCompilerResultsLog& MessageLog);

	/** Wires up the exit boundries */
	void WireUpExit(UEdGraphNode* SourceNode, UEdGraphPin* SourcePin, const TArray<UEdGraphPin*>& ExitPins, FCompilerResultsLog& MessageLog);

	/** Create base node name */
	void CreateBaseNodeName(UEdGraphNode* SourceNode);

	/** Build a guid map for nodes from the original graph */
	static void BuildSourceNodeMap(UEdGraphNode* Tunnel, TMap<FGuid, const UEdGraphNode*>& SourceNodeMap);

	/** Determines the true source tunnel instance */
	static UEdGraphNode* FindTrueSourceTunnelInstance(UEdGraphNode* Tunnel, UEdGraphNode* SourceTunnelInstance);

	/** Tries to map and locate tunnel exit or termination sites */
	static void FindTunnelExitSiteInstances(UEdGraphPin* NodeEntryPin, TArray<UEdGraphPin*>& ExitPins, TArray<UEdGraphPin*>& VisitedPins, const UEdGraphNode* TunnelExit = nullptr);

};
