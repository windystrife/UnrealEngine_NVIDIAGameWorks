// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/PlatformMath.h"


//#define IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY

// Assert on non finite numbers. Used to track NaNs.
#ifndef ENABLE_NAN_DIAGNOSTIC
	#define ENABLE_NAN_DIAGNOSTIC 0
#endif

/*-----------------------------------------------------------------------------
	Definitions.
-----------------------------------------------------------------------------*/

// Forward declarations.
struct  FVector;
struct  FVector4;
struct  FPlane;
struct  FBox;
struct  FRotator;
struct  FMatrix;
struct  FQuat;
struct  FTwoVectors;
struct  FTransform;
class  FSphere;
struct FVector2D;
struct FLinearColor;

/*-----------------------------------------------------------------------------
	Floating point constants.
-----------------------------------------------------------------------------*/

#undef  PI
#define PI 					(3.1415926535897932f)
#define SMALL_NUMBER		(1.e-8f)
#define KINDA_SMALL_NUMBER	(1.e-4f)
#define BIG_NUMBER			(3.4e+38f)
#define EULERS_NUMBER       (2.71828182845904523536f)

// Copied from float.h
#define MAX_FLT 3.402823466e+38F

// Aux constants.
#define INV_PI			(0.31830988618f)
#define HALF_PI			(1.57079632679f)

// Magic numbers for numerical precision.
#define DELTA			(0.00001f)

/**
 * Lengths of normalized vectors (These are half their maximum values
 * to assure that dot products with normalized vectors don't overflow).
 */
#define FLOAT_NORMAL_THRESH				(0.0001f)

//
// Magic numbers for numerical precision.
//
#define THRESH_POINT_ON_PLANE			(0.10f)		/* Thickness of plane for front/back/inside test */
#define THRESH_POINT_ON_SIDE			(0.20f)		/* Thickness of polygon side's side-plane for point-inside/outside/on side test */
#define THRESH_POINTS_ARE_SAME			(0.00002f)	/* Two points are same if within this distance */
#define THRESH_POINTS_ARE_NEAR			(0.015f)	/* Two points are near if within this distance and can be combined if imprecise math is ok */
#define THRESH_NORMALS_ARE_SAME			(0.00002f)	/* Two normal points are same if within this distance */
													/* Making this too large results in incorrect CSG classification and disaster */
#define THRESH_VECTORS_ARE_NEAR			(0.0004f)	/* Two vectors are near if within this distance and can be combined if imprecise math is ok */
													/* Making this too large results in lighting problems due to inaccurate texture coordinates */
#define THRESH_SPLIT_POLY_WITH_PLANE	(0.25f)		/* A plane splits a polygon in half */
#define THRESH_SPLIT_POLY_PRECISELY		(0.01f)		/* A plane exactly splits a polygon */
#define THRESH_ZERO_NORM_SQUARED		(0.0001f)	/* Size of a unit normal that is considered "zero", squared */
#define THRESH_NORMALS_ARE_PARALLEL		(0.999845f)	/* Two unit vectors are parallel if abs(A dot B) is greater than or equal to this. This is roughly cosine(1.0 degrees). */
#define THRESH_NORMALS_ARE_ORTHOGONAL	(0.017455f)	/* Two unit vectors are orthogonal (perpendicular) if abs(A dot B) is less than or equal this. This is roughly cosine(89.0 degrees). */

#define THRESH_VECTOR_NORMALIZED		(0.01f)		/** Allowed error for a normalized vector (against squared magnitude) */
#define THRESH_QUAT_NORMALIZED			(0.01f)		/** Allowed error for a normalized quaternion (against squared magnitude) */

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

/**
 * Structure for all math helper functions, inherits from platform math to pick up platform-specific implementations
 * Check GenericPlatformMath.h for additional math functions
 */
struct FMath : public FPlatformMath
{
	// Random Number Functions

	/** Helper function for rand implementations. Returns a random number in [0..A) */
	static FORCEINLINE int32 RandHelper(int32 A)
	{
		// Note that on some platforms RAND_MAX is a large number so we cannot do ((rand()/(RAND_MAX+1)) * A)
		// or else we may include the upper bound results, which should be excluded.
		return A > 0 ? Min(TruncToInt(FRand() * A), A - 1) : 0;
	}

	/** Helper function for rand implementations. Returns a random number >= Min and <= Max */
	static FORCEINLINE int32 RandRange(int32 Min, int32 Max)
	{
		const int32 Range = (Max - Min) + 1;
		return Min + RandHelper(Range);
	}

	/** Util to generate a random number in a range. Overloaded to distinguish from int32 version, where passing a float is typically a mistake. */
	static FORCEINLINE float RandRange(float InMin, float InMax)
	{
		return FRandRange(InMin, InMax);
	}

	/** Util to generate a random number in a range. */
	static FORCEINLINE float FRandRange(float InMin, float InMax)
	{
		return InMin + (InMax - InMin) * FRand();
	}

	/** Util to generate a random boolean. */
	static FORCEINLINE bool RandBool()
	{
		return (RandRange(0,1) == 1) ? true : false;
	}

	/** Return a uniformly distributed random unit length vector = point on the unit sphere surface. */
	static FVector VRand();
	
	/**
	 * Returns a random unit vector, uniformly distributed, within the specified cone
	 * ConeHalfAngleRad is the half-angle of cone, in radians.  Returns a normalized vector. 
	 */
	static CORE_API FVector VRandCone(FVector const& Dir, float ConeHalfAngleRad);

	/** 
	 * This is a version of VRandCone that handles "squished" cones, i.e. with different angle limits in the Y and Z axes.
	 * Assumes world Y and Z, although this could be extended to handle arbitrary rotations.
	 */
	static CORE_API FVector VRandCone(FVector const& Dir, float HorizontalConeHalfAngleRad, float VerticalConeHalfAngleRad);

	/** Returns a random point within the passed in bounding box */
	static CORE_API FVector RandPointInBox(const FBox& Box);

	/** 
	 * Given a direction vector and a surface normal, returns the vector reflected across the surface normal.
	 * Produces a result like shining a laser at a mirror!
	 *
	 * @param Direction Direction vector the ray is coming from.
	 * @param SurfaceNormal A normal of the surface the ray should be reflected on.
	 *
	 * @returns Reflected vector.
	 */
	static CORE_API FVector GetReflectionVector(const FVector& Direction, const FVector& SurfaceNormal);
	
	// Predicates

	/** Checks if value is within a range, exclusive on MaxValue) */
	template< class U > 
	static FORCEINLINE bool IsWithin(const U& TestValue, const U& MinValue, const U& MaxValue)
	{
		return ((TestValue >= MinValue) && (TestValue < MaxValue));
	}

	/** Checks if value is within a range, inclusive on MaxValue) */
	template< class U > 
	static FORCEINLINE bool IsWithinInclusive(const U& TestValue, const U& MinValue, const U& MaxValue)
	{
		return ((TestValue>=MinValue) && (TestValue <= MaxValue));
	}
	
	/**
	 *	Checks if two floating point numbers are nearly equal.
	 *	@param A				First number to compare
	 *	@param B				Second number to compare
	 *	@param ErrorTolerance	Maximum allowed difference for considering them as 'nearly equal'
	 *	@return					true if A and B are nearly equal
	 */
	static FORCEINLINE bool IsNearlyEqual(float A, float B, float ErrorTolerance = SMALL_NUMBER)
	{
		return Abs<float>( A - B ) <= ErrorTolerance;
	}

	/**
	 *	Checks if two floating point numbers are nearly equal.
	 *	@param A				First number to compare
	 *	@param B				Second number to compare
	 *	@param ErrorTolerance	Maximum allowed difference for considering them as 'nearly equal'
	 *	@return					true if A and B are nearly equal
	 */
	static FORCEINLINE bool IsNearlyEqual(double A, double B, double ErrorTolerance = SMALL_NUMBER)
	{
		return Abs<double>( A - B ) <= ErrorTolerance;
	}

	/**
	 *	Checks if a floating point number is nearly zero.
	 *	@param Value			Number to compare
	 *	@param ErrorTolerance	Maximum allowed difference for considering Value as 'nearly zero'
	 *	@return					true if Value is nearly zero
	 */
	static FORCEINLINE bool IsNearlyZero(float Value, float ErrorTolerance = SMALL_NUMBER)
	{
		return Abs<float>( Value ) <= ErrorTolerance;
	}

	/**
	 *	Checks if a floating point number is nearly zero.
	 *	@param Value			Number to compare
	 *	@param ErrorTolerance	Maximum allowed difference for considering Value as 'nearly zero'
	 *	@return					true if Value is nearly zero
	 */
	static FORCEINLINE bool IsNearlyZero(double Value, double ErrorTolerance = SMALL_NUMBER)
	{
		return Abs<double>( Value ) <= ErrorTolerance;
	}

	/**
	 *	Checks whether a number is a power of two.
	 *	@param Value	Number to check
	 *	@return			true if Value is a power of two
	 */
	template <typename T>
	static FORCEINLINE bool IsPowerOfTwo( T Value )
	{
		return ((Value & (Value - 1)) == (T)0);
	}


	// Math Operations

	/** Returns highest of 3 values */
	template< class T > 
	static FORCEINLINE T Max3( const T A, const T B, const T C )
	{
		return Max ( Max( A, B ), C );
	}

	/** Returns lowest of 3 values */
	template< class T > 
	static FORCEINLINE T Min3( const T A, const T B, const T C )
	{
		return Min ( Min( A, B ), C );
	}

	/** Multiples value by itself */
	template< class T > 
	static FORCEINLINE T Square( const T A )
	{
		return A*A;
	}

	/** Clamps X to be between Min and Max, inclusive */
	template< class T > 
	static FORCEINLINE T Clamp( const T X, const T Min, const T Max )
	{
		return X<Min ? Min : X<Max ? X : Max;
	}

	/** Snaps a value to the nearest grid multiple */
	static FORCEINLINE float GridSnap( float Location, float Grid )
	{
		if( Grid==0.f )	return Location;
		else			
		{
			return FloorToFloat((Location + 0.5*Grid)/Grid)*Grid;
		}
	}

	/** Snaps a value to the nearest grid multiple */
	static FORCEINLINE double GridSnap( double Location, double Grid )
	{
		if( Grid==0.0 )	return Location;
		else			
		{
			return FloorToDouble((Location + 0.5*Grid)/Grid)*Grid;
		}
	}

	/** Divides two integers and rounds up */
	template <class T>
	static FORCEINLINE T DivideAndRoundUp(T Dividend,T Divisor)
	{
		return (Dividend + Divisor - 1) / Divisor;
	}

	template <class T>
	static FORCEINLINE T DivideAndRoundDown(T Dividend,T Divisor)
	{
		return Dividend / Divisor;
	}

	/**
	 * Computes the base 2 logarithm of the specified value
	 *
	 * @param Value the value to perform the log on
	 *
	 * @return the base 2 log of the value
	 */
	static FORCEINLINE float Log2(float Value)
	{
		// Cached value for fast conversions
		static const float LogToLog2 = 1.f / Loge(2.f);
		// Do the platform specific log and convert using the cached value
		return Loge(Value) * LogToLog2;
	}

	/**
	* Computes the sine and cosine of a scalar float.
	*
	* @param ScalarSin	Pointer to where the Sin result should be stored
	* @param ScalarCos	Pointer to where the Cos result should be stored
	* @param Value  input angles 
	*/
	static FORCEINLINE void SinCos( float* ScalarSin, float* ScalarCos, float  Value )
	{
		// Map Value to y in [-pi,pi], x = 2*pi*quotient + remainder.
		float quotient = (INV_PI*0.5f)*Value;
		if (Value >= 0.0f)
		{
			quotient = (float)((int)(quotient + 0.5f));
		}
		else
		{
			quotient = (float)((int)(quotient - 0.5f));
		}
		float y = Value - (2.0f*PI)*quotient;

		// Map y to [-pi/2,pi/2] with sin(y) = sin(Value).
		float sign;
		if (y > HALF_PI)
		{
			y = PI - y;
			sign = -1.0f;
		}
		else if (y < -HALF_PI)
		{
			y = -PI - y;
			sign = -1.0f;
		}
		else
		{
			sign = +1.0f;
		}

		float y2 = y * y;

		// 11-degree minimax approximation
		*ScalarSin = ( ( ( ( (-2.3889859e-08f * y2 + 2.7525562e-06f) * y2 - 0.00019840874f ) * y2 + 0.0083333310f ) * y2 - 0.16666667f ) * y2 + 1.0f ) * y;

		// 10-degree minimax approximation
		float p = ( ( ( ( -2.6051615e-07f * y2 + 2.4760495e-05f ) * y2 - 0.0013888378f ) * y2 + 0.041666638f ) * y2 - 0.5f ) * y2 + 1.0f;
		*ScalarCos = sign*p;
	}


	// Note:  We use FASTASIN_HALF_PI instead of HALF_PI inside of FastASin(), since it was the value that accompanied the minimax coefficients below.
	// It is important to use exactly the same value in all places inside this function to ensure that FastASin(0.0f) == 0.0f.
	// For comparison:
	//		HALF_PI				== 1.57079632679f == 0x3fC90FDB
	//		FASTASIN_HALF_PI	== 1.5707963050f  == 0x3fC90FDA
#define FASTASIN_HALF_PI (1.5707963050f)
	/**
	* Computes the ASin of a scalar float.
	*
	* @param Value  input angle
	* @return ASin of Value
	*/
	static FORCEINLINE float FastAsin(float Value)
	{
		// Clamp input to [-1,1].
		bool nonnegative = (Value >= 0.0f);
		float x = FMath::Abs(Value);
		float omx = 1.0f - x;
		if (omx < 0.0f)
		{
			omx = 0.0f;
		}
		float root = FMath::Sqrt(omx);
		// 7-degree minimax approximation
		float result = ((((((-0.0012624911f * x + 0.0066700901f) * x - 0.0170881256f) * x + 0.0308918810f) * x - 0.0501743046f) * x + 0.0889789874f) * x - 0.2145988016f) * x + FASTASIN_HALF_PI;
		result *= root;  // acos(|x|)
		// acos(x) = pi - acos(-x) when x < 0, asin(x) = pi/2 - acos(x)
		return (nonnegative ? FASTASIN_HALF_PI - result : result - FASTASIN_HALF_PI);
	}
#undef FASTASIN_HALF_PI


	// Conversion Functions

	/** 
	 * Converts radians to degrees.
	 * @param	RadVal			Value in radians.
	 * @return					Value in degrees.
	 */
	template<class T>
	static FORCEINLINE auto RadiansToDegrees(T const& RadVal) -> decltype(RadVal * (180.f / PI))
	{
		return RadVal * (180.f / PI);
	}

	/** 
	 * Converts degrees to radians.
	 * @param	DegVal			Value in degrees.
	 * @return					Value in radians.
	 */
	template<class T>
	static FORCEINLINE auto DegreesToRadians(T const& DegVal) -> decltype(DegVal * (PI / 180.f))
	{
		return DegVal * (PI / 180.f);
	}

	/** 
	 * Clamps an arbitrary angle to be between the given angles.  Will clamp to nearest boundary.
	 * 
	 * @param MinAngleDegrees	"from" angle that defines the beginning of the range of valid angles (sweeping clockwise)
	 * @param MaxAngleDegrees	"to" angle that defines the end of the range of valid angles
	 * @return Returns clamped angle in the range -180..180.
	 */
	static float CORE_API ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees);

	/** Find the smallest angle between two headings (in degrees) */
	static float FindDeltaAngleDegrees(float A1, float A2)
	{
		// Find the difference
		float Delta = A2 - A1;

		// If change is larger than 180
		if (Delta > 180.0f)
		{
			// Flip to negative equivalent
			Delta = Delta - 360.0f;
		}
		else if (Delta < -180.0f)
		{
			// Otherwise, if change is smaller than -180
			// Flip to positive equivalent
			Delta = Delta + 360.0f;
		}

		// Return delta in [-180,180] range
		return Delta;
	}

	/** Find the smallest angle between two headings (in radians) */
	static float FindDeltaAngleRadians(float A1, float A2)
	{
		// Find the difference
		float Delta = A2 - A1;

		// If change is larger than PI
		if (Delta > PI)
		{
			// Flip to negative equivalent
			Delta = Delta - (PI * 2.0f);
		}
		else if (Delta < -PI)
		{
			// Otherwise, if change is smaller than -PI
			// Flip to positive equivalent
			Delta = Delta + (PI * 2.0f);
		}

		// Return delta in [-PI,PI] range
		return Delta;
	}

	DEPRECATED(4.12, "Please use FindDeltaAngleRadians(float A1, float A2) instead of FindDeltaAngle(float A1, float A2).")
	static float FindDeltaAngle(float A1, float A2)
	{
		return FindDeltaAngleRadians(A1, A2);
	}

	/** Given a heading which may be outside the +/- PI range, 'unwind' it back into that range. */
	static float UnwindRadians(float A)
	{
		while(A > PI)
		{
			A -= ((float)PI * 2.0f);
		}

		while(A < -PI)
		{
			A += ((float)PI * 2.0f);
		}

		return A;
	}

	/** Utility to ensure angle is between +/- 180 degrees by unwinding. */
	static float UnwindDegrees(float A)
	{
		while(A > 180.f)
		{
			A -= 360.f;
		}

		while(A < -180.f)
		{
			A += 360.f;
		}

		return A;
	}

	/** 
	 * Given two angles in degrees, 'wind' the rotation in Angle1 so that it avoids >180 degree flips.
	 * Good for winding rotations previously expressed as quaternions into a euler-angle representation.
	 * @param	Angle0	The first angle that we wind relative to.
	 * @param	Angle1	The second angle that we may wind relative to the first.
	 */
	static CORE_API void WindRelativeAnglesDegrees(float InAngle0, float& InOutAngle1);

	/** Returns a new rotation component value
	 *
	 * @param InCurrent is the current rotation value
	 * @param InDesired is the desired rotation value
	 * @param InDeltaRate is the rotation amount to apply
	 *
	 * @return a new rotation component value
	 */
	static CORE_API float FixedTurn(float InCurrent, float InDesired, float InDeltaRate);

	/** Converts given Cartesian coordinate pair to Polar coordinate system. */
	static FORCEINLINE void CartesianToPolar(const float X, const float Y, float& OutRad, float& OutAng)
	{
		OutRad = Sqrt(Square(X) + Square(Y));
		OutAng = Atan2(Y, X);
	}
	/** Converts given Cartesian coordinate pair to Polar coordinate system. */
	static FORCEINLINE void CartesianToPolar(const FVector2D InCart, FVector2D& OutPolar);

	/** Converts given Polar coordinate pair to Cartesian coordinate system. */
	static FORCEINLINE void PolarToCartesian(const float Rad, const float Ang, float& OutX, float& OutY)
	{
		OutX = Rad * Cos(Ang);
		OutY = Rad * Sin(Ang);
	}
	/** Converts given Polar coordinate pair to Cartesian coordinate system. */
	static FORCEINLINE void PolarToCartesian(const FVector2D InPolar, FVector2D& OutCart);

	/**
	 * Calculates the dotted distance of vector 'Direction' to coordinate system O(AxisX,AxisY,AxisZ).
	 *
	 * Orientation: (consider 'O' the first person view of the player, and 'Direction' a vector pointing to an enemy)
	 * - positive azimuth means enemy is on the right of crosshair. (negative means left).
	 * - positive elevation means enemy is on top of crosshair, negative means below.
	 *
	 * @Note: 'Azimuth' (.X) sign is changed to represent left/right and not front/behind. front/behind is the funtion's return value.
	 *
	 * @param	OutDotDist	.X = 'Direction' dot AxisX relative to plane (AxisX,AxisZ). (== Cos(Azimuth))
	 *						.Y = 'Direction' dot AxisX relative to plane (AxisX,AxisY). (== Sin(Elevation))
	 * @param	Direction	direction of target.
	 * @param	AxisX		X component of reference system.
	 * @param	AxisY		Y component of reference system.
	 * @param	AxisZ		Z component of reference system.
	 *
	 * @return	true if 'Direction' is facing AxisX (Direction dot AxisX >= 0.f)
	 */
	static CORE_API bool GetDotDistance(FVector2D &OutDotDist, const FVector &Direction, const FVector &AxisX, const FVector &AxisY, const FVector &AxisZ);

	/**
	 * Returns Azimuth and Elevation of vector 'Direction' in coordinate system O(AxisX,AxisY,AxisZ).
	 *
	 * Orientation: (consider 'O' the first person view of the player, and 'Direction' a vector pointing to an enemy)
	 * - positive azimuth means enemy is on the right of crosshair. (negative means left).
	 * - positive elevation means enemy is on top of crosshair, negative means below.
	 *
	 * @param	Direction		Direction of target.
	 * @param	AxisX			X component of reference system.
	 * @param	AxisY			Y component of reference system.
	 * @param	AxisZ			Z component of reference system.
	 *
	 * @return	FVector2D	X = Azimuth angle (in radians) (-PI, +PI)
	 *						Y = Elevation angle (in radians) (-PI/2, +PI/2)
	 */
	static CORE_API FVector2D GetAzimuthAndElevation(const FVector &Direction, const FVector &AxisX, const FVector &AxisY, const FVector &AxisZ);

	// Interpolation Functions

	/** Calculates the percentage along a line from MinValue to MaxValue that Value is. */
	static FORCEINLINE float GetRangePct(float MinValue, float MaxValue, float Value)
	{
		return (Value - MinValue) / (MaxValue - MinValue);
	}

	/** Same as above, but taking a 2d vector as the range. */
	static float GetRangePct(FVector2D const& Range, float Value);
	
	/** Basically a Vector2d version of Lerp. */
	static float GetRangeValue(FVector2D const& Range, float Pct);

	/** For the given Value clamped to the [Input:Range] inclusive, returns the corresponding percentage in [Output:Range] Inclusive. */
	static FORCEINLINE float GetMappedRangeValueClamped(const FVector2D& InputRange, const FVector2D& OutputRange, const float Value)
	{
		const float ClampedPct = Clamp<float>(GetRangePct(InputRange, Value), 0.f, 1.f);
		return GetRangeValue(OutputRange, ClampedPct);
	}

	/** Transform the given Value relative to the input range to the Output Range. */
	static FORCEINLINE float GetMappedRangeValueUnclamped(const FVector2D& InputRange, const FVector2D& OutputRange, const float Value)
	{
		return GetRangeValue(OutputRange, GetRangePct(InputRange, Value));
	}

	/** Performs a linear interpolation between two values, Alpha ranges from 0-1 */
	template< class T, class U > 
	static FORCEINLINE_DEBUGGABLE T Lerp( const T& A, const T& B, const U& Alpha )
	{
		return (T)(A + Alpha * (B-A));
	}

	/** Performs a linear interpolation between two values, Alpha ranges from 0-1. Handles full numeric range of T */
	template< class T > 
	static FORCEINLINE_DEBUGGABLE T LerpStable( const T& A, const T& B, double Alpha )
	{
		return (T)((A * (1.0 - Alpha)) + (B * Alpha));
	}

	/** Performs a linear interpolation between two values, Alpha ranges from 0-1. Handles full numeric range of T */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T LerpStable(const T& A, const T& B, float Alpha)
	{
		return (T)((A * (1.0f - Alpha)) + (B * Alpha));
	}

	/** Performs a 2D linear interpolation between four values values, FracX, FracY ranges from 0-1 */
	template< class T, class U > 
	static FORCEINLINE_DEBUGGABLE T BiLerp(const T& P00,const T& P10,const T& P01,const T& P11, const U& FracX, const U& FracY)
	{
		return Lerp(
			Lerp(P00,P10,FracX),
			Lerp(P01,P11,FracX),
			FracY
			);
	}

	/**
	 * Performs a cubic interpolation
	 *
	 * @param  P - end points
	 * @param  T - tangent directions at end points
	 * @param  Alpha - distance along spline
	 *
	 * @return  Interpolated value
	 */
	template< class T, class U > 
	static FORCEINLINE_DEBUGGABLE T CubicInterp( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
	{
		const float A2 = A  * A;
		const float A3 = A2 * A;

		return (T)(((2*A3)-(3*A2)+1) * P0) + ((A3-(2*A2)+A) * T0) + ((A3-A2) * T1) + (((-2*A3)+(3*A2)) * P1);
	}

	/**
	 * Performs a first derivative cubic interpolation
	 *
	 * @param  P - end points
	 * @param  T - tangent directions at end points
	 * @param  Alpha - distance along spline
	 *
	 * @return  Interpolated value
	 */
	template< class T, class U > 
	static FORCEINLINE_DEBUGGABLE T CubicInterpDerivative( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
	{
		T a = 6.f*P0 + 3.f*T0 + 3.f*T1 - 6.f*P1;
		T b = -6.f*P0 - 4.f*T0 - 2.f*T1 + 6.f*P1;
		T c = T0;

		const float A2 = A  * A;

		return (a * A2) + (b * A) + c;
	}

	/**
	 * Performs a second derivative cubic interpolation
	 *
	 * @param  P - end points
	 * @param  T - tangent directions at end points
	 * @param  Alpha - distance along spline
	 *
	 * @return  Interpolated value
	 */
	template< class T, class U > 
	static FORCEINLINE_DEBUGGABLE T CubicInterpSecondDerivative( const T& P0, const T& T0, const T& P1, const T& T1, const U& A )
	{
		T a = 12.f*P0 + 6.f*T0 + 6.f*T1 - 12.f*P1;
		T b = -6.f*P0 - 4.f*T0 - 2.f*T1 + 6.f*P1;

		return (a * A) + b;
	}

	/** Interpolate between A and B, applying an ease in function.  Exp controls the degree of the curve. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpEaseIn(const T& A, const T& B, float Alpha, float Exp)
	{
		float const ModifiedAlpha = Pow(Alpha, Exp);
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolate between A and B, applying an ease out function.  Exp controls the degree of the curve. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpEaseOut(const T& A, const T& B, float Alpha, float Exp)
	{
		float const ModifiedAlpha = 1.f - Pow(1.f - Alpha, Exp);
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolate between A and B, applying an ease in/out function.  Exp controls the degree of the curve. */
	template< class T > 
	static FORCEINLINE_DEBUGGABLE T InterpEaseInOut( const T& A, const T& B, float Alpha, float Exp )
	{
		return Lerp<T>(A, B, (Alpha < 0.5f) ?
			InterpEaseIn(0.f, 1.f, Alpha * 2.f, Exp) * 0.5f :
			InterpEaseOut(0.f, 1.f, Alpha * 2.f - 1.f, Exp) * 0.5f + 0.5f);
	}

	/** Interpolation between A and B, applying a step function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpStep(const T& A, const T& B, float Alpha, int32 Steps)
	{
		if (Steps <= 1 || Alpha <= 0)
		{
			return A;
		}
		else if (Alpha >= 1)
		{
			return B;
		}

		const float StepsAsFloat = static_cast<float>(Steps);
		const float NumIntervals = StepsAsFloat - 1.f;
		float const ModifiedAlpha = FloorToFloat(Alpha * StepsAsFloat) / NumIntervals;
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolation between A and B, applying a sinusoidal in function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpSinIn(const T& A, const T& B, float Alpha)
	{
		float const ModifiedAlpha = -1.f * Cos(Alpha * HALF_PI) + 1.f;
		return Lerp<T>(A, B, ModifiedAlpha);
	}
	
	/** Interpolation between A and B, applying a sinusoidal out function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpSinOut(const T& A, const T& B, float Alpha)
	{
		float const ModifiedAlpha = Sin(Alpha * HALF_PI);
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolation between A and B, applying a sinusoidal in/out function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpSinInOut(const T& A, const T& B, float Alpha)
	{
		return Lerp<T>(A, B, (Alpha < 0.5f) ?
			InterpSinIn(0.f, 1.f, Alpha * 2.f) * 0.5f :
			InterpSinOut(0.f, 1.f, Alpha * 2.f - 1.f) * 0.5f + 0.5f);
	}

	/** Interpolation between A and B, applying an exponential in function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpExpoIn(const T& A, const T& B, float Alpha)
	{
		float const ModifiedAlpha = (Alpha == 0.f) ? 0.f : Pow(2.f, 10.f * (Alpha - 1.f));
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolation between A and B, applying an exponential out function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpExpoOut(const T& A, const T& B, float Alpha)
	{
		float const ModifiedAlpha = (Alpha == 1.f) ? 1.f : -Pow(2.f, -10.f * Alpha) + 1.f;
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolation between A and B, applying an exponential in/out function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpExpoInOut(const T& A, const T& B, float Alpha)
	{
		return Lerp<T>(A, B, (Alpha < 0.5f) ?
			InterpExpoIn(0.f, 1.f, Alpha * 2.f) * 0.5f :
			InterpExpoOut(0.f, 1.f, Alpha * 2.f - 1.f) * 0.5f + 0.5f);
	}

	/** Interpolation between A and B, applying a circular in function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpCircularIn(const T& A, const T& B, float Alpha)
	{
		float const ModifiedAlpha = -1.f * (Sqrt(1.f - Alpha * Alpha) - 1.f);
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolation between A and B, applying a circular out function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpCircularOut(const T& A, const T& B, float Alpha)
	{
		Alpha -= 1.f;
		float const ModifiedAlpha = Sqrt(1.f - Alpha  * Alpha);
		return Lerp<T>(A, B, ModifiedAlpha);
	}

	/** Interpolation between A and B, applying a circular in/out function. */
	template< class T >
	static FORCEINLINE_DEBUGGABLE T InterpCircularInOut(const T& A, const T& B, float Alpha)
	{
		return Lerp<T>(A, B, (Alpha < 0.5f) ?
			InterpCircularIn(0.f, 1.f, Alpha * 2.f) * 0.5f :
			InterpCircularOut(0.f, 1.f, Alpha * 2.f - 1.f) * 0.5f + 0.5f);
	}

	// Rotator specific interpolation
	template< class U > static FRotator Lerp(const FRotator& A, const FRotator& B, const U& Alpha);
	template< class U > static FRotator LerpRange(const FRotator& A, const FRotator& B, const U& Alpha);

	// Quat-specific interpolation

	template< class U > static FQuat Lerp(const FQuat& A, const FQuat& B, const U& Alpha);
	template< class U > static FQuat BiLerp(const FQuat& P00, const FQuat& P10, const FQuat& P01, const FQuat& P11, float FracX, float FracY);

	/**
	 * In the case of quaternions, we use a bezier like approach.
	 * T - Actual 'control' orientations.
	 */
	template< class U > static FQuat CubicInterp( const FQuat& P0, const FQuat& T0, const FQuat& P1, const FQuat& T1, const U& A);

	/*
	 *	Cubic Catmull-Rom Spline interpolation. Based on http://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf 
	 *	Curves are guaranteed to pass through the control points and are easily chained together.
	 *	Equation supports abitrary parameterization. eg. Uniform=0,1,2,3 ; chordal= |Pn - Pn-1| ; centripetal = |Pn - Pn-1|^0.5
	 *	P0 - The control point preceding the interpolation range.
	 *	P1 - The control point starting the interpolation range.
	 *	P2 - The control point ending the interpolation range.
	 *	P3 - The control point following the interpolation range.
	 *	T0-3 - The interpolation parameters for the corresponding control points.		
	 *	T - The interpolation factor in the range 0 to 1. 0 returns P1. 1 returns P2.
	 */
	template< class U > 
	static FORCEINLINE_DEBUGGABLE U CubicCRSplineInterp(const U& P0, const U& P1, const U& P2, const U& P3, const float T0, const float T1, const float T2, const float T3, const float T)
	{
		//Based on http://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf 
		float InvT1MinusT0 = 1.0f / (T1 - T0);
		U L01 = ( P0 * ((T1 - T) * InvT1MinusT0) ) + ( P1 * ((T - T0) * InvT1MinusT0) );
		float InvT2MinusT1 = 1.0f / (T2 - T1);
		U L12 = ( P1 * ((T2 - T) * InvT2MinusT1) ) + ( P2 * ((T - T1) * InvT2MinusT1) );
		float InvT3MinusT2 = 1.0f / (T3 - T2);
		U L23 = ( P2 * ((T3 - T) * InvT3MinusT2) ) + ( P3 * ((T - T2) * InvT3MinusT2) );

		float InvT2MinusT0 = 1.0f / (T2 - T0);
		U L012 = ( L01 * ((T2 - T) * InvT2MinusT0) ) + ( L12 * ((T - T0) * InvT2MinusT0) );
		float InvT3MinusT1 = 1.0f / (T3 - T1);
		U L123 = ( L12 * ((T3 - T) * InvT3MinusT1) ) + ( L23 * ((T - T1) * InvT3MinusT1) );

		return  ( ( L012 * ((T2 - T) * InvT2MinusT1) ) + ( L123 * ((T - T1) * InvT2MinusT1) ) );
	}

	/* Same as CubicCRSplineInterp but with additional saftey checks. If the checks fail P1 is returned. **/
	template< class U >
	static FORCEINLINE_DEBUGGABLE U CubicCRSplineInterpSafe(const U& P0, const U& P1, const U& P2, const U& P3, const float T0, const float T1, const float T2, const float T3, const float T)
	{
		//Based on http://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf 
		float T1MinusT0 = (T1 - T0);
		float T2MinusT1 = (T2 - T1);
		float T3MinusT2 = (T3 - T2);
		float T2MinusT0 = (T2 - T0);
		float T3MinusT1 = (T3 - T1);
		if (FMath::IsNearlyZero(T1MinusT0) || FMath::IsNearlyZero(T2MinusT1) || FMath::IsNearlyZero(T3MinusT2) || FMath::IsNearlyZero(T2MinusT0) || FMath::IsNearlyZero(T3MinusT1))
		{
			//There's going to be a divide by zero here so just bail out and return P1
			return P1;
		}

		float InvT1MinusT0 = 1.0f / T1MinusT0;
		U L01 = (P0 * ((T1 - T) * InvT1MinusT0)) + (P1 * ((T - T0) * InvT1MinusT0));
		float InvT2MinusT1 = 1.0f / T2MinusT1;
		U L12 = (P1 * ((T2 - T) * InvT2MinusT1)) + (P2 * ((T - T1) * InvT2MinusT1));
		float InvT3MinusT2 = 1.0f / T3MinusT2;
		U L23 = (P2 * ((T3 - T) * InvT3MinusT2)) + (P3 * ((T - T2) * InvT3MinusT2));

		float InvT2MinusT0 = 1.0f / T2MinusT0;
		U L012 = (L01 * ((T2 - T) * InvT2MinusT0)) + (L12 * ((T - T0) * InvT2MinusT0));
		float InvT3MinusT1 = 1.0f / T3MinusT1;
		U L123 = (L12 * ((T3 - T) * InvT3MinusT1)) + (L23 * ((T - T1) * InvT3MinusT1));

		return  ((L012 * ((T2 - T) * InvT2MinusT1)) + (L123 * ((T - T1) * InvT2MinusT1)));
	}


	// Special-case interpolation

	/** Interpolate a normal vector Current to Target, by interpolating the angle between those vectors with constant step. */
	static CORE_API FVector VInterpNormalRotationTo(const FVector& Current, const FVector& Target, float DeltaTime, float RotationSpeedDegrees);

	/** Interpolate vector from Current to Target with constant step */
	static CORE_API FVector VInterpConstantTo(const FVector Current, const FVector& Target, float DeltaTime, float InterpSpeed);

	/** Interpolate vector from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out. */
	static CORE_API FVector VInterpTo( const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed );
	
	/** Interpolate vector2D from Current to Target with constant step */
	static CORE_API FVector2D Vector2DInterpConstantTo( const FVector2D& Current, const FVector2D& Target, float DeltaTime, float InterpSpeed );

	/** Interpolate vector2D from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out. */
	static CORE_API FVector2D Vector2DInterpTo( const FVector2D& Current, const FVector2D& Target, float DeltaTime, float InterpSpeed );

	/** Interpolate rotator from Current to Target with constant step */
	static CORE_API FRotator RInterpConstantTo( const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);

	/** Interpolate rotator from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out. */
	static CORE_API FRotator RInterpTo( const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed);

	/** Interpolate float from Current to Target with constant step */
	static CORE_API float FInterpConstantTo( float Current, float Target, float DeltaTime, float InterpSpeed );

	/** Interpolate float from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out. */
	static CORE_API float FInterpTo( float Current, float Target, float DeltaTime, float InterpSpeed );

	/** Interpolate Linear Color from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out. */
	static CORE_API FLinearColor CInterpTo(const FLinearColor& Current, const FLinearColor& Target, float DeltaTime, float InterpSpeed);

	/**
	 * Simple function to create a pulsating scalar value
	 *
	 * @param  InCurrentTime  Current absolute time
	 * @param  InPulsesPerSecond  How many full pulses per second?
	 * @param  InPhase  Optional phase amount, between 0.0 and 1.0 (to synchronize pulses)
	 *
	 * @return  Pulsating value (0.0-1.0)
	 */
	static float MakePulsatingValue( const double InCurrentTime, const float InPulsesPerSecond, const float InPhase = 0.0f )
	{
		return 0.5f + 0.5f * FMath::Sin( ( ( 0.25f + InPhase ) * PI * 2.0 ) + ( InCurrentTime * PI * 2.0 ) * InPulsesPerSecond );
	}

	// Geometry intersection 


	/**
	 * Find the intersection of a line and an offset plane. Assumes that the
	 * line and plane do indeed intersect; you must make sure they're not
	 * parallel before calling.
	 *
	 * @param Point1 the first point defining the line
	 * @param Point2 the second point defining the line
	 * @param PlaneOrigin the origin of the plane
	 * @param PlaneNormal the normal of the plane
	 *
	 * @return The point of intersection between the line and the plane.
	 */
	static FVector LinePlaneIntersection( const FVector &Point1, const FVector &Point2, const FVector &PlaneOrigin, const FVector &PlaneNormal);

	/**
	 * Find the intersection of a line and a plane. Assumes that the line and
	 * plane do indeed intersect; you must make sure they're not parallel before
	 * calling.
	 *
	 * @param Point1 the first point defining the line
	 * @param Point2 the second point defining the line
	 * @param Plane the plane
	 *
	 * @return The point of intersection between the line and the plane.
	 */
	static FVector LinePlaneIntersection( const FVector &Point1, const FVector &Point2, const FPlane  &Plane);

	// @parma InOutScissorRect should be set to View.ViewRect before the call
	// @return 0: light is not visible, 1:use scissor rect, 2: no scissor rect needed
	static CORE_API uint32 ComputeProjectedSphereScissorRect(struct FIntRect& InOutScissorRect, FVector SphereOrigin, float Radius, FVector ViewOrigin, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix);

	/** 
	 * Determine if a plane and an AABB intersect
	 * @param P - the plane to test
	 * @param AABB - the axis aligned bounding box to test
	 * @return if collision occurs
	 */
	static CORE_API bool PlaneAABBIntersection(const FPlane& P, const FBox& AABB);

	/**
	 * Performs a sphere vs box intersection test using Arvo's algorithm:
	 *
	 *	for each i in (x, y, z)
	 *		if (SphereCenter(i) < BoxMin(i)) d2 += (SphereCenter(i) - BoxMin(i)) ^ 2
	 *		else if (SphereCenter(i) > BoxMax(i)) d2 += (SphereCenter(i) - BoxMax(i)) ^ 2
	 *
	 * @param Sphere the center of the sphere being tested against the AABB
	 * @param RadiusSquared the size of the sphere being tested
	 * @param AABB the box being tested against
	 *
	 * @return Whether the sphere/box intersect or not.
	 */
	static bool SphereAABBIntersection(const FVector& SphereCenter,const float RadiusSquared,const FBox& AABB);

	/**
	 * Converts a sphere into a point plus radius squared for the test above
	 */
	static bool SphereAABBIntersection(const FSphere& Sphere,const FBox& AABB);

	/** Determines whether a point is inside a box. */
	static bool PointBoxIntersection( const FVector& Point, const FBox& Box );

	/** Determines whether a line intersects a box. */
	static bool LineBoxIntersection( const FBox& Box, const FVector& Start, const FVector& End, const FVector& Direction );

	/** Determines whether a line intersects a box. This overload avoids the need to do the reciprocal every time. */
	static bool LineBoxIntersection( const FBox& Box, const FVector& Start, const FVector& End, const FVector& Direction, const FVector& OneOverDirection );

	/* Swept-Box vs Box test */
	static CORE_API bool LineExtentBoxIntersection(const FBox& inBox, const FVector& Start, const FVector& End, const FVector& Extent, FVector& HitLocation, FVector& HitNormal, float& HitTime);

	/** Determines whether a line intersects a sphere. */
	static bool LineSphereIntersection(const FVector& Start,const FVector& Dir,float Length,const FVector& Origin,float Radius);

	/**
	 * Assumes the cone tip is at 0,0,0 (means the SphereCenter is relative to the cone tip)
	 * @return true: cone and sphere do intersect, false otherwise
	 */
	static CORE_API bool SphereConeIntersection(const FVector& SphereCenter, float SphereRadius, const FVector& ConeAxis, float ConeAngleSin, float ConeAngleCos);

	/** Find the point on the line segment from LineStart to LineEnd which is closest to Point */
	static CORE_API FVector ClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point);

	/** Find the point on the infinite line between two points (LineStart, LineEnd) which is closest to Point */
	static CORE_API FVector ClosestPointOnInfiniteLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point);

	/** Compute intersection point of three planes. Return 1 if valid, 0 if infinite. */
	static bool IntersectPlanes3( FVector& I, const FPlane& P1, const FPlane& P2, const FPlane& P3 );

	/**
	 * Compute intersection point and direction of line joining two planes.
	 * Return 1 if valid, 0 if infinite.
	 */
	static bool IntersectPlanes2( FVector& I, FVector& D, const FPlane& P1, const FPlane& P2 );

	/**
	 * Calculates the distance of a given Point in world space to a given line,
	 * defined by the vector couple (Origin, Direction).
	 *
	 * @param	Point				Point to check distance to line
	 * @param	Direction			Vector indicating the direction of the line. Not required to be normalized.
	 * @param	Origin				Point of reference used to calculate distance
	 * @param	OutClosestPoint	optional point that represents the closest point projected onto Axis
	 *
	 * @return	distance of Point from line defined by (Origin, Direction)
	 */
	static CORE_API float PointDistToLine(const FVector &Point, const FVector &Direction, const FVector &Origin, FVector &OutClosestPoint);
	static CORE_API float PointDistToLine(const FVector &Point, const FVector &Direction, const FVector &Origin);

	/**
	 * Returns closest point on a segment to a given point.
	 * The idea is to project point on line formed by segment.
	 * Then we see if the closest point on the line is outside of segment or inside.
	 *
	 * @param	Point			point for which we find the closest point on the segment
	 * @param	StartPoint		StartPoint of segment
	 * @param	EndPoint		EndPoint of segment
	 *
	 * @return	point on the segment defined by (StartPoint, EndPoint) that is closest to Point.
	 */
	static CORE_API FVector ClosestPointOnSegment(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint);

	/**
	* FVector2D version of ClosestPointOnSegment.
	* Returns closest point on a segment to a given 2D point.
	* The idea is to project point on line formed by segment.
	* Then we see if the closest point on the line is outside of segment or inside.
	*
	* @param	Point			point for which we find the closest point on the segment
	* @param	StartPoint		StartPoint of segment
	* @param	EndPoint		EndPoint of segment
	*
	* @return	point on the segment defined by (StartPoint, EndPoint) that is closest to Point.
	*/
	static CORE_API FVector2D ClosestPointOnSegment2D(const FVector2D &Point, const FVector2D &StartPoint, const FVector2D &EndPoint);

	/**
	 * Returns distance from a point to the closest point on a segment.
	 *
	 * @param	Point			point to check distance for
	 * @param	StartPoint		StartPoint of segment
	 * @param	EndPoint		EndPoint of segment
	 *
	 * @return	closest distance from Point to segment defined by (StartPoint, EndPoint).
	 */
	static CORE_API float PointDistToSegment(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint);

	/**
	 * Returns square of the distance from a point to the closest point on a segment.
	 *
	 * @param	Point			point to check distance for
	 * @param	StartPoint		StartPoint of segment
	 * @param	EndPoint		EndPoint of segment
	 *
	 * @return	square of the closest distance from Point to segment defined by (StartPoint, EndPoint).
	 */
	static CORE_API float PointDistToSegmentSquared(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint);

	/** 
	 * Find closest points between 2 segments.
	 *
	 * If either segment may have a length of 0, use SegmentDistToSegmentSafe instance.
	 *
	 * @param	(A1, B1)	defines the first segment.
	 * @param	(A2, B2)	defines the second segment.
	 * @param	OutP1		Closest point on segment 1 to segment 2.
	 * @param	OutP2		Closest point on segment 2 to segment 1.
	 */
	static CORE_API void SegmentDistToSegment(FVector A1, FVector B1, FVector A2, FVector B2, FVector& OutP1, FVector& OutP2);

	/** 
	 * Find closest points between 2 segments.
	 *
	 * This is the safe version, and will check both segments' lengths.
	 * Use this if either (or both) of the segments lengths may be 0.
	 *
	 * @param	(A1, B1)	defines the first segment.
	 * @param	(A2, B2)	defines the second segment.
	 * @param	OutP1		Closest point on segment 1 to segment 2.
	 * @param	OutP2		Closest point on segment 2 to segment 1.
	 */
	static CORE_API void SegmentDistToSegmentSafe(FVector A1, FVector B1, FVector A2, FVector B2, FVector& OutP1, FVector& OutP2);

	/**
	 * returns the time (t) of the intersection of the passed segment and a plane (could be <0 or >1)
	 * @param StartPoint - start point of segment
	 * @param EndPoint   - end point of segment
	 * @param Plane		- plane to intersect with
	 * @return time(T) of intersection
	 */
	static CORE_API float GetTForSegmentPlaneIntersect(const FVector& StartPoint, const FVector& EndPoint, const FPlane& Plane);

	/**
	 * Returns true if there is an intersection between the segment specified by StartPoint and Endpoint, and
	 * the plane on which polygon Plane lies. If there is an intersection, the point is placed in out_IntersectionPoint
	 * @param StartPoint - start point of segment
	 * @param EndPoint   - end point of segment
	 * @param Plane		- plane to intersect with
	 * @param out_IntersectionPoint - out var for the point on the segment that intersects the mesh (if any)
	 * @return true if intersection occurred
	 */
	static CORE_API bool SegmentPlaneIntersection(const FVector& StartPoint, const FVector& EndPoint, const FPlane& Plane, FVector& out_IntersectionPoint);


	/**
	* Returns true if there is an intersection between the segment specified by StartPoint and Endpoint, and
	* the Triangle defined by A, B and C. If there is an intersection, the point is placed in out_IntersectionPoint
	* @param StartPoint - start point of segment
	* @param EndPoint   - end point of segment
	* @param A, B, C	- points defining the triangle 
	* @param OutIntersectPoint - out var for the point on the segment that intersects the triangle (if any)
	* @param OutNormal - out var for the triangle normal
	* @return true if intersection occurred
	*/
	static CORE_API bool SegmentTriangleIntersection(const FVector& StartPoint, const FVector& EndPoint, const FVector& A, const FVector& B, const FVector& C, FVector& OutIntersectPoint, FVector& OutTriangleNormal);

	/**
	 * Returns true if there is an intersection between the segment specified by SegmentStartA and SegmentEndA, and
	 * the segment specified by SegmentStartB and SegmentEndB, in 2D space. If there is an intersection, the point is placed in out_IntersectionPoint
	 * @param SegmentStartA - start point of first segment
	 * @param SegmentEndA   - end point of first segment
	 * @param SegmentStartB - start point of second segment
	 * @param SegmentEndB   - end point of second segment
	 * @param out_IntersectionPoint - out var for the intersection point (if any)
	 * @return true if intersection occurred
	 */
	static CORE_API bool SegmentIntersection2D(const FVector& SegmentStartA, const FVector& SegmentEndA, const FVector& SegmentStartB, const FVector& SegmentEndB, FVector& out_IntersectionPoint);


	/**
	 * Returns closest point on a triangle to a point.
	 * The idea is to identify the halfplanes that the point is
	 * in relative to each triangle segment "plane"
	 *
	 * @param	Point			point to check distance for
	 * @param	A,B,C			counter clockwise ordering of points defining a triangle
	 *
	 * @return	Point on triangle ABC closest to given point
	 */
	static CORE_API FVector ClosestPointOnTriangleToPoint(const FVector& Point, const FVector& A, const FVector& B, const FVector& C);

	/**
	 * Returns closest point on a tetrahedron to a point.
	 * The idea is to identify the halfplanes that the point is
	 * in relative to each face of the tetrahedron
	 *
	 * @param	Point			point to check distance for
	 * @param	A,B,C,D			four points defining a tetrahedron
	 *
	 * @return	Point on tetrahedron ABCD closest to given point
	 */
	static CORE_API FVector ClosestPointOnTetrahedronToPoint(const FVector& Point, const FVector& A, const FVector& B, const FVector& C, const FVector& D);

	/** 
	 * Find closest point on a Sphere to a Line.
	 * When line intersects		Sphere, then closest point to LineOrigin is returned.
	 * @param SphereOrigin		Origin of Sphere
	 * @param SphereRadius		Radius of Sphere
	 * @param LineOrigin		Origin of line
	 * @param LineDir			Direction of line. Needs to be normalized!!
	 * @param OutClosestPoint	Closest point on sphere to given line.
	 */
	static CORE_API void SphereDistToLine(FVector SphereOrigin, float SphereRadius, FVector LineOrigin, FVector LineDir, FVector& OutClosestPoint);

	/**
	 * Calculates whether a Point is within a cone segment, and also what percentage within the cone (100% is along the center line, whereas 0% is along the edge)
	 *
	 * @param Point - The Point in question
	 * @param ConeStartPoint - the beginning of the cone (with the smallest radius)
	 * @param ConeLine - the line out from the start point that ends at the largest radius point of the cone
	 * @param radiusAtStart - the radius at the ConeStartPoint (0 for a 'proper' cone)
	 * @param radiusAtEnd - the largest radius of the cone
	 * @param percentageOut - output variable the holds how much within the cone the point is (1 = on center line, 0 = on exact edge or outside cone).
	 *
	 * @return true if the point is within the cone, false otherwise.
	 */
	static CORE_API bool GetDistanceWithinConeSegment(FVector Point, FVector ConeStartPoint, FVector ConeLine, float RadiusAtStart, float RadiusAtEnd, float &PercentageOut);

	/**
	 * Determines whether a given set of points are coplanar, with a tolerance. Any three points or less are always coplanar.
	 *
	 * @param Points - The set of points to determine coplanarity for.
	 * @param Tolerance - Larger numbers means more variance is allowed.
	 *
	 * @return Whether the points are relatively coplanar, based on the tolerance
	 */
	static CORE_API bool PointsAreCoplanar(const TArray<FVector>& Points, const float Tolerance = 0.1f);

	/**
	* Converts a floating point number to the nearest integer, equidistant ties go to the value which is closest to an even value: 1.5 becomes 2, 0.5 becomes 0
	* @param F		Floating point value to convert
	* @return		The rounded integer
	*/
	static CORE_API float RoundHalfToEven(float F);
	static CORE_API double RoundHalfToEven(double F);

	/**
	* Converts a floating point number to the nearest integer, equidistant ties go to the value which is further from zero: -0.5 becomes -1.0, 0.5 becomes 1.0
	* @param F		Floating point value to convert
	* @return		The rounded integer
	*/
	static CORE_API float RoundHalfFromZero(float F);
	static CORE_API double RoundHalfFromZero(double F);

	/**
	* Converts a floating point number to the nearest integer, equidistant ties go to the value which is closer to zero: -0.5 becomes 0, 0.5 becomes 0
	* @param F		Floating point value to convert
	* @return		The rounded integer
	*/
	static CORE_API float RoundHalfToZero(float F);
	static CORE_API double RoundHalfToZero(double F);

	/**
	* Converts a floating point number to an integer which is further from zero, "larger" in absolute value: 0.1 becomes 1, -0.1 becomes -1
	* @param F		Floating point value to convert
	* @return		The rounded integer
	*/
	static FORCEINLINE float RoundFromZero(float F)
	{
		return (F < 0.0f) ? FloorToFloat(F) : CeilToFloat(F);
	}

	static FORCEINLINE double RoundFromZero(double F)
	{
		return (F < 0.0) ? FloorToDouble(F) : CeilToDouble(F);
	}

	/**
	* Converts a floating point number to an integer which is closer to zero, "smaller" in absolute value: 0.1 becomes 0, -0.1 becomes 0
	* @param F		Floating point value to convert
	* @return		The rounded integer
	*/
	static FORCEINLINE float RoundToZero(float F)
	{
		return (F < 0.0f) ? CeilToFloat(F) : FloorToFloat(F);
	}

	static FORCEINLINE double RoundToZero(double F)
	{
		return (F < 0.0) ? CeilToDouble(F) : FloorToDouble(F);
	}

	/**
	* Converts a floating point number to an integer which is more negative: 0.1 becomes 0, -0.1 becomes -1
	* @param F		Floating point value to convert
	* @return		The rounded integer
	*/
	static FORCEINLINE float RoundToNegativeInfinity(float F)
	{
		return FloorToFloat(F);
	}

	static FORCEINLINE double RoundToNegativeInfinity(double F)
	{
		return FloorToDouble(F);
	}

	/**
	* Converts a floating point number to an integer which is more positive: 0.1 becomes 1, -0.1 becomes 0
	* @param F		Floating point value to convert
	* @return		The rounded integer
	*/
	static FORCEINLINE float RoundToPositiveInfinity(float F)
	{
		return CeilToFloat(F);
	}

	static FORCEINLINE double RoundToPositiveInfinity(double F)
	{
		return CeilToDouble(F);
	}


	// Formatting functions

	/**
	 * Formats an integer value into a human readable string (i.e. 12345 becomes "12,345")
	 *
	 * @param	Val		The value to use
	 * @return	FString	The human readable string
	 */
	static CORE_API FString FormatIntToHumanReadable(int32 Val);


	// Utilities

	/**
	 * Tests a memory region to see that it's working properly.
	 *
	 * @param BaseAddress	Starting address
	 * @param NumBytes		Number of bytes to test (will be rounded down to a multiple of 4)
	 * @return				true if the memory region passed the test
	 */
	static CORE_API bool MemoryTest( void* BaseAddress, uint32 NumBytes );

	/**
	 * Evaluates a numerical equation.
	 *
	 * Operators and precedence: 1:+- 2:/% 3:* 4:^ 5:&|
	 * Unary: -
	 * Types: Numbers (0-9.), Hex ($0-$f)
	 * Grouping: ( )
	 *
	 * @param	Str			String containing the equation.
	 * @param	OutValue		Pointer to storage for the result.
	 * @return				1 if successful, 0 if equation fails.
	 */
	static CORE_API bool Eval( FString Str, float& OutValue );

	/**
	 * Computes the barycentric coordinates for a given point in a triangle - simpler version
	 *
	 * @param	Point			point to convert to barycentric coordinates (in plane of ABC)
	 * @param	A,B,C			three non-colinear points defining a triangle in CCW
	 * 
	 * @return Vector containing the three weights a,b,c such that Point = a*A + b*B + c*C
	 *							                                or Point = A + b*(B-A) + c*(C-A) = (1-b-c)*A + b*B + c*C
	 */
	static CORE_API FVector GetBaryCentric2D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C);

	/**
	 * Computes the barycentric coordinates for a given point in a triangle
	 *
	 * @param	Point			point to convert to barycentric coordinates (in plane of ABC)
	 * @param	A,B,C			three non-collinear points defining a triangle in CCW
	 * 
	 * @return Vector containing the three weights a,b,c such that Point = a*A + b*B + c*C
	 *							                               or Point = A + b*(B-A) + c*(C-A) = (1-b-c)*A + b*B + c*C
	 */
	static CORE_API FVector ComputeBaryCentric2D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C);

	/**
	 * Computes the barycentric coordinates for a given point on a tetrahedron (3D)
	 *
	 * @param	Point			point to convert to barycentric coordinates
	 * @param	A,B,C,D			four points defining a tetrahedron
	 *
	 * @return Vector containing the four weights a,b,c,d such that Point = a*A + b*B + c*C + d*D
	 */
	static CORE_API FVector4 ComputeBaryCentric3D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C, const FVector& D);

	/** 32 bit values where BitFlag[x] == (1<<x) */
	static CORE_API const uint32 BitFlag[32];

	/** 
	 * Returns a smooth Hermite interpolation between 0 and 1 for the value X (where X ranges between A and B)
	 * Clamped to 0 for X <= A and 1 for X >= B.
	 *
	 * @param A Minimum value of X
	 * @param B Maximum value of X
	 * @param X Parameter
	 *
	 * @return Smoothed value between 0 and 1
	 */
	static float SmoothStep(float A, float B, float X)
	{
		if (X < A)
		{
			return 0.0f;
		}
		else if (X >= B)
		{
			return 1.0f;
		}
		const float InterpFraction = (X - A) / (B - A);
		return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
	}
	
	/**
	 * Get a bit in memory created from bitflags (uint32 Value:1), used for EngineShowFlags,
	 * TestBitFieldFunctions() tests the implementation
	 */
	static inline bool ExtractBoolFromBitfield(uint8* Ptr, uint32 Index)
	{
		uint8* BytePtr = Ptr + Index / 8;
		uint8 Mask = 1 << (Index & 0x7);

		return (*BytePtr & Mask) != 0;
	}

	/**
	 * Set a bit in memory created from bitflags (uint32 Value:1), used for EngineShowFlags,
	 * TestBitFieldFunctions() tests the implementation
	 */
	static inline void SetBoolInBitField(uint8* Ptr, uint32 Index, bool bSet)
	{
		uint8* BytePtr = Ptr + Index / 8;
		uint8 Mask = 1 << (Index & 0x7);

		if(bSet)
		{
			*BytePtr |= Mask;
		}
		else
		{
			*BytePtr &= ~Mask;
		}
	}

	/**
	 * Handy to apply scaling in the editor
	 * @param Dst in and out
	 */
	static CORE_API void ApplyScaleToFloat(float& Dst, const FVector& DeltaScale, float Magnitude = 1.0f);

	// @param x assumed to be in this range: 0..1
	// @return 0..255
	static uint8 Quantize8UnsignedByte(float x)
	{
		// 0..1 -> 0..255
		int32 Ret = (int32)(x * 255.999f);

		check(Ret >= 0);
		check(Ret <= 255);

		return Ret;
	}
	
	// @param x assumed to be in this range: -1..1
	// @return 0..255
	static uint8 Quantize8SignedByte(float x)
	{
		// -1..1 -> 0..1
		float y = x * 0.5f + 0.5f;

		return Quantize8UnsignedByte(y);
	}

	// Use the Euclidean method to find the GCD
	static int32 GreatestCommonDivisor(int32 a, int32 b)
	{
		while (b != 0)
		{
			int32 t = b;
			b = a % b;
			a = t;
		}
		return a;
	}

	// LCM = a/gcd * b
	// a and b are the number we want to find the lcm
	static int32 LeastCommonMultiplier(int32 a, int32 b)
	{
		int32 CurrentGcd = GreatestCommonDivisor(a, b);
		return CurrentGcd == 0 ? 0 : (a / CurrentGcd) * b;
	}
};
