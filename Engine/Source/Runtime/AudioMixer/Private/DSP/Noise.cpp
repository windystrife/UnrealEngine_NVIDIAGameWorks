// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DSP/Noise.h"

namespace Audio
{
	FWhiteNoise::FWhiteNoise(const float InScale, const float InAdd)
		: Scale(InScale)
		, Add(InAdd)
	{

	}

	void FWhiteNoise::SetScaleAdd(const float InScale, const float InAdd)
	{
		Scale = InScale;
		Add = InAdd;
	}

	float FWhiteNoise::Generate()
	{
		return Add + Scale * FMath::FRandRange(-1.0f, 1.0f);
	}

	FPinkNoise::FPinkNoise(const float InScale, const float InAdd)
	{
		InitFilter();
		Noise.SetScaleAdd(InScale, InAdd);
	}

	void FPinkNoise::InitFilter()
	{
		for (int32 i = 0; i < 4; ++i)
		{
			X[i] = 0.0f;
			Y[i] = 0.0f;
		}

		A[0] = 0.0f;
		A[1] = -2.495f;
		A[2] = 2.017;
		A[3] = -0.522f;

		B[0] = 0.041f;
		B[1] = -0.96f;
		B[2] = 0.051f;
		B[3] = -0.004f;
	}

	/** Sets the output scale and add parameter. */
	void FPinkNoise::SetScaleAdd(const float InScale, const float InAdd)
	{
		Noise.SetScaleAdd(InScale, InAdd);
	}

	float FPinkNoise::Generate()
	{
		X[0] = Noise.Generate();

		float Output = 0.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			Output += (B[i] * X[i] - A[i] * Y[i]);
		}

		X[3] = X[2];
		X[2] = X[1];
		X[1] = X[0];

		Y[3] = Y[2];
		Y[2] = Y[1];
		Y[1] = Y[0];
		Y[0] = Output;

		return Output;
	}
}
