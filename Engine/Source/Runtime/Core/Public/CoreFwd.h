// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainersFwd.h"
#include "UObject/UObjectHierarchyFwd.h"

// Basic types
class FName;
class FExec;
class FArchive;
class FOutputDevice;
class FFeedbackContext;
struct FDateTime;
struct FGuid;

// Math
class FSphere;
struct FBox;
struct FBox2D;
struct FColor;
struct FLinearColor;
struct FMatrix;
struct FPlane;
struct FQuat;
struct FRotator;
struct FTransform;
struct FVector;
struct FVector2D;
struct FVector4;
struct FBoxSphereBounds;
struct FIntPoint;
struct FIntRect;

// Misc
struct FResourceSizeEx;
class IConsoleVariable;
class FRunnableThread;
class FEvent;
class IPlatformFile;
class FMalloc;
class IFileHandle;
class FAutomationTestBase;
struct FGenericMemoryStats;
class FSHAHash;
class FScriptArray;
class FThreadSafeCounter;
enum class EModuleChangeReason;
struct FManifestContext;
class IConsoleObject;
class FConfigFile;
class FConfigSection;

// Text
class FText;
class FTextFilterString;
enum class ETextFilterTextComparisonMode : uint8;
enum class ETextFilterComparisonOperation : uint8;
