// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AIGraph.h"
#include "UObject/UObjectHash.h"
#include "EdGraph/EdGraphSchema.h"
#include "AIGraphTypes.h"
#include "AIGraphNode.h"
#include "AIGraphModule.h"

UAIGraph::UAIGraph(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bLockUpdates = false;
}

void UAIGraph::UpdateAsset(int32 UpdateFlags)
{
	// empty in base class
}

void UAIGraph::OnCreated()
{
	MarkVersion();
}

void UAIGraph::OnLoaded()
{
	UpdateDeprecatedClasses();
	UpdateUnknownNodeClasses();
}

void UAIGraph::Initialize()
{
	UpdateVersion();
}

void UAIGraph::UpdateVersion()
{
	if (GraphVersion == 1)
	{
		return;
	}

	MarkVersion();
	Modify();
}

void UAIGraph::MarkVersion()
{
	GraphVersion = 1;
}

bool UAIGraph::UpdateUnknownNodeClasses()
{
	bool bUpdated = false;
	for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
	{
		UAIGraphNode* MyNode = Cast<UAIGraphNode>(Nodes[NodeIdx]);
		if (MyNode)
		{
			const bool bUpdatedNode = MyNode->RefreshNodeClass();
			bUpdated = bUpdated || bUpdatedNode;

			for (int32 SubNodeIdx = 0; SubNodeIdx < MyNode->SubNodes.Num(); SubNodeIdx++)
			{
				if (MyNode->SubNodes[SubNodeIdx])
				{
					const bool bUpdatedSubNode = MyNode->SubNodes[SubNodeIdx]->RefreshNodeClass();
					bUpdated = bUpdated || bUpdatedSubNode;
				}
			}
		}
	}

	return bUpdated;
}

void UpdateAIGraphNodeErrorMessage(UAIGraphNode& Node)
{
	// Broke out setting error message in to own function so it can be reused when iterating nodes collection.
	if (Node.NodeInstance)
	{
		Node.ErrorMessage = FGraphNodeClassHelper::GetDeprecationMessage(Node.NodeInstance->GetClass());
	}
	else
	{
		// Null instance. Do we have any meaningful class data?
		FString StoredClassName = Node.ClassData.GetClassName();
		StoredClassName.RemoveFromEnd(TEXT("_C"));

		if (!StoredClassName.IsEmpty())
		{
			// There is class data here but the instance was not be created.
			static const FString IsMissingClassMessage(" class missing. Referenced by ");
			Node.ErrorMessage = StoredClassName + IsMissingClassMessage + Node.GetFullName();
		}
	}

	if (Node.HasErrors())
	{
		UE_LOG(LogAIGraph, Error, TEXT("%s"), *Node.ErrorMessage);
	}
}

void UAIGraph::UpdateDeprecatedClasses()
{
	// This function sets error messages and logs errors about nodes.

	for (int32 Idx = 0, IdxNum = Nodes.Num(); Idx < IdxNum; ++Idx)
	{
		UAIGraphNode* Node = Cast<UAIGraphNode>(Nodes[Idx]);
		if (Node != nullptr)
		{
			UpdateAIGraphNodeErrorMessage(*Node);
			
			for (int32 SubIdx = 0, SubIdxNum = Node->SubNodes.Num(); SubIdx < SubIdxNum; ++SubIdx)
			{
				if (Node->SubNodes[SubIdx] != nullptr)
				{
					UpdateAIGraphNodeErrorMessage(*Node->SubNodes[SubIdx]);
				}
			}
		}
	}
}

void UAIGraph::Serialize(FArchive& Ar)
{
	// Overridden to flags up errors in the behavior tree while cooking.
	Super::Serialize(Ar);

	if (Ar.IsSaving() || Ar.IsCooking())
	{
		// Logging of errors happens in UpdateDeprecatedClasses
		UpdateDeprecatedClasses();
	}
}

void UAIGraph::UpdateClassData()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UAIGraphNode* Node = Cast<UAIGraphNode>(Nodes[Idx]);
		if (Node)
		{
			Node->UpdateNodeClassData();

			for (int32 SubIdx = 0; SubIdx < Node->SubNodes.Num(); SubIdx++)
			{
				if (Node->SubNodes[SubIdx])
				{
					Node->UpdateNodeClassData();
				}
			}
		}
	}
}

void UAIGraph::CollectAllNodeInstances(TSet<UObject*>& NodeInstances)
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UAIGraphNode* MyNode = Cast<UAIGraphNode>(Nodes[Idx]);
		if (MyNode)
		{
			NodeInstances.Add(MyNode->NodeInstance);

			for (int32 SubIdx = 0; SubIdx < MyNode->SubNodes.Num(); SubIdx++)
			{
				if (MyNode->SubNodes[SubIdx])
				{
					NodeInstances.Add(MyNode->SubNodes[SubIdx]->NodeInstance);
				}
			}
		}
	}
}

bool UAIGraph::CanRemoveNestedObject(UObject* TestObject) const
{
	return !TestObject->IsA(UEdGraphNode::StaticClass()) &&
		!TestObject->IsA(UEdGraph::StaticClass()) &&
		!TestObject->IsA(UEdGraphSchema::StaticClass());
}

void UAIGraph::RemoveOrphanedNodes()
{
	TSet<UObject*> NodeInstances;
	CollectAllNodeInstances(NodeInstances);

	NodeInstances.Remove(nullptr);

	// Obtain a list of all nodes actually in the asset and discard unused nodes
	TArray<UObject*> AllInners;
	const bool bIncludeNestedObjects = false;
	GetObjectsWithOuter(GetOuter(), AllInners, bIncludeNestedObjects);
	for (auto InnerIt = AllInners.CreateConstIterator(); InnerIt; ++InnerIt)
	{
		UObject* TestObject = *InnerIt;
		if (!NodeInstances.Contains(TestObject) && CanRemoveNestedObject(TestObject))
		{
			OnNodeInstanceRemoved(TestObject);

			TestObject->SetFlags(RF_Transient);
			TestObject->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}
	}
}

void UAIGraph::OnNodeInstanceRemoved(UObject* NodeInstance)
{
	// empty in base class
}

void UAIGraph::OnNodesPasted(const FString& ImportStr)
{
	// empty in base class
}

UEdGraphPin* UAIGraph::FindGraphNodePin(UEdGraphNode* Node, EEdGraphPinDirection Dir)
{
	UEdGraphPin* Pin = nullptr;
	for (int32 Idx = 0; Idx < Node->Pins.Num(); Idx++)
	{
		if (Node->Pins[Idx]->Direction == Dir)
		{
			Pin = Node->Pins[Idx];
			break;
		}
	}

	return Pin;
}

bool UAIGraph::IsLocked() const
{
	return bLockUpdates;
}

void UAIGraph::LockUpdates()
{
	bLockUpdates = true;
}

void UAIGraph::UnlockUpdates()
{
	bLockUpdates = false;
	UpdateAsset();
}

void UAIGraph::OnSubNodeDropped()
{
	NotifyGraphChanged();
}
