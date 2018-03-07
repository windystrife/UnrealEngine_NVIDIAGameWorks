// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "SoundNode.generated.h"

class FAudioDevice;
class UEdGraphNode;
struct FActiveSound;
struct FPropertyChangedEvent;
struct FWaveInstance;

/*-----------------------------------------------------------------------------
	USoundNode helper macros. 
-----------------------------------------------------------------------------*/

struct FActiveSound;

#define DECLARE_SOUNDNODE_ELEMENT(Type,Name)													\
	Type& Name = *((Type*)(Payload));															\
	Payload += sizeof(Type);														

#define DECLARE_SOUNDNODE_ELEMENT_PTR(Type,Name)												\
	Type* Name = (Type*)(Payload);																\
	Payload += sizeof(Type);														

#define	RETRIEVE_SOUNDNODE_PAYLOAD( Size )														\
		uint8*	Payload					= NULL;													\
		uint32*	RequiresInitialization	= NULL;													\
		{																						\
			uint32* TempOffset = ActiveSound.SoundNodeOffsetMap.Find(NodeWaveInstanceHash);		\
			uint32 Offset;																		\
			if( !TempOffset )																	\
			{																					\
				Offset = ActiveSound.SoundNodeData.AddZeroed( Size + sizeof(uint32));				\
				ActiveSound.SoundNodeOffsetMap.Add( NodeWaveInstanceHash, Offset );				\
				RequiresInitialization = (uint32*) &ActiveSound.SoundNodeData[Offset];			\
				*RequiresInitialization = 1;													\
				Offset += sizeof(uint32);															\
			}																					\
			else																				\
			{																					\
				RequiresInitialization = (uint32*) &ActiveSound.SoundNodeData[*TempOffset];		\
				Offset = *TempOffset + sizeof(uint32);											\
			}																					\
			Payload = &ActiveSound.SoundNodeData[Offset];										\
		}

UCLASS(abstract, hidecategories=Object, editinlinenew)
class ENGINE_API USoundNode : public UObject
{
	GENERATED_UCLASS_BODY()

	static const int32 MAX_ALLOWED_CHILD_NODES = 32;

	UPROPERTY()
	TArray<class USoundNode*> ChildNodes;

#if WITH_EDITORONLY_DATA
	/** Node's Graph representation, used to get position. */
	UPROPERTY()
	UEdGraphNode*	GraphNode;

	class UEdGraphNode* GetGraphNode() const;
#endif

public:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif //WITH_EDITOR
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//
	//~ Begin USoundNode Interface. 
	//

	/**
	 * Notifies the sound node that a wave instance in its subtree has finished.
	 *
	 * @param WaveInstance	WaveInstance that was finished 
	 */
	virtual bool NotifyWaveInstanceFinished( struct FWaveInstance* WaveInstance )
	{
		return( false );
	}

	/** 
	 * Returns the maximum distance this sound can be heard from.
	 *
	 * @param	CurrentMaxDistance	The max audible distance of all parent nodes
	 * @return	float of the greater of this node's max audible distance and its parent node's max audible distance
	 */
	virtual float MaxAudibleDistance( float CurrentMaxDistance ) 
	{ 
		return( CurrentMaxDistance ); 
	}

	/** 
	 * Returns the maximum duration this sound node will play for. 
	 *
	 * @return	float of number of seconds this sound will play for. INDEFINITELY_LOOPING_DURATION means forever.
	 */
	virtual float GetDuration( void );

	virtual void ParseNodes( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const struct FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances );

	/**
	 * Returns an array of all (not just active) nodes.
	 */
	virtual void GetAllNodes( TArray<USoundNode*>& SoundNodes ); 

	/**
	 * Returns the maximum number of child nodes this node can possibly have
	 */
	virtual int32 GetMaxChildNodes() const
	{ 
		return 1 ; 
	}

	/** Returns the minimum number of child nodes this node must have */
	virtual int32 GetMinChildNodes() const
	{ 
		return 0;
	}

	/** Returns the number of simultaneous sounds this node instance plays back. */
	virtual int32 GetNumSounds(const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound) const;


	/** 
	 * Editor interface. 
	 */

	/**
	 * Called by the Sound Cue Editor for nodes which allow children.  The default behaviour is to
	 * attach a single connector. Dervied classes can override to eg add multiple connectors.
	 */
	virtual void CreateStartingConnectors( void );
	virtual void InsertChildNode( int32 Index );
	virtual void RemoveChildNode( int32 Index );
#if WITH_EDITOR
	/**
	 * Set the entire Child Node array directly, allows GraphNodes to fully control node layout.
	 * Can be overwritten to set up additional parameters that are tied to children.
	 */
	virtual void SetChildNodes(TArray<USoundNode*>& InChildNodes);

	/** Get the name of a specific input pin */
	virtual FText GetInputPinName(int32 PinIndex) const { return FText::GetEmpty(); }

	virtual FText GetTitle() const { return GetClass()->GetDisplayNameText(); }

	/** Helper function to set the position of a sound node on a grid */
	void PlaceNode(int32 NodeColumn, int32 NodeRow, int32 RowCount );

	/** Called as PIE begins */
	virtual void OnBeginPIE(const bool bIsSimulating) {};

	/** Called as PIE ends */
	virtual void OnEndPIE(const bool bIsSimulating) {};
#endif //WITH_EDITOR

	/** 
	 * Used to create a unique string to identify unique nodes
	 */
	static UPTRINT GetNodeWaveInstanceHash(const UPTRINT ParentWaveInstanceHash, const USoundNode* ChildNode, const uint32 ChildIndex);
	static UPTRINT GetNodeWaveInstanceHash(const UPTRINT ParentWaveInstanceHash, const UPTRINT ChildNodeHash, const uint32 ChildIndex);
};

