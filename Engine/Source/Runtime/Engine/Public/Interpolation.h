// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Interpolation.h: Matinee related C++ declarations
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/ICursor.h"

class FCanvas;
class FViewport;

/** Struct of data that is passed to the input interface. */
struct FInterpEdInputData
{
	FInterpEdInputData()
		: InputType( 0 ),
		  InputData( 0 ),
		  TempData( NULL ),
		  bCtrlDown( false ),
		  bAltDown( false ),
		  bShiftDown( false ),
		  bCmdDown( false ),
		  MouseStart( 0, 0 ),
		  MouseCurrent( 0, 0 ),
		  PixelsPerSec( 0.0f )
	{
	}

	FInterpEdInputData( int32 InType, int32 InData )
		: InputType( InType ),
		  InputData( InData ),
		  TempData( NULL ),
		  bCtrlDown( false ),
		  bAltDown( false ),
		  bShiftDown( false ),
		  bCmdDown( false ),
		  MouseStart( 0, 0 ),
		  MouseCurrent( 0, 0 ),
		  PixelsPerSec( 0.0f )
	{
	}

	int32 InputType;
	int32 InputData;
	void* TempData;	 // Should only be initialized in StartDrag and should only be deleted in EndDrag!

	// Mouse data - Should be filled in automatically.
	bool bCtrlDown;
	bool bAltDown;
	bool bShiftDown;
	bool bCmdDown;
	FIntPoint MouseStart;
	FIntPoint MouseCurrent;
	float PixelsPerSec;
};

/** Defines a set of functions that provide drag drop functionality for the interp editor classes. */
class FInterpEdInputInterface
{
public:
	/**
	 * @return Returns the mouse cursor to display when this input interface is moused over.
	 */
	virtual EMouseCursor::Type GetMouseCursor(FInterpEdInputData &InputData) {return EMouseCursor::Default;}

	/**
	 * Lets the interface object know that we are beginning a drag operation.
	 */
	virtual void BeginDrag(FInterpEdInputData &InputData) {}

	/**
	 * Lets the interface object know that we are ending a drag operation.
	 */
	virtual void EndDrag(FInterpEdInputData &InputData) {}

	/** @return Whether or not this object can be dropped on. */
	virtual bool AcceptsDropping(FInterpEdInputData &InputData, FInterpEdInputInterface* DragObject) {return false;}

	/**
	 * Called when an object is dragged.
	 */
	virtual void ObjectDragged(FInterpEdInputData& InputData) {};

	/**
	 * Allows the object being dragged to be draw on the canvas.
	 */
	virtual void DrawDragObject(FInterpEdInputData &InputData, FViewport* Viewport, FCanvas* Canvas) {}

	/**
	 * Allows the object being dropped on to draw on the canvas.
	 */
	virtual void DrawDropObject(FInterpEdInputData &InputData, FViewport* Viewport, FCanvas* Canvas) {}

	/** @return Whether or not the object being dragged can be dropped. */
	virtual bool ShouldDropObject(FInterpEdInputData &InputData) {return false;}

	/** @return Returns a UObject pointer of this instance if it also inherits from UObject. */
	virtual UObject* GetUObject() {return NULL;}
};

/** Parameters for drawing interp tracks, used by Matinee */
class FInterpTrackDrawParams
{

public:

	/** This track's index */
	int32 TrackIndex;
	
	/** Track display width */
	int32 TrackWidth;
	
	/** Track display height */
	int32 TrackHeight;
	
	/** The view range start time (within the sequence) */
	float StartTime;
	
	/** Scale of the track window in pixels per second */
	float PixelsPerSec;

	/** Current position of the Matinee time cursor along the timeline */
	float TimeCursorPosition;

	/** Current snap interval (1.0 / frames per second) */
	float SnapAmount;

	/** True if we want frame numbers to be rendered instead of time values where appropriate */
	bool bPreferFrameNumbers;

	/** True if we want time cursor positions drawn for all anim tracks */
	bool bShowTimeCursorPosForAllKeys;

	/** True if the user should be allowed to select using "keyframe bars," such as those for audio tracks */
	bool bAllowKeyframeBarSelection;

	/** True if the user should be allowed to select using keyframe text */
	bool bAllowKeyframeTextSelection;

	/** List of keys that are currently selected */
	TArray<struct FInterpEdSelKey> SelectedKeys;
};

namespace MatineeKeyReduction
{
	// For 1D curves, use this structure to allow selected operators on the float.
	class SFLOAT
	{
	public:
		float f;
		float& operator[](int32 i) { return f; }
		const float& operator[](int32 i) const { return f; }
		SFLOAT operator-(const SFLOAT& g) const { SFLOAT out; out.f = f - g.f; return out; }
		SFLOAT operator+(const SFLOAT& g) const { SFLOAT out; out.f = f + g.f; return out; }
		SFLOAT operator/(const float& g) const { SFLOAT out; out.f = f / g; return out; }
		SFLOAT operator*(const float& g) const { SFLOAT out; out.f = f * g; return out; }
		friend SFLOAT operator*(const float& g, const SFLOAT& Inf) { SFLOAT out; out.f = Inf.f * g; return out; }
	};

	// float-float comparison that allows for a certain error in the floating point values
	// due to floating-point operations never being exact.
	static bool IsEquivalent(float a, float b, float Tolerance = KINDA_SMALL_NUMBER)
	{
		return (a - b) > -Tolerance && (a - b) < Tolerance;
	}

	// A key extracted from a track that may be reduced.
	template <class TYPE, int DIM>
	class MKey
	{
	public:
		float Time;
		TYPE Output;
		TEnumAsByte<EInterpCurveMode> Interpolation;

		bool Smoothness[DIM]; // Only useful for broken Hermite tangents.

		float Evaluate(FInterpCurve<TYPE>& Curve, TYPE& Tolerance)
		{
			TYPE Invalid;
			TYPE Evaluated = Curve.Eval(Time, Invalid);
			float Out = 0.0f;
			for (int32 D = 0; D < DIM; ++D)
			{
				float Difference = (Output[D] - Evaluated[D]) * (Output[D] - Evaluated[D]);
				if (Difference > (Tolerance[D] * Tolerance[D]))
				{
					Out += Difference;
				}
			}
			return FMath::Sqrt(Out);
		}
	};

	// A temporary curve that is going through the key-reduction process.
	template <class TYPE, int DIM>
	class MCurve
	{
	public:
		typedef MKey<TYPE, DIM> KEY;
		FInterpCurve<TYPE> OutputCurve; // The output animation curve.
		TArray<KEY> ControlPoints; // The list of keys to reduce.
		TArray<FIntPoint> SegmentQueue; // The segments to reduce iteratively.
		TYPE Tolerance; // Acceptable tolerance for each of the dimensions.

		float RelativeTolerance; // Comes from the user: 0.05f is the default.
		float IntervalStart, IntervalEnd; // Comes from the user: interval in each to apply the reduction.

		void Reduce()
		{
			int32 ControlPointCount = ControlPoints.Num();

			// Fill in the output values for the curve key already created because they
			// cannot be reduced.
			int32 KeyFillListCount = OutputCurve.Points.Num();
			for ( int32 I = 0; I < KeyFillListCount; ++I )
			{
				float KeyTime = OutputCurve.Points[I].InVal;
				KEY* ControlPoint = NULL;
				for ( int32 J = 0; J < ControlPointCount; ++J )
				{
					if ( IsEquivalent( ControlPoints[J].Time, KeyTime, 0.001f ) ) // 1ms tolerance
					{
						ControlPoint = &ControlPoints[J];
					}
				}
				check ( ControlPoint != NULL );

				// Copy the control point value to the curve key.
				OutputCurve.Points[I].OutVal = ControlPoint->Output;
				OutputCurve.Points[I].InterpMode = ControlPoint->Interpolation;
			}

			for ( int32 I = 0; I < KeyFillListCount; ++I )
			{
				// Second step: recalculate the tangents.
				// This is done after the above since it requires valid output values on all keys.
				RecalculateTangents(I);
			}

			if ( ControlPointCount < 2 )
			{
				check( ControlPoints.Num() == 1 );
				OutputCurve.AddPoint( ControlPoints[0].Time, ControlPoints[0].Output );
			}
			else
			{
				SegmentQueue.Reserve(ControlPointCount - 1);
				if ( SegmentQueue.Num() == 0 )
				{
					SegmentQueue.Add(FIntPoint(0, ControlPointCount - 1));
				}

				// Iteratively reduce the segments.
				while (SegmentQueue.Num() > 0)
				{
					// Dequeue the first segment.
					FIntPoint Segment = SegmentQueue[0];
					SegmentQueue.RemoveAt(0);

					// Reduce this segment.
					ReduceSegment(Segment.X, Segment.Y);
				}
			}
		}

		void ReduceSegment(int32 StartIndex, int32 EndIndex)
		{
			if (EndIndex - StartIndex < 2) return;

			// Find the segment control point with the largest delta to the current curve segment.
			// Emphasize middle control points, as much as possible.
			int32 MiddleIndex = 0; float MiddleIndexDelta = 0.0f;
			for (int32 CPIndex = StartIndex + 1; CPIndex < EndIndex; ++CPIndex)
			{
				float CPDelta = ControlPoints[CPIndex].Evaluate(OutputCurve, Tolerance);
				if (CPDelta > 0.0f)
				{
					float TimeDelta[2];
					TimeDelta[0] = ControlPoints[CPIndex].Time - ControlPoints[StartIndex].Time;
					TimeDelta[1] = ControlPoints[EndIndex].Time - ControlPoints[CPIndex].Time;
					if (TimeDelta[1] < TimeDelta[0]) TimeDelta[0] = TimeDelta[1];

					CPDelta *= TimeDelta[0];
					if (CPDelta > MiddleIndexDelta)
					{
						MiddleIndex = CPIndex;
						MiddleIndexDelta = CPDelta;
					}
				}
			}

			if (MiddleIndexDelta > 0.0f)
			{
				// Add this point to the curve and re-calculate the tangents.
				int32 PointIndex = OutputCurve.AddPoint(ControlPoints[MiddleIndex].Time, ControlPoints[MiddleIndex].Output);
				OutputCurve.Points[PointIndex].InterpMode = CIM_CurveUser;
				RecalculateTangents(PointIndex);
				if (PointIndex > 0) RecalculateTangents(PointIndex - 1);
				if (PointIndex < OutputCurve.Points.Num() - 1) RecalculateTangents(PointIndex + 1);

				// Schedule the two sub-segments for evaluation.
				if (MiddleIndex - StartIndex >= 2) SegmentQueue.Add(FIntPoint(StartIndex, MiddleIndex));
				if (EndIndex - MiddleIndex >= 2) SegmentQueue.Add(FIntPoint(MiddleIndex, EndIndex));
			}
		}

		void RecalculateTangents(int32 CurvePointIndex)
		{
			// Retrieve the previous and next curve points.
			// Alias the three curve points and the tangents being calculated, for readability.
			int32 PreviousIndex = CurvePointIndex > 0 ? CurvePointIndex - 1 : 0;
			int32 NextIndex = CurvePointIndex < OutputCurve.Points.Num() - 1 ? CurvePointIndex + 1 : OutputCurve.Points.Num() - 1;
			FInterpCurvePoint<TYPE>& PreviousPoint = OutputCurve.Points[PreviousIndex];
			FInterpCurvePoint<TYPE>& CurrentPoint = OutputCurve.Points[CurvePointIndex];
			FInterpCurvePoint<TYPE>& NextPoint = OutputCurve.Points[NextIndex];
			TYPE& InSlope = CurrentPoint.ArriveTangent;
			TYPE& OutSlope = CurrentPoint.LeaveTangent;

			if ( CurrentPoint.InterpMode != CIM_CurveBreak || CurvePointIndex == 0 || CurvePointIndex == OutputCurve.Points.Num() - 1)
			{
				// Check for local minima/maxima on every dimensions
				for ( int32 D = 0; D < DIM; ++D )
				{
					// Average out the slope.
					if ( ( CurrentPoint.OutVal[D] >= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] >= PreviousPoint.OutVal[D] ) // local maxima
						|| ( CurrentPoint.OutVal[D] <= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] <= PreviousPoint.OutVal[D] ) ) // local minima
					{
						InSlope[D] = OutSlope[D] = 0.0f;
					}
					else
					{
						InSlope[D] = OutSlope[D] = (NextPoint.OutVal[D] - PreviousPoint.OutVal[D]) / (NextPoint.InVal - PreviousPoint.InVal);
					}
				}
			}
			else
			{
				KEY* ControlPoint = FindControlPoint( CurrentPoint.InVal );
				check ( ControlPoint != NULL );

				for ( int32 D = 0; D < DIM; ++D )
				{
					if ( ControlPoint->Smoothness[D] )
					{
						if ( ( CurrentPoint.OutVal[D] >= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] >= PreviousPoint.OutVal[D] ) // local maxima
							|| ( CurrentPoint.OutVal[D] <= NextPoint.OutVal[D] && CurrentPoint.OutVal[D] <= PreviousPoint.OutVal[D] ) ) // local minima
						{
							InSlope[D] = OutSlope[D] = 0.0f;
						}
						else
						{
							InSlope[D] = OutSlope[D] = (NextPoint.OutVal[D] - PreviousPoint.OutVal[D]) / (NextPoint.InVal - PreviousPoint.InVal);
						}
					}
					else
					{
						InSlope[D] = (CurrentPoint.OutVal[D] - PreviousPoint.OutVal[D]) /* / (CurrentPoint.InVal - PreviousPoint.InVal) */;
						OutSlope[D] = (NextPoint.OutVal[D] - CurrentPoint.OutVal[D]) /* / (NextPoint.InVal - CurrentPoint.InVal) */;
					}
				}
			}
		}

		// This badly needs to be optimized out.
		KEY* FindControlPoint(float Time)
		{
			int32 CPCount = ControlPoints.Num();
			if (CPCount < 8)
			{
				// Linear search
				for ( int32 I = 0; I < CPCount; ++I )
				{
					if ( IsEquivalent( ControlPoints[I].Time, Time, 0.001f ) ) // 1ms tolerance
					{
						return &ControlPoints[I];
					}
				}
			}
			else
			{
				// Binary search
				int32 Start = 0, End = CPCount;
				for ( int32 Mid = (End + Start) / 2; Start < End; Mid = (End + Start) / 2 )
				{
					if ( IsEquivalent( ControlPoints[Mid].Time, Time, 0.001f ) ) // 1ms tolerance
					{
						return &ControlPoints[Mid];
					}
					else if ( Time < ControlPoints[Mid].Time ) End = Mid;
					else Start = Mid + 1;
				}
			}
			return NULL;
		}

		// Called by the function below, this one is fairly inefficient
		// and needs to be optimized out, later.
		KEY* SortedAddControlPoint(float Time)
		{
			int32 ControlPointCount = ControlPoints.Num();
			int32 InsertionIndex = ControlPointCount;
			for ( int32 I = 0; I < ControlPointCount; ++I )
			{
				KEY& ControlPoint = ControlPoints[I];
				if ( IsEquivalent( ControlPoint.Time, Time, 0.001f ) ) return &ControlPoint; // 1ms tolerance
				else if ( ControlPoint.Time > Time ) { InsertionIndex = I; break; }
			}

			ControlPoints.InsertUninitialized( InsertionIndex, 1 );
			ControlPoints[InsertionIndex].Time = Time;
			ControlPoints[InsertionIndex].Interpolation = CIM_CurveUser;

			// Also look through the segment indices and push them up.
			int32 SegmentCount = SegmentQueue.Num();
			for ( int32 I = 0; I < SegmentCount; ++I )
			{
				FIntPoint& Segment = SegmentQueue(I);
				if ( Segment.X >= InsertionIndex ) ++Segment.X;
				if ( Segment.Y >= InsertionIndex ) ++Segment.Y;
			}

			return &ControlPoints[InsertionIndex]; 
		}

#if 0	// [GLaforte - 26-02-2007] Disabled until I have more time to debug this and implement tangent checking in the key addition code above.

		// Look for the relative maxima and minima within one dimension of a curve's segment.
		template <class TYPE2>
		void FindSegmentExtremes(const FInterpCurve<TYPE2>& OldCurve, int32 PointIndex, int32 Dimension, float& Maxima, float& Minima )
		{
			// Alias the information we are interested in, for readability.
			const FInterpCurvePoint<TYPE2>& StartPoint = OldCurve.Points[PointIndex];
			const FInterpCurvePoint<TYPE2>& EndPoint = OldCurve.Points[PointIndex + 1];
			float StartTime = StartPoint.InVal;
			float EndTime = EndPoint.InVal;
			float SegmentLength = EndTime - StartTime;
			float StartValue = StartPoint.OutVal[Dimension];
			float EndValue = EndPoint.OutVal[Dimension];
			float StartTangent = StartPoint.LeaveTangent[Dimension];
			float EndTangent = StartPoint.ArriveTangent[Dimension];
			float Slope = (EndValue - StartValue) / (EndTime - StartTime);

			// Figure out which form we have, as Hermite tangents on one dimension can only have four forms.
			float MaximaStartRange = StartTime, MaximaEndRange = EndTime;
			float MinimaStartRange = StartTime, MinimaEndRange = EndTime;
			if ( StartTangent > Slope )
			{
				if ( EndTangent < Slope )
				{
					// Form look like: /\/ .
					Maxima = ( 3.0f * StartTime + EndTime ) / 4.0f;
					MaximaEndRange = ( StartTime + EndTime ) / 2.0f;
					Minima = ( StartTime + 3.0f * EndTime ) / 4.0f;
					MinimaStartRange = ( StartTime + EndTime ) / 2.0f;
				}
				else
				{
					// Form look like: /\ .
					Maxima = ( StartTime + EndTime ) / 2.0f;
					Minima = StartTime; // Minimas at both endpoints.
				}
			}
			else
			{
				if ( EndTangent > Slope )
				{
					// Form look like: \/\ .
					Minima = ( 3.0f * StartTime + EndTime ) / 4.0f;
					MinimaEndRange = ( StartTime + EndTime ) / 2.0f;
					Maxima = ( StartTime + 3.0f * EndTime ) / 4.0f;
					MaximaStartRange = ( StartTime + EndTime ) / 2.0f;
				}
				else
				{
					// Form look like: \/ .
					Minima = ( StartTime + EndTime ) / 2.0f;
					Maxima = StartTime; // Maximas at both endpoints.
				}
			}

#define EVAL_CURVE(Time) ( \
	FMath::CubicInterp( StartValue, StartTangent, EndValue, EndTangent, ( Time - StartTime ) / SegmentLength ) \
	- Slope * ( Time - StartTime ) )
#define FIND_POINT(TimeRef, ValueRef, StartRange, EndRange, CompareOperation) \
	float ValueRef = EVAL_CURVE( TimeRef ); \
	float StartRangeValue = EVAL_CURVE( StartRange ); \
	float EndRangeValue = EVAL_CURVE( EndRange ); \
	bool TestLeft = false; /* alternate between reducing on the left and on the right. */ \
	while ( StartRange < EndRange - 0.001f ) { /* 1ms jitter is tolerable. */ \
	if (TestLeft) { \
	float TestTime = ( TimeRef + StartRange ) / 2.0f; \
	float TestValue = EVAL_CURVE( TestTime ); \
	if ( TestValue CompareOperation ValueRef ) { \
	EndRange = Minima; EndRangeValue = ValueRef; \
	ValueRef = TestValue; TimeRef = TestTime; } \
						else { \
						StartRange = TestTime; StartRangeValue = TestValue; \
						TestLeft = false; } } \
					else { \
					float TestTime = ( TimeRef + EndRange ) / 2.0f; \
					float TestValue = EVAL_CURVE( TestTime ); \
					if ( TestValue CompareOperation ValueRef ) { \
					StartRange = TimeRef; StartRangeValue = ValueRef; \
					ValueRef = TestValue; TimeRef = TestTime; } \
						else { \
						EndRange = TestTime; EndRangeValue = TestValue; \
						TestLeft = true; } } }

			if ( Minima > StartTime )
			{
				FIND_POINT(Minima, MinimaValue, MinimaStartRange, MinimaEndRange, <);
				if ( IsEquivalent( MinimaValue, StartValue ) ) Minima = StartTime; // Close enough to flat.
			}
			if ( Maxima > StartTime )
			{
				FIND_POINT(Maxima, MaximaValue, MaximaStartRange, MaximaEndRange, >);
				if ( IsEquivalent( MaximaValue, StartValue ) ) Maxima = StartTime; // Close enough to flat.
			}
		}
#endif // 0		

		bool HasControlPoints()
		{
			return (ControlPoints.Num() > 0);
		}

		template <class TYPE2>
		void CreateControlPoints(const FInterpCurve<TYPE2>& OldCurve, int32 CurveDimensionCount)
		{
			int32 OldCurvePointCount = OldCurve.Points.Num();
			if ( OldCurvePointCount > 0 && ControlPoints.Num() == 0 )
			{
				int32 ReduceSegmentStart = 0;
				bool ReduceSegmentStarted = false;

				ControlPoints.Reserve(OldCurvePointCount);
				for ( int32 I = 0; I < OldCurvePointCount; ++I )
				{
					// Skip points that are not within our interval.
					if (OldCurve.Points[I].InVal < IntervalStart || OldCurve.Points[I].InVal > IntervalEnd)
						continue;

					// Create the control points.
					// Expected value at the control points will be set by the FillControlPoints function.
					int32 ControlPointIndex = ControlPoints.AddUninitialized(1);
					ControlPoints[ControlPointIndex].Time = OldCurve.Points[I].InVal;

					// Check the interpolation values only the first time the keys are processed.
					bool SmoothInterpolation = OldCurve.Points[I].InterpMode == CIM_Linear || OldCurve.Points[I].InterpMode == CIM_CurveAuto || OldCurve.Points[I].InterpMode == CIM_CurveAutoClamped || OldCurve.Points[I].InterpMode == CIM_CurveUser;

					// Possibly that we want to add broken tangents that are equal to the list, but I cannot check for those without having the full 6D data.
					if ( SmoothInterpolation )
					{
						// We only care for STEP and HERMITE interpolations.
						// In the case of HERMITE, we do care whether the tangents are broken or not.
						ControlPoints[ControlPointIndex].Interpolation = CIM_CurveUser;
						ReduceSegmentStarted = true;
					}
					else
					{
						// This control point will be required in the output curve.
						ControlPoints[ControlPointIndex].Interpolation = OldCurve.Points[I].InterpMode;
						if ( ReduceSegmentStarted )
						{
							SegmentQueue.Add(FIntPoint(ReduceSegmentStart, ControlPointIndex));
						}
						ReduceSegmentStart = I;
						ReduceSegmentStarted = false;
					}

					if ( !SmoothInterpolation )
					{
						// When adding these points, the output is intentionally bad but will be fixed up later, in the Reduce function.
						OutputCurve.AddPoint( ControlPoints[ControlPointIndex].Time, TYPE() );
					}
				}

				// Add the first and last control points to the output curve: they are always necessary.
				if ( OutputCurve.Points.Num() == 0 || !IsEquivalent(OutputCurve.Points[0].InVal, ControlPoints[0].Time) )
				{
					OutputCurve.AddPoint( ControlPoints[0].Time, TYPE() );
				}
				if ( !IsEquivalent(OutputCurve.Points.Last().InVal, ControlPoints.Last().Time) )
				{
					OutputCurve.AddPoint( ControlPoints.Last().Time, TYPE() );
				}

				if ( ReduceSegmentStarted )
				{
					SegmentQueue.Add( FIntPoint(ReduceSegmentStart, ControlPoints.Num() - 1) );
				}
			}


#if 0
			// On smooth interpolation segments, look for extra control points to
			// create when dealing with local minima/maxima on wacky tangents.
			for ( int32 Index = 0; Index < OldCurvePointCount - 1; ++Index )
			{
				// Skip segments that do not start within our interval.
				if (OldCurve.Points[I].InVal < IntervalStart || OldCurve.Points[I].InVal > IntervalEnd)
					continue;

				// Only look at curve with tangents
				if ( OldCurve.Points[Index].InterpMode != CIM_Linear && OldCurve.Points[Index].InterpMode != CIM_Constant )
				{
					for ( int32 D = 0; D < CurveDimensionCount; ++D )
					{
						// Find the maxima and the minima for each dimension.
						float Maxima, Minima, StartTime = OldCurve.Points[Index].InVal;
						FindSegmentExtremes( OldCurve, Index, D, Maxima, Minima );

						// If the maxima and minima are valid, attempt to add them to the list of control points.
						// The "SortedAddControlPoint" function will handle duplicates and points that are very close. 
						if ( Minima - StartTime > 0.001f )
						{
							SortedAddControlPoint( Minima );
						}
						if ( Maxima - StartTime > 0.001f )
						{
							SortedAddControlPoint( Maxima );
						}
					}
				}
			}
#endif // 0
		}

		template <class TYPE2>
		void FillControlPoints(const FInterpCurve<TYPE2>& OldCurve, int32 OldCurveDimensionCount, int32 LocalCurveDimensionOffset)
		{
			check ( OldCurveDimensionCount + LocalCurveDimensionOffset <= DIM );
			check ( LocalCurveDimensionOffset >= 0 );

			// For tolerance calculations, keep track of the maximum and minimum values of the affected dimension.
			TYPE MinValue, MaxValue;
			for (int32 I = 0; I < OldCurveDimensionCount; ++I)
			{
				MinValue[I] = BIG_NUMBER;
				MaxValue[I] = -BIG_NUMBER;
			}

			// Skip all points that are before our interval.
			int32 OIndex = 0;
			while (OIndex < OldCurve.Points.Num() && OldCurve.Points[OIndex].InVal < ControlPoints[0].Time)
			{
				++OIndex;
			}

			// Fill the control point values with the information from this curve.
			for (int32 CPIndex = 0; CPIndex < ControlPoints.Num(); ++CPIndex)
			{
				// Check which is the next key to consider.
				if ( OIndex < OldCurve.Points.Num() && IsEquivalent( OldCurve.Points[OIndex].InVal, ControlPoints[CPIndex].Time, 0.01f ) )
				{
					// Simply copy the key over.
					for (int32 I = 0; I < OldCurveDimensionCount; ++I)
					{
						float Value = OldCurve.Points[OIndex].OutVal[I];
						ControlPoints[CPIndex].Output[LocalCurveDimensionOffset + I] = Value;
						if (Value < MinValue[I]) MinValue[I] = Value;
						if (Value > MaxValue[I]) MaxValue[I] = Value;
					}

					// Also check for broken-tangents interpolation. In this case, check for smoothness on all dimensions.
					if ( ControlPoints[CPIndex].Interpolation == CIM_CurveBreak )
					{
						for (int32 I = 0; I < OldCurveDimensionCount; ++I)
						{
							float CurvePointTolerance = OldCurve.Points[OIndex].ArriveTangent[I] * RelativeTolerance; // Keep a pretty large tolerance here.
							if (CurvePointTolerance < 0.0f) CurvePointTolerance = -CurvePointTolerance;
							if (CurvePointTolerance < SMALL_NUMBER) CurvePointTolerance = SMALL_NUMBER;
							bool Smooth = IsEquivalent( OldCurve.Points[OIndex].LeaveTangent[I], OldCurve.Points[OIndex].ArriveTangent[I], CurvePointTolerance );
							ControlPoints[CPIndex].Smoothness[LocalCurveDimensionOffset + I] = Smooth;
						}
					}

					++OIndex;
				}
				else
				{
					// Evaluate the Matinee animation curve at the given time, for all the dimensions.
					TYPE2 DefaultValue;
					TYPE2 EvaluatedPoint = OldCurve.Eval(ControlPoints[CPIndex].Time, DefaultValue);
					for (int32 I = 0; I < OldCurveDimensionCount; ++I)
					{
						float Value = EvaluatedPoint[I];
						ControlPoints[CPIndex].Output[LocalCurveDimensionOffset + I] = Value;
						if (Value < MinValue[I]) MinValue[I] = Value;
						if (Value > MaxValue[I]) MaxValue[I] = Value;
					}
				}
			}

			// Generate the tolerance values.
			// The relative tolerance value now comes from the user.
			for (int32 I = 0; I < OldCurveDimensionCount; ++I)
			{
				Tolerance[LocalCurveDimensionOffset + I] = FMath::Max(RelativeTolerance * (MaxValue[I] - MinValue[I]), (float) KINDA_SMALL_NUMBER);
			}
		}

		template <class TYPE2>
		void CopyCurvePoints(TArray<TYPE2>& NewCurve, int32 NewCurveDimensionCount, int32 LocalCurveDimensionOffset)
		{
			int32 PointCount = OutputCurve.Points.Num();

			// Remove the points that belong to the interval from the NewCurve.
			int32 RemoveStartIndex = -1, RemoveEndIndex = -1;
			for (int32 I = 0; I < NewCurve.Num(); ++I)
			{
				if (RemoveStartIndex == -1 && NewCurve[I].InVal >= IntervalStart)
				{
					RemoveStartIndex = I;
				}
				else if (RemoveEndIndex == -1 && NewCurve[I].InVal > IntervalEnd)
				{
					RemoveEndIndex = I;
					break;
				}
			}
			if (RemoveEndIndex == -1) RemoveEndIndex = NewCurve.Num();
			NewCurve.RemoveAt(RemoveStartIndex, RemoveEndIndex - RemoveStartIndex);

			// Add back into the curve, the new control points generated from the key reduction algorithm.
			NewCurve.InsertUninitialized(RemoveStartIndex, PointCount);
			for (int32 I = 0; I < PointCount; ++I)
			{
				NewCurve[RemoveStartIndex + I].InVal = OutputCurve.Points[I].InVal;
				NewCurve[RemoveStartIndex + I].InterpMode = OutputCurve.Points[I].InterpMode;
				for (int32 J = 0; J < NewCurveDimensionCount; ++J)
				{
					NewCurve[RemoveStartIndex + I].OutVal[J] = OutputCurve.Points[I].OutVal[LocalCurveDimensionOffset + J];
					NewCurve[RemoveStartIndex + I].ArriveTangent[J] = OutputCurve.Points[I].ArriveTangent[LocalCurveDimensionOffset + J];
					NewCurve[RemoveStartIndex + I].LeaveTangent[J] = OutputCurve.Points[I].LeaveTangent[LocalCurveDimensionOffset + J];
				}
			}
		}
	};
};


