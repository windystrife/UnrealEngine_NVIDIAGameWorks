// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/////////////////////////////////////////////////////
// USoundSubmixGraph

#include "SoundSubmixGraph/SoundSubmixGraph.h"
#include "Sound/SoundSubmix.h"
#include "SoundSubmixGraph/SoundSubmixGraphNode.h"
#include "GraphEditor.h"
#include "UObject/Package.h"

class FSoundSubmixAudioEditor : public ISoundSubmixAudioEditor
{
public:
	void RefreshGraphLinks(UEdGraph* SoundSubmixGraph) override
	{
		CastChecked<USoundSubmixGraph>(SoundSubmixGraph)->RefreshGraphLinks();
	}
};

USoundSubmixGraph::USoundSubmixGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RootSoundSubmix(NULL)
{
	if (!USoundSubmix::GetSoundSubmixAudioEditor().IsValid())
	{
		USoundSubmix::SetSoundSubmixAudioEditor(TSharedPtr<ISoundSubmixAudioEditor>(new FSoundSubmixAudioEditor()));
	}
}

void USoundSubmixGraph::SetRootSoundSubmix(USoundSubmix* InSoundSubmix)
{
	RootSoundSubmix = InSoundSubmix;
}

USoundSubmix* USoundSubmixGraph::GetRootSoundSubmix() const
{
	return RootSoundSubmix;
}

void USoundSubmixGraph::RebuildGraph()
{
	check(RootSoundSubmix);

	// Don't allow initial graph rebuild to affect package dirty state; remember current state...
	UPackage* Package = GetOutermost();
	const bool bIsDirty = Package->IsDirty();

	Modify();

	RemoveAllNodes();

	ConstructNodes(RootSoundSubmix, 0, 0);

	NotifyGraphChanged();

	// ...and restore it
	Package->SetDirtyFlag(bIsDirty);
}

void USoundSubmixGraph::AddDroppedSoundSubmixes(const TArray<USoundSubmix*>& SoundSubmixes, int32 NodePosX, int32 NodePosY)
{
	Modify();

	for (int32 ClassIndex = 0; ClassIndex < SoundSubmixes.Num(); ClassIndex++)
	{
		NodePosY += ConstructNodes(SoundSubmixes[ClassIndex], NodePosX, NodePosY);
	}

	NotifyGraphChanged();
}

void USoundSubmixGraph::AddNewSoundSubmix(UEdGraphPin* FromPin, class USoundSubmix* SoundSubmix, int32 NodePosX, int32 NodePosY, bool bSelectNewNode/* = true*/)
{
	check(SoundSubmix->ChildSubmixes.Num() == 0);

	Modify();

	USoundSubmixGraphNode* GraphNode = CreateNode(SoundSubmix, NodePosX, NodePosY, bSelectNewNode);
	GraphNode->AutowireNewNode(FromPin);

	NotifyGraphChanged();
}

bool USoundSubmixGraph::IsSubmixDisplayed(USoundSubmix* SoundSubmix) const
{
	return FindExistingNode(SoundSubmix) != nullptr;
}

void USoundSubmixGraph::LinkSoundSubmixes()
{
	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
	{
		USoundSubmixGraphNode* Node = CastChecked<USoundSubmixGraphNode>(Nodes[NodeIndex]);

		if (!Node->CheckRepresentsSoundSubmix())
		{
			Node->SoundSubmix->Modify();

			// remove parents of existing children
			for (int32 ChildIndex = 0; ChildIndex < Node->SoundSubmix->ChildSubmixes.Num(); ChildIndex++)
			{
				USoundSubmix* ChildSubmix = Node->SoundSubmix->ChildSubmixes[ChildIndex];

				if (ChildSubmix)
				{
					ChildSubmix->Modify();
					ChildSubmix->ParentSubmix = nullptr;
				}
			}

			Node->SoundSubmix->ChildSubmixes.Empty();

			UEdGraphPin* ChildPin = Node->GetChildPin();

			for (int32 ChildIndex = 0; ChildIndex < ChildPin->LinkedTo.Num(); ChildIndex++)
			{
				USoundSubmixGraphNode* ChildNode = CastChecked<USoundSubmixGraphNode>(ChildPin->LinkedTo[ChildIndex]->GetOwningNode());
				Node->SoundSubmix->ChildSubmixes.Add(ChildNode->SoundSubmix);
				ChildNode->SoundSubmix->SetParentSubmix(Node->SoundSubmix);
			}

			Node->SoundSubmix->PostEditChange();
			Node->SoundSubmix->MarkPackageDirty();
		}
	}

	RootSoundSubmix->RefreshAllGraphs(true);
}

void USoundSubmixGraph::RefreshGraphLinks()
{
	Modify();

	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); NodeIndex++)
	{
		USoundSubmixGraphNode* Node = CastChecked<USoundSubmixGraphNode>(Nodes[NodeIndex]);

		if (!Node->CheckRepresentsSoundSubmix())
		{
			UEdGraphPin* ChildPin = Node->GetChildPin();

			Node->Modify();

			ChildPin->BreakAllPinLinks();

			if (Node->SoundSubmix)
			{
				for (int32 ChildIndex = 0; ChildIndex < Node->SoundSubmix->ChildSubmixes.Num(); ChildIndex++)
				{
					USoundSubmix* ChildSubmix = Node->SoundSubmix->ChildSubmixes[ChildIndex];

					if (ChildSubmix)
					{
						USoundSubmixGraphNode* ChildNode = FindExistingNode(ChildSubmix);

						if (!ChildNode)
						{
							// New Child not yet represented on graph
							ConstructNodes(ChildSubmix, Node->NodePosX + 400, Node->NodePosY);
							ChildNode = FindExistingNode(ChildSubmix);
						}

						ChildPin->MakeLinkTo(ChildNode->GetParentPin());
					}
				}
			}

			Node->PostEditChange();
		}
	}

	NotifyGraphChanged();
}

void USoundSubmixGraph::RecursivelyRemoveNodes(const TSet<class UObject*> NodesToRemove)
{
	Modify();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(NodesToRemove); NodeIt; ++NodeIt)
	{
		USoundSubmixGraphNode* Node = Cast<USoundSubmixGraphNode>(*NodeIt);

		if (Node && Node->CanUserDeleteNode())
		{
			RecursivelyRemoveNode(Node);
		}
	}

	LinkSoundSubmixes();
}

int32 USoundSubmixGraph::ConstructNodes(class USoundSubmix* SoundSubmix, int32 NodePosX, int32 NodePosY, bool bSelectNewNode/* = true*/)
{
	check(SoundSubmix);

	TMap<USoundSubmix*, int32> ChildCounts;

	RecursivelyGatherChildCounts(SoundSubmix, ChildCounts);

	USoundSubmixGraphNode* GraphNode = CreateNode(SoundSubmix, NodePosX, NodePosY, bSelectNewNode);

	return RecursivelyConstructChildNodes(GraphNode, ChildCounts);
}

int32 USoundSubmixGraph::RecursivelyGatherChildCounts(USoundSubmix* ParentSubmix, TMap<USoundSubmix*, int32>& OutChildCounts)
{
	int32 ChildSize = 0;

	for (int32 ChildIndex = 0; ChildIndex < ParentSubmix->ChildSubmixes.Num(); ChildIndex++)
	{
		if (ParentSubmix->ChildSubmixes[ChildIndex])
		{
			ChildSize += RecursivelyGatherChildCounts(ParentSubmix->ChildSubmixes[ChildIndex], OutChildCounts);
		}
	}

	if (ChildSize == 0)
	{
		ChildSize = 1;
	}

	OutChildCounts.Add(ParentSubmix, ChildSize);
	return ChildSize;
}

int32 USoundSubmixGraph::RecursivelyConstructChildNodes(USoundSubmixGraphNode* ParentNode, const TMap<USoundSubmix*, int32>& InChildCounts, bool bSelectNewNode/* = true*/)
{
	const int32 HorizontalSpacing = 400;
	const int32 VerticalSpacing = 100;

	USoundSubmix* ParentSubmix = ParentNode->SoundSubmix;
	int32 TotalChildSizeY = InChildCounts.FindChecked(ParentSubmix) * VerticalSpacing;
	int32 NodeStartY = ParentNode->NodePosY - (TotalChildSizeY * 0.5f) + (VerticalSpacing * 0.5f);
	int32 NodePosX = ParentNode->NodePosX + HorizontalSpacing;

	for (int32 ChildIndex = 0; ChildIndex < ParentSubmix->ChildSubmixes.Num(); ChildIndex++)
	{
		if (ParentSubmix->ChildSubmixes[ChildIndex])
		{
			const int32 ChildCount = InChildCounts.FindChecked(ParentSubmix->ChildSubmixes[ChildIndex]);
			int32 NodePosY = NodeStartY + (ChildCount * VerticalSpacing * 0.5f) - (VerticalSpacing * 0.5f);
			USoundSubmixGraphNode* ChildNode = CreateNode(ParentSubmix->ChildSubmixes[ChildIndex], NodePosX, NodePosY, bSelectNewNode);
			ParentNode->GetChildPin()->MakeLinkTo(ChildNode->GetParentPin());
			RecursivelyConstructChildNodes(ChildNode, InChildCounts);
			NodeStartY += ChildCount * VerticalSpacing;
		}
	}

	return TotalChildSizeY;
}

void USoundSubmixGraph::RecursivelyRemoveNode(class USoundSubmixGraphNode* ParentNode)
{
	UEdGraphPin* ChildPin = ParentNode->GetChildPin();

	for (int32 ChildIndex = ChildPin->LinkedTo.Num() - 1; ChildIndex >= 0; ChildIndex--)
	{
		USoundSubmixGraphNode* ChildNode = CastChecked<USoundSubmixGraphNode>(ChildPin->LinkedTo[ChildIndex]->GetOwningNode());
		RecursivelyRemoveNode(ChildNode);
	}

	ParentNode->Modify();
	RemoveNode(ParentNode);
}

void USoundSubmixGraph::RemoveAllNodes()
{
	TArray<UEdGraphNode*> NodesToRemove = Nodes;
	for (int32 NodeIndex = 0; NodeIndex < NodesToRemove.Num(); ++NodeIndex)
	{
		NodesToRemove[NodeIndex]->Modify();
		RemoveNode(NodesToRemove[NodeIndex]);
	}
}

USoundSubmixGraphNode* USoundSubmixGraph::CreateNode(USoundSubmix* SoundSubmix, int32 NodePosX, int32 NodePosY, bool bSelectNewNode/* = true*/)
{
	USoundSubmixGraphNode* GraphNode = FindExistingNode(SoundSubmix);

	if (!GraphNode)
	{
		FGraphNodeCreator<USoundSubmixGraphNode> NodeCreator(*this);
		GraphNode = NodeCreator.CreateNode(bSelectNewNode);
		GraphNode->SoundSubmix = SoundSubmix;
		GraphNode->NodePosX = NodePosX;
		GraphNode->NodePosY = NodePosY;
		NodeCreator.Finalize();
	}
	return GraphNode;
}

USoundSubmixGraphNode* USoundSubmixGraph::FindExistingNode(USoundSubmix* SoundSubmix) const
{
	USoundSubmixGraphNode* ExistingNode = NULL;

	for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		USoundSubmixGraphNode* Node = CastChecked<USoundSubmixGraphNode>(Nodes[NodeIndex]);
		if (Node->SoundSubmix == SoundSubmix)
		{
			ExistingNode = Node;
			break;
		}
	}

	return ExistingNode;
}
