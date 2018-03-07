// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AllDesktopTargetPlatform.cpp: Implements the FDesktopTargetPlatform class.
=============================================================================*/

#include "AllDesktopTargetPlatform.h"

#if WITH_ENGINE
	#include "Sound/SoundWave.h"
#endif


 

/* FAllDesktopTargetPlatform structors
 *****************************************************************************/

FAllDesktopTargetPlatform::FAllDesktopTargetPlatform()
{
#if WITH_ENGINE
	// use non-platform specific settings
	FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, NULL);
	StaticMeshLODSettings.Initialize(EngineSettings);
#endif // #if WITH_ENGINE

}


FAllDesktopTargetPlatform::~FAllDesktopTargetPlatform()
{
}



/* ITargetPlatform interface
 *****************************************************************************/

#if WITH_ENGINE


void FAllDesktopTargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
	static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
	static FName NAME_GLSL_150(TEXT("GLSL_150"));
	static FName NAME_GLSL_430(TEXT("GLSL_430"));
	
#if PLATFORM_WINDOWS
	// right now, only windows can properly compile D3D shaders (this won't corrupt the DDC, but it will 
	// make it so that packages cooked on Mac/Linux will only run on Windows with -opengl)
	OutFormats.AddUnique(NAME_PCD3D_SM5);
	OutFormats.AddUnique(NAME_PCD3D_SM4);
#endif
	OutFormats.AddUnique(NAME_GLSL_150);
	OutFormats.AddUnique(NAME_GLSL_430);
}


void FAllDesktopTargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}

void FAllDesktopTargetPlatform::GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const
{
	// just use the standard texture format name for this texture (without DX11 texture support)
	OutFormats.Add(GetDefaultTextureFormatName(this, Texture, EngineSettings, false));
}

void FAllDesktopTargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	GetAllDefaultTextureFormats(this, OutFormats, false);
}


FName FAllDesktopTargetPlatform::GetWaveFormat( const class USoundWave* Wave ) const
{
	static FName NAME_OGG(TEXT("OGG"));
	static FName NAME_OPUS(TEXT("OPUS"));
	
	if (Wave->IsStreaming())
	{
		// @todo desktop platform: Does Linux support OPUS?
		return NAME_OPUS;
	}
	
	return NAME_OGG;
}


void FAllDesktopTargetPlatform::GetAllWaveFormats(TArray<FName>& OutFormats) const
{
	static FName NAME_OGG(TEXT("OGG"));
	static FName NAME_OPUS(TEXT("OPUS"));
	OutFormats.Add(NAME_OGG);
	OutFormats.Add(NAME_OPUS);

}

#endif // WITH_ENGINE
