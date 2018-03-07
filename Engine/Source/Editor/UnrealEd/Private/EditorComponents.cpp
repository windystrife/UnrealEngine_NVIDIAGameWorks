// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "EditorComponents.h"
#include "EngineDefines.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "EngineGlobals.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Selection.h"
#include "SceneManagement.h"
#include "EditorModeManager.h"
#include "GameFramework/WorldSettings.h"

static TAutoConsoleVariable<int32> CVarEditorNewLevelGrid(
	TEXT("r.Editor.NewLevelGrid"),
	2,
	TEXT("Wether to show the new editor level grid\n")
	TEXT("0: off\n")
	TEXT("1: Analytical Antialiasing\n")
	TEXT("2: Texture based(default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarEditor2DGridFade(
	TEXT("r.Editor.2DGridFade"),
	0.15f,
	TEXT("Tweak to define the grid rendering in 2D viewports."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarEditor2DSnapFade(
	TEXT("r.Editor.2DSnapFade"),
	0.3f,
	TEXT("Tweak to define the grid rendering in 2D viewports."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarEditor3DGridFade(
	TEXT("r.Editor.3DGridFade"),
	0.5f,
	TEXT("Tweak to define the grid rendering in 3D viewports."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarEditor3DSnapFade(
	TEXT("r.Editor.3DSnapFade"),
	0.35f,
	TEXT("Tweak to define the grid rendering in 3D viewports."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarEditor2DSnapMin(
	TEXT("r.Editor.2DSnapMin"),
	0.25f,
	TEXT("Tweak to define the grid rendering in 2D viewports."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarEditor2DSnapScale(
	TEXT("r.Editor.2DSnapScale"),
	10.0f,
	TEXT("Tweak to define the grid rendering in 2D viewports."),
	ECVF_RenderThreadSafe);

static bool IsEditorCompositingMSAAEnabled(ERHIFeatureLevel::Type InFeatureLevel)
{
	bool Ret = false;

	if (InFeatureLevel >= ERHIFeatureLevel::SM5)
	{
		// only supported on SM5 yet
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MSAA.CompositingSampleCount"));

		Ret = CVar->GetValueOnGameThread() > 1;
	}

	return Ret;
}

/*------------------------------------------------------------------------------
FGridWidget.
------------------------------------------------------------------------------*/

FGridWidget::FGridWidget()
{
	LevelGridMaterial = (UMaterial*)StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("/Engine/EditorMaterials/LevelGridMaterial.LevelGridMaterial"),NULL,LOAD_None,NULL );
	LevelGridMaterialInst = UMaterialInstanceDynamic::Create(LevelGridMaterial, NULL);

	LevelGridMaterial2 = (UMaterial*)StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("/Engine/EditorMaterials/LevelGridMaterial2.LevelGridMaterial2"),NULL,LOAD_None,NULL );
	LevelGridMaterialInst2 = UMaterialInstanceDynamic::Create(LevelGridMaterial2, NULL);
}

void FGridWidget::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( LevelGridMaterial );
	Collector.AddReferencedObject( LevelGridMaterialInst );
	Collector.AddReferencedObject( LevelGridMaterial2 );
	Collector.AddReferencedObject( LevelGridMaterialInst2 );
}

static void GetAxisColors(FLinearColor Out[3], bool b3D)
{
	Out[0] = FLinearColor::Red;
	Out[1] = FLinearColor::Green;
	Out[2] = FLinearColor::Blue;

	// less prominent axis lines
	for(int i = 0; i < 3; ++i)
	{
		// darker
		if(b3D)
		{
			Out[i] += FLinearColor(0.2f, 0.2f, 0.2f, 0);
			Out[i] *= 0.1f;
		}
		else
		{
//			Out[i] += FLinearColor(0.5f, 0.5f, 0.5f, 0);
			Out[i] *= 0.5f;
		}
	}
}

void FGridWidget::DrawNewGrid(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	bool bUseTextureSolution = CVarEditorNewLevelGrid.GetValueOnGameThread() > 1;
	UMaterial* GridMaterial = bUseTextureSolution ? LevelGridMaterial2 : LevelGridMaterial;
	UMaterialInstanceDynamic* MaterialInst = bUseTextureSolution ? LevelGridMaterialInst2 : LevelGridMaterialInst;

	if (GridMaterial->IsCompilingOrHadCompileError(View->GetFeatureLevel()))
	{
		// The material would appear to be black (because we don't use a MaterialDomain but a UsageFlag - we should change that).
		// Here we rather want to hide it.
		return;
	}

	if(!MaterialInst)
	{
		return;
	}

	bool bMSAA = IsEditorCompositingMSAAEnabled(View->GetFeatureLevel());
	bool bIsPerspective = ( View->ViewMatrices.GetProjectionMatrix().M[3][3] < 1.0f );

	// in unreal units
	float SnapGridSize = GEditor->GetGridSize();

	// not used yet
	const bool bSnapEnabled = GetDefault<ULevelEditorViewportSettings>()->GridEnabled;

	float SnapAlphaMultiplier = 1.0f;

	// to get a light grid in a black level but use a high opacity value to be able to see it in a bright level
	static float Darken = 0.11f;

	static FName GridColorName("GridColor");
	static FName SnapColorName("SnapColor");
	static FName ExponentName("Exponent");
	static FName AlphaBiasName("AlphaBias");
	
	if(bIsPerspective)
	{
		MaterialInst->SetVectorParameterValue(GridColorName, FLinearColor(0.6f * Darken, 0.6f * Darken, 0.6f * Darken, CVarEditor3DGridFade.GetValueOnGameThread()));
		MaterialInst->SetVectorParameterValue(SnapColorName, FLinearColor(0.5f, 0.0f, 0.0f, SnapAlphaMultiplier * CVarEditor3DSnapFade.GetValueOnGameThread()));
	}
	else
	{
		MaterialInst->SetVectorParameterValue(GridColorName, FLinearColor(0.6f * Darken, 0.6f * Darken, 0.6f * Darken, CVarEditor2DGridFade.GetValueOnGameThread()));
		MaterialInst->SetVectorParameterValue(SnapColorName, FLinearColor(0.5f, 0.0f, 0.0f, SnapAlphaMultiplier * CVarEditor2DSnapFade.GetValueOnGameThread()));
	}

	// true:1m, false:1dm ios smallest grid size
	bool bLarger1mGrid = true;

	const int Exponent = 10;

	// 2 is the default so we need to set it
	MaterialInst->SetScalarParameterValue(ExponentName, (float)Exponent);

	// without MSAA we need the grid to be more see through so lines behind it can be recognized
	MaterialInst->SetScalarParameterValue(AlphaBiasName, bMSAA ? 0.0f : 0.05f);

	// grid for size
	float GridSplit = 0.5f;
	// red dots to visualize the snap
	float SnapSplit = 0.075f;

	float WorldToUVScale = 0.001f;

	if(bLarger1mGrid)
	{
		WorldToUVScale *= 0.1f;
		GridSplit *= 0.1f;
	}

	// in 2D all grid lines are same size in world space (they are at different scale so we need to adjust here)
	FLinearColor GridSplitTriple(GridSplit * 0.01f, GridSplit * 0.1f, GridSplit);

	if(bIsPerspective)
	{
		// largest grid lines
		GridSplitTriple.R *= 8.0f;
		// medium grid lines
		GridSplitTriple.G *= 3.0f;
		// fine grid lines
		GridSplitTriple.B *= 1.0f;
	}

	if(!bIsPerspective)
	{
		// screenspace size looks better in 2d

		float ScaleX = View->ViewMatrices.GetProjectionMatrix().M[0][0] * View->ViewRect.Width();
		float ScaleY = View->ViewMatrices.GetProjectionMatrix().M[1][1] * View->ViewRect.Height();

		float Scale = FMath::Min(ScaleX, ScaleY);

		float GridScale = CVarEditor2DSnapScale.GetValueOnGameThread();
		float GridMin = CVarEditor2DSnapMin.GetValueOnGameThread();

		// we need to account for a larger grids setting
		SnapSplit = 1.25f * FMath::Min(GridScale / SnapGridSize / Scale, GridMin);

		// hack test
		GridSplitTriple.R = 0.25f * FMath::Min(GridScale / 100 / Scale * 0.01f, GridMin);
		GridSplitTriple.G = 0.25f * FMath::Min(GridScale / 100 / Scale * 0.1f, GridMin);
		GridSplitTriple.B = 0.25f * FMath::Min(GridScale / 100 / Scale, GridMin);
	}

	float SnapTile = (1.0f / WorldToUVScale) / FMath::Max(1.0f, SnapGridSize);

	MaterialInst->SetVectorParameterValue("GridSplit", GridSplitTriple);
	MaterialInst->SetScalarParameterValue("SnapSplit", SnapSplit);
	MaterialInst->SetScalarParameterValue("SnapTile", SnapTile);

	FMatrix ObjectToWorld = FMatrix::Identity;

	FVector CameraPos = View->ViewMatrices.GetViewOrigin();

	FVector2D UVCameraPos = FVector2D(CameraPos.X, CameraPos.Y);

	ObjectToWorld.SetOrigin(FVector(CameraPos.X, CameraPos.Y, 0));

	FLinearColor AxisColors[3];
	GetAxisColors(AxisColors, true);

	FLinearColor UAxisColor = AxisColors[1];
	FLinearColor VAxisColor = AxisColors[0];

	if(!bIsPerspective)
	{
		float FarZ = 100000.0f;

		if(View->ViewMatrices.GetViewMatrix().M[1][1] == -1.f )		// Top
		{
			ObjectToWorld.SetOrigin(FVector(CameraPos.X, CameraPos.Y, -FarZ));
		}
		if(View->ViewMatrices.GetViewMatrix().M[1][2] == -1.f )		// Front
		{
			UVCameraPos = FVector2D(CameraPos.Z, CameraPos.X);
			ObjectToWorld.SetAxis(0, FVector(0,0,1));
			ObjectToWorld.SetAxis(1, FVector(1,0,0));
			ObjectToWorld.SetAxis(2, FVector(0,1,0));
			ObjectToWorld.SetOrigin(FVector(CameraPos.X, -FarZ, CameraPos.Z));
			UAxisColor = AxisColors[0];
			VAxisColor = AxisColors[2];
		}
		else if(View->ViewMatrices.GetViewMatrix().M[1][0] == 1.f )		// Side
		{
			UVCameraPos = FVector2D(CameraPos.Y, CameraPos.Z);
			ObjectToWorld.SetAxis(0, FVector(0,1,0));
			ObjectToWorld.SetAxis(1, FVector(0,0,1));
			ObjectToWorld.SetAxis(2, FVector(1,0,0));
			ObjectToWorld.SetOrigin(FVector(FarZ, CameraPos.Y, CameraPos.Z));
			UAxisColor = AxisColors[2];
			VAxisColor = AxisColors[1];
		}
	}
	
	MaterialInst->SetVectorParameterValue("UAxisColor", UAxisColor);
	MaterialInst->SetVectorParameterValue("VAxisColor", VAxisColor);

	// We don't want to affect the mouse interaction.
	PDI->SetHitProxy(0);

	// good enough to avoid the AMD artifacts, horizon still appears to be a line
	float Radii = 100000;

	if(bIsPerspective)
	{
		// the higher we get the larger we make the geometry to give the illusion of an infinite grid while maintains the precision nearby
		Radii *= FMath::Max( 1.0f, FMath::Abs(CameraPos.Z) / 1000.0f );
	}
	else
	{
		float ScaleX = View->ViewMatrices.GetProjectionMatrix().M[0][0];
		float ScaleY = View->ViewMatrices.GetProjectionMatrix().M[1][1];

		float Scale = FMath::Min(ScaleX, ScaleY);

		Scale *= View->ViewRect.Width();

		// We render a larger grid if we are zoomed out more (good precision at any scale)
		Radii *= 1.0f / Scale;
	}

	FVector2D UVMid = UVCameraPos * WorldToUVScale;
	float UVRadi = Radii * WorldToUVScale;

	FVector2D UVMin = UVMid + FVector2D(-UVRadi, -UVRadi);
	FVector2D UVMax = UVMid + FVector2D(UVRadi, UVRadi);

	// vertex pos is in -1..1 range
	DrawPlane10x10(PDI, ObjectToWorld, Radii, UVMin, UVMax, MaterialInst->GetRenderProxy(false), SDPG_World );
}


/*------------------------------------------------------------------------------
FEditorCommonDrawHelper.
------------------------------------------------------------------------------*/
FEditorCommonDrawHelper::FEditorCommonDrawHelper()
	: bDrawGrid(true)
	, bDrawPivot(false)
	, bDrawBaseInfo(true)
	, bDrawWorldBox(false)
	, bDrawKillZ(false)
	, AxesLineThickness(0.0f)
	, GridColorAxis(70, 70, 70)
	, GridColorMajor(40, 40, 40)
	, GridColorMinor(20, 20, 20)
	, PerspectiveGridSize(HALF_WORLD_MAX1)
	, PivotColor(FColor::Red)
	, PivotSize(0.02f)
	, NumCells(64)
	, BaseBoxColor(FColor::Green)
	, DepthPriorityGroup(SDPG_World)
	, GridDepthBias(0.000001f)
	, GridWidget(nullptr)
{
}

FEditorCommonDrawHelper::~FEditorCommonDrawHelper()
{
	delete GridWidget;
}

void FEditorCommonDrawHelper::Draw(const FSceneView* View,class FPrimitiveDrawInterface* PDI)
{
	if( !PDI->IsHitTesting() )
	{
		if(bDrawBaseInfo)
		{
			DrawBaseInfo(View, PDI);
		}

		// Only draw the pivot if an actor is selected
		if( bDrawPivot && GEditor->GetSelectedActors()->CountSelections<AActor>() > 0 && View->Family->EngineShowFlags.Pivot )
		{
			DrawPivot(View, PDI);
		}

		if( View->Family->EngineShowFlags.Grid && bDrawGrid)
		{
			bool bShouldUseNewLevelGrid = CVarEditorNewLevelGrid.GetValueOnGameThread() != 0;

			if(!View->IsPerspectiveProjection())
			{
				// 3D looks better with the old grid (no thick lines)
				bShouldUseNewLevelGrid = false;
			}

			if(bShouldUseNewLevelGrid)
			{
				if(!GridWidget)
				{
					// defer creation to avoid GC issues
					GridWidget = new FGridWidget;
				}

				GridWidget->DrawNewGrid(View, PDI);
			}
			else
			{
				DrawOldGrid(View, PDI);
			}
		}
	}
}

/** Draw green lines to indicate what the selected actor(s) are based on. */
void FEditorCommonDrawHelper::DrawBaseInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
// @todo UE4 - reimplement with new component attachment system
}

void FEditorCommonDrawHelper::DrawOldGrid(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	ESceneDepthPriorityGroup eDPG = (ESceneDepthPriorityGroup)DepthPriorityGroup;

	bool bIsPerspective = ( View->ViewMatrices.GetProjectionMatrix().M[3][3] < 1.0f );

	// Don't attempt to draw the grid lines from the maximum half world extent as it may be clipped due to floating point truncation errors
	const float HalfWorldExtent = HALF_WORLD_MAX - 1000.0f;

	// Draw 3D perspective grid
	if( bIsPerspective)
	{
		// @todo: Persp grid should be changed to be adaptive and use same settings as ortho grid, including grid interval!
		const int32 RangeInCells = NumCells / 2;
		const int32 MajorLineInterval = NumCells / 8;

		const int32 NumLines = NumCells + 1;
		const int32 AxesIndex = NumCells / 2;
		for( int32 LineIndex = 0; LineIndex < NumLines; ++LineIndex )
		{
			bool bIsMajorLine = ( ( LineIndex - RangeInCells ) % MajorLineInterval ) == 0;

			FVector A,B;
			A.X=(PerspectiveGridSize/4.f)*(-1.0+2.0*LineIndex/NumCells);	B.X=A.X;

			A.Y=(PerspectiveGridSize/4.f);		B.Y=-(PerspectiveGridSize/4.f);
			A.Z=0.0;							B.Z=0.0;

			FColor LineColor;
			float LineThickness = 0.f;

			if ( LineIndex==AxesIndex )
			{
				LineColor = GridColorAxis;
				LineThickness = AxesLineThickness;
			}
			else if ( bIsMajorLine )
			{
				LineColor = GridColorMajor;
			}
			else
			{
				LineColor = GridColorMinor;
			}

			PDI->DrawLine(A,B,LineColor,eDPG, LineThickness, GridDepthBias);

			A.Y=A.X;							B.Y=B.X;
			A.X=(PerspectiveGridSize/4.f);		B.X=-(PerspectiveGridSize/4.f);
			PDI->DrawLine(A,B,LineColor,eDPG, LineThickness, GridDepthBias);
		}
	}
	// Draw ortho grid.
	else
	{
		const bool bIsOrthoXY = ( FMath::Abs(View->ViewMatrices.GetViewMatrix().M[2][2]) > 0.0f );
		const bool bIsOrthoXZ = ( FMath::Abs(View->ViewMatrices.GetViewMatrix().M[1][2]) > 0.0f );
		const bool bIsOrthoYZ = ( FMath::Abs(View->ViewMatrices.GetViewMatrix().M[0][2]) > 0.0f );

		FLinearColor AxisColors[3];
		GetAxisColors(AxisColors, false);

		if (bIsOrthoXY)
		{
			const bool bNegative = View->ViewMatrices.GetViewMatrix().M[2][2] > 0.0f;

			FVector StartY(0.0f, +HalfWorldExtent, bNegative ? HalfWorldExtent : -HalfWorldExtent);
			FVector EndY(0.0f, -HalfWorldExtent, bNegative ? HalfWorldExtent : -HalfWorldExtent);
			FVector StartX(+HalfWorldExtent, 0.0f, bNegative ? HalfWorldExtent : -HalfWorldExtent);
			FVector EndX(-HalfWorldExtent, 0.0f, bNegative ? HalfWorldExtent : -HalfWorldExtent);

			DrawGridSection( GEditor->GetGridSize(), &StartY, &EndY, &StartY.X, &EndY.X, 0, View, PDI);
			DrawGridSection( GEditor->GetGridSize(), &StartX, &EndX, &StartX.Y, &EndX.Y, 1, View, PDI);
			DrawOriginAxisLine( &StartY, &EndY, &StartY.X, &EndY.X, View, PDI, AxisColors[1] );
			DrawOriginAxisLine( &StartX, &EndX, &StartX.Y, &EndX.Y, View, PDI, AxisColors[0] );
		}
		else if( bIsOrthoXZ )
		{
			const bool bNegative = View->ViewMatrices.GetViewMatrix().M[1][2] > 0.0f;

			FVector StartZ(0.0f, bNegative ? HalfWorldExtent : -HalfWorldExtent, +HalfWorldExtent);
			FVector EndZ(0.0f, bNegative ? HalfWorldExtent : -HalfWorldExtent, -HalfWorldExtent);
			FVector StartX(+HalfWorldExtent, bNegative ? HalfWorldExtent : -HalfWorldExtent, 0.0f);
			FVector EndX(-HalfWorldExtent, bNegative ? HalfWorldExtent : -HalfWorldExtent, 0.0f);

			DrawGridSection( GEditor->GetGridSize(), &StartZ, &EndZ, &StartZ.X, &EndZ.X, 0, View, PDI);
			DrawGridSection( GEditor->GetGridSize(), &StartX, &EndX, &StartX.Z, &EndX.Z, 2, View, PDI);
			DrawOriginAxisLine( &StartZ, &EndZ, &StartZ.X, &EndZ.X, View, PDI, AxisColors[2] );
			DrawOriginAxisLine( &StartX, &EndX, &StartX.Z, &EndX.Z, View, PDI, AxisColors[0] );
		}
		else if( bIsOrthoYZ )
		{
			const bool bNegative = View->ViewMatrices.GetViewMatrix().M[0][2] < 0.0f;

			FVector StartZ(bNegative ? -HalfWorldExtent : HalfWorldExtent, 0.0f, +HalfWorldExtent);
			FVector EndZ(bNegative ? -HalfWorldExtent : HalfWorldExtent, 0.0f, -HalfWorldExtent);
			FVector StartY(bNegative ? -HalfWorldExtent : HalfWorldExtent, +HalfWorldExtent, 0.0f);
			FVector EndY(bNegative ? -HalfWorldExtent : HalfWorldExtent, -HalfWorldExtent, 0.0f);

			DrawGridSection( GEditor->GetGridSize(), &StartZ, &EndZ, &StartZ.Y, &EndZ.Y, 1, View, PDI);
			DrawGridSection( GEditor->GetGridSize(), &StartY, &EndY, &StartY.Z, &EndY.Z, 2, View, PDI);
			DrawOriginAxisLine( &StartZ, &EndZ, &StartZ.Y, &EndZ.Y, View, PDI, AxisColors[2] );
			DrawOriginAxisLine( &StartY, &EndY, &StartY.Z, &EndY.Z, View, PDI, AxisColors[1] );
		}

		if( bDrawKillZ && ( bIsOrthoXZ || bIsOrthoYZ ) && GWorld->GetWorldSettings()->bEnableWorldBoundsChecks )
		{
			float KillZ = GWorld->GetWorldSettings()->KillZ;

			PDI->DrawLine(FVector(-HalfWorldExtent, 0, KillZ), FVector(HalfWorldExtent, 0, KillZ), FColor::Red, SDPG_Foreground);
			PDI->DrawLine(FVector(0, -HalfWorldExtent, KillZ), FVector(0, HalfWorldExtent, KillZ), FColor::Red, SDPG_Foreground);
		}
	}

	// Draw orthogonal worldframe.
	if(bDrawWorldBox)
	{
		DrawWireBox(PDI, FBox(FVector(-HalfWorldExtent, -HalfWorldExtent, -HalfWorldExtent), FVector(HalfWorldExtent, HalfWorldExtent, HalfWorldExtent)), GEngine->C_WorldBox, eDPG);
	}
}


void FEditorCommonDrawHelper::DrawGridSection(float ViewportGridY,FVector* A,FVector* B,float* AX,float* BX,int32 Axis,const FSceneView* View,FPrimitiveDrawInterface* PDI )
{
	if( ViewportGridY == 0 )
	{
		// Don't draw zero-size grid
		return;
	}

	// todo
	int32 Exponent = GEditor->IsGridSizePowerOfTwo() ? 8 : 10;

	const float SizeX = View->ViewRect.Width();
	const float Zoom = (1.0f / View->ViewMatrices.GetProjectionMatrix().M[0][0]) * 2.0f / SizeX;
	const float Dist = SizeX * Zoom / ViewportGridY;

	// when the grid fades
	static float Tweak = 4.0f;

	float IncValue = FMath::LogX(Exponent, Dist / (float)(SizeX / Tweak));
	int32 IncScale = 1;

	for(float x = 0; x < IncValue; ++x)
	{
		IncScale *= Exponent;
	}

	if (IncScale == 0)
	{
		// Prevent divide by zero
		return;
	}

	// 0 excluded for hard transitions .. 0.5f for very soft transitions
	const float TransitionRegion = 0.5f;

	const float InvTransitionRegion = 1.0f / TransitionRegion;
	float Fract = IncValue - floorf(IncValue);
	float AlphaA = FMath::Clamp(Fract * InvTransitionRegion, 0.0f, 1.0f);
	float AlphaB = FMath::Clamp(InvTransitionRegion - Fract * InvTransitionRegion, 0.0f, 1.0f);

	if(IncValue < -0.5f)
	{
		// no fade in magnification case
		AlphaA = 1.0f;
		AlphaB = 1.0f;
	}

	const int32 MajorLineInterval = FMath::TruncToInt(GEditor->GetGridInterval());

	const FLinearColor Background = View->BackgroundColor;

	FLinearColor MajorColor = FMath::Lerp(Background, FLinearColor::White, 0.05f);
	FLinearColor MinorColor = FMath::Lerp(Background, FLinearColor::White, 0.02f);

	const FMatrix InvViewProjMatrix = View->ViewMatrices.GetInvViewProjectionMatrix();
	int32 FirstLine = FMath::TruncToInt(InvViewProjMatrix.TransformPosition(FVector(-1, -1, 0.5f)).Component(Axis) / ViewportGridY);
	int32 LastLine = FMath::TruncToInt(InvViewProjMatrix.TransformPosition(FVector(+1, +1, 0.5f)).Component(Axis) / ViewportGridY);
	if (FirstLine > LastLine)
	{
		Exchange(FirstLine, LastLine);
	}

	// Draw major and minor grid lines
	const int32 FirstLineClamped = FMath::Max<int32>(FirstLine - 1,-HALF_WORLD_MAX/ViewportGridY) / IncScale;
	const int32 LastLineClamped = FMath::Min<int32>(LastLine + 1, +HALF_WORLD_MAX/ViewportGridY) / IncScale;
	for( int32 LineIndex = FirstLineClamped; LineIndex <= LastLineClamped; ++LineIndex )
	{
		*AX = FPlatformMath::TruncToFloat(LineIndex * ViewportGridY) * IncScale;
		*BX = FPlatformMath::TruncToFloat(LineIndex * ViewportGridY) * IncScale;

		// Only minor lines fade out with ortho zoom distance.  Origin lines and major lines are drawn
		// at 100% opacity, but with a brighter value
		const bool bIsMajorLine = (MajorLineInterval == 0) || ((LineIndex % MajorLineInterval) == 0);

		{
			// Don't bother drawing the world origin line.  We'll do that later.
			const bool bIsOriginLine = ( LineIndex == 0 );
			if( !bIsOriginLine )
			{
				FLinearColor Color;
				if( bIsMajorLine )
				{
					Color = FMath::Lerp( Background, MajorColor, AlphaA );
				}
				else
				{
					Color = FMath::Lerp( Background, MinorColor, AlphaB );
				}

				PDI->DrawLine(*A,*B,Color,SDPG_World);
			}
		}
	}
}


void FEditorCommonDrawHelper::DrawOriginAxisLine(FVector* A,FVector* B,float* AX,float* BX,const FSceneView* View,FPrimitiveDrawInterface* PDI, const FLinearColor& Color)
{
	// Draw world origin lines.  We draw these last so they appear on top of the other lines.
	*AX = 0;
	*BX = 0;

	PDI->DrawLine(*A,*B,FLinearColor(Color.Quantize()),SDPG_World);
}


void FEditorCommonDrawHelper::DrawPivot(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	const FMatrix CameraToWorld = View->ViewMatrices.GetInvViewMatrix();

	const FVector PivLoc = GLevelEditorModeTools().SnappedLocation;

	const float ZoomFactor = FMath::Min<float>(View->ViewMatrices.GetProjectionMatrix().M[0][0], View->ViewMatrices.GetProjectionMatrix().M[1][1]);
	const float WidgetRadius = View->ViewMatrices.GetViewProjectionMatrix().TransformPosition(PivLoc).W * (PivotSize / ZoomFactor);

	const FVector CamX = CameraToWorld.TransformVector( FVector(1,0,0) );
	const FVector CamY = CameraToWorld.TransformVector( FVector(0,1,0) );


	PDI->DrawLine( PivLoc - (WidgetRadius*CamX), PivLoc + (WidgetRadius*CamX), PivotColor, SDPG_Foreground );
	PDI->DrawLine( PivLoc - (WidgetRadius*CamY), PivLoc + (WidgetRadius*CamY), PivotColor, SDPG_Foreground );
}
