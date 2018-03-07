// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/KeyHandle.h"
#include "Curves/IKeyFrameManipulator.h"
#include "Curves/KeyFrameManipulator.h"

namespace KeyFrameAlgorithms
{
	template<typename TimeType>
	void Scale(TKeyFrameManipulator<TimeType>& Manipulator, TimeType ScaleOrigin, TimeType ScaleFactor)
	{
		TKeyTimeIterator<TimeType> Iterator = Manipulator.IterateKeys();

		TArray<FKeyHandle> AllKeyHandles;
		AllKeyHandles.Reserve(Iterator.GetEndIndex() - Iterator.GetStartIndex());

		for (auto It = begin(Iterator); It != end(Iterator); ++It)
		{
			AllKeyHandles.Add(It.GetKeyHandle());
		}

		Scale(Manipulator, ScaleOrigin, ScaleFactor, AllKeyHandles);
	}

	template<typename TimeType, typename KeyHandleContainer>
	void Scale(TKeyFrameManipulator<TimeType>& Manipulator, TimeType ScaleOrigin, TimeType ScaleFactor, const KeyHandleContainer& KeyHandles)
	{
		// @todo: we could be cleverer here by iterating forwards and backards from the scale origin time to prevent too much shuffling in the internal key array
		for (FKeyHandle KeyHandle : KeyHandles)
		{
			TOptional<TimeType> Time = Manipulator.GetKeyTime(KeyHandle);
			if (!Time.IsSet())
			{
				continue;
			}
			Manipulator.SetKeyTime(KeyHandle, (Time.GetValue() - ScaleOrigin) * ScaleFactor + ScaleOrigin);
		}
	}

	template<typename TimeType>
	void Translate(TKeyFrameManipulator<TimeType>& Manipulator, TimeType Delta)
	{
		TKeyTimeIterator<TimeType> Iterator = Manipulator.IterateKeys();

		TSet<FKeyHandle> AllKeyHandles;
		for (auto It = begin(Iterator); It != end(Iterator); ++It)
		{
			AllKeyHandles.Add(It.GetKeyHandle());
		}

		Translate(Manipulator, Delta, AllKeyHandles);
	}

	template<typename TimeType, typename KeyHandleContainer>
	void Translate(TKeyFrameManipulator<TimeType>& Manipulator, TimeType Delta, const KeyHandleContainer& KeyHandles)
	{
		for (FKeyHandle KeyHandle : KeyHandles)
		{
			TOptional<TimeType> Time = Manipulator.GetKeyTime(KeyHandle);
			if (!Time.IsSet())
			{
				continue;
			}
			Manipulator.SetKeyTime(KeyHandle, Time.GetValue() + Delta);
		}
	}

}
