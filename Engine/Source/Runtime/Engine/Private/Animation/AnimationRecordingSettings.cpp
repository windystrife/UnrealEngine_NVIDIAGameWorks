// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimationRecordingSettings.h"
#include "Animation/AnimTypes.h"

/** 30Hz default sample rate */
const float FAnimationRecordingSettings::DefaultSampleRate = DEFAULT_SAMPLERATE;

/** 1 minute default length */
const float FAnimationRecordingSettings::DefaultMaximumLength = 1.0f * 60.0f;

/** Length used to specify unbounded */
const float FAnimationRecordingSettings::UnboundedMaximumLength = 0.0f;
