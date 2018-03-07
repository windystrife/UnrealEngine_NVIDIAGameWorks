// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintSettings.h"

#include "Misc/ConfigCacheIni.h"

UPaintBrushSettings::UPaintBrushSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	BrushRadius(128.0f),
	BrushStrength(0.5f),
	BrushFalloffAmount(0.5f),
	bEnableFlow(true),
	bOnlyFrontFacingTriangles(true),
	ColorViewMode(EMeshPaintColorViewMode::Normal)	
{
	const float BrushRadiusClampMin = 0.01f, BrushRadiusClampMax = 250000.0f;

	// Retrieve brush size properties from config
	BrushRadiusMin = 1.f;
	GConfig->GetFloat(TEXT("UnrealEd.MeshPaint"), TEXT("MinBrushRadius"), BrushRadiusMin, GEditorIni);
	BrushRadiusMin = (float)FMath::Clamp(BrushRadiusMin, BrushRadiusClampMin, BrushRadiusClampMax);

	BrushRadiusMax = 256.f;
	GConfig->GetFloat(TEXT("UnrealEd.MeshPaint"), TEXT("MaxBrushRadius"), BrushRadiusMax, GEditorIni);
	BrushRadiusMax  = (float)FMath::Clamp(BrushRadiusMax, BrushRadiusClampMin, BrushRadiusClampMax);

	GConfig->GetFloat(TEXT("MeshPaintEdit"), TEXT("DefaultBrushRadius"), BrushRadius, GEditorPerProjectIni);
	BrushRadius = (float)FMath::Clamp(BrushRadius, BrushRadiusClampMin, BrushRadiusClampMax);
}

UPaintBrushSettings::~UPaintBrushSettings()
{
	// On destruction
	if (GConfig)
	{
		BrushRadius = (float)FMath::Clamp(BrushRadius, BrushRadiusMin, BrushRadiusMax);
		GConfig->SetFloat(TEXT("MeshPaintEdit"), TEXT("DefaultBrushRadius"), BrushRadius, GEditorPerProjectIni);
	}
}

void UPaintBrushSettings::SetBrushRadius(float InRadius)
{
	BrushRadius = (float)FMath::Clamp(InRadius, BrushRadiusMin, BrushRadiusMax);
	GConfig->SetFloat(TEXT("MeshPaintEdit"), TEXT("DefaultBrushRadius"), BrushRadius, GEditorPerProjectIni);
}

void UPaintBrushSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPaintBrushSettings, BrushRadius))
	{
		BrushRadius = (float)FMath::Clamp(BrushRadius, BrushRadiusMin, BrushRadiusMax);
		GConfig->SetFloat(TEXT("MeshPaintEdit"), TEXT("DefaultBrushRadius"), BrushRadius, GEditorPerProjectIni);
	}
}