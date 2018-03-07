// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "ConnectionDrawingPolicy.h"

class FSlateWindowElementList;
class UEdGraph;

/////////////////////////////////////////////////////
// FKismetConnectionDrawingPolicy

// This class draws the connections for an UEdGraph using a K2-based schema
class GRAPHEDITOR_API FKismetConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
protected:
	// Times for one execution pair within the current graph
	struct FTimePair
	{
		double PredExecTime; // time that the node with the associated exec-out was executed
		double ThisExecTime; // time that the following node was executed

		FTimePair()
			: PredExecTime(0.0)
			, ThisExecTime(0.0)
		{
		}
	};
	/** 
	 * Map of exec-pins to a pair of execution times (holds timing info for the 
	 * owning-node, paired with a time of execution for the linked node)... only 
	 * one timing-pair per pin because exec outputs can only connect to one node
	 */
	typedef TMap<UEdGraphPin*, FTimePair> FExecPairingMap;

	// Map of executed nodes, and the execution pins that lead to them being ran
	TMap<UEdGraphNode*, FExecPairingMap> PredecessorPins;

	UEdGraph* GraphObj;
	double CurrentTime;
	double LatestTimeDiscovered;

	FLinearColor AttackColor;
	FLinearColor SustainColor;
	FLinearColor ReleaseColor;

	float AttackWireThickness;
	float SustainWireThickness;
	float ReleaseWireThickness;
	float DefaultDataWireThickness;
	float DefaultExecutionWireThickness;

	float TracePositionBonusPeriod;
	float TracePositionExponent;
	float AttackHoldPeriod;
	float DecayPeriod;
	float DecayExponent;
	float SustainHoldPeriod;
	float ReleasePeriod;
	float ReleaseExponent;

	// Each time a knot is encountered, input geometry is compared to output geometry to see if the pins on the knot need to be reversed
	TMap<class UK2Node_Knot*, bool> KnotToReversedDirectionMap;

protected:
	// Should this wire be treated as an execution pin, querying the execution trace for connected nodes to draw it differently?
	virtual bool TreatWireAsExecutionPin(UEdGraphPin* InputPin, UEdGraphPin* OutputPin) const;
public:
	FKismetConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	virtual void BuildExecutionRoadmap();

	virtual bool CanBuildRoadmap() const;

	void CalculateEnvelopeAlphas(double ExecutionTime, /*out*/ float& AttackAlpha, /*out*/ float& SustainAlpha) const;

	// FConnectionDrawingPolicy interface
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override;
	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& PinGeometries, FArrangedChildren& ArrangedNodes) override;
	// End of FConnectionDrawingPolicy interface

	virtual void DetermineStyleOfExecWire(float& Thickness, FLinearColor& WireColor, bool& bDrawBubbles, const FTimePair& Times);

	virtual void SetIncompatiblePinDrawState(const TSharedPtr<SGraphPin>& StartPin, const TSet< TSharedRef<SWidget> >& VisiblePins) override;
	virtual void ResetIncompatiblePinDrawState(const TSet< TSharedRef<SWidget> >& VisiblePins) override;

protected: 
	/**
	* Checks that the specified nodes are connected in the graph directly by an 
	* exec pin.
	* 
	* @param  InputNode	The node whose exec output(s) should be connected to OutputNode.
	* @param  OutputNode	The node whose exec input should be coming from the InputNode.
	* @return True if the two nodes are connected in the graph, otherwise false.
	*/
	bool AreNodesGraphicallySequential(UEdGraphNode* InputNode, UEdGraphNode* OutputNode) const;

	/**
	 * With the introduction of "reroute" (UK2Node_Knot) nodes, we have to 
	 * backtrace to find a node that was actually executed before the one in 
	 * question (knot nodes are removed at compile time, and therefore never
	 * executed).
	 *
	 * This function recursively searches for any pins feeding the OutputPin (or 
	 * the OutputPin itself); it looks for a pin on a non-reroute node (one that
	 * is listed in NodeExecutionList).
	 * 
	 * @param  OutputPin			An exec pin, leading into the node in question (an output pin on some other node).
	 * @param  NodeExecutionList	A list of pins that feed the execution of a specific node.
	 * @return Null if OutputPin did not cause the invocation of the node in question, otherwise a valid time pair denoting when the node was executed.
	 */
	FTimePair const* BackTraceExecPath(UEdGraphPin const* const OutputPin, FExecPairingMap const* const NodeExecutionList);

	bool ShouldChangeTangentForKnot(class UK2Node_Knot* Knot);
	bool GetAverageConnectedPosition(class UK2Node_Knot* Knot, EEdGraphPinDirection Direction, FVector2D& OutPos) const;
	bool FindPinCenter(UEdGraphPin* Pin, FVector2D& OutCenter) const;
};
