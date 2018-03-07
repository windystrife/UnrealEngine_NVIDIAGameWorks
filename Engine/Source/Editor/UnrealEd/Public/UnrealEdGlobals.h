// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

extern UNREALED_API class UUnrealEdEngine* GUnrealEd;

UNREALED_API int32 EditorInit( class IEngineLoop& EngineLoop );
UNREALED_API void EditorExit();
