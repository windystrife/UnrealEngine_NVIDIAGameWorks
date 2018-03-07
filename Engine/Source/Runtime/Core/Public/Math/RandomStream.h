// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Math/RotationMatrix.h"
#include "Math/Transform.h"

/**
 * Implements a thread-safe SRand based RNG.
 *
 * Very bad quality in the lower bits. Don't use the modulus (%) operator.
 */
struct FRandomStream
{
#ifdef COREUOBJECT_API
	friend COREUOBJECT_API class UScriptStruct* Z_Construct_UScriptStruct_FRandomStream();
#else
	friend class UScriptStruct* Z_Construct_UScriptStruct_FRandomStream();
#endif
	
public:

	/**
	 * Default constructor.
	 *
	 * The seed should be set prior to use.
	 */
	FRandomStream()
		: InitialSeed(0)
		, Seed(0)
	{ }

	/**
	 * Creates and initializes a new random stream from the specified seed value.
	 *
	 * @param InSeed The seed value.
	 */
	FRandomStream( int32 InSeed )
		: InitialSeed(InSeed)
		, Seed(InSeed)
	{ }

public:

	/**
	 * Initializes this random stream with the specified seed value.
	 *
	 * @param InSeed The seed value.
	 */
	void Initialize( int32 InSeed )
	{
		InitialSeed = InSeed;
		Seed = InSeed;
	}

	/**
	 * Resets this random stream to the initial seed value.
	 */
	void Reset() const
	{
		Seed = InitialSeed;
	}

	int32 GetInitialSeed() const
	{
		return InitialSeed;
	}

	/**
	 * Generates a new random seed.
	 */
	void GenerateNewSeed()
	{
		Initialize(FMath::Rand());
	}

	/**
	 * Returns a random number between 0 and 1.
	 *
	 * @return Random number.
	 */
	float GetFraction() const
	{
		MutateSeed();

		const float SRandTemp = 1.0f;
		float Result;

		*(int32*)&Result = (*(int32*)&SRandTemp & 0xff800000) | (Seed & 0x007fffff);

		return FMath::Fractional(Result); 
	}

	/**
	 * Returns a random number between 0 and MAXUINT.
	 *
	 * @return Random number.
	 */
	uint32 GetUnsignedInt() const
	{
		MutateSeed();

		return *(uint32*)&Seed;
	}

	/**
	 * Returns a random vector of unit size.
	 *
	 * @return Random unit vector.
	 */
	FVector GetUnitVector() const
	{
		FVector Result;
		float L;

		do
		{
			// Check random vectors in the unit sphere so result is statistically uniform.
			Result.X = GetFraction() * 2.f - 1.f;
			Result.Y = GetFraction() * 2.f - 1.f;
			Result.Z = GetFraction() * 2.f - 1.f;
			L = Result.SizeSquared();
		}
		while(L > 1.f || L < KINDA_SMALL_NUMBER);

		return Result.GetUnsafeNormal();
	}

	/**
	 * Gets the current seed.
	 *
	 * @return Current seed.
	 */
	int32 GetCurrentSeed() const
	{
		return Seed;
	}

	/**
	 * Mirrors the random number API in FMath
	 *
	 * @return Random number.
	 */
	FORCEINLINE float FRand() const
	{
		return GetFraction();
	}

	/**
	 * Helper function for rand implementations.
	 *
	 * @return A random number in [0..A)
	 */
	FORCEINLINE int32 RandHelper( int32 A ) const
	{
		// Can't just multiply GetFraction by A, as GetFraction could be == 1.0f
		return ((A > 0) ? FMath::TruncToInt(GetFraction() * ((float)A - DELTA)) : 0);
	}

	/**
	 * Helper function for rand implementations.
	 *
	 * @return A random number >= Min and <= Max
	 */
	FORCEINLINE int32 RandRange( int32 Min, int32 Max ) const
	{
		const int32 Range = (Max - Min) + 1;

		return Min + RandHelper(Range);
	}

	/**
	 * Helper function for rand implementations.
	 *
	 * @return A random number >= Min and <= Max
	 */
	FORCEINLINE float FRandRange( float InMin, float InMax ) const
	{
		return InMin + (InMax - InMin) * FRand();
	}

	/**
	 * Returns a random vector of unit size.
	 *
	 * @return Random unit vector.
	 */
	FORCEINLINE FVector VRand() const
	{
		return GetUnitVector();
	}

	/**
	 * Returns a random unit vector, uniformly distributed, within the specified cone.
	 *
	 * @param Dir The center direction of the cone
	 * @param ConeHalfAngleRad Half-angle of cone, in radians.
	 * @return Normalized vector within the specified cone.
	 */
	FORCEINLINE FVector VRandCone( FVector const& Dir, float ConeHalfAngleRad ) const
	{
		if (ConeHalfAngleRad > 0.f)
		{
			float const RandU = FRand();
			float const RandV = FRand();

			// Get spherical coords that have an even distribution over the unit sphere
			// Method described at http://mathworld.wolfram.com/SpherePointPicking.html	
			float Theta = 2.f * PI * RandU;
			float Phi = FMath::Acos((2.f * RandV) - 1.f);

			// restrict phi to [0, ConeHalfAngleRad]
			// this gives an even distribution of points on the surface of the cone
			// centered at the origin, pointing upward (z), with the desired angle
			Phi = FMath::Fmod(Phi, ConeHalfAngleRad);

			// get axes we need to rotate around
			FMatrix const DirMat = FRotationMatrix(Dir.Rotation());
			// note the axis translation, since we want the variation to be around X
			FVector const DirZ = DirMat.GetUnitAxis( EAxis::X );		
			FVector const DirY = DirMat.GetUnitAxis( EAxis::Y );

			FVector Result = Dir.RotateAngleAxis(Phi * 180.f / PI, DirY);
			Result = Result.RotateAngleAxis(Theta * 180.f / PI, DirZ);

			// ensure it's a unit vector (might not have been passed in that way)
			Result = Result.GetSafeNormal();
		
			return Result;
		}
		else
		{
			return Dir.GetSafeNormal();
		}
	}

	/**
	 * Returns a random unit vector, uniformly distributed, within the specified cone.
	 *
	 * @param Dir The center direction of the cone
	 * @param HorizontalConeHalfAngleRad Horizontal half-angle of cone, in radians.
	 * @param VerticalConeHalfAngleRad Vertical half-angle of cone, in radians.
	 * @return Normalized vector within the specified cone.
	 */
	FORCEINLINE FVector VRandCone( FVector const& Dir, float HorizontalConeHalfAngleRad, float VerticalConeHalfAngleRad ) const
	{
		if ( (VerticalConeHalfAngleRad > 0.f) && (HorizontalConeHalfAngleRad > 0.f) )
		{
			float const RandU = FRand();
			float const RandV = FRand();

			// Get spherical coords that have an even distribution over the unit sphere
			// Method described at http://mathworld.wolfram.com/SpherePointPicking.html	
			float Theta = 2.f * PI * RandU;
			float Phi = FMath::Acos((2.f * RandV) - 1.f);

			// restrict phi to [0, ConeHalfAngleRad]
			// where ConeHalfAngleRad is now a function of Theta
			// (specifically, radius of an ellipse as a function of angle)
			// function is ellipse function (x/a)^2 + (y/b)^2 = 1, converted to polar coords
			float ConeHalfAngleRad = FMath::Square(FMath::Cos(Theta) / VerticalConeHalfAngleRad) + FMath::Square(FMath::Sin(Theta) / HorizontalConeHalfAngleRad);
			ConeHalfAngleRad = FMath::Sqrt(1.f / ConeHalfAngleRad);

			// clamp to make a cone instead of a sphere
			Phi = FMath::Fmod(Phi, ConeHalfAngleRad);

			// get axes we need to rotate around
			FMatrix const DirMat = FRotationMatrix(Dir.Rotation());
			// note the axis translation, since we want the variation to be around X
			FVector const DirZ = DirMat.GetUnitAxis( EAxis::X );		
			FVector const DirY = DirMat.GetUnitAxis( EAxis::Y );

			FVector Result = Dir.RotateAngleAxis(Phi * 180.f / PI, DirY);
			Result = Result.RotateAngleAxis(Theta * 180.f / PI, DirZ);

			// ensure it's a unit vector (might not have been passed in that way)
			Result = Result.GetSafeNormal();

			return Result;
		}
		else
		{
			return Dir.GetSafeNormal();
		}
	}

	/**
	 * Exports the RandomStreams value to a string.
	 *
	 * @param ValueStr Will hold the string value.
	 * @param DefaultValue The default value.
	 * @param Parent Not used.
	 * @param PortFlags Not used.
	 * @param ExportRootScope Not used.
	 * @return true on success, false otherwise.
	 * @see ImportTextItem
	 */
	CORE_API bool ExportTextItem(FString& ValueStr, FRandomStream const& DefaultValue, class UObject* Parent, int32 PortFlags, class UObject* ExportRootScope) const;

protected:

	/**
	 * Mutates the current seed into the next seed.
	 */
	void MutateSeed() const
	{
		Seed = (Seed * 196314165) + 907633515; 
	}

private:

	// Holds the initial seed.
	int32 InitialSeed;

	// Holds the current seed.
	mutable int32 Seed;
};
