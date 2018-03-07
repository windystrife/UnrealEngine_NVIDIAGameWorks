// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioSoundFile.h"

namespace UAudio
{
	/**
	* TestDeviceQuery
	*
	* Test which queries input and output devices and displays information to log about device capabilities, format, etc.
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceQuery();

	/**
	* TestDeviceOutputSimple
	*
	* Test which runs a simple sinusoid output on all channels (each channel is one octave higher than left channel)
	* to test that audio output is working and function with no discontinuities.
	* @param TestTime The amount of time to run the test in seconds (negative value means to run indefinitely)
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceOutputSimple(double TestTime);

	/**
	* TestDeviceOutputRandomizedFm
	*
	* Test which tests output device functionality with a bit more fun than the simple test. You should hear randomized FM synthesis in panning through
	* all connected speaker outputs.
	* @param TestTime The amount of time to run the test in seconds (negative value means to run indefinitely)
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceOutputRandomizedFm(double TestTime);

	/**
	* TestDeviceOutputNoisePan
	*
	* Test which tests output device functionality with white noise that pans clockwise in surround sound setup.
	* @param TestTime The amount of time to run the test in seconds (negative value means to run indefinitely)
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceOutputNoisePan(double TestTime);

	/**
	* TestSourceConvert
	*
	* Tests converting a sound file.
	* @param ImportSettings A struct defining import settings.
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestSourceConvert(const FString& FilePath, const FSoundFileConvertFormat& ConvertFormat);

	/**
	* TestEmitterManager
	*
	* Tests the emitter manager with creating and updating emitters from one thread (simulating main thread) and processing from
	* the audio system thread.
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestEmitterManager();

	/**
	* TestVoiceManager
	*
	* Tests the voice manager with creating and updating voices from one thread (simulating main thread) and processing from
	* the audio system thread.
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestVoiceManager(const FString& FolderOrPath);


	bool UNREALAUDIO_API TestSoundFileManager(const FString& FolderOrPath);
}

