// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
SeparableSSS.cpp: Computing the kernel for the Separable Screen Space Subsurface Scattering, based on SeparableSSS, see copyright below
=============================================================================*/

/**
 * Copyright (C) 2012 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2012 Diego Gutierrez (diegog@unizar.es)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the following disclaimer
 *       in the documentation and/or other materials provided with the 
 *       distribution:
 *
 *       "Uses Separable SSS. Copyright (C) 2012 by Jorge Jimenez and Diego
 *        Gutierrez."
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS 
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS 
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are 
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the copyright holders.
 */


#include "Rendering/SeparableSSS.h"

// helper function for ComputeMirroredSSSKernel
inline FVector SeparableSSS_Gaussian(float variance, float r, FLinearColor FalloffColor)
{
	FVector Ret;

	/**
	* We use a falloff to modulate the shape of the profile. Big falloffs
	* spreads the shape making it wider, while small falloffs make it
	* narrower.
	*/
	for (int i = 0; i < 3; i++)
	{
		float rr = r / (0.001f + FalloffColor.Component(i));
		Ret[i] = exp((-(rr * rr)) / (2.0f * variance)) / (2.0f * 3.14f * variance);
	}

	return Ret;
}

// helper function for ComputeMirroredSSSKernel
// r is in mm
inline FVector SeparableSSS_Profile(float r, FLinearColor FalloffColor)
{
	/**
	* We used the red channel of the original skin profile defined in
	* [d'Eon07] for all three channels. We noticed it can be used for green
	* and blue channels (scaled using the falloff parameter) without
	* introducing noticeable differences and allowing for total control over
	* the profile. For example, it allows to create blue SSS gradients, which
	* could be useful in case of rendering blue creatures.
	*/
	// first parameter is variance in mm^2
	return  // 0.233f * SeparableSSS_Gaussian(0.0064f, r, FalloffColor) + /* We consider this one to be directly bounced light, accounted by the strength parameter (see @STRENGTH) */
		0.100f * SeparableSSS_Gaussian(0.0484f, r, FalloffColor) +
		0.118f * SeparableSSS_Gaussian(0.187f, r, FalloffColor) +
		0.113f * SeparableSSS_Gaussian(0.567f, r, FalloffColor) +
		0.358f * SeparableSSS_Gaussian(1.99f, r, FalloffColor) +
		0.078f * SeparableSSS_Gaussian(7.41f, r, FalloffColor);
}

void ComputeMirroredSSSKernel(FLinearColor* TargetBuffer, uint32 TargetBufferSize, FLinearColor SubsurfaceColor, FLinearColor FalloffColor)
{
	check(TargetBuffer);
	check(TargetBufferSize > 0);

	uint32 nNonMirroredSamples = TargetBufferSize;
	int32 nTotalSamples = nNonMirroredSamples * 2 - 1;

	// we could generate Out directly but the original code form SeparableSSS wasn't done like that so we convert it later
	// .a is in mm
	check(nTotalSamples < 64);
	FLinearColor kernel[64];
	{
		const float Range = nTotalSamples > 20 ? 3.0f : 2.0f;
		// tweak constant
		const float Exponent = 2.0f;

		// Calculate the offsets:
		float step = 2.0f * Range / (nTotalSamples - 1);
		for (int i = 0; i < nTotalSamples; i++)
		{
			float o = -Range + float(i) * step;
			float sign = o < 0.0f ? -1.0f : 1.0f;
			kernel[i].A = Range * sign * FMath::Abs(FMath::Pow(o, Exponent)) / FMath::Pow(Range, Exponent);
		}

		// Calculate the weights:
		for (int32 i = 0; i < nTotalSamples; i++)
		{
			float w0 = i > 0 ? FMath::Abs(kernel[i].A - kernel[i - 1].A) : 0.0f;
			float w1 = i < nTotalSamples - 1 ? FMath::Abs(kernel[i].A - kernel[i + 1].A) : 0.0f;
			float area = (w0 + w1) / 2.0f;
			FVector t = area * SeparableSSS_Profile(kernel[i].A, FalloffColor);
			kernel[i].R = t.X;
			kernel[i].G = t.Y;
			kernel[i].B = t.Z;
		}

		// We want the offset 0.0 to come first:
		FLinearColor t = kernel[nTotalSamples / 2];

		for (int i = nTotalSamples / 2; i > 0; i--)
		{
			kernel[i] = kernel[i - 1];
		}
		kernel[0] = t;

		// Normalize the weights in RGB
		{
			FVector sum = FVector(0, 0, 0);

			for (int i = 0; i < nTotalSamples; i++)
			{
				sum.X += kernel[i].R;
				sum.Y += kernel[i].G;
				sum.Z += kernel[i].B;
			}

			for (int i = 0; i < nTotalSamples; i++)
			{
				kernel[i].R /= sum.X;
				kernel[i].G /= sum.Y;
				kernel[i].B /= sum.Z;
			}
		}

		/* we do that in the shader for better quality with half res 
		
		// Tweak them using the desired strength. The first one is:
		//     lerp(1.0, kernel[0].rgb, strength)
		kernel[0].R = FMath::Lerp(1.0f, kernel[0].R, SubsurfaceColor.R);
		kernel[0].G = FMath::Lerp(1.0f, kernel[0].G, SubsurfaceColor.G);
		kernel[0].B = FMath::Lerp(1.0f, kernel[0].B, SubsurfaceColor.B);

		for (int i = 1; i < nTotalSamples; i++)
		{
			kernel[i].R *= SubsurfaceColor.R;
			kernel[i].G *= SubsurfaceColor.G;
			kernel[i].B *= SubsurfaceColor.B;
		}*/
	}

	// generate output (remove negative samples)
	{
		check(kernel[0].A == 0.0f);

		// center sample
		TargetBuffer[0] = kernel[0];

		// all positive samples
		for (uint32 i = 0; i < nNonMirroredSamples - 1; i++)
		{
			TargetBuffer[i + 1] = kernel[nNonMirroredSamples + i];
		}
	}
}
