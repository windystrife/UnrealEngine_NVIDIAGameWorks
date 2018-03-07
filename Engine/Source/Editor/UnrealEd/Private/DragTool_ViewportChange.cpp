// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "DragTool_ViewportChange.h"
#include "CanvasItem.h"
#include "LevelEditorViewport.h"
#include "ILevelEditor.h"
#include "CanvasTypes.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FDragTool_ViewportChange
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FDragTool_ViewportChange::FDragTool_ViewportChange(FLevelEditorViewportClient* InLevelViewportClient)
	: FDragTool(InLevelViewportClient->GetModeTools())
	, LevelViewportClient(InLevelViewportClient)
	, ViewOption(LVT_Perspective)
	, ViewOptionOffset(FVector2D(0.f,0.f))
{
	bUseSnapping = true;
	bConvertDelta = false;
}

void FDragTool_ViewportChange::StartDrag(FEditorViewportClient* InViewportClient, const FVector& InStart, const FVector2D& InStartScreen)
{
	FDragTool::StartDrag(InViewportClient, InStart, InStartScreen);

	FIntPoint MousePos;
	InViewportClient->Viewport->GetMousePos(MousePos);

	Start = FVector(InStartScreen.X, InStartScreen.Y, 0);
	End = EndWk = Start;
}

void FDragTool_ViewportChange::EndDrag()
{
	ViewOptionOffset.X = End.X - Start.X;
	ViewOptionOffset.Y = End.Y - Start.Y;

	if (ViewOptionOffset.Y == 0)
	{
		if (ViewOptionOffset.X == 0)
		{
			ViewOption = LVT_Perspective;
		}
		else if (ViewOptionOffset.X > 0)
		{
			ViewOption = LVT_OrthoNegativeYZ; //Right
		}
		else
		{
			ViewOption = LVT_OrthoYZ; //Left
		}
	}
	else
	{
		float OffsetRatio = ViewOptionOffset.X / ViewOptionOffset.Y;
		float DragAngle = FMath::RadiansToDegrees(FMath::Atan(OffsetRatio));

		if (ViewOptionOffset.Y >= 0)
		{
			if (DragAngle >= -15.f && DragAngle <= 15.f)
			{
				ViewOption = LVT_OrthoNegativeXY; //Bottom
			}
			else if (DragAngle > 75.f)
			{
				ViewOption = LVT_OrthoNegativeYZ; //Right
			}
			else if (DragAngle < -75.f)
			{
				ViewOption = LVT_OrthoYZ; //Left
			}
		}
		else
		{
			if (DragAngle >= -15.f && DragAngle < 15.f)
			{
				ViewOption = LVT_OrthoXY; //Top
			}
			else if (DragAngle >= 15.f && DragAngle < 75.f)
			{
				ViewOption = LVT_OrthoXZ; //Front
			}
			else if (DragAngle >= -75.f && DragAngle < -15.f)
			{
				ViewOption = LVT_OrthoNegativeXZ; //Back
			}
			else if (DragAngle >= 75.f)
			{
				ViewOption = LVT_OrthoYZ; //Left
			}
			else if (DragAngle <= -75.f)
			{
				ViewOption = LVT_OrthoNegativeYZ; //Right
			}
		}
	}

	float OffsetLength = FMath::RoundToFloat((End - Start).Size());
	if (OffsetLength >= 125.15f)
	{
		LevelViewportClient->SetViewportTypeFromTool(ViewOption);
		return;
	}

	if (LevelViewportClient->ParentLevelEditor.IsValid())
	{
		LevelViewportClient->ParentLevelEditor.Pin()->SummonLevelViewportViewOptionMenu(ViewOption);
	}
}

void FDragTool_ViewportChange::AddDelta(const FVector& InDelta)
{
	FDragTool::AddDelta(InDelta);

	FIntPoint MousePos;
	LevelViewportClient->Viewport->GetMousePos(MousePos);

	EndWk = FVector(MousePos);
	End = EndWk;

	ViewOptionOffset.X = End.X - Start.X;
	ViewOptionOffset.Y = End.Y - Start.Y;
}

void FDragTool_ViewportChange::Render(const FSceneView* View, FCanvas* Canvas)
{
	FCanvasLineItem LineItem(Start, End);
	Canvas->DrawItem(LineItem);
}
