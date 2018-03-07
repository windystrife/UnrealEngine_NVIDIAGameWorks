// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#include "DrawDebugHelpers.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Components/LineBatchComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"

#if ENABLE_DRAW_DEBUG

void FlushPersistentDebugLines( const UWorld* InWorld )
{
	if(InWorld && InWorld->PersistentLineBatcher)
	{
		InWorld->PersistentLineBatcher->Flush();
	}
}

ULineBatchComponent* GetDebugLineBatcher( const UWorld* InWorld, bool bPersistentLines, float LifeTime, bool bDepthIsForeground )
{
	return (InWorld ? (bDepthIsForeground ? InWorld->ForegroundLineBatcher : (( bPersistentLines || (LifeTime > 0.f) ) ? InWorld->PersistentLineBatcher : InWorld->LineBatcher)) : NULL);
}

void DrawDebugLine(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			float const LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;
			LineBatcher->DrawLine(LineStart, LineEnd, Color, DepthPriority, Thickness, LineLifeTime);
		}
	}
}

void DrawDebugPoint(const UWorld* InWorld, FVector const& Position, float Size, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			LineBatcher->DrawPoint(Position, Color.ReinterpretAsLinear(), Size, DepthPriority, LifeTime);
		}
	}
}

void DrawDebugDirectionalArrow(const UWorld* InWorld, FVector const& LineStart, FVector const& LineEnd, float ArrowSize, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		if (ArrowSize <= 0)
		{
			ArrowSize = 10.f;
		}

		DrawDebugLine(InWorld, LineStart, LineEnd, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);

		FVector Dir = (LineEnd-LineStart);
		Dir.Normalize();
		FVector Up(0, 0, 1);
		FVector Right = Dir ^ Up;
		if (!Right.IsNormalized())
		{
			Dir.FindBestAxisVectors(Up, Right);
		}
		FVector Origin = FVector::ZeroVector;
		FMatrix TM;
		// get matrix with dir/right/up
		TM.SetAxes(&Dir, &Right, &Up, &Origin);

		// since dir is x direction, my arrow will be pointing +y, -x and -y, -x
		float ArrowSqrt = FMath::Sqrt(ArrowSize);
		FVector ArrowPos;
		DrawDebugLine(InWorld, LineEnd, LineEnd + TM.TransformPosition(FVector(-ArrowSqrt, ArrowSqrt, 0)), Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, LineEnd, LineEnd + TM.TransformPosition(FVector(-ArrowSqrt, -ArrowSqrt, 0)), Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
	}
}

void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Box, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );		
		if(LineBatcher != NULL)
		{
			float LineLifeTime = (LifeTime > 0.f)? LifeTime : LineBatcher->DefaultLifeTime;

			LineBatcher->DrawLine(Center + FVector( Box.X,  Box.Y,  Box.Z), Center + FVector( Box.X, -Box.Y, Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector( Box.X, -Box.Y,  Box.Z), Center + FVector(-Box.X, -Box.Y, Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, -Box.Y,  Box.Z), Center + FVector(-Box.X,  Box.Y, Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X,  Box.Y,  Box.Z), Center + FVector( Box.X,  Box.Y, Box.Z), Color, DepthPriority, Thickness, LineLifeTime);

			LineBatcher->DrawLine(Center + FVector( Box.X,  Box.Y, -Box.Z), Center + FVector( Box.X, -Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector( Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X,  Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X,  Box.Y, -Box.Z), Center + FVector( Box.X,  Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);

			LineBatcher->DrawLine(Center + FVector( Box.X,  Box.Y,  Box.Z), Center + FVector( Box.X,  Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector( Box.X, -Box.Y,  Box.Z), Center + FVector( Box.X, -Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X, -Box.Y,  Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
			LineBatcher->DrawLine(Center + FVector(-Box.X,  Box.Y,  Box.Z), Center + FVector(-Box.X,  Box.Y, -Box.Z), Color, DepthPriority, Thickness, LineLifeTime);
		}
	}
}

void DrawDebugBox(const UWorld* InWorld, FVector const& Center, FVector const& Box, const FQuat& Rotation, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			float const LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;
			TArray<struct FBatchedLine> Lines;

			FTransform const Transform(Rotation);
			FVector Start = Transform.TransformPosition(FVector( Box.X,  Box.Y,  Box.Z));
			FVector End = Transform.TransformPosition(FVector( Box.X, -Box.Y, Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector( Box.X, -Box.Y,  Box.Z));
			End = Transform.TransformPosition(FVector(-Box.X, -Box.Y, Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector(-Box.X, -Box.Y,  Box.Z));
			End = Transform.TransformPosition(FVector(-Box.X,  Box.Y, Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector(-Box.X,  Box.Y,  Box.Z));
			End = Transform.TransformPosition(FVector( Box.X,  Box.Y, Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector( Box.X,  Box.Y, -Box.Z));
			End = Transform.TransformPosition(FVector( Box.X, -Box.Y, -Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector( Box.X, -Box.Y, -Box.Z));
			End = Transform.TransformPosition(FVector(-Box.X, -Box.Y, -Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector(-Box.X, -Box.Y, -Box.Z));
			End = Transform.TransformPosition(FVector(-Box.X,  Box.Y, -Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector(-Box.X,  Box.Y, -Box.Z));
			End = Transform.TransformPosition(FVector( Box.X,  Box.Y, -Box.Z));
			new(Lines )FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector( Box.X,  Box.Y,  Box.Z));
			End = Transform.TransformPosition(FVector( Box.X,  Box.Y, -Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector( Box.X, -Box.Y,  Box.Z));
			End = Transform.TransformPosition(FVector( Box.X, -Box.Y, -Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector(-Box.X, -Box.Y,  Box.Z));
			End = Transform.TransformPosition(FVector(-Box.X, -Box.Y, -Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			Start = Transform.TransformPosition(FVector(-Box.X,  Box.Y,  Box.Z));
			End = Transform.TransformPosition(FVector(-Box.X,  Box.Y, -Box.Z));
			new(Lines) FBatchedLine(Center + Start, Center + End, Color, LineLifeTime, Thickness, DepthPriority);

			LineBatcher->DrawLines(Lines);
		}
	}
}


void DrawDebugMesh(const UWorld* InWorld, TArray<FVector> const& Verts, TArray<int32> const& Indices, FColor const& Color, bool bPersistent, float LifeTime, uint8 DepthPriority)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistent, LifeTime, false );
		if(LineBatcher != NULL)
		{
			float const ActualLifetime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;
			LineBatcher->DrawMesh(Verts, Indices, Color, DepthPriority, ActualLifetime);
		}
	}
}

void DrawDebugSolidBox(const UWorld* InWorld, FBox const& Box, FColor const& Color, const FTransform& Transform, bool bPersistent, float LifeTime, uint8 DepthPriority)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistent, LifeTime, false );
		if(LineBatcher != NULL)
		{
			float const ActualLifetime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;
			LineBatcher->DrawSolidBox(Box, Transform, Color, DepthPriority, ActualLifetime);
		}
	}
}

void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FColor const& Color, bool bPersistent, float LifeTime, uint8 DepthPriority)
{	// No Rotation, so just use identity transform and build the box in the right place!
	FBox Box = FBox::BuildAABB(Center, Extent);

	DrawDebugSolidBox(InWorld, Box, Color, FTransform::Identity, bPersistent, LifeTime, DepthPriority);
}

void DrawDebugSolidBox(const UWorld* InWorld, FVector const& Center, FVector const& Extent, FQuat const& Rotation, FColor const& Color, bool bPersistent, float LifeTime, uint8 DepthPriority)
{
	FTransform Transform(Rotation, Center, FVector(1.0f, 1.0f, 1.0f));	// Build transform from Rotation, Center with uniform scale of 1.0.
	FBox Box = FBox::BuildAABB(FVector::ZeroVector, Extent);	// The Transform handles the Center location, so this box needs to be centered on origin.

	DrawDebugSolidBox(InWorld, Box, Color, Transform, bPersistent, LifeTime, DepthPriority);
}

/** Loc is an anchor point in the world to guide which part of the infinite plane to draw. */
void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, float Size, FColor const& Color, bool bPersistent, float LifeTime, uint8 DepthPriority)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		FVector const ClosestPtOnPlane = Loc - P.PlaneDot(Loc) * P;

		FVector U, V;
		P.FindBestAxisVectors(U, V);
		U *= Size;
		V *= Size;

		TArray<FVector> Verts;
		Verts.AddUninitialized(4);
		Verts[0] = ClosestPtOnPlane + U + V;
		Verts[1] = ClosestPtOnPlane - U + V;
		Verts[2] = ClosestPtOnPlane + U - V;
		Verts[3] = ClosestPtOnPlane - U - V;

		TArray<int32> Indices;
		Indices.AddUninitialized(6);
		Indices[0] = 0; Indices[1] = 2; Indices[2] = 1;
		Indices[3] = 1; Indices[4] = 2; Indices[5] = 3;

		// plane quad
		DrawDebugMesh(InWorld, Verts, Indices, Color, bPersistent, LifeTime, DepthPriority);

		// arrow indicating normal
		DrawDebugDirectionalArrow(InWorld, ClosestPtOnPlane, ClosestPtOnPlane + P * 16.f, 8.f, FColor::White, bPersistent, LifeTime, DepthPriority);
	}
}

ENGINE_API void DrawDebugSolidPlane(const UWorld* InWorld, FPlane const& P, FVector const& Loc, FVector2D const& Extents, FColor const& Color, bool bPersistent/*=false*/, float LifeTime/*=-1*/, uint8 DepthPriority /*= 0*/)
{
	// no debug line drawing on dedicated server
	if(GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		FVector const ClosestPtOnPlane = Loc - P.PlaneDot(Loc) * P;

		FVector U, V;
		P.FindBestAxisVectors(U, V);
		U *= Extents.Y;
		V *= Extents.X;

		TArray<FVector> Verts;
		Verts.AddUninitialized(4);
		Verts[0] = ClosestPtOnPlane + U + V;
		Verts[1] = ClosestPtOnPlane - U + V;
		Verts[2] = ClosestPtOnPlane + U - V;
		Verts[3] = ClosestPtOnPlane - U - V;

		TArray<int32> Indices;
		Indices.AddUninitialized(6);
		Indices[0] = 0; Indices[1] = 2; Indices[2] = 1;
		Indices[3] = 1; Indices[4] = 2; Indices[5] = 3;

		// plane quad
		DrawDebugMesh(InWorld, Verts, Indices, Color, bPersistent, LifeTime, DepthPriority);

		// arrow indicating normal
		DrawDebugDirectionalArrow(InWorld, ClosestPtOnPlane, ClosestPtOnPlane + P * 16.f, 8.f, FColor::White, bPersistent, LifeTime, DepthPriority);
	}
}

void DrawDebugCoordinateSystem(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		FRotationMatrix R(AxisRot);
		FVector const X = R.GetScaledAxis( EAxis::X );
		FVector const Y = R.GetScaledAxis( EAxis::Y );
		FVector const Z = R.GetScaledAxis( EAxis::Z );

		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			LineBatcher->DrawLine(AxisLoc, AxisLoc + X*Scale, FColor::Red, DepthPriority, Thickness, LifeTime );
			LineBatcher->DrawLine(AxisLoc, AxisLoc + Y*Scale, FColor::Green, DepthPriority, Thickness, LifeTime );
			LineBatcher->DrawLine(AxisLoc, AxisLoc + Z*Scale, FColor::Blue, DepthPriority, Thickness, LifeTime );
		}
	}
}

ENGINE_API void DrawDebugCrosshairs(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		FRotationMatrix R(AxisRot);
		FVector const X = 0.5f * R.GetScaledAxis(EAxis::X);
		FVector const Y = 0.5f * R.GetScaledAxis(EAxis::Y);
		FVector const Z = 0.5f * R.GetScaledAxis(EAxis::Z);

		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher(InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground));
		if (LineBatcher != NULL)
		{
			LineBatcher->DrawLine(AxisLoc - X*Scale, AxisLoc + X*Scale, Color, DepthPriority);
			LineBatcher->DrawLine(AxisLoc - Y*Scale, AxisLoc + Y*Scale, Color, DepthPriority);
			LineBatcher->DrawLine(AxisLoc - Z*Scale, AxisLoc + Z*Scale, Color, DepthPriority);
		}
	}
}


static void InternalDrawDebugCircle(const UWorld* InWorld, const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness = 0.f)
{
	// no debug line drawing on dedicated server
	if( GEngine->GetNetMode(InWorld) != NM_DedicatedServer )
	{
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			const float LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;

			// Need at least 4 segments
			Segments = FMath::Max(Segments, 4);
			const float AngleStep = 2.f * PI / float(Segments);

			const FVector Center = TransformMatrix.GetOrigin();
			const FVector AxisY = TransformMatrix.GetScaledAxis( EAxis::Y );
			const FVector AxisZ = TransformMatrix.GetScaledAxis( EAxis::Z );

			TArray<FBatchedLine> Lines;
			Lines.Empty(Segments);

			float Angle = 0.f;
			while( Segments-- )
			{
				const FVector Vertex1 = Center + Radius * (AxisY * FMath::Cos(Angle) + AxisZ * FMath::Sin(Angle));
				Angle += AngleStep;
				const FVector Vertex2 = Center + Radius * (AxisY * FMath::Cos(Angle) + AxisZ * FMath::Sin(Angle));
				Lines.Add(FBatchedLine(Vertex1, Vertex2, Color, LineLifeTime, Thickness, DepthPriority));
			}
			LineBatcher->DrawLines(Lines);
		}
	}
}

void DrawDebugCircle(const UWorld* InWorld, const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness, bool bDrawAxis)
{
	// no debug line drawing on dedicated server
	if( GEngine->GetNetMode(InWorld) != NM_DedicatedServer )
	{
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			const float LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;

			// Need at least 4 segments
			Segments = FMath::Max((Segments - 2) / 2, 4);
			InternalDrawDebugCircle(InWorld, TransformMatrix, Radius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);

			if (bDrawAxis)
			{
				const FVector Center = TransformMatrix.GetOrigin();
				const FVector AxisY = TransformMatrix.GetScaledAxis( EAxis::Y );
				const FVector AxisZ = TransformMatrix.GetScaledAxis( EAxis::Z );

				TArray<FBatchedLine> Lines;
				Lines.Empty(2);
				Lines.Add(FBatchedLine(Center - Radius * AxisY, Center + Radius * AxisY, Color, LineLifeTime, Thickness, DepthPriority));
				Lines.Add(FBatchedLine(Center - Radius * AxisZ, Center + Radius * AxisZ, Color, LineLifeTime, Thickness, DepthPriority));
				LineBatcher->DrawLines(Lines);
			}
		}
	}
}

void DrawDebugCircle(const UWorld* InWorld, FVector Center, float Radius, int32 Segments, const FColor& Color, bool PersistentLines, float LifeTime, uint8 DepthPriority, float Thickness, FVector YAxis, FVector ZAxis, bool bDrawAxis)
{
	FMatrix TM;
	TM.SetOrigin(Center);
	TM.SetAxis(0, FVector(1,0,0));
	TM.SetAxis(1, YAxis);
	TM.SetAxis(2, ZAxis);
	
	DrawDebugCircle(
		InWorld,
		TM,
		Radius,
		Segments,
		Color,
		PersistentLines,
		LifeTime,
		DepthPriority,
		Thickness,
		bDrawAxis
	);
}

void DrawDebug2DDonut(const UWorld* InWorld, const FMatrix& TransformMatrix, float InnerRadius, float OuterRadius, int32 Segments, const FColor& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if( GEngine->GetNetMode(InWorld) != NM_DedicatedServer )
	{
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			const float LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;

			// Need at least 4 segments
			Segments = FMath::Max((Segments - 4) / 2, 4);
			InternalDrawDebugCircle(InWorld, TransformMatrix, InnerRadius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
			InternalDrawDebugCircle(InWorld, TransformMatrix, OuterRadius, Segments, Color, bPersistentLines, LifeTime, DepthPriority, Thickness );
		
			const FVector Center = TransformMatrix.GetOrigin();
			const FVector AxisY = TransformMatrix.GetScaledAxis( EAxis::Y );
			const FVector AxisZ = TransformMatrix.GetScaledAxis( EAxis::Z );

			TArray<FBatchedLine> Lines;
			Lines.Empty(4);
			Lines.Add(FBatchedLine(Center - OuterRadius * AxisY, Center - InnerRadius * AxisY, Color, LineLifeTime, Thickness, DepthPriority));
			Lines.Add(FBatchedLine(Center + OuterRadius * AxisY, Center + InnerRadius * AxisY, Color, LineLifeTime, Thickness, DepthPriority));
			Lines.Add(FBatchedLine(Center - OuterRadius * AxisZ, Center - InnerRadius * AxisZ, Color, LineLifeTime, Thickness, DepthPriority));
			Lines.Add(FBatchedLine(Center + OuterRadius * AxisZ, Center + InnerRadius * AxisZ, Color, LineLifeTime, Thickness, DepthPriority));
			LineBatcher->DrawLines(Lines);
		}
	}
}

void DrawDebugSphere(const UWorld* InWorld, FVector const& Center, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if (LineBatcher != NULL)
		{
			float LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;

			// Need at least 4 segments
			Segments = FMath::Max(Segments, 4);

			FVector Vertex1, Vertex2, Vertex3, Vertex4;
			const float AngleInc = 2.f * PI / float(Segments);
			int32 NumSegmentsY = Segments;
			float Latitude = AngleInc;
			int32 NumSegmentsX;
			float Longitude;
			float SinY1 = 0.0f, CosY1 = 1.0f, SinY2, CosY2;
			float SinX, CosX;

			TArray<FBatchedLine> Lines;
			Lines.Empty(NumSegmentsY * Segments * 2);
			while (NumSegmentsY--)
			{
				SinY2 = FMath::Sin(Latitude);
				CosY2 = FMath::Cos(Latitude);

				Vertex1 = FVector(SinY1, 0.0f, CosY1) * Radius + Center;
				Vertex3 = FVector(SinY2, 0.0f, CosY2) * Radius + Center;
				Longitude = AngleInc;

				NumSegmentsX = Segments;
				while (NumSegmentsX--)
				{
					SinX = FMath::Sin(Longitude);
					CosX = FMath::Cos(Longitude);

					Vertex2 = FVector((CosX * SinY1), (SinX * SinY1), CosY1) * Radius + Center;
					Vertex4 = FVector((CosX * SinY2), (SinX * SinY2), CosY2) * Radius + Center;

					Lines.Add(FBatchedLine(Vertex1, Vertex2, Color, LineLifeTime, Thickness, DepthPriority));
					Lines.Add(FBatchedLine(Vertex1, Vertex3, Color, LineLifeTime, Thickness, DepthPriority));

					Vertex1 = Vertex2;
					Vertex3 = Vertex4;
					Longitude += AngleInc;
				}
				SinY1 = SinY2;
				CosY1 = CosY2;
				Latitude += AngleInc;
			}
			LineBatcher->DrawLines(Lines);
		}
	}
}

void DrawDebugCylinder(const UWorld* InWorld, FVector const& Start, FVector const& End, float Radius, int32 Segments, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// Need at least 4 segments
		Segments = FMath::Max(Segments, 4);

		// Rotate a point around axis to form cylinder segments
		FVector Segment;
		FVector P1, P2, P3, P4;
		const float AngleInc = 360.f / Segments;
		float Angle = AngleInc;

		// Default for Axis is up
		FVector Axis = (End - Start).GetSafeNormal();
		if( Axis.IsZero() )
		{
			Axis = FVector(0.f, 0.f, 1.f);
		}

		FVector Perpendicular;
		FVector Dummy;

		Axis.FindBestAxisVectors(Perpendicular, Dummy);
		
		Segment = Perpendicular.RotateAngleAxis(0, Axis) * Radius;
		P1 = Segment + Start;
		P3 = Segment + End;

		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			const float LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;
			while( Segments-- )
			{
				Segment = Perpendicular.RotateAngleAxis(Angle, Axis) * Radius;
				P2 = Segment + Start;
				P4 = Segment + End;

				LineBatcher->DrawLine(P2, P4, Color, DepthPriority, Thickness, LineLifeTime);
				LineBatcher->DrawLine(P1, P2, Color, DepthPriority, Thickness, LineLifeTime);
				LineBatcher->DrawLine(P3, P4, Color, DepthPriority, Thickness, LineLifeTime);

				P1 = P2;
				P3 = P4;
				Angle += AngleInc;
			}
		}
	}
}

/** Used by gameplay when defining a cone by a vertical and horizontal dot products. */
void DrawDebugAltCone(const UWorld* InWorld, FVector const& Origin, FRotator const& Rotation, float Length, float AngleWidth, float AngleHeight, FColor const& DrawColor, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	FRotationMatrix const RM( Rotation );
	FVector const AxisX = RM.GetScaledAxis( EAxis::X );
	FVector const AxisY = RM.GetScaledAxis( EAxis::Y );
	FVector const AxisZ = RM.GetScaledAxis( EAxis::Z );

	// this means foreground lines can't be persistent 
	ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
	if (LineBatcher != NULL)
	{

		float const LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;

		FVector const EndPoint = Origin + AxisX * Length;
		FVector const Up = FMath::Tan(AngleHeight * 0.5f) * AxisZ * Length;
		FVector const Right = FMath::Tan(AngleWidth * 0.5f) * AxisY * Length;
		FVector const HalfUp = Up * 0.5f;
		FVector const HalfRight = Right * 0.5f;

		TArray<FBatchedLine> Lines;
		Lines.Empty();

		FVector A = EndPoint + Up - Right;
		FVector B = EndPoint + Up + Right;
		FVector C = EndPoint - Up + Right;
		FVector D = EndPoint - Up - Right;

		// Corners
		Lines.Add(FBatchedLine(Origin, A, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, B, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, C, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, D, DrawColor, LineLifeTime, Thickness, DepthPriority));

		// Further most plane/frame
		Lines.Add(FBatchedLine(A, B, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(B, C, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(C, D, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(D, A, DrawColor, LineLifeTime, Thickness, DepthPriority));

		// Mid points
		Lines.Add(FBatchedLine(Origin, EndPoint + Up, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, EndPoint - Up, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, EndPoint + Right, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, EndPoint - Right, DrawColor, LineLifeTime, Thickness, DepthPriority));

		// Inbetween
		Lines.Add(FBatchedLine(Origin, EndPoint + Up - HalfRight, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, EndPoint + Up + HalfRight, DrawColor, LineLifeTime, Thickness, DepthPriority));

		Lines.Add(FBatchedLine(Origin, EndPoint - Up - HalfRight, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, EndPoint - Up + HalfRight, DrawColor, LineLifeTime, Thickness, DepthPriority));

		Lines.Add(FBatchedLine(Origin, EndPoint + Right - HalfUp, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, EndPoint + Right + HalfUp, DrawColor, LineLifeTime, Thickness, DepthPriority));

		Lines.Add(FBatchedLine(Origin, EndPoint - Right - HalfUp, DrawColor, LineLifeTime, Thickness, DepthPriority));
		Lines.Add(FBatchedLine(Origin, EndPoint - Right + HalfUp, DrawColor, LineLifeTime, Thickness, DepthPriority));

		LineBatcher->DrawLines(Lines);
	}
}

void DrawDebugCone(const UWorld* InWorld, FVector const& Origin, FVector const& Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FColor const& DrawColor, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		// Need at least 4 sides
		NumSides = FMath::Max(NumSides, 4);

		const float Angle1 = FMath::Clamp<float>(AngleHeight, (float)KINDA_SMALL_NUMBER, (float)(PI - KINDA_SMALL_NUMBER));
		const float Angle2 = FMath::Clamp<float>(AngleWidth, (float)KINDA_SMALL_NUMBER, (float)(PI - KINDA_SMALL_NUMBER));

		const float SinX_2 = FMath::Sin(0.5f * Angle1);
		const float SinY_2 = FMath::Sin(0.5f * Angle2);

		const float SinSqX_2 = SinX_2 * SinX_2;
		const float SinSqY_2 = SinY_2 * SinY_2;

		const float TanX_2 = FMath::Tan(0.5f * Angle1);
		const float TanY_2 = FMath::Tan(0.5f * Angle2);

		TArray<FVector> ConeVerts;
		ConeVerts.AddUninitialized(NumSides);

		for(int32 i = 0; i < NumSides; i++)
		{
			const float Fraction	= (float)i/(float)(NumSides);
			const float Thi			= 2.f * PI * Fraction;
			const float Phi			= FMath::Atan2(FMath::Sin(Thi)*SinY_2, FMath::Cos(Thi)*SinX_2);
			const float SinPhi		= FMath::Sin(Phi);
			const float CosPhi		= FMath::Cos(Phi);
			const float SinSqPhi	= SinPhi*SinPhi;
			const float CosSqPhi	= CosPhi*CosPhi;

			const float RSq			= SinSqX_2*SinSqY_2 / (SinSqX_2*SinSqPhi + SinSqY_2*CosSqPhi);
			const float R			= FMath::Sqrt(RSq);
			const float Sqr			= FMath::Sqrt(1-RSq);
			const float Alpha		= R*CosPhi;
			const float Beta		= R*SinPhi;

			ConeVerts[i].X = (1 - 2*RSq);
			ConeVerts[i].Y = 2 * Sqr * Alpha;
			ConeVerts[i].Z = 2 * Sqr * Beta;
		}

		// Calculate transform for cone.
		FVector YAxis, ZAxis;
		FVector DirectionNorm = Direction.GetSafeNormal();
		DirectionNorm.FindBestAxisVectors(YAxis, ZAxis);
		const FMatrix ConeToWorld = FScaleMatrix(FVector(Length)) * FMatrix(DirectionNorm, YAxis, ZAxis, Origin);

		// this means foreground lines can't be persistent 
		ULineBatchComponent* const LineBatcher = GetDebugLineBatcher( InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground) );
		if(LineBatcher != NULL)
		{
			float const LineLifeTime = (LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime;

			TArray<FBatchedLine> Lines;
			Lines.Empty(NumSides);

			FVector CurrentPoint, PrevPoint, FirstPoint;
			for(int32 i = 0; i < NumSides; i++)
			{
				CurrentPoint = ConeToWorld.TransformPosition(ConeVerts[i]);
				Lines.Add(FBatchedLine(ConeToWorld.GetOrigin(), CurrentPoint, DrawColor, LineLifeTime, Thickness, DepthPriority));

				// PrevPoint must be defined to draw junctions
				if( i > 0 )
				{
					Lines.Add(FBatchedLine(PrevPoint, CurrentPoint, DrawColor, LineLifeTime, Thickness, DepthPriority));
				}
				else
				{
					FirstPoint = CurrentPoint;
				}

				PrevPoint = CurrentPoint;
			}
			// Connect last junction to first
			Lines.Add(FBatchedLine(CurrentPoint, FirstPoint, DrawColor, LineLifeTime, Thickness, DepthPriority));

			LineBatcher->DrawLines(Lines);
		}
	}
}

void DrawDebugString(const UWorld* InWorld, FVector const& TextLocation, const FString& Text, class AActor* TestBaseActor, FColor const& TextColor, float Duration, bool bDrawShadow)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		check(TestBaseActor == NULL || TestBaseActor->GetWorld() == InWorld);
		AActor* BaseAct = (TestBaseActor != NULL) ? TestBaseActor : InWorld->GetWorldSettings();

		// iterate through the player controller list
		for( FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
		{
			APlayerController* PlayerController = Iterator->Get();
			if (PlayerController->MyHUD && PlayerController->Player)
			{
				PlayerController->MyHUD->AddDebugText(Text, BaseAct, Duration, TextLocation, TextLocation, TextColor, true, (TestBaseActor==NULL), false, nullptr, 1.0f, bDrawShadow);
			}
		}
	}
}

void FlushDebugStrings( const UWorld* InWorld )
{
	// iterate through the controller list
	for( FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		// if it's a player
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController->MyHUD)
		{
			PlayerController->MyHUD->RemoveAllDebugStrings();
		}
	}	
}

void DrawDebugFrustum(const UWorld* InWorld, const FMatrix& FrustumToWorld, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		FVector Vertices[2][2][2];
		for(uint32 Z = 0;Z < 2;Z++)
		{
			for(uint32 Y = 0;Y < 2;Y++)
			{
				for(uint32 X = 0;X < 2;X++)
				{
					FVector4 UnprojectedVertex = FrustumToWorld.TransformFVector4(
						FVector4(
						(X ? -1.0f : 1.0f),
						(Y ? -1.0f : 1.0f),
						(Z ?  0.0f : 1.0f),
						1.0f
						)
						);
					Vertices[X][Y][Z] = FVector(UnprojectedVertex) / UnprojectedVertex.W;
				}
			}
		}

		DrawDebugLine(InWorld, Vertices[0][0][0], Vertices[0][0][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[1][0][0], Vertices[1][0][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[0][1][0], Vertices[0][1][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[1][1][0], Vertices[1][1][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);

		DrawDebugLine(InWorld, Vertices[0][0][0], Vertices[0][1][0],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[1][0][0], Vertices[1][1][0],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[0][0][1], Vertices[0][1][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[1][0][1], Vertices[1][1][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);

		DrawDebugLine(InWorld, Vertices[0][0][0], Vertices[1][0][0],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[0][1][0], Vertices[1][1][0],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[0][0][1], Vertices[1][0][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, Vertices[0][1][1], Vertices[1][1][1],Color,  bPersistentLines, LifeTime, DepthPriority, Thickness);
	}
}



static void DrawHalfCircle(const UWorld* InWorld, const FVector& Base, const FVector& X, const FVector& Y, const FColor& Color, float Radius, int32 NumSides, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	float	AngleDelta = 2.0f * (float)PI / ((float)NumSides);
	FVector	LastVertex = Base + X * Radius;

	for(int32 SideIndex = 0; SideIndex < (NumSides/2); SideIndex++)
	{
		FVector	Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;
		DrawDebugLine(InWorld, LastVertex, Vertex, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		LastVertex = Vertex;
	}	
}

static void DrawCircle(const UWorld* InWorld, const FVector& Base, const FVector& X, const FVector& Y, const FColor& Color, float Radius, int32 NumSides, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	const float	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(int32 SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;
		DrawDebugLine(InWorld, LastVertex, Vertex, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		LastVertex = Vertex;
	}
}

void DrawDebugCapsule(const UWorld* InWorld, FVector const& Center, float HalfHeight, float Radius, const FQuat& Rotation, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness)
{
	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		const int32 DrawCollisionSides = 16;

		FVector Origin = Center;
		FMatrix Axes = FQuatRotationTranslationMatrix(Rotation, FVector::ZeroVector);
		FVector XAxis = Axes.GetScaledAxis( EAxis::X );
		FVector YAxis = Axes.GetScaledAxis( EAxis::Y );
		FVector ZAxis = Axes.GetScaledAxis( EAxis::Z ); 

		// Draw top and bottom circles
		float HalfAxis = FMath::Max<float>(HalfHeight - Radius, 1.f);
		FVector TopEnd = Origin + HalfAxis*ZAxis;
		FVector BottomEnd = Origin - HalfAxis*ZAxis;

		DrawCircle(InWorld, TopEnd, XAxis, YAxis, Color, Radius, DrawCollisionSides, bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawCircle(InWorld, BottomEnd, XAxis, YAxis, Color, Radius, DrawCollisionSides, bPersistentLines, LifeTime, DepthPriority, Thickness);

		// Draw domed caps
		DrawHalfCircle(InWorld, TopEnd, YAxis, ZAxis, Color, Radius, DrawCollisionSides, bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawHalfCircle(InWorld, TopEnd, XAxis, ZAxis, Color, Radius, DrawCollisionSides, bPersistentLines, LifeTime, DepthPriority, Thickness);

		FVector NegZAxis = -ZAxis;

		DrawHalfCircle(InWorld, BottomEnd, YAxis, NegZAxis, Color, Radius, DrawCollisionSides, bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawHalfCircle(InWorld, BottomEnd, XAxis, NegZAxis, Color, Radius, DrawCollisionSides, bPersistentLines, LifeTime, DepthPriority, Thickness);

		// Draw connected lines
		DrawDebugLine(InWorld, TopEnd + Radius*XAxis, BottomEnd + Radius*XAxis, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, TopEnd - Radius*XAxis, BottomEnd - Radius*XAxis, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, TopEnd + Radius*YAxis, BottomEnd + Radius*YAxis, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
		DrawDebugLine(InWorld, TopEnd - Radius*YAxis, BottomEnd - Radius*YAxis, Color, bPersistentLines, LifeTime, DepthPriority, Thickness);
	}
}


void DrawDebugCamera(const UWorld* InWorld, FVector const& Location, FRotator const& Rotation, float FOVDeg, float Scale, FColor const& Color, bool bPersistentLines, float LifeTime, uint8 DepthPriority)
{
	static float BaseScale = 4.f;
	static FVector BaseProportions(2.f, 1.f, 1.5f);

	// no debug line drawing on dedicated server
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		DrawDebugCoordinateSystem(InWorld, Location, Rotation, BaseScale*Scale, bPersistentLines, DepthPriority);
		FVector Extents = BaseProportions * BaseScale * Scale;
		DrawDebugBox(InWorld, Location, Extents, Rotation.Quaternion(), Color, bPersistentLines, LifeTime, DepthPriority);		// lifetime

		// draw "lens" portion
		FRotationTranslationMatrix Axes(Rotation, Location);
		FVector XAxis = Axes.GetScaledAxis( EAxis::X );
		FVector YAxis = Axes.GetScaledAxis( EAxis::Y );
		FVector ZAxis = Axes.GetScaledAxis( EAxis::Z ); 

		FVector LensPoint = Location + XAxis * Extents.X;
		float LensSize = BaseProportions.Z * Scale * BaseScale;
		float HalfLensSize = LensSize * FMath::Tan(FMath::DegreesToRadians(FOVDeg*0.5f));
		FVector Corners[4] = 
		{
			LensPoint + XAxis * LensSize + (YAxis * HalfLensSize) + (ZAxis * HalfLensSize),
			LensPoint + XAxis * LensSize + (YAxis * HalfLensSize) - (ZAxis * HalfLensSize),
			LensPoint + XAxis * LensSize - (YAxis * HalfLensSize) - (ZAxis * HalfLensSize),
			LensPoint + XAxis * LensSize - (YAxis * HalfLensSize) + (ZAxis * HalfLensSize),
		};

		DrawDebugLine(InWorld, LensPoint, Corners[0], Color, bPersistentLines, LifeTime, DepthPriority);
		DrawDebugLine(InWorld, LensPoint, Corners[1], Color, bPersistentLines, LifeTime, DepthPriority);
		DrawDebugLine(InWorld, LensPoint, Corners[2], Color, bPersistentLines, LifeTime, DepthPriority);
		DrawDebugLine(InWorld, LensPoint, Corners[3], Color, bPersistentLines, LifeTime, DepthPriority);

		DrawDebugLine(InWorld, Corners[0], Corners[1], Color, bPersistentLines, LifeTime, DepthPriority);
		DrawDebugLine(InWorld, Corners[1], Corners[2], Color, bPersistentLines, LifeTime, DepthPriority);
		DrawDebugLine(InWorld, Corners[2], Corners[3], Color, bPersistentLines, LifeTime, DepthPriority);
		DrawDebugLine(InWorld, Corners[3], Corners[0], Color, bPersistentLines, LifeTime, DepthPriority);
	}
}


void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FTransform const & DrawTransform, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent, float const & LifeTime, uint8 const & DepthPriority)
{
	int const NumSamples = FloatHistory.GetNumSamples();
	if (NumSamples >= 2)
	{
		FVector DrawLocation = DrawTransform.GetLocation();
		FVector const AxisX = DrawTransform.GetUnitAxis(EAxis::Y);
		FVector const AxisY = DrawTransform.GetUnitAxis(EAxis::Z);
		FVector const AxisXStep = AxisX *  DrawSize.X / float(NumSamples);
		FVector const AxisYStep = AxisY *  DrawSize.Y / FMath::Max(FloatHistory.GetMinMaxRange(), KINDA_SMALL_NUMBER);

		// Frame
		DrawDebugLine(&WorldRef, DrawLocation, DrawLocation + AxisX * DrawSize.X, DrawColor, bPersistent, LifeTime, DepthPriority);
		DrawDebugLine(&WorldRef, DrawLocation, DrawLocation + AxisY * DrawSize.Y, DrawColor, bPersistent, LifeTime, DepthPriority);
		DrawDebugLine(&WorldRef, DrawLocation + AxisY * DrawSize.Y, DrawLocation + AxisX * DrawSize.X + AxisY * DrawSize.Y, DrawColor, bPersistent, LifeTime, DepthPriority);
		DrawDebugLine(&WorldRef, DrawLocation + AxisX * DrawSize.X, DrawLocation + AxisX * DrawSize.X + AxisY * DrawSize.Y, DrawColor, bPersistent, LifeTime, DepthPriority);

		TArray<float> const & Samples = FloatHistory.GetSamples();

		TArray<FVector> Verts;
		Verts.AddUninitialized(NumSamples * 2);

		TArray<int32> Indices;
		Indices.AddUninitialized((NumSamples - 1) * 6);

		Verts[0] = DrawLocation;
		Verts[1] = DrawLocation + AxisYStep * Samples[0];

		for (int HistoryIndex = 1; HistoryIndex < NumSamples; HistoryIndex++)
		{
			DrawLocation += AxisXStep;

			int const VertIndex = (HistoryIndex - 1) * 2;
			Verts[VertIndex + 2] = DrawLocation;
			Verts[VertIndex + 3] = DrawLocation + AxisYStep * FMath::Clamp(Samples[HistoryIndex], FloatHistory.GetMinValue(), FloatHistory.GetMaxValue());

			int const StartIndex = (HistoryIndex - 1) * 6;
			Indices[StartIndex + 0] = VertIndex + 0; Indices[StartIndex + 1] = VertIndex + 1; Indices[StartIndex + 2] = VertIndex + 3;
			Indices[StartIndex + 3] = VertIndex + 0; Indices[StartIndex + 4] = VertIndex + 3; Indices[StartIndex + 5] = VertIndex + 2;
		}

		DrawDebugMesh(&WorldRef, Verts, Indices, DrawColor, bPersistent, LifeTime, DepthPriority);
	}
}

void DrawDebugFloatHistory(UWorld const & WorldRef, FDebugFloatHistory const & FloatHistory, FVector const & DrawLocation, FVector2D const & DrawSize, FColor const & DrawColor, bool const & bPersistent, float const & LifeTime, uint8 const & DepthPriority)
{
	APlayerController * PlayerController = WorldRef.GetGameInstance() != nullptr ? WorldRef.GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	FRotator const DrawRotation = (PlayerController && PlayerController->PlayerCameraManager) ? PlayerController->PlayerCameraManager->CameraCache.POV.Rotation : FRotator(0, 0, 0);

	FTransform const DrawTransform(DrawRotation, DrawLocation);
	DrawDebugFloatHistory(WorldRef, FloatHistory, DrawTransform, DrawSize, DrawColor, bPersistent, LifeTime, DepthPriority);
}

//////////////////////////////////////////////////////////////////
// Debug draw canvas operations

void DrawDebugCanvas2DLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor)
{
	FCanvasLineItem LineItem;
	LineItem.Origin = Start;
	LineItem.EndPos = End;
	LineItem.SetColor(LineColor);

	LineItem.Draw(Canvas->Canvas);
}

void DrawDebugCanvasLine(UCanvas* Canvas, const FVector& Start, const FVector& End, const FLinearColor& LineColor)
{
	DrawDebugCanvas2DLine(Canvas, Canvas->Project(Start), Canvas->Project(End), LineColor);
}

void DrawDebugCanvasCircle(UCanvas* Canvas, const FVector& Base, const FVector& X, const FVector& Y, FColor Color, float Radius, int32 NumSides)
{
	const float	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(int32 SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;
		DrawDebugCanvasLine(Canvas, LastVertex, Vertex, Color);
		LastVertex = Vertex;
	}
}

void DrawDebugCanvasWireSphere(UCanvas* Canvas, const FVector& Base, FColor Color, float Radius, int32 NumSides)
{
	DrawDebugCanvasCircle(Canvas, Base, FVector(1,0,0), FVector(0,1,0), Color, Radius, NumSides);
	DrawDebugCanvasCircle(Canvas, Base, FVector(1,0,0), FVector(0,0,1), Color, Radius, NumSides);
	DrawDebugCanvasCircle(Canvas, Base, FVector(0,1,0), FVector(0,0,1), Color, Radius, NumSides);
}

void DrawDebugCanvasWireCone(UCanvas* Canvas, const FTransform& Transform, float ConeRadius, float ConeAngle, int32 ConeSides, FColor Color)
{
	static const float TwoPI = 2.0f * PI;
	static const float ToRads = PI / 180.0f;
	static const float MaxAngle = 89.0f * ToRads + 0.001f;
	const float ClampedConeAngle = FMath::Clamp(ConeAngle * ToRads, 0.001f, MaxAngle);
	const float SinClampedConeAngle = FMath::Sin( ClampedConeAngle );
	const float CosClampedConeAngle = FMath::Cos( ClampedConeAngle );
	const FVector ConeDirection(1,0,0);
	const FVector ConeUpVector(0,1,0);
	const FVector ConeLeftVector(0,0,1);

	TArray<FVector> Verts;
	Verts.AddUninitialized( ConeSides );

	for ( int32 i = 0 ; i < Verts.Num() ; ++i )
	{
		const float Theta = static_cast<float>( (TwoPI * i) / Verts.Num() );
		Verts[i] = (ConeDirection * (ConeRadius * CosClampedConeAngle)) +
			((SinClampedConeAngle * ConeRadius * FMath::Cos( Theta )) * ConeUpVector) +
			((SinClampedConeAngle * ConeRadius * FMath::Sin( Theta )) * ConeLeftVector);
	}

	// Transform to world space.
	for ( int32 i = 0 ; i < Verts.Num() ; ++i )
	{
		Verts[i] = Transform.TransformPosition( Verts[i] );
	}

	// Draw spokes.
	for ( int32 i = 0 ; i < Verts.Num(); ++i )
	{
		DrawDebugCanvasLine( Canvas, Transform.GetLocation(), Verts[i], Color );
	}

	// Draw rim.
	for ( int32 i = 0 ; i < Verts.Num()-1 ; ++i )
	{
		DrawDebugCanvasLine( Canvas, Verts[i], Verts[i+1], Color );
	}
	DrawDebugCanvasLine( Canvas, Verts[Verts.Num()-1], Verts[0], Color );
}

//
// Canvas 2D
//

void DrawDebugCanvas2DLine(UCanvas* Canvas, const FVector2D& StartPosition, const FVector2D& EndPosition, const FLinearColor& LineColor, const float& LineThickness)
{
	if (Canvas)
	{
		FCanvasLineItem LineItem(StartPosition, EndPosition);
		LineItem.LineThickness = LineThickness;
		LineItem.SetColor(LineColor);
		Canvas->DrawItem(LineItem);
	}
}

void DrawDebugCanvas2DCircle(UCanvas* Canvas, const FVector2D& Center, float Radius, int32 NumSides, const FLinearColor& LineColor, const float& LineThickness)
{
	const float	AngleDelta = 2.0f * PI / NumSides;
	FVector2D AxisX(1.f, 0.f);
	FVector2D AxisY(0.f, -1.f);
	FVector2D LastVertex = Center + AxisX * Radius;

	for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
	{
		const FVector2D Vertex = Center + (AxisX * FMath::Cos(AngleDelta * (SideIndex + 1)) + AxisY * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;
		DrawDebugCanvas2DLine(Canvas, LastVertex, Vertex, LineColor, LineThickness);
		LastVertex = Vertex;
	}
}

void DrawDebugCanvas2DBox(UCanvas* Canvas, const FBox2D& Box, const FLinearColor& LineColor, const float& LineThickness)
{
	if (Canvas)
	{
		FCanvasBoxItem BoxItem(Box.Min, Box.GetSize());
		BoxItem.LineThickness = LineThickness;
		BoxItem.SetColor(LineColor);

		Canvas->DrawItem(BoxItem);
	}
}

#endif // ENABLE_DRAW_DEBUG
