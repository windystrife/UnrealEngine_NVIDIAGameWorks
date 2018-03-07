// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ImmediatePhysics
{

/** Contact pair generated between entities */
struct FContactPair
{
	/** Index of the dynamic actor that we generated the contact pair for*/
	int32 DynamicActorDataIndex;

	/** Index of the other actor that we generated the contact pair for. This could be either dynamic or static */
	uint32 OtherActorDataIndex;

	/** Index into the first contact point associated with this pair*/
	uint32 StartContactIndex;

	/** Number of contacts associated with this pair. */
	uint32 NumContacts;

	/** Identifies the pair index from the original contact generation test */
	int32 PairIdx;
};


}