// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsProjectSettings.h"


UMovieSceneToolsProjectSettings::UMovieSceneToolsProjectSettings()
	: DefaultStartTime(0.f)
	, DefaultDuration(5.f)
	, ShotDirectory(TEXT("shots"))
	, ShotPrefix(TEXT("shot"))
	, FirstShotNumber(10)
	, ShotIncrement(10)
	, ShotNumDigits(4)
	, TakeNumDigits(3)
	, FirstTakeNumber(1)
	, TakeSeparator(TEXT("_"))
	, SubSequenceSeparator(TEXT("_"))
{ }
