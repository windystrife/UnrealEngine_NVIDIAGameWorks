// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VisualLogger/VisualLoggerKismetLibrary.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"

UVisualLoggerKismetLibrary::UVisualLoggerKismetLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVisualLoggerKismetLibrary::LogText(UObject* WorldContextObject, FString Text, FName CategoryName)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::CategorizedLogf(WorldContextObject, FLogCategoryBase(*CategoryName.ToString(), DefaultVerbosity, DefaultVerbosity), DefaultVerbosity, TEXT("%s"), *Text);
#endif
}

void UVisualLoggerKismetLibrary::LogLocation(UObject* WorldContextObject, FVector Location, FString Text, FLinearColor Color, float Radius, FName CategoryName)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::GeometryShapeLogf(WorldContextObject, FLogCategoryBase(*CategoryName.ToString(), DefaultVerbosity, DefaultVerbosity), DefaultVerbosity, Location, Radius, Color.ToFColor(true), TEXT("%s"), *Text);
#endif
}

void UVisualLoggerKismetLibrary::LogBox(UObject* WorldContextObject, FBox Box, FString Text, FLinearColor ObjectColor, FName CategoryName)
{
#if ENABLE_VISUAL_LOG
	const ELogVerbosity::Type DefaultVerbosity = ELogVerbosity::Log;
	FVisualLogger::GeometryShapeLogf(WorldContextObject, FLogCategoryBase(*CategoryName.ToString(), DefaultVerbosity, DefaultVerbosity), DefaultVerbosity, Box, FMatrix::Identity, ObjectColor.ToFColor(true), TEXT("%s"), *Text);
#endif
}
