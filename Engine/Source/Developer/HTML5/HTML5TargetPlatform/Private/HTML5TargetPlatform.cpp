// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetPlatform.cpp: Implements the FHTML5TargetPlatform class.
=============================================================================*/

#include "HTML5TargetPlatform.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMisc.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHTML5TargetPlatform, Log, All);


/* Static initialization
 *****************************************************************************/

FCriticalSection FHTML5TargetPlatform::DevicesCriticalSection;

 /* FHTML5TargetPlatform structors
 *****************************************************************************/

FHTML5TargetPlatform::FHTML5TargetPlatform( )
{
	RefreshHTML5Setup();
#if WITH_ENGINE
	// load up texture settings from the config file
	HTML5LODSettings = nullptr;
	StaticMeshLODSettings.Initialize(HTML5EngineSettings);
#endif
}


 /* ITargetPlatform interface
 *****************************************************************************/

void FHTML5TargetPlatform::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	FScopeLock Lock( &DevicesCriticalSection );

	OutDevices.Reset();

	for( auto Iter = Devices.CreateConstIterator(); Iter; ++Iter )
	{
		OutDevices.Add( Iter.Value() );
	}
}


ECompressionFlags FHTML5TargetPlatform::GetBaseCompressionMethod( ) const
{
	return COMPRESS_ZLIB;
}


ITargetDevicePtr FHTML5TargetPlatform::GetDefaultDevice( ) const
{
	FScopeLock Lock( &DevicesCriticalSection );

	return Devices.FindRef( DefaultDeviceName );
}


ITargetDevicePtr FHTML5TargetPlatform::GetDevice( const FTargetDeviceId& DeviceId )
{
	if( DeviceId.GetPlatformName() == this->PlatformName() )
	{
		FScopeLock Lock( &DevicesCriticalSection );
		for( auto MapIt = Devices.CreateIterator(); MapIt; ++MapIt )
		{
			FHTML5TargetDevicePtr& Device = MapIt->Value;
			if( Device->GetName() == DeviceId.GetDeviceName() )
			{
				return Device;
			}
		}
	}

	return nullptr;
}

bool FHTML5TargetPlatform::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
	// When the EMSDK environment variable is used, locate Emscripten SDK from the directory
	// pointed to by that variable, instead of using a prepackaged SDK.
	TCHAR EmsdkDirectory[1024] = { 0 };
	FPlatformMisc::GetEnvironmentVariable(TEXT("EMSDK"), EmsdkDirectory, 1024);
	if (EmsdkDirectory[0] != 0)
	{
		bool Exists = IFileManager::Get().DirectoryExists(EmsdkDirectory);
		if (!Exists)
		{
			UE_LOG(LogTemp, Display, TEXT("Environment variable EMSDK is set to \"%s\", but that directory does not exist!"), EmsdkDirectory);
		}
		return Exists;
	}

	FString SDKPath = FPaths::EngineDir() / TEXT("Extras/ThirdPartyNotUE/emsdk") /
#if PLATFORM_WINDOWS
		TEXT("Win64");
#elif PLATFORM_MAC
		TEXT("Mac");
#elif PLATFORM_LINUX
		TEXT("Linux");
#else
		TEXT("UNKNOWN_PLATFORM");
#endif

	FString SDKDirectory = FPaths::ConvertRelativePathToFull(SDKPath);

	if (IFileManager::Get().DirectoryExists(*SDKDirectory))
	{
		return true;
	}

	UE_LOG(LogTemp, Display, TEXT("HTML5 SDK path \"%s\", does not exist!"), *SDKDirectory);

	return false;
}


bool FHTML5TargetPlatform::IsRunningPlatform( ) const
{
	return false; // but this will never be called because this platform doesn't run the target platform framework
}


#if WITH_ENGINE

static FName NAME_GLSL_ES2_WEBGL(TEXT("GLSL_ES2_WEBGL"));

void FHTML5TargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	OutFormats.AddUnique(NAME_GLSL_ES2_WEBGL);
}


void FHTML5TargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}


const class FStaticMeshLODSettings& FHTML5TargetPlatform::GetStaticMeshLODSettings( ) const
{
	return StaticMeshLODSettings;
}


void FHTML5TargetPlatform::GetTextureFormats( const UTexture* Texture, TArray<FName>& OutFormats ) const
{
	FName TextureFormatName = NAME_None;

#if WITH_EDITOR
	// Supported texture format names.
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT3(TEXT("DXT3"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameDXT5n(TEXT("DXT5n"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameG8(TEXT("G8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));
	static FName NameRGBA8(TEXT("RGBA8"));

	bool bNoCompression = Texture->CompressionNone				// Code wants the texture uncompressed.
		|| (HasEditorOnlyData() && Texture->DeferCompression)	// The user wishes to defer compression, this is ok for the Editor only.
		|| (Texture->CompressionSettings == TC_EditorIcon)
		|| (Texture->LODGroup == TEXTUREGROUP_ColorLookupTable)	// Textures in certain LOD groups should remain uncompressed.
		|| (Texture->LODGroup == TEXTUREGROUP_Bokeh)
		|| (Texture->LODGroup == TEXTUREGROUP_IESLightProfile)
		|| (Texture->Source.GetSizeX() < 4) // Don't compress textures smaller than the DXT block size.
		|| (Texture->Source.GetSizeY() < 4)
		|| (Texture->Source.GetSizeX() % 4 != 0)
		|| (Texture->Source.GetSizeY() % 4 != 0);


	ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();

	// Determine the pixel format of the (un/)compressed texture
	if (bNoCompression)
	{
			if (Texture->HasHDRSource())
			{
				TextureFormatName = NameBGRA8;
			}
			else if (SourceFormat == TSF_G8 || Texture->CompressionSettings == TC_Grayscale)
			{
				TextureFormatName = NameG8;
			}
			else if (Texture->LODGroup == TEXTUREGROUP_Shadowmap)
			{
				TextureFormatName = NameG8;
			}
			else
			{
				TextureFormatName = NameRGBA8;
			}
	}
	else if (Texture->CompressionSettings == TC_HDR
		|| Texture->CompressionSettings == TC_HDR_Compressed)
	{
		TextureFormatName = NameRGBA16F;
	}
	else if (Texture->CompressionSettings == TC_Normalmap)
	{
		TextureFormatName =  NameDXT5;
	}
	else if (Texture->CompressionSettings == TC_Displacementmap)
	{
		TextureFormatName = NameG8;
	}
	else if (Texture->CompressionSettings == TC_VectorDisplacementmap)
	{
		TextureFormatName = NameRGBA8;
	}
	else if (Texture->CompressionSettings == TC_Grayscale)
	{
		TextureFormatName = NameG8;
	}
	else if( Texture->CompressionSettings == TC_Alpha)
	{
		TextureFormatName = NameDXT5;
	}
	else if (Texture->CompressionSettings == TC_DistanceFieldFont)
	{
		TextureFormatName = NameG8;
	}
	else if (Texture->CompressionNoAlpha)
	{
		TextureFormatName = NameDXT1;
	}
	else if (Texture->bDitherMipMapAlpha)
	{
		TextureFormatName = NameDXT5;
	}
	else
	{
		TextureFormatName = NameAutoDXT;
	}

	// Some PC GPUs don't support sRGB read from G8 textures (e.g. AMD DX10 cards on ShaderModel3.0)
	// This solution requires 4x more memory but a lot of PC HW emulate the format anyway
	if ((TextureFormatName == NameG8) && Texture->SRGB && !SupportsFeature(ETargetPlatformFeatures::GrayscaleSRGB))
	{
		TextureFormatName = NameBGRA8;
	}
#endif

	OutFormats.Add( TextureFormatName);
}

void FHTML5TargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const
{

#if WITH_EDITOR
	// Supported texture format names.
	static FName NameDXT1(TEXT("DXT1"));
	static FName NameDXT3(TEXT("DXT3"));
	static FName NameDXT5(TEXT("DXT5"));
	static FName NameDXT5n(TEXT("DXT5n"));
	static FName NameAutoDXT(TEXT("AutoDXT"));
	static FName NameBGRA8(TEXT("BGRA8"));
	static FName NameG8(TEXT("G8"));
	static FName NameRGBA16F(TEXT("RGBA16F"));
	static FName NameRGBA8(TEXT("RGBA8"));

	// Supported texture format names.
	OutFormats.Add(NameDXT1);
	OutFormats.Add(NameDXT3);
	OutFormats.Add(NameDXT5);
	OutFormats.Add(NameDXT5n);
	OutFormats.Add(NameAutoDXT);
	OutFormats.Add(NameBGRA8);
	OutFormats.Add(NameG8);
	OutFormats.Add(NameRGBA16F);
	OutFormats.Add(NameRGBA8);
#endif
}

const UTextureLODSettings& FHTML5TargetPlatform::GetTextureLODSettings() const
{
	return *HTML5LODSettings;
}


FName FHTML5TargetPlatform::GetWaveFormat( const USoundWave* Wave ) const
{
	static FName NAME_OGG(TEXT("OGG"));
	return NAME_OGG;
}



void FHTML5TargetPlatform::GetAllWaveFormats(TArray<FName>& OutFormats) const
{
	static FName NAME_OGG(TEXT("OGG"));
	OutFormats.Add(NAME_OGG);
}

#endif // WITH_ENGINE

void FHTML5TargetPlatform::RefreshHTML5Setup()
{
	FString Temp;
	if (!FHTML5TargetPlatform::IsSdkInstalled(true, Temp))
	{
		// nothing to do.
		return;
	}

	FScopeLock Lock( &DevicesCriticalSection );
	// nuke everything and then repopulate
	for (auto Iter = Devices.CreateIterator(); Iter; ++Iter)
	{
		FHTML5TargetDevicePtr Device = Iter->Value;
		Iter.RemoveCurrent();
		DeviceLostEvent.Broadcast(Device.ToSharedRef());
	}

	// fill optional items first - these may be empty...
	TArray<FString> DeviceMaps;
	GConfig->GetArray( TEXT("/Script/HTML5PlatformEditor.HTML5SDKSettings"), TEXT("BrowserLauncher"), DeviceMaps, GEngineIni );
	PopulateDevices(DeviceMaps, TEXT("user: "));
	DefaultDeviceName.Empty(); // force default to one of the common browsers

	// fill with "common browsers" (if they are installed)...
	DeviceMaps.Empty();
	GConfig->GetArray( TEXT("/Script/HTML5PlatformEditor.HTML5Browsers"), TEXT("BrowserLauncher"), DeviceMaps, GEngineIni );
	PopulateDevices(DeviceMaps, TEXT(""));
}

void FHTML5TargetPlatform::PopulateDevices(TArray<FString>& DeviceMaps, FString prefix)
{
	if ( ! DeviceMaps.Num() )
	{
		return;
	}

	for (auto It : DeviceMaps)
	{
		FString DeviceName = "";
		FString DevicePath = "";
		if( FParse::Value( *It, TEXT( "BrowserName=" ), DeviceName ) && !DeviceName.IsEmpty() &&
			FParse::Value( *It, TEXT( "BrowserPath=(FilePath=" ), DevicePath ) && !DevicePath.IsEmpty() )
		{

			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*DevicePath) ||
			    FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*DevicePath))
			{
				DeviceName = prefix + DeviceName;
				FHTML5TargetDevicePtr& Device = Devices.FindOrAdd( DeviceName );
				if( ! Device.IsValid() ) // do not override developer's entry
				{
					Device = MakeShareable( new FHTML5TargetDevice( *this, DeviceName, DevicePath ) );
					DeviceDiscoveredEvent.Broadcast( Device.ToSharedRef() );
					if ( DefaultDeviceName.IsEmpty() )
					{
						DefaultDeviceName = DeviceName;
					}
				}
			}
		}
	}

#if PLATFORM_WINDOWS
	if ( prefix == "" )
	{
		// edge is launched via explorer.exe or start - which always exists but requires a parameter
		// this may potentially be used with other browsers (the use of additional parameters)
		// until then, special case this here...
		FString out_OSVersionLabel;
		FString out_OSSubVersionLabel;
		FPlatformMisc::GetOSVersions(out_OSVersionLabel, out_OSSubVersionLabel);
		{
			if (out_OSVersionLabel == "Windows 10")
			{
				FString DeviceName = "Edge";
				FString DevicePath = "start microsoft-edge:";
				FHTML5TargetDevicePtr& Device = Devices.FindOrAdd( DeviceName );
				if( ! Device.IsValid() ) // do not override developer's entry
				{
					Device = MakeShareable( new FHTML5TargetDevice( *this, DeviceName, DevicePath ) );
					DeviceDiscoveredEvent.Broadcast( Device.ToSharedRef() );
					if ( DefaultDeviceName.IsEmpty() )
					{
						DefaultDeviceName = DeviceName;
					}
				}
			}
		}
	}
#endif

}
