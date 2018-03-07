// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatform.inl: Implements the FAndroidTargetPlatform class.
=============================================================================*/


/* FAndroidTargetPlatform structors
 *****************************************************************************/

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Stats/Stats.h"
#include "Serialization/Archive.h"
#include "Misc/FileHelper.h"
#include "FileManager.h"
#include "PlatformFilemanager.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "Interfaces/IAndroidDeviceDetection.h"

#define LOCTEXT_NAMESPACE "FAndroidTargetPlatform"

class Error;
class FAndroidTargetDevice;
class FConfigCacheIni;
class FModuleManager;
class FScopeLock;
class FStaticMeshLODSettings;
class FTargetDeviceId;
class FTicker;
class IAndroidDeviceDetectionModule;
class UTexture;
class UTextureLODSettings;
struct FAndroidDeviceInfo;
enum class ETargetPlatformFeatures;
template<class TPlatformProperties> class FAndroidTargetPlatform;
template<typename TPlatformProperties> class TTargetPlatformBase;

static bool SupportsES2()
{
	// default to support ES2
	bool bBuildForES2 = true;
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES2"), bBuildForES2, GEngineIni);
	return bBuildForES2;
}

static bool SupportsES31()
{
	// default no support for ES31
	bool bBuildForES31 = false;
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES31"), bBuildForES31, GEngineIni);
	return bBuildForES31;
}

static bool SupportsAEP()
{
	return false;
}

static bool SupportsVulkan()
{
	// default to not supporting Vulkan
	bool bSupportsVulkan = false;
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkan"), bSupportsVulkan, GEngineIni);

	// glslang library is needed for vulkan shader compiling
	bool GlslangAvailable = false;
#if PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		GlslangAvailable = true;
	#endif
#elif PLATFORM_MAC
	GlslangAvailable = true;
#elif PLATFORM_LINUX
	GlslangAvailable = false;	// @TODO: change when glslang library compiled for Linux
#endif

	return bSupportsVulkan && GlslangAvailable;
}

static FString GetLicensePath()
{
	auto &AndroidDeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection");
	IAndroidDeviceDetection* DeviceDetection = AndroidDeviceDetection.GetAndroidDeviceDetection();
	FString ADBPath = DeviceDetection->GetADBPath();

	if (!FPaths::FileExists(*ADBPath))
	{
		return TEXT("");
	}

	// strip off the adb.exe part
	FString PlatformToolsPath;
	FString Filename;
	FString Extension;
	FPaths::Split(ADBPath, PlatformToolsPath, Filename, Extension);

	// remove the platform-tools part and point to licenses
	FPaths::NormalizeDirectoryName(PlatformToolsPath);
	FString LicensePath = PlatformToolsPath + "/../licenses";
	FPaths::CollapseRelativeDirectories(LicensePath);

	return LicensePath;
}

static bool GetLicenseHash(FSHAHash& LicenseHash)
{
	bool bLicenseValid = false;

	// from Android SDK Tools 25.2.3
	FString LicenseFilename = FPaths::EngineDir() + TEXT("Source/ThirdParty/Android/package.xml");

	// Create file reader
	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*LicenseFilename));
	if (FileReader)
	{
		// Create buffer for file input
		uint32 BufferSize = FileReader->TotalSize();
		uint8* Buffer = (uint8*)FMemory::Malloc(BufferSize);
		FileReader->Serialize(Buffer, BufferSize);

		uint8 StartPattern[] = "<license id=\"android-sdk-license\" type=\"text\">";
		int32 StartPatternLength = strlen((char *)StartPattern);

		uint8* LicenseStart = Buffer;
		uint8* BufferEnd = Buffer + BufferSize - StartPatternLength;
		while (LicenseStart < BufferEnd)
		{
			if (!memcmp(LicenseStart, StartPattern, StartPatternLength))
			{
				break;
			}
			LicenseStart++;
		}

		if (LicenseStart < BufferEnd)
		{
			LicenseStart += StartPatternLength;

			uint8 EndPattern[] = "</license>";
			int32 EndPatternLength = strlen((char *)EndPattern);

			uint8* LicenseEnd = LicenseStart;
			BufferEnd = Buffer + BufferSize - EndPatternLength;
			while (LicenseEnd < BufferEnd)
			{
				if (!memcmp(LicenseEnd, EndPattern, EndPatternLength))
				{
					break;
				}
				LicenseEnd++;
			}

			if (LicenseEnd < BufferEnd)
			{
				int32 LicenseLength = LicenseEnd - LicenseStart;
				FSHA1::HashBuffer(LicenseStart, LicenseLength, LicenseHash.Hash);
				bLicenseValid = true;
			}
		}
		FMemory::Free(Buffer);
	}

	return bLicenseValid;
}

static bool HasLicense()
{
	FString LicensePath = GetLicensePath();

	if (LicensePath.IsEmpty())
	{
		return false;
	}

	// directory must exist
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*LicensePath))
	{
		return false;
	}

	// license file must exist
	FString LicenseFilename = LicensePath + "/android-sdk-license";
	if (!PlatformFile.FileExists(*LicenseFilename))
	{
		return false;
	}

	FSHAHash LicenseHash;
	if (!GetLicenseHash(LicenseHash))
	{
		return false;
	}

	// contents must match hash of license text
	FString FileData = "";
	FFileHelper::LoadFileToString(FileData, *LicenseFilename);
	TArray<FString> lines;
	int32 lineCount = FileData.ParseIntoArray(lines, TEXT("\n"), true);

	FString LicenseString = LicenseHash.ToString().ToLower();
	for (FString &line : lines)
	{
		if (line.TrimStartAndEnd().Equals(LicenseString))
		{
			return true;
		}
	}

	// doesn't match
	return false;
}

template<class TPlatformProperties>
inline FAndroidTargetPlatform<TPlatformProperties>::FAndroidTargetPlatform( ) :
	DeviceDetection(nullptr)
{
	#if WITH_ENGINE
		FConfigCacheIni::LoadLocalIniFile(EngineSettings, TEXT("Engine"), true, *TTargetPlatformBase<TPlatformProperties>::PlatformName());
			TextureLODSettings = nullptr; // These are registered by the device profile system.
		StaticMeshLODSettings.Initialize(EngineSettings);
	#endif

	TickDelegate = FTickerDelegate::CreateRaw(this, &FAndroidTargetPlatform::HandleTicker);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 4.0f);
}


template<class TPlatformProperties>
inline FAndroidTargetPlatform<TPlatformProperties>::~FAndroidTargetPlatform()
{ 
	 FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}


/* ITargetPlatform overrides
 *****************************************************************************/

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	OutDevices.Reset();

	for (auto Iter = Devices.CreateConstIterator(); Iter; ++Iter)
	{
		OutDevices.Add(Iter.Value());
	}
}

template<class TPlatformProperties>
inline ECompressionFlags FAndroidTargetPlatform<TPlatformProperties>::GetBaseCompressionMethod( ) const
{
	return COMPRESS_ZLIB;
}

template<class TPlatformProperties>
inline ITargetDevicePtr FAndroidTargetPlatform<TPlatformProperties>::GetDefaultDevice( ) const
{
	// return the first device in the list
	if (Devices.Num() > 0)
	{
		auto Iter = Devices.CreateConstIterator();
		if (Iter)
		{
			return Iter.Value();
		}
	}

	return nullptr;
}

template<class TPlatformProperties>
inline ITargetDevicePtr FAndroidTargetPlatform<TPlatformProperties>::GetDevice( const FTargetDeviceId& DeviceId )
{
	if (DeviceId.GetPlatformName() == TTargetPlatformBase<TPlatformProperties>::PlatformName())
	{
		return Devices.FindRef(DeviceId.GetDeviceName());
	}

	return nullptr;
}

template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::IsRunningPlatform( ) const
{
	return false; // This platform never runs the target platform framework
}


template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const
{
	OutDocumentationPath = FString("Shared/Tutorials/SettingUpAndroidTutorial");
	return true;
}

template<class TPlatformProperties>
inline int32 FAndroidTargetPlatform<TPlatformProperties>::CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const
{
	OutDocumentationPath = TEXT("Platforms/Android/GettingStarted");

	int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready;
	if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
	{
		bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
	}

	bool bEnableGradle;
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bEnableGradle"), bEnableGradle, GEngineIni);

	if (bEnableGradle)
	{
		// need to check license was accepted
		if (!HasLicense())
		{
			CustomizedLogMessage = LOCTEXT("AndroidLicenseNotAcceptedMessageDetail", "SDK License must be accepted in the Android project settings to deploy your app to the device.");
			bReadyToBuild |= ETargetPlatformReadyStatus::LicenseNotAccepted;
		}
	}

	return bReadyToBuild;
}

template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	switch (Feature)
	{
		case ETargetPlatformFeatures::Packaging:
			return true;
			
		case ETargetPlatformFeatures::LowQualityLightmaps:
		case ETargetPlatformFeatures::MobileRendering:
			return SupportsES31() || SupportsES2() || SupportsVulkan();
			
		case ETargetPlatformFeatures::HighQualityLightmaps:
		case ETargetPlatformFeatures::Tessellation:
		case ETargetPlatformFeatures::DeferredRendering:
			return SupportsAEP();
			
		default:
			break;
	}
	
	return TTargetPlatformBase<TPlatformProperties>::SupportsFeature(Feature);
}


#if WITH_ENGINE

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_OPENGL_ES2(TEXT("GLSL_ES2"));
	static FName NAME_GLSL_310_ES_EXT(TEXT("GLSL_310_ES_EXT"));
	static FName NAME_SF_VULKAN_ES31_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID"));
	static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));

	if (SupportsVulkan())
	{
		OutFormats.AddUnique(NAME_SF_VULKAN_ES31_ANDROID);
	}

	if (SupportsES2())
	{
		OutFormats.AddUnique(NAME_OPENGL_ES2);
	}

	if (SupportsES31())
	{
		OutFormats.AddUnique(NAME_GLSL_ES3_1_ANDROID);
	}

	if (SupportsAEP())
	{
		OutFormats.AddUnique(NAME_GLSL_310_ES_EXT);
	}
}

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}


template<class TPlatformProperties>
inline const FStaticMeshLODSettings& FAndroidTargetPlatform<TPlatformProperties>::GetStaticMeshLODSettings( ) const
{
	return StaticMeshLODSettings;
}


template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const
{
	check(InTexture);

	// The order we add texture formats to OutFormats is important. When multiple formats are cooked
	// and supported by the device, the first supported format listed will be used. 
	// eg, ETC1/uncompressed should always be last

	bool bNoCompression = InTexture->CompressionNone				// Code wants the texture uncompressed.
		|| (InTexture->LODGroup == TEXTUREGROUP_ColorLookupTable)	// Textures in certain LOD groups should remain uncompressed.
		|| (InTexture->LODGroup == TEXTUREGROUP_Bokeh)
		|| (InTexture->CompressionSettings == TC_EditorIcon)
		|| (InTexture->Source.GetSizeX() < 4)						// Don't compress textures smaller than the DXT block size.
		|| (InTexture->Source.GetSizeY() < 4)
		|| (InTexture->Source.GetSizeX() % 4 != 0)
		|| (InTexture->Source.GetSizeY() % 4 != 0);

	bool bIsNonPOT = false;
#if WITH_EDITORONLY_DATA
	// is this texture not a power of 2?
	bIsNonPOT = !InTexture->Source.IsPowerOfTwo();
#endif

	// Determine the pixel format of the compressed texture.
	if (InTexture->LODGroup == TEXTUREGROUP_Shadowmap)
	{
		// forward rendering only needs one channel for shadow maps
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (bNoCompression && InTexture->HasHDRSource())
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (bNoCompression)
	{
		OutFormats.Add(AndroidTexFormat::NameBGRA8);
	}
	else if (InTexture->CompressionSettings == TC_HDR
		|| InTexture->CompressionSettings == TC_HDR_Compressed)
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (InTexture->CompressionSettings == TC_Normalmap)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
	else if (InTexture->CompressionSettings == TC_Displacementmap)
	{
		OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	}
	else if (InTexture->CompressionSettings == TC_VectorDisplacementmap)
	{
		OutFormats.Add(AndroidTexFormat::NameBGRA8);
	}
	else if (InTexture->CompressionSettings == TC_Grayscale)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->CompressionSettings == TC_Alpha)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->CompressionSettings == TC_DistanceFieldFont)
	{
		OutFormats.Add(AndroidTexFormat::NameG8);
	}
	else if (InTexture->bForcePVRTC4
		|| InTexture->CompressionSettings == TC_BC7)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
	else if (InTexture->CompressionNoAlpha)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT1, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGB, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameETC2_RGB, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameETC1, OutFormats, bIsNonPOT);
	}
	else if (InTexture->bDitherMipMapAlpha)
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
	else
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoPVRTC, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoDXT, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoATC, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT);
	}
}



template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::GetAllTextureFormats(TArray<FName>& OutFormats) const
{


	OutFormats.Add(AndroidTexFormat::NameG8);
	OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	OutFormats.Add(AndroidTexFormat::NameBGRA8);
	OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	OutFormats.Add(AndroidTexFormat::NameRGBA16F);
	OutFormats.Add(AndroidTexFormat::NameBGRA8);
	OutFormats.Add(AndroidTexFormat::NameG8);
	OutFormats.Add(AndroidTexFormat::NameG8);
	OutFormats.Add(AndroidTexFormat::NameG8);

	auto AddAllTextureFormatIfSupports = [=, &OutFormats](bool bIsNonPOT) 
	{
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoPVRTC, OutFormats, bIsNonPOT); 
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC2, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NamePVRTC4, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoDXT, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT1, OutFormats, bIsNonPOT);
		AddTextureFormatIfSupports(AndroidTexFormat::NameDXT5, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGB, OutFormats, bIsNonPOT); 
		AddTextureFormatIfSupports(AndroidTexFormat::NameATC_RGBA_I, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC1, OutFormats, bIsNonPOT); 
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoETC2, OutFormats, bIsNonPOT);
	
		AddTextureFormatIfSupports(AndroidTexFormat::NameAutoATC, OutFormats, bIsNonPOT);
	};

	AddAllTextureFormatIfSupports(true);
	AddAllTextureFormatIfSupports(false);

}


template<class TPlatformProperties>
void FAndroidTargetPlatform<TPlatformProperties>::GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const
{
	if (SupportsAEP())
	{
		// use Full HDR with AEP
		OutFormats.Add(FName(TEXT("FullHDR")));
	}
	
	// always emit encoded
	OutFormats.Add(FName(TEXT("EncodedHDR")));
}


template<class TPlatformProperties>
const UTextureLODSettings& FAndroidTargetPlatform<TPlatformProperties>::GetTextureLODSettings() const
{
	return *TextureLODSettings;
}


template<class TPlatformProperties>
FName FAndroidTargetPlatform<TPlatformProperties>::GetWaveFormat( const class USoundWave* Wave ) const
{
	static bool formatRead = false;
	static FName NAME_FORMAT;

	if (!formatRead)
	{
		formatRead = true;

		FString audioSetting;
		if (!GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("AndroidAudio"), audioSetting, GEngineIni))
		{
			audioSetting = TEXT("DEFAULT");
		}

#if WITH_OGGVORBIS
		if (audioSetting == TEXT("OGG") || audioSetting == TEXT("Default"))
		{
			static FName NAME_OGG(TEXT("OGG"));
			NAME_FORMAT = NAME_OGG;
		}
#else
		if (audioSetting == TEXT("OGG"))
		{
			UE_LOG(LogAudio, Error, TEXT("Attemped to select Ogg Vorbis encoding when the cooker is built without Ogg Vorbis support."));
		}
#endif
		else
		{
	
			// Otherwise return ADPCM as it'll either be option '2' or 'default' depending on WITH_OGGVORBIS config
			static FName NAME_ADPCM(TEXT("ADPCM"));
			NAME_FORMAT = NAME_ADPCM;
		}
	}
	return NAME_FORMAT;
}


template<class TPlatformProperties>
void FAndroidTargetPlatform<TPlatformProperties>::GetAllWaveFormats(TArray<FName>& OutFormats) const
{
	static FName NAME_OGG(TEXT("OGG"));
	static FName NAME_ADPCM(TEXT("ADPCM"));

	OutFormats.Add(NAME_OGG); 
	OutFormats.Add(NAME_ADPCM);
}

#endif //WITH_ENGINE

template<class TPlatformProperties>
bool FAndroidTargetPlatform<TPlatformProperties>::SupportsVariants() const
{
	return true;
}

template<class TPlatformProperties>
FText FAndroidTargetPlatform<TPlatformProperties>::GetVariantTitle() const
{
	return LOCTEXT("AndroidVariantTitle", "Texture Format");
}

/* FAndroidTargetPlatform implementation
 *****************************************************************************/

template<class TPlatformProperties>
inline void FAndroidTargetPlatform<TPlatformProperties>::AddTextureFormatIfSupports( FName Format, TArray<FName>& OutFormats, bool bIsCompressedNonPOT ) const
{
	if (SupportsTextureFormat(Format))
	{
		if (bIsCompressedNonPOT && SupportsCompressedNonPOT() == false)
		{
			OutFormats.Add(AndroidTexFormat::NamePOTERROR);
		}
		else
		{
			OutFormats.Add(Format);
		}
	}
}


/* FAndroidTargetPlatform callbacks
 *****************************************************************************/

template<class TPlatformProperties>
inline bool FAndroidTargetPlatform<TPlatformProperties>::HandleTicker( float DeltaTime )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAndroidTargetPlatform_HandleTicker);

	if (DeviceDetection == nullptr)
	{
		DeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection").GetAndroidDeviceDetection();
	}

	TArray<FString> ConnectedDeviceIds;

	{
		FScopeLock ScopeLock(DeviceDetection->GetDeviceMapLock());

		auto DeviceIt = DeviceDetection->GetDeviceMap().CreateConstIterator();
		
		for (; DeviceIt; ++DeviceIt)
		{
			ConnectedDeviceIds.Add(DeviceIt.Key());

			const FAndroidDeviceInfo& DeviceInfo = DeviceIt.Value();

			// see if this device is already known
			if (Devices.Contains(DeviceIt.Key()))
			{
				FAndroidTargetDevicePtr TestDevice = Devices[DeviceIt.Key()];

				// ignore if authorization didn't change
				if (DeviceInfo.bAuthorizedDevice == TestDevice->IsAuthorized())
				{
					continue;
				}

				// remove it to add again
				TestDevice->SetConnected(false);
				Devices.Remove(DeviceIt.Key());

				DeviceLostEvent.Broadcast(TestDevice.ToSharedRef());
			}

			// check if this platform is supported by the extensions and version
			if (!SupportedByExtensionsString(DeviceInfo.GLESExtensions, DeviceInfo.GLESVersion))
			{
				continue;
			}

			// create target device
			FAndroidTargetDevicePtr& Device = Devices.Add(DeviceInfo.SerialNumber);

			Device = MakeShareable(new FAndroidTargetDevice(*this, DeviceInfo.SerialNumber, GetAndroidVariantName()));

			Device->SetConnected(true);
			Device->SetModel(DeviceInfo.Model);
			Device->SetDeviceName(DeviceInfo.DeviceName);
			Device->SetAuthorized(DeviceInfo.bAuthorizedDevice);
			Device->SetVersions(DeviceInfo.SDKVersion, DeviceInfo.HumanAndroidVersion);

			DeviceDiscoveredEvent.Broadcast(Device.ToSharedRef());
		}
	}

	// remove disconnected devices
	for (auto Iter = Devices.CreateIterator(); Iter; ++Iter)
	{
		if (!ConnectedDeviceIds.Contains(Iter.Key()))
		{
			FAndroidTargetDevicePtr RemovedDevice = Iter.Value();
			RemovedDevice->SetConnected(false);

			Iter.RemoveCurrent();

			DeviceLostEvent.Broadcast(RemovedDevice.ToSharedRef());
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
