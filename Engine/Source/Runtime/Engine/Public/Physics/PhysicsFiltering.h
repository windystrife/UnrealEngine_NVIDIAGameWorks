// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"

/** 
 * Set of flags stored in the PhysX FilterData
 *
 * When this flag is saved in CreateShapeFilterData or CreateQueryFilterData, we only use 23 bits
 * If you plan to use more than 23 bits, you'll also need to change the format of ShapeFilterData,QueryFilterData
 * Make sure you also change preFilter/SimFilterShader where it's used
 */
enum EPhysXFilterDataFlags
{

	EPDF_SimpleCollision	=	0x0001,
	EPDF_ComplexCollision	=	0x0002,
	EPDF_CCD				=	0x0004,
	EPDF_ContactNotify		=	0x0008,
	EPDF_StaticShape		=	0x0010,
	EPDF_ModifyContacts		=   0x0020,
	EPDF_KinematicKinematicPairs = 0x0040,
};


// Bit counts for Word3 of filter data.
// (ExtraFilter (top NumExtraFilterBits) + MyChannel (next NumCollisionChannelBits) as ECollisionChannel + Flags (remaining NumFilterDataFlagBits)
// [NumExtraFilterBits] [NumCollisionChannelBits] [NumFilterDataFlagBits] = 32 bits
enum { NumCollisionChannelBits = 5 };
enum { NumFilterDataFlagBits = 32 - NumExtraFilterBits - NumCollisionChannelBits };


struct FPhysicsFilterBuilder
{
	ENGINE_API FPhysicsFilterBuilder(TEnumAsByte<enum ECollisionChannel> InObjectType, FMaskFilter MaskFilter, const struct FCollisionResponseContainer& ResponseToChannels);

	inline void ConditionalSetFlags(EPhysXFilterDataFlags Flag, bool bEnabled)
	{
		if (bEnabled)
		{
			Word3 |= Flag;
		}
	}

	inline void GetQueryData(uint32 ActorID, uint32& OutWord0, uint32& OutWord1, uint32& OutWord2, uint32& OutWord3) const
	{
		/**
		 * Format for QueryData : 
		 *		word0 (object ID)
		 *		word1 (blocking channels)
		 *		word2 (touching channels)
		 *		word3 (ExtraFilter (top NumExtraFilterBits) + MyChannel (next NumCollisionChannelBits) as ECollisionChannel + Flags (remaining NumFilterDataFlagBits)
		 */
		OutWord0 = ActorID;
		OutWord1 = BlockingBits;
		OutWord2 = TouchingBits;
		OutWord3 = Word3;
	}

	inline void GetSimData(uint32 BodyIndex, uint32 ComponentID, uint32& OutWord0, uint32& OutWord1, uint32& OutWord2, uint32& OutWord3) const
	{
		/**
		 * Format for SimData : 
		 * 		word0 (body index)
		 *		word1 (blocking channels)
		 *		word2 (skeletal mesh component ID)
		 *		word3 (ExtraFilter (top NumExtraFilterBits) + MyChannel (next NumCollisionChannelBits) as ECollisionChannel + Flags (remaining NumFilterDataFlagBits)
		 */
		OutWord0 = BodyIndex;
		OutWord1 = BlockingBits;
		OutWord2 = ComponentID;
		OutWord3 = Word3;
	}

	inline void GetCombinedData(uint32& OutBlockingBits, uint32& OutTouchingBits, uint32& OutObjectTypeAndFlags) const
	{
		OutBlockingBits = BlockingBits;
		OutTouchingBits = TouchingBits;
		OutObjectTypeAndFlags = Word3;
	}

private:
	uint32 BlockingBits;
	uint32 TouchingBits;
	uint32 Word3;
};

/** Utility for creating a PhysX PxFilterData for filtering query (trace) and sim (physics) from the Unreal filtering info. */
#if WITH_PHYSX
inline void CreateShapeFilterData(
	const uint8 MyChannel,
	const FMaskFilter MaskFilter,
	const int32 ActorID,
	const FCollisionResponseContainer& ResponseToChannels,
	uint32 ComponentID,
	uint16 BodyIndex,
	PxFilterData& OutQueryData,
	PxFilterData& OutSimData,
	bool bEnableCCD,
	bool bEnableContactNotify,
	bool bStaticShape,
	bool bModifyContacts = false)
{
	FPhysicsFilterBuilder Builder((ECollisionChannel)MyChannel, MaskFilter, ResponseToChannels);
	Builder.ConditionalSetFlags(EPDF_CCD, bEnableCCD);
	Builder.ConditionalSetFlags(EPDF_ContactNotify, bEnableContactNotify);
	Builder.ConditionalSetFlags(EPDF_StaticShape, bStaticShape);
	Builder.ConditionalSetFlags(EPDF_ModifyContacts, bModifyContacts);

	OutQueryData.setToDefault();
	OutSimData.setToDefault();
	Builder.GetQueryData(ActorID, OutQueryData.word0, OutQueryData.word1, OutQueryData.word2, OutQueryData.word3);
	Builder.GetSimData(BodyIndex, ComponentID, OutSimData.word0, OutSimData.word1, OutSimData.word2, OutSimData.word3);
}
#endif //WITH_PHYSX

inline ECollisionChannel GetCollisionChannel(uint32 Word3)
{
	uint32 ChannelMask = (Word3 << NumExtraFilterBits) >> (32 - NumCollisionChannelBits);
	return (ECollisionChannel)ChannelMask;
}

inline ECollisionChannel GetCollisionChannelAndExtraFilter(uint32 Word3, FMaskFilter& OutMaskFilter)
{
	uint32 ChannelMask = GetCollisionChannel(Word3);
	OutMaskFilter = Word3 >> (32 - NumExtraFilterBits);
	return (ECollisionChannel)ChannelMask;
}

inline uint32 CreateChannelAndFilter(ECollisionChannel CollisionChannel, FMaskFilter MaskFilter)
{
	uint32 ResultMask = (uint32(MaskFilter) << NumCollisionChannelBits) | (uint32)CollisionChannel;
	return ResultMask << NumFilterDataFlagBits;
}

inline void UpdateMaskFilter(uint32& Word3, FMaskFilter NewMaskFilter)
{
	static_assert(NumExtraFilterBits <= 8, "Only up to 8 extra filter bits are supported.");
	Word3 &= (0xFFFFFFFFu >> NumExtraFilterBits);	//we drop the top NumExtraFilterBits bits because that's where the new mask filter is going
	Word3 |= uint32(NewMaskFilter) << (32 - NumExtraFilterBits);
}

#if WITH_PHYSX
	extern ENGINE_API PxSimulationFilterShader GSimulationFilterShader;
#endif

/** Utility for creating a PhysX PxFilterData for performing a query (trace) against the scene */
// PxFilterData ZZ_CreateQueryFilterData(const uint8 MyChannel, const bool bTraceComplex,
// 	const FCollisionResponseContainer& InCollisionResponseContainer,
// 	const struct FCollisionObjectQueryParams & ObjectParam,
// 	const bool bMultitrace);
