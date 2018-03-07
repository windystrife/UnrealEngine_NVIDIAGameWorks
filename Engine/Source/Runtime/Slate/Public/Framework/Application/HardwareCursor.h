// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICursor.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

struct SDL_Cursor;

/**
 * Provides a cross desktop platform solution for providing hardware cursors.  These cursors
 * generally require platform specific formats.  To try and combat this, this class standardizes
 * on .png files.  However, on different platforms that support it, it searches for platform specific
 * formats first if you want to take advantage of those capabilities.
 *
 * Windows:
 *   .ani -> .cur -> .png
 *
 * Mac:
 *   .tiff -> .png
 *
 * Linux:
 *   .png
 *
 * MULTI-RESOLUTION / DPI SUPPORT
 *
 * Windows: 
 *   .ani & .cur both allow for multi-resolution cursor images.
 *
 * Mac:
 *   A multi-resolution tiff can be provided.
 *   https://developer.apple.com/library/content/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Optimizing/Optimizing.html
 * 
 * Linux:
 *   No platform specific files.
 *
 * Multi-Resolution Png Fallback
 *  Because there's not a universal multi-resolution format for cursors there's a pattern we look for
 *  on all platforms where pngs are all that is found instead of cur/ani/tiff.
 *   
 *    Pointer.png
 *    Pointer@1.25x.png
 *    Pointer@1.5x.png
 *    Pointer@1.75x.png
 *    Pointer@2x.png
 *    ...etc
 */
class SLATE_API FHardwareCursor
{
public:
	/**
	 * HotSpot needs to be in normalized UV coordinates since it may apply to different resolution images. 
	 */
	FHardwareCursor(const FString& InPathToCursorWithoutExtension, FVector2D HotSpot);

	/**
	 * 
	 */
	FHardwareCursor(const TArray<FColor>& Pixels, FIntPoint Size, FVector2D HotSpot);

	/** Destructor */
	~FHardwareCursor();

	/** Gets the platform specific handle to the cursor that was allocated.  If loading the cursor failed, this value will be null. */
	void* GetHandle();

private:

#if PLATFORM_WINDOWS
	bool LoadCursorFromAniOrCur(const FString& InPathToCursorWithoutExtension);
#elif PLATFORM_MAC
	bool LoadCursorFromTiff(const FString& InPathToCursorWithoutExtension, FVector2D InHotSpot);
#endif

	bool LoadCursorFromPngs(const FString& InPathToCursorWithoutExtension, FVector2D HotSpot);

	void CreateCursorFromRGBABuffer(const FColor* Pixels, int32 Width, int32 Height, FVector2D InHotSpot);

	struct FPngFileData
	{
		FString FileName;
		double ScaleFactor;
		TArray<uint8> FileData;

		FPngFileData()
			: ScaleFactor(1.0)
		{
		}
	};

	/**
	 * Loads all the pngs so that a multi-resolution object can be built from this information.
	 */
	bool LoadAvailableCursorPngs(TArray<TSharedPtr<FPngFileData>>& Results, const FString& InPathToCursorWithoutExtension);

private:
#if PLATFORM_WINDOWS
	HCURSOR CursorHandle;
#elif PLATFORM_MAC
	NSCursor* CursorHandle;
#elif PLATFORM_LINUX
	SDL_Cursor* CursorHandle;
#endif
};
