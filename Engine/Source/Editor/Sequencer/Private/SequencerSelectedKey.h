// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"

class IKeyArea;
class UMovieSceneSection;

/**
 * Represents a selected key in the sequencer
 */
struct FSequencerSelectedKey
{	
	/** Section that the key belongs to */
	UMovieSceneSection* Section;

	/** Key area providing the key */
	TSharedPtr<IKeyArea> KeyArea;

	/** Index of the key in the key area */
	TOptional<FKeyHandle> KeyHandle;

public:

	/** Create and initialize a new instance. */
	FSequencerSelectedKey(UMovieSceneSection& InSection, TSharedPtr<IKeyArea> InKeyArea, FKeyHandle InKeyHandle)
		: Section(&InSection)
		, KeyArea(InKeyArea)
		, KeyHandle(InKeyHandle)
	{}

	/** Default constructor. */
	FSequencerSelectedKey()
		: Section(nullptr)
		, KeyArea(nullptr)
		, KeyHandle()
	{}

	/** @return Whether or not this is a valid selected key */
	bool IsValid() const { return Section != nullptr && KeyArea.IsValid() && KeyHandle.IsSet(); }

	friend uint32 GetTypeHash(const FSequencerSelectedKey& SelectedKey)
	{
		return GetTypeHash(SelectedKey.Section) ^ GetTypeHash(SelectedKey.KeyArea) ^ 
			(SelectedKey.KeyHandle.IsSet() ? GetTypeHash(SelectedKey.KeyHandle.GetValue()) : 0);
	} 

	bool operator==(const FSequencerSelectedKey& OtherKey) const
	{
		return Section == OtherKey.Section && KeyArea == OtherKey.KeyArea &&
			KeyHandle.IsSet() && OtherKey.KeyHandle.IsSet() &&
			KeyHandle.GetValue() == OtherKey.KeyHandle.GetValue();
	}
};
