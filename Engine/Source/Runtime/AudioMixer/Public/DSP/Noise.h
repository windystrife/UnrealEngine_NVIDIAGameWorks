// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	/** 
	* White noise generator 
	* Flat spectrum
	*/
	class AUDIOMIXER_API FWhiteNoise
	{
	public:
		/** Constructor with a default scale add parameter */
		FWhiteNoise(const float InScale = 1.0f, const float InAdd = 0.0f);

		void SetScaleAdd(const float InScale, const float InAdd);

		/** Generate next sample of white noise */
		float Generate();

	private:
		float Scale;
		float Add;
	};

	/** 
	* Pink noise generator
	* 1/Frequency noise spectrum
	*/
	class AUDIOMIXER_API FPinkNoise
	{
	public:
		/** Constructor. */
		FPinkNoise(const float InScale = 1.0f, const float InAdd = 0.0f);

		/** Sets the output scale and add parameter. */
		void SetScaleAdd(const float InScale, const float InAdd);

		/** Generate next sample of pink noise. */
		float Generate();

	private:

		void InitFilter();

		FWhiteNoise Noise;

		float A[4];
		float B[4];
		float X[4];
		float Y[4];
	};


}
