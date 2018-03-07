// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"

struct FKeyEvent;
enum class EUINavigation : uint8;

/**
 * This class is used to control which FKeys should move focus.  A new navigation
 * config has to be created per user.
 */
class SLATE_API FNavigationConfig : public TSharedFromThis<FNavigationConfig>
{
public:
	FNavigationConfig();

	virtual ~FNavigationConfig();

	virtual EUINavigation GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const;

	EUINavigation GetNavigationDirectionFromAnalog(const FAnalogInputEvent& InAnalogEvent);

	virtual float GetRepeatRateForPressure(float InPressure, int32 InRepeats) const;

public:
	bool bTabNavigation;
	bool bKeyNavigation;
	bool bAnalogNavigation;
	float AnalogNavigationThreshold;

	/**  */
	TMap<FKey, EUINavigation> KeyEventRules;

protected:
	virtual EUINavigation GetNavigationDirectionFromAnalogInternal(const FAnalogInputEvent& InAnalogEvent);

	struct FAnalogNavigationState
	{
		double LastNavigationTime;
		int32 Repeats;

		FAnalogNavigationState()
			: LastNavigationTime(0)
			, Repeats(0)
		{
		}
	};

	TMap<EUINavigation, FAnalogNavigationState> AnalogNavigationState;
};
