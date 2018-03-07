// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRender.h: Definitions and inline code for rendering SkeletalMeshComponet
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SkeletalMeshTypes.h"

class USkinnedMeshComponent;

// smallest blend weight for vertex anims
extern const float MinMorphTargetBlendWeight;
// largest blend weight for vertex anims
extern const float MaxMorphTargetBlendWeight;

/** 
* Stores the data for updating instanced weights
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicUpdateVertexInfluencesData
{
public:
	/**
	* Constructor
	*/
	FDynamicUpdateVertexInfluencesData(
		int32 InLODIdx,
		const TArray<FBoneIndexPair>& InBonePairs,
		bool InbResetInfluences )
		:	LODIdx(InLODIdx)
		,	BonePairs(InBonePairs)
		,	bResetInfluences(InbResetInfluences)
	{
	}

	/** LOD this update is for */
	int32 LODIdx;
	/** set of bone pairs used to find vertices that need to have their weights updated */
	TArray<FBoneIndexPair> BonePairs;
	/** resets the array of instanced weights/bones using the ones from the base mesh defaults before udpating */
	bool bResetInfluences;
};

/**
* Utility function that fills in the array of ref-pose to local-space matrices using 
* the mesh component's updated space bases
* @param	ReferenceToLocal - matrices to update
* @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
* @param	SkeletalMeshResource - resource for which to compute RefToLocal matrices
* @param	LODIndex - each LOD has its own mapping of bones to update
* @param	ExtraRequiredBoneIndices - any extra bones apart from those active in the LOD that we'd like to update
*/
ENGINE_API void UpdateRefToLocalMatrices( TArray<FMatrix>& ReferenceToLocal, const USkinnedMeshComponent* InMeshComponent, const FSkeletalMeshResource* InSkeletalMeshResource, int32 LODIndex, const TArray<FBoneIndexType>* ExtraRequiredBoneIndices=NULL );

/**
 * Utility function that calculates the local-space origin and bone direction vectors for the
 * current pose for any TRISORT_CustomLeftRight sections.
 * @param	OutCustomLeftRightVectors - origin and direction vectors to update
 * @param	SkeletalMeshComponent - mesh primitive with updated bone matrices
 * @param	LODIndex - current LOD
 */
void UpdateCustomLeftRightVectors( TArray<FTwoVectors>& OutCustomLeftRightVectors, const USkinnedMeshComponent* InMeshComponent, const FSkeletalMeshResource* InSkeletalMeshResource, int32 LODIndex );

extern ENGINE_API const VectorRegister		VECTOR_PACK_127_5;
extern ENGINE_API const VectorRegister		VECTOR4_PACK_127_5;

extern ENGINE_API const VectorRegister		VECTOR_INV_127_5;
extern ENGINE_API const VectorRegister		VECTOR4_INV_127_5;

extern ENGINE_API const VectorRegister		VECTOR_UNPACK_MINUS_1;
extern ENGINE_API const VectorRegister		VECTOR4_UNPACK_MINUS_1;

extern ENGINE_API const VectorRegister		VECTOR_0001;

/**
* Apply scale/bias to packed normal byte values and store result in register
* Only first 3 components are copied. W component is always 0
* 
* @param PackedNormal - source vector packed with byte components
* @return vector register with unpacked float values
*/
static FORCEINLINE VectorRegister Unpack3( const uint32 *PackedNormal )
{
	return VectorMultiplyAdd( VectorLoadByte4(PackedNormal), VECTOR_INV_127_5, VECTOR_UNPACK_MINUS_1 );
}

/**
* Apply scale/bias to float register values and store results in memory as packed byte values 
* Only first 3 components are copied. W component is always 0
* 
* @param Normal - source vector register with floats
* @param PackedNormal - destination vector packed with byte components
*/
static FORCEINLINE void Pack3( VectorRegister Normal, uint32 *PackedNormal )
{
	Normal = VectorMultiplyAdd(Normal, VECTOR_PACK_127_5, VECTOR_PACK_127_5);
	VectorStoreByte4( Normal, PackedNormal );
}

/**
* Apply scale/bias to packed normal byte values and store result in register
* All 4 components are copied. 
* 
* @param PackedNormal - source vector packed with byte components
* @return vector register with unpacked float values
*/
static FORCEINLINE VectorRegister Unpack4( const uint32 *PackedNormal )
{
	return VectorMultiplyAdd( VectorLoadByte4(PackedNormal), VECTOR4_INV_127_5, VECTOR4_UNPACK_MINUS_1 );
}

/**
* Apply scale/bias to float register values and store results in memory as packed byte values 
* All 4 components are copied. 
* 
* @param Normal - source vector register with floats
* @param PackedNormal - destination vector packed with byte components
*/
static FORCEINLINE void Pack4( VectorRegister Normal, uint32 *PackedNormal )
{
	Normal = VectorMultiplyAdd(Normal, VECTOR4_PACK_127_5, VECTOR4_PACK_127_5);
	VectorStoreByte4( Normal, PackedNormal );
}

