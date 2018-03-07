// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.



#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class UCanvas;

 // Define that controls debug drawing
#define ENABLE_DRAW_DEBUG  !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_DRAW_DEBUG

/** Flush persistent lines */
ENGINE_API void FlushPersistentDebugLines(const UWorld* InWorld);
/** Draw a debug line */
ENGINE_API void DrawDebugLine(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug point */
ENGINE_API void DrawDebugPoint(const UWorld* InWorld, FVector const& Position, float Size, FColor const& PointColor, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw directional arrow **/
ENGINE_API void DrawDebugDirectionalArrow(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, float ArrowSize, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug box */
ENGINE_API void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug box with rotation */
ENGINE_API void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, const FQuat& Rotation, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw Debug coordinate system */
ENGINE_API void DrawDebugCoordinateSystem(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw Debug crosshair */
ENGINE_API void DrawDebugCrosshairs(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, const FColor& Color = FColor::White, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0);
/** Draw Debug Circle */
ENGINE_API void DrawDebugCircle(const UWorld* InWorld, const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f, bool bDrawAxis=true);
/** Draw Debug Circle */
ENGINE_API void DrawDebugCircle(const UWorld* InWorld, FVector Center, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f, FVector YAxis=FVector(0.f,1.f,0.f), FVector ZAxis=FVector(0.f,0.f,1.f), bool bDrawAxis=true);
/** Draw Debug 2D donut */
ENGINE_API void DrawDebug2DDonut(const UWorld* InWorld, const FMatrix& TransformMatrix, float InnerRadius, float OuterRadius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug sphere */
ENGINE_API void DrawDebugSphere(const UWorld* InWorld, FVector const& Center, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug cylinder */
ENGINE_API void DrawDebugCylinder(const UWorld* InWorld, FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a debug cone. AngleWidth and AngleHeight are given in radians */
ENGINE_API void DrawDebugCone(const UWorld* InWorld, FVector const& Origin, FVector const& Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FColor const& Color, bool bPersistentLines=false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Used by gameplay when defining a cone by a vertical and horizontal dot products. */
ENGINE_API void DrawDebugAltCone(const UWorld* InWorld, FVector const& Origin, FRotator const& Rotation, float Length, float AngleWidth, float AngleHeight, FColor const& DrawColor, bool bPersistentLines=false, float LifeTime=-1.f, uint8 DepthPriority=0, float Thickness = 0.f);
ENGINE_API void DrawDebugString(const UWorld* InWorld, FVector const& TextLocation, const FString& Text, class AActor* TestBaseActor = NULL, FColor const& TextColor = FColor::White, float Duration = -1.000000, bool bDrawShadow = false);
ENGINE_API void DrawDebugFrustum(const UWorld* InWorld, const FMatrix& FrustumToWorld, FColor const& Color, bool bPersistentLines = false, float LifeTime=-1.f, uint8 DepthPriority = 0, float Thickness = 0.f);
/** Draw a capsule using the LineBatcher */
ENGINE_API void DrawDebugCapsule(const UWorld* InWorld, FVector const& Center, float HalfHeight, float Radius, const FQuat& Rotation, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0);
/** Draw a debug camera shape.  FOV is full angle in degrees. */
ENGINE_API void DrawDebugCamera(const UWorld* InWorld, FVector const& Location, FRotator const& Rotation, float FOVDeg, float Scale=1.f, FColor const& Color=FColor::White, bool bPersistentLines=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
ENGINE_API void FlushDebugStrings(const UWorld* InWorld);

/** Draw a solid box.  Various API's provided for convenience, including ones that use FBox as well as ones that prefer (Center, Extent). **/
/** Draw a Debug box with optional transform.  */
ENGINE_API void DrawDebugSolidBox(const UWorld* InWorld, FBox const& Box, FColor const& Color, const FTransform& Transform=FTransform::Identity, bool bPersistent=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw a Debug box from (Center, Extent) with no rotation (axis-aligned) */
ENGINE_API void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FColor const& Color, bool bPersistent=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
/** Draw a Debug box from (Center, Extent) with specified Rotation */
ENGINE_API void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FQuat const& Rotation, FColor const& Color, bool bPersistent=false, float LifeTime=-1.f, uint8 DepthPriority = 0);

ENGINE_API void DrawDebugMesh(const UWorld* InWorld, TArray<FVector> const& Verts, TArray<int32> const& Indices, FColor const& Color, bool bPersistent=false, float LifeTime=-1.f, uint8 DepthPriority = 0);
ENGINE_API void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, float Size, FColor const& Color, bool bPersistent=false, float LifeTime=-1, uint8 DepthPriority = 0);
ENGINE_API void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, FVector2D const& Extents, FColor const& Color, bool bPersistent=false, float LifeTime=-1, uint8 DepthPriority = 0);

/* Draws a 2D Histogram of size 'DrawSize' based FDebugFloatHistory struct, using DrawTransform for the position in the world. */
ENGINE_API void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FTransform const & DrawTransform, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent = false, float const & LifeTime = -1.f, uint8 const & DepthPriority = 0);

/* Draws a 2D Histogram of size 'DrawSize' based FDebugFloatHistory struct, using DrawLocation for the location in the world, rotation will face camera of first player. */
ENGINE_API void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FVector const & DrawLocation, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent = false, float const & LifeTime = -1.f, uint8 const & DepthPriority = 0);

/**
 * Draws a 2D line on a canvas
 *
 * @param	Canvas		The Canvas to draw on
 * @param	Start		The start of the line in screen space
 * @param	End			The end of the line in screen space
 * @param	LineColor	The color to use for the line
 */

ENGINE_API void DrawDebugCanvas2DLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor);
ENGINE_API void DrawDebugCanvas2DLine(UCanvas* Canvas, const FVector2D& StartPosition, const FVector2D& EndPosition, const FLinearColor& LineColor, const float& LineThickness = 1.f);
ENGINE_API void DrawDebugCanvas2DCircle(UCanvas* Canvas, const FVector2D& Center, float Radius, int32 NumSides, const FLinearColor& LineColor, const float& LineThickness = 1.f);
ENGINE_API void DrawDebugCanvas2DBox(UCanvas* Canvas, const FBox2D& Box, const FLinearColor& LineColor, const float& LineThickness = 1.f);


/**
 * Draws a line on a canvas.
 *
 * @param	Canvas		The Canvas to draw on.
 * @param	Start		The start of the line in world space
 * @param	End			The end of the line in world space
 * @param	LineColor	The color to use for the line
 */

ENGINE_API void DrawDebugCanvasLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor);

/**
 * Draws a circle using lines.
 *
 * @param	Canvas		The Canvas to draw on.
 * @param	Base			Center of the circle.
 * @param	X				X alignment axis to draw along.
 * @param	Y				Y alignment axis to draw along.
 * @param	Color			Color of the circle.
 * @param	Radius			Radius of the circle.
 * @param	NumSides		Numbers of sides that the circle has.
 */
ENGINE_API void DrawDebugCanvasCircle(UCanvas* Canvas, const FVector& Base, const FVector& X, const FVector& Y, FColor Color, float Radius, int32 NumSides);

/**
 * Draws a sphere using circles.
 *
 * @param	Canvas		The Canvas to draw on.
 * @param	Base			Center of the sphere.
 * @param	Color			Color of the sphere.
 * @param	Radius			Radius of the sphere.
 * @param	NumSides		Numbers of sides that the circle has.
 */
ENGINE_API void DrawDebugCanvasWireSphere(UCanvas* Canvas, const FVector& Base, FColor Color, float Radius, int32 NumSides);

/**
 * Draws a wireframe cone
 *
 * @param	Canvas			Canvas to draw on
 * @param	Transform		Generic transform to apply (ex. a local-to-world transform).
 * @param	ConeRadius		Radius of the cone.
 * @param	ConeAngle		Angle of the cone.
 * @param	ConeSides		Numbers of sides that the cone has.
 * @param	Color			Color of the cone.
 */
ENGINE_API void DrawDebugCanvasWireCone(UCanvas* Canvas, const FTransform& Transform, float ConeRadius, float ConeAngle, int32 ConeSides, FColor Color);

#elif !defined(SHIPPING_DRAW_DEBUG_ERROR) || !SHIPPING_DRAW_DEBUG_ERROR

// Empty versions of above functions

FORCEINLINE void FlushPersistentDebugLines(const UWorld* InWorld) {}
FORCEINLINE void DrawDebugLine(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugPoint(const UWorld* InWorld, FVector const& Position, float Size, FColor const& PointColor, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugDirectionalArrow(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, float ArrowSize, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, const FQuat& Rotation, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugCoordinateSystem(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugCrosshairs(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, const FColor& Color = FColor::White, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugCircle(const UWorld* InWorld, const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f, bool bDrawAxis = true) {}
FORCEINLINE void DrawDebugCircle(const UWorld* InWorld, FVector Center, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f, FVector YAxis = FVector(0.f, 1.f, 0.f), FVector ZAxis = FVector(0.f, 0.f, 1.f), bool bDrawAxis = true) {}
FORCEINLINE void DrawDebug2DDonut(const UWorld* InWorld, const FMatrix& TransformMatrix, float InnerRadius, float OuterRadius, int32 Segments, const FColor& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugSphere(const UWorld* InWorld, FVector const& Center, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugCylinder(const UWorld* InWorld, FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugCone(const UWorld* InWorld, FVector const& Origin, FVector const& Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugAltCone(const UWorld* InWorld, FVector const& Origin, FRotator const& Rotation, float Length, float AngleWidth, float AngleHeight, FColor const& DrawColor, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugString(const UWorld* InWorld, FVector const& TextLocation, const FString& Text, class AActor* TestBaseActor = NULL, FColor const& TextColor = FColor::White, float Duration = -1.000000, bool bDrawShadow = false) {}
FORCEINLINE void DrawDebugFrustum(const UWorld* InWorld, const FMatrix& FrustumToWorld, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0.f) {}
FORCEINLINE void DrawDebugCapsule(const UWorld* InWorld, FVector const& Center, float HalfHeight, float Radius, const FQuat& Rotation, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0, float Thickness = 0) {}
FORCEINLINE void DrawDebugCamera(const UWorld* InWorld, FVector const& Location, FRotator const& Rotation, float FOVDeg, float Scale = 1.f, FColor const& Color = FColor::White, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0) {}
FORCEINLINE void FlushDebugStrings(const UWorld* InWorld) {}
FORCEINLINE void DrawDebugSolidBox(const UWorld* InWorld, FBox const& Box, FColor const& Color, const FTransform& Transform = FTransform::Identity, bool bPersistent = false, float LifeTime = -1.f, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FColor const& Color, bool bPersistent = false, float LifeTime = -1.f, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FQuat const& Rotation, FColor const& Color, bool bPersistent = false, float LifeTime = -1.f, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugMesh(const UWorld* InWorld, TArray<FVector> const& Verts, TArray<int32> const& Indices, FColor const& Color, bool bPersistent = false, float LifeTime = -1.f, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, float Size, FColor const& Color, bool bPersistent = false, float LifeTime = -1, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, FVector2D const& Extents, FColor const& Color, bool bPersistent = false, float LifeTime = -1, uint8 DepthPriority = 0) {}
FORCEINLINE void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FTransform const & DrawTransform, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent = false, float const & LifeTime = -1.f, uint8 const & DepthPriority = 0) {}
FORCEINLINE void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FVector const & DrawLocation, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent = false, float const & LifeTime = -1.f, uint8 const & DepthPriority = 0) {}
FORCEINLINE void DrawDebugCanvas2DLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor) {}
FORCEINLINE void DrawDebugCanvas2DLine(UCanvas* Canvas, const FVector2D& StartPosition, const FVector2D& EndPosition, const FLinearColor& LineColor, const float& LineThickness = 1.f) {}
FORCEINLINE void DrawDebugCanvas2DCircle(UCanvas* Canvas, const FVector2D& Center, float Radius, int32 NumSides, const FLinearColor& LineColor, const float& LineThickness = 1.f) {}
FORCEINLINE void DrawDebugCanvas2DBox(UCanvas* Canvas, const FBox2D& Box, const FLinearColor& LineColor, const float& LineThickness = 1.f) {}
FORCEINLINE void DrawDebugCanvasLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor) {}
FORCEINLINE void DrawDebugCanvasCircle(UCanvas* Canvas, const FVector& Base, const FVector& X, const FVector& Y, FColor Color, float Radius, int32 NumSides) {}
FORCEINLINE void DrawDebugCanvasWireSphere(UCanvas* Canvas, const FVector& Base, FColor Color, float Radius, int32 NumSides) {}
FORCEINLINE void DrawDebugCanvasWireCone(UCanvas* Canvas, const FTransform& Transform, float ConeRadius, float ConeAngle, int32 ConeSides, FColor Color) {}

#else

// Remove 'no-op' debug draw commands, users need to make sure they are not calling these functions in Shipping/Test builds or it will generate a compile error

#endif // ENABLE_DRAW_DEBUG
