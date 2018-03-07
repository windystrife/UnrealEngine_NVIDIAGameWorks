// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Math/NumericLimits.h"
#include "Misc/Crc.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Misc/Parse.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Logging/LogMacros.h"
#include "Math/Vector2D.h"
#include "Misc/ByteSwap.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Math/IntVector.h"
#include "Math/Axis.h"

#if PLATFORM_VECTOR_CUBIC_INTERP_SSE
#include "UnrealMathSSE.h"
#endif

/**
 * A vector in 3-D space composed of components (X, Y, Z) with floating point precision.
 */
struct FVector 
{
public:

	/** Vector's X component. */
	float X;

	/** Vector's Y component. */
	float Y;

	/** Vector's Z component. */
	float Z;

public:

	/** A zero vector (0,0,0) */
	static CORE_API const FVector ZeroVector;

	/** One vector (1,1,1) */
	static CORE_API const FVector OneVector;

	/** World up vector (0,0,1) */
	static CORE_API const FVector UpVector;

	/** Unreal forward vector (1,0,0) */
	static CORE_API const FVector ForwardVector;

	/** Unreal right vector (0,1,0) */
	static CORE_API const FVector RightVector;

public:

#if ENABLE_NAN_DIAGNOSTIC
	FORCEINLINE void DiagnosticCheckNaN() const
	{
		if (ContainsNaN())
		{
			logOrEnsureNanError(TEXT("FVector contains NaN: %s"), *ToString());
			*const_cast<FVector*>(this) = ZeroVector;
		}
	}

	FORCEINLINE void DiagnosticCheckNaN(const TCHAR* Message) const
	{
		if (ContainsNaN())
		{
			logOrEnsureNanError(TEXT("%s: FVector contains NaN: %s"), Message, *ToString());
			*const_cast<FVector*>(this) = ZeroVector;
		}
	}
#else
	FORCEINLINE void DiagnosticCheckNaN() const {}
	FORCEINLINE void DiagnosticCheckNaN(const TCHAR* Message) const {}
#endif

	/** Default constructor (no initialization). */
	FORCEINLINE FVector();

	/**
	 * Constructor initializing all components to a single float value.
	 *
	 * @param InF Value to set all components to.
	 */
	explicit FORCEINLINE FVector(float InF);

	/**
	 * Constructor using initial values for each component.
	 *
	 * @param InX X Coordinate.
	 * @param InY Y Coordinate.
	 * @param InZ Z Coordinate.
	 */
	FORCEINLINE FVector(float InX, float InY, float InZ);

	/**
	 * Constructs a vector from an FVector2D and Z value.
	 * 
	 * @param V Vector to copy from.
	 * @param InZ Z Coordinate.
	 */
	explicit FORCEINLINE FVector(const FVector2D V, float InZ);

	/**
	 * Constructor using the XYZ components from a 4D vector.
	 *
	 * @param V 4D Vector to copy from.
	 */
	FORCEINLINE FVector(const FVector4& V);

	/**
	 * Constructs a vector from an FLinearColor.
	 *
	 * @param InColor Color to copy from.
	 */
	explicit FVector(const FLinearColor& InColor);

	/**
	 * Constructs a vector from an FIntVector.
	 *
	 * @param InVector FIntVector to copy from.
	 */
	explicit FVector(FIntVector InVector);

	/**
	 * Constructs a vector from an FIntPoint.
	 *
	 * @param A Int Point used to set X and Y coordinates, Z is set to zero.
	 */
	explicit FVector(FIntPoint A);

	/**
	 * Constructor which initializes all components to zero.
	 *
	 * @param EForceInit Force init enum
	 */
	explicit FORCEINLINE FVector(EForceInit);

#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
	/**
	* Copy another FVector into this one
	*
	* @param Other The other vector.
	* @return Reference to vector after copy.
	*/
	FORCEINLINE FVector& operator=(const FVector& Other);
#endif

	/**
	 * Calculate cross product between this and another vector.
	 *
	 * @param V The other vector.
	 * @return The cross product.
	 */
	FORCEINLINE FVector operator^(const FVector& V) const;

	/**
	 * Calculate the cross product of two vectors.
	 *
	 * @param A The first vector.
	 * @param B The second vector.
	 * @return The cross product.
	 */
	FORCEINLINE static FVector CrossProduct(const FVector& A, const FVector& B);

	/**
	 * Calculate the dot product between this and another vector.
	 *
	 * @param V The other vector.
	 * @return The dot product.
	 */
	FORCEINLINE float operator|(const FVector& V) const;

	/**
	 * Calculate the dot product of two vectors.
	 *
	 * @param A The first vector.
	 * @param B The second vector.
	 * @return The dot product.
	 */
	FORCEINLINE static float DotProduct(const FVector& A, const FVector& B);

	/**
	 * Gets the result of component-wise addition of this and another vector.
	 *
	 * @param V The vector to add to this.
	 * @return The result of vector addition.
	 */
	FORCEINLINE FVector operator+(const FVector& V) const;

	/**
	 * Gets the result of component-wise subtraction of this by another vector.
	 *
	 * @param V The vector to subtract from this.
	 * @return The result of vector subtraction.
	 */
	FORCEINLINE FVector operator-(const FVector& V) const;

	/**
	 * Gets the result of subtracting from each component of the vector.
	 *
	 * @param Bias How much to subtract from each component.
	 * @return The result of subtraction.
	 */
	FORCEINLINE FVector operator-(float Bias) const;

	/**
	 * Gets the result of adding to each component of the vector.
	 *
	 * @param Bias How much to add to each component.
	 * @return The result of addition.
	 */
	FORCEINLINE FVector operator+(float Bias) const;

	/**
	 * Gets the result of scaling the vector (multiplying each component by a value).
	 *
	 * @param Scale What to multiply each component by.
	 * @return The result of multiplication.
	 */
	FORCEINLINE FVector operator*(float Scale) const;

	/**
	 * Gets the result of dividing each component of the vector by a value.
	 *
	 * @param Scale What to divide each component by.
	 * @return The result of division.
	 */
	FVector operator/(float Scale) const;

	/**
	 * Gets the result of component-wise multiplication of this vector by another.
	 *
	 * @param V The vector to multiply with.
	 * @return The result of multiplication.
	 */
	FORCEINLINE FVector operator*(const FVector& V) const;

	/**
	 * Gets the result of component-wise division of this vector by another.
	 *
	 * @param V The vector to divide by.
	 * @return The result of division.
	 */
	FORCEINLINE FVector operator/(const FVector& V) const;

	// Binary comparison operators.

	/**
	 * Check against another vector for equality.
	 *
	 * @param V The vector to check against.
	 * @return true if the vectors are equal, false otherwise.
	 */
	bool operator==(const FVector& V) const;

	/**
	 * Check against another vector for inequality.
	 *
	 * @param V The vector to check against.
	 * @return true if the vectors are not equal, false otherwise.
	 */
	bool operator!=(const FVector& V) const;

	/**
	 * Check against another vector for equality, within specified error limits.
	 *
	 * @param V The vector to check against.
	 * @param Tolerance Error tolerance.
	 * @return true if the vectors are equal within tolerance limits, false otherwise.
	 */
	bool Equals(const FVector& V, float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Checks whether all components of this vector are the same, within a tolerance.
	 *
	 * @param Tolerance Error tolerance.
	 * @return true if the vectors are equal within tolerance limits, false otherwise.
	 */
	bool AllComponentsEqual(float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Get a negated copy of the vector.
	 *
	 * @return A negated copy of the vector.
	 */
	FORCEINLINE FVector operator-() const;

	/**
	 * Adds another vector to this.
	 * Uses component-wise addition.
	 *
	 * @param V Vector to add to this.
	 * @return Copy of the vector after addition.
	 */
	FORCEINLINE FVector operator+=(const FVector& V);

	/**
	 * Subtracts another vector from this.
	 * Uses component-wise subtraction.
	 *
	 * @param V Vector to subtract from this.
	 * @return Copy of the vector after subtraction.
	 */
	FORCEINLINE FVector operator-=(const FVector& V);

	/**
	 * Scales the vector.
	 *
	 * @param Scale Amount to scale this vector by.
	 * @return Copy of the vector after scaling.
	 */
	FORCEINLINE FVector operator*=(float Scale);

	/**
	 * Divides the vector by a number.
	 *
	 * @param V What to divide this vector by.
	 * @return Copy of the vector after division.
	 */
	FVector operator/=(float V);

	/**
	 * Multiplies the vector with another vector, using component-wise multiplication.
	 *
	 * @param V What to multiply this vector with.
	 * @return Copy of the vector after multiplication.
	 */
	FVector operator*=(const FVector& V);

	/**
	 * Divides the vector by another vector, using component-wise division.
	 *
	 * @param V What to divide vector by.
	 * @return Copy of the vector after division.
	 */
	FVector operator/=(const FVector& V);

	/**
	 * Gets specific component of the vector.
	 *
	 * @param Index the index of vector component
	 * @return reference to component.
	 */
	float& operator[](int32 Index);

	/**
	 * Gets specific component of the vector.
	 *
	 * @param Index the index of vector component
	 * @return Copy of the component.
	 */
	float operator[](int32 Index)const;

	/**
	* Gets a specific component of the vector.
	*
	* @param Index The index of the component required.
	*
	* @return Reference to the specified component.
	*/
	float& Component(int32 Index);

	/**
	* Gets a specific component of the vector.
	*
	* @param Index The index of the component required.
	* @return Copy of the specified component.
	*/
	float Component(int32 Index) const;


	/** Get a specific component of the vector, given a specific axis by enum */
	float GetComponentForAxis(EAxis::Type Axis) const;

	/** Set a specified componet of the vector, given a specific axis by enum */
	void SetComponentForAxis(EAxis::Type Axis, float Component);

public:

	// Simple functions.

	/**
	 * Set the values of the vector directly.
	 *
	 * @param InX New X coordinate.
	 * @param InY New Y coordinate.
	 * @param InZ New Z coordinate.
	 */
	void Set(float InX, float InY, float InZ);

	/**
	 * Get the maximum value of the vector's components.
	 *
	 * @return The maximum value of the vector's components.
	 */
	float GetMax() const;

	/**
	 * Get the maximum absolute value of the vector's components.
	 *
	 * @return The maximum absolute value of the vector's components.
	 */
	float GetAbsMax() const;

	/**
	 * Get the minimum value of the vector's components.
	 *
	 * @return The minimum value of the vector's components.
	 */
	float GetMin() const;

	/**
	 * Get the minimum absolute value of the vector's components.
	 *
	 * @return The minimum absolute value of the vector's components.
	 */
	float GetAbsMin() const;

	/** Gets the component-wise min of two vectors. */
	FVector ComponentMin(const FVector& Other) const;

	/** Gets the component-wise max of two vectors. */
	FVector ComponentMax(const FVector& Other) const;

	/**
	 * Get a copy of this vector with absolute value of each component.
	 *
	 * @return A copy of this vector with absolute value of each component.
	 */
	FVector GetAbs() const;

	/**
	 * Get the length (magnitude) of this vector.
	 *
	 * @return The length of this vector.
	 */
	float Size() const;

	/**
	 * Get the squared length of this vector.
	 *
	 * @return The squared length of this vector.
	 */
	float SizeSquared() const;

	/**
	 * Get the length of the 2D components of this vector.
	 *
	 * @return The 2D length of this vector.
	 */
	float Size2D() const ;

	/**
	 * Get the squared length of the 2D components of this vector.
	 *
	 * @return The squared 2D length of this vector.
	 */
	float SizeSquared2D() const ;

	/**
	 * Checks whether vector is near to zero within a specified tolerance.
	 *
	 * @param Tolerance Error tolerance.
	 * @return true if the vector is near to zero, false otherwise.
	 */
	bool IsNearlyZero(float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Checks whether all components of the vector are exactly zero.
	 *
	 * @return true if the vector is exactly zero, false otherwise.
	 */
	bool IsZero() const;

	/**
	 * Normalize this vector in-place if it is larger than a given tolerance. Leaves it unchanged if not.
	 *
	 * @param Tolerance Minimum squared length of vector for normalization.
	 * @return true if the vector was normalized correctly, false otherwise.
	 */
	bool Normalize(float Tolerance=SMALL_NUMBER);

	/**
	 * Checks whether vector is normalized.
	 *
	 * @return true if Normalized, false otherwise.
	 */
	bool IsNormalized() const;

	/**
	 * Util to convert this vector into a unit direction vector and its original length.
	 *
	 * @param OutDir Reference passed in to store unit direction vector.
	 * @param OutLength Reference passed in to store length of the vector.
	 */
	void ToDirectionAndLength(FVector &OutDir, float &OutLength) const;

	/**
	 * Get a copy of the vector as sign only.
	 * Each component is set to +1 or -1, with the sign of zero treated as +1.
	 *
	 * @param A copy of the vector with each component set to +1 or -1
	 */
	FORCEINLINE FVector GetSignVector() const;

	/**
	 * Projects 2D components of vector based on Z.
	 *
	 * @return Projected version of vector based on Z.
	 */
	FVector Projection() const;

	/**
	 * Calculates normalized version of vector without checking for zero length.
	 *
	 * @return Normalized version of vector.
	 * @see GetSafeNormal()
	 */
	FORCEINLINE FVector GetUnsafeNormal() const;

	/**
	 * Gets a copy of this vector snapped to a grid.
	 *
	 * @param GridSz Grid dimension.
	 * @return A copy of this vector snapped to a grid.
	 * @see FMath::GridSnap()
	 */
	FVector GridSnap(const float& GridSz) const;

	/**
	 * Get a copy of this vector, clamped inside of a cube.
	 *
	 * @param Radius Half size of the cube.
	 * @return A copy of this vector, bound by cube.
	 */
	FVector BoundToCube(float Radius) const;

	/** Create a copy of this vector, with its magnitude clamped between Min and Max. */
	FVector GetClampedToSize(float Min, float Max) const;

	/** Create a copy of this vector, with the 2D magnitude clamped between Min and Max. Z is unchanged. */
	FVector GetClampedToSize2D(float Min, float Max) const;

	/** Create a copy of this vector, with its maximum magnitude clamped to MaxSize. */
	FVector GetClampedToMaxSize(float MaxSize) const;

	/** Create a copy of this vector, with the maximum 2D magnitude clamped to MaxSize. Z is unchanged. */
	FVector GetClampedToMaxSize2D(float MaxSize) const;

	/**
	 * Add a vector to this and clamp the result in a cube.
	 *
	 * @param V Vector to add.
	 * @param Radius Half size of the cube.
	 */
	void AddBounded(const FVector& V, float Radius=MAX_int16);

	/**
	 * Gets the reciprocal of this vector, avoiding division by zero.
	 * Zero components are set to BIG_NUMBER.
	 *
	 * @return Reciprocal of this vector.
	 */
	FVector Reciprocal() const;

	/**
	 * Check whether X, Y and Z are nearly equal.
	 *
	 * @param Tolerance Specified Tolerance.
	 * @return true if X == Y == Z within the specified tolerance.
	 */
	bool IsUniform(float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Mirror a vector about a normal vector.
	 *
	 * @param MirrorNormal Normal vector to mirror about.
	 * @return Mirrored vector.
	 */
	FVector MirrorByVector(const FVector& MirrorNormal) const;

	/**
	 * Mirrors a vector about a plane.
	 *
	 * @param Plane Plane to mirror about.
	 * @return Mirrored vector.
	 */
	FVector MirrorByPlane(const FPlane& Plane) const;

	/**
	 * Rotates around Axis (assumes Axis.Size() == 1).
	 *
	 * @param Angle Angle to rotate (in degrees).
	 * @param Axis Axis to rotate around.
	 * @return Rotated Vector.
	 */
	FVector RotateAngleAxis(const float AngleDeg, const FVector& Axis) const;

	/**
	 * Gets a normalized copy of the vector, checking it is safe to do so based on the length.
	 * Returns zero vector if vector length is too small to safely normalize.
	 *
	 * @param Tolerance Minimum squared vector length.
	 * @return A normalized copy if safe, (0,0,0) otherwise.
	 */
	FVector GetSafeNormal(float Tolerance=SMALL_NUMBER) const;

	/**
	 * Gets a normalized copy of the 2D components of the vector, checking it is safe to do so. Z is set to zero. 
	 * Returns zero vector if vector length is too small to normalize.
	 *
	 * @param Tolerance Minimum squared vector length.
	 * @return Normalized copy if safe, otherwise returns zero vector.
	 */
	FVector GetSafeNormal2D(float Tolerance=SMALL_NUMBER) const;

	/**
	 * Returns the cosine of the angle between this vector and another projected onto the XY plane (no Z).
	 *
	 * @param B the other vector to find the 2D cosine of the angle with.
	 * @return The cosine.
	 */
	FORCEINLINE float CosineAngle2D(FVector B) const;

	/**
	 * Gets a copy of this vector projected onto the input vector.
	 *
	 * @param A	Vector to project onto, does not assume it is normalized.
	 * @return Projected vector.
	 */
	FORCEINLINE FVector ProjectOnTo(const FVector& A) const ;

	/**
	 * Gets a copy of this vector projected onto the input vector, which is assumed to be unit length.
	 * 
	 * @param  Normal Vector to project onto (assumed to be unit length).
	 * @return Projected vector.
	 */
	FORCEINLINE FVector ProjectOnToNormal(const FVector& Normal) const;

	/**
	 * Return the FRotator orientation corresponding to the direction in which the vector points.
	 * Sets Yaw and Pitch to the proper numbers, and sets Roll to zero because the roll can't be determined from a vector.
	 *
	 * @return FRotator from the Vector's direction, without any roll.
	 * @see ToOrientationQuat()
	 */
	CORE_API FRotator ToOrientationRotator() const;

	/**
	 * Return the Quaternion orientation corresponding to the direction in which the vector points.
	 * Similar to the FRotator version, returns a result without roll such that it preserves the up vector.
	 *
	 * @note If you don't care about preserving the up vector and just want the most direct rotation, you can use the faster
	 * 'FQuat::FindBetweenVectors(FVector::ForwardVector, YourVector)' or 'FQuat::FindBetweenNormals(...)' if you know the vector is of unit length.
	 *
	 * @return Quaternion from the Vector's direction, without any roll.
	 * @see ToOrientationRotator(), FQuat::FindBetweenVectors()
	 */
	CORE_API FQuat ToOrientationQuat() const;

	/**
	 * Return the FRotator orientation corresponding to the direction in which the vector points.
	 * Sets Yaw and Pitch to the proper numbers, and sets Roll to zero because the roll can't be determined from a vector.
	 * @note Identical to 'ToOrientationRotator()' and preserved for legacy reasons.
	 * @return FRotator from the Vector's direction.
	 * @see ToOrientationRotator(), ToOrientationQuat()
	 */
	CORE_API FRotator Rotation() const;

	/**
	 * Find good arbitrary axis vectors to represent U and V axes of a plane,
	 * using this vector as the normal of the plane.
	 *
	 * @param Axis1 Reference to first axis.
	 * @param Axis2 Reference to second axis.
	 */
	CORE_API void FindBestAxisVectors(FVector& Axis1, FVector& Axis2) const;

	/** When this vector contains Euler angles (degrees), ensure that angles are between +/-180 */
	CORE_API void UnwindEuler();

	/**
	 * Utility to check if there are any non-finite values (NaN or Inf) in this vector.
	 *
	 * @return true if there are any non-finite values in this vector, false otherwise.
	 */
	bool ContainsNaN() const;

	/**
	 * Check if the vector is of unit length, with specified tolerance.
	 *
	 * @param LengthSquaredTolerance Tolerance against squared length.
	 * @return true if the vector is a unit vector within the specified tolerance.
	 */
	FORCEINLINE bool IsUnit(float LengthSquaredTolerance = KINDA_SMALL_NUMBER) const;

	/**
	 * Get a textual representation of this vector.
	 *
	 * @return A string describing the vector.
	 */
	FString ToString() const;

	/**
	* Get a locale aware textual representation of this vector.
	*
	* @return A string describing the vector.
	*/
	FText ToText() const;

	/** Get a short textural representation of this vector, for compact readable logging. */
	FString ToCompactString() const;

	/** Get a short locale aware textural representation of this vector, for compact readable logging. */
	FText ToCompactText() const;

	/**
	 * Initialize this Vector based on an FString. The String is expected to contain X=, Y=, Z=.
	 * The FVector will be bogus when InitFromString returns false.
	 *
	 * @param	InSourceString	FString containing the vector values.
	 * @return true if the X,Y,Z values were read successfully; false otherwise.
	 */
	bool InitFromString(const FString& InSourceString);

	/** 
	 * Converts a Cartesian unit vector into spherical coordinates on the unit sphere.
	 * @return Output Theta will be in the range [0, PI], and output Phi will be in the range [-PI, PI]. 
	 */
	FVector2D UnitCartesianToSpherical() const;

	/**
	 * Convert a direction vector into a 'heading' angle.
	 *
	 * @return 'Heading' angle between +/-PI. 0 is pointing down +X.
	 */
	float HeadingAngle() const;

	/**
	 * Create an orthonormal basis from a basis with at least two orthogonal vectors.
	 * It may change the directions of the X and Y axes to make the basis orthogonal,
	 * but it won't change the direction of the Z axis.
	 * All axes will be normalized.
	 *
	 * @param XAxis The input basis' XAxis, and upon return the orthonormal basis' XAxis.
	 * @param YAxis The input basis' YAxis, and upon return the orthonormal basis' YAxis.
	 * @param ZAxis The input basis' ZAxis, and upon return the orthonormal basis' ZAxis.
	 */
	static CORE_API void CreateOrthonormalBasis(FVector& XAxis,FVector& YAxis,FVector& ZAxis);

	/**
	 * Compare two points and see if they're the same, using a threshold.
	 *
	 * @param P First vector.
	 * @param Q Second vector.
	 * @return Whether points are the same within a threshold. Uses fast distance approximation (linear per-component distance).
	 */
	static bool PointsAreSame(const FVector &P, const FVector &Q);
	
	/**
	 * Compare two points and see if they're within specified distance.
	 *
	 * @param Point1 First vector.
	 * @param Point2 Second vector.
	 * @param Dist Specified distance.
	 * @return Whether two points are within the specified distance. Uses fast distance approximation (linear per-component distance).
	 */
	static bool PointsAreNear(const FVector &Point1, const FVector &Point2, float Dist);

	/**
	 * Calculate the signed distance (in the direction of the normal) between a point and a plane.
	 *
	 * @param Point The Point we are checking.
	 * @param PlaneBase The Base Point in the plane.
	 * @param PlaneNormal The Normal of the plane (assumed to be unit length).
	 * @return Signed distance between point and plane.
	 */
	static float PointPlaneDist(const FVector &Point, const FVector &PlaneBase, const FVector &PlaneNormal);

	/**
	 * Calculate the projection of a point on the given plane.
	 *
	 * @param Point The point to project onto the plane
	 * @param Plane The plane
	 * @return Projection of Point onto Plane
	 */
	static FVector PointPlaneProject(const FVector& Point, const FPlane& Plane);

	/**
	 * Calculate the projection of a point on the plane defined by counter-clockwise (CCW) points A,B,C.
	 *
	 * @param Point The point to project onto the plane
	 * @param A 1st of three points in CCW order defining the plane 
	 * @param B 2nd of three points in CCW order defining the plane
	 * @param C 3rd of three points in CCW order defining the plane
	 * @return Projection of Point onto plane ABC
	 */
	static FVector PointPlaneProject(const FVector& Point, const FVector& A, const FVector& B, const FVector& C);

	/**
	* Calculate the projection of a point on the plane defined by PlaneBase and PlaneNormal.
	*
	* @param Point The point to project onto the plane
	* @param PlaneBase Point on the plane
	* @param PlaneNorm Normal of the plane (assumed to be unit length).
	* @return Projection of Point onto plane
	*/
	static FVector PointPlaneProject(const FVector& Point, const FVector& PlaneBase, const FVector& PlaneNormal);

	/**
	 * Calculate the projection of a vector on the plane defined by PlaneNormal.
	 * 
	 * @param  V The vector to project onto the plane.
	 * @param  PlaneNormal Normal of the plane (assumed to be unit length).
	 * @return Projection of V onto plane.
	 */
	static FVector VectorPlaneProject(const FVector& V, const FVector& PlaneNormal);

	/**
	 * Euclidean distance between two points.
	 *
	 * @param V1 The first point.
	 * @param V2 The second point.
	 * @return The distance between two points.
	 */
	static FORCEINLINE float Dist(const FVector &V1, const FVector &V2);
	static FORCEINLINE float Distance(const FVector &V1, const FVector &V2) { return Dist(V1, V2); }

	/**
	* Euclidean distance between two points in the XY plane (ignoring Z).
	*
	* @param V1 The first point.
	* @param V2 The second point.
	* @return The distance between two points in the XY plane.
	*/
	static FORCEINLINE float DistXY(const FVector &V1, const FVector &V2);
	static FORCEINLINE float Dist2D(const FVector &V1, const FVector &V2) { return DistXY(V1, V2); }

	/**
	 * Squared distance between two points.
	 *
	 * @param V1 The first point.
	 * @param V2 The second point.
	 * @return The squared distance between two points.
	 */
	static FORCEINLINE float DistSquared(const FVector &V1, const FVector &V2);

	/**
	 * Squared distance between two points in the XY plane only.
	 *	
	 * @param V1 The first point.
	 * @param V2 The second point.
	 * @return The squared distance between two points in the XY plane
	 */
	static FORCEINLINE float DistSquaredXY(const FVector &V1, const FVector &V2);
	static FORCEINLINE float DistSquared2D(const FVector &V1, const FVector &V2) { return DistSquaredXY(V1, V2); }
	
	/**
	 * Compute pushout of a box from a plane.
	 *
	 * @param Normal The plane normal.
	 * @param Size The size of the box.
	 * @return Pushout required.
	 */
	static FORCEINLINE float BoxPushOut(const FVector& Normal, const FVector& Size);

	/**
	 * See if two normal vectors are nearly parallel, meaning the angle between them is close to 0 degrees.
	 *
	 * @param  Normal1 First normalized vector.
	 * @param  Normal1 Second normalized vector.
	 * @param  ParallelCosineThreshold Normals are parallel if absolute value of dot product (cosine of angle between them) is greater than or equal to this. For example: cos(1.0 degrees).
	 * @return true if vectors are nearly parallel, false otherwise.
	 */
	static bool Parallel(const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL);

	/**
	 * See if two normal vectors are coincident (nearly parallel and point in the same direction).
	 * 
	 * @param  Normal1 First normalized vector.
	 * @param  Normal2 Second normalized vector.
	 * @param  ParallelCosineThreshold Normals are coincident if dot product (cosine of angle between them) is greater than or equal to this. For example: cos(1.0 degrees).
	 * @return true if vectors are coincident (nearly parallel and point in the same direction), false otherwise.
	 */
	static bool Coincident(const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL);

	/**
	 * See if two normal vectors are nearly orthogonal (perpendicular), meaning the angle between them is close to 90 degrees.
	 * 
	 * @param  Normal1 First normalized vector.
	 * @param  Normal2 Second normalized vector.
	 * @param  OrthogonalCosineThreshold Normals are orthogonal if absolute value of dot product (cosine of angle between them) is less than or equal to this. For example: cos(89.0 degrees).
	 * @return true if vectors are orthogonal (perpendicular), false otherwise.
	 */
	static bool Orthogonal(const FVector& Normal1, const FVector& Normal2, float OrthogonalCosineThreshold = THRESH_NORMALS_ARE_ORTHOGONAL);

	/**
	 * See if two planes are coplanar. They are coplanar if the normals are nearly parallel and the planes include the same set of points.
	 *
	 * @param Base1 The base point in the first plane.
	 * @param Normal1 The normal of the first plane.
	 * @param Base2 The base point in the second plane.
	 * @param Normal2 The normal of the second plane.
	 * @param ParallelCosineThreshold Normals are parallel if absolute value of dot product is greater than or equal to this.
	 * @return true if the planes are coplanar, false otherwise.
	 */
	static bool Coplanar(const FVector& Base1, const FVector& Normal1, const FVector& Base2, const FVector& Normal2, float ParallelCosineThreshold = THRESH_NORMALS_ARE_PARALLEL);

	/**
	 * Triple product of three vectors: X dot (Y cross Z).
	 *
	 * @param X The first vector.
	 * @param Y The second vector.
	 * @param Z The third vector.
	 * @return The triple product: X dot (Y cross Z).
	 */
	static float Triple(const FVector& X, const FVector& Y, const FVector& Z);

	/**
	 * Generates a list of sample points on a Bezier curve defined by 2 points.
	 *
	 * @param ControlPoints	Array of 4 FVectors (vert1, controlpoint1, controlpoint2, vert2).
	 * @param NumPoints Number of samples.
	 * @param OutPoints Receives the output samples.
	 * @return The path length.
	 */
	static CORE_API float EvaluateBezier(const FVector* ControlPoints, int32 NumPoints, TArray<FVector>& OutPoints);

	/**
	 * Converts a vector containing radian values to a vector containing degree values.
	 *
	 * @param RadVector	Vector containing radian values
	 * @return Vector  containing degree values
	 */
	static FVector RadiansToDegrees(const FVector& RadVector);

	/**
	 * Converts a vector containing degree values to a vector containing radian values.
	 *
	 * @param DegVector	Vector containing degree values
	 * @return Vector containing radian values
	 */
	static FVector DegreesToRadians(const FVector& DegVector);

	/**
	 * Given a current set of cluster centers, a set of points, iterate N times to move clusters to be central. 
	 *
	 * @param Clusters Reference to array of Clusters.
	 * @param Points Set of points.
	 * @param NumIterations Number of iterations.
	 * @param NumConnectionsToBeValid Sometimes you will have long strings that come off the mass of points
	 * which happen to have been chosen as Cluster starting points.  You want to be able to disregard those.
	 */
	static CORE_API void GenerateClusterCenters(TArray<FVector>& Clusters, const TArray<FVector>& Points, int32 NumIterations, int32 NumConnectionsToBeValid);

	/**
	 * Serializer.
	 *
	 * @param Ar Serialization Archive.
	 * @param V Vector to serialize.
	 * @return Reference to Archive after serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, FVector& V)
	{
		// @warning BulkSerialize: FVector is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		return Ar << V.X << V.Y << V.Z;
	}
	
	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	/** 
	 * Network serialization function.
	 * FVectors NetSerialize without quantization (ie exact values are serialized).
	 *
	 * @see FVector_NetQuantize, FVector_NetQuantize10, FVector_NetQuantize100, FVector_NetQuantizeNormal
	 */
	CORE_API bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};


/* FVector inline functions
 *****************************************************************************/

/**
 * Multiplies a vector by a scaling factor.
 *
 * @param Scale Scaling factor.
 * @param V Vector to scale.
 * @return Result of multiplication.
 */
FORCEINLINE FVector operator*(float Scale, const FVector& V)
{
	return V.operator*(Scale);
}


/**
 * Creates a hash value from a FVector. 
 *
 * @param Vector the vector to create a hash value for
 * @return The hash value from the components
 */
FORCEINLINE uint32 GetTypeHash(const FVector& Vector)
{
	// Note: this assumes there's no padding in FVector that could contain uncompared data.
	return FCrc::MemCrc_DEPRECATED(&Vector,sizeof(Vector));
}


#if PLATFORM_LITTLE_ENDIAN
	#define INTEL_ORDER_VECTOR(x) (x)
#else
	static FORCEINLINE FVector INTEL_ORDER_VECTOR(FVector v)
	{
		return FVector(INTEL_ORDERF(v.X), INTEL_ORDERF(v.Y), INTEL_ORDERF(v.Z));
	}
#endif


/** 
 * Util to calculate distance from a point to a bounding box 
 *
 * @param Mins 3D Point defining the lower values of the axis of the bound box
 * @param Max 3D Point defining the lower values of the axis of the bound box
 * @param Point 3D position of interest
 * @return the distance from the Point to the bounding box.
 */
FORCEINLINE float ComputeSquaredDistanceFromBoxToPoint(const FVector& Mins, const FVector& Maxs, const FVector& Point)
{
	// Accumulates the distance as we iterate axis
	float DistSquared = 0.f;

	// Check each axis for min/max and add the distance accordingly
	// NOTE: Loop manually unrolled for > 2x speed up
	if (Point.X < Mins.X)
	{
		DistSquared += FMath::Square(Point.X - Mins.X);
	}
	else if (Point.X > Maxs.X)
	{
		DistSquared += FMath::Square(Point.X - Maxs.X);
	}
	
	if (Point.Y < Mins.Y)
	{
		DistSquared += FMath::Square(Point.Y - Mins.Y);
	}
	else if (Point.Y > Maxs.Y)
	{
		DistSquared += FMath::Square(Point.Y - Maxs.Y);
	}
	
	if (Point.Z < Mins.Z)
	{
		DistSquared += FMath::Square(Point.Z - Mins.Z);
	}
	else if (Point.Z > Maxs.Z)
	{
		DistSquared += FMath::Square(Point.Z - Maxs.Z);
	}
	
	return DistSquared;
}


FORCEINLINE FVector::FVector(const FVector2D V, float InZ)
	: X(V.X), Y(V.Y), Z(InZ)
{
	DiagnosticCheckNaN();
}

FORCEINLINE FIntVector::FIntVector( FVector InVector )
	: X(FMath::TruncToInt(InVector.X))
	, Y(FMath::TruncToInt(InVector.Y))
	, Z(FMath::TruncToInt(InVector.Z))
{ }

inline FVector FVector::RotateAngleAxis(const float AngleDeg, const FVector& Axis) const
{
	float S, C;
	FMath::SinCos(&S, &C, FMath::DegreesToRadians(AngleDeg));

	const float XX	= Axis.X * Axis.X;
	const float YY	= Axis.Y * Axis.Y;
	const float ZZ	= Axis.Z * Axis.Z;

	const float XY	= Axis.X * Axis.Y;
	const float YZ	= Axis.Y * Axis.Z;
	const float ZX	= Axis.Z * Axis.X;

	const float XS	= Axis.X * S;
	const float YS	= Axis.Y * S;
	const float ZS	= Axis.Z * S;

	const float OMC	= 1.f - C;

	return FVector(
		(OMC * XX + C) * X + (OMC * XY - ZS) * Y + (OMC * ZX + YS) * Z,
		(OMC * XY + ZS) * X + (OMC * YY + C) * Y + (OMC * YZ - XS) * Z,
		(OMC * ZX - YS) * X + (OMC * YZ + XS) * Y + (OMC * ZZ + C) * Z
		);
}

inline bool FVector::PointsAreSame(const FVector &P, const FVector &Q)
{
	float Temp;
	Temp=P.X-Q.X;
	if((Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME))
	{
		Temp=P.Y-Q.Y;
		if((Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME))
		{
			Temp=P.Z-Q.Z;
			if((Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME))
			{
				return true;
			}
		}
	}
	return false;
}

inline bool FVector::PointsAreNear(const FVector &Point1, const FVector &Point2, float Dist)
{
	float Temp;
	Temp=(Point1.X - Point2.X); if (FMath::Abs(Temp)>=Dist) return false;
	Temp=(Point1.Y - Point2.Y); if (FMath::Abs(Temp)>=Dist) return false;
	Temp=(Point1.Z - Point2.Z); if (FMath::Abs(Temp)>=Dist) return false;
	return true;
}

inline float FVector::PointPlaneDist
(
	const FVector &Point,
	const FVector &PlaneBase,
	const FVector &PlaneNormal
)
{
	return (Point - PlaneBase) | PlaneNormal;
}


inline FVector FVector::PointPlaneProject(const FVector& Point, const FVector& PlaneBase, const FVector& PlaneNorm)
{
	//Find the distance of X from the plane
	//Add the distance back along the normal from the point
	return Point - FVector::PointPlaneDist(Point,PlaneBase,PlaneNorm) * PlaneNorm;
}

inline FVector FVector::VectorPlaneProject(const FVector& V, const FVector& PlaneNormal)
{
	return V - V.ProjectOnToNormal(PlaneNormal);
}

inline bool FVector::Parallel(const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold)
{
	const float NormalDot = Normal1 | Normal2;
	return FMath::Abs(NormalDot) >= ParallelCosineThreshold;
}

inline bool FVector::Coincident(const FVector& Normal1, const FVector& Normal2, float ParallelCosineThreshold)
{
	const float NormalDot = Normal1 | Normal2;
	return NormalDot >= ParallelCosineThreshold;
}

inline bool FVector::Orthogonal(const FVector& Normal1, const FVector& Normal2, float OrthogonalCosineThreshold)
{
	const float NormalDot = Normal1 | Normal2;
	return FMath::Abs(NormalDot) <= OrthogonalCosineThreshold;
}

inline bool FVector::Coplanar(const FVector &Base1, const FVector &Normal1, const FVector &Base2, const FVector &Normal2, float ParallelCosineThreshold)
{
	if      (!FVector::Parallel(Normal1,Normal2,ParallelCosineThreshold)) return false;
	else if (FVector::PointPlaneDist (Base2,Base1,Normal1) > THRESH_POINT_ON_PLANE) return false;
	else return true;
}

inline float FVector::Triple(const FVector& X, const FVector& Y, const FVector& Z)
{
	return
	(	(X.X * (Y.Y * Z.Z - Y.Z * Z.Y))
	+	(X.Y * (Y.Z * Z.X - Y.X * Z.Z))
	+	(X.Z * (Y.X * Z.Y - Y.Y * Z.X)));
}

inline FVector FVector::RadiansToDegrees(const FVector& RadVector)
{
	return RadVector * (180.f / PI);
}

inline FVector FVector::DegreesToRadians(const FVector& DegVector)
{
	return DegVector * (PI / 180.f);
}

FORCEINLINE FVector::FVector()
{}

FORCEINLINE FVector::FVector(float InF)
	: X(InF), Y(InF), Z(InF)
{
	DiagnosticCheckNaN();
}

FORCEINLINE FVector::FVector(float InX, float InY, float InZ)
	: X(InX), Y(InY), Z(InZ)
{
	DiagnosticCheckNaN();
}

FORCEINLINE FVector::FVector(const FLinearColor& InColor)
	: X(InColor.R), Y(InColor.G), Z(InColor.B)
{
	DiagnosticCheckNaN();
}

FORCEINLINE FVector::FVector(FIntVector InVector)
	: X(InVector.X), Y(InVector.Y), Z(InVector.Z)
{
	DiagnosticCheckNaN();
}

FORCEINLINE FVector::FVector(FIntPoint A)
	: X(A.X), Y(A.Y), Z(0.f)
{
	DiagnosticCheckNaN();
}

FORCEINLINE FVector::FVector(EForceInit)
	: X(0.0f), Y(0.0f), Z(0.0f)
{
	DiagnosticCheckNaN();
}

#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
FORCEINLINE FVector& FVector::operator=(const FVector& Other)
{
	this->X = Other.X;
	this->Y = Other.Y;
	this->Z = Other.Z;

	DiagnosticCheckNaN();

	return *this;
}
#endif

FORCEINLINE FVector FVector::operator^(const FVector& V) const
{
	return FVector
		(
		Y * V.Z - Z * V.Y,
		Z * V.X - X * V.Z,
		X * V.Y - Y * V.X
		);
}

FORCEINLINE FVector FVector::CrossProduct(const FVector& A, const FVector& B)
{
	return A ^ B;
}

FORCEINLINE float FVector::operator|(const FVector& V) const
{
	return X*V.X + Y*V.Y + Z*V.Z;
}

FORCEINLINE float FVector::DotProduct(const FVector& A, const FVector& B)
{
	return A | B;
}

FORCEINLINE FVector FVector::operator+(const FVector& V) const
{
	return FVector(X + V.X, Y + V.Y, Z + V.Z);
}

FORCEINLINE FVector FVector::operator-(const FVector& V) const
{
	return FVector(X - V.X, Y - V.Y, Z - V.Z);
}

FORCEINLINE FVector FVector::operator-(float Bias) const
{
	return FVector(X - Bias, Y - Bias, Z - Bias);
}

FORCEINLINE FVector FVector::operator+(float Bias) const
{
	return FVector(X + Bias, Y + Bias, Z + Bias);
}

FORCEINLINE FVector FVector::operator*(float Scale) const
{
	return FVector(X * Scale, Y * Scale, Z * Scale);
}

FORCEINLINE FVector FVector::operator/(float Scale) const
{
	const float RScale = 1.f/Scale;
	return FVector(X * RScale, Y * RScale, Z * RScale);
}

FORCEINLINE FVector FVector::operator*(const FVector& V) const
{
	return FVector(X * V.X, Y * V.Y, Z * V.Z);
}

FORCEINLINE FVector FVector::operator/(const FVector& V) const
{
	return FVector(X / V.X, Y / V.Y, Z / V.Z);
}

FORCEINLINE bool FVector::operator==(const FVector& V) const
{
	return X==V.X && Y==V.Y && Z==V.Z;
}

FORCEINLINE bool FVector::operator!=(const FVector& V) const
{
	return X!=V.X || Y!=V.Y || Z!=V.Z;
}

FORCEINLINE bool FVector::Equals(const FVector& V, float Tolerance) const
{
	return FMath::Abs(X-V.X) <= Tolerance && FMath::Abs(Y-V.Y) <= Tolerance && FMath::Abs(Z-V.Z) <= Tolerance;
}

FORCEINLINE bool FVector::AllComponentsEqual(float Tolerance) const
{
	return FMath::Abs(X - Y) <= Tolerance && FMath::Abs(X - Z) <= Tolerance && FMath::Abs(Y - Z) <= Tolerance;
}


FORCEINLINE FVector FVector::operator-() const
{
	return FVector(-X, -Y, -Z);
}


FORCEINLINE FVector FVector::operator+=(const FVector& V)
{
	X += V.X; Y += V.Y; Z += V.Z;
	DiagnosticCheckNaN();
	return *this;
}

FORCEINLINE FVector FVector::operator-=(const FVector& V)
{
	X -= V.X; Y -= V.Y; Z -= V.Z;
	DiagnosticCheckNaN();
	return *this;
}

FORCEINLINE FVector FVector::operator*=(float Scale)
{
	X *= Scale; Y *= Scale; Z *= Scale;
	DiagnosticCheckNaN();
	return *this;
}

FORCEINLINE FVector FVector::operator/=(float V)
{
	const float RV = 1.f/V;
	X *= RV; Y *= RV; Z *= RV;
	DiagnosticCheckNaN();
	return *this;
}

FORCEINLINE FVector FVector::operator*=(const FVector& V)
{
	X *= V.X; Y *= V.Y; Z *= V.Z;
	DiagnosticCheckNaN();
	return *this;
}

FORCEINLINE FVector FVector::operator/=(const FVector& V)
{
	X /= V.X; Y /= V.Y; Z /= V.Z;
	DiagnosticCheckNaN();
	return *this;
}

FORCEINLINE float& FVector::operator[](int32 Index)
{
	check(Index >= 0 && Index < 3);
	if(Index == 0)
	{
		return X;
	}
	else if(Index == 1)
	{
		return Y;
	}
	else
	{
		return Z;
	}
}

FORCEINLINE float FVector::operator[](int32 Index)const
{
	check(Index >= 0 && Index < 3);
	if(Index == 0)
	{
		return X;
	}
	else if(Index == 1)
	{
		return Y;
	}
	else
	{
		return Z;
	}
}

FORCEINLINE void FVector::Set(float InX, float InY, float InZ)
{
	X = InX;
	Y = InY;
	Z = InZ;
	DiagnosticCheckNaN();
}

FORCEINLINE float FVector::GetMax() const
{
	return FMath::Max(FMath::Max(X,Y),Z);
}

FORCEINLINE float FVector::GetAbsMax() const
{
	return FMath::Max(FMath::Max(FMath::Abs(X),FMath::Abs(Y)),FMath::Abs(Z));
}

FORCEINLINE float FVector::GetMin() const
{
	return FMath::Min(FMath::Min(X,Y),Z);
}

FORCEINLINE float FVector::GetAbsMin() const
{
	return FMath::Min(FMath::Min(FMath::Abs(X),FMath::Abs(Y)),FMath::Abs(Z));
}

FORCEINLINE FVector FVector::ComponentMin(const FVector& Other) const
{
	return FVector(FMath::Min(X, Other.X), FMath::Min(Y, Other.Y), FMath::Min(Z, Other.Z));
}

FORCEINLINE FVector FVector::ComponentMax(const FVector& Other) const
{
	return FVector(FMath::Max(X, Other.X), FMath::Max(Y, Other.Y), FMath::Max(Z, Other.Z));
}

FORCEINLINE FVector FVector::GetAbs() const
{
	return FVector(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
}

FORCEINLINE float FVector::Size() const
{
	return FMath::Sqrt(X*X + Y*Y + Z*Z);
}

FORCEINLINE float FVector::SizeSquared() const
{
	return X*X + Y*Y + Z*Z;
}

FORCEINLINE float FVector::Size2D() const 
{
	return FMath::Sqrt(X*X + Y*Y);
}

FORCEINLINE float FVector::SizeSquared2D() const 
{
	return X*X + Y*Y;
}

FORCEINLINE bool FVector::IsNearlyZero(float Tolerance) const
{
	return
		FMath::Abs(X)<=Tolerance
		&&	FMath::Abs(Y)<=Tolerance
		&&	FMath::Abs(Z)<=Tolerance;
}

FORCEINLINE bool FVector::IsZero() const
{
	return X==0.f && Y==0.f && Z==0.f;
}

FORCEINLINE bool FVector::Normalize(float Tolerance)
{
	const float SquareSum = X*X + Y*Y + Z*Z;
	if(SquareSum > Tolerance)
	{
		const float Scale = FMath::InvSqrt(SquareSum);
		X *= Scale; Y *= Scale; Z *= Scale;
		return true;
	}
	return false;
}

FORCEINLINE bool FVector::IsNormalized() const
{
	return (FMath::Abs(1.f - SizeSquared()) < THRESH_VECTOR_NORMALIZED);
}

FORCEINLINE void FVector::ToDirectionAndLength(FVector &OutDir, float &OutLength) const
{
	OutLength = Size();
	if (OutLength > SMALL_NUMBER)
	{
		float OneOverLength = 1.0f/OutLength;
		OutDir = FVector(X*OneOverLength, Y*OneOverLength,
			Z*OneOverLength);
	}
	else
	{
		OutDir = FVector::ZeroVector;
	}
}

FORCEINLINE FVector FVector::GetSignVector() const
{
	return FVector
		(
		FMath::FloatSelect(X, 1.f, -1.f), 
		FMath::FloatSelect(Y, 1.f, -1.f), 
		FMath::FloatSelect(Z, 1.f, -1.f)
		);
}

FORCEINLINE FVector FVector::Projection() const
{
	const float RZ = 1.f/Z;
	return FVector(X*RZ, Y*RZ, 1);
}

FORCEINLINE FVector FVector::GetUnsafeNormal() const
{
	const float Scale = FMath::InvSqrt(X*X+Y*Y+Z*Z);
	return FVector(X*Scale, Y*Scale, Z*Scale);
}

FORCEINLINE FVector FVector::GridSnap(const float& GridSz) const
{
	return FVector(FMath::GridSnap(X, GridSz),FMath::GridSnap(Y, GridSz),FMath::GridSnap(Z, GridSz));
}

FORCEINLINE FVector FVector::BoundToCube(float Radius) const
{
	return FVector
		(
		FMath::Clamp(X,-Radius,Radius),
		FMath::Clamp(Y,-Radius,Radius),
		FMath::Clamp(Z,-Radius,Radius)
		);
}

FORCEINLINE FVector FVector::GetClampedToSize(float Min, float Max) const
{
	float VecSize = Size();
	const FVector VecDir = (VecSize > SMALL_NUMBER) ? (*this/VecSize) : FVector::ZeroVector;

	VecSize = FMath::Clamp(VecSize, Min, Max);

	return VecSize * VecDir;
}

FORCEINLINE FVector FVector::GetClampedToSize2D(float Min, float Max) const
{
	float VecSize2D = Size2D();
	const FVector VecDir = (VecSize2D > SMALL_NUMBER) ? (*this/VecSize2D) : FVector::ZeroVector;

	VecSize2D = FMath::Clamp(VecSize2D, Min, Max);

	return FVector(VecSize2D * VecDir.X, VecSize2D * VecDir.Y, Z);
}

FORCEINLINE FVector FVector::GetClampedToMaxSize(float MaxSize) const
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float VSq = SizeSquared();
	if (VSq > FMath::Square(MaxSize))
	{
		const float Scale = MaxSize * FMath::InvSqrt(VSq);
		return FVector(X*Scale, Y*Scale, Z*Scale);
	}
	else
	{
		return *this;
	}	
}

FORCEINLINE FVector FVector::GetClampedToMaxSize2D(float MaxSize) const
{
	if (MaxSize < KINDA_SMALL_NUMBER)
	{
		return FVector(0.f, 0.f, Z);
	}

	const float VSq2D = SizeSquared2D();
	if (VSq2D > FMath::Square(MaxSize))
	{
		const float Scale = MaxSize * FMath::InvSqrt(VSq2D);
		return FVector(X*Scale, Y*Scale, Z);
	}
	else
	{
		return *this;
	}
}

FORCEINLINE void FVector::AddBounded(const FVector& V, float Radius)
{
	*this = (*this + V).BoundToCube(Radius);
}

FORCEINLINE float& FVector::Component(int32 Index)
{
	return (&X)[Index];
}

FORCEINLINE float FVector::Component(int32 Index) const
{
	return (&X)[Index];
}

FORCEINLINE float FVector::GetComponentForAxis(EAxis::Type Axis) const
{
	switch (Axis)
	{
	case EAxis::X:
		return X;
	case EAxis::Y:
		return Y;
	case EAxis::Z:
		return Z;
	default:
		return 0.f;
	}
}

FORCEINLINE void FVector::SetComponentForAxis(EAxis::Type Axis, float Component)
{
	switch (Axis)
	{
	case EAxis::X:
		X = Component;
		break;
	case EAxis::Y:
		Y = Component;
		break;
	case EAxis::Z:
		Z = Component;
		break;
	}
}

FORCEINLINE FVector FVector::Reciprocal() const
{
	FVector RecVector;
	if (X!=0.f)
	{
		RecVector.X = 1.f/X;
	}
	else 
	{
		RecVector.X = BIG_NUMBER;
	}
	if (Y!=0.f)
	{
		RecVector.Y = 1.f/Y;
	}
	else 
	{
		RecVector.Y = BIG_NUMBER;
	}
	if (Z!=0.f)
	{
		RecVector.Z = 1.f/Z;
	}
	else 
	{
		RecVector.Z = BIG_NUMBER;
	}

	return RecVector;
}




FORCEINLINE bool FVector::IsUniform(float Tolerance) const
{
	return AllComponentsEqual(Tolerance);
}

FORCEINLINE FVector FVector::MirrorByVector(const FVector& MirrorNormal) const
{
	return *this - MirrorNormal * (2.f * (*this | MirrorNormal));
}

FORCEINLINE FVector FVector::GetSafeNormal(float Tolerance) const
{
	const float SquareSum = X*X + Y*Y + Z*Z;

	// Not sure if it's safe to add tolerance in there. Might introduce too many errors
	if(SquareSum == 1.f)
	{
		return *this;
	}		
	else if(SquareSum < Tolerance)
	{
		return FVector::ZeroVector;
	}
	const float Scale = FMath::InvSqrt(SquareSum);
	return FVector(X*Scale, Y*Scale, Z*Scale);
}

FORCEINLINE FVector FVector::GetSafeNormal2D(float Tolerance) const
{
	const float SquareSum = X*X + Y*Y;

	// Not sure if it's safe to add tolerance in there. Might introduce too many errors
	if(SquareSum == 1.f)
	{
		if(Z == 0.f)
		{
			return *this;
		}
		else
		{
			return FVector(X, Y, 0.f);
		}
	}
	else if(SquareSum < Tolerance)
	{
		return FVector::ZeroVector;
	}

	const float Scale = FMath::InvSqrt(SquareSum);
	return FVector(X*Scale, Y*Scale, 0.f);
}

FORCEINLINE float FVector::CosineAngle2D(FVector B) const
{
	FVector A(*this);
	A.Z = 0.0f;
	B.Z = 0.0f;
	A.Normalize();
	B.Normalize();
	return A | B;
}

FORCEINLINE FVector FVector::ProjectOnTo(const FVector& A) const 
{ 
	return (A * ((*this | A) / (A | A))); 
}

FORCEINLINE FVector FVector::ProjectOnToNormal(const FVector& Normal) const
{
	return (Normal * (*this | Normal));
}


FORCEINLINE bool FVector::ContainsNaN() const
{
	return (!FMath::IsFinite(X) || 
			!FMath::IsFinite(Y) ||
			!FMath::IsFinite(Z));
}

FORCEINLINE bool FVector::IsUnit(float LengthSquaredTolerance) const
{
	return FMath::Abs(1.0f - SizeSquared()) < LengthSquaredTolerance;
}

FORCEINLINE FString FVector::ToString() const
{
	return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f"), X, Y, Z);
}

FORCEINLINE FText FVector::ToText() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), X);
	Args.Add(TEXT("Y"), Y);
	Args.Add(TEXT("Z"), Z);

	return FText::Format(NSLOCTEXT("Core", "Vector3", "X={X} Y={Y} Z={Z}"), Args);
}

FORCEINLINE FText FVector::ToCompactText() const
{
	if (IsNearlyZero())
	{
		return NSLOCTEXT("Core", "Vector3_CompactZeroVector", "V(0)");
	}

	const bool XIsNotZero = !FMath::IsNearlyZero(X);
	const bool YIsNotZero = !FMath::IsNearlyZero(Y);
	const bool ZIsNotZero = !FMath::IsNearlyZero(Z);

	FNumberFormattingOptions FormatRules;
	FormatRules.MinimumFractionalDigits = 2;
	FormatRules.MinimumIntegralDigits = 0;

	FFormatNamedArguments Args;
	Args.Add(TEXT("X"), FText::AsNumber(X, &FormatRules));
	Args.Add(TEXT("Y"), FText::AsNumber(Y, &FormatRules));
	Args.Add(TEXT("Z"), FText::AsNumber(Z, &FormatRules));

	if (XIsNotZero && YIsNotZero && ZIsNotZero)
	{
		return FText::Format(NSLOCTEXT("Core", "Vector3_CompactXYZ", "V(X={X}, Y={Y}, Z={Z})"), Args);
	}
	else if (!XIsNotZero && YIsNotZero && ZIsNotZero)
	{
		return FText::Format(NSLOCTEXT("Core", "Vector3_CompactYZ", "V(Y={Y}, Z={Z})"), Args);
	}
	else if (XIsNotZero && !YIsNotZero && ZIsNotZero)
	{
		return FText::Format(NSLOCTEXT("Core", "Vector3_CompactXZ", "V(X={X}, Z={Z})"), Args);
	}
	else if (XIsNotZero && YIsNotZero && !ZIsNotZero)
	{
		return FText::Format(NSLOCTEXT("Core", "Vector3_CompactXY", "V(X={X}, Y={Y})"), Args);
	}
	else if (!XIsNotZero && !YIsNotZero && ZIsNotZero)
	{
		return FText::Format(NSLOCTEXT("Core", "Vector3_CompactZ", "V(Z={Z})"), Args);
	}
	else if (XIsNotZero && !YIsNotZero && !ZIsNotZero)
	{
		return FText::Format(NSLOCTEXT("Core", "Vector3_CompactX", "V(X={X})"), Args);
	}
	else if (!XIsNotZero && YIsNotZero && !ZIsNotZero)
	{
		return FText::Format(NSLOCTEXT("Core", "Vector3_CompactY", "V(Y={Y})"), Args);
	}

	return NSLOCTEXT("Core", "Vector3_CompactZeroVector", "V(0)");
}

FORCEINLINE FString FVector::ToCompactString() const
{
	if(IsNearlyZero())
	{
		return FString::Printf(TEXT("V(0)"));
	}

	FString ReturnString(TEXT("V("));
	bool bIsEmptyString = true;
	if(!FMath::IsNearlyZero(X))
	{
		ReturnString += FString::Printf(TEXT("X=%.2f"), X);
		bIsEmptyString = false;
	}
	if(!FMath::IsNearlyZero(Y))
	{
		if(!bIsEmptyString)
		{
			ReturnString += FString(TEXT(", "));
		}
		ReturnString += FString::Printf(TEXT("Y=%.2f"), Y);
		bIsEmptyString = false;
	}
	if(!FMath::IsNearlyZero(Z))
	{
		if(!bIsEmptyString)
		{
			ReturnString += FString(TEXT(", "));
		}
		ReturnString += FString::Printf(TEXT("Z=%.2f"), Z);
		bIsEmptyString = false;
	}
	ReturnString += FString(TEXT(")"));
	return ReturnString;
}

FORCEINLINE bool FVector::InitFromString(const FString& InSourceString)
{
	X = Y = Z = 0;

	// The initialization is only successful if the X, Y, and Z values can all be parsed from the string
	const bool bSuccessful = FParse::Value(*InSourceString, TEXT("X=") , X) && FParse::Value(*InSourceString, TEXT("Y="), Y) && FParse::Value(*InSourceString, TEXT("Z="), Z);

	return bSuccessful;
}

FORCEINLINE FVector2D FVector::UnitCartesianToSpherical() const
{
	checkSlow(IsUnit());
	const float Theta = FMath::Acos(Z / Size());
	const float Phi = FMath::Atan2(Y, X);
	return FVector2D(Theta, Phi);
}

FORCEINLINE float FVector::HeadingAngle() const
{
	// Project Dir into Z plane.
	FVector PlaneDir = *this;
	PlaneDir.Z = 0.f;
	PlaneDir = PlaneDir.GetSafeNormal();

	float Angle = FMath::Acos(PlaneDir.X);

	if(PlaneDir.Y < 0.0f)
	{
		Angle *= -1.0f;
	}

	return Angle;
}



FORCEINLINE float FVector::Dist(const FVector &V1, const FVector &V2)
{
	return FMath::Sqrt(FVector::DistSquared(V1, V2));
}

FORCEINLINE float FVector::DistXY(const FVector &V1, const FVector &V2)
{
	return FMath::Sqrt(FVector::DistSquaredXY(V1, V2));
}

FORCEINLINE float FVector::DistSquared(const FVector &V1, const FVector &V2)
{
	return FMath::Square(V2.X-V1.X) + FMath::Square(V2.Y-V1.Y) + FMath::Square(V2.Z-V1.Z);
}

FORCEINLINE float FVector::DistSquaredXY(const FVector &V1, const FVector &V2)
{
	return FMath::Square(V2.X-V1.X) + FMath::Square(V2.Y-V1.Y);
}

FORCEINLINE float FVector::BoxPushOut(const FVector& Normal, const FVector& Size)
{
	return FMath::Abs(Normal.X*Size.X) + FMath::Abs(Normal.Y*Size.Y) + FMath::Abs(Normal.Z*Size.Z);
}

/** Component-wise clamp for FVector */
FORCEINLINE FVector ClampVector(const FVector& V, const FVector& Min, const FVector& Max)
{
	return FVector(
		FMath::Clamp(V.X,Min.X,Max.X),
		FMath::Clamp(V.Y,Min.Y,Max.Y),
		FMath::Clamp(V.Z,Min.Z,Max.Z)
		);
}

template <> struct TIsPODType<FVector> { enum { Value = true }; };

/* FMath inline functions
 *****************************************************************************/

inline FVector FMath::LinePlaneIntersection
	(
	const FVector &Point1,
	const FVector &Point2,
	const FVector &PlaneOrigin,
	const FVector &PlaneNormal
	)
{
	return
		Point1
		+	(Point2-Point1)
		*	(((PlaneOrigin - Point1)|PlaneNormal) / ((Point2 - Point1)|PlaneNormal));
}

inline bool FMath::LineSphereIntersection(const FVector& Start,const FVector& Dir,float Length,const FVector& Origin,float Radius)
{
	const FVector	EO = Start - Origin;
	const float		v = (Dir | (Origin - Start));
	const float		disc = Radius * Radius - ((EO | EO) - v * v);

	if(disc >= 0.0f)
	{
		const float	Time = (v - Sqrt(disc)) / Length;

		if(Time >= 0.0f && Time <= 1.0f)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

inline FVector FMath::VRand()
{
	FVector Result;

	float L;

	do
	{
		// Check random vectors in the unit sphere so result is statistically uniform.
		Result.X = FRand() * 2.f - 1.f;
		Result.Y = FRand() * 2.f - 1.f;
		Result.Z = FRand() * 2.f - 1.f;
		L = Result.SizeSquared();
	}
	while(L > 1.0f || L < KINDA_SMALL_NUMBER);

	return Result * (1.0f / Sqrt(L));
}


/* FVector2D inline functions
 *****************************************************************************/

FORCEINLINE FVector2D::FVector2D( const FVector& V )
	: X(V.X), Y(V.Y)
{ 
}

inline FVector FVector2D::SphericalToUnitCartesian() const
{
	const float SinTheta = FMath::Sin(X);
	return FVector(FMath::Cos(Y) * SinTheta, FMath::Sin(Y) * SinTheta, FMath::Cos(X));
}

#if PLATFORM_VECTOR_CUBIC_INTERP_SSE
template<>
FORCEINLINE_DEBUGGABLE FVector FMath::CubicInterp(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, const float& A)
{
	static_assert(PLATFORM_ENABLE_VECTORINTRINSICS == 1, "Requires SSE intrinsics.");
	FVector res;

	const float A2 = A  * A;
	const float A3 = A2 * A;

	float s0 = (2 * A3) - (3 * A2) + 1;
	float s1 = A3 - (2 * A2) + A;
	float s2 = (A3 - A2);
	float s3 = (-2 * A3) + (3 * A2);

	VectorRegister v0 = VectorMultiply(VectorLoadFloat1(&s0), VectorLoadFloat3(&P0));
	VectorRegister v1 = VectorMultiply(VectorLoadFloat1(&s1), VectorLoadFloat3(&T0));
	VectorRegister v2 = VectorMultiply(VectorLoadFloat1(&s2), VectorLoadFloat3(&T1));
	VectorRegister v3 = VectorMultiply(VectorLoadFloat1(&s3), VectorLoadFloat3(&P1));

	VectorStoreFloat3(VectorAdd(VectorAdd(v0, v1), VectorAdd(v2, v3)), &res);

	return res;
}
#endif