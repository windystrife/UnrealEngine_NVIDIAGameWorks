// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundNode.h"
#include "EngineUtils.h"
#include "Sound/SoundCue.h"

/*-----------------------------------------------------------------------------
	USoundNode implementation.
-----------------------------------------------------------------------------*/
USoundNode::USoundNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void USoundNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
	{
		FStripDataFlags StripFlags(Ar);
#if WITH_EDITORONLY_DATA
		if (!StripFlags.IsEditorDataStripped())
		{
			Ar << GraphNode;
		}
#endif
	}
#if WITH_EDITOR
	else
	{
		Ar << GraphNode;
	}
#endif
}

#if WITH_EDITOR
void USoundNode::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USoundNode* This = CastChecked<USoundNode>(InThis);

	Collector.AddReferencedObject(This->GraphNode, This);

	Super::AddReferencedObjects(InThis, Collector);
}
#endif //WITH_EDITOR

#if WITH_EDITORONLY_DATA
UEdGraphNode* USoundNode::GetGraphNode() const
{
	return GraphNode;
}
#endif

UPTRINT USoundNode::GetNodeWaveInstanceHash(const UPTRINT ParentWaveInstanceHash, const USoundNode* ChildNode, const uint32 ChildIndex)
{
	checkf(ChildIndex < MAX_ALLOWED_CHILD_NODES, TEXT("Too many children (%d) in SoundCue '%s'"), ChildIndex, *CastChecked<USoundCue>(ChildNode->GetOuter())->GetFullName());
	return ((ParentWaveInstanceHash << ChildIndex) ^ (UPTRINT)ChildNode);
}

UPTRINT USoundNode::GetNodeWaveInstanceHash(const UPTRINT ParentWaveInstanceHash, const UPTRINT ChildNodeHash, const uint32 ChildIndex)
{
	checkf(ChildIndex < MAX_ALLOWED_CHILD_NODES, TEXT("Too many children (%d) in SoundCue"), ChildIndex);
	return ((ParentWaveInstanceHash << ChildIndex) ^ ChildNodeHash);
}

void USoundNode::ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	for( int32 i = 0; i < ChildNodes.Num() && i < GetMaxChildNodes(); ++i )
	{
		if( ChildNodes[ i ] )
		{
			ChildNodes[ i ]->ParseNodes( AudioDevice, GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNodes[i], i), ActiveSound, ParseParams, WaveInstances );
		}
	}
}

void USoundNode::GetAllNodes( TArray<USoundNode*>& SoundNodes )
{
	SoundNodes.Add( this );
	for( int32 i = 0; i < ChildNodes.Num(); ++i )
	{
		if( ChildNodes[ i ] )
		{
			ChildNodes[ i ]->GetAllNodes( SoundNodes );
		}
	}
}

void USoundNode::CreateStartingConnectors()
{
	int32 ConnectorsToMake = FMath::Max(1, GetMinChildNodes());
	while (ConnectorsToMake > 0)
	{
		InsertChildNode( ChildNodes.Num() );
		--ConnectorsToMake;
	}
}

void USoundNode::InsertChildNode( int32 Index )
{
	check( Index >= 0 && Index <= ChildNodes.Num() );
	int32 MaxChildNodes = GetMaxChildNodes();
	if (MaxChildNodes > ChildNodes.Num())
	{
		ChildNodes.InsertZeroed( Index );
#if WITH_EDITOR
		USoundCue::GetSoundCueAudioEditor()->CreateInputPin(GetGraphNode());
#endif //WITH_EDITORONLY_DATA
	}
}

void USoundNode::RemoveChildNode( int32 Index )
{
	check( Index >= 0 && Index < ChildNodes.Num() );
	int32 MinChildNodes = GetMinChildNodes();
	if (ChildNodes.Num() > MinChildNodes )
	{
		ChildNodes.RemoveAt( Index );
	}
}

#if WITH_EDITOR
void USoundNode::SetChildNodes(TArray<USoundNode*>& InChildNodes)
{
	int32 MaxChildNodes = GetMaxChildNodes();
	int32 MinChildNodes = GetMinChildNodes();
	if (MaxChildNodes >= InChildNodes.Num() && InChildNodes.Num() >= MinChildNodes)
	{
		ChildNodes = InChildNodes;
	}
}
#endif //WITH_EDITOR

float USoundNode::GetDuration()
{
	// Iterate over children and return maximum length of any of them
	float MaxDuration = 0.0f;
	for( int32 i = 0; i < ChildNodes.Num(); i++ )
	{
		if( ChildNodes[ i ] )
		{
			MaxDuration = FMath::Max( ChildNodes[ i ]->GetDuration(), MaxDuration );
		}
	}
	return MaxDuration;
}

int32 USoundNode::GetNumSounds(const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound) const
{
	// Default implementation loops through all child nodes and sums the number of sounds.
	// For most nodes this will result in 1, for node mixers, this will result in multiple sounds.
	int32 NumSounds = 0;
	for (int32 i = 0; i < ChildNodes.Num(); ++i)
	{
		if (ChildNodes[i])
		{
			const UPTRINT ChildNodeWaveInstanceHash = GetNodeWaveInstanceHash(NodeWaveInstanceHash, ChildNodes[i], i);
			NumSounds += ChildNodes[i]->GetNumSounds(ChildNodeWaveInstanceHash, ActiveSound);
		}
	}

	return NumSounds;
}

#if WITH_EDITOR
void USoundNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MarkPackageDirty();
}

void USoundNode::PostLoad()
{
	Super::PostLoad();
	// Make sure sound nodes are transactional (so they work with undo system)
	SetFlags(RF_Transactional);
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void USoundNode::PlaceNode( int32 NodeColumn, int32 NodeRow, int32 RowCount )
{
	GraphNode->NodePosX = (-150 * NodeColumn) - 100;
	GraphNode->NodePosY = (100 * NodeRow) - (50 * RowCount);
}

#endif //WITH_EDITOR
