// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	class FParam
	{
	public:
		FParam()
			: CurrentValue(0.0f)
			, StartingValue(0.0f)
			, TargetValue(0.0f)
			, DeltaValue(0.0f)
			, bIsInit(true)
		{}

		// Set the parameter value to the given target value over the given interpolation frames.
		FORCEINLINE void SetValue(const float InValue, const int32 InNumInterpFrames = 0)
		{
			TargetValue = InValue;
			if (bIsInit || InNumInterpFrames == 0)
			{
				bIsInit = false;
				StartingValue = TargetValue;
				CurrentValue = TargetValue;
				DeltaValue = 0.0f;
			}
			else
			{
				DeltaValue = (InValue - CurrentValue) / InNumInterpFrames;
				StartingValue = CurrentValue;
			}
		}

		FORCEINLINE void Init()
		{
			bIsInit = true;
		}

		// Resets the delta value back to 0.0. To be called at beginning of callback render.
		FORCEINLINE void Reset()
		{
			DeltaValue = 0.0f;
			CurrentValue = TargetValue;
		}

		// Updates the parameter, assumes called in one of the frames.
		FORCEINLINE float Update()
		{
			CurrentValue += DeltaValue;
			return CurrentValue;
		}

		// Returns the current value, but does not update the value
		FORCEINLINE float GetValue() const
		{
			return CurrentValue;
		}

	private:
		float CurrentValue;
		float StartingValue;
		float TargetValue;
		float DeltaValue;
		bool bIsInit;
	};

}
