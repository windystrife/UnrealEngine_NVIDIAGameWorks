// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealEngine.cpp: Implements the UEngine class and helpers.
=============================================================================*/

#include "UnrealEngine.h"
#include "UObject/GCObject.h"
#include "Misc/IQueuedWork.h"
#include "HAL/RunnableThread.h"
#include "RHI.h"
#include "Widgets/SWidget.h"
#include "UnrealClient.h"
#include "Engine/DebugDisplayProperty.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/GameViewportClient.h"
#include "RenderingThread.h"
#include "RHIStaticStates.h"
#include "Engine/TextureStreamingTypes.h"
#include "Components/PrimitiveComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/Runnable.h"
#include "Misc/OutputDeviceArchiveWrapper.h"
#include "Stats/StatsMisc.h"
#include "Containers/Ticker.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/App.h"
#include "Misc/TimeGuard.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/ObjectMemoryAnalyzer.h"
#include "Serialization/ArchiveCountMem.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ArchiveTraceRoute.h"
#include "Misc/PackageName.h"
#include "Misc/EngineVersion.h"
#include "UObject/LinkerLoad.h"
#include "Misc/StartupPackages.h"
#include "GameMapsSettings.h"
#include "Materials/MaterialInterface.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Misc/WildcardString.h"
#include "Misc/OutputDeviceConsole.h"
#include "Serialization/ArchiveReplaceOrClearExternalReferences.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Font.h"
#include "Materials/Material.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Sound/SoundAttenuation.h"
#include "GameFramework/GameModeBase.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/WorldSettings.h"
#include "Components/AudioComponent.h"
#include "Particles/ParticleSystem.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "ParticleHelper.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleSystemComponent.h"
#include "Exporters/Exporter.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/NetDriver.h"
#include "Widgets/SBoxPanel.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "SystemSettings.h"
#include "ContentStreaming.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Engine/TextureLODSettings.h"
#include "Engine/LevelStreamingPersistent.h"
#include "Engine/ObjectReferencer.h"
#include "Misc/NetworkVersion.h"
#include "Net/OnlineEngineInterface.h"
#include "Engine/Console.h"
#include "VisualLogger/VisualLogger.h"
#include "SkeletalMeshMerge.h"
#include "ShaderCompiler.h"
#include "Slate/SlateSoundDevice.h"
#include "DerivedDataCacheInterface.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "EngineAnalytics.h"
#include "TickTaskManagerInterface.h"
#include "Net/NetworkProfiler.h"
#include "ProfilingDebugging/MallocProfiler.h"
#include "StereoRendering.h"
#include "IHeadMountedDisplayModule.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Stats/StatsData.h"
#include "Stats/StatsFile.h"
#include "AudioThread.h"
#include "AudioDeviceManager.h"
#include "Sound/ReverbEffect.h"
#include "AudioDevice.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Character.h"
#include "GameDelegates.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/LevelStreamingVolume.h"
#include "Engine/WorldComposition.h"
#include "Engine/LevelScriptActor.h"
#include "IHardwareSurveyModule.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

#include "Components/TextRenderComponent.h"
#include "Classes/Sound/AudioSettings.h"
#include "Streaming/Texture2DUpdate.h"


#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif
// @third party code - BEGIN HairWorks
#include "HairWorksSDK.h"
// @third party code - END HairWorks

// @todo this is here only due to circular dependency to AIModule. To be removed

#if WITH_EDITORONLY_DATA
#include "ObjectEditorUtils.h"
#endif

#if WITH_EDITOR
#include "AudioEditorModule.h"
#endif

#include "HardwareInfo.h"
#include "EngineModule.h"
#include "UnrealExporter.h"
#include "BufferVisualizationData.h"

#include "Misc/HotReloadInterface.h"
#include "Widgets/Testing/STestSuite.h"
#include "Engine/DemoNetDriver.h"
#include "Widgets/Images/SThrobber.h"
#include "Engine/TextureCube.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "Engine/GameEngine.h"
#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "Components/BrushComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/UserInterfaceSettings.h"
#include "ComponentRecreateRenderStateContext.h"

#include "IMessageRpcClient.h"
#include "IMessagingRpcModule.h"
#include "IPortalRpcModule.h"
#include "IPortalRpcLocator.h"
#include "IPortalServicesModule.h"
#include "IPortalServiceLocator.h"
#include "Misc/TypeContainer.h"

#include "IMovieSceneCapture.h"
#include "MovieSceneCaptureModule.h"
#include "GameFramework/OnlineSession.h"
#include "ProfilingDebugging/ABTesting.h"
#include "Performance/EnginePerformanceTargets.h"

#include "InstancedReferenceSubobjectHelper.h"
#include "Engine/EndUserSettings.h"

#include "Engine/LODActor.h"
#include "Engine/AssetManager.h"
#include "GameplayTagsManager.h"

#if !UE_BUILD_SHIPPING
	#include "HAL/ExceptionHandling.h"
	#include "IAutomationWorkerModule.h"
#endif	// UE_BUILD_SHIPPING

#if ENABLE_LOC_TESTING
	#include "LocalizationModule.h"
#endif

#include "GeneralProjectSettings.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "ObjectKey.h"
#include "AssetRegistryModule.h"

#if !UE_BUILD_SHIPPING
	#include "IPluginManager.h"
	#include "GenericPlatformCrashContext.h"
	#include "EngineBuildSettings.h"
#endif

#include "FileManagerGeneric.h"

DEFINE_LOG_CATEGORY(LogEngine);
IMPLEMENT_MODULE( FEngineModule, Engine );

#define LOCTEXT_NAMESPACE "UnrealEngine"


void OnChangeEngineCVarRequiringRecreateRenderState(IConsoleVariable* Var)
{
	// Propgate cvar change to static draw lists
	FGlobalComponentRecreateRenderStateContext Context;
}

void FEngineModule::StartupModule()
{
	// Setup delegate callback for ProfilingHelpers to access current map name
	extern const FString GetMapNameStatic();
	GGetMapNameDelegate.BindStatic(&GetMapNameStatic);

	static auto CVarCacheWPOPrimitives = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.CacheWPOPrimitives"));
	CVarCacheWPOPrimitives->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnChangeEngineCVarRequiringRecreateRenderState));

	SuspendTextureStreamingRenderTasks = &SuspendTextureStreamingRenderTasksInternal;
	ResumeTextureStreamingRenderTasks = &ResumeTextureStreamingRenderTasksInternal;
}


/* Global variables
 *****************************************************************************/

/**
 * Global engine pointer. Can be 0 so don't use without checking.
 */
ENGINE_API UEngine*	GEngine = NULL;

/**
 * Whether to visualize the light map selected by the Debug Camera.
 */
ENGINE_API bool GShowDebugSelectedLightmap = false;

#if WITH_PROFILEGPU
	/**
	 * true if we debug material names with SCOPED_DRAW_EVENT.	 
	 */
	int32 GShowMaterialDrawEvents = 0;	
	static FAutoConsoleVariableRef CVARShowMaterialDrawEvents(
		TEXT("r.ShowMaterialDrawEvents"),
		GShowMaterialDrawEvents,
		TEXT("Enables a draw event around each material draw if supported by the platform"),
		ECVF_Default
		);
#endif

ENGINE_API uint32 GGPUFrameTime = 0;

/** System resolution instance */
FSystemResolution GSystemResolution;

static TAutoConsoleVariable<float> CVarDebugTextScale(
	TEXT("r.DebugTextScale"),
	1.0,
	TEXT("Sets the scale of the debug text.\n"),
	ECVF_Default);

TAutoConsoleVariable<int32> CVarAllowOneFrameThreadLag(
	TEXT("r.OneFrameThreadLag"),
	1,
	TEXT("Whether to allow the rendering thread to lag one frame behind the game thread (0: disabled, otherwise enabled)")
	);

static FAutoConsoleVariable CVarSystemResolution(
	TEXT("r.SetRes"),
	TEXT("1280x720w"),
	TEXT("Set the display resolution for the current game view. Has no effect in the editor.\n")
	TEXT("e.g. 1280x720w for windowed\n")
	TEXT("     1920x1080f for fullscreen\n")
	TEXT("     1920x1080wf for windowed fullscreen\n")
	);

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<float> CVarSetOverrideFPS(
	TEXT("t.OverrideFPS"),
	0.0f,
	TEXT("This allows to override the frame time measurement with a fixed fps number (game can run faster or slower).\n")
	TEXT("<=0:off, in frames per second, e.g. 60"),
	ECVF_Cheat);
#endif // !UE_BUILD_SHIPPING

// Should we show errors and warnings (when DurationOfErrorsAndWarningsOnHUD is greater than zero), or only errors?
int32 GSupressWarningsInOnScreenDisplay = 0;
static FAutoConsoleVariableRef GSupressWarningsInOnScreenDisplayCVar(
	TEXT("Engine.SupressWarningsInOnScreenDisplay"),
	GSupressWarningsInOnScreenDisplay,
	TEXT("0: Show both errors and warnings on screen, 1: Show only errors on screen (in either case only when DurationOfErrorsAndWarningsOnHUD is greater than zero)"),
	ECVF_Default
);

/** Whether texture memory has been corrupted because we ran out of memory in the pool. */
bool GIsTextureMemoryCorrupted = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Whether PrepareMapChange is attempting to load a map that doesn't exist */
	bool GIsPrepareMapChangeBroken = false;
#endif

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FSimpleMulticastDelegate UEngine::OnPostEngineInit;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

// We expose these variables to everyone as we need to access them in other files via an extern
ENGINE_API float GAverageFPS = 0.0f;
ENGINE_API float GAverageMS = 0.0f;
ENGINE_API double GLastMemoryWarningTime = 0.f;

static FCachedSystemScalabilityCVars GCachedScalabilityCVars;

const FCachedSystemScalabilityCVars& GetCachedScalabilityCVars()
{
	check(GCachedScalabilityCVars.bInitialized);
	return GCachedScalabilityCVars;
}

FCachedSystemScalabilityCVars::FCachedSystemScalabilityCVars()
	: bInitialized(false)
	, DetailMode(-1)
	, MaterialQualityLevel(EMaterialQualityLevel::Num)
	, MaxShadowResolution(-1)
	, MaxCSMShadowResolution(-1)
	, ViewDistanceScale(-1)
	, ViewDistanceScaleSquared(-1)
	, MaxAnisotropy(-1)
{

}

void ScalabilityCVarsSinkCallback()
{
	IConsoleManager& ConsoleMan = IConsoleManager::Get();

	FCachedSystemScalabilityCVars LocalScalabilityCVars = GCachedScalabilityCVars;

	{
		static const auto DetailMode = ConsoleMan.FindTConsoleVariableDataInt(TEXT("r.DetailMode"));
		LocalScalabilityCVars.DetailMode = DetailMode->GetValueOnGameThread();
	}

	{
		static const auto* MaxAnisotropy = ConsoleMan.FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));
		LocalScalabilityCVars.MaxAnisotropy = MaxAnisotropy->GetValueOnGameThread();
	}

	{
		static const auto* MaxShadowResolution = ConsoleMan.FindTConsoleVariableDataInt(TEXT("r.Shadow.MaxResolution"));
		LocalScalabilityCVars.MaxShadowResolution = MaxShadowResolution->GetValueOnGameThread();
	}

	{
		static const auto* MaxCSMShadowResolution = ConsoleMan.FindTConsoleVariableDataInt(TEXT("r.Shadow.MaxCSMResolution"));
		LocalScalabilityCVars.MaxCSMShadowResolution = MaxCSMShadowResolution->GetValueOnGameThread();
	}

	{
		static const auto ViewDistanceScale = ConsoleMan.FindTConsoleVariableDataFloat(TEXT("r.ViewDistanceScale"));
		LocalScalabilityCVars.ViewDistanceScale = FMath::Max(ViewDistanceScale->GetValueOnGameThread(), 0.0f);
		LocalScalabilityCVars.ViewDistanceScaleSquared = FMath::Square(LocalScalabilityCVars.ViewDistanceScale);
	}

	{
		static const auto MaterialQualityLevelVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaterialQualityLevel"));
		LocalScalabilityCVars.MaterialQualityLevel = (EMaterialQualityLevel::Type)FMath::Clamp(MaterialQualityLevelVar->GetValueOnGameThread(), 0, (int32)EMaterialQualityLevel::Num - 1);
	}

	LocalScalabilityCVars.bInitialized = true;

	FlushRenderingCommands();

	if (!GCachedScalabilityCVars.bInitialized)
	{
		// optimization: the first time we assume the render thread wasn't started and we don't need to destroy proxies
		GCachedScalabilityCVars = LocalScalabilityCVars;
	}
	else
	{
		bool bRecreateRenderstate = false;
		bool bCacheResourceShaders = false;

		if (LocalScalabilityCVars.DetailMode != GCachedScalabilityCVars.DetailMode)
		{
			bRecreateRenderstate = true;
		}

		if (LocalScalabilityCVars.MaterialQualityLevel != GCachedScalabilityCVars.MaterialQualityLevel)
		{
			bCacheResourceShaders = true;
		}

		if (bRecreateRenderstate || bCacheResourceShaders)
		{
			// after FlushRenderingCommands() to not have render thread pick up the data partially
			GCachedScalabilityCVars = LocalScalabilityCVars;

			// Note: constructor and destructor has side effect
			FGlobalComponentRecreateRenderStateContext Recreate;

			if (bCacheResourceShaders)
			{
				// For all materials, UMaterial::CacheResourceShadersForRendering
				UMaterial::AllMaterialsCacheResourceShadersForRendering();
				UMaterialInstance::AllMaterialsCacheResourceShadersForRendering();
			}
		}
		else
		{
			GCachedScalabilityCVars = LocalScalabilityCVars;
		}
	}
}

static bool GHDROutputEnabled = false;

bool ParseResolution(const TCHAR* InResolution, uint32& OutX, uint32& OutY, int32& OutWindowMode)
{
	if(*InResolution)
	{
		FString CmdString(InResolution);
		CmdString = CmdString.TrimStartAndEnd().ToLower();

		//Retrieve the X dimensional value
		const uint32 X = FMath::Max(FCString::Atof(*CmdString), 0.0f);

		// Determine whether the user has entered a resolution and extract the Y dimension.
		FString YString;

		// Find separator between values (Example of expected format: 1280x768)
		const TCHAR* YValue = NULL;
		if(FCString::Strchr(*CmdString,'x'))
		{
			YValue = const_cast<TCHAR*> (FCString::Strchr(*CmdString,'x')+1);
			YString = YValue;
			// Remove any whitespace from the end of the string
			YString = YString.TrimStartAndEnd();
		}

		// If the Y dimensional value exists then setup to use the specified resolution.
		uint32 Y = 0;
		if ( YValue && YString.Len() > 0 )
		{
			// See if there is a fullscreen flag on the end
			FString FullScreenChar = YString.Mid(YString.Len() - 1);
			FString WindowFullScreenChars = YString.Mid(YString.Len() - 2);
			int32 WindowMode = OutWindowMode;
			if (!FullScreenChar.IsNumeric())
			{
				int StringTripLen = 0;

				if (WindowFullScreenChars == TEXT("wf"))
				{
					WindowMode = EWindowMode::WindowedFullscreen;
					StringTripLen = 2;
				}
				else if (FullScreenChar == TEXT("f"))
				{
					WindowMode = EWindowMode::Fullscreen;
					StringTripLen = 1;
				}
				else if (FullScreenChar == TEXT("w"))
				{
					WindowMode = EWindowMode::Windowed;
					StringTripLen = 1;
				}

				YString = YString.Left(YString.Len() - StringTripLen).TrimStartAndEnd();
			}

			if (YString.IsNumeric())
			{
				Y = FMath::Max(FCString::Atof(YValue), 0.0f);
				OutX = X;
				OutY = Y;
				OutWindowMode = WindowMode;
				return true;
			}
		}
	}
	return false;
}

void SystemResolutionSinkCallback()
{
	auto ResString = CVarSystemResolution->GetString();
	
	uint32 ResX, ResY;
	int32 WindowModeInt = GSystemResolution.WindowMode;

	bool bHDROutputEnabled = GRHISupportsHDROutput && IsHDREnabled();
	
	if (ParseResolution(*ResString, ResX, ResY, WindowModeInt))
	{
		EWindowMode::Type WindowMode = EWindowMode::ConvertIntToWindowMode(WindowModeInt);

		if( GSystemResolution.ResX != ResX ||
			GSystemResolution.ResY != ResY ||
			GSystemResolution.WindowMode != WindowMode ||
			GHDROutputEnabled != bHDROutputEnabled ||
			GSystemResolution.bForceRefresh)
		{
			GSystemResolution.ResX = ResX;
			GSystemResolution.ResY = ResY;
			GSystemResolution.WindowMode = WindowMode;
			GSystemResolution.bForceRefresh = false;
			GHDROutputEnabled = bHDROutputEnabled;

			if(GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame)
			{
				GEngine->GameViewport->ViewportFrame->ResizeFrame(ResX, ResY, WindowMode);
			}
		}
	}
}

/**
 * if we need to update the sample states
*/
void RefreshSamplerStatesCallback()
{
	if (FApp::CanEverRender() == false)
	{
		// Avoid unnecessary work when running in dedicated server mode.
		return;
	}

	bool bRefreshSamplerStates = false;

	{
		float MipMapBiasOffset = UTexture2D::GetGlobalMipMapLODBias();
		static float LastMipMapLODBias = 0;

		if(LastMipMapLODBias != MipMapBiasOffset)
		{
			LastMipMapLODBias = MipMapBiasOffset;
			bRefreshSamplerStates = true;
		}
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));
		int32 MaxAnisotropy = CVar->GetValueOnGameThread();
		// compare against the default so with that number we avoid RefreshSamplerStates() calls on startup
		// todo: This can be improved since we now have many defaults (see BaseScalability.ini)
		static int32 LastMaxAnisotropy = 4;

		if(LastMaxAnisotropy != MaxAnisotropy)
		{
			LastMaxAnisotropy = MaxAnisotropy;
			bRefreshSamplerStates = true;
		}
	}

	if(bRefreshSamplerStates)
	{
		for (TObjectIterator<UTexture2D>It; It; ++It)
		{
			UTexture2D* Texture = *It;
			Texture->RefreshSamplerStates();
		}
		UMaterialInterface::RecacheAllMaterialUniformExpressions();
	}
}

void RefreshEngineSettings()
{
	extern void FreeSkeletalMeshBuffersSinkCallback();

	RefreshSamplerStatesCallback();
	ScalabilityCVarsSinkCallback();
	FreeSkeletalMeshBuffersSinkCallback();
	SystemResolutionSinkCallback();
}

FConsoleVariableSinkHandle GRefreshEngineSettingsSinkHandle;

ENGINE_API void InitializeRenderingCVarsCaching()
{
	GRefreshEngineSettingsSinkHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(FConsoleCommandDelegate::CreateStatic(&RefreshEngineSettings));

	// Initialise this to invalid
	GCachedScalabilityCVars.MaterialQualityLevel = EMaterialQualityLevel::Num;

	// Initial cache
	SystemResolutionSinkCallback();
	ScalabilityCVarsSinkCallback();
}

static void ShutdownRenderingCVarsCaching()
{
	IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(GRefreshEngineSettingsSinkHandle);
}

static bool HandleDumpShaderPipelineStatsCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	FString FlagStr(FParse::Token(Cmd, 0));
	EShaderPlatform Platform = GMaxRHIShaderPlatform;
	if (FlagStr.Len() > 0)
	{
		Platform = ShaderFormatToLegacyShaderPlatform(FName(*FlagStr));
	}
	Ar.Logf(TEXT("Dumping shader pipeline stats for platform %s"), *LegacyShaderPlatformToShaderFormat(Platform).ToString());

	DumpShaderPipelineStats(Platform);
	return true;
}

namespace
{
	/**
	 * Attempts to set process limits as configured in Engine.ini or elsewhere.
	 * Assumed to be called during initialization.
	 */
	void SetConfiguredProcessLimits()
	{
		int32 VirtualMemoryLimitInKB = 0;
		if (GConfig)
		{
			GConfig->GetInt(TEXT("ProcessLimits"), TEXT("VirtualMemoryLimitInKB"), VirtualMemoryLimitInKB, GEngineIni);
		}
		
		// command line parameters take precendence
		FParse::Value(FCommandLine::Get(), TEXT("virtmemkb="), VirtualMemoryLimitInKB);

		if (VirtualMemoryLimitInKB > 0)
		{
			UE_LOG(LogInit, Display, TEXT("Limiting process virtual memory size to %d KB"), VirtualMemoryLimitInKB);
			if (!FPlatformProcess::SetProcessLimits(EProcessResource::VirtualMemory, static_cast< uint64 >(VirtualMemoryLimitInKB) * 1024))
			{
				UE_LOG(LogInit, Fatal, TEXT("Could not limit process virtual memory usage to %d KB"), VirtualMemoryLimitInKB);
			}
		}
	}

	UWorld* CreatePIEWorldByLoadingFromPackage(const FWorldContext& WorldContext, const FString& SourceWorldPackage, UPackage*& OutPackage)
	{
		// Load map from the disk in case editor does not have it
		const FString PIEPackageName = UWorld::ConvertToPIEPackageName(SourceWorldPackage, WorldContext.PIEInstance);

		// Set the world type in the static map, so that UWorld::PostLoad can set the world type
		const FName PIEPackageFName = FName(*PIEPackageName);
		UWorld::WorldTypePreLoadMap.FindOrAdd( PIEPackageFName ) = WorldContext.WorldType;
		FSoftObjectPath::AddPIEPackageName(PIEPackageFName);

		uint32 LoadFlags = LOAD_None;
		UPackage* NewPackage = CreatePackage(NULL, *PIEPackageName);
		if (NewPackage != nullptr && WorldContext.WorldType == EWorldType::PIE)
		{
			NewPackage->SetPackageFlags(PKG_PlayInEditor);
			LoadFlags |= LOAD_PackageForPIE;
		}
		OutPackage = LoadPackage(NewPackage, *SourceWorldPackage, LoadFlags);

		// Clean up the world type list now that PostLoad has occurred
		UWorld::WorldTypePreLoadMap.Remove( PIEPackageFName );

		if (OutPackage == nullptr)
		{
			return nullptr;
		}

		UWorld* NewWorld = UWorld::FindWorldInPackage(OutPackage);

		// If the world was not found, follow a redirector if there is one.
		if ( !NewWorld )
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(OutPackage);
			if ( NewWorld )
			{
				OutPackage = NewWorld->GetOutermost();
			}
		}

		check(NewWorld);

		OutPackage->PIEInstanceID = WorldContext.PIEInstance;
		OutPackage->SetPackageFlags(PKG_PlayInEditor);

		// Rename streaming levels to PIE
		for (ULevelStreaming* StreamingLevel : NewWorld->StreamingLevels)
		{
			StreamingLevel->RenameForPIE(WorldContext.PIEInstance);
		}

		return NewWorld;
	}
}
/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

/**
 * Compresses and decompresses thumbnails using the PNG format.  This is used by the package loading and
 * saving process.
 */
class FPNGThumbnailCompressor
	: public FThumbnailCompressionInterface
{

public:

	/**
	 * Compresses an image
	 *
	 * @param	InUncompressedData	The uncompressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutCompressedData	[Out] Compressed image data
	 *
	 * @return	true if the image was compressed successfully, otherwise false if an error occurred
	 */
	virtual bool CompressImage( const TArray< uint8 >& InUncompressedData, const int32 InWidth, const int32 InHeight, TArray< uint8 >& OutCompressedData )
	{
		bool bSucceeded = false;
		OutCompressedData.Reset();
		if( InUncompressedData.Num() > 0 )
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
			if ( ImageWrapper.IsValid() && ImageWrapper->SetRaw( &InUncompressedData[ 0 ], InUncompressedData.Num(), InWidth, InHeight, ERGBFormat::RGBA, 8 ) )
			{
				OutCompressedData = ImageWrapper->GetCompressed();
				bSucceeded = true;
			}
		}

		return bSucceeded;
	}


	/**
	 * Decompresses an image
	 *
	 * @param	InCompressedData	The compressed image data
	 * @param	InWidth				Width of the image
	 * @param	InHeight			Height of the image
	 * @param	OutUncompressedData	[Out] Uncompressed image data
	 *
	 * @return	true if the image was decompressed successfully, otherwise false if an error occurred
	 */
	virtual bool DecompressImage( const TArray< uint8 >& InCompressedData, const int32 InWidth, const int32 InHeight, TArray< uint8 >& OutUncompressedData )
	{
		bool bSucceeded = false;
		OutUncompressedData.Reset();
		if( InCompressedData.Num() > 0 )
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
			if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( &InCompressedData[ 0 ], InCompressedData.Num() ) )
			{
				check( ImageWrapper->GetWidth() == InWidth );
				check( ImageWrapper->GetHeight() == InHeight );
				const TArray<uint8>* RawData = NULL;
				if ( ImageWrapper->GetRaw( ERGBFormat::RGBA, 8, RawData ) )	// @todo CB: Eliminate image copy here? (decompress straight to buffer)
				{
					OutUncompressedData = *RawData;
					bSucceeded = true;
				}
			}
		}

		return bSucceeded;
	}
};


/**
 * Helper class inhibiting screen saver by e.g. moving the mouse by 0 pixels every 50 seconds.
 */
class FScreenSaverInhibitor : public FRunnable
{
public:
	/** Default constructor. */
	FScreenSaverInhibitor()
		: bEnabled(true)
	{}

	protected:

	bool Init() override
	{ 
		return true; 
	}

	void Stop() override
	{
		bEnabled = false;
		FPlatformMisc::MemoryBarrier();
	}

	/**
	 * Prevents screensaver from kicking in by calling FPlatformMisc::PreventScreenSaver every 50 seconds.
	 */
	uint32 Run() override
	{
		while( bEnabled )
		{
			const int32 NUM_SECONDS_TO_SLEEP = 50;
			for( int32 Sec = 0; Sec < NUM_SECONDS_TO_SLEEP && bEnabled; ++Sec )
			{
				FPlatformProcess::Sleep( 1 );
			}
			FPlatformApplicationMisc::PreventScreenSaver();
		}
		return 0;
	}

	bool bEnabled;
};

/*-----------------------------------------------------------------------------
	FWorldContext
-----------------------------------------------------------------------------*/

void FWorldContext::SetCurrentWorld(UWorld* World)
{
	if (World != nullptr)
	{
		// Set the world's audio device handle so that audio components playing in the 
		// world will use the correct audio device instance.
		World->SetAudioDeviceHandle(AudioDeviceHandle);
	}

	for (int32 idx = 0; idx < ExternalReferences.Num(); ++idx)
	{
		if (ExternalReferences[idx] && *ExternalReferences[idx] == ThisCurrentWorld)
		{
			*ExternalReferences[idx] = World;
		}
	}

	ThisCurrentWorld = World;
}

void FWorldContext::AddReferencedObjects(FReferenceCollector& Collector, const UObject* ReferencingObject)
{
	// TODO: This is awfully unsafe as anything in a WorldContext that changes may not be referenced
	//	 hopefully a utility to push the WorldContext back in to the collector with property collection
	//       will happen in the future
	Collector.AddReferencedObject(PendingNetGame, ReferencingObject);
	for (FFullyLoadedPackagesInfo& PackageInfo : PackagesToFullyLoad)
	{
		Collector.AddReferencedObjects(PackageInfo.LoadedObjects, ReferencingObject);
	}
	Collector.AddReferencedObjects(LoadedLevelsForPendingMapChange, ReferencingObject);
	Collector.AddReferencedObjects(ObjectReferencers, ReferencingObject);
	Collector.AddReferencedObject(GameViewport, ReferencingObject);
	Collector.AddReferencedObject(OwningGameInstance, ReferencingObject);
	for (FNamedNetDriver& ActiveNetDriver : ActiveNetDrivers)
	{
		Collector.AddReferencedObject(ActiveNetDriver.NetDriver, ReferencingObject);
	}
	Collector.AddReferencedObject(ThisCurrentWorld, ReferencingObject);
}

/*-----------------------------------------------------------------------------
	World/ Level/ Actor GC verification.
-----------------------------------------------------------------------------*/

#if STATS

/** Used by a delegate for access to player's viewpoint from StatsNotifyProviders */
void GetFirstPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation)
{
	ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();
	if( Player != NULL && Player->PlayerController != NULL )
	{
		// Calculate the player's view information.
		Player->PlayerController->GetPlayerViewPoint( out_Location, out_Rotation );		
	}
}

#endif


namespace EngineDefs
{
	// Time between successive runs of the hardware survey
	static const FTimespan HardwareSurveyInterval(30, 0, 0, 0);	// 30 days
}

/*-----------------------------------------------------------------------------
	Engine init and exit.
-----------------------------------------------------------------------------*/

/** Callback from OS when we get a low memory warning.
  * Note: might not be called from the game thread
  */
void EngineMemoryWarningHandler(const FGenericMemoryWarningContext& GenericContext)
{
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("EngineMemoryWarningHandler: Mem Used %.2f MB, Texture Memory %.2f MB, Render Target memory %.2f MB, OS Free %.2f MB\n"), 
		Stats.UsedPhysical / 1048576.0f, 
		GCurrentTextureMemorySize / 1024.f,
		GCurrentRendertargetMemorySize / 1024.f,
		Stats.AvailablePhysical / 1048576.0f);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	static const auto OOMMemReportVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Debug.OOMMemReport")); 
	const int32 OOMMemReport = OOMMemReportVar ? OOMMemReportVar->GetValueOnAnyThread() : false;
	if( OOMMemReport )
	{
		GEngine->Exec(NULL, TEXT("OBJ LIST"));
		GEngine->Exec(NULL, TEXT("MEM FROMREPORT"));
	}
#endif

	GLastMemoryWarningTime = FPlatformTime::Seconds();
}

UEngine::FOnNewStatRegistered UEngine::NewStatDelegate;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int32> CVarStressTestGCWhileStreaming(
	TEXT("gc.StressTestGC"),
	0,
	TEXT("If set to 1, the engine will attempt to trigger GC each frame while async loading."));
#endif

static TAutoConsoleVariable<int32> CVarCollectGarbageEveryFrame(
	TEXT("gc.CollectGarbageEveryFrame"),
	0,
	TEXT("Used to debug garbage collection...Collects garbage every frame if the value is > 0."));

static float GTimeBetweenPurgingPendingKillObjects = 60.0f;
static FAutoConsoleVariableRef CVarTimeBetweenPurgingPendingKillObjects(
	TEXT("gc.TimeBetweenPurgingPendingKillObjects"),
	GTimeBetweenPurgingPendingKillObjects,
	TEXT("Time in seconds (game time) we should wait between purging object references to objects that are pending kill."),
	ECVF_Default
);

static float GTimeBetweenPurgingPendingKillObjectsOnIdleServerMultiplier = 10.0f;
static FAutoConsoleVariableRef CVarTimeBetweenPurgingPendingKillObjectsOnIdleServerMultiplier(
	TEXT("gc.TimeBetweenPurgingPendingKillObjectsOnIdleServerMultiplier"),
	GTimeBetweenPurgingPendingKillObjectsOnIdleServerMultiplier,
	TEXT("Multiplier to apply to time between purging pending kill objects when on an idle server."),
	ECVF_Default
);

void UEngine::PreGarbageCollect()
{
	ForEachObjectOfClass(UWorld::StaticClass(), [](UObject* WorldObj)
	{
		UWorld* World = CastChecked<UWorld>(WorldObj);

		if (World->HasEndOfFrameUpdates())
		{
			// Make sure deferred component updates have been sent to the rendering thread before deleting any UObjects which the rendering thread may be referencing
			// This fixes rendering thread crashes in the following order of operations 1) UMeshComponent::SetMaterial 2) GC 3) Rendering command that dereferences the UMaterial
			World->SendAllEndOfFrameUpdates();
		}
	});
}

float UEngine::GetTimeBetweenGarbageCollectionPasses() const
{
	float TimeBetweenGC = GTimeBetweenPurgingPendingKillObjects;

	if (IsRunningDedicatedServer())
	{
		bool bAtLeastOnePlayerConnected = false;

		ForEachObjectOfClass(UWorld::StaticClass(),[&bAtLeastOnePlayerConnected](UObject* WorldObj)
		{
			UWorld* World = CastChecked<UWorld>(WorldObj);
			bAtLeastOnePlayerConnected = bAtLeastOnePlayerConnected || ((World->NetDriver != nullptr) && World->NetDriver->ClientConnections.Num() > 0);
		});

		if (!bAtLeastOnePlayerConnected)
		{
			TimeBetweenGC *= GTimeBetweenPurgingPendingKillObjectsOnIdleServerMultiplier;
		}
	}

	return TimeBetweenGC;
}

void UEngine::ForceGarbageCollection(bool bForcePurge/*=false*/)
{
	TimeSinceLastPendingKillPurge = 1.0f + GetTimeBetweenGarbageCollectionPasses();
	bFullPurgeTriggered = bFullPurgeTriggered || bForcePurge;
}

void UEngine::DelayGarbageCollection()
{
	bShouldDelayGarbageCollect = true;
}

void UEngine::SetTimeUntilNextGarbageCollection(const float MinTimeUntilNextPass)
{
	const float TimeBetweenPurgingPendingKillObjects = GetTimeBetweenGarbageCollectionPasses();

	// This can make it go negative if the desired interval is longer than the typical interval, but it's only ever compared against TimeBetweenPurgingPendingKillObjects
	TimeSinceLastPendingKillPurge = TimeBetweenPurgingPendingKillObjects - MinTimeUntilNextPass;
}

void UEngine::ConditionalCollectGarbage()
{
	if (GFrameCounter != LastGCFrame)
	{
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CVarStressTestGCWhileStreaming.GetValueOnGameThread() && IsAsyncLoading())
		{
			TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
		}
		else
	#endif
		if (bFullPurgeTriggered)
		{
			if (TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true))
			{
				ForEachObjectOfClass(UWorld::StaticClass(),[](UObject* World)
				{
					CastChecked<UWorld>(World)->CleanupActors();
				});
				bFullPurgeTriggered = false;
				bShouldDelayGarbageCollect = false;
				TimeSinceLastPendingKillPurge = 0.0f;
			}
		}
		else
		{
			bool bHasAWorldBegunPlay = false;
			ForEachObjectOfClass(UWorld::StaticClass(), [&bHasAWorldBegunPlay](UObject* World)
			{
				bHasAWorldBegunPlay = bHasAWorldBegunPlay || CastChecked<UWorld>(World)->HasBegunPlay();
			});

			if (bHasAWorldBegunPlay)
			{
				TimeSinceLastPendingKillPurge += FApp::GetDeltaTime();

				const float TimeBetweenPurgingPendingKillObjects = GetTimeBetweenGarbageCollectionPasses();

				// See if we should delay garbage collect for this frame
				if (bShouldDelayGarbageCollect)
				{
					bShouldDelayGarbageCollect = false;
				}
				// Perform incremental purge update if it's pending or in progress.
				else if (!IsIncrementalPurgePending()
					// Purge reference to pending kill objects every now and so often.
					&& (TimeSinceLastPendingKillPurge > TimeBetweenPurgingPendingKillObjects) && TimeBetweenPurgingPendingKillObjects > 0.f)
				{
					SCOPE_CYCLE_COUNTER(STAT_GCMarkTime);
					PerformGarbageCollectionAndCleanupActors();
				}
				else
				{
					SCOPE_CYCLE_COUNTER(STAT_GCSweepTime);
					IncrementalPurgeGarbage(true);
				}
			}
		}

		if (CVarCollectGarbageEveryFrame.GetValueOnGameThread() > 0)
		{
			ForceGarbageCollection(true);
		}

		LastGCFrame = GFrameCounter;
	}
}

void UEngine::PerformGarbageCollectionAndCleanupActors()
{
	// We don't collect garbage while there are outstanding async load requests as we would need
	// to block on loading the remaining data.
	if (!IsAsyncLoading())
	{
		// Perform housekeeping.
		if (TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, false))
		{
			ForEachObjectOfClass(UWorld::StaticClass(), [](UObject* World)
			{
				CastChecked<UWorld>(World)->CleanupActors();
			});

			// Reset counter.
			TimeSinceLastPendingKillPurge = 0.0f;
			bFullPurgeTriggered = false;
			LastGCFrame = GFrameCounter;
		}
	}
}

//
// Initialize the engine.
//
void UEngine::Init(IEngineLoop* InEngineLoop)
{
	UE_LOG(LogEngine, Log, TEXT("Initializing Engine..."));
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Engine Initialized"), STAT_EngineStartup, STATGROUP_LoadTime);

	// Start capturing errors and warnings
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ErrorsAndWarningsCollector.Initialize();
#endif

#if !UE_BUILD_SHIPPING
	if(!FEngineBuildSettings::IsInternalBuild())
	{
		TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();

		for (auto Plugin : EnabledPlugins)
		{
			const FPluginDescriptor& Desc = Plugin->GetDescriptor();

			FString DescStr;
			Desc.Write(DescStr);
			FGenericCrashContext::AddPlugin(DescStr);
		}
	}
#endif

	// Set the memory warning handler
	FPlatformMisc::SetMemoryWarningHandler(EngineMemoryWarningHandler);

	EngineLoop = InEngineLoop;

	// Subsystems.
	FURL::StaticInit();
	FLinkerLoad::StaticInit(UTexture2D::StaticClass());

#if !UE_BUILD_SHIPPING
	// Check for overrides to the default map on the command line
	TCHAR MapName[512];
	if ( FParse::Value(FCommandLine::Get(), TEXT("DEFAULTMAP="), MapName, ARRAY_COUNT(MapName)) )
	{
		UE_LOG(LogEngine, Log, TEXT("Overriding default map to %s"), MapName);

		FString MapString = FString(MapName);
		UGameMapsSettings::SetGameDefaultMap(MapString);
	}
#endif // !UE_BUILD_SHIPPING
	
	InitializeRunningAverageDeltaTime();

	// Add to root.
	AddToRoot();

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddStatic(UEngine::PreGarbageCollect);

	// Initialize the HMDs and motion controllers, if any
	InitializeHMDDevice();

	// Disable the screensaver when running the game.
	if( GIsClient && !GIsEditor )
	{
		EnableScreenSaver( false );
	}

	if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
	{
		// If Slate is being used, initialize the renderer after RHIInit 
		FSlateApplication& CurrentSlateApp = FSlateApplication::Get();
		CurrentSlateApp.InitializeSound( TSharedRef<FSlateSoundDevice>( new FSlateSoundDevice() ) );

#if !UE_BUILD_SHIPPING
		// Create test windows (if we were asked to do that)
		if( FParse::Param( FCommandLine::Get(), TEXT("SlateDebug") ) )
		{
			RestoreSlateTestSuite();
		}
#endif // #if !UE_BUILD_SHIPPING
	}

	// Assign thumbnail compressor/decompressor
	FObjectThumbnail::SetThumbnailCompressor( new FPNGThumbnailCompressor() );

	//UEngine::StaticClass()->GetDefaultObject(true);
	LoadObject<UClass>(UEngine::StaticClass()->GetOuter(), *UEngine::StaticClass()->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );
	// This reads the Engine.ini file to get the proper DefaultMaterial, etc.
	LoadConfig();

	SetConfiguredProcessLimits();

	bIsOverridingSelectedColor = false;

	// Set colors for selection materials
	SelectedMaterialColor = DefaultSelectedMaterialColor;
	SelectionOutlineColor = DefaultSelectedMaterialColor;

	InitializeObjectReferences();

	if (GConfig)
	{
		bool bTemp = true;
		GConfig->GetBool(TEXT("/Script/Engine.Engine"), TEXT("bEnableOnScreenDebugMessages"), bTemp, GEngineIni);
		bEnableOnScreenDebugMessages = bTemp ? true : false;
		bEnableOnScreenDebugMessagesDisplay = bEnableOnScreenDebugMessages;

		GConfig->GetBool(TEXT("DevOptions.Debug"), TEXT("ShowSelectedLightmap"), GShowDebugSelectedLightmap, GEngineIni);
	}

	// Update Script Maximum loop iteration count
	FBlueprintCoreDelegates::SetScriptMaximumLoopIterations( GEngine->MaximumLoopIterationCount );

	GNearClippingPlane = NearClipPlane;

	UTextRenderComponent::InitializeMIDCache();

	if (GIsEditor)
	{
		// Create a WorldContext for the editor to use and create an initially empty world.
		FWorldContext &InitialWorldContext = CreateNewWorldContext(EWorldType::Editor);
		InitialWorldContext.SetCurrentWorld( UWorld::CreateWorld( EWorldType::Editor, true ) );
		GWorld = InitialWorldContext.World();
	}

	// Initialize the audio device after a world context is setup
	InitializeAudioDeviceManager();

	if ( IsConsoleBuild() )
	{
		bUseConsoleInput = true;
	}

	// Make sure networking checksum has access to project version
	const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
	FNetworkVersion::bHasCachedNetworkChecksum = false;
	FNetworkVersion::ProjectVersion = ProjectSettings.ProjectVersion;

#if !(UE_BUILD_SHIPPING)
	// Optionally Exec an exec file
	FString Temp;
	if( FParse::Value(FCommandLine::Get(), TEXT("EXEC="), Temp) )
	{
		new(GEngine->DeferredCommands) FString(FString(TEXT("exec ")) + Temp);
	}

	// Optionally exec commands passed in the command line.
	FString ExecCmds;
	if( FParse::Value(FCommandLine::Get(), TEXT("ExecCmds="), ExecCmds, false) )
	{
		TArray<FString> CommandArray;
		ExecCmds.ParseIntoArray( CommandArray, TEXT(","), true );

		for( int32 Cx = 0; Cx < CommandArray.Num(); ++Cx )
		{
			const FString& Command = CommandArray[Cx];
			// Skip leading whitespaces in the command.
			int32 Index = 0;
			while( FChar::IsWhitespace( Command[Index] ) )
			{
				Index++;
			}

			if( Index < Command.Len()-1 )
			{
				new(GEngine->DeferredCommands) FString(*Command+Index);
			}
		}
	}

	// optionally set the vsync console variable
	if( FParse::Param(FCommandLine::Get(), TEXT("vsync")) )
	{
		new(GEngine->DeferredCommands) FString(TEXT("r.vsync 1"));
	}

	// optionally set the vsync console variable
	if( FParse::Param(FCommandLine::Get(), TEXT("novsync")) )
	{
		new(GEngine->DeferredCommands) FString(TEXT("r.vsync 0"));
	}
#endif // !(UE_BUILD_SHIPPING)

	if (GetDerivedDataCache())
	{
		GetDerivedDataCacheRef().NotifyBootComplete();
	}

	// Manually delete any potential leftover crash videos in case we can't access the module
	// because the crash reporter will upload any leftover crash video from last session
	FString CrashVideoPath = FPaths::ProjectLogDir() + TEXT("CrashVideo.avi");
	IFileManager::Get().Delete(*CrashVideoPath);
	
	// register the engine with the travel and network failure broadcasts
	// games can override these to provide proper behavior in each error case
	OnTravelFailure().AddUObject(this, &UEngine::HandleTravelFailure);
	OnNetworkFailure().AddUObject(this, &UEngine::HandleNetworkFailure);
	OnNetworkLagStateChanged().AddUObject(this, &UEngine::HandleNetworkLagStateChanged);

	UE_LOG(LogInit, Log, TEXT("Texture streaming: %s"), IStreamingManager::Get().IsTextureStreamingEnabled() ? TEXT("Enabled") : TEXT("Disabled") );

	// Initialize the online subsystem as early as possible
	FOnlineExternalUIChanged OnExternalUIChangeDelegate;
	OnExternalUIChangeDelegate.BindUObject(this, &UEngine::OnExternalUIChange);
	UOnlineEngineInterface::Get()->BindToExternalUIOpening(OnExternalUIChangeDelegate);

	// Initialise buffer visualization system data
	GetBufferVisualizationData().Initialize();

	// Initialize Portal services
	if (!IsRunningCommandlet() && !IsRunningDedicatedServer())
	{
		InitializePortalServices();
	}

	// Connect the engine analytics provider
	FEngineAnalytics::Initialize();

	// Dynamically load engine runtime modules
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("StreamingPauseRendering"));
		FModuleManager::Get().LoadModuleChecked(TEXT("GeometryCache"));
		FModuleManager::Get().LoadModuleChecked(TEXT("MovieScene"));
		FModuleManager::Get().LoadModuleChecked(TEXT("MovieSceneTracks"));
	}

	// Finish asset manager loading
	if (AssetManager)
	{
		AssetManager->FinishInitialLoading();
	}

	bool bIsRHS = true;
	if (GConfig)
	{
		GConfig->GetBool(TEXT("DevOptions.Debug"), TEXT("bEngineStatsOnRHS"), bIsRHS, GEngineIni);
	}

	// Add the stats to the list, note this is also the order that they get rendered in if active.
#if !UE_BUILD_SHIPPING
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Version"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatVersion, NULL, bIsRHS));
#endif // !UE_BUILD_SHIPPING
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_NamedEvents"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatNamedEvents, &UEngine::ToggleStatNamedEvents, bIsRHS));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_FPS"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatFPS, &UEngine::ToggleStatFPS, bIsRHS));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Summary"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSummary, NULL, bIsRHS));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Unit"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatUnit, &UEngine::ToggleStatUnit, bIsRHS));
	/* @todo Slate Rendering
#if STATS
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SlateBatches"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSlateBatches, NULL, true));
#endif
	*/
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Hitches"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatHitches, &UEngine::ToggleStatHitches, bIsRHS));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_AI"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatAI, NULL, bIsRHS));

	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_ColorList"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatColorList, NULL));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Levels"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatLevels, NULL));
#if !UE_BUILD_SHIPPING
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SoundMixes"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSoundMixes, &UEngine::ToggleStatSoundMixes));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Reverb"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatReverb, NULL));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SoundWaves"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSoundWaves, &UEngine::ToggleStatSoundWaves));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_SoundCues"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSoundCues, &UEngine::ToggleStatSoundCues));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Sounds"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatSounds, &UEngine::ToggleStatSounds));
#endif // !UE_BUILD_SHIPPING
	/* @todo UE4 physx fix this once we have convexelem drawing again
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_LevelMap"), TEXT("STATCAT_Engine"), FText::GetEmpty(), &UEngine::RenderStatLevelMap, NULL));
*/
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Detailed"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatDetailed));
#if !UE_BUILD_SHIPPING
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_UnitMax"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatUnitMax));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_UnitGraph"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatUnitGraph));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_UnitTime"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatUnitTime));
	EngineStats.Add(FEngineStatFuncs(TEXT("STAT_Raw"), TEXT("STATCAT_Engine"), FText::GetEmpty(), NULL, &UEngine::ToggleStatRaw));
#endif

	// Let any listeners know about the new stats
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		NewStatDelegate.Broadcast(EngineStat.CommandName, EngineStat.CategoryName, EngineStat.DescriptionString);
	}

	// Record the analytics for any attached HMD devices
	RecordHMDAnalytics();
}

void UEngine::Start()
{
	// Start the game!
}

void UEngine::RegisterBeginStreamingPauseRenderingDelegate( FBeginStreamingPauseDelegate* InDelegate )
{
	BeginStreamingPauseDelegate = InDelegate;
}

void UEngine::RegisterEndStreamingPauseRenderingDelegate( FEndStreamingPauseDelegate* InDelegate )
{
	EndStreamingPauseDelegate = InDelegate;
}

void UEngine::OnExternalUIChange(bool bInIsOpening)
{
	FSlateApplication::Get().ExternalUIChange(bInIsOpening);
}

void UEngine::ShutdownAudioDeviceManager()
{
	// Shutdown the main audio device in the UEEngine
	if (AudioDeviceManager)
	{
		FAudioCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();

		FAudioThread::StopAudioThread();

		AudioDeviceManager->ShutdownAllAudioDevices();

		delete AudioDeviceManager;
		AudioDeviceManager = NULL;
	}
}

void UEngine::PreExit()
{
	if (IMovieSceneCaptureModule* Module = FModuleManager::GetModulePtr<IMovieSceneCaptureModule>( "MovieSceneCapture" ))
	{
		Module->DestroyAllActiveCaptures();
	}

	UTextRenderComponent::ShutdownMIDCache();

	ShutdownRenderingCVarsCaching();
	const bool bIsEngineShutdown = true;
	FEngineAnalytics::Shutdown(bIsEngineShutdown);
	if (ScreenSaverInhibitor)
	{
		// Resume the thread to avoid a deadlock while waiting for finish.
		ScreenSaverInhibitor->Suspend( false );
		delete ScreenSaverInhibitor;
		ScreenSaverInhibitor = nullptr;
	}

	delete ScreenSaverInhibitorRunnable;

	ShutdownHMD();
}

void UEngine::ShutdownHMD()
{
	// we can't just nulify these pointers here since RenderThread still might use them.
	auto SavedStereo = StereoRenderingDevice;
	auto SavedHMD = XRSystem;
	auto SavedViewExtentions = ViewExtensions;
	{
		FSuspendRenderingThread Suspend(false);
		StereoRenderingDevice.Reset();
		XRSystem.Reset();
	}
	// shutdown will occur here.
}

void UEngine::TickDeferredCommands()
{
	SCOPE_TIME_GUARD(TEXT("UEngine::TickDeferredCommands"));

	const double StartTime = FPlatformTime::Seconds();

	QUICK_SCOPE_CYCLE_COUNTER(STAT_UEngine_TickDeferredCommands);
	// Execute all currently queued deferred commands (allows commands to be queued up for next frame).
	const int32 DeferredCommandsCount = DeferredCommands.Num();
	for( int32 DeferredCommandsIndex=0; DeferredCommandsIndex<DeferredCommandsCount; DeferredCommandsIndex++ )
	{
		// Use LocalPlayer if available...
		ULocalPlayer* LocalPlayer = GetDebugLocalPlayer();
		if( LocalPlayer )
		{
			LocalPlayer->Exec( LocalPlayer->GetWorld(), *DeferredCommands[DeferredCommandsIndex], *GLog );
		}
		// and fall back to UEngine otherwise.
		else
		{
			Exec( GWorld, *DeferredCommands[DeferredCommandsIndex], *GLog );
		}
	}

	const double ElapsedTimeMS = (FPlatformTime::Seconds() - StartTime) / 1000.0;

	// If we're not in the editor, and commands took more than our target frame time to execute, print them out so there's a paper trail
	if (GIsEditor == false && ElapsedTimeMS >= FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS())
	{
		UE_LOG(LogEngine, Warning, TEXT("UEngine::TickDeferredCommands took %.02fms to execute %d commands!"), ElapsedTimeMS, DeferredCommandsCount);
		
		for (int32 i = 0; i < DeferredCommandsCount; i++)
		{
			UE_LOG(LogEngine, Warning, TEXT("\t%s"), *DeferredCommands[i]);
		}	
	}

	DeferredCommands.RemoveAt(0, DeferredCommandsCount);
}

void PumpABTest()
{
#if ENABLE_ABTEST
	const TCHAR* Command = FABTest::Get().TickAndGetCommand();
	if (Command)
	{
		GEngine->Exec(nullptr, Command, *GLog);
	}
#endif
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

struct FTimedMemReport
{
	FTimedMemReport()
	: TotalTime(0.f)
	, DumpDelayTime(-1.f)
	{
		if(const TCHAR* CommandLine = FCommandLine::Get())
		{
			FParse::Value(CommandLine, TEXT("TimedMemoryReport="), DumpDelayTime);
		}
	}

	static FTimedMemReport& Get()
	{
		static FTimedMemReport Singleton;
		return Singleton;
	}

	static void SetDumpDelayParse(const TArray<FString>& Args)
	{
		if(Args.Num())
		{
			float DumpDelay = FCString::Atof(*Args[0]);
			Get().SetDumpDelay(DumpDelay);
		}
	}

	void SetDumpDelay(float InDumpDelay)
	{
		DumpDelayTime = InDumpDelay;
		TotalTime = 0.f;	//reset time
	}

	void PumpTimedMemoryReports()
	{
		if (DumpDelayTime > 0)
		{
			TotalTime += FApp::GetDeltaTime();
			if (TotalTime > DumpDelayTime)
			{
				GEngine->Exec(nullptr, TEXT("memreport"), *GLog);
				TotalTime = 0.f;
			}
		}
	}

private:
	float TotalTime;
	float DumpDelayTime;
};

static FAutoConsoleCommand SetTimedMemReport(TEXT("TimedMemReport.Delay"), TEXT("Determines how long to wait before getting a memreport. < 0 is off"), FConsoleCommandWithArgsDelegate::CreateStatic(&FTimedMemReport::SetDumpDelayParse), ECVF_Cheat);



#endif

void UEngine::UpdateTimeAndHandleMaxTickRate()
{
	PumpABTest();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FTimedMemReport::Get().PumpTimedMemoryReports();
#endif

	// This is always in realtime and is not adjusted by fixed framerate. Start slightly below current real time
	static double LastRealTime = FPlatformTime::Seconds() - 0.0001;
	static bool bTimeWasManipulated = false;
	bool bTimeWasManipulatedDebug = bTimeWasManipulated;	//Just used for logging of previous frame

	// Figure out whether we want to use real or fixed time step.
	const bool bUseFixedTimeStep = FApp::IsBenchmarking() || FApp::UseFixedTimeStep();

	// Updates logical last time to match logical current time from last tick
	FApp::UpdateLastTime();

	// Calculate delta time and update time.
	if( bUseFixedTimeStep )
	{
		// In fixed time step we set the real times to the logical time, this causes it to lie about how fast it is going
		bTimeWasManipulated = true;
		const float FrameRate = FApp::GetFixedDeltaTime();
		FApp::SetDeltaTime(FrameRate);
		LastRealTime = FApp::GetCurrentTime();
		FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());
	}
	else
	{
		// Updates logical time to real time, this may be changed by fixed frame rate below
		double CurrentRealTime = FPlatformTime::Seconds();
		FApp::SetCurrentTime(CurrentRealTime);

		// Did we just switch from a fixed time step to real-time?  If so, then we'll update our
		// cached 'last time' so our current interval isn't huge (or negative!)
		if( bTimeWasManipulated && !bUseFixedFrameRate )
		{
			LastRealTime = CurrentRealTime - FApp::GetDeltaTime();
			bTimeWasManipulated = false;
		}

		// Calculate delta time, this is in real time seconds
		float DeltaRealTime = CurrentRealTime - LastRealTime;

		// Negative delta time means something is wrong with the system. Error out so user can address issue.
		if(DeltaRealTime < 0 )
		{
#if PLATFORM_ANDROID
			UE_LOG(LogEngine, Warning, TEXT("Detected negative delta time - ignoring"));
#else
			// AMD dual-core systems are a known issue that require AMD CPU drivers to be installed. Installer will take care of this for shipping.
			UE_LOG(LogEngine, Fatal,TEXT("Detected negative delta time - on AMD systems please install http://files.aoaforums.com/I3199-setup.zip.html - DeltaTime:%f, bUseFixedFrameRate:%d, bTimeWasManipulatedDebug:%d, FixedFrameRate:%f"), 
				DeltaRealTime, bUseFixedFrameRate, bTimeWasManipulatedDebug, FixedFrameRate);
#endif
			DeltaRealTime = 0.01;
		}

		// Give engine chance to update frame rate smoothing
		UpdateRunningAverageDeltaTime(DeltaRealTime);

		// Get max tick rate based on network settings and current delta time.
		const float GivenMaxTickRate = GetMaxTickRate(DeltaRealTime);
		const float MaxTickRate = FABTest::StaticIsActive() ? 0.0f : (bUseFixedFrameRate ? FixedFrameRate : GivenMaxTickRate);
		float WaitTime		= 0;
		// Convert from max FPS to wait time.
		if( MaxTickRate > 0 )
		{
			WaitTime = FMath::Max( 1.f / MaxTickRate - DeltaRealTime, 0.f );
		}

		// Enforce maximum framerate and smooth framerate by waiting.
		double ActualWaitTime = 0.f;
		if( WaitTime > 0 )
		{
			// track all this waiting so that Game Thread is correct
			FThreadIdleStats::FScopeIdle Scope;
			
			FSimpleScopeSecondsCounter ActualWaitTimeCounter(ActualWaitTime);
			double WaitEndTime = CurrentRealTime + WaitTime;

			SCOPE_CYCLE_COUNTER(STAT_GameTickWaitTime);
			SCOPE_CYCLE_COUNTER(STAT_GameIdleTime);

			if (IsRunningDedicatedServer()) // We aren't so concerned about wall time with a server, lots of CPU is wasted spinning. I suspect there is more to do with sleeping and time on dedicated servers.
			{
				FPlatformProcess::SleepNoStats(WaitTime);
			}
			else
			{
				// Sleep if we're waiting more than 5 ms. We set the scheduler granularity to 1 ms
				// at startup on PC. We reserve 2 ms of slack time which we will wait for by giving
				// up our timeslice.
				if( WaitTime > 5 / 1000.f )
				{
					FPlatformProcess::SleepNoStats( WaitTime - 0.002f );
				}

				// Give up timeslice for remainder of wait time.
				while( FPlatformTime::Seconds() < WaitEndTime )
				{
					FPlatformProcess::SleepNoStats( 0 );
				}
			}
			CurrentRealTime = FPlatformTime::Seconds();

			if(bUseFixedFrameRate)
			{
				// We are on fixed framerate but had to delay, we set the current time with a fixed time step, which will set Delta below
				const double FrameTime = 1.0 / FixedFrameRate;
				FApp::SetCurrentTime(LastRealTime + FrameTime);
				bTimeWasManipulated = true;
			}
			else
			{
				FApp::SetCurrentTime(CurrentRealTime);
			}
		}
		else if(bUseFixedFrameRate && MaxTickRate == FixedFrameRate)
		{
			// We are doing fixed framerate and the real delta time is bigger than our desired delta time. In this case we start falling behind real time (and that's ok)
			const double FrameTime = 1.0 / FixedFrameRate;
			FApp::SetCurrentTime(LastRealTime + FrameTime);
			bTimeWasManipulated = true;
		}

		SET_FLOAT_STAT(STAT_GameTickWantedWaitTime,WaitTime * 1000.f);
		double AdditionalWaitTimeInMs = (ActualWaitTime - static_cast<double>(WaitTime)) * 1000.0;
		SET_FLOAT_STAT(STAT_GameTickAdditionalWaitTime,FMath::Max<float>(static_cast<float>(AdditionalWaitTimeInMs),0.f));

		// Update logical delta time based on logical current time
		FApp::SetDeltaTime(FApp::GetCurrentTime() - LastRealTime);
		FApp::SetIdleTime(ActualWaitTime);

		// Negative delta time means something is wrong with the system. Error out so user can address issue.
		if( FApp::GetDeltaTime() < 0 )
		{
#if PLATFORM_ANDROID
			UE_LOG(LogEngine, Warning, TEXT("Detected negative delta time - ignoring"));
#else
			// AMD dual-core systems are a known issue that require AMD CPU drivers to be installed. Installer will take care of this for shipping.
			UE_LOG(LogEngine, Fatal, TEXT("Detected negative delta time - on AMD systems please install http://files.aoaforums.com/I3199-setup.zip.html"));
#endif
			FApp::SetDeltaTime(0.01);
		}

		LastRealTime = CurrentRealTime;

		// Enforce a maximum delta time if wanted.
		UGameEngine* GameEngine = Cast<UGameEngine>(this);
		const float MaxDeltaTime = GameEngine ? GameEngine->MaxDeltaTime : 0.f;
		if( MaxDeltaTime > 0.f )
		{
			UWorld* World = NULL;

			int32 NumGamePlayers = 0;
			for (int32 WorldIndex = 0; WorldIndex < WorldList.Num(); ++WorldIndex)
			{
				if (WorldList[WorldIndex].WorldType == EWorldType::Game && WorldList[WorldIndex].OwningGameInstance != NULL)
				{
					World = WorldList[WorldIndex].World();
					NumGamePlayers = WorldList[WorldIndex].OwningGameInstance->GetNumLocalPlayers();
					break;
				}
			}

			// We don't want to modify delta time if we are dealing with network clients as either host or client.
			if( World != NULL
				// Not having a game info implies being a client.
				&& ( ( World->GetAuthGameMode() != NULL
				// NumPlayers and GamePlayer only match in standalone game types and handles the case of splitscreen.
				&&	World->GetAuthGameMode()->GetNumPlayers() == NumGamePlayers ) ) )
			{
				// Happy clamping!
				FApp::SetDeltaTime(FMath::Min<double>(FApp::GetDeltaTime(), MaxDeltaTime));
			}
		}
	}

#if !UE_BUILD_SHIPPING
	{
		float OverrideFPS = CVarSetOverrideFPS.GetValueOnGameThread();
		if(OverrideFPS >= 0.001f)
		{
			// in seconds
			FApp::SetDeltaTime(1.0f / OverrideFPS);
			LastRealTime = FApp::GetCurrentTime();
			FApp::SetCurrentTime(FApp::GetCurrentTime() + FApp::GetDeltaTime());
			bTimeWasManipulated = true;
		}
	}
#endif // !UE_BUILD_SHIPPING
}


void UEngine::ParseCommandline()
{
	// If dedicated server, the -nosound, or -benchmark parameters are used, disable sound.
	if(FParse::Param(FCommandLine::Get(),TEXT("nosound")) || FApp::IsBenchmarking() || IsRunningDedicatedServer() || (IsRunningCommandlet() && !IsAllowCommandletAudio()))
	{
		bUseSound = false;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("enablesound")))
	{
		bUseSound = true;
	}

	if( FParse::Param( FCommandLine::Get(), TEXT("noailogging")) )
	{
		bDisableAILogging = true;
	}

	if( FParse::Param( FCommandLine::Get(), TEXT("enableailogging")) )
	{
		bDisableAILogging = false;
	}
}


/**
 * Loads a special material and verifies that it is marked as a special material (some shaders
 * will only be compiled for materials marked as "special engine material")
 *
 * @param MaterialName Fully qualified name of a material to load/find
 * @param Material Reference to a material object pointer that will be filled out
 * @param bCheckUsage Check if the material has been marked to be used as a special engine material
 */
void LoadSpecialMaterial(const FString& MaterialName, UMaterial*& Material, bool bCheckUsage)
{
	// only bother with materials that aren't already loaded
	if (Material == NULL)
	{
		// find or load the object
		Material = LoadObject<UMaterial>(NULL, *MaterialName, NULL, LOAD_None, NULL);	

		if (!Material)
		{
#if !WITH_EDITORONLY_DATA
			UE_LOG(LogEngine, Log, TEXT("ERROR: Failed to load special material '%s'. This will probably have bad consequences (depending on its use)"), *MaterialName);
#else
			UE_LOG(LogEngine, Fatal,TEXT("Failed to load special material '%s'"), *MaterialName);
#endif
		}
		// if the material wasn't marked as being a special engine material, then not all of the shaders 
		// will have been compiled on it by this point, so we need to compile them and alert the use
		// to set the bit
		else if (!Material->bUsedAsSpecialEngineMaterial && bCheckUsage) 
		{
#if !WITH_EDITOR
			// consoles must have the flag set properly in the editor
			UE_LOG(LogEngine, Fatal,TEXT("The special material (%s) was not marked with bUsedAsSpecialEngineMaterial. Make sure this flag is set in the editor, save the package, and compile shaders for this platform"), *MaterialName);
#else
			Material->bUsedAsSpecialEngineMaterial = true;
			Material->MarkPackageDirty();

			// make sure all necessary shaders for the default are compiled, now that the flag is set
			Material->PostEditChange();

			FMessageDialog::Open( EAppMsgType::Ok, FText::Format( NSLOCTEXT("Engine", "SpecialMaterialConfiguredIncorrectly", "The special material ({0}) has not been marked with bUsedAsSpecialEngineMaterial.\nThis will prevent shader precompiling properly, so the flag has been set automatically.\nMake sure to save the package and distribute to everyone using this material."), FText::FromString( MaterialName ) ) );
#endif
		}
	}
}


template<typename ClassType>
void LoadEngineClass(const FSoftClassPath& ClassName, TSubclassOf<ClassType>& EngineClassRef)
{
	if ( EngineClassRef == nullptr )
	{
		EngineClassRef = LoadClass<ClassType>(nullptr, *ClassName.ToString());
		if (EngineClassRef == nullptr)
		{
			EngineClassRef = ClassType::StaticClass();
			UE_LOG(LogEngine, Error, TEXT("Failed to load '%s', falling back to '%s'"), *ClassName.ToString(), *EngineClassRef->GetName());
		}
	}
}

/**
 * Loads all Engine object references from their corresponding config entries.
 */
void UEngine::InitializeObjectReferences()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UEngine::InitializeObjectReferences"), STAT_InitializeObjectReferences, STATGROUP_LoadTime);

	// initialize the special engine/editor materials
	if (AllowDebugViewmodes())
	{
		// Materials that are needed in-game if debug viewmodes are allowed
		LoadSpecialMaterial(WireframeMaterialName, WireframeMaterial, true);
		LoadSpecialMaterial(LevelColorationLitMaterialName, LevelColorationLitMaterial, true);
		LoadSpecialMaterial(LevelColorationUnlitMaterialName, LevelColorationUnlitMaterial, true);
		LoadSpecialMaterial(LightingTexelDensityName, LightingTexelDensityMaterial, false);
		LoadSpecialMaterial(ShadedLevelColorationLitMaterialName, ShadedLevelColorationLitMaterial, true);
		LoadSpecialMaterial(ShadedLevelColorationUnlitMaterialName, ShadedLevelColorationUnlitMaterial, true);
		LoadSpecialMaterial(VertexColorMaterialName, VertexColorMaterial, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_ColorOnly, VertexColorViewModeMaterial_ColorOnly, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_AlphaAsColor, VertexColorViewModeMaterial_AlphaAsColor, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_RedOnly, VertexColorViewModeMaterial_RedOnly, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_GreenOnly, VertexColorViewModeMaterial_GreenOnly, false);
		LoadSpecialMaterial(VertexColorViewModeMaterialName_BlueOnly, VertexColorViewModeMaterial_BlueOnly, false);
	}

	// Materials that may or may not be needed when debug viewmodes are disabled but haven't been fixed up yet
	LoadSpecialMaterial(RemoveSurfaceMaterialName.ToString(), RemoveSurfaceMaterial, false);

	// these one's are needed both editor and standalone 
	LoadSpecialMaterial(DebugMeshMaterialName.ToString(), DebugMeshMaterial, false);
	LoadSpecialMaterial(InvalidLightmapSettingsMaterialName.ToString(), InvalidLightmapSettingsMaterial, false);
	LoadSpecialMaterial(ArrowMaterialName.ToString(), ArrowMaterial, false);

#if !UE_BUILD_SHIPPING
	LoadSpecialMaterial(TEXT("/Engine/EngineMaterials/PhAT_JointLimitMaterial.PhAT_JointLimitMaterial"), ConstraintLimitMaterial, false);

	ConstraintLimitMaterialX = UMaterialInstanceDynamic::Create(ConstraintLimitMaterial, NULL);
	ConstraintLimitMaterialX->SetVectorParameterValue(FName("Color"), FLinearColor::Red);
	ConstraintLimitMaterialX->SetScalarParameterValue(FName("Desaturation"), 0.6f);
	ConstraintLimitMaterialXAxis = UMaterialInstanceDynamic::Create(ConstraintLimitMaterial, NULL);
	ConstraintLimitMaterialXAxis->SetVectorParameterValue(FName("Color"), FLinearColor::Red);

	ConstraintLimitMaterialY = UMaterialInstanceDynamic::Create(ConstraintLimitMaterial, NULL);
	ConstraintLimitMaterialY->SetVectorParameterValue(FName("Color"), FLinearColor::Green);
	ConstraintLimitMaterialY->SetScalarParameterValue(FName("Desaturation"), 0.6f);
	ConstraintLimitMaterialYAxis = UMaterialInstanceDynamic::Create(ConstraintLimitMaterial, NULL);
	ConstraintLimitMaterialYAxis->SetVectorParameterValue(FName("Color"), FLinearColor::Green);

	ConstraintLimitMaterialZ = UMaterialInstanceDynamic::Create(ConstraintLimitMaterial, NULL);
	ConstraintLimitMaterialZ->SetVectorParameterValue(FName("Color"), FLinearColor::Blue);
	ConstraintLimitMaterialZ->SetScalarParameterValue(FName("Desaturation"), 0.6f);
	ConstraintLimitMaterialZAxis = UMaterialInstanceDynamic::Create(ConstraintLimitMaterial, NULL);
	ConstraintLimitMaterialZAxis->SetVectorParameterValue(FName("Color"), FLinearColor::Blue);

	ConstraintLimitMaterialPrismatic = UMaterialInstanceDynamic::Create(ConstraintLimitMaterial, NULL);
	ConstraintLimitMaterialPrismatic->SetVectorParameterValue(FName("Color"), FLinearColor(FColor::Orange));
#endif


	if (GIsEditor && !IsRunningCommandlet())
	{
		// Materials that are only needed in the interactive editor
#if WITH_EDITORONLY_DATA
		LoadSpecialMaterial(GeomMaterialName.ToString(), GeomMaterial, false);
		LoadSpecialMaterial(EditorBrushMaterialName.ToString(), EditorBrushMaterial, false);
		LoadSpecialMaterial(BoneWeightMaterialName.ToString(), BoneWeightMaterial, false);
		LoadSpecialMaterial(ClothPaintMaterialName.ToString(), ClothPaintMaterial, false);
		LoadSpecialMaterial(ClothPaintMaterialWireframeName.ToString(), ClothPaintMaterialWireframe, false);
		LoadSpecialMaterial(DebugEditorMaterialName.ToString(), DebugEditorMaterial, false);

		ClothPaintMaterialInstance = UMaterialInstanceDynamic::Create(ClothPaintMaterial, nullptr);
		ClothPaintMaterialWireframeInstance = UMaterialInstanceDynamic::Create(ClothPaintMaterialWireframe, nullptr);
#endif

		LoadSpecialMaterial(PreviewShadowsIndicatorMaterialName.ToString(), PreviewShadowsIndicatorMaterial, false);
		
		//@TODO: This should move into the editor (used in editor modes exclusively)
		if (DefaultBSPVertexTexture == NULL)
		{
			DefaultBSPVertexTexture = LoadObject<UTexture2D>(NULL, *DefaultBSPVertexTextureName.ToString(), NULL, LOAD_None, NULL);
		}
	}

	if( DefaultTexture == NULL )
	{
		DefaultTexture = LoadObject<UTexture2D>(NULL, *DefaultTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if( DefaultDiffuseTexture == NULL )
	{
		DefaultDiffuseTexture = LoadObject<UTexture2D>(NULL, *DefaultDiffuseTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if( HighFrequencyNoiseTexture == NULL )
	{
		HighFrequencyNoiseTexture = LoadObject<UTexture2D>(NULL, *HighFrequencyNoiseTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if( DefaultBokehTexture == NULL )
	{
		DefaultBokehTexture = LoadObject<UTexture2D>(NULL, *DefaultBokehTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if (DefaultBloomKernelTexture == NULL)
	{
		DefaultBloomKernelTexture = LoadObject<UTexture2D>(NULL, *DefaultBloomKernelTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if( PreIntegratedSkinBRDFTexture == NULL )
	{
		PreIntegratedSkinBRDFTexture = LoadObject<UTexture2D>(NULL, *PreIntegratedSkinBRDFTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if( MiniFontTexture == NULL )
	{
		MiniFontTexture = LoadObject<UTexture2D>(NULL, *MiniFontTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if( WeightMapPlaceholderTexture == NULL )
	{
		WeightMapPlaceholderTexture = LoadObject<UTexture2D>(NULL, *WeightMapPlaceholderTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if (LightMapDensityTexture == NULL)
	{
		LightMapDensityTexture = LoadObject<UTexture2D>(NULL, *LightMapDensityTextureName.ToString(), NULL, LOAD_None, NULL);
	}

	if ( DefaultPhysMaterial == NULL )
	{
		DefaultPhysMaterial = LoadObject<UPhysicalMaterial>(NULL, *DefaultPhysMaterialName.ToString(), NULL, LOAD_None, NULL);
		if(!DefaultPhysMaterial)
		{
			UE_LOG(LogEngine, Error, TEXT("The default physical material (%s) was not found. Please make sure you have your default physical material set up correctly."), *DefaultPhysMaterialName.ToString());
			DefaultPhysMaterial = NewObject<UPhysicalMaterial>();
		}
	}

	LoadEngineClass<UConsole>(ConsoleClassName, ConsoleClass);
	LoadEngineClass<UGameViewportClient>(GameViewportClientClassName, GameViewportClientClass);
	LoadEngineClass<ULocalPlayer>(LocalPlayerClassName, LocalPlayerClass);
	LoadEngineClass<AWorldSettings>(WorldSettingsClassName, WorldSettingsClass);
	LoadEngineClass<UNavigationSystem>(NavigationSystemClassName, NavigationSystemClass);
	LoadEngineClass<UAvoidanceManager>(AvoidanceManagerClassName, AvoidanceManagerClass);
	LoadEngineClass<UPhysicsCollisionHandler>(PhysicsCollisionHandlerClassName, PhysicsCollisionHandlerClass);
	LoadEngineClass<UGameUserSettings>(GameUserSettingsClassName, GameUserSettingsClass);
	LoadEngineClass<ALevelScriptActor>(LevelScriptActorClassName, LevelScriptActorClass);

	// set the font object pointers, unless on server
	if (!IsRunningDedicatedServer())
	{
		auto ConditionalLoadEngineFont = [](UFont*& FontPtr, const FString& FontName)
		{
			if (!FontPtr && FontName.Len() > 0)
			{
				FontPtr = LoadObject<UFont>(nullptr, *FontName, nullptr, LOAD_None, nullptr);
			}
		};

		// Standard fonts.
		ConditionalLoadEngineFont(TinyFont, TinyFontName.ToString());
		ConditionalLoadEngineFont(SmallFont, SmallFontName.ToString());
		ConditionalLoadEngineFont(MediumFont, MediumFontName.ToString());
		ConditionalLoadEngineFont(LargeFont, LargeFontName.ToString());
		ConditionalLoadEngineFont(SubtitleFont, SubtitleFontName.ToString());

		// Additional fonts.
		AdditionalFonts.Empty(AdditionalFontNames.Num());
		for (const FString& FontName : AdditionalFontNames)
		{
			UFont* NewFont = nullptr;
			ConditionalLoadEngineFont(NewFont, FontName);
			AdditionalFonts.Add(NewFont);
		}
	}

	if (GameSingleton == nullptr && GameSingletonClassName.ToString().Len() > 0)
	{
		UClass *SingletonClass = LoadClass<UObject>(nullptr, *GameSingletonClassName.ToString());

		if (SingletonClass)
		{
			GameSingleton = NewObject<UObject>(this, SingletonClass);
		}
		else
		{
			UE_LOG(LogEngine, Error, TEXT("Engine config value GameSingletonClassName '%s' is not a valid class name."), *GameSingletonClassName.ToString());
		}
	}

	if (AssetManager == nullptr && AssetManagerClassName.ToString().Len() > 0)
	{
		UClass *SingletonClass = LoadClass<UObject>(nullptr, *AssetManagerClassName.ToString());

		if (SingletonClass)
		{
			AssetManager = NewObject<UAssetManager>(this, SingletonClass);

			if (AssetManager)
			{
				AssetManager->StartInitialLoading();
			}
		}
		else
		{
			UE_LOG(LogEngine, Error, TEXT("Engine config value AssetManagerClassName '%s' is not a valid class name."), *AssetManagerClassName.ToString());
		}
	}

	UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	UISettings->ForceLoadResources();

	// This initializes the tag data if it hasn't been already
	UGameplayTagsManager::Get();
}

void UEngine::InitializePortalServices()
{
	IMessagingRpcModule* MessagingRpcModule = nullptr;
	IPortalRpcModule* PortalRpcModule = nullptr;
	IPortalServicesModule* PortalServicesModule = nullptr;

#if WITH_PORTAL_SERVICES && UE_EDITOR
	MessagingRpcModule = static_cast<IMessagingRpcModule*>(FModuleManager::Get().LoadModule("MessagingRpc"));
	PortalRpcModule = static_cast<IPortalRpcModule*>(FModuleManager::Get().LoadModule("PortalRpc"));
	PortalServicesModule = static_cast<IPortalServicesModule*>(FModuleManager::Get().LoadModule("PortalServices"));
#endif

	if (MessagingRpcModule &&
		PortalRpcModule &&
		PortalServicesModule)
	{
		// Initialize Portal services
		PortalRpcClient = MessagingRpcModule->CreateRpcClient();
		{
			// @todo gmp: catch timeouts?
		}

		PortalRpcLocator = PortalRpcModule->CreateLocator();
		{
			PortalRpcLocator->OnServerLocated().BindLambda([=]() { PortalRpcClient->Connect(PortalRpcLocator->GetServerAddress()); });
			PortalRpcLocator->OnServerLost().BindLambda([=]() { PortalRpcClient->Disconnect(); });
		}

		ServiceDependencies = MakeShareable(new FTypeContainer);
		{
			ServiceDependencies->RegisterInstance<IMessageRpcClient>(PortalRpcClient.ToSharedRef());
		}

		ServiceLocator = PortalServicesModule->CreateLocator(ServiceDependencies.ToSharedRef());
		{
			// @todo add any Engine specific Portal services here
			ServiceLocator->Configure(TEXT("IPortalApplicationWindow"), TEXT("*"), "PortalProxies");
			ServiceLocator->Configure(TEXT("IPortalUser"), TEXT("*"), "PortalProxies");
			ServiceLocator->Configure(TEXT("IPortalUserLogin"), TEXT("*"), "PortalProxies");
		}
	}
	else
	{
		class FNullPortalServiceLocator
			: public IPortalServiceLocator
		{
			virtual void Configure(const FString& ServiceName, const FWildcardString& ProductId, const FName& ServiceModule) override
			{

			}

			virtual TSharedPtr<IPortalService> GetService(const FString& ServiceName, const FString& ProductId) override
			{
				return nullptr;
			}
		};

		ServiceLocator = MakeShareable(new FNullPortalServiceLocator());
	}
}

//
// Exit the engine.
//
void UEngine::FinishDestroy()
{
	// Remove from root.
	RemoveFromRoot();

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// shut down all subsystems.
		GEngine = NULL;
		ShutdownAudioDeviceManager();

		FURL::StaticExit();
	}

	Super::FinishDestroy();
}

void UEngine::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// count memory
	if (Ar.IsCountingMemory())
	{
		// Only use the main audio device when counting memory
		if (FAudioDevice* AudioDevice = GetMainAudioDevice())
		{
			AudioDevice->CountBytes(Ar);
		}
	}
}

void UEngine::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UEngine* This = CastChecked<UEngine>(InThis);

	// track objects in all the active audio devices
	if (This->AudioDeviceManager)
	{
		This->AudioDeviceManager->AddReferencedObjects(Collector);
	}

	// TODO: This is quite dangerous as FWorldContext::AddReferencedObjects could fail to be updated when something it
	//       references changes.  Hopefully something will come along that will allow the ustruct to be provided to the
	//       collector in a property handling method
	for (FWorldContext& Context : This->WorldList)
	{
		Context.AddReferencedObjects(Collector, This);
	}

	Super::AddReferencedObjects(This, Collector);
}

void UEngine::CleanupGameViewport()
{
	for (auto WorldIt = WorldList.CreateIterator(); WorldIt; ++WorldIt)
	{
		FWorldContext &Context = *WorldIt;

		if ( Context.OwningGameInstance != NULL )
		{
			Context.OwningGameInstance->CleanupGameViewport();
		}

		if ( Context.GameViewport != NULL && Context.GameViewport->Viewport == NULL )
		{
			if (Context.GameViewport == GameViewport)
			{
				GameViewport = NULL;
			}

			Context.GameViewport->DetachViewportClient();
			Context.GameViewport = NULL;
		}
	}
}

bool UEngine::IsEditor()
{
	return GIsEditor;
}


UFont* UEngine::GetTinyFont()
{
	return GEngine->TinyFont;
}


UFont* UEngine::GetSmallFont()
{
	return GEngine->SmallFont;
}


UFont* UEngine::GetMediumFont()
{
	return GEngine->MediumFont;
}

/**
 * Returns the engine's default large font
 */
UFont* UEngine::GetLargeFont()
{
	return GEngine->LargeFont;
}

/**
 * Returns the engine's default subtitle font
 */
UFont* UEngine::GetSubtitleFont()
{
	return GEngine->SubtitleFont;
}

/**
 * Returns the specified additional font.
 *
 * @param	AdditionalFontIndex		Index into the AddtionalFonts array.
 */
UFont* UEngine::GetAdditionalFont(int32 AdditionalFontIndex)
{
	return GEngine->AdditionalFonts.IsValidIndex(AdditionalFontIndex) ? GEngine->AdditionalFonts[AdditionalFontIndex] : NULL;
}

class FAudioDeviceManager* UEngine::GetAudioDeviceManager()
{
	return AudioDeviceManager;
}

uint32 UEngine::GetAudioDeviceHandle() const
{
	return MainAudioDeviceHandle;
}

FAudioDevice* UEngine::GetMainAudioDevice()
{
	if (AudioDeviceManager != nullptr)
	{
		return AudioDeviceManager->GetAudioDevice(MainAudioDeviceHandle);
	}
	return nullptr;
}

FAudioDevice* UEngine::GetActiveAudioDevice()
{
	if (AudioDeviceManager != nullptr)
	{
		return AudioDeviceManager->GetActiveAudioDevice();
	}
	return nullptr;
}

/**
 *	Initialize the audio device
 *
 *	@return	bool		true if successful, false if not
 */
bool UEngine::InitializeAudioDeviceManager()
{
	if (AudioDeviceManager == nullptr)
	{
		// Initialize the audio device.
		if (bUseSound == true)
		{
			// Check if we're going to try to force loading the audio mixer from the command line
			bool bForceAudioMixer = FParse::Param(FCommandLine::Get(), TEXT("AudioMixer"));

			// If not using command line switch to use audio mixer, check the engine ini file
			if (!bForceAudioMixer)
			{
				GConfig->GetBool(TEXT("Audio"), TEXT("EnableAudioMixer"), bForceAudioMixer, GEngineIni);
			}

			FString AudioDeviceModuleName;
			if (bForceAudioMixer)
			{
				GConfig->GetString(TEXT("Audio"), TEXT("AudioMixerModuleName"), AudioDeviceModuleName, GEngineIni);
			}

			// get the module name from the ini file
			if (!bForceAudioMixer || AudioDeviceModuleName.IsEmpty())
			{
				GConfig->GetString(TEXT("Audio"), TEXT("AudioDeviceModuleName"), AudioDeviceModuleName, GEngineIni);
			}

			if (AudioDeviceModuleName.Len() > 0)
			{
				// load the module by name from the .ini
				IAudioDeviceModule* AudioDeviceModule = FModuleManager::LoadModulePtr<IAudioDeviceModule>(*AudioDeviceModuleName);

				// did the module exist?
				if (AudioDeviceModule)
				{
					const bool bIsAudioMixerEnabled = AudioDeviceModule->IsAudioMixerModule();
					GetMutableDefault<UAudioSettings>()->SetAudioMixerEnabled(bIsAudioMixerEnabled);

#if WITH_EDITOR
					if (bIsAudioMixerEnabled)
					{
						IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
						AudioEditorModule->RegisterAudioMixerAssetActions();
						AudioEditorModule->RegisterEffectPresetAssetActions();
					}
#endif

					// Create the audio device manager and register the platform module to the device manager
					AudioDeviceManager = new FAudioDeviceManager();
					AudioDeviceManager->RegisterAudioDeviceModule(AudioDeviceModule);

					FAudioDeviceManager::FCreateAudioDeviceResults NewDeviceResults;

					// Create a new audio device.
					if (AudioDeviceManager->CreateAudioDevice(true, NewDeviceResults))
					{
						MainAudioDeviceHandle = NewDeviceResults.Handle;
						AudioDeviceManager->SetActiveDevice(MainAudioDeviceHandle);
						FAudioThread::StartAudioThread();
					}
					else
					{
						ShutdownAudioDeviceManager();
					}
				}
			}
		}
	}

	return AudioDeviceManager != nullptr;
}

bool UEngine::UseSound() const
{
	return (bUseSound && AudioDeviceManager != nullptr);
}
/**
 * A fake stereo rendering device used to test stereo rendering without an attached device.
 */
class FFakeStereoRenderingDevice : public IStereoRendering
{
public:
	FFakeStereoRenderingDevice() 
	: FOVInDegrees(100)
	, MonoCullingDistance(0.0f)
	, Width(640)
	, Height(480)
	{
		static TAutoConsoleVariable<float> CVarEmulateStereoFOV(TEXT("r.StereoEmulationFOV"), 0, TEXT("FOV in degrees, of the imaginable HMD for stereo emulation"));
		static TAutoConsoleVariable<int32> CVarEmulateStereoWidth(TEXT("r.StereoEmulationWidth"), 0, TEXT("Width of the imaginable HMD for stereo emulation"));
		static TAutoConsoleVariable<int32> CVarEmulateStereoHeight(TEXT("r.StereoEmulationHeight"), 0, TEXT("Height of the imaginable HMD for stereo emulation"));
		float FOV = CVarEmulateStereoFOV.GetValueOnAnyThread();
		if (FOV != 0)
		{
			FOVInDegrees = FMath::Clamp(FOV, 20.f, 300.f);
		}
		int32 W = CVarEmulateStereoWidth.GetValueOnAnyThread();
		int32 H = CVarEmulateStereoHeight.GetValueOnAnyThread();
		if (W != 0)
		{
			Width = FMath::Clamp(W, 100, 10000);
		}
		if (H != 0)
		{
			Height = FMath::Clamp(H, 100, 10000);
		}
	}

	virtual ~FFakeStereoRenderingDevice() {}

	virtual bool IsStereoEnabled() const override { return true; }

	virtual bool EnableStereo(bool stereo = true) override { return true; }

	virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override
	{
		SizeX = SizeX / 2;
		if (StereoPass == eSSP_RIGHT_EYE)
		{
			X += SizeX;
		}
	}

	virtual void CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation) override
	{
		if (StereoPassType != eSSP_FULL && StereoPassType != eSSP_MONOSCOPIC_EYE)
		{
			float EyeOffset = 3.20000005f;
			const float PassOffset = (StereoPassType == eSSP_LEFT_EYE) ? EyeOffset : -EyeOffset;
			ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(0,PassOffset,0));
		}
	}

	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override
	{
		const float HalfFov = FMath::DegreesToRadians(FOVInDegrees) / 2.f;
		const float InWidth = Width;
		const float InHeight = Height;
		const float XS = 1.0f / FMath::Tan(HalfFov);
		const float YS = InWidth / FMath::Tan(HalfFov) / InHeight;
		const float NearZ = (StereoPassType != eSSP_MONOSCOPIC_EYE) ? GNearClippingPlane : MonoCullingDistance;

		return FMatrix(
			FPlane(XS, 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, YS, 0.0f, 0.0f),
			FPlane(0.0f, 0.0f, 0.0f, 1.0f),
			FPlane(0.0f, 0.0f, NearZ, 0.0f));
	}

	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override
	{
		if (InView != nullptr && InView->Family != nullptr)
		{
			const FSceneViewFamily& ViewFamily = *InView->Family;
			MonoCullingDistance = ViewFamily.MonoParameters.CullingDistance - ViewFamily.MonoParameters.OverlapDistance;
		}
	}

	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture, FVector2D WindowSize) const override
	{
		check(IsInRenderingThread());

		FRHIRenderTargetView BackBufferView = FRHIRenderTargetView(BackBuffer, ERenderTargetLoadAction::EClear);
		FRHISetRenderTargetsInfo Info(1, &BackBufferView, FRHIDepthRenderTargetView());
		RHICmdList.SetRenderTargetsAndClear(Info);
		const uint32 ViewportWidth = BackBuffer->GetSizeX();
		const uint32 ViewportHeight = BackBuffer->GetSizeY();
		RHICmdList.SetViewport( 0,0,0,ViewportWidth, ViewportHeight, 1.0f );
	}

	float FOVInDegrees;		// max(HFOV, VFOV) in degrees of imaginable HMD
	float MonoCullingDistance;
	int32 Width, Height;	// resolution of imaginable HMD
};

bool UEngine::InitializeHMDDevice()
{
	if (!IsRunningCommandlet())
	{
		static TAutoConsoleVariable<int32> CVarEmulateStereo(TEXT("r.EnableStereoEmulation"), 0, TEXT("Emulate stereo rendering"));
		if (FParse::Param(FCommandLine::Get(), TEXT("emulatestereo")) || CVarEmulateStereo.GetValueOnAnyThread() != 0)
		{
			TSharedPtr<FFakeStereoRenderingDevice, ESPMode::ThreadSafe> FakeStereoDevice(new FFakeStereoRenderingDevice());
			StereoRenderingDevice = FakeStereoDevice;
		}
		// No reason to connect an HMD on a dedicated server.  Also fixes dedicated servers stealing the oculus connection.
		else if (!XRSystem.IsValid() && !FParse::Param(FCommandLine::Get(), TEXT("nohmd")) && !IsRunningDedicatedServer())
		{
			// Get a list of modules that implement this feature
			FName Type = IHeadMountedDisplayModule::GetModularFeatureName();
			IModularFeatures& ModularFeatures = IModularFeatures::Get();
			TArray<IHeadMountedDisplayModule*> HMDModules = ModularFeatures.GetModularFeatureImplementations<IHeadMountedDisplayModule>(Type);

			// Check whether the user passed in an explicit HMD module on the command line
			FString ExplicitHMDName;
			bool bUseExplicitHMDDevice = FParse::Value(FCommandLine::Get(), TEXT("hmd="), ExplicitHMDName);
						
			// Sort modules by priority
			HMDModules.Sort(IHeadMountedDisplayModule::FCompareModulePriority());

			// Select first module with a connected HMD able to create a device
			IHeadMountedDisplayModule* HMDModuleSelected = nullptr;
			TArray<IHeadMountedDisplayModule*> HMDModulesDisconnected;

			for (auto HMDModuleIt = HMDModules.CreateIterator(); HMDModuleIt; ++HMDModuleIt)
			{
				IHeadMountedDisplayModule* HMDModule = *HMDModuleIt;

				// Skip all non-matching modules when an explicit module name has been specified on the command line
				if (bUseExplicitHMDDevice)
				{
					TArray<FString> HMDAliases;
					HMDModule->GetModuleAliases(HMDAliases);
					HMDAliases.Add(HMDModule->GetModuleKeyName());

					bool bMatchesExplicitDevice = false;
					for (const FString& HMDModuleName : HMDAliases)
					{
						if (ExplicitHMDName.Equals(HMDModuleName, ESearchCase::IgnoreCase))
						{
							bMatchesExplicitDevice = true;
							break;
						}
					}

					if (!bMatchesExplicitDevice)
					{
						continue;
					}
				}

				if(HMDModule->IsHMDConnected())
				{
					XRSystem = HMDModule->CreateTrackingSystem();

					if (XRSystem.IsValid())
					{
						HMDModuleSelected = HMDModule;
						break;
					}
				}
				else
				{
					HMDModulesDisconnected.Add(HMDModule);
				}
			}

			// If no module selected yet, just select first module able to create a device, even if HMD is not connected.
			if (!HMDModuleSelected)
			{
				for (auto HMDModuleIt = HMDModulesDisconnected.CreateIterator(); HMDModuleIt; ++HMDModuleIt)
				{
					IHeadMountedDisplayModule* HMDModule = *HMDModuleIt;

					XRSystem = HMDModule->CreateTrackingSystem();

					if (XRSystem.IsValid())
					{
						HMDModuleSelected = HMDModule;
						break;
					}
				}
			}

			// Unregister modules which were not selected, since they will not be used.
			for (auto HMDModuleIt = HMDModules.CreateIterator(); HMDModuleIt; ++HMDModuleIt)
			{
				IHeadMountedDisplayModule* HMDModule = *HMDModuleIt;

				if(HMDModule != HMDModuleSelected)
				{
					ModularFeatures.UnregisterModularFeature(Type, HMDModule);
				}
			}

			// If we found a valid XRSystem, use it to get a stereo rendering device, if available
			if (XRSystem.IsValid())
			{
				StereoRenderingDevice = XRSystem->GetStereoRenderingDevice();
				const bool bShouldStartInVR = StereoRenderingDevice.IsValid() && (FParse::Param(FCommandLine::Get(), TEXT("vr")) || GetDefault<UGeneralProjectSettings>()->bStartInVR);
				if (bShouldStartInVR)
				{
					StereoRenderingDevice->EnableStereo(true);
				}
			}
			// Else log an error if we got an explicit module name on the command line
			else if (bUseExplicitHMDDevice)
			{
				UE_LOG(LogInit, Error, TEXT("Failed to find or initialize HMD module named '%s'. HMD mode will be disabled."), *ExplicitHMDName);
			}

		}
	}
 
	return StereoRenderingDevice.IsValid();
}


void UEngine::RecordHMDAnalytics()
{
	if (XRSystem.IsValid() && !FParse::Param(FCommandLine::Get(), TEXT("nohmd")) && XRSystem->GetHMDDevice() && XRSystem->GetHMDDevice()->IsHMDConnected())
	{
		XRSystem->GetHMDDevice()->RecordAnalytics();
	}
}

/** @return whether we're currently running in split screen (more than one local player) */
bool UEngine::IsSplitScreen(UWorld *InWorld)
{
	if (InWorld == NULL)
	{
		// If no specified world, return true if any world context has multiple local players
		for (auto It = WorldList.CreateIterator(); It; ++It)
		{
			if (It->OwningGameInstance != NULL && It->OwningGameInstance->GetNumLocalPlayers() > 1)
			{
				return true;
			}
		}

		return false;
	}

	return (GetNumGamePlayers(InWorld) > 1);
}

/** @return whether we're currently running with stereoscopic 3D enabled */
bool UEngine::IsStereoscopic3D(FViewport* InViewport)
{
	return (!InViewport || InViewport->IsStereoRenderingAllowed()) &&
		   (StereoRenderingDevice.IsValid() && StereoRenderingDevice->IsStereoEnabled());
}

ULocalPlayer* GetLocalPlayerFromControllerId_local(const TArray<class ULocalPlayer*>& GamePlayers, const int32 ControllerId)
{
	for ( ULocalPlayer* const Player : GamePlayers )
	{
		if ( Player && Player->GetControllerId() == ControllerId )
		{
			return Player;
		}
	}

	return nullptr;
}

ULocalPlayer* UEngine::GetLocalPlayerFromControllerId( const UGameViewportClient* InViewport, const int32 ControllerId ) const
{
	if (GetWorldContextFromGameViewport(InViewport) != nullptr)
	{
		const TArray<class ULocalPlayer*>& GamePlayers = GetGamePlayers(InViewport);
		return GetLocalPlayerFromControllerId_local(GamePlayers, ControllerId);
	}
	return nullptr;
}

ULocalPlayer* UEngine::GetLocalPlayerFromControllerId( UWorld * InWorld, const int32 ControllerId ) const
{
	const TArray<class ULocalPlayer*>& GamePlayers = GetGamePlayers(InWorld);
	return GetLocalPlayerFromControllerId_local(GamePlayers, ControllerId);
}

void UEngine::SwapControllerId(ULocalPlayer *NewPlayer, const int32 CurrentControllerId, const int32 NewControllerID) const
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		if (It->OwningGameInstance == nullptr)
		{
			continue;
		}

		const TArray<class ULocalPlayer*> & LocalPlayers = It->OwningGameInstance->GetLocalPlayers();

		if (LocalPlayers.Contains(NewPlayer))
		{
			// This is the world context that NewPlayer belongs to, see if anyone is using his CurrentControllerId
			for (ULocalPlayer* LocalPlayer : LocalPlayers)
			{
				if(LocalPlayer && LocalPlayer->GetControllerId() == NewControllerID)
				{
					LocalPlayer->SetControllerId( CurrentControllerId);
					return;
				}
			}
		}
	}
}

APlayerController* UEngine::GetFirstLocalPlayerController(UWorld *InWorld)
{
	const FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	
	return ( Context.OwningGameInstance != NULL ) ? Context.OwningGameInstance->GetFirstLocalPlayerController(InWorld) : NULL;
}

void UEngine::GetAllLocalPlayerControllers(TArray<APlayerController*> & PlayerList)
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		if ( It->OwningGameInstance == NULL )
		{
			continue;
		}
		for (auto PlayerIt = It->OwningGameInstance->GetLocalPlayerIterator(); PlayerIt; ++PlayerIt)
		{
			ULocalPlayer *Player = *PlayerIt;
			if (Player->PlayerController)
			{
				PlayerList.Add( Player->PlayerController );
			}
		}
	}
}


/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

#if !UE_BUILD_SHIPPING

/**
 * Helper structure for sorting textures by relative cost.
 */
struct FSortedTexture 
{
	int32		MaxAllowedSizeX;	// This is the disk size when cooked.
	int32		MaxAllowedSizeY;
	EPixelFormat Format;
	int32		CurSizeX;
	int32		CurSizeY;
	int32		LODBias;
	int32		MaxAllowedSize;
	int32		CurrentSize;
	FString		Name;
	int32		LODGroup;
	bool		bIsStreaming;
	int32		UsageCount;

	/** Constructor, initializing every member variable with passed in values. */
	FSortedTexture(	int32 InMaxAllowedSizeX, int32 InMaxAllowedSizeY, EPixelFormat InFormat, int32 InCurSizeX, int32 InCurSizeY, int32 InLODBias, int32 InMaxAllowedSize, int32 InCurrentSize, const FString& InName, int32 InLODGroup, bool bInIsStreaming, int32 InUsageCount )
	:	MaxAllowedSizeX( InMaxAllowedSizeX )
	,	MaxAllowedSizeY( InMaxAllowedSizeY )
	,	Format( InFormat )
	,	CurSizeX( InCurSizeX )
	,	CurSizeY( InCurSizeY )
	,	LODBias( InLODBias )
	,	MaxAllowedSize( InMaxAllowedSize )
	,	CurrentSize( InCurrentSize )
	,	Name( InName )
	,	LODGroup( InLODGroup )
	,	bIsStreaming( bInIsStreaming )
	,	UsageCount( InUsageCount )
	{}
};
struct FCompareFSortedTexture
{
	bool bAlphaSort;
	FCompareFSortedTexture( bool InAlphaSort )
		: bAlphaSort( InAlphaSort )
	{}
	FORCEINLINE bool operator()( const FSortedTexture& A, const FSortedTexture& B ) const
	{
		return bAlphaSort ? ( A.Name < B.Name ) : ( B.CurrentSize < A.CurrentSize );
	}
};

/** Helper struct for sorting anim sets by size */
struct FSortedSet
{
	FString Name;
	int32		Size;

	FSortedSet( const FString& InName, int32 InSize )
	:	Name(InName)
	,	Size(InSize)
	{}
};
struct FCompareFSortedSet
{
	bool bAlphaSort;
	FCompareFSortedSet( bool InAlphaSort )
		: bAlphaSort( InAlphaSort )
	{}
	FORCEINLINE bool operator()( const FSortedSet& A, const FSortedSet& B ) const
	{
		return bAlphaSort ? ( A.Name < B.Name ) : ( B.Size < A.Size );
	}
};

#if !UE_BUILD_SHIPPING
struct FSortedParticleSet
{
	FString Name;
	int32		Size;
	int32		PSysSize;
	int32		ModuleSize;
	int32		ComponentSize;
	int32		ComponentCount;
	FResourceSizeEx ComponentResourceSize;
	FResourceSizeEx ComponentTrueResourceSize;

	FSortedParticleSet( const FString& InName, int32 InSize, int32 InPSysSize, int32 InModuleSize, 
		int32 InComponentSize, int32 InComponentCount, FResourceSizeEx InComponentResourceSize, FResourceSizeEx InComponentTrueResourceSize) :
		  Name(InName)
		, Size(InSize)
		, PSysSize(InPSysSize)
		, ModuleSize(InModuleSize)
		, ComponentSize(InComponentSize)
		, ComponentCount(InComponentCount)
		, ComponentResourceSize(InComponentResourceSize)
		, ComponentTrueResourceSize(InComponentTrueResourceSize)
	{}

	FSortedParticleSet(const FString& InName)
		: Name(InName)
		, Size(0)
		, PSysSize(0)
		, ModuleSize(0)
		, ComponentSize(0)
		, ComponentCount(0)
		, ComponentResourceSize(EResourceSizeMode::Inclusive)
		, ComponentTrueResourceSize(EResourceSizeMode::Exclusive)
	{
	}

	const FSortedParticleSet& operator += (const FSortedParticleSet& InOther)
	{
		Size += InOther.Size;
		PSysSize += InOther.PSysSize;
		ModuleSize += InOther.ModuleSize;
		ComponentSize += InOther.ComponentSize;
		ComponentCount += InOther.ComponentCount;
		ComponentResourceSize += InOther.ComponentResourceSize;
		ComponentTrueResourceSize += InOther.ComponentTrueResourceSize;
		return *this;
	}

	void Dump(FOutputDevice& InArchive)
	{
		InArchive.Logf(TEXT("%10d,%s,%d,%d,%d,%d,%d,%d"),
			Size, *Name, PSysSize, ModuleSize, ComponentSize,
			ComponentCount, ComponentResourceSize.GetTotalMemoryBytes(), ComponentTrueResourceSize.GetTotalMemoryBytes());
	}
};

struct FCompareFSortedParticleSet
{
	bool bAlphaSort;
	FCompareFSortedParticleSet( bool InAlphaSort )
		: bAlphaSort( InAlphaSort )
	{}
	FORCEINLINE bool operator()( const FSortedParticleSet& A, const FSortedParticleSet& B ) const
	{
		return bAlphaSort ? ( A.Name < B.Name ) : ( B.Size < A.Size );
	}
};

#endif

static void ShowSubobjectGraph( FOutputDevice& Ar, UObject* CurrentObject, const FString& IndentString )
{
	if ( CurrentObject == NULL )
	{
		Ar.Logf(TEXT("%sX NULL"), *IndentString);
	}
	else
	{
		TArray<UObject*> ReferencedObjs;
		FReferenceFinder RefCollector( ReferencedObjs, CurrentObject, true, false, false, false);
		RefCollector.FindReferences( CurrentObject );

		if ( ReferencedObjs.Num() == 0 )
		{
			Ar.Logf(TEXT("%s. %s"), *IndentString, IndentString.Len() == 0 ? *CurrentObject->GetPathName() : *CurrentObject->GetName());
		}
		else
		{
			Ar.Logf(TEXT("%s+ %s"), *IndentString, IndentString.Len() == 0 ? *CurrentObject->GetPathName() : *CurrentObject->GetName());
			for ( int32 ObjIndex = 0; ObjIndex < ReferencedObjs.Num(); ObjIndex++ )
			{
				ShowSubobjectGraph( Ar, ReferencedObjs[ObjIndex], IndentString + TEXT( "|\t" ) );
			}
		}
	}
}
struct FItem
{
	UClass*	Class;
	int32 Count;
	SIZE_T Num;
	SIZE_T Max;
	/** Only exclusive resource size, the truer resource size. */
	FResourceSizeEx TrueResourceSize;

	FItem( UClass* InClass=NULL )
	: Class(InClass), Count(0), Num(0), Max(0), TrueResourceSize()
	{}

	FItem( UClass* InClass, int32 InCount, SIZE_T InNum, SIZE_T InMax, FResourceSizeEx InTrueResourceSize ) :
		Class( InClass ),
		Count( InCount ),
		Num( InNum ), 
		Max( InMax ), 
		TrueResourceSize( InTrueResourceSize )
	{}

	void Add( FArchiveCountMem& Ar, FResourceSizeEx InTrueResourceSize )
	{
		Count++;
		Num += Ar.GetNum();
		Max += Ar.GetMax();
		TrueResourceSize += InTrueResourceSize;
	}
};

struct FSubItem
{
	UObject* Object;
	/** Size of the object, counting containers as current usage */
	SIZE_T Num;
	/** Size of the object, counting containers as total allocated (max usage) */
	SIZE_T Max;
	/** Resource size of the object and all of its references, the 'old-style'. */
	SIZE_T ResourceSize;
	/** Only exclusive resource size, the truer resource size. */
	FResourceSizeEx TrueResourceSize;

	FSubItem( UObject* InObject, SIZE_T InNum, SIZE_T InMax, FResourceSizeEx InTrueResourceSize )
	: Object( InObject ), Num( InNum ), Max( InMax ), TrueResourceSize( InTrueResourceSize )
	{}
};

#endif // !UE_BUILD_SHIPPING

MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : 4717))

#if defined (__clang__) 
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Winfinite-recursion"
#endif


// clang can optimize this out (http://stackoverflow.com/questions/18478078/clang-infinite-tail-recursion-optimization), make the function do some useful work

volatile int GInfiniteRecursionCount = 0;
int InfiniteRecursionFunction(int B)
{
	GInfiniteRecursionCount += InfiniteRecursionFunction(B + 1);
	return GInfiniteRecursionCount;
}

#if defined (__clang__) 
	#pragma clang diagnostic pop
#endif

MSVC_PRAGMA(warning(pop))

/** DEBUG used for exe "DEBUG BUFFEROVERFLOW" */
static void BufferOverflowFunction(SIZE_T BufferSize, const ANSICHAR* Buffer) 
{
	ANSICHAR LocalBuffer[32];
	LocalBuffer[0] = LocalBuffer[31] = 0; //if BufferSize is 0 then there's nothing to print out!

	BufferSize = FMath::Min<SIZE_T>(BufferSize, ARRAY_COUNT(LocalBuffer)-1);

	for( uint32 i = 0; i < BufferSize; i++ ) 
	{
		LocalBuffer[i] = Buffer[i];
	}
	UE_LOG(LogEngine, Log, TEXT("BufferOverflowFunction BufferSize=%d LocalBuffer=%s"),(int32)BufferSize, ANSI_TO_TCHAR(LocalBuffer));
}

bool UEngine::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// If we don't have a viewport specified to catch the stat commands, use to the game viewport
	if (GStatProcessingViewportClient == NULL)
	{
		GStatProcessingViewportClient = GameViewport;
	}


	// See if any other subsystems claim the command.
	if (StaticExec(InWorld, Cmd,Ar) == true)
	{
		return true;
	}
	

	if (GDebugToolExec && (GDebugToolExec->Exec( InWorld, Cmd,Ar) == true))
	{
		return true;
	}

	if (GMalloc && (GMalloc->Exec( InWorld, Cmd,Ar) == true))
	{
		return true;
	}

	if (GSystemSettings.Exec( InWorld, Cmd,Ar) == true)
	{
		return true;
	}

	FAudioDevice* AudioDevice = nullptr;
	if (InWorld)
	{
		AudioDevice = InWorld->GetAudioDevice();
	}
	else
	{
		AudioDevice = GetMainAudioDevice();
	}

	if (AudioDevice && AudioDevice->Exec(InWorld, Cmd, Ar) == true)
	{
		return true;
	}
	
	if (FPlatformMisc::Exec( InWorld, Cmd,Ar) == true)
	{
		return true;
	}

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	if (FLowLevelMemTracker::Get().Exec(Cmd, Ar))
	{
		return true;
	}
#endif

	{
		FString CultureName;
		if (FParse::Value(Cmd, TEXT("CULTURE="), CultureName))
		{
			FInternationalization::Get().SetCurrentCulture(CultureName);
		}
	}

	{
		FString LanguageName;
		if (FParse::Value(Cmd, TEXT("LANGUAGE="), LanguageName))
		{
			FInternationalization::Get().SetCurrentLanguage(LanguageName);
		}
	}

	{
		FString LocaleName;
		if (FParse::Value(Cmd, TEXT("LOCALE="), LocaleName))
		{
			FInternationalization::Get().SetCurrentLocale(LocaleName);
		}
	}

#if ENABLE_LOC_TESTING
	{
		FString ConfigFilePath;
		if (FParse::Value(Cmd, TEXT("REGENLOC="), ConfigFilePath))
		{
			ILocalizationModule::Get().HandleRegenLocCommand(ConfigFilePath, /*bSkipSourceCheck*/false);
		}
	}
#endif

	// Handle engine command line.
	if ( FParse::Command(&Cmd,TEXT("FLUSHLOG")) )
	{
		return HandleFlushLogCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd, TEXT("GAMEVER")) ||  FParse::Command(&Cmd, TEXT("GAMEVERSION")))
	{
		return HandleGameVerCommand( Cmd, Ar );
	}
#if 0
	else if (FParse::Command(&Cmd, TEXT("HOTFIXTEST")))
	{
		FTestHotFixPayload Test;
		Test.ValueToReturn = true;
		Test.Result = false;
		Test.Message = FString(TEXT("Hi there"));
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).ExecuteIfBound(&Test, sizeof(FTestHotFixPayload));
		check(Test.ValueToReturn == Test.Result);
	}
#endif
	else if( FParse::Command(&Cmd,TEXT("STAT")) )
	{
		return HandleStatCommand(InWorld, GStatProcessingViewportClient, Cmd, Ar);
	}
	else if( FParse::Command(&Cmd,TEXT("STOPMOVIECAPTURE")) && GIsEditor )
	{
		return HandleStopMovieCaptureCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("CRACKURL")) )
	{
		return HandleCrackURLCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DEFER")) )
	{
		return HandleDeferCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("OPEN") ) )
	{
		return HandleOpenCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("STREAMMAP")) )
	{
		return HandleStreamMapCommand( Cmd, Ar, InWorld );
	}
#if WITH_SERVER_CODE
	else if (FParse::Command(&Cmd, TEXT("SERVERTRAVEL")) )
	{
		return HandleServerTravelCommand( Cmd, Ar, InWorld );
	}
#endif // WITH_SERVER_CODE
	else if( FParse::Command( &Cmd, TEXT("DISCONNECT")) )
	{
		return HandleDisconnectCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("RECONNECT")) )
	{
		return HandleReconnectCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("TRAVEL") ) )
	{
		return HandleTravelCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("CE")))
	{
		return HandleCeCommand( InWorld, Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("GAMMA") ) )
	{
		return HandleGammaCommand( Cmd, Ar );
	}
#if STATS
	else if( FParse::Command( &Cmd, TEXT("DUMPPARTICLEMEM") ) )
	{
		return HandleDumpParticleMemCommand( Cmd, Ar );
	}
#endif

#if WITH_PROFILEGPU
	else if( FParse::Command(&Cmd,TEXT("PROFILEGPU")) )
	{
		return HandleProfileGPUCommand( Cmd, Ar );
	}	
#endif // #if !UE_BUILD_SHIPPING

#if	!(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_HOT_RELOAD
	else if( FParse::Command(&Cmd,TEXT("HotReload")) )
	{
		return HandleHotReloadCommand( Cmd, Ar );
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_HOT_RELOAD


#if !UE_BUILD_SHIPPING
	else if (FParse::Command(&Cmd, TEXT("DumpConsoleCommands")))
	{
		return HandleDumpConsoleCommandsCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPAVAILABLERESOLUTIONS")))
	{
		return HandleDumpAvailableResolutionsCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("ANIMSEQSTATS")))
	{
		return HandleAnimSeqStatsCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("CountDisabledParticleItems")))
	{
		return HandleCountDisabledParticleItemsCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("VIEWNAMES") ) )
	{
		return HandleViewnamesCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("FREEZESTREAMING")) )
	{
		return HandleFreezeStreamingCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command(&Cmd,TEXT("FREEZEALL")) )
	{
		return HandleFreezeAllCommand( Cmd, Ar, InWorld );
	}
	
	else if( FParse::Command(&Cmd,TEXT("ToggleRenderingThread")) )
	{
		return HandleToggleRenderingThreadCommand( Cmd, Ar );
	}	
	else if (FParse::Command(&Cmd, TEXT("ToggleAsyncCompute")))
	{
		return HandleToggleAsyncComputeCommand(Cmd, Ar);
	}
	else if( FParse::Command(&Cmd,TEXT("RecompileShaders")) )				    
	{
		return HandleRecompileShadersCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("RecompileGlobalShaders")) )
	{
		return HandleRecompileGlobalShadersCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPSHADERSTATS")) )
	{
		return HandleDumpShaderStatsCommand( Cmd, Ar );		
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPMATERIALSTATS")) )
	{
		return HandleDumpMaterialStatsCommand( Cmd, Ar );	
	}
	else if (FParse::Command(&Cmd, TEXT("DumpShaderPipelineStats")))
	{
		return HandleDumpShaderPipelineStatsCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("visrt")))
	{
		extern bool HandleVisualizeRT();
		return HandleVisualizeRT();
	}
	else if( FParse::Command(&Cmd,TEXT("PROFILE")) )
	{
		return HandleProfileCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("PROFILEGPUHITCHES")) )				    
	{
		return HandleProfileGPUHitchesCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SHADERCOMPLEXITY")) )
	{
		return HandleShaderComplexityCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("FREEZERENDERING")) )
	{
		return HandleFreezeRenderingCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("ShowSelectedLightmap")))
	{
		return HandleShowSelectedLightmapCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SHOWLOG")) )
	{
		return HandleShowLogCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("STARTFPSCHART")) )
	{
		return HandleStartFPSChartCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("STOPFPSCHART")) )
	{
		return HandleStopFPSChartCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("DumpLevelScriptActors")))
	{
		return HandleDumpLevelScriptActorsCommand( InWorld, Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("KE")) || FParse::Command(&Cmd, TEXT("KISMETEVENT")))
	{
		return HandleKismetEventCommand( InWorld, Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("LISTTEXTURES")))
	{
		return HandleListTexturesCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("REMOTETEXTURESTATS")))
	{
		return HandleRemoteTextureStatsCommand( Cmd, Ar );
	}
	else if(FParse::Command(&Cmd,TEXT("LISTPARTICLESYSTEMS")))
	{
		return HandleListParticleSystemsCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("LISTSPAWNEDACTORS")) )
	{
		return HandleListSpawnedActorsCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("MemReport") ) )
	{
		return HandleMemReportCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("MemReportDeferred") ) )
	{
		return HandleMemReportDeferredCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("PARTICLEMESHUSAGE") ) )
	{
		return HandleParticleMeshUsageCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("DUMPPARTICLECOUNTS") ) )
	{
		return HandleDumpParticleCountsCommand( Cmd, Ar );
	}
	// we can't always do an obj linkers, as cooked games have their linkers tossed out.  So we need to look at the actual packages which are loaded
	else if( FParse::Command( &Cmd, TEXT("ListLoadedPackages") ) )
	{
		return HandleListLoadedPackagesCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd,TEXT("MEM")) )
	{
		return HandleMemCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("LOGOUTSTATLEVELS")) )
	{
		return HandleLogoutStatLevelsCommand( Cmd, Ar, InWorld );
	}
	else if( FParse::Command( &Cmd, TEXT("DEBUG") ) )
	{
		return HandleDebugCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("MERGEMESH")))
	{
		return HandleMergeMeshCommand( Cmd, Ar, InWorld );
	}
	else if (FParse::Command(&Cmd, TEXT("CONTENTCOMPARISON")))
	{
		return HandleContentComparisonCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("TOGGLEGTPSYSLOD")))
	{
		return HandleTogglegtPsysLODCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("OBJ")) )
	{
		return HandleObjCommand( Cmd, Ar );
	}
	else if( FParse::Command( &Cmd, TEXT("TESTSLATEGAMEUI")) && InWorld && InWorld->IsGameWorld() )
	{
		return HandleTestslateGameUICommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DIR")) )		// DIR [path\pattern]
	{
		return HandleDirCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("TRACKPARTICLERENDERINGSTATS")) )
	{
		return HandleTrackParticleRenderingStatsCommand( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("DUMPALLOCS")) )
	{
		return HandleDumpAllocatorStats( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("HEAPCHECK")) )
	{
		return HandleHeapCheckCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("TOGGLEONSCREENDEBUGMESSAGEDISPLAY")))
	{
		return HandleToggleOnscreenDebugMessageDisplayCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("TOGGLEONSCREENDEBUGMESSAGESYSTEM")))
	{
		return HandleToggleOnscreenDebugMessageSystemCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("DISABLEALLSCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("DISABLESCREENMESSAGES")))
	{
		return HandleDisableAllScreenMessagesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("ENABLEALLSCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("ENABLESCREENMESSAGES")))
	{
		return HandleEnableAllScreenMessagesCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd,TEXT("TOGGLEALLSCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("TOGGLESCREENMESSAGES")) || FParse::Command(&Cmd,TEXT("CAPTUREMODE")))
	{
		return HandleToggleAllScreenMessagesCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("CONFIGHASH")) )
	{
		return HandleConfigHashCommand( Cmd, Ar );
	}
	else if ( FParse::Command(&Cmd,TEXT("CONFIGMEM")) )
	{
		return HandleConfigMemCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("GETINI")))
	{
		return HandleGetIniCommand(Cmd, Ar);
	}
#endif // !UE_BUILD_SHIPPING
	
	else if ( FParse::Command(&Cmd,TEXT("SCALABILITY")) )
	{
		Scalability::ProcessCommand(Cmd, Ar);
		return true;
	}
	else if(IConsoleManager::Get().ProcessUserConsoleInput(Cmd, Ar, InWorld))
	{
		// console variable interaction (get value, set value or get help)
		return true;
	}
	else if (!IStreamingManager::HasShutdown() && IStreamingManager::Get().Exec( InWorld, Cmd,Ar ))
	{
		// The streaming manager has handled the exec command.
	}
	else if( FParse::Command(&Cmd, TEXT("DUMPTICKS")) )
	{
		return HandleDumpTicksCommand( InWorld, Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("CANCELASYNCLOAD")))
	{
		CancelAsyncLoading();
		return true;
	}
#if USE_NETWORK_PROFILER
	else if( FParse::Command(&Cmd,TEXT("NETPROFILE")) )
	{
		GNetworkProfiler.Exec( InWorld, Cmd, Ar );
	}
#endif
	else 
	{
		return false;
	}

	return true;
}

bool UEngine::HandleFlushLogCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GLog->FlushThreadedLogs();
	GLog->Flush();
	return true;
}

bool UEngine::HandleGameVerCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString VersionString = FString::Printf( TEXT( "GameVersion Branch: %s, Configuration: %s, Build: %s, CommandLine: %s" ),
											 *FApp::GetBranchName(), EBuildConfigurations::ToString( FApp::GetBuildConfiguration() ), FApp::GetBuildVersion(), FCommandLine::Get() );

	Ar.Logf( TEXT("%s"), *VersionString );
	FPlatformApplicationMisc::ClipboardCopy( *VersionString );

	if (FCString::Stristr(Cmd, TEXT("-display")))
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, VersionString);
	}

	return 1;
}

bool UEngine::HandleStatCommand( UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR* Temp = Cmd;
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		if (FParse::Command( &Temp, *EngineStat.CommandNameString ))
		{
			if (EngineStat.ToggleFunc)
			{
				return ViewportClient ? (this->*(EngineStat.ToggleFunc))(World, ViewportClient, Temp) : false;
			}
			return true;
		}
	}
	return false;
}

bool UEngine::HandleStopMovieCaptureCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (IMovieSceneCaptureInterface* CaptureInterface = IMovieSceneCaptureModule::Get().GetFirstActiveMovieSceneCapture())
	{
		CaptureInterface->Close();
		return true;
	}
	return false;
}

bool UEngine::HandleCrackURLCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FURL URL(NULL,Cmd,TRAVEL_Absolute);
	if( URL.Valid )
	{
		Ar.Logf( TEXT("     Protocol: %s"), *URL.Protocol );
		Ar.Logf( TEXT("         Host: %s"), *URL.Host );
		Ar.Logf( TEXT("         Port: %i"), URL.Port );
		Ar.Logf( TEXT("          Map: %s"), *URL.Map );
		Ar.Logf( TEXT("   NumOptions: %i"), URL.Op.Num() );
		for( int32 i=0; i<URL.Op.Num(); i++ )
			Ar.Logf( TEXT("     Option %i: %s"), i, *URL.Op[i] );
		Ar.Logf( TEXT("       Portal: %s"), *URL.Portal );
		Ar.Logf( TEXT("       String: '%s'"), *URL.ToString() );
	}
	else Ar.Logf( TEXT("BAD URL") );
	return 1;
}

bool UEngine::HandleDeferCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	new(DeferredCommands)FString(Cmd);
	return 1;
}

bool UEngine::HandleCeCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString ErrorMessage = TEXT("No level found for CE processing");
	bool bResult = false;

	// Try to execute the command on all level script actors
	for (TArray<ULevel*>::TConstIterator it = InWorld->GetLevels().CreateConstIterator(); it; ++it)
	{
		ULevel* CurrentLevel = *it;
		if (CurrentLevel)
		{

			if (CurrentLevel->GetLevelScriptActor())
			{
				ErrorMessage.Empty();

				// return true if at least one level handles the command
				bResult |= CurrentLevel->GetLevelScriptActor()->CallFunctionByNameWithArguments( Cmd, Ar, NULL, true );
			}
		}
	}

	if (!bResult)
	{
		ErrorMessage = FString::Printf(TEXT("CE command '%s' wasn't processed for levels from world '%s'."), Cmd, *InWorld->GetPathName());
	}

	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG( LogEngine, Error, TEXT( "%s" ), *ErrorMessage );
	}

	// the command was processed (resulted in executing the command or an error message) - no other spot handles "CE"
	return true;
}


bool UEngine::HandleDumpTicksCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Handle optional parameters, will dump all tick functions by default.
	bool bShowEnabled = true;
	bool bShowDisabled = true;
	if (FParse::Command( &Cmd, TEXT( "ENABLED" ) ))
	{
		bShowDisabled = false;
	}
	else if (FParse::Command( &Cmd, TEXT( "DISABLED" ) ))
	{
		bShowEnabled = false;
	}
	FTickTaskManagerInterface::Get().DumpAllTickFunctions( Ar, InWorld, bShowEnabled, bShowDisabled );
	return true;
}

bool UEngine::HandleGammaCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	DisplayGamma = (*Cmd != 0) ? FMath::Clamp<float>( FCString::Atof( *FParse::Token( Cmd, false ) ), 0.5f, 5.0f ) : 2.2f;
	return true;
}

bool UEngine::HandleShowLogCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Toggle display of console log window.
	if (GLogConsole)
	{
		GLogConsole->Show( !GLogConsole->IsShown() );
	}
	return 1;
}

#if STATS
bool UEngine::HandleDumpParticleMemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FParticleMemoryStatManager::DumpParticleMemoryStats( Ar );
	return true;
}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_HOT_RELOAD
bool UEngine::HandleHotReloadCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString Module( FParse::Token( Cmd, 0 ) );
	FString PackagePath( FString( TEXT( "/Script/" ) ) + Module );
	UPackage *Package = FindPackage( NULL, *PackagePath );
	if (!Package)
	{
		Ar.Logf( TEXT( "Could not HotReload '%s', package not found in memory" ), *Module );
	}
	else
	{
		Ar.Logf( TEXT( "HotReloading %s..." ), *Module );
		TArray< UPackage*> PackagesToRebind;
		PackagesToRebind.Add( Package );
		const bool bWaitForCompletion = true;	// Always wait when hotreload is initiated from the console
		IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>( "HotReload" );
		const ECompilationResult::Type CompilationResult = HotReloadSupport.RebindPackages( PackagesToRebind, TArray<FName>(), bWaitForCompletion, Ar );
	}
	return true;
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_HOT_RELOAD

#if !UE_BUILD_SHIPPING

static void DumpHelp(UWorld* InWorld)
{
	UE_LOG(LogEngine, Display, TEXT("Console Help:"));
	UE_LOG(LogEngine, Display, TEXT("============="));
	UE_LOG(LogEngine, Display, TEXT(" "));
	UE_LOG(LogEngine, Display, TEXT("A console variable is a engine wide key value pair. The key is a string usually starting with the subsystem prefix followed"));
	UE_LOG(LogEngine, Display, TEXT("by '.' e.g. r.BloomQuality. The value can be of different tpe (e.g. float, int, string). A console command has no state associated with"));
	UE_LOG(LogEngine, Display, TEXT("and gets executed immediately."));
	UE_LOG(LogEngine, Display, TEXT(" "));
	UE_LOG(LogEngine, Display, TEXT("Console variables can be put into ini files (e.g. ConsoleVariables.ini or BaseEngine.ini) with this syntax:"));
	UE_LOG(LogEngine, Display, TEXT("<Console variable> = <value>"));
	UE_LOG(LogEngine, Display, TEXT(" "));
	UE_LOG(LogEngine, Display, TEXT("DumpConsoleCommands         Lists all console variables and commands that are registered (Some are not registered)"));
	UE_LOG(LogEngine, Display, TEXT("<Console variable>          Get the console variable state"));
	UE_LOG(LogEngine, Display, TEXT("<Console variable> ?        Get the console variable help text"));
	UE_LOG(LogEngine, Display, TEXT("<Console variable> <value>  Set the console variable value"));
	UE_LOG(LogEngine, Display, TEXT("<Console command> [Params]  Execute the console command with optional parameters"));

	UE_LOG(LogEngine, Display, TEXT(" "));

	FString FilePath = FPaths::ProjectSavedDir() + TEXT("ConsoleHelp.html");

	UE_LOG(LogEngine, Display, TEXT("To browse console variables open this: '%s'"), *FilePath);
	UE_LOG(LogEngine, Display, TEXT(" "));

	ConsoleCommandLibrary_DumpLibraryHTML(InWorld, *GEngine, FilePath);

	// Notification in editor
#if WITH_EDITOR
	{
		const FText Message = NSLOCTEXT("UnrealEd", "ConsoleHelpExported", "ConsoleHelp.html was saved as");
		FNotificationInfo Info(Message);
		Info.bFireAndForget = true;
		Info.ExpireDuration = 5.0f;
		Info.bUseSuccessFailIcons = false;
		Info.bUseLargeFont = false;

		const FString HyperLinkText = FPaths::ConvertRelativePathToFull(FilePath);
		Info.Hyperlink = FSimpleDelegate::CreateStatic([](FString SourceFilePath) 
		{
			// open folder, you can choose the browser yourself
			FPlatformProcess::ExploreFolder(*(FPaths::GetPath(SourceFilePath)));
		}, HyperLinkText);
		Info.HyperlinkText = FText::FromString(HyperLinkText);

		FSlateNotificationManager::Get().AddNotification(Info);

		// Always try to open the help file on Windows (including in -game, etc...)
#if PLATFORM_WINDOWS
		const FString LaunchableURL = FString(TEXT("file://")) + HyperLinkText;
		FPlatformProcess::LaunchURL(*LaunchableURL, nullptr, nullptr);
#endif
	}
#endif// WITH_EDITOR
}
static FAutoConsoleCommandWithWorld GConsoleCommandHelp(
	TEXT("help"),
	TEXT("Outputs some helptext to the console and the log"),
	FConsoleCommandWithWorldDelegate::CreateStatic(DumpHelp)
	);

bool UEngine::HandleDumpConsoleCommandsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	Ar.Logf(TEXT("DumpConsoleCommands: %s*"), Cmd);
	Ar.Logf(TEXT(""));
	ConsoleCommandLibrary_DumpLibrary( InWorld, *GEngine, FString(Cmd) + TEXT("*"), Ar);
	return true;
}

bool UEngine::HandleDumpAvailableResolutionsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	UE_LOG(LogEngine, Log, TEXT("DumpAvailableResolutions"));

	FScreenResolutionArray ResArray;
	if (RHIGetAvailableResolutions(ResArray, false))
	{
		for (int32 ModeIndex = 0; ModeIndex < ResArray.Num(); ModeIndex++)
		{
			FScreenResolutionRHI& ScreenRes = ResArray[ModeIndex];
			UE_LOG(LogEngine, Log, TEXT("DefaultAdapter - %4d x %4d @ %d"), 
				ScreenRes.Width, ScreenRes.Height, ScreenRes.RefreshRate);
		}
	}
	else
	{
		UE_LOG(LogEngine, Log, TEXT("Failed to get available resolutions!"));
	}
	return true;
}
bool UEngine::HandleAnimSeqStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern void GatherAnimSequenceStats(FOutputDevice& Ar);
	GatherAnimSequenceStats( Ar );
	return true;
}

bool UEngine::HandleCountDisabledParticleItemsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	int32 ParticleSystemCount = 0;
	int32 EmitterCount = 0;
	int32 DisabledEmitterCount = 0;
	int32 CookedOutEmitterCount = 0;
	int32 LODLevelCount = 0;
	int32 DisabledLODLevelCount = 0;
	int32 ModuleCount = 0;
	int32 DisabledModuleCount = 0;
	TMap<FString, int32> ModuleMap;
	for (TObjectIterator<UParticleSystem> It; It; ++It)
	{
		ParticleSystemCount++;

		TArray<UParticleModule*> ProcessedModules;
		TArray<UParticleModule*> DisabledModules;

		UParticleSystem* PSys = *It;
		for (int32 EmitterIdx = 0; EmitterIdx < PSys->Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = PSys->Emitters[EmitterIdx];
			if (Emitter != NULL)
			{
				bool bDisabledEmitter = true;
				EmitterCount++;
				if (Emitter->bCookedOut == true)
				{
					CookedOutEmitterCount++;
				}
				for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
					if (LODLevel != NULL)
					{
						LODLevelCount++;
						if (LODLevel->bEnabled == false)
						{
							DisabledLODLevelCount++;
						}
						else
						{
							bDisabledEmitter = false;
						}
						for (int32 ModuleIdx = -3; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = NULL;
							switch (ModuleIdx)
							{
							case -3:	Module = LODLevel->RequiredModule;		break;
							case -2:	Module = LODLevel->SpawnModule;			break;
							case -1:	Module = LODLevel->TypeDataModule;		break;
							default:	Module = LODLevel->Modules[ModuleIdx];	break;
							}

							int32 DummyIdx;
							if ((Module != NULL) && (ProcessedModules.Find(Module, DummyIdx) == false))
							{
								ModuleCount++;
								ProcessedModules.AddUnique(Module);
								if (Module->bEnabled == false)
								{
									check(DisabledModules.Find(Module, DummyIdx) == false);
									DisabledModules.AddUnique(Module);
									DisabledModuleCount++;
								}

								FString ModuleName = Module->GetClass()->GetName();
								int32* ModuleCounter = ModuleMap.Find(ModuleName);
								if (ModuleCounter == NULL)
								{
									int32 TempInt = 0;
									ModuleMap.Add(ModuleName, TempInt);
									ModuleCounter = ModuleMap.Find(ModuleName);
								}
								check(ModuleCounter != NULL);
								*ModuleCounter = (*ModuleCounter + 1);
							}
						}
					}
				}

				if (bDisabledEmitter)
				{
					DisabledEmitterCount++;
				}
			}
		}
	}

	UE_LOG(LogEngine, Log, TEXT("%5d particle systems w/ %7d emitters (%5d disabled or %5.3f%% - %4d cookedout)"),
		ParticleSystemCount, EmitterCount, DisabledEmitterCount, (float)DisabledEmitterCount / (float)EmitterCount, CookedOutEmitterCount);
	UE_LOG(LogEngine, Log, TEXT("\t%8d lodlevels (%5d disabled or %5.3f%%)"),
		LODLevelCount, DisabledLODLevelCount, (float)DisabledLODLevelCount / (float)LODLevelCount);
	UE_LOG(LogEngine, Log, TEXT("\t\t%10d modules (%5d disabled or %5.3f%%)"),
		ModuleCount, DisabledModuleCount, (float)DisabledModuleCount / (float)ModuleCount);
	for (TMap<FString,int32>::TIterator It(ModuleMap); It; ++It)
	{
		FString ModuleName = It.Key();
		int32 ModuleCounter = It.Value();

		UE_LOG(LogEngine, Log, TEXT("\t\t\t%4d....%s"), ModuleCounter, *ModuleName);
	}

	return true;
}

// View the last N number of names added to the name table. Useful for tracking down name table bloat
bool UEngine::HandleViewnamesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	int32 NumNames = 0;
	if (FParse::Value(Cmd,TEXT("NUM="),NumNames))
	{
		for (int32 NameIndex = FMath::Max<int32>(FName::GetMaxNames() - NumNames, 0); NameIndex < FName::GetMaxNames(); NameIndex++)
		{
			Ar.Logf(TEXT("%d->%s"), NameIndex, *FName::SafeString(NameIndex));
		}
	}
	return true;
}

bool UEngine::HandleFreezeStreamingCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	ProcessToggleFreezeStreamingCommand(InWorld);
	return true;
}

bool UEngine::HandleFreezeAllCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld  )
{
	ProcessToggleFreezeCommand( InWorld );
	ProcessToggleFreezeStreamingCommand(InWorld);
	return true;
}

extern void ToggleFreezeFoliageCulling();

bool UEngine::HandleFreezeRenderingCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld  )
{
	ProcessToggleFreezeCommand( InWorld );

	ToggleFreezeFoliageCulling();

	return true;
}

bool UEngine::HandleShowSelectedLightmapCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GShowDebugSelectedLightmap = !GShowDebugSelectedLightmap;
	GConfig->SetBool(TEXT("DevOptions.Debug"), TEXT("ShowSelectedLightmap"), GShowDebugSelectedLightmap, GEngineIni);
	Ar.Logf( TEXT( "Showing the selected lightmap: %s" ), GShowDebugSelectedLightmap ? TEXT("true") : TEXT("false") );
	return true;
}

bool UEngine::HandleShaderComplexityCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FlagStr(FParse::Token(Cmd, 0));
	if( FlagStr.Len() > 0 )
	{
		if( FCString::Stricmp(*FlagStr,TEXT("MAX"))==0)
		{
			float NewMax = FCString::Atof(Cmd);
			if (NewMax > 0.0f)
			{
				GEngine->MaxPixelShaderAdditiveComplexityCount = NewMax;
			}
		}
		else
		{
			Ar.Logf( TEXT("Format is 'shadercomplexity [toggleadditive] [togglepixel] [max $int]"));
			return true;
		}

		float CurrentMax = GEngine->MaxPixelShaderAdditiveComplexityCount;

		Ar.Logf( TEXT("New ShaderComplexity Settings: Max = %f"), CurrentMax);
	} 
	else
	{
		Ar.Logf( TEXT("Format is 'shadercomplexity [max $int]"));
	}
	return true; 
}

bool UEngine::HandleProfileGPUHitchesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GTriggerGPUHitchProfile = !GTriggerGPUHitchProfile;
	if (GTriggerGPUHitchProfile)
	{
		Ar.Logf(TEXT("Profiling GPU hitches."));
	}
	else
	{
		Ar.Logf(TEXT("Stopped profiling GPU hitches."));
	}
	return true;
}

bool UEngine::HandleToggleRenderingThreadCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if(GIsThreadedRendering)
	{
		StopRenderingThread();
		GUseThreadedRendering = false;
	}
	else
	{
		GUseThreadedRendering = true;
		StartRenderingThread();
	}
	Ar.Logf( TEXT("RenderThread is now in %s threaded mode."), GUseThreadedRendering ? TEXT("multi") : TEXT("single"));
	return true;
}

bool UEngine::HandleToggleAsyncComputeCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (GDynamicRHI)
	{
		bool bWasAsyncCompute = GEnableAsyncCompute;
		bool bWasThreadedRendering = GIsThreadedRendering;
		if (bWasThreadedRendering)
		{
			StopRenderingThread();
		}
		
		GEnableAsyncCompute = !bWasAsyncCompute;

		if (GEnableAsyncCompute)
		{
			FRHICommandListExecutor::GetImmediateAsyncComputeCommandList().SetComputeContext(RHIGetDefaultAsyncComputeContext());
		}
		else
		{
			FRHICommandListExecutor::GetImmediateAsyncComputeCommandList().SetContext(RHIGetDefaultContext());
		}		

		if (bWasThreadedRendering)
		{
			StartRenderingThread();
		}
		Ar.Logf(TEXT("AsyncCompute is now %s."), GEnableAsyncCompute ? TEXT("active") : TEXT("inactive"));
	}	
	return true;
}


bool UEngine::HandleRecompileShadersCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	return RecompileShaders(Cmd, Ar);
}

bool UEngine::HandleRecompileGlobalShadersCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern void RecompileGlobalShaders();
	RecompileGlobalShaders();
	return 1;
}

bool UEngine::HandleDumpShaderStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FlagStr(FParse::Token(Cmd, 0));
	EShaderPlatform Platform = GMaxRHIShaderPlatform;
	if (FlagStr.Len() > 0)
	{
		Platform = ShaderFormatToLegacyShaderPlatform(FName(*FlagStr));
	}
	Ar.Logf(TEXT("Dumping shader stats for platform %s"), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
	// Dump info on all loaded shaders regardless of platform and frequency.
	DumpShaderStats( Platform, SF_NumFrequencies );
	return true;
}

bool UEngine::HandleDumpMaterialStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString FlagStr(FParse::Token(Cmd, 0));
	EShaderPlatform Platform = GMaxRHIShaderPlatform;
	if (FlagStr.Len() > 0)
	{
		Platform = ShaderFormatToLegacyShaderPlatform(FName(*FlagStr));
	}
	Ar.Logf(TEXT("Dumping material stats for platform %s"), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
	// Dump info on all loaded shaders regardless of platform and frequency.
	extern ENGINE_API void DumpMaterialStats(EShaderPlatform);
	DumpMaterialStats( Platform );
	return true;
}

bool UEngine::HandleProfileCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( FParse::Command(&Cmd,TEXT("GPU")) )
	{
		if (!GTriggerGPUHitchProfile)
		{
			GTriggerGPUProfile = true;
			Ar.Logf(TEXT("Profiling the next GPU frame"));
		}
		else
		{
			Ar.Logf(TEXT("Can't do a gpu profile during a hitch profile!"));
		}
		return true;
	}
	return false;
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if WITH_PROFILEGPU
bool UEngine::HandleProfileGPUCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd, TEXT("TRACE")))
	{
		FString Filename = CreateProfileDirectoryAndFilename(TEXT(""), TEXT(".rtt"));
		//FPaths::MakePlatformFilename(Filename);
		GGPUTraceFileName = Filename;
		Ar.Logf(TEXT("Tracing the next GPU frame"));
	}
	else
	{
		if (!GTriggerGPUHitchProfile)
		{
			GTriggerGPUProfile = true;
			Ar.Logf(TEXT("Profiling the next GPU frame"));
		}
		else
		{
			Ar.Logf(TEXT("Can't do a gpu profile during a hitch profile!"));
		}
	}

	return true;
}

#endif // WITH_PROFILEGPU

#if !UE_BUILD_SHIPPING
bool UEngine::HandleStartFPSChartCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// start the chart data capture
	const FString Label = FParse::Token(Cmd, 0);
	StartFPSChart( Label, /*bRecordPerFrameTimes=*/ true );
	return true;
}

bool UEngine::HandleStopFPSChartCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	// stop the chart data capture and log it
	const FString MapName = InWorld ? InWorld->GetMapName() : TEXT("None");
	StopFPSChart( MapName );
	return true;
}

bool UEngine::HandleDumpLevelScriptActorsCommand( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Dumps the object properties for all level script actors
	for( TArray<ULevel*>::TConstIterator  it = InWorld->GetLevels().CreateConstIterator(); it; ++it )
	{
		ULevel* CurrentLevel = *it;
		if( CurrentLevel && CurrentLevel->GetLevelScriptActor() )
		{
			AActor* LSActor = CurrentLevel->GetLevelScriptActor();
			if( LSActor )
			{
				UE_LOG(LogEngine, Log, TEXT("--- %s (%s) ---"), *LSActor->GetName(), *LSActor->GetOutermost()->GetName())
					for( TFieldIterator<UProperty> PropertyIt(LSActor->GetClass(), EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt )
					{
						UObjectPropertyBase* MyProperty = Cast<UObjectPropertyBase>(*PropertyIt);
						if( MyProperty )
						{
							UObject* PointedObject = NULL;
							PointedObject = MyProperty->GetObjectPropertyValue_InContainer(LSActor);

							if( PointedObject != NULL )
							{
								UObject* PointedOutermost = PointedObject->GetOutermost();
								UE_LOG(LogEngine, Log, TEXT("%s: %s (%s)"), *MyProperty->GetName(), *PointedObject->GetName(), *PointedOutermost->GetName());
							}

						}
					}
			}
		}
	}
	return true;
}

bool UEngine::HandleKismetEventCommand(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	FString const ObjectName = FParse::Token(Cmd, 0);
	if (ObjectName == TEXT("*"))
	{
		// Send the command to everything in the world we're dealing with...
		for (TObjectIterator<UObject> It; It; ++It)
		{
			UObject* const Obj = *It;
			UWorld const* const ObjWorld = Obj->GetWorld();
			if (ObjWorld == InWorld)
			{
				Obj->CallFunctionByNameWithArguments(Cmd, Ar, NULL, true);
			}
		}
	}
	else
	{
		UObject* ObjectToMatch = FindObject<UObject>(ANY_PACKAGE, *ObjectName);

		if (ObjectToMatch == NULL)
		{
			Ar.Logf(TEXT("Failed to find object named '%s'.  Specify a valid name or *"), *ObjectName);
		}
		else
		{
			ObjectToMatch->CallFunctionByNameWithArguments(Cmd, Ar, NULL, true);
		}
	}

	return true;
}

bool UEngine::HandleListTexturesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	const bool bShouldOnlyListStreaming = FParse::Command(&Cmd, TEXT("STREAMING"));
	const bool bShouldOnlyListNonStreaming = FParse::Command(&Cmd, TEXT("NONSTREAMING")) && !bShouldOnlyListStreaming;
	const bool bShouldOnlyListForced = FParse::Command(&Cmd, TEXT("FORCED")) && !bShouldOnlyListStreaming && !bShouldOnlyListNonStreaming;
	const bool bAlphaSort = FParse::Param( Cmd, TEXT("ALPHASORT") );
	const bool bCSV = FParse::Param( Cmd, TEXT("CSV") );

	Ar.Logf( TEXT("Listing %s textures."), bShouldOnlyListForced ? TEXT("forced") : bShouldOnlyListNonStreaming ? TEXT("non streaming") : bShouldOnlyListStreaming ? TEXT("streaming") : TEXT("all")  );

	// Find out how many times a texture is referenced by primitive components.
	TMap<UTexture2D*,int32> TextureToUsageMap;
	for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
	{
		UPrimitiveComponent* PrimitiveComponent = *It;

		// Use the existing texture streaming functionality to gather referenced textures. Worth noting
		// that GetStreamingTextureInfo doesn't check whether a texture is actually streamable or not
		// and is also implemented for skeletal meshes and such.
		FStreamingTextureLevelContext LevelContext(EMaterialQualityLevel::Num, PrimitiveComponent);
		TArray<FStreamingTexturePrimitiveInfo> StreamingTextures;
		PrimitiveComponent->GetStreamingTextureInfo( LevelContext, StreamingTextures );

		// Increase usage count for all referenced textures
		for( int32 TextureIndex=0; TextureIndex<StreamingTextures.Num(); TextureIndex++ )
		{
			UTexture2D* Texture = StreamingTextures[TextureIndex].Texture;
			if( Texture )
			{
				// Initializes UsageCount to 0 if texture is not found.
				int32 UsageCount = TextureToUsageMap.FindRef( Texture );
				TextureToUsageMap.Add(Texture, UsageCount+1);
			}
		}
	}

	// Collect textures.
	TArray<FSortedTexture> SortedTextures;
	for( TObjectIterator<UTexture2D> It; It; ++It )
	{
		UTexture2D*			Texture				= *It;
		int32				LODGroup			= Texture->LODGroup;
		int32				NumMips				= Texture->GetNumMips();
		int32				MaxResLODBias		 = NumMips - Texture->GetNumMipsAllowed(false);
		int32				MaxAllowedSizeX		= FMath::Max<int32>(Texture->GetSizeX() >> MaxResLODBias, 1);
		int32				MaxAllowedSizeY		= FMath::Max<int32>(Texture->GetSizeY() >> MaxResLODBias, 1);
		EPixelFormat		Format				= Texture->GetPixelFormat();
		int32				DroppedMips			= Texture->GetNumMips() - Texture->GetNumResidentMips();
		int32				CurSizeX			= FMath::Max<int32>(Texture->GetSizeX() >> DroppedMips, 1);
		int32				CurSizeY			= FMath::Max<int32>(Texture->GetSizeY() >> DroppedMips, 1);
		bool			bIsStreamingTexture		= Texture->GetStreamingIndex() != INDEX_NONE;
		int32				MaxAllowedSize		= Texture->CalcTextureMemorySizeEnum( TMC_AllMipsBiased );
		int32				CurrentSize			= Texture->CalcTextureMemorySizeEnum( TMC_ResidentMips );
		int32				UsageCount			= TextureToUsageMap.FindRef( Texture );
		bool				bIsForced			= Texture->ShouldMipLevelsBeForcedResident() && bIsStreamingTexture;

		if( (bShouldOnlyListStreaming && bIsStreamingTexture) ||	
			(bShouldOnlyListNonStreaming && !bIsStreamingTexture) ||
			(bShouldOnlyListForced && bIsForced) ||   
			(!bShouldOnlyListStreaming && !bShouldOnlyListNonStreaming && !bShouldOnlyListForced) )
		{
			new(SortedTextures) FSortedTexture( 
				MaxAllowedSizeX,
				MaxAllowedSizeY,
				Format,
				CurSizeX,
				CurSizeY,
				MaxResLODBias, 
				MaxAllowedSize,
				CurrentSize,
				Texture->GetPathName(), 
				LODGroup, 
				bIsStreamingTexture,
				UsageCount);
		}
	}

	// Sort textures by cost.
	SortedTextures.Sort( FCompareFSortedTexture( bAlphaSort ) );

	// Retrieve mapping from LOD group enum value to text representation.
	TArray<FString> TextureGroupNames = UTextureLODSettings::GetTextureGroupNames();

	TArray<uint64> TextureGroupCurrentSizes;
	TArray<uint64> TextureGroupMaxAllowedSizes;
	
	TArray<uint64> FormatCurrentSizes;
	TArray<uint64> FormatMaxAllowedSizes;

	TextureGroupCurrentSizes.AddZeroed(TextureGroupNames.Num());
	TextureGroupMaxAllowedSizes.AddZeroed(TextureGroupNames.Num());

	FormatCurrentSizes.AddZeroed(PF_MAX);
	FormatMaxAllowedSizes.AddZeroed(PF_MAX);

	// Display.
	int32 TotalMaxAllowedSize = 0;
	int32 TotalCurrentSize	= 0;

	if (bCSV)
	{
		Ar.Logf(TEXT(",Max Width,Max Height,Max Size (KB),Bias Authored,Current Width,Current Height,Current Size (KB),Format,LODGroup,Name,Streaming,Usage Count"));
	}
	else if (!FPlatformProperties::RequiresCookedData())
	{
		Ar.Logf(TEXT("MaxAllowedSize: Width x Height (Size in KB, Authored Bias), Current/InMem: Width x Height (Size in KB), Format, LODGroup, Name, Streaming, Usage Count"));
	}
	else
	{
		Ar.Logf(TEXT("Cooked/OnDisk: Width x Height (Size in KB, Authored Bias), Current/InMem: Width x Height (Size in KB), Format, LODGroup, Name, Streaming, Usage Count"));
	}

	for( int32 TextureIndex=0; TextureIndex<SortedTextures.Num(); TextureIndex++ )
	{
		const FSortedTexture& SortedTexture = SortedTextures[TextureIndex];
		const bool bValidTextureGroup = TextureGroupNames.IsValidIndex(SortedTexture.LODGroup);

		FString AuthoredBiasString(TEXT("?"));
		if (!FPlatformProperties::RequiresCookedData())
		{
			AuthoredBiasString.Empty();
			AuthoredBiasString.AppendInt(SortedTexture.LODBias);
		}

		Ar.Logf(bCSV ? TEXT(",%i, %i, %i, %s, %i, %i, %i, %s, %s, %s, %s, %i") : TEXT("%ix%i (%i KB, %s), %ix%i (%i KB), %s, %s, %s, %s, %i"),
			SortedTexture.MaxAllowedSizeX, SortedTexture.MaxAllowedSizeY, (SortedTexture.MaxAllowedSize + 512) / 1024, 
			*AuthoredBiasString,
			SortedTexture.CurSizeX, SortedTexture.CurSizeY, (SortedTexture.CurrentSize + 512) / 1024,
			GetPixelFormatString(SortedTexture.Format),
			bValidTextureGroup ? *TextureGroupNames[SortedTexture.LODGroup] : TEXT("INVALID"),
			*SortedTexture.Name,
			SortedTexture.bIsStreaming ? TEXT("YES") : TEXT("NO"),
			SortedTexture.UsageCount);

		if (bValidTextureGroup)
		{
			TextureGroupCurrentSizes[SortedTexture.LODGroup] += SortedTexture.CurrentSize;
			TextureGroupMaxAllowedSizes[SortedTexture.LODGroup] += SortedTexture.MaxAllowedSize;
		}

		if (SortedTexture.Format >= 0 && SortedTexture.Format < PF_MAX)
		{
			FormatCurrentSizes[SortedTexture.Format] += SortedTexture.CurrentSize;
			FormatMaxAllowedSizes[SortedTexture.Format] += SortedTexture.MaxAllowedSize;
		}

		TotalMaxAllowedSize	+= SortedTexture.MaxAllowedSize;
		TotalCurrentSize	+= SortedTexture.CurrentSize;
	}

	Ar.Logf(TEXT("Total size: InMem= %.2f MB  OnDisk= %.2f MB  Count=%d"), (double)TotalCurrentSize / 1024. / 1024., (double)TotalMaxAllowedSize / 1024. / 1024., SortedTextures.Num() );
	for (int32 i = 0; i < PF_MAX; ++i)
	{
		if (FormatCurrentSizes[i] > 0 || FormatMaxAllowedSizes[i] > 0)
		{
			Ar.Logf(TEXT("Total %s size: InMem= %.2f MB  OnDisk= %.2f MB "), GetPixelFormatString((EPixelFormat)i), (double)FormatCurrentSizes[i] / 1024. / 1024., (double)FormatMaxAllowedSizes[i] / 1024. / 1024.);
		}
	}

	for (int32 i = 0; i < TextureGroupCurrentSizes.Num(); ++i)
	{
		if (TextureGroupCurrentSizes[i] > 0 || TextureGroupMaxAllowedSizes[i] > 0)
		{
			Ar.Logf(TEXT("Total %s size: InMem= %.2f MB  OnDisk= %.2f MB "), *TextureGroupNames[i], (double)TextureGroupCurrentSizes[i] / 1024. / 1024., (double)TextureGroupMaxAllowedSizes[i] / 1024. / 1024.);
		}
	}
	return true;
}

bool UEngine::HandleRemoteTextureStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Address which sent the command.  We will send stats back to this address
	FString Addr(FParse::Token(Cmd, 0));
	// Port to send to
	FString Port(FParse::Token(Cmd, 0));

	// Make an IP address. 	// @TODO ONLINE - Revisit "send over network"
	//FIpAddr SrcAddr;
	//SrcAddr.Port = FCString::Atoi( *Port );
	//SrcAddr.Addr = FCString::Atoi( *Addr );

	// Gather stats.
	double LastTime = FApp::GetLastTime();

	UE_LOG(LogEngine, Log,  TEXT("Remote AssetsStats request received.") );

	TMap<UTexture2D*,int32> TextureToUsageMap;

	TArray<UMaterialInterface*> UsedMaterials;
	TArray<UTexture*> UsedTextures;

	// Find out how many times a texture is referenced by primitive components.
	for( TObjectIterator<UPrimitiveComponent> It; It; ++It )
	{
		UPrimitiveComponent* PrimitiveComponent = *It;

		UsedMaterials.Reset();
		// Get the used materials off the primitive component so we can find the textures
		PrimitiveComponent->GetUsedMaterials( UsedMaterials );
		for( int32 MatIndex = 0; MatIndex < UsedMaterials.Num(); ++MatIndex )
		{
			// Ensure we dont have any NULL elements.
			if( UsedMaterials[ MatIndex ] )
			{
				UsedTextures.Reset();
				UsedMaterials[MatIndex]->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, false, GMaxRHIFeatureLevel, true);

				// Increase usage count for all referenced textures
				for( int32 TextureIndex=0; TextureIndex<UsedTextures.Num(); TextureIndex++ )
				{
					UTexture2D* Texture = Cast<UTexture2D>( UsedTextures[TextureIndex] );
					if( Texture )
					{
						// Initializes UsageCount to 0 if texture is not found.
						int32 UsageCount = TextureToUsageMap.FindRef( Texture );
						TextureToUsageMap.Add(Texture, UsageCount+1);
					}
				}
			}
		}
	}

	for(TObjectIterator<UTexture> It; It; ++It)
	{
		UTexture* Texture = *It;
		FString FullyQualifiedPath = Texture->GetPathName();
		FString MaxDim = FString::Printf( TEXT( "%dx%d" ), (int32)Texture->GetSurfaceWidth(), (int32)Texture->GetSurfaceHeight() );

		uint32 GroupId = Texture->LODGroup;
		uint32 FullyLoadedInBytes = Texture->CalcTextureMemorySizeEnum( TMC_AllMips );
		uint32 CurrentInBytes = Texture->CalcTextureMemorySizeEnum( TMC_ResidentMips );
		FString TexType;	// e.g. "2D", "Cube", ""
		uint32 FormatId = 0;
		float LastTimeRendered = FLT_MAX;
		uint32 NumUses = 0;
		int32 LODBias = Texture->GetCachedLODBias();
		FTexture* Resource = Texture->Resource; 

		if(Resource)
		{
			LastTimeRendered = LastTime - Resource->LastRenderTime;
		}

		FString CurrentDim = TEXT("?");
		UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
		if(Texture2D)
		{
			FormatId = Texture2D->GetPixelFormat();
			TexType = TEXT("2D");
			NumUses = TextureToUsageMap.FindRef( Texture2D );

			// Calculate in game current dimensions 
			const int32 DroppedMips = Texture2D->GetNumMips() - Texture2D->GetNumResidentMips();
			CurrentDim = FString::Printf(TEXT("%dx%d"), Texture2D->GetSizeX() >> DroppedMips, Texture2D->GetSizeY() >> DroppedMips);
		}
		else
		{
			UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
			if(TextureCube)
			{
				FormatId = TextureCube->GetPixelFormat();
				TexType = TEXT("Cube");
				// Calculate in game current dimensions
				CurrentDim = FString::Printf(TEXT("%dx%d"), TextureCube->GetSizeX(), TextureCube->GetSizeY());
			}
		}

		float CurrentKB = CurrentInBytes / 1024.0f;
		float FullyLoadedKB = FullyLoadedInBytes / 1024.0f;

		// @TODO ONLINE - Revisit "send over network"
		// 10KB should be enough
		//FNboSerializeToBuffer PayloadWriter(10 * 1024);
		//PayloadWriter << FullyQualifiedPath << MaxDim << CurrentDim << TexType << FormatId << GroupId << FullyLoadedKB << CurrentKB << LastTimeRendered << NumUses << LODBias;	
	}
	return true;
}

bool UEngine::HandleListParticleSystemsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Switches;
	TArray<FString> Tokens;
	FCommandLine::Parse(Cmd, Tokens, Switches);

	const bool bAlphaSort = (Tokens.Find(TEXT("ALPHASORT")) != INDEX_NONE) || (Switches.Find(TEXT("ALPHASORT")) != INDEX_NONE);
	bool bDumpMesh = (Tokens.Find(TEXT("DUMPMESH")) != INDEX_NONE) || (Switches.Find(TEXT("DUMPMESH")) != INDEX_NONE);

	TArray<FSortedParticleSet> SortedSets;
	TMap<UObject *,int32> SortMap;

	FString Description;
	for( TObjectIterator<UParticleSystem> SystemIt; SystemIt; ++SystemIt )
	{			
		UParticleSystem* Tree = *SystemIt;
		Description = FString::Printf(TEXT("%s"), *Tree->GetPathName());
		FArchiveCountMem Count( Tree );
		int32 RootSize = Count.GetMax();

		SortedSets.Add(FSortedParticleSet(Description, RootSize, RootSize, 0, 0, 0, FResourceSizeEx(EResourceSizeMode::Inclusive), FResourceSizeEx(EResourceSizeMode::Exclusive)));
		SortMap.Add(Tree,SortedSets.Num() - 1);
	}

	for( TObjectIterator<UParticleModule> It; It; ++It )
	{
		UParticleModule* Module = *It;
		int32 *pIndex = SortMap.Find(Module->GetOuter());

		if (pIndex && SortedSets.IsValidIndex(*pIndex))
		{
			FSortedParticleSet &Set = SortedSets[*pIndex];
			FArchiveCountMem ModuleCount( Module );
			Set.ModuleSize += ModuleCount.GetMax();
			Set.Size += ModuleCount.GetMax();
		}
	}

	for( TObjectIterator<UParticleSystemComponent> It; It; ++It )
	{
		UParticleSystemComponent* Comp = *It;
		int32 *pIndex = SortMap.Find(Comp->Template);

		if (pIndex && SortedSets.IsValidIndex(*pIndex))
		{				
			FSortedParticleSet &Set = SortedSets[*pIndex];				
			FArchiveCountMem ComponentCount( Comp );
			Set.ComponentSize += ComponentCount.GetMax();

			// Save this for adding to the total
			FResourceSizeEx CompResSize = FResourceSizeEx(EResourceSizeMode::Inclusive);
			Comp->GetResourceSizeEx(CompResSize);

			Set.ComponentResourceSize += CompResSize;
			Comp->GetResourceSizeEx(Set.ComponentTrueResourceSize);

			Set.Size += ComponentCount.GetMax();
			Set.Size += CompResSize.GetTotalMemoryBytes();
			Set.ComponentCount++;

			UParticleSystem* Tree = Comp->Template;
			if (bDumpMesh && Tree != NULL)
			{
				for (int32 EmitterIdx = 0; (EmitterIdx < Tree->Emitters.Num()); EmitterIdx++)
				{
					UParticleEmitter* Emitter = Tree->Emitters[EmitterIdx];
					if (Emitter != NULL)
					{
						// Have to check each LOD level...
						if (Emitter->LODLevels.Num() > 0)
						{
							UParticleLODLevel* LODLevel = Emitter->LODLevels[0];
							if (LODLevel != NULL)
							{
								if (LODLevel->RequiredModule->bUseLocalSpace == true)
								{
									UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
									if (MeshTD != NULL)
									{
										int32 InstCount = 0;
										// MESH EMITTER
										if (EmitterIdx < Comp->EmitterInstances.Num())
										{
											FParticleEmitterInstance* Inst = Comp->EmitterInstances[EmitterIdx];
											if (Inst != NULL)
											{
												InstCount = Inst->ActiveParticles;
											}

											UE_LOG(LogEngine, Warning, TEXT("---> PSys w/ mesh emitters: %2d %4d %s %s "), EmitterIdx, InstCount, 
												Comp->SceneProxy ? TEXT("Y") : TEXT("N"),
												*(Tree->GetPathName()));
										}
									}
								}
							}
						}
					}
				}
			}

		}
	}

	// Sort anim sets by cost
	SortedSets.Sort( FCompareFSortedParticleSet( bAlphaSort ) );

	// Now print them out.
	Ar.Logf(TEXT("ParticleSystems:"));
	Ar.Logf(TEXT("Size,Name,PSysSize,ModuleSize,ComponentSize,ComponentCount,CompResSize,CompTrueResSize"));
	FSortedParticleSet TotalSet(TEXT("Total"));
	int32 TotalSize = 0;
	for(int32 i=0; i<SortedSets.Num(); i++)
	{
		FSortedParticleSet& SetInfo = SortedSets[i];
		TotalSize += SetInfo.Size;
		TotalSet += SetInfo;
		SetInfo.Dump(Ar);
	}
	TotalSet.Dump(Ar);
	return true;
}

bool UEngine::HandleListSpawnedActorsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if( InWorld )
	{
		const float	TimeSeconds		    = InWorld->GetTimeSeconds();

		// Create alphanumerically sorted list of actors in persistent level.
		TArray<AActor*> SortedActorList = InWorld->PersistentLevel->Actors;
		SortedActorList.Remove(NULL);
		SortedActorList.Sort();

		Ar.Logf(TEXT("Listing spawned actors in persistent level:"));
		Ar.Logf(TEXT("Total: %d" ), SortedActorList.Num());

		if ( GetNumGamePlayers(InWorld) )
		{
			// If have local player, give info on distance to player
			const FVector PlayerLocation = GetGamePlayers(InWorld)[0]->LastViewLocation;

			// Iterate over all non-static actors and log detailed information.
			Ar.Logf(TEXT("TimeUnseen,TimeAlive,Distance,Class,Name,Owner"));
			for( int32 ActorIndex=0; ActorIndex<SortedActorList.Num(); ActorIndex++ )
			{
				AActor* Actor = SortedActorList[ActorIndex];
				if( !Actor->IsNetStartupActor() )
				{
					// Calculate time actor has been alive for. Certain actors can be spawned before TimeSeconds is valid
					// so we manually reset them to the same time as TimeSeconds.
					float TimeAlive	= TimeSeconds - Actor->CreationTime;
					if( TimeAlive < 0 )
					{
						TimeAlive = TimeSeconds;
					}
					const float TimeUnseen = TimeSeconds - Actor->GetLastRenderTime();
					const float DistanceToPlayer = FVector::Dist( Actor->GetActorLocation(), PlayerLocation );
					Ar.Logf(TEXT("%6.2f,%6.2f,%8.0f,%s,%s,%s"),TimeUnseen,TimeAlive,DistanceToPlayer,*Actor->GetClass()->GetName(),*Actor->GetName(),*GetNameSafe(Actor->GetOwner()));
				}
			}
		}
		else
		{
			// Iterate over all non-static actors and log detailed information.
			Ar.Logf(TEXT("TimeAlive,Class,Name,Owner"));
			for( int32 ActorIndex=0; ActorIndex<SortedActorList.Num(); ActorIndex++ )
			{
				AActor* Actor = SortedActorList[ActorIndex];
				if( !Actor->IsNetStartupActor() )
				{
					// Calculate time actor has been alive for. Certain actors can be spawned before TimeSeconds is valid
					// so we manually reset them to the same time as TimeSeconds.
					float TimeAlive	= TimeSeconds - Actor->CreationTime;
					if( TimeAlive < 0 )
					{
						TimeAlive = TimeSeconds;
					}
					Ar.Logf(TEXT("%6.2f,%s,%s,%s"),TimeAlive,*Actor->GetClass()->GetName(),*Actor->GetName(),*GetNameSafe(Actor->GetOwner()));
				}
			}
		}
	}
	else
	{
		Ar.Logf(TEXT("LISTSPAWNEDACTORS failed."));
	}
	return true;
}

bool UEngine::HandleMemReportCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	// This will defer the report to the end of the frame so we can force a GC and get a real report with no gcable objects
	GEngine->DeferredCommands.Add(FString::Printf(TEXT("MemReportDeferred %s"), Cmd ));

	return true;
}

bool UEngine::HandleMemReportDeferredCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
#if ALLOW_DEBUG_FILES
	QUICK_SCOPE_CYCLE_COUNTER(HandleMemReportDeferredCommand);

	const bool bPerformSlowCommands = FParse::Param( Cmd, TEXT("FULL") );
	const bool bLogOutputToFile = !FParse::Param( Cmd, TEXT("LOG") );
	FString InFileName;
	FParse::Value(Cmd, TEXT("NAME="), InFileName);

	// Turn off as it makes diffing hard
	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

	// Flush rendering and do a GC
	FlushAsyncLoading();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS,true);
	FlushRenderingCommands();

	FOutputDevice* ReportAr = &Ar;
	FArchive* FileAr = nullptr;
	FOutputDeviceArchiveWrapper* FileArWrapper = nullptr;
	FString FilenameFull;
	
	if (bLogOutputToFile)
	{	
		const FString PathName = *(FPaths::ProfilingDir() + TEXT("MemReports/"));
		IFileManager::Get().MakeDirectory( *PathName );

		const FString Filename = CreateProfileFilename(InFileName, TEXT(".memreport"), true );
		FilenameFull = PathName + Filename;
	
		FileAr = IFileManager::Get().CreateDebugFileWriter(*FilenameFull);
		FileArWrapper = new FOutputDeviceArchiveWrapper(FileAr);
		ReportAr = FileArWrapper;

		UE_LOG(LogEngine, Log, TEXT("MemReportDeferred: saving to %s"), *FilenameFull);		
	}

	ReportAr->Logf(TEXT( "CommandLine Options: %s" ),  FCommandLine::Get() );
	ReportAr->Logf(TEXT("Time Since Boot: %.02f Seconds") LINE_TERMINATOR, FPlatformTime::Seconds() - GStartTime);

	// Run commands from the ini
	FConfigSection* CommandsToRun = GConfig->GetSectionPrivate(TEXT("MemReportCommands"), 0, 1, GEngineIni);

	if (CommandsToRun)
	{
		for (FConfigSectionMap::TIterator It(*CommandsToRun); It; ++It)
		{
			Exec( InWorld, *It.Value().GetValue(), *ReportAr );
			ReportAr->Logf( LINE_TERMINATOR );
		}
	}

	if (bPerformSlowCommands)
	{
		CommandsToRun = GConfig->GetSectionPrivate(TEXT("MemReportFullCommands"), 0, 1, GEngineIni);

		if (CommandsToRun)
		{
			for (FConfigSectionMap::TIterator It(*CommandsToRun); It; ++It)
			{
				Exec( InWorld, *It.Value().GetValue(), *ReportAr );
				ReportAr->Logf( LINE_TERMINATOR );
			}
		}
	}

	if (FileArWrapper != nullptr)
	{
		FileArWrapper->TearDown();
		delete FileArWrapper;
		delete FileAr;
	}
#endif

	return true;
}


bool UEngine::HandleParticleMeshUsageCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Mapping from static mesh to particle systems using it.
	TMultiMap<UStaticMesh*,UParticleSystem*> StaticMeshToParticleSystemMap;
	// Unique array of referenced static meshes, used for sorting and index into map.
	TArray<UStaticMesh*> UniqueReferencedMeshes;

	// Iterate over all mesh modules to find and keep track of mesh to system mappings.
	for( TObjectIterator<UParticleModuleTypeDataMesh> It; It; ++It )
	{
		UStaticMesh* StaticMesh = It->Mesh;
		if( StaticMesh )
		{
			// Find particle system in outer chain.
			UParticleSystem* ParticleSystem = NULL;
			UObject* Outer = It->GetOuter();
			while( Outer && !ParticleSystem )
			{
				ParticleSystem = Cast<UParticleSystem>(Outer);
				Outer = Outer->GetOuter();
			}

			// Add unique mapping from static mesh to particle system.
			if( ParticleSystem )
			{
				StaticMeshToParticleSystemMap.AddUnique( StaticMesh, ParticleSystem );
				UniqueReferencedMeshes.AddUnique( StaticMesh );
			}
		}
	}

	struct FCompareUStaticMeshByResourceSize
	{
		FORCEINLINE bool operator()( UStaticMesh& A, UStaticMesh& B ) const
		{
			const SIZE_T ResourceSizeA = A.GetResourceSizeBytes(EResourceSizeMode::Inclusive);
			const SIZE_T ResourceSizeB = B.GetResourceSizeBytes(EResourceSizeMode::Inclusive);
			return ResourceSizeB < ResourceSizeA;
		}
	};

	// Sort by resource size.
	UniqueReferencedMeshes.Sort( FCompareUStaticMeshByResourceSize() );

	// Calculate total size for summary.
	int32 TotalSize = 0;
	for( int32 StaticMeshIndex=0; StaticMeshIndex<UniqueReferencedMeshes.Num(); StaticMeshIndex++ )
	{
		UStaticMesh* StaticMesh	= UniqueReferencedMeshes[StaticMeshIndex];
		const SIZE_T StaticMeshResourceSize = StaticMesh->GetResourceSizeBytes(EResourceSizeMode::Inclusive);
		TotalSize += StaticMeshResourceSize;
	}

	// Log sorted summary.
	Ar.Logf(TEXT("%5i KByte of static meshes referenced by particle systems:"),TotalSize / 1024);
	for( int32 StaticMeshIndex=0; StaticMeshIndex<UniqueReferencedMeshes.Num(); StaticMeshIndex++ )
	{
		UStaticMesh* StaticMesh	= UniqueReferencedMeshes[StaticMeshIndex];

		// Find all particle systems using this static mesh.
		TArray<UParticleSystem*> ParticleSystems;
		StaticMeshToParticleSystemMap.MultiFind( StaticMesh, ParticleSystems );

		const SIZE_T StaticMeshResourceSize = StaticMesh->GetResourceSizeBytes(EResourceSizeMode::Inclusive);

		// Log meshes including resource size and referencing particle systems.
		Ar.Logf(TEXT("%5i KByte  %s"), StaticMeshResourceSize / 1024, *StaticMesh->GetFullName());
		for( int32 ParticleSystemIndex=0; ParticleSystemIndex<ParticleSystems.Num(); ParticleSystemIndex++ )
		{
			UParticleSystem* ParticleSystem = ParticleSystems[ParticleSystemIndex];
			Ar.Logf(TEXT("             %s"),*ParticleSystem->GetFullName());
		}
	}

	return true;
}

class ParticleSystemUsage
{
public:
	UParticleSystem* Template;
	int32	Count;
	int32	ActiveTotal;
	int32	MaxActiveTotal;
	// Reported whether the emitters are instanced or not...
	int32	StoredMaxActiveTotal;

	TArray<int32>		EmitterActiveTotal;
	TArray<int32>		EmitterMaxActiveTotal;
	// Reported whether the emitters are instanced or not...
	TArray<int32>		EmitterStoredMaxActiveTotal;

	ParticleSystemUsage() :
		Template(NULL),
		Count(0),
		ActiveTotal(0),
		MaxActiveTotal(0),
		StoredMaxActiveTotal(0)
	{
	}

	~ParticleSystemUsage()
	{
	}
};

bool UEngine::HandleDumpParticleCountsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TMap<UParticleSystem*, ParticleSystemUsage> UsageMap;

	bool bTrackUsage = FParse::Command(&Cmd, TEXT("USAGE"));
	bool bTrackUsageOnly = FParse::Command(&Cmd, TEXT("USAGEONLY"));
	for( TObjectIterator<UObject> It; It; ++It )
	{
		UParticleSystemComponent* PSysComp = Cast<UParticleSystemComponent>(*It);
		if (PSysComp)
		{
			ParticleSystemUsage* Usage = NULL;

			if (bTrackUsageOnly == false)
			{
				Ar.Logf( TEXT("ParticleSystemComponent %s"), *(PSysComp->GetName()));
			}

			UParticleSystem* PSysTemplate = PSysComp->Template;
			if (PSysTemplate != NULL)
			{
				if (bTrackUsage || bTrackUsageOnly)
				{
					ParticleSystemUsage* pUsage = UsageMap.Find(PSysTemplate);
					if (pUsage == NULL)
					{
						ParticleSystemUsage TempUsage;
						TempUsage.Template = PSysTemplate;
						TempUsage.Count = 1;

						UsageMap.Add(PSysTemplate, TempUsage);
						Usage = UsageMap.Find(PSysTemplate);
						check(Usage);
					}					
					else
					{
						Usage = pUsage;
						Usage->Count++;
					}
				}
				if (bTrackUsageOnly == false)
				{
					Ar.Logf( TEXT("\tTemplate         : %s"), *(PSysTemplate->GetPathName()));
				}
			}
			else
			{
				if (bTrackUsageOnly == false)
				{
					Ar.Logf( TEXT("\tTemplate         : %s"), TEXT("NULL"));
				}
			}

			// Dump each emitter
			int32 TotalActiveCount = 0;
			if (bTrackUsageOnly == false)
			{
				Ar.Logf( TEXT("\tEmitterCount     : %d"), PSysComp->EmitterInstances.Num());
			}

			if (PSysComp->EmitterInstances.Num() > 0)
			{
				for (int32 EmitterIndex = 0; EmitterIndex < PSysComp->EmitterInstances.Num(); EmitterIndex++)
				{
					FParticleEmitterInstance* EmitInst = PSysComp->EmitterInstances[EmitterIndex];
					if (EmitInst)
					{
						UParticleLODLevel* LODLevel = EmitInst->SpriteTemplate ? EmitInst->SpriteTemplate->LODLevels[0] : NULL;
						if (bTrackUsageOnly == false)
						{
							Ar.Logf( TEXT("\t\tEmitter %2d:\tActive = %4d\tMaxActive = %4d"), 
								EmitterIndex, EmitInst->ActiveParticles, EmitInst->MaxActiveParticles);
						}
						TotalActiveCount += EmitInst->MaxActiveParticles;
						if (bTrackUsage || bTrackUsageOnly)
						{
							check(Usage);
							Usage->ActiveTotal += EmitInst->ActiveParticles;
							Usage->MaxActiveTotal += EmitInst->MaxActiveParticles;
							Usage->StoredMaxActiveTotal += EmitInst->MaxActiveParticles;
							if (Usage->EmitterActiveTotal.Num() <= EmitterIndex)
							{
								int32 CheckIndex;
								CheckIndex = Usage->EmitterActiveTotal.AddZeroed(1);
								check(CheckIndex == EmitterIndex);
								CheckIndex = Usage->EmitterMaxActiveTotal.AddZeroed(1);
								check(CheckIndex == EmitterIndex);
								CheckIndex = Usage->EmitterStoredMaxActiveTotal.AddZeroed(1);
								check(CheckIndex == EmitterIndex);
							}
							Usage->EmitterActiveTotal[EmitterIndex] = Usage->EmitterActiveTotal[EmitterIndex] + EmitInst->ActiveParticles;
							Usage->EmitterMaxActiveTotal[EmitterIndex] = Usage->EmitterMaxActiveTotal[EmitterIndex] + EmitInst->MaxActiveParticles;
							Usage->EmitterStoredMaxActiveTotal[EmitterIndex] = Usage->EmitterStoredMaxActiveTotal[EmitterIndex] + EmitInst->MaxActiveParticles;
						}
					}
					else
					{
						if (bTrackUsageOnly == false)
						{
							Ar.Logf( TEXT("\t\tEmitter %2d:\tActive = %4d\tMaxActive = %4d"), EmitterIndex, 0, 0);
						}
					}
				}
			}
			else
				if (PSysTemplate != NULL)
				{
					for (int32 EmitterIndex = 0; EmitterIndex < PSysTemplate->Emitters.Num(); EmitterIndex++)
					{
						UParticleEmitter* Emitter = PSysTemplate->Emitters[EmitterIndex];
						if (Emitter)
						{
							int32 MaxActive = 0;

							for (int32 LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
							{
								UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIndex];
								if (LODLevel)
								{
									if (LODLevel->PeakActiveParticles > MaxActive)
									{
										MaxActive = LODLevel->PeakActiveParticles;
									}
								}
							}

							if (bTrackUsage || bTrackUsageOnly)
							{
								check(Usage);
								Usage->StoredMaxActiveTotal += MaxActive;
								if (Usage->EmitterStoredMaxActiveTotal.Num() <= EmitterIndex)
								{
									int32 CheckIndex;
									CheckIndex = Usage->EmitterActiveTotal.AddZeroed(1);
									check(CheckIndex == EmitterIndex);
									CheckIndex = Usage->EmitterMaxActiveTotal.AddZeroed(1);
									check(CheckIndex == EmitterIndex);
									CheckIndex = Usage->EmitterStoredMaxActiveTotal.AddZeroed(1);
									check(CheckIndex == EmitterIndex);
								}
								// Don't update the non-stored entries...
								Usage->EmitterStoredMaxActiveTotal[EmitterIndex] = Usage->EmitterStoredMaxActiveTotal[EmitterIndex] + MaxActive;
							}
						}
					}
				}
				if (bTrackUsageOnly == false)
				{
					Ar.Logf( TEXT("\tTotalActiveCount : %d"), TotalActiveCount);
				}
		}
	}

	if (bTrackUsage || bTrackUsageOnly)
	{
		Ar.Logf( TEXT("PARTICLE USAGE DUMP:"));
		for (TMap<UParticleSystem*, ParticleSystemUsage>::TIterator It(UsageMap); It; ++It)
		{
			ParticleSystemUsage& Usage = It.Value();
			UParticleSystem* Template = Usage.Template;
			check(Template);

			Ar.Logf( TEXT("\tParticleSystem..%s"), *(Usage.Template->GetPathName()));
			Ar.Logf( TEXT("\t\tCount.....................%d"), Usage.Count);
			Ar.Logf( TEXT("\t\tActiveTotal...............%5d"), Usage.ActiveTotal);
			Ar.Logf( TEXT("\t\tMaxActiveTotal............%5d (%4d per instance)"), Usage.MaxActiveTotal, (Usage.MaxActiveTotal / Usage.Count));
			Ar.Logf( TEXT("\t\tPotentialMaxActiveTotal...%5d (%4d per instance)"), Usage.StoredMaxActiveTotal, (Usage.StoredMaxActiveTotal / Usage.Count));
			Ar.Logf( TEXT("\t\tEmitters..................%d"), Usage.EmitterActiveTotal.Num());
			check(Usage.EmitterActiveTotal.Num() == Usage.EmitterMaxActiveTotal.Num());
			for (int32 EmitterIndex = 0; EmitterIndex < Usage.EmitterActiveTotal.Num(); EmitterIndex++)
			{
				int32 EActiveTotal = Usage.EmitterActiveTotal[EmitterIndex];
				int32 EMaxActiveTotal = Usage.EmitterMaxActiveTotal[EmitterIndex];
				int32 EStoredMaxActiveTotal = Usage.EmitterStoredMaxActiveTotal[EmitterIndex];
				Ar.Logf( TEXT("\t\t\tEmitter %2d - AT = %5d, MT = %5d (%4d per emitter), Potential MT = %5d (%4d per emitter)"),
					EmitterIndex, EActiveTotal,
					EMaxActiveTotal, (EMaxActiveTotal / Usage.Count),
					EStoredMaxActiveTotal, (EStoredMaxActiveTotal / Usage.Count)
					);
			}
		}
	}
	return true;
}


bool UEngine::HandleListLoadedPackagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

	struct FPackageInfo
	{
		FString Name;
		float LoadTime;
		UClass* AssetType;

		FPackageInfo(UPackage* InPackage)
		{
			Name = InPackage->GetPathName();
			LoadTime = InPackage->GetLoadTime();
			AssetType = nullptr;
		}
	};

	TArray<FPackageInfo> Packages;

	TArray<UObject*> ObjectsInPackageTemp;

	for (TObjectIterator<UPackage> It; It; ++It)
	{
		UPackage* Package = *It;

		const bool bIsARootPackage = Package->GetOuter() == nullptr;

		if (bIsARootPackage == true)
		{
			const int32 NewIndex = Packages.Emplace(Package);

			// Determine the contained asset type
			ObjectsInPackageTemp.Reset();
			GetObjectsWithOuter(Package, /*out*/ ObjectsInPackageTemp, /*bIncludeNestedObjects=*/ false);

			UClass* AssetType = nullptr;
			for (UObject* Object : ObjectsInPackageTemp)
			{
				if (!Object->IsA(UMetaData::StaticClass()) && !Object->IsA(UClass::StaticClass()) && !Object->HasAnyFlags(RF_ClassDefaultObject))
				{
					AssetType = Object->GetClass();
					break;
				}
			}

			Packages[NewIndex].AssetType = AssetType;
		}
	}

	// Sort by name
	Packages.Sort([](const FPackageInfo& A, const FPackageInfo& B) { return A.Name < B.Name; });

	Ar.Logf(TEXT("List of all loaded packages"));
	Ar.Logf(TEXT("Name,Type,LoadTime"), Packages.Num());
	for (const FPackageInfo& Info : Packages)
	{
		Ar.Logf(TEXT("%s,%s,%f"), *Info.Name, (Info.AssetType != nullptr) ? *Info.AssetType->GetName() : TEXT("unknown"), Info.LoadTime);
	}

	Ar.Logf( TEXT( "Total Number Of Packages Loaded: %i " ), Packages.Num() );

	return true;
}

bool UEngine::HandleMemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	const FString Token = FParse::Token( Cmd, 0 );
	const bool bDetailed = ( Token == TEXT("DETAILED") || Token == TEXT("STAT"));
	const bool bReport = ( Token == TEXT("FROMREPORT") );

	if (!bReport)
	{
		// Mem report is called 
		FlushAsyncLoading();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS,true);
		FlushRenderingCommands();
	}

#if !NO_LOGGING
	const FName CategoryName(LogMemory.GetCategoryName());
#else
	const FName CategoryName(TEXT("LogMemory"));
#endif
	FPlatformMemory::DumpStats( Ar );
	Ar.CategorizedLogf( CategoryName, ELogVerbosity::Log, TEXT("") );
	GMalloc->DumpAllocatorStats( Ar );

	if( bDetailed || bReport)
	{
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Memory Stats:") );
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("FMemStack (gamethread) current size = %.2f MB"), FMemStack::Get().GetByteCount() / (1024.0f * 1024.0f));
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("FPageAllocator (all threads) allocation size [used/ unused] = [%.2f / %.2f] MB"), (FPageAllocator::BytesUsed()) / (1024.0f * 1024.0f), (FPageAllocator::BytesFree()) / (1024.0f * 1024.0f));
		Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Nametable memory usage = %.2f MB"), FName::GetNameTableMemorySize() / (1024.0f * 1024.0f));

		FAssetRegistryModule* AssetRegistryModule = FModuleManager::LoadModulePtr<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		if (AssetRegistryModule)
		{
			Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("AssetRegistry memory usage = %.2f MB"), AssetRegistryModule->Get().GetAllocatedSize() / (1024.0f * 1024.0f));
		}

#if STATS
		TArray<FStatMessage> Stats;
		GetPermanentStats(Stats);

		FName NAME_STATGROUP_SceneMemory( FStatGroup_STATGROUP_SceneMemory::GetGroupName() );
		FName NAME_STATGROUP_Memory(FStatGroup_STATGROUP_Memory::GetGroupName());
		FName NAME_STATGROUP_TextureGroup("STATGROUP_TextureGroup");
		FName NAME_STATGROUP_RHI(FStatGroup_STATGROUP_RHI::GetGroupName());

		for (int32 Index = 0; Index < Stats.Num(); Index++)
		{
			FStatMessage const& Meta = Stats[Index];
			FName LastGroup = Meta.NameAndInfo.GetGroupName();
			if ((LastGroup == NAME_STATGROUP_SceneMemory || LastGroup == NAME_STATGROUP_Memory || LastGroup == NAME_STATGROUP_TextureGroup || LastGroup == NAME_STATGROUP_RHI)  && Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
			{
				Ar.CategorizedLogf( CategoryName, ELogVerbosity::Log, TEXT("%s"), *FStatsUtils::DebugPrint(Meta));
			}
		}
#endif
	}

	return true;
}

bool UEngine::HandleDebugCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd, TEXT("RESETLOADERS")))
	{
		ResetLoaders( NULL );
		return true;
	}

	// Handle "DEBUG CRASH" etc. 
	return PerformError(Cmd, Ar);
}


bool UEngine::HandleMergeMeshCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FString CmdCopy = Cmd;
	TArray<FString> Tokens;
	while (CmdCopy.Len() > 0)
	{
		const TCHAR* LocalCmd = *CmdCopy;
		FString Token = FParse::Token( LocalCmd, true );
		Tokens.Add( Token );
		CmdCopy = CmdCopy.Right( CmdCopy.Len() - Token.Len() - 1 );
	}

	// array of source meshes that will be merged
	TArray<USkeletalMesh*> SourceMeshList;

	if (Tokens.Num() >= 2)
	{
		for (int32 I = 0; I<Tokens.Num(); ++I)
		{
			USkeletalMesh * SrcMesh = LoadObject<USkeletalMesh>( NULL, *Tokens[I], NULL, LOAD_None, NULL );
			if (SrcMesh)
			{
				SourceMeshList.Add( SrcMesh );
			}
		}
	}

	// find player controller skeletalmesh
	APawn * PlayerPawn = NULL;
	USkeletalMesh * PlayerMesh = NULL;
	for (FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (PlayerController->GetCharacter() != NULL && PlayerController->GetCharacter()->GetMesh())
		{
			PlayerPawn = PlayerController->GetCharacter();
			PlayerMesh = PlayerController->GetCharacter()->GetMesh()->SkeletalMesh;
			break;
		}
	}

	if (PlayerMesh)
	{
		if (SourceMeshList.Num() == 0)
		{
			SourceMeshList.Add( PlayerMesh );
			SourceMeshList.Add( PlayerMesh );
		}
	}
	else
	{
		// we don't have a pawn (because we couldn't find a mesh), use any pawn as a spawn point
		for (FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			if (PlayerController->GetPawn() != NULL)
			{
				PlayerPawn = PlayerController->GetPawn();
				break;
			}
		}
	}

	if (PlayerPawn && SourceMeshList.Num() >= 2)
	{
		// create the composite mesh
		USkeletalMesh* CompositeMesh = NewObject<USkeletalMesh>( GetTransientPackage(), NAME_None, RF_Transient );

		TArray<FSkelMeshMergeSectionMapping> InForceSectionMapping;
		// create an instance of the FSkeletalMeshMerge utility
		FSkeletalMeshMerge MeshMergeUtil( CompositeMesh, SourceMeshList, InForceSectionMapping, 0 );

		// merge the source meshes into the composite mesh
		if (!MeshMergeUtil.DoMerge())
		{
			// handle errors
			// ...
			UE_LOG( LogEngine, Log, TEXT( "DoMerge Error: Merge Mesh Test Failed" ) );
			return true;
		}

		FVector SpawnLocation = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector()*50.f;

		// set the new composite mesh in the existing skeletal mesh component
		ASkeletalMeshActor* const SMA = PlayerPawn->GetWorld()->SpawnActor<ASkeletalMeshActor>( SpawnLocation, PlayerPawn->GetActorRotation()*-1 );
		if (SMA)
		{
			SMA->GetSkeletalMeshComponent()->SetSkeletalMesh( CompositeMesh );
		}
	}

	return true;
}

bool UEngine::HandleContentComparisonCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(Cmd, Tokens, Switches);
	if (Tokens.Num() > 0)
	{
		// The first token MUST be the base class name of interest
		FString BaseClassName = Tokens[0];
		TArray<FString> BaseClassesToIgnore;
		int32 Depth = 1;
		for (int32 TokenIdx = 1; TokenIdx < Tokens.Num(); TokenIdx++)
		{
			FString Token = Tokens[TokenIdx];
			FString TempString;
			if (FParse::Value(*Token, TEXT("DEPTH="), TempString))
			{
				Depth = FCString::Atoi(*TempString);
			}
			else
			{
				BaseClassesToIgnore.Add(Token);
				UE_LOG(LogEngine, Log, TEXT("Added ignored base class: %s"), *Token);
			}
		}

		UE_LOG(LogEngine, Log, TEXT("Calling CompareClasses w/ Depth of %d on %s"), Depth, *BaseClassName);
		UE_LOG(LogEngine, Log, TEXT("Ignoring base classes:"));
		for (int32 DumpIdx = 0; DumpIdx < BaseClassesToIgnore.Num(); DumpIdx++)
		{
			UE_LOG(LogEngine, Log, TEXT("\t%s"), *(BaseClassesToIgnore[DumpIdx]));
		}
		FContentComparisonHelper ContentComparisonHelper;
		ContentComparisonHelper.CompareClasses(BaseClassName, BaseClassesToIgnore, Depth);
	}
	return true;
}

bool UEngine::HandleTogglegtPsysLODCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern bool GbEnableGameThreadLODCalculation;
		GbEnableGameThreadLODCalculation = !GbEnableGameThreadLODCalculation;
		UE_LOG(LogEngine, Warning, TEXT("Particle LOD determination is now on the %s thread!"),
			GbEnableGameThreadLODCalculation ? TEXT("GAME") : TEXT("RENDER"));
	return true;
}

struct FHierarchyNode
{
	UObject* This;
	UObject* Parent;
	TSet<UObject*> Children;
	TSet<UObject*> Items;
	int64 Inc;
	int64 Exc;
	int32 IncCount;
	int32 ExcCount;
	FHierarchyNode()
		: This(NULL)
		, Parent(NULL)
		, Inc(-1)
		, Exc(-1)
		, IncCount(-1)
		, ExcCount(-1)
	{
	}
	bool operator< (FHierarchyNode const& Other) const
	{
		return Inc > Other.Inc;
	}

	bool IsLeaf()
	{
		return Children.Num() + Items.Num() == 0;
	}
};

struct FHierarchy
{
	int64 Limit;
	TMap<UObject*, FHierarchyNode> Nodes;

	FHierarchy(int32 InLimit)
		: Limit(InLimit)
	{
	}
	FHierarchyNode& AddFlat(UObject* This)
	{
		FHierarchyNode& Node = Nodes.FindOrAdd(This);
		if (!Node.This && This)
		{
			Node.This = This;
			Node.Parent = NULL;
			AddFlat(NULL).Children.Add(This);
		}
		return Node;
	}
	FHierarchyNode& AddOuter(UObject* This)
	{
		FHierarchyNode& Node = Nodes.FindOrAdd(This);
		if (!Node.This && This)
		{
			Node.This = This;
			Node.Parent = This->GetOuter();
			AddOuter(Node.Parent).Children.Add(This);
		}
		return Node;
	}
	FHierarchyNode& AddClass(UClass* This)
	{
		FHierarchyNode& Node = Nodes.FindOrAdd(This);
		if (!Node.This && This)
		{
			Node.This = This;
			Node.Parent = This->GetSuperClass();
			AddClass(This->GetSuperClass()).Children.Add(This);
		}
		return Node;
	}
	void AddClassInstance(UObject* This)
	{
		if (!This->IsA(UClass::StaticClass()))
		{
			AddClass(This->GetClass()).Items.Add(This);
			FHierarchyNode& Node = Nodes.FindOrAdd(This);
			if (!Node.This)
			{
				Node.This = This;
				Node.Parent = This->GetClass();
			}
		}
		else
		{
			AddClass((UClass*)This);
		}
	}
	FHierarchyNode& Compute(UObject* This, TMap<UObject*, FSubItem> const& Objects, bool bCountItems)
	{
		FHierarchyNode& Node = Nodes.FindChecked(This);
		if (Node.Inc < 0)
		{
			Node.Exc = 0;
			Node.ExcCount = 1;
			if (This)
			{
				FSubItem const& Item = Objects.FindChecked(This);
				Node.Exc += Item.Max;
				Node.Exc += Item.TrueResourceSize.GetTotalMemoryBytes();
				if (bCountItems)
				{
					Node.ExcCount += Node.Items.Num();
				}
				else
				{
					Node.ExcCount += Node.Children.Num();
				}
			}
			Node.Inc = Node.Exc;
			Node.IncCount = Node.ExcCount;
			for (TSet<UObject*>::TConstIterator It(Node.Children); It; ++It)
			{
				FHierarchyNode& Child = Compute(*It, Objects, bCountItems);
				Node.Inc += Child.Inc;
				if (!bCountItems)
				{
					Node.IncCount += Child.IncCount;
				}
			}
			for (TSet<UObject*>::TConstIterator It(Node.Items); It; ++It)
			{
				FHierarchyNode& Child = Compute(*It, Objects, bCountItems);
				Node.Inc += Child.Inc;
				if (bCountItems)
				{
					Node.IncCount += Child.IncCount;
				}
			}
		}
		return Node;
	}
	void SortSet(TSet<UObject*> const& In, TArray<FHierarchyNode>& Out)
	{
		Out.Empty(In.Num());
		for (TSet<UObject*>::TConstIterator It(In); It; ++It)
		{
			Out.Add(Nodes.FindChecked(*It));
		}
		Out.Sort();
	}
	static FString Size(uint64 Mem)
	{
		if (Mem / 1024 < 10000)
		{
			return FString::Printf(TEXT("%4lldK"), Mem / 1024);
		}
		if (Mem / (1024*1024) < 10000)
		{
			return FString::Printf(TEXT("%4lldM"), Mem / (1024*1024));
		}
		return FString::Printf(TEXT("%4lldG"), Mem / (1024*1024*1024));
	}
	void LogSet(TSet<UObject*> const& In, UClass* ClassToCheck, bool bCntItems, int Indent)
	{
		TArray<FHierarchyNode> Children;
		SortSet(In, Children);
		int32 Index = 0;
		for (; Index < Children.Num(); Index++)
		{
			UObject* Child = Children[Index].This;
			// Only makes sense for flat hierarchy.
			const bool bIsClassToCheck = Child->IsA( ClassToCheck );
			if( !bIsClassToCheck )
			{
				continue;
			}

			if( !Log( Child, ClassToCheck, bCntItems, Indent + 1, Index + 1 < Children.Num() ) )
			{
				break;
			}
		}
		if (Index < Children.Num())
		{
			int32 NumExtra = 0;
			FHierarchyNode Extra;
			Extra.Exc = 0;
			Extra.Inc = 0;
			Extra.ExcCount = 0;
			Extra.IncCount = 0;
			for (; Index < Children.Num(); Index++)
			{
				Extra.Exc += Children[Index].Exc;
				Extra.Inc += Children[Index].Inc;
				Extra.ExcCount += Children[Index].ExcCount;
				Extra.IncCount += Children[Index].IncCount;
				NumExtra++;
			}
			FString Line = FString::Printf(TEXT("%s        %5d %s (%d)"), *Size(Extra.Inc), Extra.IncCount, TEXT("More"), NumExtra);
			UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * (Indent + 1)), *Line);
		}
	}
	bool Log(UObject* This, UClass* ClassToCheck, bool bCountItems, int Indent = 0, bool bAllowCull = true)
	{
		FHierarchyNode& Node = Nodes.FindChecked(This);
		if (bAllowCull && Node.Inc < Limit && Node.Exc < Limit)
		{
			return false;
		}
		FString Line;
		if (Node.IsLeaf())
		{
			Line = FString::Printf(TEXT("%s        %5d %s"), *Size(Node.Inc), Node.IncCount, Node.This ? *Node.This->GetFullName() : TEXT("Root"));
			UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * Indent), *Line);
		}
		else
		{
			Line = FString::Printf(TEXT("%s %sx %5d %s"), *Size(Node.Inc), *Size(Node.Exc), Node.IncCount, Node.This ? *Node.This->GetFullName() : TEXT("Root"));
			UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * Indent), *Line);
			if (bCountItems && Node.Children.Num())
			{
				UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * (Indent + 1)), TEXT("Child Classes"));
			}
			LogSet(Node.Children, ClassToCheck, bCountItems, Indent + 2);

			if (bCountItems && Node.Items.Num())
			{
				UE_LOG(LogEngine, Log, TEXT("%s%s") , FCString::Spc(2 * (Indent + 1)), TEXT("Instances"));
			}
			LogSet(Node.Items, ClassToCheck, bCountItems, Indent);
		}

		return true;
	}
};

// #TODO Move to ObjectCommads.cpp or ObjectExec.cpp
bool UEngine::HandleObjCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( FParse::Command(&Cmd,TEXT("GARBAGE")) || FParse::Command(&Cmd,TEXT("GC")) )
	{
		// Purge unclaimed objects.
		Ar.Logf(TEXT("Collecting garbage and resetting GC timer."));
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
		TimeSinceLastPendingKillPurge = 0.f;
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("TRYGC")))
	{
		// Purge unclaimed objects.		
		if (TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS))
		{
			Ar.Logf(TEXT("Collecting garbage and resetting GC timer."));
			TimeSinceLastPendingKillPurge = 0.f;
		}
		else
		{
			Ar.Logf(TEXT("Garbage collection blocked by other threads."));
		}
		return true;
	}
	else if (FParse::Command(&Cmd,TEXT("LIST2")))
	{			
		UClass* ClassToCheck = NULL;
		ParseObject<UClass>(Cmd, TEXT("CLASS="  ), ClassToCheck, ANY_PACKAGE );

		if (ClassToCheck == NULL)
		{
			ClassToCheck = UObject::StaticClass();
		}

		FObjectMemoryAnalyzer MemAnalyze(ClassToCheck);
		MemAnalyze.PrintResults(Ar, FObjectMemoryAnalyzer::EPrintFlags::PrintReferences);
		return true;
	}
	else if (FParse::Command(&Cmd,TEXT("MemSub")))
	{
		struct FCompareByInclusiveSize
		{
			FORCEINLINE bool operator()( const FItem& A, const FItem& B ) const 
			{ 
				return A.Max > B.Max; 
			}
		};

		struct FLocal
		{
			static void GetReferencedObjs( UObject* CurrentObject, TArray<UObject*>& out_ReferencedObjs )
			{
				TArray<UObject*> ReferencedObjs;
				FReferenceFinder RefCollector( ReferencedObjs, CurrentObject, true, false, false, false );
				RefCollector.FindReferences( CurrentObject );

				out_ReferencedObjs.Append( ReferencedObjs );
				for( UObject*& RefObj : ReferencedObjs )
				{
					GetReferencedObjs( RefObj, out_ReferencedObjs );
				}
			}
		};

		int32 Limit = 16;
		FParse::Value(Cmd, TEXT("CULL="), Limit);
		Limit *= 1024;

		UClass* ClassToCheck = NULL;
		ParseObject<UClass>(Cmd, TEXT("CLASS="  ), ClassToCheck, ANY_PACKAGE );
		if (ClassToCheck == NULL)
		{
			ClassToCheck = UObject::StaticClass();
		}

		TMap<UClass*,FItem> ObjectsByClass;

		Ar.Logf( TEXT("**********************************************") );
		Ar.Logf( TEXT("Obj MemSub for class '%s'"), *ClassToCheck->GetName() );
		Ar.Logf( TEXT("") );

		for( FObjectIterator It(ClassToCheck); It; ++It )
		{
			UObject* Obj = *It;
			if( Obj->IsTemplate( RF_ClassDefaultObject ) )
			{
				continue;
			}

			// Get references.
			TArray<UObject*> ReferencedObjects;
			FLocal::GetReferencedObjs( Obj, ReferencedObjects );

			// Calculate memory usage.
			FItem ThisObject( Obj->GetClass() );
			for( UObject*& RefObj : ReferencedObjects )
			{
				FArchiveCountMem Count( RefObj );
				FResourceSizeEx TrueResourceSize = FResourceSizeEx(EResourceSizeMode::Exclusive);
				It->GetResourceSizeEx( TrueResourceSize );
				ThisObject.Add( Count, TrueResourceSize );
			}

			FItem& ClassObjects = ObjectsByClass.FindOrAdd( ThisObject.Class );
			ClassObjects.Count++;
			ClassObjects.Num += ThisObject.Num;
			ClassObjects.Max += ThisObject.Max;
			ClassObjects.TrueResourceSize += ThisObject.TrueResourceSize;
		}

		ObjectsByClass.ValueSort( FCompareByInclusiveSize() );

		Ar.Logf( 
			TEXT("%32s %12s %12s %12s %12s %12s %12s %12s %12s %12s"), 
			TEXT("Class"),
			TEXT("IncMax"),
			TEXT("IncNum"),
			TEXT("ResExc"),
			TEXT("ResExcDedSys"),
			TEXT("ResExcShrSys"),
			TEXT("ResExcDedVid"),
			TEXT("ResExcShrVid"),
			TEXT("ResExcUnk"),
			TEXT("Count") 
			);

		FItem Total;
		FItem Culled;
		for( const auto& It : ObjectsByClass )
		{
			UClass* Class = It.Key;
			const FItem& ClassObjects = It.Value;

			if( ClassObjects.Max < (SIZE_T)Limit )
			{
				Culled.Count += ClassObjects.Count;
				Culled.Num += ClassObjects.Num;
				Culled.Max += ClassObjects.Max;
				Culled.TrueResourceSize += ClassObjects.TrueResourceSize;
			}
			else
			{
				Ar.Logf( TEXT("%32s %12s %12s %12s %12s %12s %12s %12s %12s %12i"), 
					*Class->GetName(), 
					*FHierarchy::Size(ClassObjects.Max), 
					*FHierarchy::Size(ClassObjects.Num), 
					*FHierarchy::Size(ClassObjects.TrueResourceSize.GetTotalMemoryBytes()),
					*FHierarchy::Size(ClassObjects.TrueResourceSize.GetDedicatedSystemMemoryBytes()),
					*FHierarchy::Size(ClassObjects.TrueResourceSize.GetSharedSystemMemoryBytes()),
					*FHierarchy::Size(ClassObjects.TrueResourceSize.GetDedicatedVideoMemoryBytes()),
					*FHierarchy::Size(ClassObjects.TrueResourceSize.GetSharedVideoMemoryBytes()),
					*FHierarchy::Size(ClassObjects.TrueResourceSize.GetUnknownMemoryBytes()),
					ClassObjects.Count 
					);

			}

			Total.Count += ClassObjects.Count;
			Total.Num += ClassObjects.Num;
			Total.Max += ClassObjects.Max;
			Total.TrueResourceSize += ClassObjects.TrueResourceSize;
		}

		if( Culled.Count > 0 )
		{
			Ar.Logf( TEXT("") );
			Ar.Logf( TEXT("%32s %12s %12s %12s %12s %12s %12s %12s %12s %12i"), 
				TEXT("(Culled)"), 
				*FHierarchy::Size(Culled.Max), 
				*FHierarchy::Size(Culled.Num), 
				*FHierarchy::Size(Culled.TrueResourceSize.GetTotalMemoryBytes()),
				*FHierarchy::Size(Culled.TrueResourceSize.GetDedicatedSystemMemoryBytes()),
				*FHierarchy::Size(Culled.TrueResourceSize.GetSharedSystemMemoryBytes()),
				*FHierarchy::Size(Culled.TrueResourceSize.GetDedicatedVideoMemoryBytes()),
				*FHierarchy::Size(Culled.TrueResourceSize.GetSharedVideoMemoryBytes()),
				*FHierarchy::Size(Culled.TrueResourceSize.GetUnknownMemoryBytes()),
				Culled.Count 
				);
		}

		Ar.Logf( TEXT("") );
		Ar.Logf( TEXT("%32s %12s %12s %12s %12s %12s %12s %12s %12s %12i"), 
			TEXT("Total"), 
			*FHierarchy::Size(Total.Max), 
			*FHierarchy::Size(Total.Num), 
			*FHierarchy::Size(Total.TrueResourceSize.GetTotalMemoryBytes()),
			*FHierarchy::Size(Total.TrueResourceSize.GetDedicatedSystemMemoryBytes()),
			*FHierarchy::Size(Total.TrueResourceSize.GetSharedSystemMemoryBytes()),
			*FHierarchy::Size(Total.TrueResourceSize.GetDedicatedVideoMemoryBytes()),
			*FHierarchy::Size(Total.TrueResourceSize.GetSharedVideoMemoryBytes()),
			*FHierarchy::Size(Total.TrueResourceSize.GetUnknownMemoryBytes()),
			Total.Count 
			);
		Ar.Logf( TEXT("**********************************************") );
		return true;
	}
	else if (FParse::Command(&Cmd,TEXT("Mem")))
	{
		int32 Limit = 50;
		FParse::Value(Cmd, TEXT("CULL="), Limit);
		Limit *= 1024;

		UClass* ClassToCheck = NULL;
		ParseObject<UClass>(Cmd, TEXT("CLASS="  ), ClassToCheck, ANY_PACKAGE );

		if (ClassToCheck == NULL)
		{
			ClassToCheck = UObject::StaticClass();
		}
		else
		{
			// Class is set, so lower a bit the limit.
			Limit /= 10;
		}

		FHierarchy Classes(Limit);
		FHierarchy Outers(Limit);
		FHierarchy Flat(Limit);

		TMap<UObject*, FSubItem> Objects;
		for( FObjectIterator It; It; ++It )
		{
			FArchiveCountMem Count( *It );
			FResourceSizeEx TrueResourceSize = FResourceSizeEx(EResourceSizeMode::Exclusive);
			It->GetResourceSizeEx(TrueResourceSize);
			Objects.Add(*It, FSubItem(*It, Count.GetNum(), Count.GetMax(), TrueResourceSize));
			Classes.AddClassInstance(*It);
			Outers.AddOuter(*It);
			Flat.AddFlat(*It);
		}

		UE_LOG(LogEngine, Log, TEXT("********************************************** By Outer Hierarchy") );
		Outers.Compute(NULL, Objects, false);
		Outers.Log( ClassToCheck, UObject::StaticClass(), false );
		

		UE_LOG(LogEngine, Log, TEXT("********************************************** By Class Hierarchy") );
		Classes.Compute(NULL, Objects, true);
		Classes.Log( ClassToCheck, UObject::StaticClass(), true );

		UE_LOG(LogEngine, Log, TEXT("********************************************** Flat") );
		Flat.Compute(NULL, Objects, false);
		Flat.Log( NULL, ClassToCheck, false );
		UE_LOG(LogEngine, Log, TEXT("**********************************************") );

		return true;
	}
	else if( FParse::Command(&Cmd,TEXT("LIST")) )
	{
		static TSet<FObjectKey> ForgottenObjects;

		// "obj list forget" will prevent all current objects from being reported in future "obj list" commands.
		// "obj list remember" clears that list
		if (FParse::Command(&Cmd, TEXT("FORGET")))
		{
			for (FObjectIterator It; It; ++It)
			{
				ForgottenObjects.Add(FObjectKey(*It));
			}
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("REMEMBER")))
		{
			ForgottenObjects.Empty();
			return true;
		}

		FString CmdLineOut = FString::Printf(TEXT("Obj List: %s"), Cmd);
		Ar.Log( *CmdLineOut );
		Ar.Log( TEXT("Objects:") );
		Ar.Log( TEXT("") );

		UClass* CheckType = NULL;
		UClass* MetaClass = NULL;

		// allow checking for any Outer, not just a UPackage
		UObject* CheckOuter = NULL;
		UPackage* InsidePackage = NULL;
		UObject* InsideObject = NULL;
		ParseObject<UClass>(Cmd, TEXT("CLASS="  ), CheckType, ANY_PACKAGE );
		ParseObject<UObject>(Cmd, TEXT("OUTER="), CheckOuter, ANY_PACKAGE);

		ParseObject<UPackage>(Cmd, TEXT("PACKAGE="), InsidePackage, NULL);
		if ( InsidePackage == NULL )
		{
			ParseObject<UObject>( Cmd, TEXT("INSIDE=" ), InsideObject, NULL );
		}
		int32 Depth = -1;
		FParse::Value(Cmd, TEXT("DEPTH="), Depth);

		FString ObjectName;
		FParse::Value(Cmd, TEXT("NAME="), ObjectName);

		TArray<FItem> List;
		TArray<FSubItem> Objects;
		FItem Total;

		// support specifying metaclasses when listing class objects
		if ( CheckType && CheckType->IsChildOf(UClass::StaticClass()) )
		{
			ParseObject<UClass>  ( Cmd, TEXT("TYPE="   ), MetaClass,     ANY_PACKAGE );
		}

		const bool bAll				= FParse::Param( Cmd, TEXT("ALL") );

		// if we specified a parameter in the command, but no objects of that parameter were found,
		// and they didn't specify "all", then don't list all objects
		if ( bAll ||
			((CheckType		||	!FCString::Strifind(Cmd,TEXT("CLASS=")))
			&&	(MetaClass		||	!FCString::Strifind(Cmd,TEXT("TYPE=")))
			&&	(CheckOuter		||	!FCString::Strifind(Cmd,TEXT("OUTER=")))
			&&	(InsidePackage	||	!FCString::Strifind(Cmd,TEXT("PACKAGE="))) 
			&&	(InsideObject	||	!FCString::Strifind(Cmd,TEXT("INSIDE=")))))
		{
			const bool bTrackDetailedObjectInfo = bAll || (CheckType != NULL && CheckType != UObject::StaticClass()) || CheckOuter != NULL || InsideObject != NULL || InsidePackage != NULL || !ObjectName.IsEmpty();
			const bool bOnlyListGCObjects = FParse::Param(Cmd, TEXT("GCONLY"));
			const bool bOnlyListGCObjectsNoClusters = FParse::Param(Cmd, TEXT("GCNOCLUSTERS"));
			const bool bOnlyListRootObjects = FParse::Param(Cmd, TEXT("ROOTONLY"));
			const bool bShouldIncludeDefaultObjects = FParse::Param(Cmd, TEXT("INCLUDEDEFAULTS"));
			const bool bOnlyListDefaultObjects = FParse::Param(Cmd, TEXT("DEFAULTSONLY"));
			const bool bShowDetailedObjectInfo = FParse::Param(Cmd, TEXT("NODETAILEDINFO")) == false && bTrackDetailedObjectInfo;

			for( FObjectIterator It; It; ++It )
			{
				if (ForgottenObjects.Contains(FObjectKey(*It)))
				{
					continue;
				}
				if (It->IsTemplate(RF_ClassDefaultObject))
				{
					if( !bShouldIncludeDefaultObjects )
					{
						continue;
					}
				}
				else if( bOnlyListDefaultObjects )
				{
					continue;
				}

				if ( bOnlyListGCObjects && GUObjectArray.IsDisregardForGC(*It) )
				{
					continue;
				}

				if (bOnlyListGCObjectsNoClusters)
				{
					if (GUObjectArray.IsDisregardForGC(*It))
					{
						continue;
					}
					FUObjectItem* ObjectItem = GUObjectArray.ObjectToObjectItem(*It);
					if (ObjectItem->GetOwnerIndex() > 0)
					{
						continue;
					}
				}

				if ( bOnlyListRootObjects && !It->IsRooted() )
				{
					continue;
				}

				if ( CheckType && !It->IsA(CheckType) )
				{
					continue;
				}

				if ( CheckOuter && It->GetOuter() != CheckOuter )
				{
					continue;
				}

				if ( InsidePackage && !It->IsIn(InsidePackage) )
				{
					continue;
				}

				if ( InsideObject && !It->IsIn(InsideObject) )
				{
					continue;
				}

				if (!ObjectName.IsEmpty() && It->GetName() != ObjectName)
				{
					continue;
				}

				if ( MetaClass )
				{
					UClass* ClassObj = Cast<UClass>(*It);
					if ( ClassObj && !ClassObj->IsChildOf(MetaClass) )
					{
						continue;
					}
				}

				FArchiveCountMem Count( *It );
				FResourceSizeEx TrueResourceSize = FResourceSizeEx(EResourceSizeMode::Exclusive);
				It->GetResourceSizeEx(TrueResourceSize);

				int32 i;

				// which class are we going to file this object under? by default, it's class
				UClass* ClassToUse = It->GetClass();
				// if we specified a depth to use, then put this object into the class Depth away from Object
				if (Depth != -1)
				{
					UClass* Travel = ClassToUse;
					// go up the class hierarchy chain, using a trail pointer Depth away
					for (int32 Up = 0; Up < Depth && Travel != UObject::StaticClass(); Up++)
					{
						Travel = Travel->GetSuperClass();
					}
					// when travel is a UObject, ClassToUse will be pointing to a class Depth away
					while (Travel != UObject::StaticClass())
					{
						Travel = Travel->GetSuperClass();
						ClassToUse = ClassToUse->GetSuperClass();
					}
				}

				for( i=0; i<List.Num(); i++ )
				{
					if( List[i].Class == ClassToUse )
					{
						break;
					}
				}
				if( i==List.Num() )
				{
					i = List.Add(FItem( ClassToUse ));
				}

				if( bShowDetailedObjectInfo )
				{
					Objects.Add( FSubItem( *It, Count.GetNum(), Count.GetMax(), TrueResourceSize ) );
				}
				List[i].Add( Count, TrueResourceSize );
				Total.Add( Count, TrueResourceSize );
			}
		}

		const bool bAlphaSort = FParse::Param( Cmd, TEXT("ALPHASORT") );
		const bool bCountSort = FParse::Param( Cmd, TEXT("COUNTSORT") );

		if( Objects.Num() )
		{
			struct FCompareFSubItem
			{
				bool bAlphaSort;
				FCompareFSubItem( bool InAlphaSort )
					: bAlphaSort( InAlphaSort )
				{}

				FORCEINLINE bool operator()( const FSubItem& A, const FSubItem& B ) const
				{
					return bAlphaSort ? (A.Object->GetPathName() < B.Object->GetPathName()) : (B.Max < A.Max);
				}
			};
			Objects.Sort( FCompareFSubItem( bAlphaSort ) );

			Ar.Logf( 
				TEXT("%140s %10s %10s %10s %15s %15s %15s %15s %15s"), 
				TEXT("Object"), 
				TEXT("NumKB"), 
				TEXT("MaxKB"), 
				TEXT("ResExcKB"),
				TEXT("ResExcDedSysKB"),
				TEXT("ResExcShrSysKB"),
				TEXT("ResExcDedVidKB"),
				TEXT("ResExcShrVidKB"),
				TEXT("ResExcUnkKB")
				);

			for (const FSubItem& ObjItem : Objects)
			{
				Ar.Logf(
					TEXT("%140s %10.2f %10.2f %10.2f %15.2f %15.2f %15.2f %15.2f %15.2f"), 
					*ObjItem.Object->GetFullName(), 
					ObjItem.Num / 1024.0f, 
					ObjItem.Max / 1024.0f, 
					ObjItem.TrueResourceSize.GetTotalMemoryBytes() / 1024.0f, 
					ObjItem.TrueResourceSize.GetDedicatedSystemMemoryBytes() / 1024.0f, 
					ObjItem.TrueResourceSize.GetSharedSystemMemoryBytes() / 1024.0f, 
					ObjItem.TrueResourceSize.GetDedicatedVideoMemoryBytes() / 1024.0f, 
					ObjItem.TrueResourceSize.GetSharedVideoMemoryBytes() / 1024.0f, 
					ObjItem.TrueResourceSize.GetUnknownMemoryBytes() / 1024.0f
					);
			}
			Ar.Log(TEXT(""));
		}

		if( List.Num() )
		{
			struct FCompareFItem
			{
				bool bAlphaSort, bCountSort;
				FCompareFItem( bool InAlphaSort, bool InCountSort )
					: bAlphaSort( InAlphaSort )
					, bCountSort( InCountSort )
				{}
				FORCEINLINE bool operator()( const FItem& A, const FItem& B ) const
				{
					return bAlphaSort ? (A.Class->GetName() < B.Class->GetName()) : bCountSort ? (B.Count < A.Count) : (B.Max < A.Max); 
				}
			};
			List.Sort( FCompareFItem( bAlphaSort, bCountSort ) );
			Ar.Logf(
				TEXT(" %100s %8s %10s %10s %10s %15s %15s %15s %15s %15s"), 
				TEXT("Class"), 
				TEXT("Count"), 
				TEXT("NumKB"), 
				TEXT("MaxKB"), 
				TEXT("ResExcKB"),
				TEXT("ResExcDedSysKB"),
				TEXT("ResExcShrSysKB"),
				TEXT("ResExcDedVidKB"),
				TEXT("ResExcShrVidKB"),
				TEXT("ResExcUnkKB")
				);

			for( int32 i=0; i<List.Num(); i++ )
			{
				Ar.Logf(
					TEXT(" %100s %8i %10.2f %10.2f %10.2f %15.2f %15.2f %15.2f %15.2f %15.2f"), 
					*List[i].Class->GetName(), 
					(int32)List[i].Count, 
					List[i].Num / 1024.0f, 
					List[i].Max / 1024.0f, 
					List[i].TrueResourceSize.GetTotalMemoryBytes() / 1024.0f, 
					List[i].TrueResourceSize.GetDedicatedSystemMemoryBytes() / 1024.0f, 
					List[i].TrueResourceSize.GetSharedSystemMemoryBytes() / 1024.0f, 
					List[i].TrueResourceSize.GetDedicatedVideoMemoryBytes() / 1024.0f, 
					List[i].TrueResourceSize.GetSharedVideoMemoryBytes() / 1024.0f, 
					List[i].TrueResourceSize.GetUnknownMemoryBytes() / 1024.0f
					);
			}
			Ar.Log( TEXT("") );
		}
		Ar.Logf( 
			TEXT("%i Objects (Total: %.3fM / Max: %.3fM / Res: %.3fM | ResDedSys: %.3fM / ResShrSys: %.3fM / ResDedVid: %.3fM / ResShrVid: %.3fM / ResUnknown: %.3fM)"),
			Total.Count, 
			(float)Total.Num/1024.0/1024.0, 
			(float)Total.Max/1024.0/1024.0, 
			(float)Total.TrueResourceSize.GetTotalMemoryBytes()/1024.0/1024.0,
			(float)Total.TrueResourceSize.GetDedicatedSystemMemoryBytes()/1024.0/1024.0, 
			(float)Total.TrueResourceSize.GetSharedSystemMemoryBytes()/1024.0/1024.0, 
			(float)Total.TrueResourceSize.GetDedicatedVideoMemoryBytes()/1024.0/1024.0, 
			(float)Total.TrueResourceSize.GetSharedVideoMemoryBytes()/1024.0/1024.0, 
			(float)Total.TrueResourceSize.GetUnknownMemoryBytes()/1024.0/1024.0
			);
		return true;

	}
	else if ( FParse::Command(&Cmd,TEXT("COMPONENTS")) )
	{
		UObject* Obj=NULL;
		FString ObjectName;

		if ( FParse::Token(Cmd,ObjectName,true) )
		{
			Obj = FindObject<UObject>(ANY_PACKAGE,*ObjectName);

			if ( Obj != NULL )
			{
				Ar.Log(TEXT(""));
				DumpComponents(Obj);
				Ar.Log(TEXT(""));
			}
			else
			{
				Ar.Logf(TEXT("No objects found named '%s'"), *ObjectName);
			}
		}
		else
		{
			Ar.Logf(TEXT("Syntax: OBJ COMPONENTS <Name Of Object>"));
		}
		return true;
	}
	else if ( FParse::Command(&Cmd,TEXT("DUMP")) )
	{
		// Dump all variable values for the specified object
		// supports specifying categories to hide or show
		// OBJ DUMP playercontroller0 hide="actor,object,lighting,movement"     OR
		// OBJ DUMP playercontroller0 show="playercontroller,controller"        OR
		// OBJ DUMP class=playercontroller name=playercontroller0 show=object OR
		// OBJ DUMP playercontroller0 recurse=true
		TCHAR ObjectName[1024];
		UObject* Obj = NULL;
		UClass* Cls = NULL;

		TArray<FString> HiddenCategories, ShowingCategories;

		if ( !ParseObject<UClass>( Cmd, TEXT("CLASS="), Cls, ANY_PACKAGE ) || !ParseObject(Cmd,TEXT("NAME="), Cls, Obj, ANY_PACKAGE) )
		{
			if ( FParse::Token(Cmd,ObjectName,ARRAY_COUNT(ObjectName), 1) )
			{
				Obj = FindObject<UObject>(ANY_PACKAGE,ObjectName);
			}
		}

		if ( Obj )
		{
			if ( Cast<UClass>(Obj) != NULL )
			{
				Obj = Cast<UClass>(Obj)->GetDefaultObject();
			}

			FString Value;

			Ar.Logf(TEXT(""));

			const bool bRecurse = FParse::Value(Cmd, TEXT("RECURSE=true"), Value);
			Ar.Logf(TEXT("*** Property dump for object %s'%s' ***"), bRecurse ? TEXT("(Recursive) ") : TEXT(""), *Obj->GetFullName() );

			if ( bRecurse )
			{
				const FExportObjectInnerContext Context;
				ExportProperties( &Context, Ar, Obj->GetClass(), (uint8*)Obj, 0, Obj->GetArchetype()->GetClass(), (uint8*)Obj->GetArchetype(), Obj, PPF_IncludeTransient );
			}
			else
			{
#if WITH_EDITORONLY_DATA
				//@todo: add support to FParse::Value() for specifying characters that should be ignored
				if ( FParse::Value(Cmd, TEXT("HIDE="), Value/*, TEXT(",")*/) )
				{
					Value.ParseIntoArray(HiddenCategories,TEXT(","),1);
				}
				else if ( FParse::Value(Cmd, TEXT("SHOW="), Value/*, TEXT(",")*/) )
				{
					Value.ParseIntoArray(ShowingCategories,TEXT(","),1);
				}
#endif
				UClass* LastOwnerClass = NULL;
				for ( TFieldIterator<UProperty> It(Obj->GetClass()); It; ++It )
				{
					UClass* Owner = It->GetOwnerClass();

					Value.Empty();
#if WITH_EDITOR
					if ( HiddenCategories.Num() )
					{
						const FString Category = FObjectEditorUtils::GetCategory(*It);
						int32 i;
						for ( i = 0; i < HiddenCategories.Num(); i++ )
						{
							if ( !Category.IsEmpty() && HiddenCategories[i] == Category )
							{
								break;
							}

							if ( HiddenCategories[i] == *Owner->GetName() )
							{
								break;
							}
						}

						if ( i < HiddenCategories.Num() )
						{
							continue;
						}
					}
					else if ( ShowingCategories.Num() )
					{
						const FString Category = FObjectEditorUtils::GetCategory(*It);
						int32 i;
						for ( i = 0; i < ShowingCategories.Num(); i++ )
						{
							if ( !Category.IsEmpty() && ShowingCategories[i] == Category )
							{
								break;
							}

							if ( ShowingCategories[i] == *Owner->GetName() )
							{
								break;
							}
						}

						if ( i == ShowingCategories.Num() )
						{
							continue;
						}
					}
#endif // #if WITH_EDITOR
					if ( LastOwnerClass != Owner )
					{
						LastOwnerClass = Owner;
						Ar.Logf(TEXT("=== %s properties ==="), *LastOwnerClass->GetName());
					}

					if ( It->ArrayDim > 1 )
					{
						for ( int32 i = 0; i < It->ArrayDim; i++ )
						{
							Value.Empty();
							It->ExportText_InContainer(i, Value, Obj, Obj, Obj, PPF_IncludeTransient);
							Ar.Logf(TEXT("  %s[%i]=%s"), *It->GetName(), i, *Value);
						}
					}
					else
					{
						UArrayProperty* ArrayProp = Cast<UArrayProperty>(*It);
						if ( ArrayProp != NULL )
						{
							FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, Obj);
							for( int32 i=0; i<FMath::Min(ArrayHelper.Num(),100); i++ )
							{
								Value.Empty();
								ArrayProp->Inner->ExportTextItem( Value, ArrayHelper.GetRawPtr(i), ArrayHelper.GetRawPtr(i), Obj, PPF_IncludeTransient );
								Ar.Logf(TEXT("  %s(%i)=%s"), *ArrayProp->GetName(), i, *Value);
							}

							if ( ArrayHelper.Num() >= 100 )
							{
								Ar.Logf(TEXT("  ... %i more elements"), ArrayHelper.Num() - 99);
							}
						}
						else
						{
							It->ExportText_InContainer(0, Value, Obj, Obj, Obj, PPF_IncludeTransient);
							Ar.Logf(TEXT("  %s=%s"), *It->GetName(), *Value);
						}
					}
				}
			}

			TMap<FString,FString> NativePropertyValues;
			if ( Obj->GetNativePropertyValues(NativePropertyValues) )
			{
				int32 LargestKey = 0;
				for ( TMap<FString,FString>::TIterator It(NativePropertyValues); It; ++It )
				{
					LargestKey = FMath::Max(LargestKey, It.Key().Len());
				}

				Ar.Log(TEXT("=== Native properties ==="));
				for ( TMap<FString,FString>::TIterator It(NativePropertyValues); It; ++It )
				{
					Ar.Logf(TEXT("  %s%s"), *It.Key().RightPad(LargestKey), *It.Value());
				}
			}
		}
		else
		{
			UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("No objects found using command '%s'"), Cmd));
		}

		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("HASH")))
	{
		const bool bShowHashBucketCollisionInfo = FParse::Param(Cmd, TEXT("SHOWBUCKETCOLLISIONS"));
		LogHashStatistics(Ar, bShowHashBucketCollisionInfo);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("HASHOUTER")))
	{
		const bool bShowHashBucketCollisionInfo = FParse::Param(Cmd, TEXT("SHOWBUCKETCOLLISIONS"));
		LogHashOuterStatistics(Ar, bShowHashBucketCollisionInfo);
		return true;
	}
	else
	{
		// OBJ command but not supported here
		return false;
	}
	return false;
}

bool UEngine::HandleDirCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TArray<FString> Files;
	TArray<FString> Directories;

	IFileManager::Get().FindFiles( Files, Cmd, 1, 0 );
	IFileManager::Get().FindFiles( Directories, Cmd, 0, 1 );

	// Directories
	Directories.Sort();
	for( int32 x = 0 ; x < Directories.Num() ; x++ )
	{
		Ar.Logf( TEXT("[%s]"), *Directories[x] );
	}

	// Files
	Files.Sort();
	for( int32 x = 0 ; x < Files.Num() ; x++ )
	{
		Ar.Logf( TEXT("[%s]"), *Files[x] );
	}

	return true;
}

bool UEngine::HandleTrackParticleRenderingStatsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	extern float GTimeBetweenParticleRenderStatCaptures;
	extern float GMinParticleDrawTimeToTrack;
	FString FlagStr(FParse::Token(Cmd, 0));
	if (FlagStr.Len() > 0)
	{
		GTimeBetweenParticleRenderStatCaptures = FCString::Atof(*FlagStr);
	}

	FString FlagStr2(FParse::Token(Cmd, 0));
	if (FlagStr2.Len() > 0)
	{
		GMinParticleDrawTimeToTrack = FCString::Atof(*FlagStr2);
	}

	extern bool GTrackParticleRenderingStats;
	GTrackParticleRenderingStats = !GTrackParticleRenderingStats;
	if (GTrackParticleRenderingStats)
	{
		if (GetCachedScalabilityCVars().DetailMode == DM_High)
		{
			Ar.Logf(TEXT("Currently in high detail mode, note that particle stats will only be captured in medium or low detail modes (eg splitscreen)."));
		}
		Ar.Logf(TEXT("Enabled particle render stat tracking with %.1fs between captures, min tracked time of %.4fs, use DUMPPARTICLERENDERINGSTATS to save results."),
			GTimeBetweenParticleRenderStatCaptures, GMinParticleDrawTimeToTrack);
	}
	else
	{
		Ar.Logf(TEXT("Disabled particle render stat tracking."));
	}
	return 1;
}

bool UEngine::HandleDumpAllocatorStats( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GMalloc->DumpAllocatorStats(Ar);
	return true;
}

bool UEngine::HandleHeapCheckCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GMalloc->ValidateHeap();
	return true;
}

bool UEngine::HandleToggleOnscreenDebugMessageDisplayCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GEngine->bEnableOnScreenDebugMessagesDisplay = !GEngine->bEnableOnScreenDebugMessagesDisplay;
	UE_LOG(LogEngine, Log, TEXT("OnScreenDebug Message Display is now %s"), 
		GEngine->bEnableOnScreenDebugMessagesDisplay ? TEXT("ENABLED") : TEXT("DISABLED"));
	if ((GEngine->bEnableOnScreenDebugMessagesDisplay == true) && (GEngine->bEnableOnScreenDebugMessages == false))
	{
		UE_LOG(LogEngine, Log, TEXT("OnScreenDebug Message system is DISABLED!"));
	}
	return true;
}

bool UEngine::HandleToggleOnscreenDebugMessageSystemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GEngine->bEnableOnScreenDebugMessages = !GEngine->bEnableOnScreenDebugMessages;
	UE_LOG(LogEngine, Log, TEXT("OnScreenDebug Message System is now %s"), 
		GEngine->bEnableOnScreenDebugMessages ? TEXT("ENABLED") : TEXT("DISABLED"));
	return true;
}

bool UEngine::HandleDisableAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GAreScreenMessagesEnabled = false;
	UE_LOG(LogEngine, Log, TEXT("Onscreen warnings/messages are now DISABLED"));
	return true;
}


bool UEngine::HandleEnableAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GAreScreenMessagesEnabled = true;
	UE_LOG(LogEngine, Log, TEXT("Onscreen warngins/messages are now ENABLED"));
	return true;
}


bool UEngine::HandleToggleAllScreenMessagesCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GAreScreenMessagesEnabled = !GAreScreenMessagesEnabled;
	UE_LOG(LogEngine, Log, TEXT("Onscreen warngins/messages are now %s"),
		GAreScreenMessagesEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
	return true;
}

bool UEngine::HandleTestslateGameUICommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	TSharedRef<SWidget> GameUI = 
		SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 5.0f )
		.HAlign( HAlign_Left )
		.VAlign( VAlign_Top )
		[
			SNew( SButton )
			.Text( NSLOCTEXT("UnrealEd", "TestSlateGameUIButtonText", "Test Button!") )
		]
	+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding( 5.0f )
		.FillWidth(0.66f)
		[
			SNew(SThrobber)
		];

	GEngine->GameViewport->AddViewportWidgetContent( GameUI );
	return true;
}

bool UEngine::HandleConfigHashCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString ConfigFilename;
	if ( FParse::Token(Cmd, ConfigFilename, true) )
	{
		if ( ConfigFilename == TEXT("NAMESONLY") )
		{
			Ar.Log( TEXT("Files map:") );
			for ( FConfigCacheIni::TIterator It(*GConfig); It; ++It )
			{
				// base filename is what Dump() compares against
				Ar.Logf(TEXT("FileName: %s (%s)"), *FPaths::GetBaseFilename(It.Key()), *It.Key());
			}
		}
		else
		{
			Ar.Logf(TEXT("Attempting to dump data for config file: %s"), *ConfigFilename);
			GConfig->Dump(Ar, *ConfigFilename);
		}
	}
	else
	{
		GConfig->Dump( Ar );
	}
	return true;
}

bool UEngine::HandleConfigMemCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	GConfig->ShowMemoryUsage(Ar);
	return true;
}

bool UEngine::HandleGetIniCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Format: GetIni IniFile:Section.SubSection Key
	TCHAR IniPlusSectionName[256];
	TCHAR KeyName[256];

	if (FParse::Token(Cmd, IniPlusSectionName, ARRAY_COUNT(IniPlusSectionName), true))
	{
		FString IniPlusSection = IniPlusSectionName;
		int32 IniDelim = IniPlusSection.Find(TEXT(":"));
		FString IniName;
		FString SectionName = (IniDelim != INDEX_NONE ? IniPlusSection.Mid(IniDelim+1) : IniPlusSection);

		if (IniDelim != INDEX_NONE)
		{
			// Check for hardcoded engine-ini:, game-ini: etc. parsing, and if not found fallback to raw string
			const FString* HardcodedIni = GetIniFilenameFromObjectsReference(IniPlusSection);

			if (HardcodedIni != NULL)
			{
				IniName = *HardcodedIni;
			}
			else
			{
				TArray<FString> ConfigList;
				FString SearchStr = IniPlusSection.Left(IniDelim) + TEXT(".ini");

				GConfig->GetConfigFilenames(ConfigList);

				for (auto CurConfig : ConfigList)
				{
					if (CurConfig.Contains(SearchStr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
					{
						IniName = CurConfig;
						break;
					}
				}

				if (IniName.IsEmpty())
				{
					UE_SUPPRESS(LogExec, Warning,
								Ar.Logf(TEXT("Failed to find IniFile '%s' (note: can only search loaded ini files)."), *SearchStr));
				}
			}
		}
		else
		{
			IniName = GEngineIni;
		}

		if (!IniName.IsEmpty() && !SectionName.IsEmpty())
		{
			if (FParse::Token(Cmd, KeyName, ARRAY_COUNT(KeyName), true))
			{
				TArray<FString> Values;

				bool bSuccess = !!GConfig->GetArray(*SectionName, KeyName, Values, IniName);

				if (bSuccess)
				{
					for (auto CurValue : Values)
					{
						Ar.Log(*CurValue);
					}
				}
				else
				{
					UE_SUPPRESS(LogExec, Warning, Ar.Logf(TEXT("Failed to get config key '%s', from section '%s', in ini file '%s'."),
								KeyName, *SectionName, *IniName));
				}
			}
			else
			{
				UE_SUPPRESS(LogExec, Warning,
							Ar.Logf(TEXT("No Key specified. Command format: GetIni IniFile:Section.SubSection Key")));
			}
		}
		else if (IniName.IsEmpty())
		{
			UE_SUPPRESS(LogExec, Warning,
						Ar.Logf(TEXT("IniFile parsing failed (%s). Command format: GetIni IniFile:Section.SubSection Key"),
								IniPlusSectionName));
		}
		else // if (SectionName.IsEmpty())
		{
			UE_SUPPRESS(LogExec, Warning,
						Ar.Logf(TEXT("Section parsing failed (%s). Command format: GetIni IniFile:Section.SubSection Key"),
								IniPlusSectionName));
		}
	}
	else
	{
		UE_SUPPRESS(LogExec, Warning,
					Ar.Logf(TEXT("No Section specified. Command format: GetIni IniFile:Section.SubSection Key")))
	}

	return true;
}
#endif // !UE_BUILD_SHIPPING


// debug flag to allocate memory every frame, to trigger an OOM condition
static bool GDebugAllocMemEveryFrame = false;

/** Helper function to cause a stack overflow crash */
FORCENOINLINE void StackOverflowFunction(int32* DummyArg)
{
	int32 StackArray[8196];
	FMemory::Memset(StackArray, 0, sizeof(StackArray));
	if (StackArray[0] == 0)
	{
		UE_LOG(LogEngine, VeryVerbose, TEXT("StackOverflowFunction(%d)"), DummyArg ? DummyArg[0] : 0);
		StackOverflowFunction(StackArray);
	}
}

bool UEngine::PerformError(const TCHAR* Cmd, FOutputDevice& Ar)
{
#if !UE_BUILD_SHIPPING
	if (FParse::Command(&Cmd, TEXT("RENDERCRASH")))
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(CauseRenderThreadCrash, { UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.")); SetCrashType(ECrashType::Debug); UE_LOG(LogEngine, Fatal, TEXT("Crashing the renderthread at your request")); });
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("RENDERCHECK")))
	{
		struct FRender
		{
			static void Check()
			{				
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
				SetCrashType(ECrashType::Debug);
				check(!"Crashing the renderthread via check(0) at your request");
			}
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND(CauseRenderThreadCrash, { FRender::Check(); });
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("RENDERGPF")))
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(CauseRenderThreadCrash, { UE_LOG(LogEngine, Warning, TEXT("Printed warning to log.")); SetCrashType(ECrashType::Debug); *(int32 *)3 = 123; });
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("RENDERFATAL")))
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(CauseRenderThreadCrash,
		{
			UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
			SetCrashType(ECrashType::Debug);
			LowLevelFatalError(TEXT("FError::LowLevelFatal test"));
		});
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("RENDERENSURE")))
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(CauseRenderThreadEnsure,
		{
			UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
			if (!ensure(0))
			{
				UE_LOG(LogEngine, Warning, TEXT("Ensure condition failed (this is the expected behavior)."));
			}
		});
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("THREADCRASH")))
	{
		struct FThread
		{
			static void Crash(ENamedThreads::Type, const  FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
				SetCrashType(ECrashType::Debug);
				UE_LOG(LogEngine, Fatal, TEXT("Crashing the worker thread at your request"));
			}
		};

		DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.FThread::Crash"),
		STAT_FDelegateGraphTask_FThread__Crash,
			STATGROUP_TaskGraphTasks);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateStatic(FThread::Crash),
			GET_STATID(STAT_FDelegateGraphTask_FThread__Crash)
			),
			ENamedThreads::GameThread
			);
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("THREADCHECK")))
	{
		struct FThread
		{
			static void Check(ENamedThreads::Type, const FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
				SetCrashType(ECrashType::Debug);
				check(!"Crashing a worker thread via check(0) at your request");
			}
		};

		DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.FThread::Check"),
		STAT_FDelegateGraphTask_FThread__Check,
			STATGROUP_TaskGraphTasks);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateStatic(FThread::Check),
			GET_STATID(STAT_FDelegateGraphTask_FThread__Check)
			),
			ENamedThreads::GameThread
			);
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("THREADGPF")))
	{
		struct FThread
		{
			static void GPF(ENamedThreads::Type, const FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
				SetCrashType(ECrashType::Debug);
				*(int32 *)3 = 123;
			}
		};

		DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.FThread::GPF"),
		STAT_FDelegateGraphTask_FThread__GPF,
			STATGROUP_TaskGraphTasks);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateStatic(FThread::GPF),
			GET_STATID(STAT_FDelegateGraphTask_FThread__GPF)
			),
			ENamedThreads::GameThread
			);
		return true;
	}
	if (FParse::Command(&Cmd, TEXT("TWOTHREADSCRASH")))
	{
		class FThreadPoolCrash : public IQueuedWork
		{
		private:
			double CrashDelay;
		public:
			FThreadPoolCrash(double InCrashDelay)
				: CrashDelay(InCrashDelay)
			{
			}
			void Abandon()
			{
			}
			void DoThreadedWork()
			{
				double CrashTime = FPlatformTime::Seconds() + CrashDelay;
				do 
				{
					if (FPlatformTime::Seconds() >= CrashTime)
					{
						UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
						SetCrashType(ECrashType::Debug);
						UE_LOG(LogEngine, Fatal, TEXT("Crashing the worker thread at your request"));
						break;
					}
					else
					{
						FPlatformProcess::Sleep(0);
					}
				} while (true);
			}
		};

		UE_LOG(LogEngine, Warning, TEXT("Queuing two tasks to crash."));
		GThreadPool->AddQueuedWork(new FThreadPoolCrash(0.100));
		GThreadPool->AddQueuedWork(new FThreadPoolCrash(0.110));

		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("TWOTHREADSGPF")))
	{
		class FThreadPoolCrash : public IQueuedWork
		{
		private:
			double CrashDelay;
		public:
			FThreadPoolCrash(double InCrashDelay)
				: CrashDelay(InCrashDelay)
			{
			}
			void Abandon()
			{
			}
			void DoThreadedWork()
			{
				double CrashTime = FPlatformTime::Seconds() + CrashDelay;
				do
				{
					if (FPlatformTime::Seconds() >= CrashTime)
					{
						UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
						SetCrashType(ECrashType::Debug);
						*(int32 *)3 = 123;
						break;
					}
					else
					{
						FPlatformProcess::Sleep(0);
					}
				} while (true);
			}
		};

		UE_LOG(LogEngine, Warning, TEXT("Queuing two tasks to crash."));
		GThreadPool->AddQueuedWork(new FThreadPoolCrash(0.100));
		GThreadPool->AddQueuedWork(new FThreadPoolCrash(0.110));

		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("THREADENSURE")))
	{
		struct FThread
		{
			static void Ensure(ENamedThreads::Type, const FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
				ensure(0);
			}
		};

		DECLARE_CYCLE_STAT(TEXT("FThread::Ensure"),
		STAT_FThread__Ensure,
			STATGROUP_TaskGraphTasks);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateStatic(FThread::Ensure),
			GET_STATID(STAT_FThread__Ensure)),
			ENamedThreads::GameThread
			);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("THREADFATAL")))
	{
		struct FThread
		{
			static void Fatal(ENamedThreads::Type, const FGraphEventRef&)
			{
				UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
				SetCrashType(ECrashType::Debug);
				LowLevelFatalError(TEXT("FError::LowLevelFatal test"));
			}
		};

		DECLARE_CYCLE_STAT(TEXT("FThread::Fatal"),
		STAT_FThread__Fatal,
			STATGROUP_TaskGraphTasks);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateStatic(FThread::Fatal),
			GET_STATID(STAT_FThread__Fatal)),
			ENamedThreads::GameThread
			);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("CRASH")))
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
		SetCrashType(ECrashType::Debug);
		UE_LOG(LogEngine, Fatal, TEXT("%s"), TEXT("Crashing the gamethread at your request"));
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("CHECK")))
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
		SetCrashType(ECrashType::Debug);
		check(!"Crashing the game thread via check(0) at your request");
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("GPF")))
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
		Ar.Log(TEXT("Crashing with voluntary GPF"));
		SetCrashType(ECrashType::Debug);
		// changed to 3 from NULL because clang noticed writing to NULL and warned about it
		*(int32 *)3 = 123;
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("ENSURE")))
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
		if (!ensure(0))
		{
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("ENSUREALWAYS")))
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
		if (!ensureAlways(0))
		{
			return true;
		}
	}
	else if (FParse::Command(&Cmd, TEXT("FATAL")))
	{
		UE_LOG(LogEngine, Warning, TEXT("Printed warning to log."));
		SetCrashType(ECrashType::Debug);
		LowLevelFatalError(TEXT("FError::LowLevelFatal test"));
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("BUFFEROVERRUN")))
	{
		// stack overflow test - this case should be caught by /GS (Buffer Overflow Check) compile option
		ANSICHAR SrcBuffer[] = "12345678901234567890123456789012345678901234567890";
		SetCrashType(ECrashType::Debug);
		BufferOverflowFunction(ARRAY_COUNT(SrcBuffer), SrcBuffer);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("CRTINVALID")))
	{
		SetCrashType(ECrashType::Debug);
		FString::Printf(TEXT("%s"), (const char*)nullptr);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("HITCH")))
	{
		SCOPE_CYCLE_COUNTER(STAT_IntentionalHitch);
		FPlatformProcess::Sleep(1.0f);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("RENDERHITCH")))
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(CauseRenderThreadHitch, { SCOPE_CYCLE_COUNTER(STAT_IntentionalHitch); FPlatformProcess::Sleep(1.0f); });
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("LONGLOG")))
	{
		UE_LOG(LogEngine, Log, TEXT("This is going to be a really long log message to test the code to resize the buffer used to log with. %02048s"), TEXT("HAHA, this isn't really a long string, but it sure has a lot of zeros!"));
	}
	else if (FParse::Command(&Cmd, TEXT("RECURSE")))
	{
		Ar.Logf(TEXT("Recursing to create a very deep callstack."));
		GLog->Flush();
		SetCrashType(ECrashType::Debug);
		InfiniteRecursionFunction(1);
		Ar.Logf(TEXT("You will never see this log line."));
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("THREADRECURSE")))
	{
		Ar.Log(TEXT("Recursing to create a very deep callstack (in a separate thread)."));
		struct FThread
		{
			static void InfiniteRecursion(ENamedThreads::Type, const FGraphEventRef&)
			{
				SetCrashType(ECrashType::Debug);
				InfiniteRecursionFunction(1);
			}
		};

		DECLARE_CYCLE_STAT(TEXT("FThread::InfiniteRecursion"),
		STAT_FThread__InfiniteRecursion,
			STATGROUP_TaskGraphTasks);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateStatic(FThread::InfiniteRecursion),
			GET_STATID(STAT_FThread__InfiniteRecursion)),
			ENamedThreads::GameThread
			);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("EATMEM")))
	{
		Ar.Log(TEXT("Eating up all available memory"));
		SetCrashType(ECrashType::Debug);
		while (1)
		{
			void* Eat = FMemory::Malloc(65536);
			FMemory::Memset(Eat, 0, 65536);
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("OOM")))
	{
		Ar.Log(TEXT("Will continuously allocate 1MB per frame until we hit OOM"));
		GDebugAllocMemEveryFrame = true;
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("STACKOVERFLOW")))
	{
		Ar.Log(TEXT("Infinite recursion to cause stack overflow"));
		SetCrashType(ECrashType::Debug);
		StackOverflowFunction(nullptr);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("THREADSTACKOVERFLOW")))
	{
		Ar.Log(TEXT("Infinite recursion to cause stack overflow will happen in a separate thread."));
		struct FThread
		{
			static void StackOverflow(ENamedThreads::Type, const FGraphEventRef&)
			{
				SetCrashType(ECrashType::Debug);
				StackOverflowFunction(nullptr);
			}
		};

		DECLARE_CYCLE_STAT(TEXT("FThread::StackOverflow"),
		STAT_FThread__StackOverflow,
			STATGROUP_TaskGraphTasks);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateStatic(FThread::StackOverflow),
			GET_STATID(STAT_FThread__StackOverflow)),
			ENamedThreads::GameThread
			);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("SOFTLOCK")))
	{
		Ar.Log(TEXT("Hanging the current thread"));
		SetCrashType(ECrashType::Debug);
		while (1)
		{
			FPlatformProcess::Sleep(1.0f);
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("INFINITELOOP")))
	{
		Ar.Log(TEXT("Hanging the current thread (CPU-intensive)"));
		SetCrashType(ECrashType::Debug);
		for(;;)
		{
		}
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("SLEEP")))
	{
		Ar.Log(TEXT("Sleep for 1 hour. This should crash after a few seconds in cooked builds."));
		FPlatformProcess::Sleep(3600);
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("AUDIOGPF")))
	{
		FAudioThread::RunCommandOnAudioThread([]()
		{
			*(int32 *)3 = 123;
		}, TStatId());
		return true;
	}
	else if (FParse::Command(&Cmd, TEXT("AUDIOCHECK")))
	{
		FAudioThread::RunCommandOnAudioThread([]()
		{
			check(!"Crashing the audio thread via check(0) at your request");
		}, TStatId());
		return true;
	}
#endif // !UE_BUILD_SHIPPING
	return false;
}


/**
 * Computes a color to use for property coloration for the given object.
 *
 * @param	Object		The object for which to compute a property color.
 * @param	OutColor	[out] The returned color.
 * @return				true if a color was successfully set on OutColor, false otherwise.
 */
bool UEngine::GetPropertyColorationColor(UObject* Object, FColor& OutColor)
{
	return false;
}

/** Uses StatColorMappings to find a color for this stat's value. */
bool UEngine::GetStatValueColoration(const FString& StatName, float Value, FColor& OutColor)
{
	for(const FStatColorMapping& Mapping : StatColorMappings)
	{
		if(StatName == Mapping.StatName)
		{
			const int32 NumPoints = Mapping.ColorMap.Num();

			// If no point in curve, return the Default value we passed in.
			if( NumPoints == 0 )
			{
				return false;
			}

			// If only one point, or before the first point in the curve, return the first points value.
			if( NumPoints < 2 || (Value <= Mapping.ColorMap[0].In) )
			{
				OutColor = Mapping.ColorMap[0].Out;
				return true;
			}

			// If beyond the last point in the curve, return its value.
			if( Value >= Mapping.ColorMap[NumPoints-1].In )
			{
				OutColor = Mapping.ColorMap[NumPoints-1].Out;
				return true;
			}

			// Somewhere with curve range - linear search to find value.
			for( int32 PointIndex=1; PointIndex<NumPoints; PointIndex++ )
			{	
				if( Value < Mapping.ColorMap[PointIndex].In )
				{
					if (Mapping.DisableBlend)
					{
						OutColor = Mapping.ColorMap[PointIndex].Out;
					}
					else
					{
						const float Diff = Mapping.ColorMap[PointIndex].In - Mapping.ColorMap[PointIndex-1].In;
						const float Alpha = (Value - Mapping.ColorMap[PointIndex-1].In) / Diff;

						FLinearColor A(Mapping.ColorMap[PointIndex-1].Out);
						FVector AV(A.R, A.G, A.B);

						FLinearColor B(Mapping.ColorMap[PointIndex].Out);
						FVector BV(B.R, B.G, B.B);

						FVector OutColorV = FMath::Lerp( AV, BV, Alpha );
						OutColor = FLinearColor(OutColorV.X, OutColorV.Y, OutColorV.Z).ToFColor(true);
					}

					return true;
				}
			}

			OutColor = Mapping.ColorMap[NumPoints-1].Out;
			return true;
		}
	}

	// No entry for this stat name
	return false;
}

void UEngine::OnLostFocusPause(bool EnablePause)
{
	if( bPauseOnLossOfFocus )
	{
		for (auto It = WorldList.CreateIterator(); It; ++It)
		{
			FWorldContext &Context = *It;

			if ( Context.OwningGameInstance == NULL )
			{
				continue;
			}

			const TArray<class ULocalPlayer*> & LocalPlayers = Context.OwningGameInstance->GetLocalPlayers();

			// Iterate over all players and pause / unpause them
			// Note: pausing / unpausing the player is done via their HUD pausing / unpausing
			for (int32 PlayerIndex = 0; PlayerIndex < LocalPlayers.Num(); ++PlayerIndex)
			{
				APlayerController* PlayerController = LocalPlayers[PlayerIndex]->PlayerController;
				if(PlayerController && PlayerController->MyHUD)
				{
					PlayerController->MyHUD->OnLostFocusPause(EnablePause);
				}
			}
		}
	}
}

void UEngine::StartHardwareSurvey()
{
	// The hardware survey costs time and we don't want to slow down debug builds.
	// This is mostly because of the CPU benchmark running in the survey and the results in debug are not being valid.
	// Never run the survey in games, only in the editor.
	if (FEngineAnalytics::IsAvailable() && FEngineAnalytics::IsEditorRun())
	{
		IHardwareSurveyModule::Get().StartHardwareSurvey(FEngineAnalytics::GetProvider());
	}
}

void UEngine::InitHardwareSurvey()
{
	StartHardwareSurvey();
}

void UEngine::TickHardwareSurvey()
{

}

bool UEngine::IsHardwareSurveyRequired()
{
	// Analytics must have been initialized FIRST.
	if (!FEngineAnalytics::IsAvailable() || IsRunningDedicatedServer())
	{
		return false;
	}

#if PLATFORM_IOS || PLATFORM_ANDROID || PLATFORM_DESKTOP
	bool bSurveyDone = false;
	bool bSurveyExpired = false;

	// platform agnostic code to get the last time we did a survey
	FString LastRecordedTimeString;
	if (FPlatformMisc::GetStoredValue(TEXT("Epic Games"), TEXT("Unreal Engine/Hardware Survey"), TEXT("HardwareSurveyDateTime"), LastRecordedTimeString))
	{
		// attempt to convert to FDateTime
		FDateTime LastRecordedTime;
		if (FDateTime::Parse(LastRecordedTimeString, LastRecordedTime))
		{
			bSurveyDone = true;

			// make sure it was a month ago
			FTimespan Diff = FDateTime::UtcNow() - LastRecordedTime;

			if (Diff.GetTotalDays() > 30)
			{
				bSurveyExpired = true;
			}
		}
	}

	return !bSurveyDone || bSurveyExpired;
#else
	return false;
#endif
}

FString UEngine::HardwareSurveyBucketRAM(uint32 MemoryMB)
{
	const float GBToMB = 1024.0f;
	FString BucketedRAM;

	if (MemoryMB < 2.0f * GBToMB) BucketedRAM = TEXT("<2GB");
	else if (MemoryMB < 4.0f * GBToMB) BucketedRAM = TEXT("2GB-4GB");
	else if (MemoryMB < 6.0f * GBToMB) BucketedRAM = TEXT("4GB-6GB");
	else if (MemoryMB < 8.0f * GBToMB) BucketedRAM = TEXT("6GB-8GB");
	else if (MemoryMB < 12.0f * GBToMB) BucketedRAM = TEXT("8GB-12GB");
	else if (MemoryMB < 16.0f * GBToMB) BucketedRAM = TEXT("12GB-16GB");
	else if (MemoryMB < 20.0f * GBToMB) BucketedRAM = TEXT("16GB-20GB");
	else if (MemoryMB < 24.0f * GBToMB) BucketedRAM = TEXT("20GB-24GB");
	else if (MemoryMB < 28.0f * GBToMB) BucketedRAM = TEXT("24GB-28GB");
	else if (MemoryMB < 32.0f * GBToMB) BucketedRAM = TEXT("28GB-32GB");
	else if (MemoryMB < 36.0f * GBToMB) BucketedRAM = TEXT("32GB-36GB");
	else BucketedRAM = TEXT(">36GB");

	return BucketedRAM;
}

FString UEngine::HardwareSurveyBucketVRAM(uint32 VidMemoryMB)
{
	const float GBToMB = 1024.0f;
	FString BucketedVRAM;

	if (VidMemoryMB < 0.25f * GBToMB) BucketedVRAM = TEXT("<256MB");
	else if (VidMemoryMB < 0.5f * GBToMB) BucketedVRAM = TEXT("256MB-512MB");
	else if (VidMemoryMB < 1.0f * GBToMB) BucketedVRAM = TEXT("512MB-1GB");
	else if (VidMemoryMB < 1.5f * GBToMB) BucketedVRAM = TEXT("1GB-1.5GB");
	else if (VidMemoryMB < 2.0f * GBToMB) BucketedVRAM = TEXT("1.5GB-2GB");
	else if (VidMemoryMB < 2.5f * GBToMB) BucketedVRAM = TEXT("2GB-2.5GB");
	else if (VidMemoryMB < 3.0f * GBToMB) BucketedVRAM = TEXT("2.5GB-3GB");
	else if (VidMemoryMB < 4.0f * GBToMB) BucketedVRAM = TEXT("3GB-4GB");
	else if (VidMemoryMB < 6.0f * GBToMB) BucketedVRAM = TEXT("4GB-6GB");
	else if (VidMemoryMB < 8.0f * GBToMB) BucketedVRAM = TEXT("6GB-8GB");
	else BucketedVRAM = TEXT(">8GB");

	return BucketedVRAM;
}

FString UEngine::HardwareSurveyBucketResolution(uint32 DisplayWidth, uint32 DisplayHeight)
{
	FString BucketedRes;
	float AspectRatio = (float)DisplayWidth / DisplayHeight;

	if (AspectRatio < 1.5f)
	{
		// approx 4:3
		if (DisplayWidth < 1150)
		{
			BucketedRes = TEXT("1024x768");
		}
		else if (DisplayHeight < 912)
		{
			BucketedRes = TEXT("1280x800");
		}
		else
		{
			BucketedRes = TEXT("1280x1024");
		}
	}
	else
	{
		// widescreen
		if (DisplayWidth < 1400)
		{
			BucketedRes = TEXT("1366x768");
		}
		else if (DisplayWidth < 1520)
		{
			BucketedRes = TEXT("1440x900");
		}
		else if (DisplayWidth < 1640)
		{
			BucketedRes = TEXT("1600x900");
		}
		else if (DisplayWidth < 1800)
		{
			BucketedRes = TEXT("1680x1050");
		}
		else if (DisplayHeight < 1140)
		{
			BucketedRes = TEXT("1920x1080");
		}
		else
		{
			BucketedRes = TEXT("1920x1200");
		}
	}

	return BucketedRes;
}

FString UEngine::HardwareSurveyGetResolutionClass(uint32 LargestDisplayHeight)
{
	FString ResolutionClass = TEXT( "720" );

	if( LargestDisplayHeight < 700 )
	{
		ResolutionClass = TEXT( "<720" );
	}
	else if( LargestDisplayHeight > 1024 )
	{
		ResolutionClass = TEXT( "1080+" );
	}

	return ResolutionClass;
}

void UEngine::OnHardwareSurveyComplete(const FHardwareSurveyResults& SurveyResults)
{
}

static TAutoConsoleVariable<float> CVarMaxFPS(
	TEXT("t.MaxFPS"),0.f,
	TEXT("Caps FPS to the given value.  Set to <= 0 to be uncapped."));
// CauseHitches cvar
static TAutoConsoleVariable<int32> CVarCauseHitches(
	TEXT("CauseHitches"),0,
	TEXT("Causes a 200ms hitch every second."));

static TAutoConsoleVariable<int32> CVarUnsteadyFPS(
	TEXT("t.UnsteadyFPS"),0,
	TEXT("Causes FPS to bounce around randomly in the 8-32 range."));

void UEngine::InitializeRunningAverageDeltaTime()
{
	// Running average delta time, initial value at 100 FPS so fast machines don't have to creep up
	// to a good frame rate due to code limiting upward "mobility".
	RunningAverageDeltaTime = 1 / 100.f;
}

bool UEngine::IsAllowedFramerateSmoothing() const
{
	return FPlatformProperties::AllowsFramerateSmoothing() && bSmoothFrameRate && !bForceDisableFrameRateSmoothing && !IsRunningDedicatedServer();
}

/** Compute tick rate limitor. */
void UEngine::UpdateRunningAverageDeltaTime(float DeltaTime, bool bAllowFrameRateSmoothing)
{
	if (bAllowFrameRateSmoothing && IsAllowedFramerateSmoothing())
	{
		// Smooth the framerate if wanted. The code uses a simplistic running average. Other approaches, like reserving
		// a percentage of time, ended up creating negative feedback loops in conjunction with GPU load and were abandonend.
			if( DeltaTime < 0.0f )
			{
#if PLATFORM_ANDROID
				UE_LOG(LogEngine, Warning, TEXT("Detected negative delta time - ignoring"));
				DeltaTime = 0.01;
#elif (UE_BUILD_SHIPPING && WITH_EDITOR)
				// End users don't have access to the secure parts of UDN. The localized string points to the release notes,
				// which should include a link to the AMD CPU drivers download site.
				UE_LOG(LogEngine, Fatal, TEXT("%s"), TEXT("CPU time drift detected! Please consult release notes on how to address this."));
#else
				// Send developers to the support list thread.
				UE_LOG(LogEngine, Fatal, TEXT("Negative delta time! Please see https://udn.epicgames.com/lists/showpost.php?list=ue3bugs&id=4364"));
#endif
			}

			// Keep track of running average over 300 frames, clamping at min of 5 FPS for individual delta times.
			RunningAverageDeltaTime = FMath::Lerp<float>( RunningAverageDeltaTime, FMath::Min<float>( DeltaTime, 0.2f ), 1 / 300.f );
	}
}


/** Get tick rate limitor. */
float UEngine::GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing) const
{
	float MaxTickRate = 0;

	if (bAllowFrameRateSmoothing && IsAllowedFramerateSmoothing())
	{
		// Work in FPS domain as that is what the function will return.
		MaxTickRate = 1.f / RunningAverageDeltaTime;

		// Clamp FPS into ini defined min/ max range.
		if (SmoothedFrameRateRange.HasLowerBound())
		{
			MaxTickRate = FMath::Max( MaxTickRate, SmoothedFrameRateRange.GetLowerBoundValue() );
		}
		if (SmoothedFrameRateRange.HasUpperBound())
		{
			MaxTickRate = FMath::Min( MaxTickRate, SmoothedFrameRateRange.GetUpperBoundValue() );
		}
	}

	if (CVarCauseHitches.GetValueOnGameThread())
	{
		static float RunningHitchTimer = 0.f;
		RunningHitchTimer += DeltaTime;
		if (RunningHitchTimer > 1.f)
		{
			// hitch!
			UE_LOG(LogEngine, Display, TEXT("Hitching by request!"));
			FPlatformProcess::Sleep(0.2f);
			RunningHitchTimer = 0.f;
		}
	}

	if (CVarUnsteadyFPS.GetValueOnGameThread())
	{
		static float LastMaxTickRate = 20.f;
		float RandDelta = FMath::FRandRange(-5.f, 5.f);
		MaxTickRate = FMath::Clamp(LastMaxTickRate + RandDelta, 8.f, 32.f);
		LastMaxTickRate = MaxTickRate;
	}
	else if (CVarMaxFPS.GetValueOnGameThread() > 0)
	{
		MaxTickRate = CVarMaxFPS.GetValueOnGameThread();
	}

	return MaxTickRate;
}

float UEngine::GetMaxFPS() const
{
	return CVarMaxFPS.GetValueOnAnyThread();
}

void UEngine::SetMaxFPS(const float MaxFPS)
{
	IConsoleVariable* ConsoleVariable = CVarMaxFPS.AsVariable();

	const EConsoleVariableFlags LastSetReason = (EConsoleVariableFlags)(ConsoleVariable->GetFlags() & ECVF_SetByMask);
	const EConsoleVariableFlags ThisSetReason = (LastSetReason == ECVF_SetByConstructor) ? ECVF_SetByScalability : LastSetReason;

	ConsoleVariable->Set(MaxFPS, ThisSetReason);
}

/**
 * Enables or disables the ScreenSaver (desktop only)
 *
 * @param bEnable	If true the enable the screen saver, if false disable it.
 */
void UEngine::EnableScreenSaver( bool bEnable )
{
#if PLATFORM_DESKTOP
	if (GIsRequestingExit)
	{
		return;
	}

	TCHAR EnvVariable[32];
	FPlatformMisc::GetEnvironmentVariable(TEXT("UE-DisallowScreenSaverInhibitor"), EnvVariable, ARRAY_COUNT(EnvVariable));
	const bool bDisallowScreenSaverInhibitor = FString(EnvVariable).ToBool();
	
	// By default we allow to use screen saver inhibitor, but in some cases user can override this setting.
	if( !bDisallowScreenSaverInhibitor )
	{
		// try a simpler API first
		if ( !FPlatformApplicationMisc::ControlScreensaver( bEnable ? FPlatformApplicationMisc::EScreenSaverAction::Enable : FPlatformApplicationMisc::EScreenSaverAction::Disable ) )
		{
			// Screen saver inhibitor disabled if no multithreading is available.
			if (FPlatformProcess::SupportsMultithreading() )
			{
				if( !ScreenSaverInhibitor )
				{
					// Create thread inhibiting screen saver while it is running.
					ScreenSaverInhibitorRunnable = new FScreenSaverInhibitor();
					ScreenSaverInhibitor = FRunnableThread::Create(ScreenSaverInhibitorRunnable, TEXT("ScreenSaverInhibitor"), 16 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
					// Only actually run when needed to not bypass group policies for screensaver, etc.
					ScreenSaverInhibitor->Suspend( true );
					ScreenSaverInhibitorSemaphore = 0;
				}

				if( bEnable && ScreenSaverInhibitorSemaphore > 0)
				{
					if( --ScreenSaverInhibitorSemaphore == 0 )
					{	
						// If the semaphore is zero and we are enabling the screensaver
						// the thread preventing the screen saver should be suspended
						ScreenSaverInhibitor->Suspend( true );
					}
				}
				else if( !bEnable )
				{
					if( ++ScreenSaverInhibitorSemaphore == 1 )
					{
						// If the semaphore is just becoming one, the thread 
						// is was not running so enable it.
						ScreenSaverInhibitor->Suspend( false );
					}
				}
			}
		}
	}
#endif
}

/**
 * Queue up view "slave" locations to the streaming system. These locations will be added properly at the next call to AddViewInformation,
 * re-using the screensize and FOV settings.
 *
 * @param SlaveLocation			World-space view origin
 * @param BoostFactor			A factor that affects all streaming distances for this location. 1.0f is default. Higher means higher-resolution textures and vice versa.
 * @param bOverrideLocation		Whether this is an override location, which forces the streaming system to ignore all other locations
 * @param OverrideDuration		How long the streaming system should keep checking this location if bOverrideLocation is true, in seconds. 0 means just for the next Tick.
 */
void UEngine::AddTextureStreamingSlaveLoc(FVector InLoc, float BoostFactor, bool bOverrideLocation, float OverrideDuration)
{
	IStreamingManager::Get().AddViewSlaveLocation(InLoc, BoostFactor, bOverrideLocation, OverrideDuration);
}

/** Looks up the GUID of a package on disk. The package must NOT be in the autodownload cache.
 * This may require loading the header of the package in question and is therefore slow.
 */
FGuid UEngine::GetPackageGuid(FName PackageName, bool bForPIE)
{
	FGuid Result(0,0,0,0);

	BeginLoad(*PackageName.ToString());
	uint32 LoadFlags = LOAD_NoWarn | LOAD_NoVerify;
	if (bForPIE)
	{
		LoadFlags |= LOAD_PackageForPIE;
	}
	UPackage* PackageToReset = nullptr;
	FLinkerLoad* Linker = GetPackageLinker(NULL, *PackageName.ToString(), LoadFlags, NULL, NULL);
	if (Linker != NULL && Linker->LinkerRoot != NULL)
	{
		Result = Linker->LinkerRoot->GetGuid();
		PackageToReset = Linker->LinkerRoot;
	}
	EndLoad();
	
	ResetLoaders(PackageToReset);
	Linker = nullptr;

	return Result;
}

/** 
 * Returns whether we are running on a console platform or on the PC.
 *
 * @return true if we're on a console, false if we're running on a PC
 */
bool UEngine::IsConsoleBuild(EConsoleType ConsoleType) const
{
	switch (ConsoleType)
	{
		case CONSOLE_Any:
#if !PLATFORM_DESKTOP
			return true;
#else
			return false;
#endif
		case CONSOLE_Mobile:
			return false;
		default:
			UE_LOG(LogEngine, Warning, TEXT("Unknown ConsoleType passed to IsConsoleBuild()"));
			return false;
	}
}

/**
 *	This function will add a debug message to the onscreen message list.
 *	It will be displayed for FrameCount frames.
 *
 *	@param	Key				A unique key to prevent the same message from being added multiple times.
 *	@param	TimeToDisplay	How long to display the message, in seconds.
 *	@param	DisplayColor	The color to display the text in.
 *	@param	DebugMessage	The message to display.
 */
void UEngine::AddOnScreenDebugMessage(uint64 Key, float TimeToDisplay, FColor DisplayColor, const FString& DebugMessage, bool bNewerOnTop, const FVector2D& TextScale)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bEnableOnScreenDebugMessages == true)
	{
		if (Key == (uint64)-1)
		{
			if (bNewerOnTop)
			{
			FScreenMessageString* NewMessage = new(PriorityScreenMessages)FScreenMessageString();
			check(NewMessage);
			NewMessage->Key = Key;
			NewMessage->ScreenMessage = DebugMessage;
			NewMessage->DisplayColor = DisplayColor;
			NewMessage->TimeToDisplay = TimeToDisplay;
			NewMessage->CurrentTimeDisplayed = 0.0f;
				NewMessage->TextScale = TextScale;
			}
			else
			{
				FScreenMessageString NewMessage;
				NewMessage.CurrentTimeDisplayed = 0.0f;
				NewMessage.Key = Key;
				NewMessage.DisplayColor = DisplayColor;
				NewMessage.TimeToDisplay = TimeToDisplay;
				NewMessage.ScreenMessage = DebugMessage;
				NewMessage.TextScale = TextScale;
				PriorityScreenMessages.Insert(NewMessage, 0);
			}
		}
		else
		{
			FScreenMessageString* Message = ScreenMessages.Find(Key);
			if (Message == NULL)
			{
				FScreenMessageString NewMessage;
				NewMessage.CurrentTimeDisplayed = 0.0f;
				NewMessage.Key = Key;
				NewMessage.DisplayColor = DisplayColor;
				NewMessage.TimeToDisplay = TimeToDisplay;
				NewMessage.ScreenMessage = DebugMessage;
				NewMessage.TextScale = TextScale;
				ScreenMessages.Add((int32)Key, NewMessage);
			}
			else
			{
				// Set the message, and update the time to display and reset the current time.
				Message->ScreenMessage = DebugMessage;
				Message->DisplayColor = DisplayColor;
				Message->TimeToDisplay = TimeToDisplay;
				Message->CurrentTimeDisplayed = 0.0f;
				Message->TextScale = TextScale;
			}
		}
	}
#endif
}

UEngine::FErrorsAndWarningsCollector::FErrorsAndWarningsCollector()
{
}

void UEngine::FErrorsAndWarningsCollector::Initialize()
{
	DisplayTime = 0.0f;
	GConfig->GetFloat(TEXT("/Script/Engine.Engine"), TEXT("DurationOfErrorsAndWarningsOnHUD"), DisplayTime, GEngineIni);

	if (DisplayTime > 0.0f)
	{
		SetVerbosity((GSupressWarningsInOnScreenDisplay != 0) ? ELogVerbosity::Error : ELogVerbosity::Warning);
		TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &UEngine::FErrorsAndWarningsCollector::Tick), DisplayTime);
		FOutputDeviceRedirector::Get()->AddOutputDevice(this);
	}
}

UEngine::FErrorsAndWarningsCollector::~FErrorsAndWarningsCollector()
{
	if (TickerHandle.IsValid())
	{
		FOutputDeviceRedirector::Get()->RemoveOutputDevice(this);
		FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
}

bool UEngine::FErrorsAndWarningsCollector::Tick(float Seconds)
{
	// Set this each tick, in case the cvar is changed at runtime
	SetVerbosity((GSupressWarningsInOnScreenDisplay != 0) ? ELogVerbosity::Error : ELogVerbosity::Warning);

	if (BufferedLines.Num())
	{
		int DupeCount = 0;
		int CurrentHash = 0;

		// Remove any dupes and count them
		do 
		{
			uint32 ThisHash = FCrc::StrCrc32(*BufferedLines[DupeCount].Data);

			if (CurrentHash && ThisHash != CurrentHash)
			{
				break;
			}

			CurrentHash = ThisHash;
			DupeCount++;

		} while (DupeCount < BufferedLines.Num());
		
		// Save off properties
		FString Msg = BufferedLines[0].Data;
		ELogVerbosity::Type Verbosity = BufferedLines[0].Verbosity;

		// Remove any lines we condensed
		BufferedLines.RemoveAt(0, DupeCount);

		uint32* pCount = MessagesToCountMap.Find(CurrentHash);
		
		if (pCount)
		{
			DupeCount += (*pCount);
		}

		MessagesToCountMap.Add(CurrentHash, DupeCount);

		if (DupeCount > 1)
		{
			Msg = FString::Printf(TEXT("%s (x%d)"), *Msg, DupeCount);
		}

		const FColor LineColor = Verbosity <= ELogVerbosity::Error ? FColor::Red : FColor::Yellow;
		GEngine->AddOnScreenDebugMessage(-1, DisplayTime, LineColor, Msg);
	}

	return true;
}

/** Wrapper from int32 to uint64 */
void UEngine::AddOnScreenDebugMessage(int32 Key, float TimeToDisplay, FColor DisplayColor, const FString& DebugMessage, bool bNewerOnTop, const FVector2D& TextScale)
{
	if (bEnableOnScreenDebugMessages == true)
	{
		AddOnScreenDebugMessage((uint64)Key, TimeToDisplay, DisplayColor, DebugMessage, bNewerOnTop, TextScale);
	}
}

bool UEngine::OnScreenDebugMessageExists(uint64 Key)
{
	if (bEnableOnScreenDebugMessages == true)
	{
		if (Key == (uint64)-1)
		{
			// Priority messages assumed to always exist...
			// May want to check for there being none.
			return true;
		}

		if (ScreenMessages.Find(Key) != NULL)
		{
			return true;
		}
	}

	return false;
}

void UEngine::ClearOnScreenDebugMessages()
{
	ScreenMessages.Empty();
	PriorityScreenMessages.Empty();
}

#if !UE_BUILD_SHIPPING
void UEngine::PerformanceCapture(UWorld* World, const FString& MapName, const FString& MatineeName, float EventTime)
{
	// todo
	uint32 t = IStreamingManager::Get().StreamAllResources(5.0f);
	ensure(!t);

	LogPerformanceCapture(World, MapName, MatineeName, EventTime);

	// can be define by command line -BuildName="ByCustomBuildName" or "CL<changelist>"
	FString BuildName = GetBuildNameForPerfTesting();

	// e.g. XboxOne, AllDesktop, Android_.., PS4, HTML5 
	FString PlatformName = FPlatformProperties::PlatformName();
	
	// e.g. D3D11,OpenGL,Vulcan,D3D12
	FString RHIName = TEXT("UnknownRHI");
	{
		// Create the folder name based on the hardware specs we have been provided
		FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();

		FString RHILookup = NAME_RHI.ToString() + TEXT("=");
		if (!FParse::Value(*HardwareDetails, *RHILookup, RHIName))
		{
			// todo error?
		}
	}

	FString CaptureName = FString::Printf(TEXT("Map(%s) Actor(%s) Time(%4.2f)"), *MapName, *GetName(), EventTime);

	FString ScreenshotName = FPaths::AutomationDir() / TEXT("RenderOutputValidation") / BuildName / PlatformName + TEXT("_") + RHIName / CaptureName + TEXT(".png");

	{
		UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
		FConsoleOutputDevice StrOut(ViewportConsole);

		StrOut.Logf(TEXT("  frame:%llu %s"), (uint64)GFrameCounter, *ScreenshotName);
	}

	const bool bShowUI = false;
	const bool bAddFilenameSuffix = false;
	FScreenshotRequest::RequestScreenshot( ScreenshotName, bShowUI, bAddFilenameSuffix );
}

void UEngine::LogPerformanceCapture(UWorld* World, const FString& MapName, const FString& MatineeName, float EventTime)
{
	const FString EventType = TEXT("PERF");
	const int32 ChangeList = FEngineVersion::Current().GetChangelist();

	extern ENGINE_API float GAverageFPS;

	if ((World != nullptr) && (World->GetGameViewport() != nullptr))
	{
		const FStatUnitData* StatUnitData = World->GetGameViewport()->GetStatUnitData();

		FAutomationPerformanceSnapshot PerfSnapshot;
		PerfSnapshot.Changelist = FString::FromInt(ChangeList);
		PerfSnapshot.BuildConfiguration = EBuildConfigurations::ToString(FApp::GetBuildConfiguration());
		PerfSnapshot.MapName = MapName;
		PerfSnapshot.MatineeName = MatineeName;
		PerfSnapshot.AverageFPS = FString::Printf(TEXT("%0.2f"), GAverageFPS);
		PerfSnapshot.AverageFrameTime = FString::Printf(TEXT("%0.2f"), StatUnitData->FrameTime);
		PerfSnapshot.AverageGameThreadTime = FString::Printf(TEXT("%0.2f"), StatUnitData->GameThreadTime);
		PerfSnapshot.AverageRenderThreadTime = FString::Printf(TEXT("%0.2f"), StatUnitData->RenderThreadTime);
		PerfSnapshot.AverageGPUTime = FString::Printf(TEXT("%0.2f"), StatUnitData->GPUFrameTime);
		// PerfSnapshot.PercentOfFramesAtLeast60FPS = ???;	// @todo
		// PerfSnapshot.PercentOfFramesAtLeast60FPS = ???;	// @todo

		const FString PerfSnapshotAsCommaDelimitedString = PerfSnapshot.ToCommaDelimetedString();

		FAutomationTestFramework::Get().AddAnalyticsItemToCurrentTest(
			FString::Printf(TEXT("%s,%s"), *PerfSnapshotAsCommaDelimitedString, *EventType));
	}
}
#endif	// UE_BUILD_SHIPPING

/** Transforms a location in 3D space into 'map space', in 2D */
static FVector2D TransformLocationToMap(FVector2D TopLeftPos, FVector2D BottomRightPos, FVector2D MapOrigin, const FVector2D& MapSize, FVector Loc)
{
	FVector2D MapPos;

	MapPos = MapOrigin;

	MapPos.X += MapSize.X * ((Loc.Y - TopLeftPos.Y)/(BottomRightPos.Y - TopLeftPos.Y));
	MapPos.Y += MapSize.Y * (1.0 - ((Loc.X - BottomRightPos.X)/(TopLeftPos.X - BottomRightPos.X)));	

	return MapPos;
}

/** Utility for drawing a volume geometry (as seen from above) onto the canvas */
static void DrawVolumeOnCanvas(const AVolume* Volume, FCanvas* Canvas, const FVector2D& TopLeftPos, const FVector2D& BottomRightPos, const FVector2D& MapOrigin, const FVector2D& MapSize, const FColor& VolColor)
{
	if(Volume && Volume->GetBrushComponent() && Volume->GetBrushComponent()->BrushBodySetup)
	{
		FTransform BrushTM = Volume->GetBrushComponent()->GetComponentTransform();

		// Iterate over each piece
		for(int32 ConIdx=0; ConIdx<Volume->GetBrushComponent()->BrushBodySetup->AggGeom.ConvexElems.Num(); ConIdx++)
		{
			FKConvexElem& ConvElem = Volume->GetBrushComponent()->BrushBodySetup->AggGeom.ConvexElems[ConIdx];

#if 0 // @todo UE4 physx fix this once we have convexelem drawing again
			// Draw each triangle that makes up the convex hull
			const int32 NumTris = ConvElem.FaceTriData.Num()/3;
			for(int32 i=0; i<NumTris; i++)
			{
				// Get the verts that make up this triangle.
				const int32 I0 = ConvElem.FaceTriData((i*3)+0);
				const int32 I1 = ConvElem.FaceTriData((i*3)+1);
				const int32 I2 = ConvElem.FaceTriData((i*3)+2);

				const FVector V0 = BrushTM.TransformPosition(ConvElem.VertexData(I0));
				const FVector V1 = BrushTM.TransformPosition(ConvElem.VertexData(I1));
				const FVector V2 = BrushTM.TransformPosition(ConvElem.VertexData(I2));

				// We only want to draw faces pointing up
				const FVector Edge0 = V1 - V0;
				const FVector Edge1 = V2 - V1;
				const FVector Normal = (Edge1 ^ Edge0).GetSafeNormal();
				if(Normal.Z > 0.01)
				{
					// Transform as 2d points in 'map space'
					const FVector2D M0 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V0 );
					const FVector2D M1 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V1 );
					const FVector2D M2 = TransformLocationToMap( TopLeftPos, BottomRightPos, MapOrigin, MapSize, V2 );

					// dummy UVs
					FVector2D UVCoords(0,0);
					Canvas->DrawTriangle2D( M0, UVCoords, M1, UVCoords, M2, UVCoords, FLinearColor(VolColor));					

					// Draw edges of face
					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I0) - ConvElem.VertexData(I1)) )
					{
						DrawLine( Canvas, FVector(M0.X,M0.Y,0) , FVector(M1.X,M1.Y,0), VolColor );
					}

					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I1) - ConvElem.VertexData(I2)) )
					{
						DrawLine( Canvas, FVector(M1.X,M1.Y,0) , FVector(M2.X,M2.Y,0), VolColor );
					}

					if( ConvElem.DirIsFaceEdge(ConvElem.VertexData(I2) - ConvElem.VertexData(I0)) )
					{
						DrawLine( Canvas, FVector(M2.X,M2.Y,0) , FVector(M0.X,M0.Y,0), VolColor );
					}
				}
			}
#endif
		}
	}
}

/** Util that takes a 2D vector and rotates it by RotAngle (given in radians) */
static FVector2D RotateVec2D(const FVector2D InVec, float RotAngle)
{
	FVector2D OutVec;
	OutVec.X = (InVec.X * FMath::Cos(RotAngle)) - (InVec.Y * FMath::Sin(RotAngle));
	OutVec.Y = (InVec.X * FMath::Sin(RotAngle)) + (InVec.Y * FMath::Cos(RotAngle));
	return OutVec;
}

#if !UE_BUILD_SHIPPING
bool UEngine::HandleLogoutStatLevelsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	const TArray<FSubLevelStatus> SubLevelsStatusList = GetSubLevelsStatus(InWorld);

	Ar.Logf( TEXT( "Levels:" ) );

	// now draw the "map" name
	if (SubLevelsStatusList.Num())
	{
		// First entry - always persistent level
		FString MapName	= SubLevelsStatusList[0].PackageName.ToString();
		if (SubLevelsStatusList[0].bPlayerInside)
		{
			MapName = *FString::Printf( TEXT("->  %s"), *MapName );
		}
		else
		{
			MapName = *FString::Printf( TEXT("    %s"), *MapName );
		}

		Ar.Logf( TEXT( "%s" ), *MapName );
	}

	// now log the levels
	for (int32 LevelIdx = 1; LevelIdx < SubLevelsStatusList.Num(); ++LevelIdx)
	{
		const FSubLevelStatus& LevelStatus = SubLevelsStatusList[LevelIdx];
		FString DisplayName = LevelStatus.PackageName.ToString();
		FString StatusName;

		switch( LevelStatus.StreamingStatus )
		{
		case LEVEL_Visible:
			StatusName = TEXT( "red loaded and visible" );
			break;
		case LEVEL_MakingVisible:
			StatusName = TEXT( "orange, in process of being made visible" );
			break;
		case LEVEL_Loaded:
			StatusName = TEXT( "yellow loaded but not visible" );
			break;
		case LEVEL_UnloadedButStillAround:
			StatusName = TEXT( "blue  (GC needs to occur to remove this)" );
			break;
		case LEVEL_Unloaded:
			StatusName = TEXT( "green Unloaded" );
			break;
		case LEVEL_Preloading:
			StatusName = TEXT( "purple (preloading)" );
			break;
		default:
			break;
		};

		if (LevelStatus.LODIndex != INDEX_NONE)
		{
			DisplayName += FString::Printf(TEXT(" [LOD%d]"), LevelStatus.LODIndex+1);
		}

		UPackage* LevelPackage = FindObjectFast<UPackage>( NULL, LevelStatus.PackageName );

		if( LevelPackage 
			&& (LevelPackage->GetLoadTime() > 0) 
			&& (LevelStatus.StreamingStatus != LEVEL_Unloaded) )
		{
			DisplayName += FString::Printf(TEXT(" - %4.1f sec"), LevelPackage->GetLoadTime());
		}
		else
		{
			const float LevelLoadPercentage = GetAsyncLoadPercentage(LevelStatus.PackageName);
			if (LevelLoadPercentage >= 0.0f)
			{
				const int32 Percentage = FMath::TruncToInt(LevelLoadPercentage);
				DisplayName += FString::Printf(TEXT(" - %3i %%"), Percentage);
			}
		}

		if ( LevelStatus.bPlayerInside )
		{
			DisplayName = *FString::Printf( TEXT("->  %s"), *DisplayName );
		}
		else
		{
			DisplayName = *FString::Printf( TEXT("    %s"), *DisplayName );
		}

		DisplayName = FString::Printf( TEXT("%s \t\t%s"), *DisplayName, *StatusName );

		Ar.Logf( TEXT( "%s" ), *DisplayName );

	}

	return true;
}
#endif // !UE_BUILD_SHIPPING

/** Helper structure for sorting sounds by predefined criteria. */
struct FSoundInfo
{
	/** Path name to this sound. */
	FString PathName;

	/** Distance betweend a listener and this sound. */
	float Distance;

	/** Sound group this sound belongs to. */
	FName ClassName;

	/** Wave instances currently used by this sound. */
	TArray<FWaveInstance*> WaveInstances;

	FSoundInfo( FString InPathName, float InDistance, FName InClassName )
		: PathName( InPathName )
		, Distance( InDistance )
		, ClassName( InClassName )
	{}

	bool ComparePathNames( const FSoundInfo& Other ) const
	{
		return PathName < Other.PathName;
	}

	bool CompareDistance( const FSoundInfo& Other ) const
	{
		return Distance < Other.Distance;
	}

	bool CompareClass( const FSoundInfo& Other ) const
	{
		return ClassName < Other.ClassName;
	}

	bool CompareWaveInstancesNum( const FSoundInfo& Other ) const
	{
		return Other.WaveInstances.Num() < WaveInstances.Num();
	}
};

struct FCompareFSoundInfoByName
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.ComparePathNames( B ); }
};

struct FCompareFSoundInfoByDistance
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.CompareDistance( B ); }
};

struct FCompareFSoundInfoByClass
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.CompareClass( B ); }
};

struct FCompareFSoundInfoByWaveInstNum
{
	FORCEINLINE bool operator()( const FSoundInfo& A, const FSoundInfo& B ) const { return A.CompareWaveInstancesNum( B ); }
};

/** draws a property of the given object on the screen similarly to stats */
static void DrawProperty(UCanvas* CanvasObject, UObject* Obj, const FDebugDisplayProperty& PropData, UProperty* Prop, int32 X, int32& Y)
{
#if !UE_BUILD_SHIPPING
	checkSlow(PropData.bSpecialProperty || Prop != NULL);
	checkSlow(Prop == NULL || Obj->GetClass()->IsChildOf(Prop->GetOwnerClass()));

	FCanvas* Canvas = CanvasObject->Canvas;
	FString PropText, ValueText;
	if (!PropData.bSpecialProperty)
	{
		if (PropData.WithinClass != NULL)
		{
			PropText = FString::Printf(TEXT("%s.%s.%s.%s = "), *Obj->GetOutermost()->GetName(), *Obj->GetOuter()->GetName(), *Obj->GetName(), *Prop->GetName());
		}
		else
		{
			PropText = FString::Printf(TEXT("%s.%s.%s = "), *Obj->GetOutermost()->GetName(), *Obj->GetName(), *Prop->GetName());
		}
		if (Prop->ArrayDim == 1)
		{
			Prop->ExportText_InContainer(0, ValueText, Obj, Obj, Obj, PPF_IncludeTransient);
		}
		else
		{
			ValueText += TEXT("(");
			for (int32 i = 0; i < Prop->ArrayDim; i++)
			{
				Prop->ExportText_InContainer(i, ValueText, Obj, Obj, Obj, PPF_IncludeTransient);
				if (i + 1 < Prop->ArrayDim)
				{
					ValueText += TEXT(",");
				}
			}
			ValueText += TEXT(")");
		}
	}
	else
	{
		if (PropData.PropertyName == NAME_None)
		{
			if (PropData.WithinClass != NULL)
			{
				PropText = FString::Printf(TEXT("%s.%s.%s"), *Obj->GetOutermost()->GetName(), *Obj->GetOuter()->GetName(), *Obj->GetName());
			}
			else
			{
				PropText = FString::Printf(TEXT("%s.%s"), *Obj->GetOutermost()->GetName(), *Obj->GetName());
			}
			ValueText = TEXT("");
		}
		else
		{
			if (PropData.WithinClass != NULL)
			{
				PropText = FString::Printf(TEXT("%s.%s.%s.(%s) = "), *Obj->GetOutermost()->GetName(), *Obj->GetOuter()->GetName(), *Obj->GetName(), *PropData.PropertyName.ToString());
			}
			else
			{
				PropText = FString::Printf(TEXT("%s.%s.(%s) = "), *Obj->GetOutermost()->GetName(), *Obj->GetName(), *PropData.PropertyName.ToString());
			}

			if (PropData.PropertyName == NAME_Location)
			{
				AActor *Actor = Cast<AActor>(Obj);
				USceneComponent *Component = Cast<USceneComponent>(Obj);
				if (Actor != nullptr)
				{
					ValueText = Actor->GetActorLocation().ToString();
				}
				else if (Component != nullptr)
				{
					ValueText = Component->GetComponentLocation().ToString();
				}
				else
				{
					ValueText = TEXT("Unsupported for this type");
				}
			}
			else if (PropData.PropertyName == NAME_Rotation)
			{
				AActor *Actor = Cast<AActor>(Obj);
				USceneComponent *Component = Cast<USceneComponent>(Obj);
				if (Actor != nullptr)
				{
					ValueText = Actor->GetActorRotation().ToString();
				}
				else if (Component != nullptr)
				{
					ValueText = Component->GetComponentRotation().ToString();
				}
				else
				{
					ValueText = TEXT("Unsupported for this type");
				}
			}
		}
	}

	
	int32 CommaIdx = -1;
	bool bDrawPropName = true;
	do
	{
		FString Str = ValueText;
		CommaIdx = ValueText.Find( TEXT(",") );
		if( CommaIdx >= 0 )
		{
			Str = ValueText.Left(CommaIdx);
			ValueText = ValueText.Mid( CommaIdx+1 );
		}

		int32 XL, YL;
		CanvasObject->ClippedStrLen(GEngine->GetSmallFont(), 1.0f, 1.0f, XL, YL, *PropText);
		FTextSizingParameters DrawParams(X, Y, CanvasObject->SizeX - X, 0, GEngine->GetSmallFont());
		TArray<FWrappedStringElement> TextLines;
		CanvasObject->WrapString(DrawParams, X + XL, *Str, TextLines);
		int32 XL2 = XL;
		if (TextLines.Num() > 0)
		{
			XL2 += FMath::TruncToInt(TextLines[0].LineExtent.X);
			for (int32 i = 1; i < TextLines.Num(); i++)
			{
				XL2 = FMath::Max<int32>(XL2, FMath::TruncToInt(TextLines[i].LineExtent.X));
			}
		}
		Canvas->DrawTile(  X, Y, XL2 + 1, YL * FMath::Max<int>(TextLines.Num(), 1), 0, 0, CanvasObject->DefaultTexture->GetSizeX(), CanvasObject->DefaultTexture->GetSizeY(),
			FLinearColor(0.5f, 0.5f, 0.5f, 0.5f), CanvasObject->DefaultTexture->Resource );
		if( bDrawPropName )
		{
			bDrawPropName = false;
			Canvas->DrawShadowedString( X, Y, *PropText, GEngine->GetSmallFont(), FLinearColor(0.0f, 1.0f, 0.0f));
			if( TextLines.Num() > 1 )
			{
				Y += YL;
			}
		}
		if (TextLines.Num() > 0)
		{
			Canvas->DrawShadowedString( X + XL, Y, *TextLines[0].Value, GEngine->GetSmallFont(), FLinearColor(1.0f, 0.0f, 0.0f));
			for (int32 i = 1; i < TextLines.Num(); i++)
			{
				Canvas->DrawShadowedString( X, Y + YL * i, *TextLines[i].Value, GEngine->GetSmallFont(), FLinearColor(1.0f, 0.0f, 0.0f));
			}
			Y += YL * TextLines.Num();
		}
		else
		{
			Y += YL;
		}
	} while( CommaIdx >= 0 );
#endif // !UE_BUILD_SHIPPING
}

/** Basic timing collation - cannot use stats as these are not enabled in Win32 shipping */
static uint64 StatUnitLastFrameCounter = 0;
static uint32 StatUnitTotalFrameCount = 0;
static float StatUnitTotalFrameTime = 0.0f;
static float StatUnitTotalGameThreadTime = 0.0f;
static float StatUnitTotalRenderThreadTime = 0.0f;
static float StatUnitTotalGPUTime = 0.0f;

void UEngine::GetAverageUnitTimes( TArray<float>& AverageTimes )
{
	uint32 FrameCount = 0;
	AverageTimes.AddZeroed( 4 );

	if( StatUnitTotalFrameCount > 0 )
	{
		AverageTimes[0] = StatUnitTotalFrameTime / StatUnitTotalFrameCount;
		AverageTimes[1] = StatUnitTotalGameThreadTime / StatUnitTotalFrameCount;
		AverageTimes[2] = StatUnitTotalGPUTime / StatUnitTotalFrameCount;
		AverageTimes[3] = StatUnitTotalRenderThreadTime / StatUnitTotalFrameCount;
	}

	/** Reset the counters for the next call */
	StatUnitTotalFrameCount = 0;
	StatUnitTotalFrameTime = 0.0f;
	StatUnitTotalGameThreadTime = 0.0f;
	StatUnitTotalRenderThreadTime = 0.0f;
	StatUnitTotalGPUTime = 0.0f;
}

void UEngine::SetAverageUnitTimes(float FrameTime, float RenderThreadTime, float GameThreadTime, float GPUFrameTime)
{
	/** Only record the information once for the current frame */
	if (StatUnitLastFrameCounter != GFrameCounter)
	{
		StatUnitLastFrameCounter = GFrameCounter;

		/** Total times over a play session for averaging purposes */
		StatUnitTotalFrameCount++;
		StatUnitTotalFrameTime += FrameTime;
		StatUnitTotalRenderThreadTime += RenderThreadTime;
		StatUnitTotalGameThreadTime += GameThreadTime;
		StatUnitTotalGPUTime += GPUFrameTime;
	}
}

FColor UEngine::GetFrameTimeDisplayColor(float FrameTimeMS) const
{
	const float UnacceptableTime = FEnginePerformanceTargets::GetUnacceptableFrameTimeThresholdMS();
	const float TargetTime = FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS();

	return (FrameTimeMS > UnacceptableTime) ? FColor::Red : ((FrameTimeMS > TargetTime) ? FColor::Yellow : FColor::Green);
}

bool UEngine::ShouldThrottleCPUUsage() const
{
	return false;
}

/**
*	Renders warnings about the level that should be addressed prior to shipping
*
*	@param World			The World to render stats about
*	@param Viewport			The viewport to render to
*	@param Canvas			Canvas object to use for rendering
*	@param CanvasObject		Optional canvas object for visualizing properties
*	@param MessageX			X Pos to start drawing at on the Canvas
*	@param MessageY			Y Pos to draw at on the Canvas
*
*	@return The Y position in the canvas after the last drawn string
*/
float DrawMapWarnings(UWorld* World, FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, float MessageX, float MessageY)
{
	FCanvasTextItem SmallTextItem(FVector2D(0, 0), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White);
	SmallTextItem.EnableShadow(FLinearColor::Black);

	const int32 FontSizeY = 20;

	if (GIsTextureMemoryCorrupted)
	{
		FCanvasTextItem TextItem(FVector2D(100, 200), LOCTEXT("OutOfTextureMemory", "RAN OUT OF TEXTURE MEMORY, EXPECT CORRUPTION AND GPU HANGS!"), GEngine->GetMediumFont(), FLinearColor::Red);
		TextItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(TextItem);
	}


	// Put the messages over fairly far to stay in the safe zone on consoles
	if (World->NumLightingUnbuiltObjects > 0)
	{
		SmallTextItem.SetColor(FLinearColor::White);
		// Color unbuilt lighting red if encountered within the last second
		if (FApp::GetCurrentTime() - World->LastTimeUnbuiltLightingWasEncountered < 1)
		{
			SmallTextItem.SetColor(FLinearColor::Red);
		}

		int32 NumLightingScenariosEnabled = 0;

		for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
		{
			ULevel* Level = World->GetLevels()[LevelIndex];

			if (Level->bIsLightingScenario && Level->bIsVisible)
			{
				NumLightingScenariosEnabled++;
			}
		}
		
		if (NumLightingScenariosEnabled > 1)
		{
			SmallTextItem.Text = FText::FromString( FString(TEXT("MULTIPLE LIGHTING SCENARIO LEVELS ENABLED")) );		
		}
		else
		{
			// Use 'DumpUnbuiltLightInteractions' to investigate, if lighting is still unbuilt after a lighting build
			SmallTextItem.Text = FText::FromString( FString::Printf(TEXT("LIGHTING NEEDS TO BE REBUILT (%u unbuilt object(s))"), World->NumLightingUnbuiltObjects) );		
		}

		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}

	// Warn about invalid reflection captures, this can appear only with FeatureLevel < SM4
	if (World->NumInvalidReflectionCaptureComponents > 0)
	{
		SmallTextItem.SetColor(FLinearColor::Red);
		if( World->IsGameWorld())
		{
			SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("INVALID REFLECTION CAPTURES (%u Components, resave map in the editor)"), World->NumInvalidReflectionCaptureComponents));
		}
		else
		{
			SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("REFLECTION CAPTURE UPDATE REQUIRED (%u out-of-date reflection capture(s))"), World->NumInvalidReflectionCaptureComponents));
		}
		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}
	
	// Check HLOD clusters and show warning if unbuilt
#if WITH_EDITOR
	if (World->GetWorldSettings()->bEnableHierarchicalLODSystem)
#endif // WITH_EDITOR
	{
		// Cache so we don't iterate everything in non-editor builds
		static TMap<FString, int32>  WorldUnbuiltHLODMap;

		static double LastCheckTime = 0;
		static int32 UnbuiltLODCount = 0;

		double TimeNow = FPlatformTime::Seconds();

		// Recheck every 20 secs to handle the case where levels may have been
		// Streamed in/out
		if ((TimeNow - LastCheckTime) > 20)
		{
			LastCheckTime = TimeNow;
			UnbuiltLODCount = 0;
			for (TActorIterator<ALODActor> HLODIt(World); HLODIt; ++HLODIt)
			{
				if (!HLODIt->IsBuilt())
				{
					++UnbuiltLODCount;
				}
			}
		}

		if (UnbuiltLODCount)
		{
			SmallTextItem.SetColor(FLinearColor::Red);
			SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("HLOD CLUSTER(S) NEED TO BE REBUILT (%u unbuilt object(s))"), UnbuiltLODCount));
			Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
			MessageY += FontSizeY;
		}
	}

	if (World->NumTextureStreamingUnbuiltComponents > 0 || World->NumTextureStreamingDirtyResources > 0)
	{
		SmallTextItem.SetColor(FLinearColor::Red);
		SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("TEXTURE STREAMING NEEDS TO BE REBUILT (%u Components, %u Resource Refs)"), World->NumTextureStreamingUnbuiltComponents, World->NumTextureStreamingDirtyResources));
		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}

	if (FPlatformProperties::SupportsTextureStreaming() && IStreamingManager::Get().IsTextureStreamingEnabled())
	{
		auto MemOver = IStreamingManager::Get().GetTextureStreamingManager().GetMemoryOverBudget();
		if (MemOver > 0)
		{
			SmallTextItem.SetColor(FLinearColor::Red);
			SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("TEXTURE STREAMING POOL OVER %0.2f MB"), (float)MemOver / 1024.0f / 1024.0f));
			Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
			MessageY += FontSizeY;
		}
	
	}

	// check navmesh
#if WITH_EDITOR
	const bool bIsNavigationAutoUpdateEnabled = UNavigationSystem::GetIsNavigationAutoUpdateEnabled();
#else
	const bool bIsNavigationAutoUpdateEnabled = true;
#endif
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys && NavSys->IsNavigationDirty() &&
		(!bIsNavigationAutoUpdateEnabled || !NavSys->SupportsNavigationGeneration() || !NavSys->CanRebuildDirtyNavigation()))
	{
		SmallTextItem.SetColor(FLinearColor::White);
		SmallTextItem.Text = LOCTEXT("NAVMESHERROR", "NAVMESH NEEDS TO BE REBUILT");
		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}

	if (World->bKismetScriptError)
	{
		SmallTextItem.Text = LOCTEXT("BlueprintInLevelHadCompileErrorMessage", "BLUEPRINT COMPILE ERROR");
		SmallTextItem.SetColor(FLinearColor::Red);
		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}

	SmallTextItem.SetColor(FLinearColor::White);

	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		SmallTextItem.Text = FText::FromString(FString::Printf(TEXT("Shaders Compiling (%u)"), GShaderCompilingManager->GetNumRemainingJobs()));
		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}

	/* @todo ue4 temporarily disabled
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if( !WorldSettings->IsNavigationRebuilt() )
	{
	DrawShadowedString(Canvas,
	MessageX,
	MessageY,
	TEXT("PATHS NEED TO BE REBUILT"),
	GEngine->GetSmallFont(),
	FColor(128,128,128)
	);
	MessageY += FontSizeY;
	}
	*/

	if (World->bIsLevelStreamingFrozen)
	{
		SmallTextItem.Text = LOCTEXT("Levelstreamingfrozen", "Level streaming frozen...");
		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	extern bool GIsPrepareMapChangeBroken;
	if (GIsPrepareMapChangeBroken)
	{
		SmallTextItem.Text = LOCTEXT("PrepareMapChangeError", "PrepareMapChange had a bad level name! Check the log (tagged with PREPAREMAPCHANGE) for info");
		Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
		MessageY += FontSizeY;
	}
#endif

	// ask any delegates for extra messages
	if (FCoreDelegates::OnGetOnScreenMessages.IsBound())
	{
		FCoreDelegates::FSeverityMessageMap ExtraMessages;
		FCoreDelegates::OnGetOnScreenMessages.Broadcast(ExtraMessages);

		// draw them all!
		for (auto It = ExtraMessages.CreateConstIterator(); It; ++It)
		{
			SmallTextItem.Text = It.Value();
			switch (It.Key())
			{
				case FCoreDelegates::EOnScreenMessageSeverity::Info:
					SmallTextItem.SetColor(FLinearColor::White);
					break;
				case FCoreDelegates::EOnScreenMessageSeverity::Warning:
					SmallTextItem.SetColor(FLinearColor::Yellow);
					break;
				case FCoreDelegates::EOnScreenMessageSeverity::Error:
					SmallTextItem.SetColor(FLinearColor::Red);
					break;
			}

			Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
			MessageY += FontSizeY;
		}
	}


	return MessageY;
}

/**
*	Renders warnings about the level that should be addressed prior to shipping
*
*	@param World			The World to render stats about
*	@param Viewport			The viewport to render to
*	@param Canvas			Canvas object to use for rendering
*	@param CanvasObject		Optional canvas object for visualizing properties
*	@param MessageX			X Pos to start drawing at on the Canvas
*	@param MessageY			Y Pos to draw at on the Canvas
*
*	@return The Y position in the canvas after the last drawn string
*/
float DrawOnscreenDebugMessages(UWorld* World, FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, float MessageX, float MessageY)
{
	int32 YPos = MessageY;
	const int32 MaxYPos = CanvasObject ? CanvasObject->SizeY : 700;
	if (GEngine->PriorityScreenMessages.Num() > 0)
	{
		FCanvasTextItem MessageTextItem(FVector2D(0, 0), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White);
		MessageTextItem.EnableShadow(FLinearColor::Black);
		for (int32 PrioIndex = GEngine->PriorityScreenMessages.Num() - 1; PrioIndex >= 0; PrioIndex--)
		{
			FScreenMessageString& Message = GEngine->PriorityScreenMessages[PrioIndex];
			if (YPos < MaxYPos)
			{
				MessageTextItem.Text = FText::FromString(Message.ScreenMessage);
				MessageTextItem.SetColor(Message.DisplayColor);
				MessageTextItem.Scale = Message.TextScale;
				Canvas->DrawItem(MessageTextItem, FVector2D(MessageX, YPos));
				YPos += MessageTextItem.DrawnSize.Y * 1.15f;
			}
			Message.CurrentTimeDisplayed += World->GetDeltaSeconds();
			if (Message.CurrentTimeDisplayed >= Message.TimeToDisplay)
			{
				GEngine->PriorityScreenMessages.RemoveAt(PrioIndex);
			}
		}
	}

	if (GEngine->ScreenMessages.Num() > 0)
	{
		FCanvasTextItem MessageTextItem(FVector2D(0, 0), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White);
		MessageTextItem.EnableShadow(FLinearColor::Black);
		for (TMap<int32, FScreenMessageString>::TIterator MsgIt(GEngine->ScreenMessages); MsgIt; ++MsgIt)
		{
			FScreenMessageString& Message = MsgIt.Value();
			if (YPos < MaxYPos)
			{
				MessageTextItem.Text = FText::FromString(Message.ScreenMessage);
				MessageTextItem.SetColor(Message.DisplayColor);
				MessageTextItem.Scale = Message.TextScale;
				Canvas->DrawItem(MessageTextItem, FVector2D(MessageX, YPos));
				YPos += MessageTextItem.DrawnSize.Y * 1.15f;
			}
			Message.CurrentTimeDisplayed += World->GetDeltaSeconds();
			if (Message.CurrentTimeDisplayed >= Message.TimeToDisplay)
			{
				MsgIt.RemoveCurrent();
			}
		}
	}

	return MessageY;
}


/**
 *	Renders stats
 *
 *  @param World			The World to render stats about
 *	@param Viewport			The viewport to render to
 *	@param Canvas			Canvas object to use for rendering
 *	@param CanvasObject		Optional canvas object for visualizing properties
 *	@param DebugProperties	List of properties to visualize (in/out)
 *	@param ViewLocation		Location of camera
 *	@param ViewRotation		Rotation of camera
 */
void DrawStatsHUD( UWorld* World, FViewport* Viewport, FCanvas* Canvas, UCanvas* CanvasObject, TArray<FDebugDisplayProperty>& DebugProperties, const FVector& ViewLocation, const FRotator& ViewRotation )
{
	LLM_SCOPE(ELLMTag::Stats);

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "DrawStatsHUD" ), STAT_DrawStatsHUD, STATGROUP_StatSystem );

	// We cannot draw without a canvas
	if (Canvas == NULL)
	{
		return;
	}

	//@todo joeg: Move this stuff to a function, make safe to use on consoles by
	// respecting the various safe zones, and make it compile out.
	const int32 FPSXOffset	= (GEngine->IsStereoscopic3D(Viewport)) ? Viewport->GetSizeXY().X * 0.5f * 0.334f : (FPlatformProperties::SupportsWindowedMode() ? 110 : 250);
	const int32 StatsXOffset = 100;// FPlatformProperties::SupportsWindowedMode() ? 4 : 100;

	static const int32 MessageStartY = GIsEditor ? 35 : 100; // Account for safe frame
	int32 MessageY = MessageStartY;

	// This is the percentage of the screen that a single line of stats should take up.
	float TextScale = CVarDebugTextScale.GetValueOnAnyThread();
	const FVector2D FontScale = FVector2D(TextScale, TextScale);
	const int32 FontSizeY = 20 * FontScale.X;
#if !UE_BUILD_SHIPPING
	if (!GIsHighResScreenshot && !GIsDumpingMovie && GAreScreenMessagesEnabled)
	{
		const int32 MessageX = (GEngine->IsStereoscopic3D(Viewport)) ? Viewport->GetSizeXY().X * 0.5f * 0.3f : 40;
		
		FCanvasTextItem SmallTextItem(FVector2D(0, 0), FText::GetEmpty(), GEngine->GetSmallFont(), FLinearColor::White);
		SmallTextItem.Scale = FontScale;
		SmallTextItem.EnableShadow(FLinearColor::Black);

		// Draw map warnings?
		if (!GEngine->bSuppressMapWarnings)
		{
			MessageY = DrawMapWarnings(World, Viewport, Canvas, CanvasObject, MessageX, MessageY);
		}

#if ENABLE_VISUAL_LOG
		if (FVisualLogger::Get().IsRecording() || FVisualLogger::Get().IsRecordingOnServer())
		{
			int32 XSize;
			int32 YSize;
			FString String = FString::Printf(TEXT("VisLog recording active"));
			StringSize(GEngine->GetSmallFont(), XSize, YSize, *String);

			SmallTextItem.Position = FVector2D((int32)Viewport->GetSizeXY().X - XSize - 16, 36);
			SmallTextItem.Text = FText::FromString(String);
			SmallTextItem.SetColor(FLinearColor::Red);
			SmallTextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(SmallTextItem);
			SmallTextItem.SetColor(FLinearColor::White);
		}
#endif

#if STATS
		if (FThreadStats::IsCollectingData())
		{
			SmallTextItem.SetColor(FLinearColor::Red);
			if (!GEngine->bDisableAILogging)
			{
				SmallTextItem.Text = LOCTEXT("AIPROFILINGWARNING", "PROFILING WITH AI LOGGING ON!");
				Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
				MessageY += FontSizeY;
			}
			if (GShouldVerifyGCAssumptions)
			{
				SmallTextItem.Text = LOCTEXT("GCPROFILINGWARNING", "PROFILING WITH GC VERIFY ON!");
				Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
				MessageY += FontSizeY;
			}

			const bool bIsStatsFileActive = FCommandStatsFile::Get().IsStatFileActive();
			if (bIsStatsFileActive)
			{
				SmallTextItem.SetColor(FLinearColor::White);
				SmallTextItem.Text = FCommandStatsFile::Get().GetFileMetaDesc();
				Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
				MessageY += FontSizeY;
			}

#if ENABLE_LOW_LEVEL_MEM_TRACKER
			if (FLowLevelMemTracker::Get().IsEnabled() && !FPlatformMemory::IsDebugMemoryEnabled())
			{
				SmallTextItem.Text = LOCTEXT("MEMPROFILINGWARNINGLLM", "LLM enabled without Debug Memory enabled!");
				Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
				MessageY += FontSizeY;
			}
#endif
		}
#endif // STATS

		// Only output disable message if there actually were any
		if (MessageY != MessageStartY)
		{
			SmallTextItem.SetColor(FLinearColor(.05f, .05f, .05f, .2f));
			SmallTextItem.Text = FText::FromString(FString(TEXT("'DisableAllScreenMessages' to suppress")));
			Canvas->DrawItem(SmallTextItem, FVector2D(MessageX + 50, MessageY));
			MessageY += 16;
		}

#if !(UE_BUILD_TEST)
		if (GEngine->bEnableOnScreenDebugMessagesDisplay && GEngine->bEnableOnScreenDebugMessages)
		{
			MessageY = DrawOnscreenDebugMessages(World, Viewport, Canvas, CanvasObject, MessageX, MessageY);
		}
#endif // UE_BUILD_TEST

		if (FPlatformMemory::IsDebugMemoryEnabled())
		{
			SmallTextItem.Text = LOCTEXT("MEMPROFILINGWARNING", "WARNING: Running with Debug Memory Enabled!");
			Canvas->DrawItem(SmallTextItem, FVector2D(MessageX, MessageY));
			MessageY += FontSizeY;
		}
	}
#endif // UE_BUILD_SHIPPING 

	{
		int32 X = (CanvasObject) ? CanvasObject->SizeX - FPSXOffset : Viewport->GetSizeXY().X - FPSXOffset; //??
		int32 Y = (GEngine->IsStereoscopic3D(Viewport)) ? FMath::TruncToInt(Viewport->GetSizeXY().Y * 0.40f) : FMath::TruncToInt(Viewport->GetSizeXY().Y * 0.20f);

		// give the viewport first shot at drawing stats
		Y = Viewport->DrawStatsHUD(Canvas, X, Y);

		// Render all the simple stats
		GEngine->RenderEngineStats(World, Viewport, Canvas, StatsXOffset, MessageY, X, Y, &ViewLocation, &ViewRotation);

		// @third party code - BEGIN HairWorks
#if STATS
		// Render HairWorks stats
		::HairWorks::RenderStats(X, Y, Canvas);
#endif
		// @third party code - END HairWorks

#if STATS
		extern void RenderStats(FViewport* Viewport, class FCanvas* Canvas, int32 X, int32 Y, int32 SizeX, const float TextScale);
		RenderStats( Viewport, Canvas, StatsXOffset, Y, CanvasObject != nullptr ? CanvasObject->CachedDisplayWidth - CanvasObject->SafeZonePadX * 2 : Viewport->GetSizeXY().X, TextScale);
#endif
	}

	// draw debug properties
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if (UE_BUILD_SHIPPING && WITH_EDITOR)
	if (GEngine != NULL && World->GetNetMode() == NM_Standalone && CanvasObject != NULL)
#endif
	{
		// construct a list of objects relevant to "getall" type elements, so that we only have to do the object iterator once
		// we do the iterator each frame so that new objects will show up immediately
		struct FDebugClass
		{
			UClass* Class;
			UClass* WithinClass;
			FDebugClass(UClass* InClass, UClass* InWithinClass)
				: Class(InClass), WithinClass(InWithinClass)
			{}
		};
		TArray<FDebugClass> DebugClasses;
		DebugClasses.Reserve(DebugProperties.Num());
		for (int32 i = 0; i < DebugProperties.Num(); i++)
		{
			if (DebugProperties[i].Obj != NULL && !DebugProperties[i].Obj->IsPendingKill())
			{
				UClass* Cls = Cast<UClass>(DebugProperties[i].Obj);
				if (Cls != NULL)
				{
					new(DebugClasses) FDebugClass(Cls, DebugProperties[i].WithinClass);
				}
			}
			else
			{
				// invalid, object was destroyed, etc. so remove the entry
				DebugProperties.RemoveAt(i--, 1);
			}
		}

		TSet<UObject*> RelevantObjects;
		for (const FDebugClass& DebugClass : DebugClasses)
		{
			if (DebugClass.Class)
		{
				TArray<UObject*> DebugObjectsOfClass;
				const bool bIncludeDerivedClasses = true;
				GetObjectsOfClass(DebugClass.Class, DebugObjectsOfClass, bIncludeDerivedClasses);
				for (UObject* Obj : DebugObjectsOfClass)
			{
					if (!Obj)
				{
					continue;
				}

					if (Obj->GetWorld() && Obj->GetWorld() != World)
				{
						continue;
					}

					if ( !Obj->IsTemplate() &&
						(DebugClass.WithinClass == NULL || (Obj->GetOuter() != NULL && Obj->GetOuter()->GetClass()->IsChildOf(DebugClass.WithinClass))) )
					{
						RelevantObjects.Add(Obj);
					}
				}
			}
		}
		// draw starting in the top left
		int32 X = StatsXOffset;
		int32 Y = FPlatformProperties::SupportsWindowedMode() ? 20 : 40;
		int32 MaxY = int32(Canvas->GetRenderTarget()->GetSizeXY().Y);
		for (int32 i = 0; i < DebugProperties.Num() && Y < MaxY; i++)
		{
			// we removed entries with invalid Obj above so no need to check for that here
			UClass* Cls = Cast<UClass>(DebugProperties[i].Obj);
			if (Cls != NULL)
			{
				UProperty* Prop = FindField<UProperty>(Cls, DebugProperties[i].PropertyName);
				if (Prop != NULL || DebugProperties[i].bSpecialProperty)
				{
					// getall
					for (UObject* RelevantObject : RelevantObjects)
					{
						if ( RelevantObject->IsA(Cls) && !RelevantObject->IsPendingKill() &&
							(DebugProperties[i].WithinClass == NULL || (RelevantObject->GetOuter() != NULL && RelevantObject->GetOuter()->GetClass()->IsChildOf(DebugProperties[i].WithinClass))) )
						{
							DrawProperty(CanvasObject, RelevantObject, DebugProperties[i], Prop, X, Y);
						}
					}
				}
				else
				{
					// invalid entry
					DebugProperties.RemoveAt(i--, 1);
				}
			}
			else
			{
				UProperty* Prop = FindField<UProperty>(DebugProperties[i].Obj->GetClass(), DebugProperties[i].PropertyName);
				if (Prop != NULL || DebugProperties[i].bSpecialProperty)
				{
					DrawProperty(CanvasObject, DebugProperties[i].Obj, DebugProperties[i], Prop, X, Y);
				}
				else
				{
					DebugProperties.RemoveAt(i--, 1);
				}
			}
		}
	}
#endif
}


/**
 * Stats objects for Engine
 */
DEFINE_STAT(STAT_GameEngineTick);
DEFINE_STAT(STAT_GameViewportTick);
DEFINE_STAT(STAT_RedrawViewports);
DEFINE_STAT(STAT_UpdateLevelStreaming);
DEFINE_STAT(STAT_RHITickTime);
DEFINE_STAT(STAT_IntentionalHitch);
DEFINE_STAT(STAT_FrameSyncTime);
DEFINE_STAT(STAT_DeferredTickTime);

/** Input stat */
DEFINE_STAT(STAT_InputTime);
DEFINE_STAT(STAT_InputLatencyTime);

/** HUD stat */
DEFINE_STAT(STAT_HudTime);

/** Static mesh tris rendered */
DEFINE_STAT(STAT_StaticMeshTriangles);

/** Skeletal stats */
DEFINE_STAT(STAT_SkinningTime);
DEFINE_STAT(STAT_UpdateClothVertsTime);
DEFINE_STAT(STAT_UpdateSoftBodyVertsTime);
DEFINE_STAT(STAT_SkelMeshTriangles);
DEFINE_STAT(STAT_SkelMeshDrawCalls);
DEFINE_STAT(STAT_CPUSkinVertices);
DEFINE_STAT(STAT_GPUSkinVertices);

/** Unit times */
DEFINE_STAT(STAT_UnitFrame);
DEFINE_STAT(STAT_UnitGame);
DEFINE_STAT(STAT_UnitRender);
DEFINE_STAT(STAT_UnitGPU);

/*-----------------------------------------------------------------------------
	Lightmass object/actor implementations.
-----------------------------------------------------------------------------*/






/*-----------------------------------------------------------------------------
	ULightmappedSurfaceCollection
-----------------------------------------------------------------------------*/

UFont* GetStatsFont()
{
	return GEngine->GetSmallFont();
}


/**
 * Syncs the game thread with the render thread. Depending on passed in bool this will be a total
 * sync or a one frame lag.
 */
void FFrameEndSync::Sync( bool bAllowOneFrameThreadLag )
{
	check(IsInGameThread());			

	Fence[EventIndex].BeginFence();

	bool bEmptyGameThreadTasks = !FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread);

	if (bEmptyGameThreadTasks)
	{
		// need to process gamethread tasks at least once a frame no matter what
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}

	// Use two events if we allow a one frame lag.
	if( bAllowOneFrameThreadLag )
	{
		EventIndex = (EventIndex + 1) % 2;
	}

	Fence[EventIndex].Wait(bEmptyGameThreadTasks);  // here we also opportunistically execute game thread tasks while we wait

}

FString appGetStartupMap(const TCHAR* CommandLine)
{
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), GGameIni );

	// convert commandline to a URL
	FString Error;
	TCHAR Parm[4096]=TEXT("");

#if UE_BUILD_SHIPPING
	// In shipping don't allow an override
	CommandLine = NULL;
#endif // UE_BUILD_SHIPPING

	const TCHAR* Tmp = CommandLine ? CommandLine : TEXT("");
	if (!FParse::Token(Tmp, Parm, ARRAY_COUNT(Parm), 0) || Parm[0] == '-')
	{
		const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
		FCString::Strcpy(Parm, *(GameMapsSettings->GetGameDefaultMap() + GameMapsSettings->LocalMapOptions));
	}
	FURL URL(&DefaultURL, Parm, TRAVEL_Partial);

	// strip off extension of the map if there is one
	return FPaths::GetBaseFilename(URL.Map);
}

void appGetAllPotentialStartupPackageNames(TArray<FString>& PackageNames, const FString& EngineConfigFilename, bool bIsCreatingHashes)
{
	// startup packages from .ini
	FStartupPackages::GetStartupPackageNames(PackageNames, EngineConfigFilename, bIsCreatingHashes);

	// add the startup map
	PackageNames.Add(*appGetStartupMap(NULL));

	//@todo-packageloc Handle localized packages.
}

#if WITH_EDITOR
FOnSwitchWorldForPIE FScopedConditionalWorldSwitcher::SwitchWorldForPIEDelegate;

FScopedConditionalWorldSwitcher::FScopedConditionalWorldSwitcher( FViewportClient* InViewportClient )
	: ViewportClient( InViewportClient )
	, OldWorld( NULL )
{
	if( GIsEditor )
	{
		if( ViewportClient && ViewportClient == GEngine->GameViewport && !GIsPlayInEditorWorld )
		{
			OldWorld = GWorld; 
			const bool bSwitchToPIEWorld = true;
			// Delegate must be valid
			SwitchWorldForPIEDelegate.ExecuteIfBound( bSwitchToPIEWorld );
		} 
		else if( ViewportClient )
		{
			// Tell the viewport client to set the correct world and store what the world used to be
			OldWorld = ViewportClient->ConditionalSetWorld();
		}
	}
}

FScopedConditionalWorldSwitcher::~FScopedConditionalWorldSwitcher()
{
	// Only switch in the editor and if we made a swtich (OldWorld not null)
	if( GIsEditor && OldWorld )
	{
		if( ViewportClient && ViewportClient == GEngine->GameViewport && GIsPlayInEditorWorld )
		{
			const bool bSwitchToPIEWorld = false;
			// Delegate must be valid
			SwitchWorldForPIEDelegate.ExecuteIfBound( bSwitchToPIEWorld );
		} 
		else if( ViewportClient )
		{
			// Tell the viewport client to restore the old world
			ViewportClient->ConditionalRestoreWorld( OldWorld );
		}
	}
}

#endif

void UEngine::OverrideSelectedMaterialColor( const FLinearColor& OverrideColor )
{
	bIsOverridingSelectedColor = true;
	SelectedMaterialColorOverride = OverrideColor;
}

void UEngine::RestoreSelectedMaterialColor()
{
	bIsOverridingSelectedColor = false;
}

void UEngine::WorldAdded( UWorld* InWorld )
{
	WorldAddedEvent.Broadcast( InWorld );
}

void UEngine::WorldDestroyed( UWorld* InWorld )
{
	WorldDestroyedEvent.Broadcast( InWorld );
}

UWorld* UEngine::GetWorldFromContextObject(const UObject* Object, EGetWorldErrorMode ErrorMode) const
{
	if (Object == nullptr)
	{
		switch (ErrorMode)
		{
		case EGetWorldErrorMode::Assert:
			check(Object);
			break;
		case EGetWorldErrorMode::LogAndReturnNull:
			FFrame::KismetExecutionMessage(TEXT("A null object was passed as a world context object to UEngine::GetWorldFromContextObject()."), ELogVerbosity::Error);
			//UE_LOG(LogEngine, Warning, TEXT("UEngine::GetWorldFromContextObject() passed a nullptr"));
			break;
		case EGetWorldErrorMode::ReturnNull:
			break;
		}
		return nullptr;
	}

	bool bSupported = true;
	UWorld* World = (ErrorMode == EGetWorldErrorMode::Assert) ? Object->GetWorldChecked(/*out*/ bSupported) : Object->GetWorld();
	if (bSupported && (World == nullptr) && (ErrorMode == EGetWorldErrorMode::LogAndReturnNull))
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("No world was found for object (%s) passed in to UEngine::GetWorldFromContextObject()."), *GetPathNameSafe(Object)), ELogVerbosity::Error);
	}
	return (bSupported ? World : GWorld);
}

TArray<class ULocalPlayer*>::TConstIterator	UEngine::GetLocalPlayerIterator(UWorld* World)
{
	return GetGamePlayers(World).CreateConstIterator();
}

TArray<class ULocalPlayer*>::TConstIterator UEngine::GetLocalPlayerIterator(const UGameViewportClient *Viewport)
{
	return GetGamePlayers(Viewport).CreateConstIterator();
}

static TArray<ULocalPlayer*> FakeEmptyLocalPlayers;

const TArray<class ULocalPlayer*>& HandleFakeLocalPlayersList()
{
#if 0
	if (!IsRunningCommandlet())
	{
		UE_LOG(LogLoad, Error, TEXT("WorldContext requested with NULL game instance object.") );
		check(false);
	}
#endif
	check(FakeEmptyLocalPlayers.Num() == 0);
	return FakeEmptyLocalPlayers;
}

const TArray<class ULocalPlayer*>& UEngine::GetGamePlayers(UWorld *World) const
{
	const FWorldContext &Context = GetWorldContextFromWorldChecked(World);
	if ( Context.OwningGameInstance == NULL )
	{
		return HandleFakeLocalPlayersList();
	}
	return Context.OwningGameInstance->GetLocalPlayers();
}
	
const TArray<class ULocalPlayer*>& UEngine::GetGamePlayers(const UGameViewportClient *Viewport) const
{
	const FWorldContext &Context = GetWorldContextFromGameViewportChecked(Viewport);
	if ( Context.OwningGameInstance == NULL )
	{
		return HandleFakeLocalPlayersList();
	}
	return Context.OwningGameInstance->GetLocalPlayers();
}

ULocalPlayer* UEngine::FindFirstLocalPlayerFromControllerId(int32 ControllerId) const
{
	for (auto It=WorldList.CreateConstIterator(); It; ++It)
	{
		const FWorldContext &Context = *It;
		if (Context.World() && Context.OwningGameInstance && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
		{
			const TArray<class ULocalPlayer*> & LocalPlayers = Context.OwningGameInstance->GetLocalPlayers();

			// Use this world context, look for the ULocalPlayer with this ControllerId
			for (ULocalPlayer* LocalPlayer : LocalPlayers)
			{
				if (LocalPlayer && LocalPlayer->GetControllerId() == ControllerId)
				{
					return LocalPlayer;
				}
			}
		}
	}

	return nullptr;
}

int32 UEngine::GetNumGamePlayers(UWorld *InWorld)
{
	return GetGamePlayers(InWorld).Num();
}

int32 UEngine::GetNumGamePlayers(const UGameViewportClient *InViewport)
{
	return GetGamePlayers(InViewport).Num();
}

ULocalPlayer* UEngine::GetGamePlayer( UWorld * InWorld, int32 InPlayer )
{
	const TArray<class ULocalPlayer*>& PlayerList = GetGamePlayers(InWorld);
	check( InPlayer < PlayerList.Num() );
	return PlayerList[ InPlayer ];
}

ULocalPlayer* UEngine::GetGamePlayer( const UGameViewportClient* InViewport, int32 InPlayer )
{
	const TArray<class ULocalPlayer*>& PlayerList = GetGamePlayers(InViewport);
	check( InPlayer < PlayerList.Num() );
	return PlayerList[ InPlayer ];
}
	
ULocalPlayer* UEngine::GetFirstGamePlayer(UWorld *InWorld)
{
	const TArray<class ULocalPlayer*>& PlayerList = GetGamePlayers(InWorld);
	return PlayerList.Num() != 0 ? PlayerList[0] : NULL;
}

ULocalPlayer* UEngine::GetFirstGamePlayer( UPendingNetGame *PendingNetGame )
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		if (It->PendingNetGame == PendingNetGame)
		{
			return ( It->OwningGameInstance != NULL ) ? It->OwningGameInstance->GetFirstGamePlayer() : NULL;
		}
	}
	return NULL;
}

ULocalPlayer* UEngine::GetFirstGamePlayer(const UGameViewportClient *InViewport )
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		if (It->GameViewport == InViewport)
		{
			return ( It->OwningGameInstance != NULL ) ? It->OwningGameInstance->GetFirstGamePlayer() : NULL;
		}
	}
	return NULL;
}

ULocalPlayer* UEngine::GetDebugLocalPlayer()
{
	for (auto It = WorldList.CreateConstIterator(); It; ++It)
	{
		if (It->OwningGameInstance != NULL && It->OwningGameInstance->GetFirstGamePlayer() != NULL )
		{
			return It->OwningGameInstance->GetFirstGamePlayer();
		}
	}
	return NULL;
}

#if !UE_BUILD_SHIPPING
// Moved this class from Class.cpp because FExportObjectInnerContext is defined in Engine
static class FCDODump : private FSelfRegisteringExec
{
	static FString ObjectString(UObject* Object)
	{
		UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

		FStringOutputDevice Archive;
		const FExportObjectInnerContext Context;
		UExporter::ExportToOutputDevice(&Context, Object, NULL, Archive, TEXT("copy"), 0, PPF_Copy | PPF_DebugDump, false);
		Archive.Log(TEXT("\r\n\r\n"));

		FString ExportedText;
		ExportedText = Archive;
		return ExportedText;
	}

	/** Console commands, see embeded usage statement **/
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if(FParse::Command(&Cmd,TEXT("CDODump")))
		{
			FString All;
			TArray<UClass*> Classes;
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* Cls = *ClassIt;
				if (!Cls->IsChildOf(UClass::StaticClass()) && (Cls != UObject::StaticClass()) && (Cls->GetName() != TEXT("World")) && (Cls->GetName() != TEXT("Level")))
				{
					Classes.Add(Cls);
				}
			}
			Classes.Sort();

			for (int32 Index = 0; Index < Classes.Num(); Index++)
			{
				All += ObjectString(Classes[Index]->GetDefaultObject());
			}
			FString Filename = FPaths::ProjectSavedDir() / TEXT("CDO.txt");
			verify(FFileHelper::SaveStringToFile(All, *Filename));
			return true;
		}
		return false;
	}
} CDODump;
#endif // !UE_BUILD_SHIPPING

void UEngine::ShutdownWorldNetDriver( UWorld * World )
{
	if (World)
	{
		/**
		 * Shut down the world's net driver, completely disconnecting any clients/servers connected 
		 * at the time.   Destroys the net driver.
		 */
		UNetDriver* NetDriver = World->GetNetDriver();
		if (NetDriver)
		{
			UE_LOG(LogNet, Log, TEXT("World NetDriver shutdown %s [%s]"), *NetDriver->GetName(), *NetDriver->NetDriverName.ToString());
			World->SetNetDriver(NULL);
			DestroyNamedNetDriver(World, NetDriver->NetDriverName);
		}

		// Take care of the demo net driver specifically (so the world can clear the DemoNetDriver property)
		World->DestroyDemoNetDriver();

		// Also disconnect any net drivers that have this set as their world, to avoid GC issues
		FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(World);

		for (int32 Index = 0; Index < Context.ActiveNetDrivers.Num(); Index++)
		{
			NetDriver = Context.ActiveNetDrivers[Index].NetDriver;
			if (NetDriver && NetDriver->GetWorld() == World)
			{
				UE_LOG(LogNet, Log, TEXT("World NetDriver shutdown %s [%s]"), *NetDriver->GetName(), *NetDriver->NetDriverName.ToString());
				DestroyNamedNetDriver(World, NetDriver->NetDriverName);
				Index--;
			}
		}
	}
}

void UEngine::ShutdownAllNetDrivers()
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		TArray<FNamedNetDriver> & ActiveNetDrivers = It->ActiveNetDrivers;

		for (int32 Index = 0; Index < ActiveNetDrivers.Num(); Index++)
		{
			FNamedNetDriver& NamedNetDriver = ActiveNetDrivers[Index];
			UNetDriver* NetDriver = NamedNetDriver.NetDriver;
			if (NetDriver)
			{
				UE_LOG(LogNet, Log, TEXT("World NetDriver shutdown %s [%s]"), *NetDriver->GetName(), *NetDriver->NetDriverName.ToString());
				UWorld * World = NetDriver->GetWorld();
				if (World)
				{
					World->SetNetDriver(NULL);
				}
				NetDriver->SetWorld(NULL);
				DestroyNamedNetDriver(It->World(), NetDriver->NetDriverName);
				Index--;
			}
		}

		ActiveNetDrivers.Empty();
	}
}

UNetDriver* FindNamedNetDriver_Local(const TArray<FNamedNetDriver>& ActiveNetDrivers, FName NetDriverName)
{
	for (int32 Index = 0; Index < ActiveNetDrivers.Num(); Index++)
	{
		const FNamedNetDriver& NamedNetDriver = ActiveNetDrivers[Index];
		UNetDriver* NetDriver = NamedNetDriver.NetDriver;
		if (NetDriver && NetDriver->NetDriverName == NetDriverName)
		{
			return NetDriver;
		}
	}
	return NULL;
}

UNetDriver* UEngine::FindNamedNetDriver(UWorld * InWorld, FName NetDriverName)
{
#if WITH_EDITOR
	const FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld);
	if (WorldContext)
	{
		return FindNamedNetDriver_Local(WorldContext->ActiveNetDrivers, NetDriverName);
	}

	return nullptr;
#else
	return FindNamedNetDriver_Local(GetWorldContextFromWorldChecked(InWorld).ActiveNetDrivers, NetDriverName);
#endif // WITH_EDITOR
}

UNetDriver* UEngine::FindNamedNetDriver(const UPendingNetGame* InPendingNetGame, FName NetDriverName)
{
	return FindNamedNetDriver_Local(GetWorldContextFromPendingNetGameChecked(InPendingNetGame).ActiveNetDrivers, NetDriverName);
}

UNetDriver* CreateNetDriver_Local(UEngine *Engine, FWorldContext &Context, FName NetDriverDefinition)
{
	UNetDriver* NetDriver = nullptr;
	for (int32 Index = 0; Index < Engine->NetDriverDefinitions.Num(); Index++)
	{
		FNetDriverDefinition& NetDriverDef = Engine->NetDriverDefinitions[Index];
		if (NetDriverDef.DefName == NetDriverDefinition)
		{
			// find the class to load
			UClass* NetDriverClass = StaticLoadClass(UNetDriver::StaticClass(), NULL, *NetDriverDef.DriverClassName.ToString(), NULL, LOAD_Quiet, NULL);

			// if it fails, then fall back to standard fallback
			if (NetDriverClass == nullptr || !NetDriverClass->GetDefaultObject<UNetDriver>()->IsAvailable())
			{
				NetDriverClass = StaticLoadClass(UNetDriver::StaticClass(), NULL, *NetDriverDef.DriverClassNameFallback.ToString(), NULL, LOAD_None, NULL);
			}

			// Bail out if the net driver isn't available. The name may be incorrect or the class might not be built as part of the game configuration.
			if (NetDriverClass == nullptr)
			{
				break;
			}

			// Try to create network driver.
			NetDriver = NewObject<UNetDriver>(GetTransientPackage(), NetDriverClass);
			check(NetDriver);
			NetDriver->SetNetDriverName(NetDriver->GetFName());

			new (Context.ActiveNetDrivers) FNamedNetDriver(NetDriver, &NetDriverDef);
			return NetDriver;
		}
	}
	
	UE_LOG(LogNet, Log, TEXT("CreateNamedNetDriver failed to create driver from definition %s"), *NetDriverDefinition.ToString());
	return nullptr;
}

UNetDriver* UEngine::CreateNetDriver(UWorld *InWorld, FName NetDriverDefinition)
{
	return CreateNetDriver_Local(this, GetWorldContextFromWorldChecked(InWorld), NetDriverDefinition);
}

bool CreateNamedNetDriver_Local(UEngine *Engine, FWorldContext &Context, FName NetDriverName, FName NetDriverDefinition)
{
	UNetDriver* NetDriver = FindNamedNetDriver_Local(Context.ActiveNetDrivers, NetDriverName);
	if (NetDriver == nullptr)
	{
		NetDriver = CreateNetDriver_Local(Engine, Context, NetDriverDefinition);
		if (NetDriver)
		{
			NetDriver->SetNetDriverName(NetDriverName);
			return true;
		}
	}

	if (NetDriver)
	{
		UE_LOG(LogNet, Log, TEXT("CreateNamedNetDriver %s already exists as %s"), *NetDriverName.ToString(), *NetDriver->GetName());
	}
	else
	{
		UE_LOG(LogNet, Log, TEXT("CreateNamedNetDriver failed to create driver %s from definition %s"), *NetDriverName.ToString(), *NetDriverDefinition.ToString());
	}
	
	return false;
}

bool UEngine::CreateNamedNetDriver(UWorld *InWorld, FName NetDriverName, FName NetDriverDefinition)
{
	return CreateNamedNetDriver_Local( this, GetWorldContextFromWorldChecked(InWorld), NetDriverName, NetDriverDefinition );
}

bool UEngine::CreateNamedNetDriver(UPendingNetGame *PendingNetGame, FName NetDriverName, FName NetDriverDefinition)
{
	return CreateNamedNetDriver_Local( this, GetWorldContextFromPendingNetGameChecked(PendingNetGame), NetDriverName, NetDriverDefinition);
}

void DestroyNamedNetDriver_Local(FWorldContext &Context, FName NetDriverName)
{
	for (int32 Index = 0; Index < Context.ActiveNetDrivers.Num(); Index++)
	{
		FNamedNetDriver& NamedNetDriver = Context.ActiveNetDrivers[Index];
		UNetDriver* NetDriver = NamedNetDriver.NetDriver;
		if (NetDriver && NetDriver->NetDriverName == NetDriverName)
		{
			UE_LOG(LogNet, Log, TEXT("DestroyNamedNetDriver %s [%s]"), *NetDriver->GetName(), *NetDriverName.ToString());
			NetDriver->SetWorld(NULL);
			NetDriver->Shutdown();
			NetDriver->LowLevelDestroy();
			Context.ActiveNetDrivers.RemoveAtSwap(Index);

			// Remove this driver from the main level collection
			const ELevelCollectionType DriverType = NetDriver->GetDuplicateLevelID() == INDEX_NONE ? ELevelCollectionType::DynamicSourceLevels : ELevelCollectionType::DynamicDuplicatedLevels;
			FLevelCollection* const LevelCollection = Context.World()->FindCollectionByType(DriverType);
			if (LevelCollection)
			{
				if (LevelCollection->GetNetDriver() == NetDriver)
				{
					LevelCollection->SetNetDriver(nullptr);
				}

				if (LevelCollection->GetDemoNetDriver() == NetDriver)
				{
					LevelCollection->SetDemoNetDriver(nullptr);
				}
			}

			break;
		}
	}
}

void UEngine::DestroyNamedNetDriver(UWorld *InWorld, FName NetDriverName)
{
	DestroyNamedNetDriver_Local( GetWorldContextFromWorldChecked(InWorld), NetDriverName);
}

void UEngine::DestroyNamedNetDriver(UPendingNetGame *PendingNetGame, FName NetDriverName)
{
	DestroyNamedNetDriver_Local( GetWorldContextFromPendingNetGameChecked(PendingNetGame), NetDriverName );
}

ENetMode UEngine::GetNetMode(const UWorld *World) const
{ 
	if (World)
	{
		return World->GetNetMode();
	}

	return NM_Standalone;
}

static inline void CallHandleDisconnectForFailure(UWorld* InWorld, UNetDriver* NetDriver)
{
	// No world will be created yet if you fail to initialize network driver while trying to connect via cmd line arg.

	// Calls any global delegates listening, such as on game mode
	FGameDelegates::Get().GetHandleDisconnectDelegate().Broadcast(InWorld, NetDriver);
	
	// A valid world or NetDriver is required to look up a GameInstance/ULocalPlayer.
	if (InWorld)
	{
		if (InWorld->GetGameInstance() != nullptr && InWorld->GetGameInstance()->GetOnlineSession() != nullptr)
		{
			InWorld->GetGameInstance()->GetOnlineSession()->HandleDisconnect(InWorld, NetDriver);
		}
	}
	else if(NetDriver && NetDriver->NetDriverName == NAME_PendingNetDriver)
	{
		// The only disconnect case without a valid InWorld, should be in a travel case where there is a pending game net driver.
		FWorldContext &Context = GEngine->GetWorldContextFromPendingNetGameNetDriverChecked(NetDriver);
		check(Context.OwningGameInstance != NULL && Context.OwningGameInstance->GetFirstGamePlayer() != NULL);

		if ( Context.OwningGameInstance->GetOnlineSession() != nullptr )
		{
			Context.OwningGameInstance->GetOnlineSession()->HandleDisconnect(Context.World(), NetDriver);
		}
	}
	else
	{
		// Handle disconnect should always have a valid world or net driver to give the call context
		UE_LOG(LogNet, Error, TEXT("CallHandleDisconnectForFailure called without valid world or netdriver. (NetDriver: %s"), NetDriver ? *NetDriver->GetName() : TEXT("NULL") );
	}
}

void UEngine::HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (InWorld == NULL)
	{
		UE_LOG(LogNet, Error, TEXT("TravelFailure: %s, Reason for Failure: '%s' with a NULL UWorld"), ETravelFailure::ToString(FailureType), *ErrorString);
	}
	else
	{
		UE_LOG(LogNet, Log, TEXT("TravelFailure: %s, Reason for Failure: '%s'"), ETravelFailure::ToString(FailureType), *ErrorString);

		// Give the GameInstance a chance to handle the failure.
		HandleTravelFailure_NotifyGameInstance(InWorld, FailureType);

		// Cancel pending net game if there was one
		CancelPending(InWorld);

		// Any of these errors should attempt to load back to some stable map
		CallHandleDisconnectForFailure(InWorld, InWorld->GetNetDriver());
	}
}

void UEngine::HandleNetworkFailure(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogNet, Log, TEXT("NetworkFailure: %s, Error: '%s'"), ENetworkFailure::ToString(FailureType), *ErrorString);

	if (!NetDriver)
	{
		return;
	}

	// Only handle failure at this level for game or pending net drivers.
	FName NetDriverName = NetDriver->NetDriverName;
	if (NetDriverName == NAME_GameNetDriver || NetDriverName == NAME_PendingNetDriver)
	{
		// If this net driver has already been unregistered with this world, then don't handle it.
		if (World)
		{
			if (!FindNamedNetDriver(World, NetDriverName))
			{
				// This netdriver has already been destroyed (probably waiting for GC)
				return;
			}
		}

		// Give the GameInstance a chance to handle the failure.
		HandleNetworkFailure_NotifyGameInstance(World, NetDriver, FailureType);

		ENetMode FailureNetMode = NetDriver->GetNetMode();	// NetMode of the driver that failed
		bool bShouldTravel = true;

		switch (FailureType)
		{
		case ENetworkFailure::FailureReceived:
			break;
		case ENetworkFailure::PendingConnectionFailure:
			// TODO stop the connecting movie
			break;
		case ENetworkFailure::ConnectionLost:
			// Hosts don't travel when clients disconnect
			bShouldTravel = (FailureNetMode == NM_Client);
			break;
		case ENetworkFailure::ConnectionTimeout:
			// Hosts don't travel when clients disconnect
			bShouldTravel = (FailureNetMode == NM_Client);
			break;
		case ENetworkFailure::NetGuidMismatch:
		case ENetworkFailure::NetChecksumMismatch:
			// Hosts don't travel when clients have actor issues
			bShouldTravel = (FailureNetMode == NM_Client);
			break;
		case ENetworkFailure::NetDriverAlreadyExists:
		case ENetworkFailure::NetDriverCreateFailure:
		case ENetworkFailure::OutdatedClient:
		case ENetworkFailure::OutdatedServer:
		default:
			break;
		}

		if (bShouldTravel)
		{
			CallHandleDisconnectForFailure(World, NetDriver);
		}
	}
}

void UEngine::HandleNetworkLagStateChanged(UWorld* World, UNetDriver* NetDriver, ENetworkLagState::Type LagType)
{
	// Stub. Implement in subclasses
}

void UEngine::HandleNetworkFailure_NotifyGameInstance(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType)
{
	bool bIsServer = true;

	if (NetDriver != nullptr)
	{
		bIsServer = NetDriver->GetNetMode() != NM_Client;
	}

	if (World != nullptr && World->GetGameInstance() != nullptr)
	{
		World->GetGameInstance()->HandleNetworkError(FailureType, bIsServer);
	}
	else
	{
		// Since the UWorld passed in might be null, as well as the NetDriver's UWorld,
		// go through the world contexts until we find the one with this net driver.
		for (auto& Context : WorldList)
		{
			if (Context.PendingNetGame != nullptr &&
				Context.PendingNetGame->NetDriver == NetDriver &&
				Context.OwningGameInstance != nullptr)
			{
				// Use the GameInstance from the current context.
				Context.OwningGameInstance->HandleNetworkError(FailureType, bIsServer);
			}
		}
	}
}

void UEngine::HandleTravelFailure_NotifyGameInstance(UWorld *World, ETravelFailure::Type FailureType)
{
	if (World != nullptr && World->GetGameInstance() != nullptr)
	{
		World->GetGameInstance()->HandleTravelError(FailureType);
	}
}

void UEngine::SpawnServerActors(UWorld *World)
{
	TArray<FString> FullServerActors;

	FullServerActors.Append(ServerActors);
	FullServerActors.Append(RuntimeServerActors);

	for( int32 i=0; i < FullServerActors.Num(); i++ )
	{
		TCHAR Str[2048];
		const TCHAR* Ptr = * FullServerActors[i];
		if( FParse::Token( Ptr, Str, ARRAY_COUNT(Str), 1 ) )
		{
			UE_LOG(LogNet, Log, TEXT("Spawning: %s"), Str );
			UClass* HelperClass = StaticLoadClass( AActor::StaticClass(), NULL, Str, NULL, LOAD_None, NULL );
			AActor* Actor = World->SpawnActor( HelperClass );
			while( Actor && FParse::Token(Ptr,Str,ARRAY_COUNT(Str),1) )
			{
				TCHAR* Value = FCString::Strchr(Str,'=');
				if( Value )
				{
					*Value++ = 0;
					for( TFieldIterator<UProperty> It(Actor->GetClass()); It; ++It )
					{
						if(	FCString::Stricmp(*It->GetName(),Str)==0
							&&	(It->PropertyFlags & CPF_Config) )
						{
							It->ImportText( Value, It->ContainerPtrToValuePtr<uint8>(Actor), 0, Actor );
						}
					}
				}
			}
		}
	}
}

bool UEngine::HandleOpenCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Absolute);
	if (TestURL.IsLocalInternal())
	{
		// make sure the file exists if we are opening a local file
		if (!MakeSureMapNameIsValid(TestURL.Map))
		{
			Ar.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
			return true;
		}
#if WITH_EDITOR
		else
		{
	// Next comes a complicated but necessary way of blocking a crash caused by opening a level when playing multiprocess as a client (that's not allowed because of streaming levels)
	ULevelEditorPlaySettings* PlayInSettings = GetMutableDefault<ULevelEditorPlaySettings>();
	check(PlayInSettings);
	bool bMultiProcess = !([&PlayInSettings] { bool RunUnderOneProcess(false); return (PlayInSettings->GetRunUnderOneProcess(RunUnderOneProcess) && RunUnderOneProcess); }());

	const EPlayNetMode PlayNetMode = [&PlayInSettings] { EPlayNetMode NetMode(PIE_Standalone); return (PlayInSettings->GetPlayNetMode(NetMode) ? NetMode : PIE_Standalone); }();
	bool bClientMode = PlayNetMode == EPlayNetMode::PIE_Client;

	if (bMultiProcess && bClientMode)
	{
		UE_LOG(LogNet, Log, TEXT("%s"), TEXT("Opening a map is not allowed in this play mode (client mode + multiprocess)!"));
				return true;
			}
	}
#endif
	}

		SetClientTravel(InWorld, Cmd, TRAVEL_Absolute);
	return true;
}

bool UEngine::HandleTravelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Partial);
	if (TestURL.IsLocalInternal())
	{
		// make sure the file exists if we are opening a local file
		bool bMapFound = MakeSureMapNameIsValid(TestURL.Map);
		if (!bMapFound)
		{
			Ar.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
			return true;
		}
	}

	SetClientTravel( InWorld, Cmd, TRAVEL_Partial );
	return true;
}

bool UEngine::HandleStreamMapCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	FURL TestURL(&WorldContext.LastURL, Cmd, TRAVEL_Partial);
	if (TestURL.IsLocalInternal())
	{
		// make sure the file exists if we are opening a local file
		if (MakeSureMapNameIsValid(TestURL.Map) && TestURL.Valid)
		{
			for (const ULevel* const Level : InWorld->GetLevels())
			{
				if (Level->URL.Map == TestURL.Map)
				{
					Ar.Logf(TEXT("ERROR: The map '%s' is already loaded."), *TestURL.Map);
					return true;
				}
			}

			TArray<FName> LevelNames;
			LevelNames.Add(*TestURL.Map);

			FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);

			PrepareMapChange(Context, LevelNames);
			Context.bShouldCommitPendingMapChange = true;
			ConditionalCommitMapChange(Context);
		}
		else
		{
			Ar.Logf(TEXT("ERROR: The map '%s' does not exist."), *TestURL.Map);
		}
	}
	else
	{
		Ar.Logf(TEXT("ERROR: Can only perform streaming load for local URLs."));
	}
	return true;
}

#if WITH_SERVER_CODE
bool UEngine::HandleServerTravelCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	if( InWorld->IsServer() )
	{
		FString URL(Cmd);
		FString MapName, Options;

		// Split the options off the map name before we validate it
		if (URL.Split(TEXT("?"), &MapName, &Options, ESearchCase::CaseSensitive) == false)
		{
			MapName = URL;
		}

		if (MakeSureMapNameIsValid(MapName))
		{
			// If there were options reconstitute the URL before sending in to the server travel call 
			URL = (Options.IsEmpty() ? MapName : MapName + TEXT("?") + Options);
			InWorld->ServerTravel(URL);
			return true;
		}
		else
		{
			Ar.Logf(TEXT("ERROR: The map '%s' is either short package name or does not exist."), *MapName);
		}
	}
	return false;
}

bool UEngine::HandleSayCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	return false;
}
#endif

bool UEngine::HandleDisconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld )
{
	// This should only be called from typing 'disconnect' at the console. InWorld must have a valid WorldContext.
	check(InWorld);
	check(GetWorldContextFromWorld(InWorld));

	HandleDisconnect(InWorld, InWorld->GetNetDriver());
	return true;
}

void UEngine::HandleDisconnect( UWorld *InWorld, UNetDriver *NetDriver )
{
	// There must be some context for this disconnect
	check(InWorld || NetDriver);

	// If the NetDriver that failed was a pending netgame driver, cancel the PendingNetGame
	CancelPending(NetDriver);

	// InWorld might be null. It might also not map to any valid world context (for example, a pending net game disconnect)
	// If there is a context for this world, setup client travel.
	if (FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld))
	{
		// Remove ?Listen parameter, if it exists
		WorldContext->LastURL.RemoveOption( TEXT("Listen") );
		WorldContext->LastURL.RemoveOption( TEXT("LAN") );

		// Net driver destruction will occur during LoadMap (prevents GetNetMode from changing output for the remainder of the frame)
		SetClientTravel( InWorld, TEXT("?closed"), TRAVEL_Absolute );
	}
	else if (NetDriver)
	{
		// Shut down any existing game connections
		if (InWorld)
		{
			// Call this to remove the NetDriver from the world context's ActiveNetDriver list
			DestroyNamedNetDriver(InWorld, NetDriver->NetDriverName);
		}
		else
		{
			NetDriver->Shutdown();
			NetDriver->LowLevelDestroy();

			// In this case, the world is null and something went wrong, so we should travel back to the default world so that we
			// can get back to a good state.
			for (FWorldContext& PotentialWorldContext : WorldList)
			{
				if (PotentialWorldContext.WorldType == EWorldType::Game)
				{
					FURL DefaultURL;
					DefaultURL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
					const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
					if (GameMapsSettings)
					{
						PotentialWorldContext.TravelURL = FURL(&DefaultURL, *(GameMapsSettings->GetGameDefaultMap() + GameMapsSettings->LocalMapOptions), TRAVEL_Partial).ToString();
						PotentialWorldContext.TravelType = TRAVEL_Partial;
					}
				}
			}
		}
	}
}

bool UEngine::HandleReconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld )
{
	FWorldContext &WorldContext = GetWorldContextFromWorldChecked(InWorld);
	if (WorldContext.LastRemoteURL.Valid && WorldContext.LastRemoteURL.Host != TEXT(""))
	{
		SetClientTravel(InWorld, *WorldContext.LastRemoteURL.ToString(), TRAVEL_Absolute);
	}
	return true;
}

bool UEngine::MakeSureMapNameIsValid(FString& InOutMapName)
{
	const FString TestMapName = UWorld::RemovePIEPrefix(InOutMapName);

	// Check if the map name is long package name and if it actually exists.
	// Short package names are only supported in non-shipping builds.
	bool bIsValid = !FPackageName::IsShortPackageName(TestMapName);
	if (bIsValid)
	{
		// If the user starts a multiplayer PIE session with an unsaved map,
		// DoesPackageExist won't find it, so we have to try to find the package in memory as well.
		bIsValid = (FindObjectFast<UPackage>(nullptr, FName(*TestMapName)) != nullptr) || FPackageName::DoesPackageExist(TestMapName);

		// If we're not in the editor, then we always want to strip off the PIE prefix.  We might be connected to
		// a PIE listen server.  In this case, we'll use our version of the map without the PIE prefix.
		if (bIsValid && !GIsEditor)
		{
			InOutMapName = TestMapName;
		}
	}
	else
	{
		// Look up on disk. Slow!
		FString LongPackageName;
		bIsValid = FPackageName::SearchForPackageOnDisk(TestMapName, &LongPackageName);
		if (bIsValid)
		{
			InOutMapName = LongPackageName;
		}
	}
	return bIsValid;
}

void UEngine::SetClientTravel( UWorld *InWorld, const TCHAR* NextURL, ETravelType InTravelType )
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);

	// set TravelURL.  Will be processed safely on the next tick in UGameEngine::Tick().
	Context.TravelURL    = NextURL;
	Context.TravelType   = InTravelType;

	// Prevent crashing the game by attempting to connect to own listen server
	if ( Context.LastURL.HasOption(TEXT("Listen")) )
	{
		Context.LastURL.RemoveOption(TEXT("Listen"));
	}
}

void UEngine::SetClientTravel( UPendingNetGame *PendingNetGame, const TCHAR* NextURL, ETravelType InTravelType )
{
	FWorldContext &Context = GetWorldContextFromPendingNetGameChecked(PendingNetGame);

	// set TravelURL.  Will be processed safely on the next tick in UGameEngine::Tick().
	Context.TravelURL    = NextURL;
	Context.TravelType   = InTravelType;

	// Prevent crashing the game by attempting to connect to own listen server
	if ( Context.LastURL.HasOption(TEXT("Listen")) )
	{
		Context.LastURL.RemoveOption(TEXT("Listen"));
	}
}

void UEngine::SetClientTravelFromPendingGameNetDriver( UNetDriver *PendingGameNetDriverGame, const TCHAR* NextURL, ETravelType InTravelType )
{
	// Find WorldContext whose pendingNetGame's NetDriver is the passed in net driver
	for( int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		FWorldContext &Context = WorldList[idx];
		if (Context.PendingNetGame && Context.PendingNetGame->NetDriver == PendingGameNetDriverGame)
		{
			SetClientTravel( Context.PendingNetGame, NextURL, InTravelType );
			return;
		}
	}
	check(false);
}

EBrowseReturnVal::Type UEngine::Browse( FWorldContext& WorldContext, FURL URL, FString& Error )
{
	Error = TEXT("");
	WorldContext.TravelURL = TEXT("");

	// Convert .unreal link files.
	const TCHAR* LinkStr = TEXT(".unreal");//!!
	if( FCString::Strstr(*URL.Map,LinkStr)-*URL.Map==FCString::Strlen(*URL.Map)-FCString::Strlen(LinkStr) )
	{
		UE_LOG(LogNet, Log,  TEXT("Link: %s"), *URL.Map );
		FString NewUrlString;
		if( GConfig->GetString( TEXT("Link")/*!!*/, TEXT("Server"), NewUrlString, *URL.Map ) )
		{
			// Go to link.
			URL = FURL( NULL, *NewUrlString, TRAVEL_Absolute );//!!
		}
		else
		{
			// Invalid link.
			Error = FText::Format( NSLOCTEXT("Engine", "InvalidLink", "Invalid Link: {0}"), FText::FromString( URL.Map ) ).ToString();
			return EBrowseReturnVal::Failure;
		}
	}

	// Crack the URL.
	UE_LOG(LogNet, Log, TEXT("Browse: %s"), *URL.ToString() );

	// Handle it.
	if( !URL.Valid )
	{
		// Unknown URL.
		Error = FText::Format( NSLOCTEXT("Engine", "InvalidUrl", "Invalid URL: {0}"), FText::FromString( URL.ToString() ) ).ToString();
		BroadcastTravelFailure(WorldContext.World(), ETravelFailure::InvalidURL, Error);
		return EBrowseReturnVal::Failure;
	}
	else if (URL.HasOption(TEXT("failed")) || URL.HasOption(TEXT("closed")))
	{
		// Browsing after a failure, load default map

		if (WorldContext.PendingNetGame)
		{
			CancelPending(WorldContext);
		}
		// Handle failure URL.
		UE_LOG(LogNet, Log, TEXT("%s"), TEXT("Failed; returning to Entry") );
		if (WorldContext.World() != NULL)
		{
			ResetLoaders( WorldContext.World()->GetOuter() );
		}
		
		const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
		const FString TextURL = GameMapsSettings->GetGameDefaultMap() + GameMapsSettings->LocalMapOptions;
		if (!LoadMap(WorldContext, FURL(&URL, *TextURL, TRAVEL_Partial), NULL, Error))
		{
			HandleBrowseToDefaultMapFailure(WorldContext, TextURL, Error);
			return EBrowseReturnVal::Failure;
		}

		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		// now remove "failed" and "closed" options from LastURL so it doesn't get copied on to future URLs
		WorldContext.LastURL.RemoveOption(TEXT("failed"));
		WorldContext.LastURL.RemoveOption(TEXT("closed"));
		return EBrowseReturnVal::Success;
	}
	else if( URL.HasOption(TEXT("restart")) )
	{
		// Handle restarting.
		URL = WorldContext.LastURL;
	}

	// Handle normal URL's.
	if (GDisallowNetworkTravel && URL.HasOption(TEXT("listen")))
	{
		Error = NSLOCTEXT("Engine", "UsedCheatCommands", "Console commands were used which are disallowed in netplay.  You must restart the game to create a match.").ToString();
		BroadcastTravelFailure(WorldContext.World(), ETravelFailure::CheatCommands, Error);
		return EBrowseReturnVal::Failure;
	}
	if( URL.IsLocalInternal() )
	{
		// Local map file.
		return LoadMap( WorldContext, URL, NULL, Error ) ? EBrowseReturnVal::Success : EBrowseReturnVal::Failure;
	}
	else if( URL.IsInternal() && GIsClient )
	{
		// Network URL.
		if( WorldContext.PendingNetGame )
		{
			CancelPending(WorldContext);
		}

		// Clean up the netdriver/socket so that the pending level succeeds
		if (WorldContext.World() && ShouldShutdownWorldNetDriver())
		{
			ShutdownWorldNetDriver(WorldContext.World());
		}

		WorldContext.PendingNetGame = NewObject<UPendingNetGame>();
		WorldContext.PendingNetGame->Initialize(URL);
		WorldContext.PendingNetGame->InitNetDriver();
		if( !WorldContext.PendingNetGame->NetDriver )
		{
			// UPendingNetGame will set the appropriate error code and connection lost type, so
			// we just have to propagate that message to the game.
			BroadcastTravelFailure(WorldContext.World(), ETravelFailure::PendingNetGameCreateFailure, WorldContext.PendingNetGame->ConnectionError);
			WorldContext.PendingNetGame = NULL;
			return EBrowseReturnVal::Failure;
		}
		return EBrowseReturnVal::Pending;
	}
	else if( URL.IsInternal() )
	{
		// Invalid.
		Error = NSLOCTEXT("Engine", "ServerOpen", "Servers can't open network URLs").ToString();
		return EBrowseReturnVal::Failure;
	}
	{
		// External URL - disabled by default.
		// Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));
		// FPlatformProcess::LaunchURL( *URL.ToString(), TEXT(""), &Error );
		return EBrowseReturnVal::Failure;
	}
}

void UEngine::CancelPending(UNetDriver* PendingNetGameDriver)
{
	if (PendingNetGameDriver==NULL)
	{
		return;
	}

	// Find WorldContext whose pendingNetGame's NetDriver is the passed in net driver
	for( int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		FWorldContext &Context = WorldList[idx];
		if (Context.PendingNetGame && Context.PendingNetGame->NetDriver == PendingNetGameDriver)
		{
			// Kill this PendingNetGame
			CancelPending(Context);
			check(Context.PendingNetGame == NULL);	// Verify PendingNetGame was cleared in CancelPending
		}
	}
}

void UEngine::CancelPending(FWorldContext &Context)
{
	if (Context.PendingNetGame && Context.PendingNetGame->NetDriver && Context.PendingNetGame->NetDriver->ServerConnection)
	{
		Context.PendingNetGame->NetDriver->ServerConnection->Close();
		DestroyNamedNetDriver_Local(Context, Context.PendingNetGame->NetDriver->NetDriverName);
		Context.PendingNetGame->NetDriver = NULL;
	}

	Context.PendingNetGame = NULL;
}

bool UEngine::WorldIsPIEInNewViewport(UWorld *InWorld)
{
	// UEditorEngine will override to check slate state
	return false;
}

void UEngine::CancelPending(UWorld *InWorld, UPendingNetGame *NewPendingNetGame)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	CancelPending(Context);
	Context.PendingNetGame = NewPendingNetGame;
}

void UEngine::CancelAllPending()
{
	for( int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		FWorldContext &Context = WorldList[idx];
		CancelPending(Context);
	}
}

void UEngine::BrowseToDefaultMap( FWorldContext& Context )
{
	FString Error;
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), GGameIni );
	const UGameMapsSettings* GameMapsSettings = GetDefault<UGameMapsSettings>();
	const FString& TextURL = GameMapsSettings->GetGameDefaultMap() + GameMapsSettings->LocalMapOptions;

	if (Browse( Context, FURL(&DefaultURL, *TextURL, TRAVEL_Partial), Error ) != EBrowseReturnVal::Success)
	{
		HandleBrowseToDefaultMapFailure(Context, TextURL, Error);
	}
}

void UEngine::HandleBrowseToDefaultMapFailure(FWorldContext& Context, const FString& TextURL, const FString& Error)
{
	UE_LOG(LogNet, Error, TEXT("Failed to load default map (%s). Error: (%s)"), *TextURL, *Error);
	const FText Message = FText::Format(NSLOCTEXT("Engine", "FailedToLoadDefaultMap", "Error '{0}'. Exiting."), FText::FromString(Error));
	FMessageDialog::Open(EAppMsgType::Ok, Message);

	// Even though we're probably going to shut down anyway, create a dummy world since a lot of code expects it.
	if (Context.World() == nullptr)
	{
		Context.SetCurrentWorld( UWorld::CreateWorld( Context.WorldType, false ) );
	}
}

void UEngine::TickWorldTravel(FWorldContext& Context, float DeltaSeconds)
{
	// Handle seamless traveling
	if (Context.SeamlessTravelHandler.IsInTransition())
	{
		// Note: SeamlessTravelHandler.Tick may automatically update Context.World and GWorld internally
		Context.SeamlessTravelHandler.Tick();
	}

	// Handle server traveling.
	if (Context.World() == nullptr)
	{
		UE_LOG(LogLoad, Error, TEXT("UEngine::TickWorldTravel has no world after ticking seamless travel handler."));
		BrowseToDefaultMap(Context);
		BroadcastTravelFailure(Context.World(), ETravelFailure::ServerTravelFailure, TEXT("UEngine::TickWorldTravel has no world after ticking seamless travel handler."));
		return;
	}
	
	if( !Context.World()->NextURL.IsEmpty() )
	{
		Context.World()->NextSwitchCountdown -= DeltaSeconds;
		if( Context.World()->NextSwitchCountdown <= 0.f )
		{
			UE_LOG(LogEngine, Log,  TEXT("Server switch level: %s"), *Context.World()->NextURL );
			if (Context.World()->GetAuthGameMode() != NULL)
			{
				Context.World()->GetAuthGameMode()->StartToLeaveMap();
			}
			FString Error;
			FString NextURL = Context.World()->NextURL;
			EBrowseReturnVal::Type Ret = Browse( Context, FURL(&Context.LastURL,*NextURL,(ETravelType)Context.World()->NextTravelType), Error );
			if (Ret != EBrowseReturnVal::Success )
			{
				UE_LOG(LogLoad, Warning, TEXT("UEngine::TickWorldTravel failed to Handle server travel to URL: %s. Error: %s"), *NextURL, *Error);
				check(Ret != EBrowseReturnVal::Pending); // server travel should never create a pending net game

				// Failed to load a new map
				if (Context.World() != NULL)
				{
					// If we didn't change worlds, clear out NextURL so we don't do this again next frame.
					Context.World()->NextURL = TEXT("");
				}
				else
				{
					// Our old world got stomped out. Load the default map
					BrowseToDefaultMap(Context);
				}

				// Let people know that we failed to server travel
				BroadcastTravelFailure(Context.World(), ETravelFailure::ServerTravelFailure, Error);
			}
			return;
		}
	}

	// Handle client traveling.
	if( !Context.TravelURL.IsEmpty() )
	{	
		AGameModeBase* const GameMode = Context.World()->GetAuthGameMode();
		if (GameMode)
		{
			GameMode->StartToLeaveMap();
		}

		FString Error, TravelURLCopy = Context.TravelURL;
		if (Browse( Context, FURL(&Context.LastURL,*TravelURLCopy,(ETravelType)Context.TravelType), Error ) == EBrowseReturnVal::Failure)
		{
			// If the failure resulted in no world being loaded (we unloaded our last world, then failed to load the new one)
			// then load the default map to avoid getting in a situation where we have no valid UWorld.
			if (Context.World() == NULL)
			{
				BrowseToDefaultMap(Context);
			}

			// Let people know that we failed to client travel
			BroadcastTravelFailure(Context.World(), ETravelFailure::ClientTravelFailure, Error);
		}
		check(Context.World() != NULL);
		return;
	}

	// Update the pending level.
	if( Context.PendingNetGame )
	{
		Context.PendingNetGame->Tick( DeltaSeconds );
		if ( Context.PendingNetGame && Context.PendingNetGame->ConnectionError.Len() > 0 )
		{
			BroadcastNetworkFailure(NULL, Context.PendingNetGame->NetDriver, ENetworkFailure::PendingConnectionFailure, Context.PendingNetGame->ConnectionError);
			CancelPending(Context);
		}
		else if (Context.PendingNetGame && Context.PendingNetGame->bSuccessfullyConnected && !Context.PendingNetGame->bSentJoinRequest && (Context.OwningGameInstance == NULL || !Context.OwningGameInstance->DelayPendingNetGameTravel()))
		{
			if (!MakeSureMapNameIsValid(Context.PendingNetGame->URL.Map))
			{
				BrowseToDefaultMap(Context);
				BroadcastTravelFailure(Context.World(), ETravelFailure::PackageMissing, Context.PendingNetGame->URL.RedirectURL);
			}
			else
			{
				// Attempt to load the map.
				FString Error;

				const bool bLoadedMapSuccessfully = LoadMap(Context, Context.PendingNetGame->URL, Context.PendingNetGame, Error);

				Context.PendingNetGame->LoadMapCompleted(this, Context, bLoadedMapSuccessfully, Error);

				// Kill the pending level.
				Context.PendingNetGame = NULL;
			}
		}
	}
	else if (TransitionType == TT_WaitingToConnect)
	{
		TransitionType = TT_None;
	}

	return;
}

bool UEngine::LoadMap( FWorldContext& WorldContext, FURL URL, class UPendingNetGame* Pending, FString& Error )
{
	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "LoadMap - " ) + URL.Map )) );

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UEngine::LoadMap"), STAT_LoadMap, STATGROUP_LoadTime);

	LLM_SCOPE(ELLMTag::LoadMapMisc);

	NETWORK_PROFILER(GNetworkProfiler.TrackSessionChange(true,URL));
	MALLOC_PROFILER( FMallocProfiler::SnapshotMemoryLoadMapStart( URL.Map ) );
	Error = TEXT("");

	FLoadTimeTracker::Get().ResetRawLoadTimes();

	// make sure level streaming isn't frozen
	if (WorldContext.World())
	{
		WorldContext.World()->bIsLevelStreamingFrozen = false;
	}

	// send a callback message
	FCoreUObjectDelegates::PreLoadMap.Broadcast(URL.Map);
	// make sure there is a matching PostLoadMap() no matter how we exit
	struct FPostLoadMapCaller
	{
		bool bCalled;
		FPostLoadMapCaller()
			: bCalled(false)
		{}
		~FPostLoadMapCaller()
		{
			if (!bCalled)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				FCoreUObjectDelegates::PostLoadMap.Broadcast();
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				FCoreUObjectDelegates::PostLoadMapWithWorld.Broadcast(nullptr);
			}
		}
	} PostLoadMapCaller;

	// Cancel any pending texture streaming requests.  This avoids a significant delay on consoles 
	// when loading a map and there are a lot of outstanding texture streaming requests from the previous map.
	UTexture2D::CancelPendingTextureStreaming();

	// play a load map movie if specified in ini
	bStartedLoadMapMovie = false;

	// clean up any per-map loaded packages for the map we are leaving
	if (WorldContext.World() && WorldContext.World()->PersistentLevel)
	{
		CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Map, WorldContext.World()->PersistentLevel->GetOutermost()->GetName());
	}

	// cleanup the existing per-game pacakges
	// @todo: It should be possible to not unload/load packages if we are going from/to the same GameMode.
	//        would have to save the game pathname here and pass it in to SetGameMode below
	CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Game_PreLoadClass, TEXT(""));
	CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Game_PostLoadClass, TEXT(""));
	CleanupPackagesToFullyLoad(WorldContext, FULLYLOAD_Mutator, TEXT(""));


	// Cancel any pending async map changes after flushing async loading. We flush async loading before canceling the map change
	// to avoid completion after cancellation to not leave references to the "to be changed to" level around. Async loading is
	// implicitly flushed again later on during garbage collection.
	FlushAsyncLoading();
	CancelPendingMapChange(WorldContext);
	WorldContext.SeamlessTravelHandler.CancelTravel();

	double	StartTime = FPlatformTime::Seconds();

	UE_LOG(LogLoad, Log,  TEXT("LoadMap: %s"), *URL.ToString() );
	GInitRunaway();

#if !UE_BUILD_SHIPPING
	const bool bOldWorldWasShowingCollisionForHiddenComponents = WorldContext.World() && WorldContext.World()->bCreateRenderStateForHiddenComponents;
#endif

	// Unload the current world
	if( WorldContext.World() )
	{
		if(!URL.HasOption(TEXT("quiet")) )
		{
			TransitionType = TT_Loading;
			TransitionDescription = URL.Map;
			if (URL.HasOption(TEXT("Game=")))
			{
				TransitionGameMode = URL.GetOption(TEXT("Game="), TEXT(""));
			}
			else
			{
				TransitionGameMode = TEXT("");
			}
			
			// Display loading screen.		
			// Check if a loading movie is playing.  If so it is not safe to redraw the viewport due to potential race conditions with font rendering
			bool bIsLoadingMovieCurrentlyPlaying = FCoreDelegates::IsLoadingMovieCurrentlyPlaying.IsBound() ? FCoreDelegates::IsLoadingMovieCurrentlyPlaying.Execute() : false;
			if(!bIsLoadingMovieCurrentlyPlaying)
			{
				LoadMapRedrawViewports();
			}
			
			TransitionType = TT_None;
		}

		// Clean up networking
		ShutdownWorldNetDriver(WorldContext.World());

		// Make sure there are no pending visibility requests.
		WorldContext.World()->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
		
		// send a message that all levels are going away (NULL means every sublevel is being removed
		// without a call to RemoveFromWorld for each)
		//if (WorldContext.World()->GetNumLevels() > 1)
		{
			// TODO: Consider actually broadcasting for each level?
			FWorldDelegates::LevelRemovedFromWorld.Broadcast(nullptr, WorldContext.World());
		}

		// Disassociate the players from their PlayerControllers in this world.
		if (WorldContext.OwningGameInstance != nullptr)
		{
			for(auto It = WorldContext.OwningGameInstance->GetLocalPlayerIterator(); It; ++It)
			{
				ULocalPlayer *Player = *It;
				if(Player->PlayerController && Player->PlayerController->GetWorld() == WorldContext.World())
				{
					if(Player->PlayerController->GetPawn())
					{
						WorldContext.World()->DestroyActor(Player->PlayerController->GetPawn(), true);
					}
					WorldContext.World()->DestroyActor(Player->PlayerController, true);
					Player->PlayerController = nullptr;
				}
				// reset split join info so we'll send one after loading the new map if necessary
				Player->bSentSplitJoin = false;
			}
		}

		for (FActorIterator ActorIt(WorldContext.World()); ActorIt; ++ActorIt)
		{
			ActorIt->RouteEndPlay(EEndPlayReason::LevelTransition);
		}

		// Do this after destroying pawns/playercontrollers, in case that spawns new things (e.g. dropped weapons)
		WorldContext.World()->CleanupWorld();

		if( GEngine )
		{
			// clear any "DISPLAY" properties referencing level objects
			if (GEngine->GameViewport != nullptr)
			{
				ClearDebugDisplayProperties();
			}

			GEngine->WorldDestroyed(WorldContext.World());
		}
		WorldContext.World()->RemoveFromRoot();

		// mark everything else contained in the world to be deleted
		for (auto LevelIt(WorldContext.World()->GetLevelIterator()); LevelIt; ++LevelIt)
		{
			const ULevel* Level = *LevelIt;
			if (Level)
			{
				CastChecked<UWorld>(Level->GetOuter())->MarkObjectsPendingKill();
			}
		}

		for (ULevelStreaming* LevelStreaming : WorldContext.World()->StreamingLevels)
		{
			// If an unloaded levelstreaming still has a loaded level we need to mark its objects to be deleted as well
			if ((!LevelStreaming->bShouldBeLoaded || !LevelStreaming->bShouldBeVisible) && LevelStreaming->GetLoadedLevel())
			{
				CastChecked<UWorld>(LevelStreaming->GetLoadedLevel()->GetOuter())->MarkObjectsPendingKill();
			}
		}

		// Stop all audio to remove references to current level.
		if (FAudioDevice* AudioDevice = WorldContext.World()->GetAudioDevice())
		{
			AudioDevice->Flush(WorldContext.World());
			AudioDevice->SetTransientMasterVolume(1.0f);
		}

		WorldContext.SetCurrentWorld(nullptr);
	}

	// trim memory to clear up allocations from the previous level (also flushes rendering)
	TrimMemory();

	// Cancels the Forced StreamType for textures using a timer.
	if (!IStreamingManager::HasShutdown())
	{
		IStreamingManager::Get().CancelForcedResources();
	}

	if (FPlatformProperties::RequiresCookedData())
	{
		appDefragmentTexturePool();
		appDumpTextureMemoryStats(TEXT(""));
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Dump info

	VerifyLoadMapWorldCleanup();
	
#endif

	MALLOC_PROFILER( FMallocProfiler::SnapshotMemoryLoadMapMid( URL.Map ); )

	WorldContext.OwningGameInstance->PreloadContentForURL(URL);

	UPackage* WorldPackage = NULL;
	UWorld*	NewWorld = NULL;
	
	// If this world is a PIE instance, we need to check if we are traveling to another PIE instance's world.
	// If we are, we need to set the PIERemapPrefix so that we load a copy of that world, instead of loading the
	// PIE world directly.
	if (!WorldContext.PIEPrefix.IsEmpty())
	{
		for (const FWorldContext& WorldContextFromList : WorldList)
		{
			// We want to ignore our own PIE instance so that we don't unnecessarily set the PIERemapPrefix if we are not traveling to
			// a server.
			if (WorldContextFromList.World() != WorldContext.World())
			{
				if (!WorldContextFromList.PIEPrefix.IsEmpty() && URL.Map.Contains(WorldContextFromList.PIEPrefix))
				{
					FString SourceWorldPackage = UWorld::RemovePIEPrefix(URL.Map);

					// We are loading a new world for this context, so clear out PIE fixups that might be lingering.
					// (note we dont want to do this in DuplicateWorldForPIE, since that is also called on streaming worlds.
					GPlayInEditorID = WorldContext.PIEInstance;
					FLazyObjectPtr::ResetPIEFixups();

					NewWorld = UWorld::DuplicateWorldForPIE(SourceWorldPackage, nullptr);
					if (NewWorld == nullptr)
					{
						NewWorld = CreatePIEWorldByLoadingFromPackage(WorldContext, SourceWorldPackage, WorldPackage);
						if (NewWorld == nullptr)
						{
							Error = FString::Printf(TEXT("Failed to load package '%s' while in PIE"), *SourceWorldPackage);
							return false;
						}
					}
					else
					{
						WorldPackage = CastChecked<UPackage>(NewWorld->GetOuter());
					}

					NewWorld->StreamingLevelsPrefix = UWorld::BuildPIEPackagePrefix(WorldContext.PIEInstance);
					GIsPlayInEditorWorld = true;
				}
			}
		}
	}

	const FString URLTrueMapName = URL.Map;

	// Normal map loading
	if (NewWorld == NULL)
	{
		// Set the world type in the static map, so that UWorld::PostLoad can set the world type
		const FName URLMapFName = FName(*URL.Map);
		UWorld::WorldTypePreLoadMap.FindOrAdd( URLMapFName ) = WorldContext.WorldType;

		// See if the level is already in memory
		WorldPackage = FindPackage(nullptr, *URL.Map);

		bool bPackageAlreadyLoaded = (WorldPackage != nullptr);

		// If the level isn't already in memory, load level from disk
		if (WorldPackage == nullptr)
		{
			WorldPackage = LoadPackage(nullptr, *URL.Map, (WorldContext.WorldType == EWorldType::PIE ? LOAD_PackageForPIE : LOAD_None));
		}

		// Clean up the world type list now that PostLoad has occurred
		UWorld::WorldTypePreLoadMap.Remove( URLMapFName );

		if (WorldPackage == nullptr)
		{
			// it is now the responsibility of the caller to deal with a NULL return value and alert the user if necessary
			Error = FString::Printf(TEXT("Failed to load package '%s'"), *URL.Map);
			return false;
		}

		// Find the newly loaded world.
		NewWorld = UWorld::FindWorldInPackage(WorldPackage);

		// If the world was not found, it could be a redirector to a world. If so, follow it to the destination world.
		if ( !NewWorld )
		{
			NewWorld = UWorld::FollowWorldRedirectorInPackage(WorldPackage);
			if ( NewWorld )
			{
				// Treat this as an already loaded package because we were loaded by the redirector
				bPackageAlreadyLoaded = true;
				WorldPackage = NewWorld->GetOutermost();
			}
		}
		check(NewWorld);

		NewWorld->PersistentLevel->HandleLegacyMapBuildData();

		FScopeCycleCounterUObject MapScope(WorldPackage);

		if (WorldContext.WorldType == EWorldType::PIE)
		{
			// If we are a PIE world and the world we just found is already initialized, then we're probably reloading the editor world and we
			// need to create a PIE world by duplication instead
			if (bPackageAlreadyLoaded || NewWorld->WorldType == EWorldType::Editor)
			{
				if (WorldContext.PIEInstance == -1)
				{
					// Assume if we get here, that it's safe to just give a PIE instance so that we can duplicate the world 
					//	If we won't duplicate the world, we'll refer to the existing world (most likely the editor version, and it can be modified under our feet, which is bad)
					// So far, the only known way to get here is when we use the console "open" command while in a client PIE instance connected to non PIE server 
					// (i.e. multi process PIE where client is in current editor process, and dedicated server was launched as separate process)
					WorldContext.PIEInstance = 0;
				}

				NewWorld = CreatePIEWorldByDuplication(WorldContext, NewWorld, URL.Map);
				// CreatePIEWorldByDuplication clears GIsPlayInEditorWorld so set it again
				GIsPlayInEditorWorld = true;
			}
			// Otherwise we are probably loading new map while in PIE, so we need to rename world package and all streaming levels
			else if (Pending == NULL)
			{
				NewWorld->RenameToPIEWorld(WorldContext.PIEInstance);
			}
			ResetPIEAudioSetting(NewWorld);
		}
		else if (WorldContext.WorldType == EWorldType::Game)
		{
			// If we are a game world and the world we just found is already initialized, then we're probably trying to load
			// an independent fresh copy of the world into a different context. Create a package with a prefixed name
			// and load the world from disk to keep the instances independent. If this is the case, assume the creator
			// of the FWorldContext was aware and set WorldContext.PIEInstance to a valid value.
			if (NewWorld->bIsWorldInitialized && WorldContext.PIEInstance != -1)
			{
				NewWorld = CreatePIEWorldByLoadingFromPackage(WorldContext, URL.Map, WorldPackage);

				if (NewWorld == nullptr)
				{
					Error = FString::Printf(TEXT("Failed to load package '%s' into a new game world."), *URL.Map);
					return false;
				}
			}
		}
	}
	NewWorld->SetGameInstance(WorldContext.OwningGameInstance);

	GWorld = NewWorld;

	WorldContext.SetCurrentWorld(NewWorld);
	WorldContext.World()->WorldType = WorldContext.WorldType;
	
#if !UE_BUILD_SHIPPING
	GWorld->bCreateRenderStateForHiddenComponents = bOldWorldWasShowingCollisionForHiddenComponents;
#endif
	
	// Fixme: hacky but we need to set PackageFlags here if we are in a PIE Context.
	// Also, don't add to root when in PIE, since PIE doesn't remove world from root
	if (WorldContext.WorldType == EWorldType::PIE)
	{
		check(WorldContext.World()->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor));
		WorldContext.World()->ClearFlags(RF_Standalone);
	}
	else
	{
		WorldContext.World()->AddToRoot();
	}

	// In the PIE case the world will already have been initialized as part of CreatePIEWorldByDuplication
	if (!WorldContext.World()->bIsWorldInitialized)
	{
		WorldContext.World()->InitWorld();
	}

	// Handle pending level.
	if( Pending )
	{
		check(Pending == WorldContext.PendingNetGame);
		MovePendingLevel(WorldContext);
	}
	else
	{
		check(!WorldContext.World()->GetNetDriver());
	}

	WorldContext.World()->SetGameMode(URL);

	if (FAudioDevice* AudioDevice = WorldContext.World()->GetAudioDevice())
	{
		AudioDevice->SetDefaultBaseSoundMix(WorldContext.World()->GetWorldSettings()->DefaultBaseSoundMix);
	}

	// Listen for clients.
	if (Pending == NULL && (!GIsClient || URL.HasOption(TEXT("Listen"))))
	{
		if (!WorldContext.World()->Listen(URL))
		{
			UE_LOG(LogNet, Error, TEXT("LoadMap: failed to Listen(%s)"), *URL.ToString());
		}
	}

	const TCHAR* MutatorString = URL.GetOption(TEXT("Mutator="), TEXT(""));
	if (MutatorString)
	{
		TArray<FString> Mutators;
		FString(MutatorString).ParseIntoArray(Mutators, TEXT(","), true);

		for (int32 MutatorIndex = 0; MutatorIndex < Mutators.Num(); MutatorIndex++)
		{
			LoadPackagesFully(WorldContext.World(), FULLYLOAD_Mutator, Mutators[MutatorIndex]);
		}
	}

	// Process global shader results before we try to render anything
	// Do this before we register components, as USkinnedMeshComponents require the GPU skin cache global shaders when creating render state.
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->ProcessAsyncResults(false, true);
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UEngine::LoadMap.LoadPackagesFully"), STAT_LoadMap_LoadPackagesFully, STATGROUP_LoadTime);

		// load any per-map packages
		check(WorldContext.World()->PersistentLevel);
		LoadPackagesFully(WorldContext.World(), FULLYLOAD_Map, WorldContext.World()->PersistentLevel->GetOutermost()->GetName());

		// Make sure "always loaded" sub-levels are fully loaded
		WorldContext.World()->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);

		if (!GIsEditor && !IsRunningDedicatedServer())
		{
			// If requested, duplicate dynamic levels here after the source levels are created.
			WorldContext.World()->DuplicateRequestedLevels(FName(*URL.Map));
		}
	}
	
	// Note that AI system will be created only if ai-system-creation conditions are met
	WorldContext.World()->CreateAISystem();

	// Initialize gameplay for the level.
	WorldContext.World()->InitializeActorsForPlay(URL);		

	// calling it after InitializeActorsForPlay has been called to have all potential bounding boxed initialized
	UNavigationSystem::InitializeForWorld(WorldContext.World(), FNavigationSystemRunMode::GameMode);

	// Remember the URL. Put this before spawning player controllers so that
	// a player controller can get the map name during initialization and
	// have it be correct
	WorldContext.LastURL = URL;
	WorldContext.LastURL.Map = URLTrueMapName;

	if (WorldContext.World()->GetNetMode() == NM_Client)
	{
		WorldContext.LastRemoteURL = URL;
	}

	// Spawn play actors for all active local players
	if (WorldContext.OwningGameInstance != NULL)
	{
		for(auto It = WorldContext.OwningGameInstance->GetLocalPlayerIterator(); It; ++It)
		{
			FString Error2;
			if(!(*It)->SpawnPlayActor(URL.ToString(1),Error2,WorldContext.World()))
			{
				UE_LOG(LogEngine, Fatal, TEXT("Couldn't spawn player: %s"), *Error2);
			}
		}
	}

	// Prime texture streaming.
	IStreamingManager::Get().NotifyLevelChange();

	if (GEngine && GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->OnBeginPlay(WorldContext);
	}
	WorldContext.World()->BeginPlay();

	// send a callback message
	PostLoadMapCaller.bCalled = true;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FCoreUObjectDelegates::PostLoadMap.Broadcast();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FCoreUObjectDelegates::PostLoadMapWithWorld.Broadcast(WorldContext.World());
	
	WorldContext.World()->bWorldWasLoadedThisTick = true;

	// We want to update streaming immediately so that there's no tick prior to processing any levels that should be initially visible
	// that requires calculating the scene, so redraw everything now to take care of it all though don't present the frame.
	RedrawViewports(false);

	// RedrawViewports() may have added a dummy playerstart location. Remove all views to start from fresh the next Tick().
	IStreamingManager::Get().RemoveStreamingViews( RemoveStreamingViews_All );
	
	// See if we need to record network demos
	if ( WorldContext.World()->GetAuthGameMode() == NULL || !WorldContext.World()->GetAuthGameMode()->IsHandlingReplays() )
	{
		if ( URL.HasOption( TEXT( "DemoRec" ) ) && WorldContext.OwningGameInstance != nullptr )
		{
			const TCHAR* DemoRecName = URL.GetOption( TEXT( "DemoRec=" ), NULL );

			// Record the demo, optionally with the specified custom name.
			WorldContext.OwningGameInstance->StartRecordingReplay( FString(DemoRecName), WorldContext.World()->GetMapName(), URL.Op );
		}
	}

	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "LoadMapComplete - " ) + URL.Map )) );
	MALLOC_PROFILER( FMallocProfiler::SnapshotMemoryLoadMapEnd( URL.Map ); )

	double StopTime = FPlatformTime::Seconds();

	UE_LOG(LogLoad, Log, TEXT("Took %f seconds to LoadMap(%s)"), StopTime - StartTime, *URL.Map);
	FLoadTimeTracker::Get().DumpRawLoadTimes();
	WorldContext.OwningGameInstance->LoadComplete(StopTime - StartTime, *URL.Map);

	// Successfully started local level.
	return true;
}

void UEngine::TrimMemory()
{
	// Clean up the previous level out of memory.
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);

	// For platforms which manage GPU memory directly we must Enqueue a flush, and wait for it to be processed
	// so that any pending frees that depend on the GPU will be processed.  Otherwise a whole map's worth of GPU memory
	// may be unavailable to load the next one.
	ENQUEUE_UNIQUE_RENDER_COMMAND(FlushCommand,
	{
		GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
		RHIFlushResources();
		GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	}
	);
	FlushRenderingCommands();

	// Ask systems to trim memory where possible
	FCoreDelegates::GetMemoryTrimDelegate().Broadcast();
}

void UEngine::BlockTillLevelStreamingCompleted(UWorld* InWorld)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UEngine_BlockTillLevelStreamingCompleted);

	check(InWorld);
	
	// Update streaming levels state using streaming volumes
	InWorld->ProcessLevelStreamingVolumes();

	if (InWorld->WorldComposition)
	{
		InWorld->WorldComposition->UpdateStreamingState();
	}

	// Probe if we have anything to do
	InWorld->UpdateLevelStreaming();
	bool bWorkToDo = (InWorld->IsVisibilityRequestPending() || IsAsyncLoading());
	
	if (bWorkToDo)
	{
		if( GameViewport && GEngine->BeginStreamingPauseDelegate && GEngine->BeginStreamingPauseDelegate->IsBound() )
		{
			GEngine->BeginStreamingPauseDelegate->Execute( GameViewport->Viewport );
		}	

		// Flush level streaming requests, blocking till completion.
		InWorld->FlushLevelStreaming(EFlushLevelStreamingType::Full);

		if( GEngine->EndStreamingPauseDelegate && GEngine->EndStreamingPauseDelegate->IsBound() )
		{
			GEngine->EndStreamingPauseDelegate->Execute( );
		}	
	}
}

void UEngine::CleanupPackagesToFullyLoad(FWorldContext &Context, EFullyLoadPackageType FullyLoadType, const FString& Tag)
{
	//UE_LOG(LogEngine, Log, TEXT("------------------ CleanupPackagesToFullyLoad: %i, %s"),(int32)FullyLoadType, *Tag);
	for (int32 MapIndex = 0; MapIndex < Context.PackagesToFullyLoad.Num(); MapIndex++)
	{
		FFullyLoadedPackagesInfo& PackagesInfo = Context.PackagesToFullyLoad[MapIndex];
		// is this entry for the map/game?
		if (PackagesInfo.FullyLoadType == FullyLoadType && (PackagesInfo.Tag == Tag || Tag == TEXT("")))
		{
			// mark all objects from this map as unneeded
			for (int32 ObjectIndex = 0; ObjectIndex < PackagesInfo.LoadedObjects.Num(); ObjectIndex++)
			{
//				UE_LOG(LogEngine, Log, TEXT("Removing %s from root"), *PackagesInfo.LoadedObjects(ObjectIndex)->GetFullName());
				PackagesInfo.LoadedObjects[ObjectIndex]->RemoveFromRoot();	
			}
			// empty the array of pointers to the objects
			PackagesInfo.LoadedObjects.Empty();
		}
	}
}

void UEngine::CancelPendingMapChange(FWorldContext &Context)
{
	// Empty intermediate arrays.
	Context.LevelsToLoadForPendingMapChange.Empty();
	Context.LoadedLevelsForPendingMapChange.Empty();

	// Reset state and make sure conditional map change doesn't fire.
	Context.PendingMapChangeFailureDescription	= TEXT("");
	Context.bShouldCommitPendingMapChange		= false;
	
	// Reset array of levels to prepare for client.
	if( Context.World() )
	{
		Context.World()->PreparingLevelNames.Empty();
	}
}

/** Clear out the debug properties array that is storing values to show on the screen */
void UEngine::ClearDebugDisplayProperties()
{
	for (int32 i = 0; i < GameViewport->DebugProperties.Num(); i++)
	{
		if (GameViewport->DebugProperties[i].Obj == NULL)
		{
			GameViewport->DebugProperties.RemoveAt(i, 1);
			i--;
		}
		else
		{
			for (UObject* TestObj = GameViewport->DebugProperties[i].Obj; TestObj != NULL; TestObj = TestObj->GetOuter())
			{
				if (TestObj->IsA(ULevel::StaticClass()) || TestObj->IsA(UWorld::StaticClass()) || TestObj->IsA(AActor::StaticClass()))
				{
					GameViewport->DebugProperties.RemoveAt(i, 1);
					i--;
					break;
				}
			}
		}
	}
}

void UEngine::MovePendingLevel(FWorldContext &Context)
{
	check(Context.World());
	check(Context.PendingNetGame);

	Context.World()->SetNetDriver(Context.PendingNetGame->NetDriver);
	
	UNetDriver* NetDriver = Context.PendingNetGame->NetDriver;
	if( NetDriver )
	{
		// The pending net driver is renamed to the current "game net driver"
		NetDriver->SetNetDriverName(NAME_GameNetDriver);
		NetDriver->SetWorld(Context.World());

		FLevelCollection& SourceLevels = Context.World()->FindOrAddCollectionByType(ELevelCollectionType::DynamicSourceLevels);
		SourceLevels.SetNetDriver(NetDriver);
			
		FLevelCollection& StaticLevels = Context.World()->FindOrAddCollectionByType(ELevelCollectionType::StaticLevels);
		StaticLevels.SetNetDriver(NetDriver);
	}

	// Attach the DemoNetDriver to the world if there is one
	if ( Context.PendingNetGame->DemoNetDriver != NULL )
	{
		Context.PendingNetGame->DemoNetDriver->SetWorld( Context.World() );
		Context.World()->DemoNetDriver = Context.PendingNetGame->DemoNetDriver;

		FLevelCollection& MainLevels = Context.World()->FindOrAddCollectionByType(ELevelCollectionType::DynamicSourceLevels);
		MainLevels.SetDemoNetDriver(Context.PendingNetGame->DemoNetDriver);
	}

	// Reset the Navigation System
	Context.World()->SetNavigationSystem(NULL);
}

void UEngine::LoadPackagesFully(UWorld * InWorld, EFullyLoadPackageType FullyLoadType, const FString& Tag)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	//UE_LOG(LogEngine, Log, TEXT("------------------ LoadPackagesFully: %i, %s"),(int32)FullyLoadType, *Tag);

	// look for all entries for the given map
	for (int32 MapIndex = ((Tag == TEXT("___TAILONLY___")) ? Context.PackagesToFullyLoad.Num() - 1 : 0); MapIndex < Context.PackagesToFullyLoad.Num(); MapIndex++)
	{
		FFullyLoadedPackagesInfo& PackagesInfo = Context.PackagesToFullyLoad[MapIndex];
		/*
		for (int32 PackageIndex = 0; PackageIndex < PackagesInfo.PackagesToLoad.Num(); PackageIndex++)
		{
			UE_LOG(LogEngine, Log, TEXT("--------------------- Considering: %i, %s, %s"),(int32)PackagesInfo.FullyLoadType, *PackagesInfo.Tag, *PackagesInfo.PackagesToLoad(PackageIndex).ToString());
		}
		*/

		// is this entry for the map/game?
		if (PackagesInfo.FullyLoadType == FullyLoadType && (PackagesInfo.Tag == Tag || Tag == TEXT("") || Tag == TEXT("___TAILONLY___")))
		{
			// go over all packages that need loading
			for (int32 PackageIndex = 0; PackageIndex < PackagesInfo.PackagesToLoad.Num(); PackageIndex++)
			{
				//UE_LOG(LogEngine, Log, TEXT("------------------------ looking for %s"),*PackagesInfo.PackagesToLoad(PackageIndex).ToString());

				// look for the package in the package cache
				FString SFPackageName = PackagesInfo.PackagesToLoad[PackageIndex].ToString() + STANDALONE_SEEKFREE_SUFFIX;
				bool bFoundFile = false;
				FString PackagePath;
				if (FPackageName::DoesPackageExist(SFPackageName, NULL, &PackagePath))
				{
					bFoundFile = true;
				}
				else if ( (FPackageName::DoesPackageExist(PackagesInfo.PackagesToLoad[PackageIndex].ToString(), NULL, &PackagePath)) )
				{
					bFoundFile = true;
				}
				if (bFoundFile)
				{
					// load the package
					// @todo: This would be nice to be async probably, but how would we add it to the root? (LOAD_AddPackageToRoot?)
					//UE_LOG(LogEngine, Log, TEXT("------------------ Fully loading %s"), *PackagePath);
					UPackage* Package = LoadPackage(NULL, *PackagePath, 0);

					// add package to root so we can find it
					Package->AddToRoot();

					// remember the object for unloading later
					PackagesInfo.LoadedObjects.Add(Package);

					// add the objects to the root set so that it will not be GC'd
					for (TObjectIterator<UObject> It; It; ++It)
					{
						if (It->IsIn(Package))
						{
//							UE_LOG(LogEngine, Log, TEXT("Adding %s to root"), *It->GetFullName());
							It->AddToRoot();

							// remember the object for unloading later
							PackagesInfo.LoadedObjects.Add(*It);
						}
					}
				}
				else
				{
					UE_LOG(LogEngine, Log, TEXT("Failed to find Package %s to FullyLoad [FullyLoadType = %d, Tag = %s]"), *PackagesInfo.PackagesToLoad[PackageIndex].ToString(), (int32)FullyLoadType, *Tag);
				}
			}
		}
		/*
		else
		{
			UE_LOG(LogEngine, Log, TEXT("DIDN't MATCH!!!"));
			for (int32 PackageIndex = 0; PackageIndex < PackagesInfo.PackagesToLoad.Num(); PackageIndex++)
			{
				UE_LOG(LogEngine, Log, TEXT("DIDN't MATCH!!! %i, \"%s\"(\"%s\"), %s"),(int32)PackagesInfo.FullyLoadType, *PackagesInfo.Tag, *Tag, *PackagesInfo.PackagesToLoad(PackageIndex).ToString());
			}
		}
		*/
	}
}


void UEngine::UpdateTransitionType(UWorld *CurrentWorld)
{
	// Update the transition screen.
	if(TransitionType == TT_Connecting)
	{
		// Check to see if all players have finished connecting.
		TransitionType = TT_None;

		FWorldContext &Context = GetWorldContextFromWorldChecked(CurrentWorld);
		if (Context.OwningGameInstance != NULL)
		{
			for(auto It = Context.OwningGameInstance->GetLocalPlayerIterator(); It; ++It)
			{
				if(!(*It)->PlayerController)
				{
					// This player has not received a PlayerController from the server yet, so leave the connecting screen up.
					TransitionType = TT_Connecting;
					break;
				}
			}
		}
	}
	else if(TransitionType == TT_None || TransitionType == TT_Paused)
	{
		// Display a paused screen if the game is paused.
		TransitionType = (CurrentWorld->GetWorldSettings()->Pauser != NULL) ? TT_Paused : TT_None;
	}
}

FWorldContext& UEngine::CreateNewWorldContext(EWorldType::Type WorldType)
{
	FWorldContext *NewWorldContext = (new (WorldList) FWorldContext);
	NewWorldContext->WorldType = WorldType;
	NewWorldContext->ContextHandle = FName(*FString::Printf(TEXT("Context_%d"), NextWorldContextHandle++));
	
	return *NewWorldContext;
}

FWorldContext& HandleInvalidWorldContext()
{
	if (!IsRunningCommandlet())
	{
		UE_LOG(LogLoad, Error, TEXT("WorldContext requested with invalid context object.") );
		check(false);
	}
	
	return GEngine->CreateNewWorldContext(EWorldType::None);
}

FWorldContext* UEngine::GetWorldContextFromHandle(const FName WorldContextHandle)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.ContextHandle == WorldContextHandle)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromHandle(const FName WorldContextHandle) const
{
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.ContextHandle == WorldContextHandle)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext& UEngine::GetWorldContextFromHandleChecked(const FName WorldContextHandle)
{
	if (FWorldContext* WorldContext = GetWorldContextFromHandle(WorldContextHandle))
	{
		return *WorldContext;
	}

	UE_LOG(LogLoad, Warning, TEXT("WorldContext requested with invalid context handle %s"), *WorldContextHandle.ToString());
	return HandleInvalidWorldContext();
}

const FWorldContext& UEngine::GetWorldContextFromHandleChecked(const FName WorldContextHandle) const
{
	if (const FWorldContext* WorldContext = GetWorldContextFromHandle(WorldContextHandle))
	{
		return *WorldContext;
	}

	UE_LOG(LogLoad, Warning, TEXT("WorldContext requested with invalid context handle %s"), *WorldContextHandle.ToString());
	return HandleInvalidWorldContext();
}

FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.World() == InWorld)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromWorld(const UWorld* InWorld) const
{
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.World() == InWorld)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext& UEngine::GetWorldContextFromWorldChecked(const UWorld *InWorld)
{
	if (FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

const FWorldContext& UEngine::GetWorldContextFromWorldChecked(const UWorld *InWorld) const
{
	if (const FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

UGameViewportClient* UEngine::GameViewportForWorld(const UWorld *InWorld) const
{
	const FWorldContext* Context = GetWorldContextFromWorld(InWorld);
	return (Context ? Context->GameViewport : NULL);
}

bool UEngine::AreGameAnalyticsEnabled() const
{ 
	return FPlatformMisc::AllowSendAnonymousGameUsageDataToEpic() && GetDefault<UEndUserSettings>()->bSendAnonymousUsageDataToEpic;
}

bool UEngine::AreGameAnalyticsAnonymous() const
{
	return !GetDefault<UEndUserSettings>()->bAllowUserIdInUsageData;
}

bool UEngine::AreGameMTBFEventsEnabled() const
{
	return GetDefault<UEndUserSettings>()->bSendMeanTimeBetweenFailureDataToEpic;
}

void UEngine::SetIsVanillaProduct(bool bInIsVanillaProduct)
{
	// set bIsVanillaProduct and if it changes broadcast the core delegate
	static bool bFirstCall = true;
	if (bFirstCall || bInIsVanillaProduct != bIsVanillaProduct)
	{
		bFirstCall = false;
		bIsVanillaProduct = bInIsVanillaProduct;
		FCoreDelegates::IsVanillaProductChanged.Broadcast(bIsVanillaProduct);
	}
}

FWorldContext* UEngine::GetWorldContextFromGameViewport(const UGameViewportClient *InViewport)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.GameViewport == InViewport)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromGameViewport(const UGameViewportClient *InViewport) const
{
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.GameViewport == InViewport)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext& UEngine::GetWorldContextFromGameViewportChecked(const UGameViewportClient *InViewport)
{
	if (FWorldContext* WorldContext = GetWorldContextFromGameViewport(InViewport))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

const FWorldContext& UEngine::GetWorldContextFromGameViewportChecked(const UGameViewportClient *InViewport) const
{
	if (const FWorldContext* WorldContext = GetWorldContextFromGameViewport(InViewport))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

FWorldContext* UEngine::GetWorldContextFromPendingNetGame(const UPendingNetGame *InPendingNetGame)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.PendingNetGame == InPendingNetGame)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromPendingNetGame(const UPendingNetGame *InPendingNetGame) const
{
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.PendingNetGame == InPendingNetGame)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext& UEngine::GetWorldContextFromPendingNetGameChecked(const UPendingNetGame *InPendingNetGame)
{
	if (FWorldContext* WorldContext = GetWorldContextFromPendingNetGame(InPendingNetGame))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

const FWorldContext& UEngine::GetWorldContextFromPendingNetGameChecked(const UPendingNetGame *InPendingNetGame) const
{
	if (const FWorldContext* WorldContext = GetWorldContextFromPendingNetGame(InPendingNetGame))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

FWorldContext* UEngine::GetWorldContextFromPendingNetGameNetDriver(const UNetDriver *InPendingNetDriver)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.PendingNetGame && WorldContext.PendingNetGame->NetDriver == InPendingNetDriver)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromPendingNetGameNetDriver(const UNetDriver *InPendingNetDriver) const
{
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.PendingNetGame && WorldContext.PendingNetGame->NetDriver == InPendingNetDriver)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext& UEngine::GetWorldContextFromPendingNetGameNetDriverChecked(const UNetDriver *InPendingNetDriver)
{
	if (FWorldContext* WorldContext = GetWorldContextFromPendingNetGameNetDriver(InPendingNetDriver))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

const FWorldContext& UEngine::GetWorldContextFromPendingNetGameNetDriverChecked(const UNetDriver *InPendingNetDriver) const
{
	if (const FWorldContext* WorldContext = GetWorldContextFromPendingNetGameNetDriver(InPendingNetDriver))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

FWorldContext* UEngine::GetWorldContextFromPIEInstance(const int32 PIEInstance)
{
	for (FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.PIEInstance == PIEInstance)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

const FWorldContext* UEngine::GetWorldContextFromPIEInstance(const int32 PIEInstance) const
{
	for (const FWorldContext& WorldContext : WorldList)
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.PIEInstance == PIEInstance)
		{
			return &WorldContext;
		}
	}
	return nullptr;
}

FWorldContext& UEngine::GetWorldContextFromPIEInstanceChecked(const int32 PIEInstance)
{
	if (FWorldContext* WorldContext = GetWorldContextFromPIEInstance(PIEInstance))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

const FWorldContext& UEngine::GetWorldContextFromPIEInstanceChecked(const int32 PIEInstance) const
{
	if (const FWorldContext* WorldContext = GetWorldContextFromPIEInstance(PIEInstance))
	{
		return *WorldContext;
	}
	return HandleInvalidWorldContext();
}

UPendingNetGame* UEngine::PendingNetGameFromWorld( UWorld* InWorld )
{
	return GetWorldContextFromWorldChecked(InWorld).PendingNetGame;
}

void UEngine::DestroyWorldContext(UWorld * InWorld)
{
	for (int32 idx=0; idx < WorldList.Num(); ++idx)
	{
		if (WorldList[idx].World() == InWorld)
		{
#if WITH_EDITOR
			WorldContextDestroyedEvent.Broadcast(WorldList[idx]);
#endif
			// Set the current world to NULL so that any external referencers are cleaned up before we remove
			WorldList[idx].SetCurrentWorld(NULL);
			WorldList.RemoveAt(idx);
			break;
		}
	}
}

bool UEngine::WorldHasValidContext(UWorld *InWorld)
{
	return (GetWorldContextFromWorld(InWorld) != NULL);
}

bool UEngine::IsWorldDuplicate(const UWorld* const InWorld)
{
	// World is considered a duplicate if it's the outer of a level in a duplicate levels collection
	for (const FWorldContext& Context : WorldList)
	{
		if (Context.World())
		{	
			const FLevelCollection* const Collection = Context.World()->FindCollectionByType(ELevelCollectionType::DynamicDuplicatedLevels);
			if (Collection)
			{
				for (const ULevel* const Level : Collection->GetLevels())
				{
					if (Level->GetOuter() == InWorld)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void UEngine::VerifyLoadMapWorldCleanup()
{
	// All worlds at this point should be the CurrentWorld of some context, preview worlds, or streaming level
	// worlds that are owned by the CurrentWorld of some context.
	for( TObjectIterator<UWorld> It; It; ++It )
	{
		UWorld* World = *It;
		const bool bIsPersistantWorldType = (World->WorldType == EWorldType::Inactive) || (World->WorldType == EWorldType::EditorPreview);
		if (!bIsPersistantWorldType && !WorldHasValidContext(World))
		{
			if ((World->PersistentLevel == nullptr || !WorldHasValidContext(World->PersistentLevel->OwningWorld)) && !IsWorldDuplicate(World))
			{
				// Print some debug information...
				UE_LOG(LogLoad, Log, TEXT("%s not cleaned up by garbage collection! "), *World->GetFullName());
				StaticExec(World, *FString::Printf(TEXT("OBJ REFS CLASS=WORLD NAME=%s"), *World->GetPathName()));
				TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( World, true, GARBAGE_COLLECTION_KEEPFLAGS );
				FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, World );
				UE_LOG(LogLoad, Log, TEXT("%s"),*ErrorString);
				// before asserting.

				UE_LOG(LogLoad, Fatal, TEXT("%s not cleaned up by garbage collection!") LINE_TERMINATOR TEXT("%s") , *World->GetFullName(), *ErrorString );
			}
		}
	}
}


/*-----------------------------------------------------------------------------
	Async persistent level map change.
-----------------------------------------------------------------------------*/

/**
 * Callback function used in UGameEngine::PrepareMapChange to pass to LoadPackageAsync.
 *
 * @param	LevelPackage	level package that finished async loading
 * @param	InGameEngine	pointer to game engine object to associated loaded level with so it won't be GC'ed
 */
static void AsyncMapChangeLevelLoadCompletionCallback(const FName& PackageName, UPackage* LevelPackage, EAsyncLoadingResult::Type Result, FName InWorldHandle)
{
	FWorldContext &Context = GEngine->GetWorldContextFromHandleChecked( InWorldHandle );

	if( LevelPackage )
	{	
		// Try to find a UWorld object in the level package.
		UWorld* World = UWorld::FindWorldInPackage(LevelPackage);

		// If the world was not found, try to follow a redirector if it exists
		if ( !World )
		{
			World = UWorld::FollowWorldRedirectorInPackage(LevelPackage);
			if ( World )
			{
				LevelPackage = World->GetOutermost();
			}
		}

		ULevel* Level = World ? World->PersistentLevel : nullptr;	
		
		// Print out a warning and set the error if we couldn't find a level in this package.
		if( !Level )
		{
			// NULL levels can happen if existing package but not level is specified as a level name.
			Context.PendingMapChangeFailureDescription = FString::Printf(TEXT("Couldn't find level in package %s"), *LevelPackage->GetName());
			UE_LOG(LogEngine, Error, TEXT( "ERROR ERROR %s was not found in the PackageCache It must exist or the Level Loading Action will FAIL!!!! " ), *LevelPackage->GetName() );
			UE_LOG(LogEngine, Warning, TEXT("%s"), *Context.PendingMapChangeFailureDescription );
			UE_LOG(LogEngine, Error, TEXT( "ERROR ERROR %s was not found in the PackageCache It must exist or the Level Loading Action will FAIL!!!! " ), *LevelPackage->GetName() );
		}

		// Add loaded level to array to prevent it from being garbage collected.
		Context.LoadedLevelsForPendingMapChange.Add( Level );
	}
	else
	{
		// Add NULL entry so we don't end up waiting forever on a level that is never going to be loaded.
		Context.LoadedLevelsForPendingMapChange.Add( nullptr );
		UE_LOG(LogEngine, Warning, TEXT("NULL LevelPackage as argument to AsyncMapChangeLevelCompletionCallback") );
	}

	STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "PrepareMapChangeComplete - " ) + PackageName.ToString() )) );
}


bool UEngine::PrepareMapChange(FWorldContext &Context, const TArray<FName>& LevelNames)
{
	// make sure level streaming isn't frozen
	Context.World()->bIsLevelStreamingFrozen = false;

	// Make sure we don't interrupt a pending map change in progress.
	if( !IsPreparingMapChange(Context) )
	{
		Context.LevelsToLoadForPendingMapChange.Empty();
		Context.LevelsToLoadForPendingMapChange += LevelNames;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Verify that all levels specified are in the package file cache.
		for( int32 LevelIndex=0; LevelIndex < Context.LevelsToLoadForPendingMapChange.Num(); LevelIndex++ )
		{
			const FName LevelName = Context.LevelsToLoadForPendingMapChange[LevelIndex];
			if( !FPackageName::DoesPackageExist( LevelName.ToString() ) )
			{
				Context.LevelsToLoadForPendingMapChange.Empty();
				Context.PendingMapChangeFailureDescription = FString::Printf(TEXT("Couldn't find package for level '%s'"), *LevelName.ToString());
				// write it out immediately so make sure it's in the log even without a CommitMapChange happening
				UE_LOG(LogEngine, Warning, TEXT("PREPAREMAPCHANGE: %s"), *Context.PendingMapChangeFailureDescription);

				// tell user on screen!
				extern bool GIsPrepareMapChangeBroken;
				GIsPrepareMapChangeBroken = true;

				return false;
			}
			//@todo streaming: make sure none of the maps are already loaded/ being loaded?
		}
#endif

		// copy LevelNames into the WorldInfo's array to keep track of the map change that we're preparing (primarily for servers so clients that join in progress can be notified)
		if (Context.World() != nullptr)
		{
			Context.World()->PreparingLevelNames = LevelNames;
		}

		// Kick off async loading of packages.
		for( int32 LevelIndex=0; LevelIndex < Context.LevelsToLoadForPendingMapChange.Num(); LevelIndex++ )
		{
			const FName LevelName = Context.LevelsToLoadForPendingMapChange[LevelIndex];
		
			STAT_ADD_CUSTOMMESSAGE_NAME( STAT_NamedMarker, *(FString( TEXT( "PrepareMapChange - " ) + LevelName.ToString() )) );
			LoadPackageAsync(LevelName.ToString(),
				FLoadPackageAsyncDelegate::CreateStatic(&AsyncMapChangeLevelLoadCompletionCallback, Context.ContextHandle)
				);
		}

		return true;
	}
	else
	{
		Context.PendingMapChangeFailureDescription = TEXT("Current map change still in progress");
		return false;
	}
}


FString UEngine::GetMapChangeFailureDescription(FWorldContext &Context)
{
	return Context.PendingMapChangeFailureDescription;
}
	

bool UEngine::IsPreparingMapChange(FWorldContext &Context)
{
	return Context.LevelsToLoadForPendingMapChange.Num() > 0;
}
	

bool UEngine::IsReadyForMapChange(FWorldContext &Context)
{
	return IsPreparingMapChange(Context) && (Context.LevelsToLoadForPendingMapChange.Num() == Context.LoadedLevelsForPendingMapChange.Num());
}


void UEngine::ConditionalCommitMapChange(FWorldContext &Context)
{
	// Check whether there actually is a pending map change and whether we want it to be committed yet.
	if( Context.bShouldCommitPendingMapChange && IsPreparingMapChange(Context) )
	{
		// Block on remaining async data.
		if( !IsReadyForMapChange(Context) )
		{
			FlushAsyncLoading();
			check( IsReadyForMapChange(Context) );
		}
		
		// Perform map change.
		if (!CommitMapChange(Context.World()))
		{
			UE_LOG(LogEngine, Warning, TEXT("Committing map change via %s was not successful: %s"), *GetFullName(), *GetMapChangeFailureDescription(Context));
		}
		// No pending map change - called commit without prepare.
		else
		{
			UE_LOG(LogEngine, Log, TEXT("Committed map change via %s"), *GetFullName());
		}

		// We just commited, so reset the flag.
		Context.bShouldCommitPendingMapChange = false;
	}
}

/** struct to temporarily hold on to already loaded but unbound levels we're going to make visible at the end of CommitMapChange() while we first trigger GC */
struct FPendingStreamingLevelHolder : public FGCObject
{
public:
	TArray<ULevel*> Levels;

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObjects( Levels ); 
	}
};


bool UEngine::CommitMapChange( FWorldContext &Context )
{
	if (!IsPreparingMapChange(Context))
	{
		Context.PendingMapChangeFailureDescription = TEXT("No map change is being prepared");
		return false;
	}
	else if (!IsReadyForMapChange(Context))
	{
		Context.PendingMapChangeFailureDescription = TEXT("Map change is not ready yet");
		return false;
	}
	else
	{
		check(Context.World());

		AGameModeBase* GameMode = Context.World()->GetAuthGameMode();

		// tell the game we are about to switch levels
		if (GameMode)
		{
			// get the actual persistent level's name
			FString PreviousMapName = Context.World()->PersistentLevel->GetOutermost()->GetName();
			FString NextMapName = Context.LevelsToLoadForPendingMapChange[0].ToString();

			// look for a persistent streamed in sublevel
			for (int32 LevelIndex = 0; LevelIndex < Context.World()->StreamingLevels.Num(); LevelIndex++)
			{
				ULevelStreamingPersistent* PersistentLevel = Cast<ULevelStreamingPersistent>(Context.World()->StreamingLevels[LevelIndex]);
				if (PersistentLevel)
				{
					PreviousMapName = PersistentLevel->GetWorldAssetPackageName();
					// only one persistent level
					break;
				}
			}
			FGameDelegates::Get().GetPreCommitMapChangeDelegate().Broadcast(PreviousMapName, NextMapName);
		}

		// on the client, check if we already loaded pending levels to be made visible due to e.g. the PackageMap
		FPendingStreamingLevelHolder LevelHolder;
		if (Context.PendingLevelStreamingStatusUpdates.Num() > 0)
		{
			// Iterating over GCed ULevels. A TObjectIterator<ULevel> can not do this.
			for (TObjectIterator<UObject> It(true); It; ++It)
			{
				ULevel* Level = Cast<ULevel>(*It);
				if ( Level )
				{
					for (int32 i = 0; i < Context.PendingLevelStreamingStatusUpdates.Num(); i++)
					{
						if ( Level->GetOutermost()->GetFName() == Context.PendingLevelStreamingStatusUpdates[i].PackageName && 
							(Context.PendingLevelStreamingStatusUpdates[i].bShouldBeLoaded || Context.PendingLevelStreamingStatusUpdates[i].bShouldBeVisible) )
						{
							LevelHolder.Levels.Add(Level);
							break;
						}
					}
				}
			}
		}

		// we are no longer preparing this change
		Context.World()->PreparingLevelNames.Empty();

		// Iterate over level collection, marking them to be forcefully unloaded.
		for( int32 LevelIndex=0; LevelIndex < Context.World()->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel	= Context.World()->StreamingLevels[LevelIndex];
			if( StreamingLevel )
			{
				StreamingLevel->bIsRequestingUnloadAndRemoval = true;
			}
		}

		// Collect garbage. @todo streaming: make sure that this doesn't stall due to async loading in the background
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );

		// The new fake persistent level is first in the LevelsToLoadForPendingMapChange array.
		FName	FakePersistentLevelName = Context.LevelsToLoadForPendingMapChange[0];
		ULevel*	FakePersistentLevel		= nullptr;
		// copy to WorldInfo to keep track of the last map change we performed (primarily for servers so clients that join in progress can be notified)
		// we don't need to remember secondary levels as the join code iterates over all streaming levels and updates them
		Context.World()->CommittedPersistentLevelName = FakePersistentLevelName;

		// Find level package in loaded levels array.
		for( int32 LevelIndex=0; LevelIndex < Context.LoadedLevelsForPendingMapChange.Num(); LevelIndex++ )
		{
			ULevel* Level = Context.LoadedLevelsForPendingMapChange[LevelIndex];

			// NULL levels can happen if existing package but not level is specified as a level name.
			if( Level && (FakePersistentLevelName == Level->GetOutermost()->GetFName()) )
			{
				FakePersistentLevel = Level;
				break;
			}
		}
		check( FakePersistentLevel );

		// Construct a new ULevelStreamingPersistent for the new persistent level.
		ULevelStreamingPersistent* LevelStreamingPersistent = NewObject<ULevelStreamingPersistent>(
			Context.World(),
			*FString::Printf(TEXT("LevelStreamingPersistent_%s"), *FakePersistentLevel->GetOutermost()->GetName()) );

		// Propagate level and name to streaming object.
		LevelStreamingPersistent->SetLoadedLevel(FakePersistentLevel);
		LevelStreamingPersistent->SetWorldAssetByPackageName(FakePersistentLevelName);
		// And add it to the world info's list of levels.
		Context.World()->StreamingLevels.Add( LevelStreamingPersistent );

		UWorld* FakeWorld = CastChecked<UWorld>(FakePersistentLevel->GetOuter());

		// Rename the newly loaded streaming levels so that their outer is correctly set to the main context's world,
		// rather than the fake world.
		for (ULevelStreaming* const FakeWorldStreamingLevel : FakeWorld->StreamingLevels)
		{
			FakeWorldStreamingLevel->Rename(nullptr, Context.World(), REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		}

		// Move the secondary levels to the world info levels array.
		Context.World()->StreamingLevels += MoveTemp(FakeWorld->StreamingLevels);

		// fixup up any kismet streaming objects to force them to be loaded if they were preloaded, this
		// will keep streaming volumes from immediately unloading the levels that were just loaded
		for( int32 LevelIndex=0; LevelIndex < Context.World()->StreamingLevels.Num(); LevelIndex++ )
		{
			ULevelStreaming* StreamingLevel	= Context.World()->StreamingLevels[LevelIndex];
			// mark any kismet streamers to force be loaded
			if (StreamingLevel)
			{
				bool bWasFound = false;
				// was this one of the packages we wanted to load?
				for (int32 LoadLevelIndex = 0; LoadLevelIndex < Context.LevelsToLoadForPendingMapChange.Num(); LoadLevelIndex++)
				{
					if (Context.LevelsToLoadForPendingMapChange[LoadLevelIndex] == StreamingLevel->GetWorldAssetPackageFName())
					{
						bWasFound = true;
						break;
					}
				}

				// if this level was preloaded, mark it as to be loaded and visible
				if (bWasFound)
				{
					StreamingLevel->bShouldBeLoaded		= true;
					StreamingLevel->bShouldBeVisible	= true;

#if WITH_SERVER_CODE
					if (Context.World()->IsServer())
					{
						// notify players of the change
						for( FConstPlayerControllerIterator Iterator = Context.World()->GetPlayerControllerIterator(); Iterator; ++Iterator )
						{
							(*Iterator)->LevelStreamingStatusChanged( 
									StreamingLevel, 
									StreamingLevel->bShouldBeLoaded, 
									StreamingLevel->bShouldBeVisible,
									StreamingLevel->bShouldBlockOnLoad,
									StreamingLevel->LevelLODIndex);							
						}
					}
#endif // WITH_SERVER_CODE
				}
			}
		}

		// Update level streaming, forcing existing levels to be unloaded and their streaming objects 
		// removed from the world info.	We can't kick off async loading in this update as we want to 
		// collect garbage right below.
		Context.World()->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
		
		// make sure any looping sounds, etc are stopped
		
		if (FAudioDevice* AudioDevice = Context.World()->GetAudioDevice())
		{
			AudioDevice->StopAllSounds();
		}

		// Remove all unloaded levels from memory and perform full purge.
		CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS, true );
		
		// if there are pending streaming changes replicated from the server, apply them immediately
		if (Context.PendingLevelStreamingStatusUpdates.Num() > 0)
		{
			for (const FLevelStreamingStatus& PendingUpdate : Context.PendingLevelStreamingStatusUpdates)
				{
				ULevelStreaming** Found = Context.World()->StreamingLevels.FindByPredicate([&](ULevelStreaming* Level){
					return Level && Level->GetWorldAssetPackageFName() == PendingUpdate.PackageName;
				});

				if (Found)
					{
					(*Found)->bShouldBeLoaded  = PendingUpdate.bShouldBeLoaded;
					(*Found)->bShouldBeVisible = PendingUpdate.bShouldBeVisible;
					(*Found)->LevelLODIndex    = PendingUpdate.LODIndex;
						}
						else
						{
					UE_LOG(LogStreaming, Log, TEXT("Unable to find streaming object %s"), *PendingUpdate.PackageName.ToString());
				}
			}

			Context.PendingLevelStreamingStatusUpdates.Empty();

			Context.World()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
		}
		else
		{
			// Make sure there are no pending visibility requests.
			Context.World()->FlushLevelStreaming(EFlushLevelStreamingType::Visibility);
		}

		// delay the use of streaming volumes for a few frames
		Context.World()->DelayStreamingVolumeUpdates(3);

		// Empty intermediate arrays.
		Context.LevelsToLoadForPendingMapChange.Empty();
		Context.LoadedLevelsForPendingMapChange.Empty();
		Context.PendingMapChangeFailureDescription = TEXT("");

		// Prime texture streaming.
		IStreamingManager::Get().NotifyLevelChange();

		// tell the game we are done switching levels
		if (GameMode)
		{
			FGameDelegates::Get().GetPostCommitMapChangeDelegate().Broadcast();
		}

		return true;
	}
}
void UEngine::AddNewPendingStreamingLevel(UWorld *InWorld, FName PackageName, bool bNewShouldBeLoaded, bool bNewShouldBeVisible, int32 LODIndex)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	new(Context.PendingLevelStreamingStatusUpdates) FLevelStreamingStatus(PackageName, bNewShouldBeLoaded, bNewShouldBeVisible, LODIndex);
}

bool UEngine::ShouldCommitPendingMapChange(const UWorld *InWorld) const
{
	const FWorldContext* WorldContext = GetWorldContextFromWorld(InWorld);
	return (WorldContext ? WorldContext->bShouldCommitPendingMapChange : false);
}

void UEngine::SetShouldCommitPendingMapChange(UWorld *InWorld, bool NewShouldCommitPendingMapChange)
{
	FWorldContext &Context = GetWorldContextFromWorldChecked(InWorld);
	Context.bShouldCommitPendingMapChange = NewShouldCommitPendingMapChange;
}

FSeamlessTravelHandler&	UEngine::SeamlessTravelHandlerForWorld(UWorld *World)
{
	return GetWorldContextFromWorldChecked(World).SeamlessTravelHandler;
}

FURL& UEngine::LastURLFromWorld(UWorld *World)
{
	return GetWorldContextFromWorldChecked(World).LastURL;
}

void UEngine::CreateGameUserSettings()
{
	UGameUserSettings::LoadConfigIni();
	GameUserSettings = NewObject<UGameUserSettings>(GetTransientPackage(), GEngine->GameUserSettingsClass);
	GameUserSettings->LoadSettings();
}

const UGameUserSettings* UEngine::GetGameUserSettings() const
{
	if (GameUserSettings == NULL)
	{
		UEngine* ConstThis = const_cast< UEngine* >( this );	// Hack because Header Generator doesn't yet support mutable keyword
		ConstThis->CreateGameUserSettings();
	}
	return GameUserSettings;
}

UGameUserSettings* UEngine::GetGameUserSettings()
{
	if (GameUserSettings == NULL)
	{
		CreateGameUserSettings();
	}
	return GameUserSettings;
}

// Stores information (such as modified properties) for an instanced object (component or subobject)
// in the old CDO, to allow them to be reapplied to the new instance under the new CDO
struct FInstancedObjectRecord
{
	TArray<uint8> SavedProperties;
	UObject* OldInstance;
};

static TAutoConsoleVariable<int32> CVarDumpCopyPropertiesForUnrelatedObjects(
	TEXT("DumpCopyPropertiesForUnrelatedObjects"),
	0,
	TEXT("Dump the objects that are cross class copied")
	);

/* 
 * Houses base functionality shared between CPFUO archivers (FCPFUOWriter/FCPFUOReader) 
 * Used to track whether tagged data is being processed (and whether we should be serializing it).
 */
struct FCPFUOArchive
{
public:
	FCPFUOArchive(bool bIncludeUntaggedDataIn)
		: bIncludeUntaggedData(bIncludeUntaggedDataIn)
		, TaggedDataScope(0)
	{}

	FCPFUOArchive(const FCPFUOArchive& DataSrc)
		: bIncludeUntaggedData(DataSrc.bIncludeUntaggedData)
		, TaggedDataScope(0)
	{}

protected:
	FORCEINLINE void OpenTaggedDataScope()  { ++TaggedDataScope; }
	FORCEINLINE void CloseTaggedDataScope() { --TaggedDataScope; }

	FORCEINLINE bool IsSerializationEnabled()
	{
		return bIncludeUntaggedData || (TaggedDataScope > 0);
	}

	bool bIncludeUntaggedData;
private:
	int32 TaggedDataScope;
};

/* Serializes and stores property data from a specified 'source' object. Only stores data compatible with a target destination object. */
struct FCPFUOWriter : public FObjectWriter, public FCPFUOArchive
{
public:
	/* Contains the source object's serialized data */
	TArray<uint8> SavedPropertyData;

public:
	FCPFUOWriter(UObject* SrcObject, UObject* DstObject, const UEngine::FCopyPropertiesForUnrelatedObjectsParams& Params)
		: FObjectWriter(SavedPropertyData)
		// if the two objects don't share a common native base class, then they may have different
		// serialization methods, which is dangerous (the data is not guaranteed to be homogeneous)
		// in that case, we have to stick with tagged properties only
		, FCPFUOArchive(FindNativeSuperClass(SrcObject) == FindNativeSuperClass(DstObject))
		, bSkipCompilerGeneratedDefaults(Params.bSkipCompilerGeneratedDefaults)
	{
		ArIgnoreArchetypeRef = true;
		ArNoDelta = !Params.bDoDelta;
		ArIgnoreClassRef = true;
		ArPortFlags |= Params.bCopyDeprecatedProperties ? PPF_UseDeprecatedProperties : PPF_None;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor && !(ArPortFlags & (PPF_DuplicateVerbatim | PPF_DuplicateForPIE)))
		{
			SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(DstObject));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		SrcObject->Serialize(*this);
	}

	//~ Begin FArchive Interface
	virtual void Serialize(void* Data, int64 Num) override
	{
		if (IsSerializationEnabled())
		{
			FObjectWriter::Serialize(Data, Num);
		}
	}

	virtual void MarkScriptSerializationStart(const UObject* Object) override { OpenTaggedDataScope(); }
	virtual void MarkScriptSerializationEnd(const UObject* Object) override   { CloseTaggedDataScope(); }

#if WITH_EDITOR
	virtual bool ShouldSkipProperty(const class UProperty* InProperty) const override
	{
		static FName BlueprintCompilerGeneratedDefaultsName(TEXT("BlueprintCompilerGeneratedDefaults"));
		return bSkipCompilerGeneratedDefaults && InProperty->HasMetaData(BlueprintCompilerGeneratedDefaultsName);
	}
#endif 
	//~ End FArchive Interface

private:
	static UClass* FindNativeSuperClass(UObject* Object)
	{
		UClass* Class = Object->GetClass();
		for (; Class; Class = Class->GetSuperClass())
		{
			if ((Class->ClassFlags & CLASS_Native) != 0)
			{
				break;
			}
		}
		return Class;
	}

	bool bSkipCompilerGeneratedDefaults;
};

/* Responsible for applying the saved property data from a FCPFUOWriter to a specified object */
struct FCPFUOReader : public FObjectReader, public FCPFUOArchive
{
public:
	FCPFUOReader(FCPFUOWriter& DataSrc, UObject* DstObject)
		: FObjectReader(DataSrc.SavedPropertyData)
		, FCPFUOArchive(DataSrc)
	{
		ArIgnoreArchetypeRef = true;
		ArIgnoreClassRef = true;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor && !(ArPortFlags & (PPF_DuplicateVerbatim | PPF_DuplicateForPIE)))
		{
			SetLocalizationNamespace(TextNamespaceUtil::EnsurePackageNamespace(DstObject));
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		DstObject->Serialize(*this);
	}

	//~ Begin FArchive Interface
	virtual void Serialize(void* Data, int64 Num) override
	{
		if (IsSerializationEnabled())
		{
			FObjectReader::Serialize(Data, Num);
		}
	}

	virtual void MarkScriptSerializationStart(const UObject* Object) override { OpenTaggedDataScope(); }
	virtual void MarkScriptSerializationEnd(const UObject* Object) override   { CloseTaggedDataScope(); }
	// ~End FArchive Interface
};

void UEngine::CopyPropertiesForUnrelatedObjects(UObject* OldObject, UObject* NewObject, FCopyPropertiesForUnrelatedObjectsParams Params)
{
	check(OldObject && NewObject);

	// Bad idea to write data to an actor while its components are registered
	AActor* NewActor = Cast<AActor>(NewObject);
	if (NewActor != nullptr)
	{
		TInlineComponentArray<UActorComponent*> Components;
		NewActor->GetComponents(Components);

		for(int32 i=0; i<Components.Num(); i++)
		{
			ensure(!Components[i]->IsRegistered());
		}
	}

	// If the new object is an Actor, save the root component reference, to be restored later
	USceneComponent* SavedRootComponent = nullptr;
	UObjectProperty* RootComponentProperty = nullptr;
	if (NewActor != nullptr && Params.bPreserveRootComponent)
	{
		RootComponentProperty = FindField<UObjectProperty>(NewActor->GetClass(), "RootComponent");
		if (RootComponentProperty != nullptr)
		{
			SavedRootComponent = Cast<USceneComponent>(RootComponentProperty->GetObjectPropertyValue_InContainer(NewActor));
		}
	}

	// Serialize out the modified properties on the old default object
	TIndirectArray<FInstancedObjectRecord> SavedInstances;
	TMap<FString, int32> OldInstanceMap;
 	// Save the modified properties of the old CDO
	FCPFUOWriter Writer(OldObject, NewObject, Params);

	{
		// Find all instanced objects of the old CDO, and save off their modified properties to be later applied to the newly instanced objects of the new CDO
		TArray<UObject*> Components;
		OldObject->CollectDefaultSubobjects(Components, true);

		for (UObject* OldInstance : Components)
		{
			FInstancedObjectRecord* pRecord = new(SavedInstances) FInstancedObjectRecord();
			pRecord->OldInstance = OldInstance;
			OldInstanceMap.Add(OldInstance->GetPathName(OldObject), SavedInstances.Num() - 1);
			const uint32 AdditionalPortFlags = Params.bCopyDeprecatedProperties ? PPF_UseDeprecatedProperties : PPF_None;
			FObjectWriter SubObjWriter(OldInstance, pRecord->SavedProperties, true, true, Params.bDoDelta, AdditionalPortFlags);
		}
	}

	// Gather references to old instances or objects that need to be replaced after we serialize in saved data
	TMap<UObject*, UObject*> ReferenceReplacementMap;
	ReferenceReplacementMap.Add(OldObject, NewObject);
	ReferenceReplacementMap.Add(OldObject->GetArchetype(), NewObject->GetArchetype());
	if (Params.bReplaceObjectClassReferences)
	{
		ReferenceReplacementMap.Add(OldObject->GetClass(), NewObject->GetClass());
	}
	ReferenceReplacementMap.Add(OldObject->GetClass()->GetDefaultObject(), NewObject->GetClass()->GetDefaultObject());

	TArray<UObject*> ComponentsOnNewObject;
	{
		TArray<UObject*> EditInlineSubobjectsOfComponents;
		NewObject->CollectDefaultSubobjects(ComponentsOnNewObject,true);

		// populate the ReferenceReplacementMap 
		for (int32 Index = 0; Index < ComponentsOnNewObject.Num(); Index++)
		{
			UObject* NewInstance = ComponentsOnNewObject[Index];
			if (int32* pOldInstanceIndex = OldInstanceMap.Find(NewInstance->GetPathName(NewObject)))
			{
				FInstancedObjectRecord& Record = SavedInstances[*pOldInstanceIndex];
				ReferenceReplacementMap.Add(Record.OldInstance, NewInstance);
				if (Params.bAggressiveDefaultSubobjectReplacement)
				{
					UClass* Class = OldObject->GetClass()->GetSuperClass();
					if (Class)
					{
						UObject *CDOInst = Class->GetDefaultSubobjectByName(NewInstance->GetFName());
						if (CDOInst)
						{
							ReferenceReplacementMap.Add(CDOInst, NewInstance);
#if WITH_EDITOR
							if (Class->ClassGeneratedBy && Cast<UBlueprint>(Class->ClassGeneratedBy)->SkeletonGeneratedClass)
							{
								UObject *CDOInstS = Cast<UBlueprint>(Class->ClassGeneratedBy)->SkeletonGeneratedClass->GetDefaultSubobjectByName(NewInstance->GetFName());
								if (CDOInstS)
								{
									ReferenceReplacementMap.Add(CDOInstS, NewInstance);
								}

							}
#endif // WITH_EDITOR
						}
					}
				}
			}
			else
			{
				bool bContainedInsideNewInstance = false;
				for (UObject* Parent = NewInstance->GetOuter(); Parent != NULL; Parent = Parent->GetOuter())
				{
					if (Parent == NewObject)
					{
						bContainedInsideNewInstance = true;
						break;
					}
				}

				if (!bContainedInsideNewInstance)
				{
					// A bad thing has happened and cannot be reasonably fixed at this point
					UE_LOG(LogEngine, Log, TEXT("Warning: The CDO '%s' references a component that does not have the CDO in its outer chain!"), *NewObject->GetFullName(), *NewInstance->GetFullName());
				}
			}
		}

		// Serialize in the modified properties from the old CDO to the new CDO
		if (Writer.SavedPropertyData.Num() > 0)
		{
			FCPFUOReader Reader(Writer, NewObject);
		}

		for (int32 Index = 0; Index < ComponentsOnNewObject.Num(); Index++)
		{
			UObject* NewInstance = ComponentsOnNewObject[Index];
			if (int32* pOldInstanceIndex = OldInstanceMap.Find(NewInstance->GetPathName(NewObject)))
			{
				// Restore modified properties into the new instance
				FInstancedObjectRecord& Record = SavedInstances[*pOldInstanceIndex];
				FObjectReader Reader(NewInstance, Record.SavedProperties, true, true);
				FFindInstancedReferenceSubobjectHelper::Duplicate(Record.OldInstance, NewInstance, ReferenceReplacementMap, EditInlineSubobjectsOfComponents);
			}
		}
		ComponentsOnNewObject.Append(EditInlineSubobjectsOfComponents);
	}

	FFindInstancedReferenceSubobjectHelper::Duplicate(OldObject, NewObject, ReferenceReplacementMap, ComponentsOnNewObject);

	// Replace anything with an outer of the old object with NULL, unless it already has a replacement
	ForEachObjectWithOuter(OldObject, [&ReferenceReplacementMap](UObject* ObjectInOuter)
	{
		if (!ReferenceReplacementMap.Contains(ObjectInOuter))
		{
			ReferenceReplacementMap.Add(ObjectInOuter, nullptr);
		}
	});

	if(Params.bClearReferences)
	{
		UPackage* NewPackage = NewObject->GetOutermost();
		// Replace references to old classes and instances on this object with the corresponding new ones
		FArchiveReplaceOrClearExternalReferences<UObject> ReplaceInCDOAr(NewObject, ReferenceReplacementMap, NewPackage);

		// Replace references inside each individual component. This is always required because if something is in ReferenceReplacementMap, the above replace code will skip fixing child properties
		for (int32 ComponentIndex = 0; ComponentIndex < ComponentsOnNewObject.Num(); ++ComponentIndex)
		{
			UObject* NewComponent = ComponentsOnNewObject[ComponentIndex];
			FArchiveReplaceOrClearExternalReferences<UObject> ReplaceInComponentAr(NewComponent, ReferenceReplacementMap, NewPackage);
		}
	}

	// Restore the root component reference
	if (NewActor != nullptr && Params.bPreserveRootComponent)
	{
		if (RootComponentProperty != nullptr)
		{
			RootComponentProperty->SetObjectPropertyValue_InContainer(NewActor, SavedRootComponent);
		}

		NewActor->ResetOwnedComponents();
	}

	bool bDumpProperties = CVarDumpCopyPropertiesForUnrelatedObjects.GetValueOnAnyThread() != 0;
	// Uncomment the next line to debug CPFUO for a specific object:
	// bDumpProperties |= (NewObject->GetName().Find(TEXT("SpinTree")) != INDEX_NONE);
	if (bDumpProperties)
	{
		DumpObject(*FString::Printf(TEXT("CopyPropertiesForUnrelatedObjects: Old (%s)"), *OldObject->GetFullName()), OldObject);
		DumpObject(*FString::Printf(TEXT("CopyPropertiesForUnrelatedObjects: New (%s)"), *NewObject->GetFullName()), NewObject);
	}

	// Now notify any tools that aren't already updated via the FArchiveReplaceObjectRef path
	if (Params.bNotifyObjectReplacement && GEngine != nullptr)
	{
		GEngine->NotifyToolsOfObjectReplacement(ReferenceReplacementMap);
	}
}

// This is a really bad hack for UBlueprintFunctionLibrary::GetFunctionCallspace. See additional comments there.
bool UEngine::ShouldAbsorbAuthorityOnlyEvent()
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		FWorldContext &Context = *It;
		bool useIt = false;
		if (GPlayInEditorID != -1)
		{
			if (Context.WorldType == EWorldType::PIE && Context.PIEInstance == GPlayInEditorID)
			{
				useIt = true;
			}
		}
		else
		{
			if (Context.WorldType == EWorldType::Game)
			{
				useIt = true;
			}
		}

		if (useIt && (Context.World() != nullptr))
		{
			return (Context.World()->GetNetMode() ==  NM_Client);
		}
	}
	return false;
}


bool UEngine::ShouldAbsorbCosmeticOnlyEvent()
{
	for (auto It = WorldList.CreateIterator(); It; ++It)
	{
		FWorldContext &Context = *It;
		bool useIt = false;
		if (GPlayInEditorID != -1)
		{
			if (Context.WorldType == EWorldType::PIE && Context.PIEInstance == GPlayInEditorID)
			{
				useIt = true;
			}
		}
		else
		{
			if (Context.WorldType == EWorldType::Game)
			{
				useIt = true;
			}
		}

		if (useIt && (Context.World() != nullptr))
		{
			return (Context.World()->GetNetMode() == NM_DedicatedServer);
		}
	}
	return false;
}

static void SetNearClipPlane(const TArray<FString>& Args)
{
	const float MinClipPlane = 1.0f;
	float NewClipPlane = 20.0f;
	if (Args.Num())
	{
		NewClipPlane = FCString::Atof(*Args[0]);
	}
	FlushRenderingCommands();
	GNearClippingPlane = FMath::Max(NewClipPlane,MinClipPlane);
}
FAutoConsoleCommand GSetNearClipPlaneCmd(TEXT("r.SetNearClipPlane"),TEXT("Set the near clipping plane (in cm)"),FConsoleCommandWithArgsDelegate::CreateStatic(SetNearClipPlane));

static TAutoConsoleVariable<int32> CVarAllowHighQualityLightMaps(
	TEXT("r.HighQualityLightMaps"),
	1,
	TEXT("If set to 1, allow high quality lightmaps which don't bake in direct lighting of stationary lights"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);


bool AllowHighQualityLightmaps(ERHIFeatureLevel::Type FeatureLevel)
{
	return FPlatformProperties::SupportsHighQualityLightmaps()
		&& (FeatureLevel > ERHIFeatureLevel::ES3_1)
		&& (CVarAllowHighQualityLightMaps.GetValueOnAnyThread() != 0)
		&& !IsMobilePlatform(GShaderPlatformForFeatureLevel[FeatureLevel]);
}

// Helper function for changing system resolution via the r.setres console command
void FSystemResolution::RequestResolutionChange(int32 InResX, int32 InResY, EWindowMode::Type InWindowMode)
{
	if (PLATFORM_LINUX)
	{
		// Fullscreen and WindowedFullscreen behave the same on Linux, see FLinuxWindow::ReshapeWindow()/SetWindowMode().
		// Allowing Fullscreen window mode confuses higher level code (see UE-19996).
		if (InWindowMode == EWindowMode::Fullscreen)
		{
			InWindowMode = EWindowMode::WindowedFullscreen;
		}
	}

	FString WindowModeSuffix;
	switch (InWindowMode)
	{
		case EWindowMode::Windowed:
		{
			WindowModeSuffix = TEXT("w");
		} break;
		case EWindowMode::WindowedFullscreen:
		{
			WindowModeSuffix = TEXT("wf");
		} break;
		case EWindowMode::Fullscreen:
		{
			WindowModeSuffix = TEXT("f");
		} break;
	}

	FString NewValue = FString::Printf(TEXT("%dx%d%s"), InResX, InResY, *WindowModeSuffix);
	CVarSystemResolution->Set(*NewValue, ECVF_SetByConsole);
}

//////////////////////////////////////////////////////////////////////////
// STATS

/** Utility that gets a color for a particular level status */
FColor GetColorForLevelStatus(int32 Status)
{
	FColor Color = FColor::White;
	switch (Status)
	{
	case LEVEL_Visible:
		Color = FColor::Green;		// green  loaded and visible
		break;
	case LEVEL_MakingVisible:
		Color = FColorList::Orange;	// orange, in process of being made visible
		break;
	case LEVEL_Loading:
		Color = FColor::Magenta;	// purple, in process of being loaded
		break;
	case LEVEL_Loaded:
		Color = FColor::Yellow;		// yellow loaded but not visible
		break;
	case LEVEL_UnloadedButStillAround:
		Color = FColor::Blue;		// blue  (GC needs to occur to remove this)
		break;
	case LEVEL_Unloaded:
		Color = FColor::Red;		// Red   unloaded
		break;
	case LEVEL_Preloading:
		Color = FColor::Magenta;	// purple (preloading)
		break;
	default:
		break;
	};

	return Color;
}

void UEngine::ExecEngineStat(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* InName)
{
	// Store a ptr to the viewport that needs to process this stat command
	GStatProcessingViewportClient = ViewportClient;

	FString StatCommand = TEXT("STAT ");
	StatCommand += InName;
	Exec(World, *StatCommand, *GLog);
}

bool UEngine::IsEngineStat(const FString& InName)
{
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		if (EngineStat.CommandNameString == InName)
		{
			return true;
		}
	}
	return false;
}

void UEngine::SetEngineStat(UWorld* World, FCommonViewportClient* ViewportClient, const FString& InName, const bool bShow)
{
	if (ViewportClient && IsEngineStat(InName) && ViewportClient->IsStatEnabled(*InName) != bShow)
	{
		ExecEngineStat(World, ViewportClient, *InName);
	}
}

void UEngine::SetEngineStats(UWorld* World, FCommonViewportClient* ViewportClient, const TArray<FString>& InNames, const bool bShow)
{
	for (int32 StatIdx = 0; StatIdx < InNames.Num(); StatIdx++)
	{
		// If we need to disable, do it in the reverse order incase one stat affects another
		const int32 StatIndex = bShow ? StatIdx : (InNames.Num() - 1) - StatIdx;
		SetEngineStat(World, ViewportClient, *InNames[StatIndex], bShow);
	}
}

void UEngine::RenderEngineStats(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 LHSX, int32& InOutLHSY, int32 RHSX, int32& InOutRHSY, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	for (int32 StatIdx = 0; StatIdx < EngineStats.Num(); StatIdx++)
	{
		const FEngineStatFuncs& EngineStat = EngineStats[StatIdx];
		if (EngineStat.RenderFunc && (!Viewport->GetClient() || Viewport->GetClient()->IsStatEnabled(EngineStat.CommandNameString)))
		{
			// Render the stat either on the left or right hand side of the screen, keeping track of the new Y position
			const int32 StatX = EngineStat.bIsRHS ? RHSX : LHSX;
			int32* StatY = EngineStat.bIsRHS ? &InOutRHSY : &InOutLHSY;
			*StatY = (this->*(EngineStat.RenderFunc))(World, Viewport, Canvas, StatX, *StatY, ViewLocation, ViewRotation);
		}
	}
}

// VERSION
#if !UE_BUILD_SHIPPING
int32 UEngine::RenderStatVersion(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	if (!GIsHighResScreenshot && !GIsDumpingMovie && GAreScreenMessagesEnabled)
	{
		if (!bSuppressMapWarnings)
		{
			FCanvasTextItem TextItem(FVector2D(X - 40, Y), FText::FromString(Viewport->AppVersionString), GetSmallFont(), FLinearColor::Yellow);
			TextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TextItem);
			Y += TextItem.DrawnSize.Y;
		}
	}
	return Y;
}
#endif // !UE_BUILD_SHIPPING

// DETAILED
bool UEngine::ToggleStatDetailed(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if( ViewportClient == nullptr )
	{
		// Ignore if all Viewports are closed.
		return false;
	}

	// Each of these stats should call "Detailed -Skip" when they themselves are disabled
	static bool bSetup = false;
	static TArray<FString> DetailedStats;
	if (!bSetup)
	{
		bSetup = true;
		DetailedStats.Add(TEXT("FPS"));
		DetailedStats.Add(TEXT("Unit"));
		DetailedStats.Add(TEXT("UnitMax"));
		DetailedStats.Add(TEXT("UnitGraph"));
		DetailedStats.Add(TEXT("Raw"));
	}

	// If any of the detailed stats are inactive, take this as enabling all, unless 'Skip' is specifically specified
	const bool bSkip = Stream ? FParse::Param(Stream, TEXT("Skip")) : false;
	if (!bSkip)
	{
		// Enable or disable all the other stats depending on the current state
		const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
		SetEngineStats(World, ViewportClient, DetailedStats, bShowDetailed);

		// Extra stat, needs to do the opposite of the others (order of exec unimportant)
		SetEngineStat(World, ViewportClient, TEXT("UnitTime"), !bShowDetailed);
	}

	return true;
}

// FPS
bool UEngine::ToggleStatFPS(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	const bool bShowFPS = ViewportClient->IsStatEnabled(TEXT("FPS"));
	const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
	if (!bShowFPS && bShowDetailed)
	{
		// Since we're turning this off, we also need to toggle off detailed too
		ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
	}

	return true;
}

int32 UEngine::RenderStatFPS(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Pick a larger font on console.
	UFont* Font = FPlatformProperties::SupportsWindowedMode() ? GetSmallFont() : GetMediumFont();

	// Choose the counter color based on the average frame time.
	const FColor FPSColor = GetFrameTimeDisplayColor(GAverageMS);

	// Start drawing the various counters.
	const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);

	// Draw the FPS counter.
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%5.2f FPS"), GAverageFPS),
		Font,
		FPSColor
		);
	Y += RowHeight;

	// Draw the frame time.
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%5.2f ms"), GAverageMS),
		Font,
		FPSColor
		);
	Y += RowHeight;
	return Y;
}

// HITCHES
bool UEngine::ToggleStatHitches(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	FPlatformProcess::Sleep(0.11f); // cause a hitch so it is evidently working
	return false;
}

int32 UEngine::RenderStatHitches(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Forward this draw request to the viewport client
	if (Viewport->GetClient())
	{
		checkf(Viewport->GetClient()->GetStatHitchesData(), TEXT("StatHitchesData must be allocated for this viewport if you wish to display stat."));
		Y = Viewport->GetClient()->GetStatHitchesData()->DrawStat(Viewport, Canvas, X, Y);
	}
	return Y;
}

// SUMMARY
int32 UEngine::RenderStatSummary(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Pick a larger font on console.
	UFont* Font = FPlatformProperties::SupportsWindowedMode() ? GetSmallFont() : GetMediumFont();

	// Retrieve allocation info.
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	float MemoryInMByte = MemoryStats.UsedPhysical / 1024.f / 1024.f;

	// Draw the memory summary stats.
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%5.2f MByte"), MemoryInMByte),
		Font,
		FColor(30, 144, 255)
		);

	const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);
	Y += RowHeight;
	return Y;
}

// NAMEDEVENTS
bool UEngine::ToggleStatNamedEvents(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if( ViewportClient == nullptr )
	{
		// Ignore if all Viewports are closed.
		return false;
	}

	// Enable emission of named events and force enable cycle stats.
	if (ViewportClient->IsStatEnabled(TEXT("NamedEvents")))
	{
		if (GCycleStatsShouldEmitNamedEvents == 0)
		{
			StatsMasterEnableAdd();
		}
		GCycleStatsShouldEmitNamedEvents++;
	}
	// Disable emission of named events and force-enabling cycle stats.
	else
	{
		if (GCycleStatsShouldEmitNamedEvents == 1)
		{
			StatsMasterEnableSubtract();
		}
		GCycleStatsShouldEmitNamedEvents = FMath::Max(0, GCycleStatsShouldEmitNamedEvents - 1);
	}
	return false;
}

int32 UEngine::RenderStatNamedEvents(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	FCanvasTextItem TextItem(FVector2D(X - 40, Y), LOCTEXT("NAMEDEVENTSENABLED", "NAMED EVENTS ENABLED"), GetSmallFont(), FLinearColor::Blue);
	TextItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(TextItem);
	Y += TextItem.DrawnSize.Y;
	return Y;
}

// COLORLIST
int32 UEngine::RenderStatColorList(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	UFont* Font = GetTinyFont();

	const int32 LineHeight = FMath::TruncToInt(Font->GetMaxCharHeight());
	const int32 ColorsNum = GColorList.GetColorsNum();
	const int32 MaxLinesInColumn = 35;
	const int32 ColumnsNum = FMath::CeilToInt((float)ColorsNum / (float)MaxLinesInColumn);

	Y += 16;
	const int32 SavedY = Y;
	int32 LowestY = Y + MaxLinesInColumn * LineHeight;

	// Draw columns with color list.
	for (int32 ColumnIndex = 0; ColumnIndex < ColumnsNum; ColumnIndex++)
	{
		int32 LineWidthMax = 0;

		for (int32 ColColorIndex = 0; ColColorIndex < MaxLinesInColumn; ColColorIndex++)
		{
			const int32 ColorIndex = ColumnIndex * MaxLinesInColumn + ColColorIndex;
			if (ColorIndex >= ColorsNum)
			{
				break;
			}

			const FColor& Color = GColorList.GetFColorByIndex(ColorIndex);
			const FString Line = *FString::Printf(TEXT("%3i %s %s"), ColorIndex, *GColorList.GetColorNameByIndex(ColorIndex), *Color.ToString());

			LineWidthMax = FMath::Max(LineWidthMax, Font->GetStringSize(*Line));

			Canvas->DrawShadowedString(X, Y, *Line, Font, FLinearColor(Color));
			Y += LineHeight;
		}

		X += LineWidthMax;
		LineWidthMax = 0;
		Y = SavedY;
	}
	return LowestY;
}

// LEVELS
int32 UEngine::RenderStatLevels(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	int32 MaxY = Y;
	const TArray<FSubLevelStatus> SubLevelsStatusList = GetSubLevelsStatus(World);

	// now do drawing to the screen

	// Render unloaded levels in red, loaded ones in yellow and visible ones in green. Blue signifies that a level is unloaded but
	// hasn't been garbage collected yet.
	Canvas->DrawShadowedString(X, Y, TEXT("Levels"), GetSmallFont(), FLinearColor::White);
	Y += 12;

	if (SubLevelsStatusList.Num())
	{
		// First entry - always persistent level
		FString MapName	= SubLevelsStatusList[0].PackageName.ToString();
		UPackage* LevelPackage = FindObjectFast<UPackage>(NULL, SubLevelsStatusList[0].PackageName);
		if (SubLevelsStatusList[0].bPlayerInside)
		{
			MapName = *FString::Printf(TEXT("->  %s - %4.1f sec"), *MapName, LevelPackage->GetLoadTime());
		}
		else
		{
			MapName = *FString::Printf(TEXT("    %s - %4.1f sec"), *MapName, LevelPackage->GetLoadTime());
		}

		Canvas->DrawShadowedString(X, Y, *MapName, GetSmallFont(), FColor(127, 127, 127));
		Y += 12;
	}

	int32 BaseY = Y;

	// now draw the levels
	for (int32 LevelIdx = 1; LevelIdx < SubLevelsStatusList.Num(); ++LevelIdx)
	{
		const FSubLevelStatus& LevelStatus = SubLevelsStatusList[LevelIdx];
		
		// Wrap around at the bottom.
		if (Y > Viewport->GetSizeXY().Y - 30)
		{
			MaxY = FMath::Max(MaxY, Y);
			Y = BaseY;
			X += 350;
		}

		FColor	Color = GetColorForLevelStatus(LevelStatus.StreamingStatus);
		FString DisplayName = LevelStatus.PackageName.ToString();

		if (LevelStatus.LODIndex != INDEX_NONE)
		{
			DisplayName += FString::Printf(TEXT(" [LOD%d]"), LevelStatus.LODIndex+1);
		}

		UPackage* LevelPackage = FindObjectFast<UPackage>(NULL, LevelStatus.PackageName);
				
		if (LevelPackage
			&& (LevelPackage->GetLoadTime() > 0)
			&& (LevelStatus.StreamingStatus != LEVEL_Unloaded))
		{
			DisplayName += FString::Printf(TEXT(" - %4.1f sec"), LevelPackage->GetLoadTime());
		}
		else
		{
			const float AsyncLoadPercentage = GetAsyncLoadPercentage(*LevelStatus.PackageName.ToString());
			if (AsyncLoadPercentage >= 0.0f)
			{
				const int32 Percentage = FMath::TruncToInt(AsyncLoadPercentage);
				DisplayName += FString::Printf(TEXT(" - %3i %%"), Percentage);
			}
		}

		if (LevelStatus.bPlayerInside)
		{
			DisplayName = *FString::Printf(TEXT("->  %s"), *DisplayName);
		}
		else
		{
			DisplayName = *FString::Printf(TEXT("    %s"), *DisplayName);
		}
		
		Canvas->DrawShadowedString(X + 4, Y, *DisplayName, GetSmallFont(), Color);
		Y += 12;
	}
	return FMath::Max(MaxY, Y);
}

// LEVELMAP
int32 UEngine::RenderStatLevelMap(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	const FVector2D MapOrigin = FVector2D(512, 128);
	const FVector2D MapSize = FVector2D(512, 512);

	// Get status of each sublevel (by name)
	const TArray<FSubLevelStatus> SubLevelsStatusList = GetSubLevelsStatus(World);

	// First iterate to find bounds of all streaming volumes
	FBox AllVolBounds(ForceInit);
	for (const FSubLevelStatus& LevelStatus : SubLevelsStatusList)
	{
		ULevelStreaming* LevelStreaming = World->GetLevelStreamingForPackageName(LevelStatus.PackageName);
		if (LevelStreaming && LevelStreaming->bDrawOnLevelStatusMap)
		{
			AllVolBounds += LevelStreaming->GetStreamingVolumeBounds();
		}
	}

	// We need to ensure the XY aspect ratio of AllVolBounds is the same as the map

	// Work out scale factor between map and world
	FVector VolBoundsSize = (AllVolBounds.Max - AllVolBounds.Min);
	float ScaleX = MapSize.X / VolBoundsSize.X;
	float ScaleY = MapSize.Y / VolBoundsSize.Y;
	float UseScale = FMath::Min(ScaleX, ScaleY); // Pick the smallest scaling factor

	// Resize AllVolBounds
	FVector NewVolBoundsSize = VolBoundsSize;
	NewVolBoundsSize.X = MapSize.X / UseScale;
	NewVolBoundsSize.Y = MapSize.Y / UseScale;
	FVector DeltaBounds = (NewVolBoundsSize - VolBoundsSize);
	AllVolBounds.Min -= 0.5f * DeltaBounds;
	AllVolBounds.Max += 0.5f * DeltaBounds;

	// Find world-space location for top-left and bottom-right corners of map
	FVector2D TopLeftPos(AllVolBounds.Max.X, AllVolBounds.Min.Y); // max X, min Y
	FVector2D BottomRightPos(AllVolBounds.Min.X, AllVolBounds.Max.Y); // min X, max Y


	// Now we iterate and actually draw volumes
	for (const FSubLevelStatus& LevelStatus : SubLevelsStatusList)
	{
		// Find the color to draw this level in
		FColor StatusColor = GetColorForLevelStatus(LevelStatus.StreamingStatus);
		StatusColor.A = 64; // make it translucent

		ULevelStreaming* LevelStreaming = World->GetLevelStreamingForPackageName(LevelStatus.PackageName);
		if (LevelStreaming && LevelStreaming->bDrawOnLevelStatusMap)
		{
			for (int32 VolIdx = 0; VolIdx < LevelStreaming->EditorStreamingVolumes.Num(); VolIdx++)
			{
				ALevelStreamingVolume* StreamingVol = LevelStreaming->EditorStreamingVolumes[VolIdx];
				if (StreamingVol)
				{
					DrawVolumeOnCanvas(StreamingVol, Canvas, TopLeftPos, BottomRightPos, MapOrigin, MapSize, StatusColor);
				}
			}
		}
	}



	// Now we want to draw the player(s) location on the map
	{
		// Find map location for arrow
		check(ViewLocation);
		const FVector2D PlayerMapPos = TransformLocationToMap(TopLeftPos, BottomRightPos, MapOrigin, MapSize, *ViewLocation);

		// Make verts for little rotated arrow
		check(ViewRotation);
		float PlayerYaw = (ViewRotation->Yaw * PI / 180.f) - (0.5f * PI); // We have to add 90 degrees because +X in world space means -Y in map space
		const FVector2D M0 = PlayerMapPos + RotateVec2D(FVector2D(7, 0), PlayerYaw);
		const FVector2D M1 = PlayerMapPos + RotateVec2D(FVector2D(-7, 5), PlayerYaw);
		const FVector2D M2 = PlayerMapPos + RotateVec2D(FVector2D(-7, -5), PlayerYaw);

		FCanvasTriangleItem TriItem(M0, M1, M2, GWhiteTexture);
		Canvas->DrawItem(TriItem);
	}
	return Y;
}

// UNIT
bool UEngine::ToggleStatUnit(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if( ViewportClient == nullptr )
	{
		// Ignore if all Viewports are closed.
		return false;
	}

	const bool bShowUnitMaxTimes = ViewportClient->IsStatEnabled(TEXT("UnitMax"));
	if (bShowUnitMaxTimes != false)
	{
		// Toggle UnitMax back to Inactive
		ExecEngineStat(World, ViewportClient, TEXT("UnitMax"));

		// Force Unit back to Active if turning UnitMax off
		SetEngineStat(World, ViewportClient, TEXT("Unit"), true);
	}

	const bool bShowUnitTimes = ViewportClient->IsStatEnabled(TEXT("Unit"));
	const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
	if (!bShowUnitTimes && bShowDetailed)
	{
		// Since we're turning this off, we also need to toggle off detailed too
		ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
	}

	return true;
}

int32 UEngine::RenderStatUnit(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Forward this draw request to the viewport client
	if (Viewport->GetClient())
	{
		checkf(Viewport->GetClient()->GetStatUnitData(), TEXT("StatUnitData must be allocated for this viewport if you wish to display stat."));
		Y = Viewport->GetClient()->GetStatUnitData()->DrawStat(Viewport, Canvas, X, Y);
	}
	return Y;
}

// UNITMAX
#if !UE_BUILD_SHIPPING
bool UEngine::ToggleStatUnitMax(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if( ViewportClient == nullptr )
	{
		// Ignore if all Viewports are closed.
		return false;
	}
	const bool bShowUnitMaxTimes = ViewportClient->IsStatEnabled(TEXT("UnitMax"));
	if (bShowUnitMaxTimes)
	{
		// Force Unit to Active
		SetEngineStat(World, ViewportClient, TEXT("Unit"), true);

		// Force UnitMax to true as Unit will have Toggled it back to false
		SetEngineStat(World, ViewportClient, TEXT("UnitMax"), true);
	}
	else
	{
		const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
		if (bShowDetailed)
		{
			// Since we're turning this off, we also need to toggle off detailed too
			ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
		}
	}
	return true;
}

// UNITGRAPH
bool UEngine::ToggleStatUnitGraph(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if( ViewportClient == nullptr )
	{
		// Ignore if all Viewports are closed.
		return false;
	}
	const bool bShowUnitGraph = ViewportClient->IsStatEnabled(TEXT("UnitGraph"));
	if (bShowUnitGraph)
	{
		// Force Unit to Active
		SetEngineStat(World, ViewportClient, TEXT("Unit"), true);

		// Force UnitTime to Active
		SetEngineStat(World, ViewportClient, TEXT("UnitTime"), true);	
	}
	else
	{
		const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
		if (bShowDetailed)
		{
			// Since we're turning this off, we also need to toggle off detailed too
			ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
		}
	}
	return true;
}

// UNITTIME
bool UEngine::ToggleStatUnitTime(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if( ViewportClient == nullptr )
	{
		// Ignore if all Viewports are closed.
		return false;
	}
	const bool bShowUnitTime = ViewportClient->IsStatEnabled(TEXT("UnitTime"));
	if (bShowUnitTime)
	{
		// Force UnitGraph to Active
		SetEngineStat(World, ViewportClient, TEXT("UnitGraph"), true);
	}
	return true;
}

// RAW
bool UEngine::ToggleStatRaw(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	const bool bShowRaw = ViewportClient->IsStatEnabled(TEXT("Raw"));
	const bool bShowDetailed = ViewportClient->IsStatEnabled(TEXT("Detailed"));
	if (bShowRaw)
	{
		// Force UnitGraph to Active
		SetEngineStat(World, ViewportClient, TEXT("UnitGraph"), true);
	}
	else if (bShowDetailed)
	{
		// Since we're turning this off, we also need to toggle off detailed too
		ExecEngineStat(World, ViewportClient, TEXT("Detailed -Skip"));
	}
	return true;
}
#endif


static uint64 TaskThread = 0xFFFFFFFFFFFFFFFF;
static uint64 GameThread = 0xFFFFFFFFFFFFFFFF;
static uint64 RenderThread = 0xFFFFFFFFFFFFFFFF;
static uint64 RHIThread = 0xFFFFFFFFFFFFFFFF;
FThreadSafeCounter StallForTaskThread;

void SetAffinityOnThread()
{
	if (IsInActualRenderingThread())
	{
		FPlatformProcess::SetThreadAffinityMask(RenderThread);
		UE_LOG(LogConsoleResponse, Display, TEXT("RT     %016llX"), RenderThread);
	}
	else if (IsInRHIThread())
	{
		FPlatformProcess::SetThreadAffinityMask(RHIThread);
		UE_LOG(LogConsoleResponse, Display, TEXT("RHI    %016llX"), RHIThread);
	}
	else if (IsInGameThread())
	{
		FPlatformProcess::SetThreadAffinityMask(GameThread);
		UE_LOG(LogConsoleResponse, Display, TEXT("GT     %016llX"), GameThread);
	}
	else // assume task thread
	{
		int32 TaskThreadIndex = int32(FTaskGraphInterface::Get().GetCurrentThreadIfKnown()) - int32(ENamedThreads::ActualRenderingThread) - 1;
		FPlatformProcess::SetThreadAffinityMask(TaskThread);
		UE_LOG(LogConsoleResponse, Display, TEXT("Task%2d %016llX"), TaskThreadIndex, TaskThread);
		StallForTaskThread.Decrement();
		// we wait for the others to finish here so that we do all task threads
		while (StallForTaskThread.GetValue())
		{
			FPlatformProcess::Sleep(.0001f);
		}
	}
}


static void SetupThreadAffinity(const TArray<FString>& Args)
{
	static bool LoadedDefaults = false;
	if (!LoadedDefaults || (Args.Num() && Args[0] == TEXT("default")))
	{
		LoadedDefaults = true;
		TaskThread = FPlatformAffinity::GetTaskGraphThreadMask();
		GameThread = FPlatformAffinity::GetMainGameMask();
		RenderThread = FPlatformAffinity::GetRenderingThreadMask();
		RHIThread = FPlatformAffinity::GetRHIThreadMask();
	}
	for (int32 Index = 0; Index + 1 < Args.Num(); Index += 2)
	{
		uint64 Aff = FParse::HexNumber(*Args[Index + 1]); // this is only 32 bits
		if (!Aff)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Parsed 0 for affinity, using 0xFFFFFFFFFFFFFFFF instead"));
			Aff = 0xFFFFFFFFFFFFFFFF;
		}
		if (Args[Index] == TEXT("GT"))
		{
			GameThread = Aff;
		}
		else if (Args[Index] == TEXT("RT"))
		{
			RenderThread = Aff;
		}
		else if (Args[Index] == TEXT("RHI"))
		{
			RHIThread = Aff;
		}
		else if (Args[Index] == TEXT("Task"))
		{
			TaskThread = Aff;
		}
		else
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Unrecognized thread name %s"), *Args[Index]);
		}
	}

	StallForTaskThread.Reset();
	StallForTaskThread.Add(FTaskGraphInterface::Get().GetNumWorkerThreads());

	for (int32 Index = 0; Index < FTaskGraphInterface::Get().GetNumWorkerThreads(); Index++)
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SetAffinityOnThread),
			TStatId(), NULL, ENamedThreads::AnyNormalThreadHiPriTask);
	}
	if (ENamedThreads::bHasHighPriorityThreads)
	{
		for (int32 Index = 0; Index < FTaskGraphInterface::Get().GetNumWorkerThreads(); Index++)
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SetAffinityOnThread),
				TStatId(), NULL, ENamedThreads::AnyHiPriThreadHiPriTask);
		}
	}
	if (ENamedThreads::bHasBackgroundThreads)
	{
		for (int32 Index = 0; Index < FTaskGraphInterface::Get().GetNumWorkerThreads(); Index++)
		{
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SetAffinityOnThread),
				TStatId(), NULL, ENamedThreads::AnyBackgroundHiPriTask);
		}
	}
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SetAffinityOnThread),
		TStatId(), NULL, ENamedThreads::RenderThread);
	if (GRHIThread_InternalUseOnly)
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateStatic(&SetAffinityOnThread),
			TStatId(), NULL, ENamedThreads::RHIThread);
	}
	check(IsInGameThread());
	SetAffinityOnThread();
	FlushRenderingCommands();
	GLog->FlushThreadedLogs();
}

static FAutoConsoleCommand SetupThreadAffinityCmd(
	TEXT("SetThreadAffinity"),
	TEXT("Sets the thread affinity. A single arg of default resets the thread affinity, otherwise pairs of args [GT|RT|RHI|Task] [Hex affinity] sets the affinity."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SetupThreadAffinity)
	);

#if !UE_BUILD_SHIPPING

static void PakFileTest(const TArray<FString>& Args)
{
	FString PakFilename = TEXT("D:\\work\\Dev-Core\\Samples\\Games\\ShooterGame\\Saved\\StagedBuilds\\WindowsNoEditor\\ShooterGame\\Content\\Paks\\test.pak");
	if (Args.Num() < 1)
	{
		UE_LOG(LogConsoleResponse, Error, TEXT("Usage: PakFileTest path-to-pak-file"));
	}
	else
	{
		PakFilename = Args[0];
	}
	if (!PakFilename.IsEmpty())
	{
		const FString MountPoint = FPaths::ProjectSavedDir() / TEXT("PakFileTest");
		FFileManagerGeneric::Get().DeleteDirectory(*MountPoint, false, true);

		FString MountCmd = FString::Printf(TEXT("mount %s %s"), *PakFilename, *MountPoint);
		GEngine->Exec(nullptr, *MountCmd);

		static TArray<FString> FileNames;
		check(!FileNames.Num()); // don't run this twice!
		IFileManager::Get().FindFilesRecursive(FileNames, *MountPoint, TEXT("*.*"), true, false);
		check(FileNames.Num());


		static FString ReloadTestFile;
		static int64 ReloadTestSize = -1;
		static uint32 ReloadTestCRC = 0;
		static FCriticalSection ReloadLock;
		static FThreadSafeCounter Processed;


		static TFunction<void()> Broadcast =
			[
			]
		()
		{
			FRandomStream RNG(FPlatformTime::Cycles());
			{
				int32 NumProc = Processed.Increment();
				if (NumProc % 1000 == 1 || NumProc == 11 || NumProc == 101 || NumProc == 501)
				{
					UE_LOG(LogTemp, Display, TEXT("Processed %d files (Thread  %x)"), NumProc - 1, FPlatformTLS::GetCurrentThreadId());
				}

				bool bMyReload = false;
				FString TestFile;
				int64 TestSize = 0;
				uint32 TestCRC = 0;
				if (RNG.GetFraction() > .75f)
				{
					FScopeLock Lock(&ReloadLock);
					if (ReloadTestSize != -1)
					{
						TestSize = ReloadTestSize;
						TestCRC = ReloadTestCRC;
						TestFile = ReloadTestFile;

						ReloadTestFile.Empty();
						ReloadTestSize = -1;
						ReloadTestCRC = 0;

						bMyReload = true;
					}
				}
				if (TestFile.IsEmpty())
				{
					TestFile = FileNames[RNG.RandRange(0, FileNames.Num() - 1)];
				}

				IAsyncReadFileHandle* IORequestHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*TestFile);
				check(IORequestHandle);
				IAsyncReadRequest* SizeReq = IORequestHandle->SizeRequest();
				if (!SizeReq->PollCompletion()) // this should already be done with pak files
				{
					//check(0);
					UE_LOG(LogTemp, Display, TEXT("Had to wait for size!!! =  %s"), *TestFile);
					SizeReq->WaitCompletion();
				}
				if (bMyReload)
				{
					check(TestSize == SizeReq->GetSizeResults());
				}
				TestSize = SizeReq->GetSizeResults();
				SizeReq->WaitCompletion();
				delete SizeReq;

				check(TestSize >= 0);

				uint32 NewCRC = 0;
				bool bAbortAfterCancel = RNG.GetFraction() > .95f;
				if (TestSize > 0)
				{
					uint8* Memory = (uint8*)FMemory::Malloc(TestSize);

					TArray<int64> SpanOffsets;
					TArray<int64> SpanSizes;

					int64 CurrentOffset = 0;
					while (CurrentOffset < TestSize)
					{
						SpanOffsets.Add(CurrentOffset);
						int64 Span = int64(RNG.RandRange(int32(FMath::Min<int64>(8192, TestSize - CurrentOffset)), TestSize - CurrentOffset));
						SpanSizes.Add(Span);
						CurrentOffset += Span;
						check(CurrentOffset <= TestSize);
					}
					TArray<IAsyncReadRequest*> PrecacheReqs;
					if (RNG.GetFraction() > .75f)
					{
						for (int32 i = 0; i < SpanOffsets.Num() / 5; i++)
						{
							int32 Index = RNG.RandRange(0, SpanOffsets.Num() - 1);
							CurrentOffset = SpanOffsets[Index];
							int64 Span = SpanSizes[Index];

							PrecacheReqs.Add(IORequestHandle->ReadRequest(CurrentOffset, Span, AIOP_Precache));
						}
					}
					while (SpanOffsets.Num())
					{
						int32 Index = RNG.RandRange(0, SpanOffsets.Num() - 1);
						CurrentOffset = SpanOffsets[Index];
						int64 Span = SpanSizes[Index];

						bool CallbackCalled = false;

						FAsyncFileCallBack AsyncFileCallBack =
							[&CallbackCalled](bool bWasCancelled, IAsyncReadRequest* Req)
						{
							CallbackCalled = true;
						};

						bool bUserMem = !!RNG.RandRange(0, 1);
						EAsyncIOPriority Pri = EAsyncIOPriority(RNG.RandRange(AIOP_Low, AIOP_CriticalPath));
						IAsyncReadRequest* ReadReq = IORequestHandle->ReadRequest(CurrentOffset, Span, Pri, &AsyncFileCallBack, bUserMem ? Memory + CurrentOffset : nullptr);

						bool bCancel = RNG.RandRange(0, 5) == 0;

						if (bCancel)
						{
							float s = float(RNG.RandRange(0, 5)) / 1000.0f;
							if (s >= .001f)
							{
								FPlatformProcess::Sleep(s);
							}
							ReadReq->Cancel();
						}

						switch (RNG.RandRange(0, 4))
						{
						default:
							ReadReq->WaitCompletion();
							break;
						case 1:
							while (!ReadReq->PollCompletion())
							{
								FPlatformProcess::SleepNoStats(0.016f);
							}
							break;
						case 2:
							while (!ReadReq->PollCompletion())
							{
								FPlatformProcess::SleepNoStats(0);
							}
							break;
						case 3:
							while (!ReadReq->WaitCompletion(.016f))
							{
							}
							break;
						case 4:
							// can't wait for the callback after we have canceled
							if (bCancel)
							{
								ReadReq->WaitCompletion();
							}
							else
							{
								while (!CallbackCalled)
								{
									FPlatformProcess::SleepNoStats(0);
								}
							}
							break;
						}

						if (!bUserMem)
						{
							uint8 *Mem = ReadReq->GetReadResults();
							check(Mem || bCancel);
							if (Mem)
							{
								FMemory::Memcpy(Memory + CurrentOffset, Mem, Span);
								bCancel = false; // we should have the memory anyway
								FMemory::Free(Mem);
								DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Span);
							}
						}
						ReadReq->WaitCompletion();
						delete ReadReq;
						if (!bCancel)
						{
							SpanOffsets.RemoveAtSwap(Index);
							SpanSizes.RemoveAtSwap(Index);
						}
						else if (bAbortAfterCancel)
						{
							break;
						}
					}

					if (!bAbortAfterCancel)
					{
						NewCRC = FCrc::MemCrc32(Memory, TestSize, 0x56);
					}

					FMemory::Free(Memory);

					for (IAsyncReadRequest* Req : PrecacheReqs)
					{
						Req->Cancel();
						Req->WaitCompletion();
						delete Req;
					}

				}

				if (!bAbortAfterCancel)
				{
					if (bMyReload)
					{
						//UE_LOG(LogTemp, Display, TEXT("Reload pass CRC = %x    %s"), NewCRC, *TestFile);
						check(NewCRC == TestCRC);
					}
					else
					{
						//UE_LOG(LogTemp, Display, TEXT("CRC = %x    %s"), NewCRC, *TestFile);
					}
					if (RNG.GetFraction() > .75f)
					{
						FScopeLock Lock(&ReloadLock);
						if (ReloadTestSize == -1)
						{
							ReloadTestSize = TestSize;
							ReloadTestCRC = NewCRC;
							ReloadTestFile = TestFile;
						}
					}
				}

				delete IORequestHandle;
			}
			if (!GIsRequestingExit)
			{
				switch (RNG.RandRange(0, 2))
				{
				case 1:
					if (ENamedThreads::bHasBackgroundThreads)
					{
						FFunctionGraphTask::CreateAndDispatchWhenReady(Broadcast, TStatId(), NULL, ENamedThreads::AnyBackgroundThreadNormalTask);
						return;
					}
					break;
				case 2:
					if (ENamedThreads::bHasHighPriorityThreads)
					{
						FFunctionGraphTask::CreateAndDispatchWhenReady(Broadcast, TStatId(), NULL, ENamedThreads::AnyHiPriThreadNormalTask);
						return;
					}
					break;
				}

				FFunctionGraphTask::CreateAndDispatchWhenReady(Broadcast, TStatId(), NULL, ENamedThreads::AnyThread);
			}
		};

		int32 NumThreads = 1; // careful, it is easy to deadlock, one should not wait in task graph tasks!
		check(NumThreads > 0);
		for (int32 i = 0; i < NumThreads; i++)
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady(Broadcast, TStatId(), NULL, ENamedThreads::AnyThread);
		}
		if (ENamedThreads::bHasBackgroundThreads)
		{
			for (int32 i = 0; i < NumThreads; i++)
			{
				FFunctionGraphTask::CreateAndDispatchWhenReady(Broadcast, TStatId(), NULL, ENamedThreads::AnyBackgroundThreadNormalTask);
			}
		}
		if (ENamedThreads::bHasHighPriorityThreads)
		{
			for (int32 i = 0; i < NumThreads; i++)
			{
				FFunctionGraphTask::CreateAndDispatchWhenReady(Broadcast, TStatId(), NULL, ENamedThreads::AnyHiPriThreadNormalTask);
			}
		}
	}
}

static FAutoConsoleCommand PakFileTestCmd(
	TEXT("PakFileTest"),
	TEXT("Tests the low level filesystem by mounting a pak file and doing multithreaded loads on it forever. Arg should be a full path to a pak file."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&PakFileTest)
);

#endif

// REVERB
#if !UE_BUILD_SHIPPING
int32 UEngine::RenderStatReverb(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		AudioDevice->RenderStatReverb(World, Viewport, Canvas, X, Y, ViewLocation, ViewRotation);
	}

	return Y;
}

void FAudioDevice::RenderStatReverb(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32& Y, const FVector* ViewLocation, const FRotator* ViewRotation) const
{
	UReverbEffect* ReverbEffect = GetCurrentReverbEffect();
	FString TheString;
	if (ReverbEffect)
	{
		TheString = FString::Printf(TEXT("Active Reverb Effect: %s"), *ReverbEffect->GetName());
		Canvas->DrawShadowedString(X, Y, *TheString, UEngine::GetSmallFont(), FLinearColor::White);
		Y += 12;

		AAudioVolume* CurrentAudioVolume = nullptr;
		for (const FTransform& Transform : ListenerTransforms)
		{
			AAudioVolume* PlayerAudioVolume = World->GetAudioSettings(Transform.GetLocation(), nullptr, nullptr);
			if (PlayerAudioVolume && ((CurrentAudioVolume == nullptr) || (PlayerAudioVolume->GetPriority() > CurrentAudioVolume->GetPriority())))
			{
				CurrentAudioVolume = PlayerAudioVolume;
			}
		}
		if (CurrentAudioVolume && CurrentAudioVolume->GetReverbSettings().ReverbEffect)
		{
			TheString = FString::Printf(TEXT("  Audio Volume Reverb Effect: %s (Priority: %g Volume Name: %s)"), *CurrentAudioVolume->GetReverbSettings().ReverbEffect->GetName(), CurrentAudioVolume->GetPriority(), *CurrentAudioVolume->GetName());
		}
		else
		{
			TheString = TEXT("  Audio Volume Reverb Effect: None");
		}
		Canvas->DrawShadowedString(X, Y, *TheString, UEngine::GetSmallFont(), FLinearColor::White);
		Y += 12;
		if (ActivatedReverbs.Num() == 0)
		{
			TheString = TEXT("  Activated Reverb: None");
			Canvas->DrawShadowedString(X, Y, *TheString, UEngine::GetSmallFont(), FLinearColor::White);
			Y += 12;
		}
		else if (ActivatedReverbs.Num() == 1)
		{
			auto It = ActivatedReverbs.CreateConstIterator();
			TheString = FString::Printf(TEXT("  Activated Reverb Effect: %s (Priority: %g Tag: '%s')"), *It.Value().ReverbSettings.ReverbEffect->GetName(), It.Value().Priority, *It.Key().ToString());
			Canvas->DrawShadowedString(X, Y, *TheString, UEngine::GetSmallFont(), FLinearColor::White);
			Y += 12;
		}
		else
		{
			Canvas->DrawShadowedString(X, Y, TEXT("  Activated Reverb Effects:"), UEngine::GetSmallFont(), FLinearColor::White);
			Y += 12;
			TMap<int32, FString> PrioritySortedActivatedReverbs;
			for (auto It = ActivatedReverbs.CreateConstIterator(); It; ++It)
			{
				TheString = FString::Printf(TEXT("    %s (Priority: %g Tag: '%s')"), *It.Value().ReverbSettings.ReverbEffect->GetName(), It.Value().Priority, *It.Key().ToString());
				PrioritySortedActivatedReverbs.Add(It.Value().Priority, TheString);
			}
			for (auto It = PrioritySortedActivatedReverbs.CreateConstIterator(); It; ++It)
			{
				Canvas->DrawShadowedString(X, Y, *It.Value(), UEngine::GetSmallFont(), FLinearColor::White);
				Y += 12;
			}
		}
	}
	else
	{
		TheString = TEXT("Active Reverb Effect: None");
		Canvas->DrawShadowedString(X, Y, *TheString, UEngine::GetSmallFont(), FLinearColor::White);
		Y += 12;
	}
}

// SOUNDMIXES
int32 UEngine::RenderStatSoundMixes(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	Canvas->DrawShadowedString(X, Y, TEXT("Active Sound Mixes:"), GetSmallFont(), FColor::Green);
	Y += 12;

	bool bDisplayedSoundMixes = false;

	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		const FAudioStats& AudioStats = AudioDevice->GetAudioStats();

		if (!AudioStats.bStale)
		{
			if (AudioStats.StatSoundMixes.Num() > 0)
			{
				bDisplayedSoundMixes = true;

				for (const FAudioStats::FStatSoundMix& StatSoundMix : AudioStats.StatSoundMixes)
				{
					const FString TheString = FString::Printf(TEXT("%s - Fade Proportion: %1.2f - Total Ref Count: %i"), *StatSoundMix.MixName, StatSoundMix.InterpValue, StatSoundMix.RefCount);

					const FColor& TextColour = (StatSoundMix.bIsCurrentEQ ? FColor::Yellow : FColor::White);

					Canvas->DrawShadowedString(X + 12, Y, *TheString, GetSmallFont(), TextColour);
					Y += 12;
				}
			}
		}
	}
	
	if (!bDisplayedSoundMixes)
	{
		Canvas->DrawShadowedString(X + 12, Y, TEXT("None"), GetSmallFont(), FColor::White);
		Y += 12;
	}
	return Y;
}

void FAudioDevice::UpdateSoundShowFlags(const uint8 OldSoundShowFlags, const uint8 NewSoundShowFlags)
{
	if (NewSoundShowFlags != OldSoundShowFlags)
	{
		uint8 RequestedStatChange = 0;
		if ((NewSoundShowFlags == FViewportClient::ESoundShowFlags::Disabled) || (OldSoundShowFlags == FViewportClient::ESoundShowFlags::Disabled))
		{
			RequestedStatChange |= ERequestedAudioStats::Sounds;
		}
		if ((NewSoundShowFlags ^ OldSoundShowFlags) & FViewportClient::ESoundShowFlags::Debug)
		{
			RequestedStatChange |= ERequestedAudioStats::DebugSounds;
		}
		if ((NewSoundShowFlags ^ OldSoundShowFlags) & FViewportClient::ESoundShowFlags::Long_Names)
		{
			RequestedStatChange |= ERequestedAudioStats::LongSoundNames;
		}
		if (RequestedStatChange != 0)
		{
			UpdateRequestedStat(RequestedStatChange);
		}
	}
}

void FAudioDevice::ResolveDesiredStats(FViewportClient* ViewportClient)
{
	check(IsInGameThread());

	uint8 SetStats = 0;
	uint8 ClearStats = 0;

	if (ViewportClient->IsStatEnabled(TEXT("SoundCues")))
	{
		SetStats |= ERequestedAudioStats::SoundCues;
	}
	else
	{
		ClearStats |= ERequestedAudioStats::SoundCues;
	}

	if (ViewportClient->IsStatEnabled(TEXT("SoundWaves")))
	{
		SetStats |= ERequestedAudioStats::SoundWaves;
	}
	else
	{
		ClearStats |= ERequestedAudioStats::SoundWaves;
	}

	if (ViewportClient->IsStatEnabled(TEXT("SoundMixes")))
	{
		SetStats |= ERequestedAudioStats::SoundMixes;
	}
	else
	{
		ClearStats |= ERequestedAudioStats::SoundMixes;
	}

	if (ViewportClient->IsStatEnabled(TEXT("Sounds")))
	{
		const FViewportClient::ESoundShowFlags::Type SoundShowFlags = ViewportClient->GetSoundShowFlags();
		SetStats |= ERequestedAudioStats::Sounds;
		
		if (SoundShowFlags & FViewportClient::ESoundShowFlags::Debug)
		{
			SetStats |= ERequestedAudioStats::DebugSounds;
		}
		else
		{
			ClearStats |= ERequestedAudioStats::DebugSounds;
		}

		if (SoundShowFlags & FViewportClient::ESoundShowFlags::Long_Names)
		{
			SetStats |= ERequestedAudioStats::LongSoundNames;
		}
		else
		{
			ClearStats |= ERequestedAudioStats::LongSoundNames;
		}
	}
	else
	{
		ClearStats |= ERequestedAudioStats::Sounds;
		ClearStats |= ERequestedAudioStats::DebugSounds;
		ClearStats |= ERequestedAudioStats::LongSoundNames;
	}

	DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.ResolveDesiredStats"), STAT_AudioResolveDesiredStats, STATGROUP_TaskGraphTasks);

	FAudioDevice* AudioDevice = this;
	FAudioThread::RunCommandOnAudioThread([AudioDevice, SetStats, ClearStats]()
	{
		AudioDevice->RequestedAudioStats |= SetStats;
		AudioDevice->RequestedAudioStats &= ~ClearStats;
	}, GET_STATID(STAT_AudioResolveDesiredStats));
}

void FAudioDevice::UpdateRequestedStat(const uint8 RequestedStat)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.UpdateRequestedStat"), STAT_AudioUpdateRequestedStat, STATGROUP_TaskGraphTasks);

		FAudioDevice* AudioDevice = this;
		FAudioThread::RunCommandOnAudioThread([AudioDevice, RequestedStat]()
		{
			AudioDevice->UpdateRequestedStat(RequestedStat);
		}, GET_STATID(STAT_AudioUpdateRequestedStat));
		return;
	}

	RequestedAudioStats ^= RequestedStat;
}

bool UEngine::ToggleStatSoundWaves(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if (AudioDeviceManager)
	{
		AudioDeviceManager->ToggleDebugStat(ERequestedAudioStats::SoundWaves);
	}
	return true;
}

bool UEngine::ToggleStatSoundCues(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if (AudioDeviceManager)
	{
		AudioDeviceManager->ToggleDebugStat(ERequestedAudioStats::SoundCues);
	}
	return true;
}

bool UEngine::ToggleStatSoundMixes(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if (AudioDeviceManager)
	{
		AudioDeviceManager->ToggleDebugStat(ERequestedAudioStats::SoundMixes);
	}
	return true;
}

// SOUNDWAVES
int32 UEngine::RenderStatSoundWaves(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		const FAudioStats& AudioStats = AudioDevice->GetAudioStats();

		if (!AudioStats.bStale)
		{
			Canvas->DrawShadowedString(X, Y, TEXT("Active Sound Waves:"), GetSmallFont(), FLinearColor::White);
			Y += 12;

			typedef TPair<const FAudioStats::FStatWaveInstanceInfo*, const FAudioStats::FStatSoundInfo*> FWaveInstancePair;

			TArray<FWaveInstancePair> WaveInstances;

			for (const FAudioStats::FStatSoundInfo& StatSoundInfo : AudioStats.StatSoundInfos)
			{
				for (const FAudioStats::FStatWaveInstanceInfo& WaveInstanceInfo : StatSoundInfo.WaveInstanceInfos)
				{
					if (WaveInstanceInfo.ActualVolume >= 0.01f)
					{
						WaveInstances.Emplace(&WaveInstanceInfo, &StatSoundInfo);
					}
				}
			}

			WaveInstances.Sort([](const FWaveInstancePair& A, const FWaveInstancePair& B) { return A.Key->InstanceIndex < B.Key->InstanceIndex; });

			for (const FWaveInstancePair& WaveInstanceInfo : WaveInstances)
			{
				UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(WaveInstanceInfo.Value->AudioComponentID);
				AActor* SoundOwner = AudioComponent ? AudioComponent->GetOwner() : nullptr;

				FString TheString = *FString::Printf(TEXT("%4i.    %6.2f  %s   Owner: %s   SoundClass: %s"),
					WaveInstanceInfo.Key->InstanceIndex,
					WaveInstanceInfo.Key->ActualVolume,
					*WaveInstanceInfo.Key->WaveInstanceName.ToString(),
					SoundOwner ? *SoundOwner->GetName() : TEXT("None"),
					*WaveInstanceInfo.Value->SoundClassName.ToString());

				Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor::White);
				Y += 12;
			}

			const int32 ActiveInstances = WaveInstances.Num();

			const int32 Max = AudioDevice->MaxChannels / 2;
			float f = FMath::Clamp<float>((float)(ActiveInstances - Max) / (float)Max, 0.f, 1.f);
			const int32 R = FMath::TruncToInt(f * 255);

			if (ActiveInstances > Max)
			{
				f = FMath::Clamp<float>((float)(Max - ActiveInstances) / (float)Max, 0.5f, 1.f);
			}
			else
			{
				f = 1.0f;
			}
			const int32 G = FMath::TruncToInt(f * 255);
			const int32 B = 0;

			Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT(" Total: %i"), ActiveInstances), GetSmallFont(), FColor(R, G, B));
			Y += 12;
		}
	}
	else
	{
		Canvas->DrawShadowedString(X, Y, TEXT("Active Sound Waves:"), GetSmallFont(), FLinearColor::White);
		Y += 12;

		Canvas->DrawShadowedString(X, Y, TEXT(" Total: 0"), GetSmallFont(), FLinearColor::White);
		Y += 12;
	}
	return Y;
}

// SOUNDCUES
int32 UEngine::RenderStatSoundCues(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	Canvas->DrawShadowedString(X, Y, TEXT("Active Sound Cues:"), GetSmallFont(), FColor::Green);
	Y += 12;

	int32 ActiveSoundCount = 0;

	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		const FAudioStats& AudioStats = AudioDevice->GetAudioStats();

		for (const FAudioStats::FStatSoundInfo& StatSoundInfo : AudioDevice->GetAudioStats().StatSoundInfos)
		{
			for (const FAudioStats::FStatWaveInstanceInfo& WaveInstanceInfo : StatSoundInfo.WaveInstanceInfos)
			{
				if (WaveInstanceInfo.ActualVolume >= 0.01f)
				{
					const FString TheString = FString::Printf(TEXT("%4i. %s %s"), ActiveSoundCount++, *StatSoundInfo.SoundName, *StatSoundInfo.SoundClassName.ToString());
					Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor::White);
					Y += 12;
					break;
				}
			}
		}
	}

	Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT("Total: %i"), ActiveSoundCount), GetSmallFont(), FColor::Green);
	Y += 12;
	return Y;
}

// SOUNDS
bool UEngine::ToggleStatSounds(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
{
	if (ViewportClient == nullptr)
	{
		// Ignore if all Viewports are closed.
		return false;
	}
	const bool bHelp = Stream ? FCString::Stristr(Stream, TEXT("?")) != NULL : false;
	if (bHelp)
	{
		GLog->Logf(TEXT("stat sounds description"));
		GLog->Logf(TEXT("  stat sounds off - Disables drawing stat sounds"));
		GLog->Logf(TEXT("  stat sounds sort=distance|class|name|waves|default"));
		GLog->Logf(TEXT("      distance - sort list by distance to player"));
		GLog->Logf(TEXT("      class - sort by sound class name"));
		GLog->Logf(TEXT("      name - sort by cue pathname"));
		GLog->Logf(TEXT("      waves - sort by waves' num"));
		GLog->Logf(TEXT("      default - sorting is no enabled"));
		GLog->Logf(TEXT("  stat sounds -debug - enables debugging mode like showing sound radius sphere and names, but only for cues with enabled property bDebug"));
		GLog->Logf(TEXT(""));
		GLog->Logf(TEXT("Ex. stat sounds sort=class -debug"));
		GLog->Logf(TEXT(" This will show only debug sounds sorted by sound class"));
	}

	uint32 OldSoundShowFlags = ViewportClient->GetSoundShowFlags();

	uint32 ShowSounds = FViewportClient::ESoundShowFlags::Disabled;

	if (Stream)
	{
		const bool bHide = FParse::Command(&Stream, TEXT("off"));
		if (bHide)
		{
			ShowSounds = FViewportClient::ESoundShowFlags::Disabled;
		}
		else
		{
			const bool bDebug = FParse::Param(Stream, TEXT("debug"));
			if (bDebug)
			{
				ShowSounds |= FViewportClient::ESoundShowFlags::Debug;
			}

			const bool bLongNames = FParse::Param(Stream, TEXT("longnames"));
			if (bLongNames)
			{
				ShowSounds |= FViewportClient::ESoundShowFlags::Long_Names;
			}

			FString SortStr;
			FParse::Value(Stream, TEXT("sort="), SortStr);
			if (SortStr == TEXT("distance"))
			{
				ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Distance;
			}
			else if (SortStr == TEXT("class"))
			{
				ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Class;
			}
			else if (SortStr == TEXT("name"))
			{
				ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Name;
			}
			else if (SortStr == TEXT("waves"))
			{
				ShowSounds |= FViewportClient::ESoundShowFlags::Sort_WavesNum;
			}
			else
			{
				ShowSounds |= FViewportClient::ESoundShowFlags::Sort_Disabled;
			}
		}
	}

	if (OldSoundShowFlags != FViewportClient::ESoundShowFlags::Disabled)
	{
		if (ShowSounds != FViewportClient::ESoundShowFlags::Disabled && ShowSounds != FViewportClient::ESoundShowFlags::Sort_Disabled)
		{
			if (!ViewportClient->IsStatEnabled(TEXT("Sounds")))
			{
				if (const TArray<FString>* CurrentStats = ViewportClient->GetEnabledStats())
				{
					TArray<FString> NewStats = *CurrentStats;
					NewStats.Add(TEXT("Sounds"));
					ViewportClient->SetEnabledStats(NewStats);
					ViewportClient->SetShowStats(true);
				}
			}
		}
		else
		{
			ShowSounds = FViewportClient::ESoundShowFlags::Disabled;
		}
	}
	else if (ShowSounds == FViewportClient::ESoundShowFlags::Disabled)
	{
		if (ViewportClient->IsStatEnabled(TEXT("Sounds")))
		{
			if (const TArray<FString>* CurrentStats = ViewportClient->GetEnabledStats())
			{
				TArray<FString> NewStats = *CurrentStats;
				NewStats.Remove(TEXT("Sounds"));
				ViewportClient->SetEnabledStats(NewStats);
			}
		}
	}
	ViewportClient->SetSoundShowFlags((FViewportClient::ESoundShowFlags::Type)ShowSounds);

	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		AudioDevice->UpdateSoundShowFlags(OldSoundShowFlags, ShowSounds);
	}
	
	return true;
}

int32 UEngine::RenderStatSounds(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	const FViewportClient::ESoundShowFlags::Type ShowSounds = Viewport->GetClient() ? Viewport->GetClient()->GetSoundShowFlags() : FViewportClient::ESoundShowFlags::Disabled;
	const bool bDebug = ShowSounds & FViewportClient::ESoundShowFlags::Debug;

	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		FAudioStats& AudioStats = AudioDevice->GetAudioStats();
		if (!AudioStats.bStale)
		{
			FString SortingName = TEXT("disabled");

			// Sort the list.
			if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_Name)
			{
				AudioStats.StatSoundInfos.Sort([](const FAudioStats::FStatSoundInfo& A, const FAudioStats::FStatSoundInfo& B) { return A.SoundName < B.SoundName; });
				SortingName = TEXT("pathname");
			}
			else if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_Distance)
			{
				AudioStats.StatSoundInfos.Sort([](const FAudioStats::FStatSoundInfo& A, const FAudioStats::FStatSoundInfo& B) { return A.Distance < B.Distance; });
				SortingName = TEXT("distance");
			}
			else if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_Class)
			{
				AudioStats.StatSoundInfos.Sort([](const FAudioStats::FStatSoundInfo& A, const FAudioStats::FStatSoundInfo& B) { return A.SoundClassName < B.SoundClassName; });
				SortingName = TEXT("class");
			}
			else if (ShowSounds & FViewportClient::ESoundShowFlags::Sort_WavesNum)
			{
				AudioStats.StatSoundInfos.Sort([](const FAudioStats::FStatSoundInfo& A, const FAudioStats::FStatSoundInfo& B) { return A.WaveInstanceInfos.Num() > B.WaveInstanceInfos.Num(); });
				SortingName = TEXT("waves' num");
			}

			Canvas->DrawShadowedString(X, Y, TEXT("Active Sounds:"), GetSmallFont(), FColor::Green);
			Y += 12;

			const FString InfoText = FString::Printf(TEXT(" Sorting: %s Debug: %s"), *SortingName, bDebug ? TEXT("enabled") : TEXT("disabled"));
			Canvas->DrawShadowedString(X, Y, *InfoText, GetSmallFont(), FColor(128, 255, 128));
			Y += 12;

			Canvas->DrawShadowedString(X, Y, TEXT("Index Path (Class) Distance"), GetSmallFont(), FColor::Green);
			Y += 12;

			int32 TotalSoundWavesNum = 0;
			for (int32 SoundIndex = 0 ; SoundIndex < AudioStats.StatSoundInfos.Num(); ++SoundIndex)
			{
				const FAudioStats::FStatSoundInfo& StatSoundInfo = AudioStats.StatSoundInfos[SoundIndex];
				const int32 WaveInstancesNum = StatSoundInfo.WaveInstanceInfos.Num();
				if (WaveInstancesNum > 0)
				{
					{
						const FString TheString = FString::Printf(TEXT("%4i. %s (%s) %6.2f"), SoundIndex, *StatSoundInfo.SoundName, *StatSoundInfo.SoundClassName.ToString(), StatSoundInfo.Distance);
						Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor::White);
						Y += 12;
					}

					TotalSoundWavesNum += WaveInstancesNum;

					// Get the active sound waves.
					for (int32 WaveIndex = 0; WaveIndex < WaveInstancesNum; WaveIndex++)
					{
						const FString TheString = *FString::Printf(TEXT("    %4i. %s"), WaveIndex, *StatSoundInfo.WaveInstanceInfos[WaveIndex].Description);
						Canvas->DrawShadowedString(X, Y, *TheString, GetSmallFont(), FColor(205, 205, 205));
						Y += 12;
					}
				}
			}

			Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT("Total sounds: %i, sound waves: %i"), AudioStats.StatSoundInfos.Num(), TotalSoundWavesNum), GetSmallFont(), FColor::Green);
			Y += 12;

			Canvas->DrawShadowedString(X, Y, *FString::Printf(TEXT("Listener position: %s"), *AudioStats.ListenerLocation.ToString()), GetSmallFont(), FColor::Green);
			Y += 12;
		}

		// Draw sound cue's sphere.
		if (bDebug)
		{
			for (const FAudioStats::FStatSoundInfo& StatSoundInfo : AudioStats.StatSoundInfos)
			{
				const FTransform& SoundTransform = StatSoundInfo.Transform;
				const int32 WaveInstancesNum = StatSoundInfo.WaveInstanceInfos.Num();

				if (StatSoundInfo.Distance > 100.0f && WaveInstancesNum > 0)
				{
					float SphereRadius = 0.f;
					float SphereInnerRadius = 0.f;

					if (StatSoundInfo.ShapeDetailsMap.Num() > 0)
					{
						DrawDebugString(World, SoundTransform.GetTranslation(), StatSoundInfo.SoundName, NULL, FColor::White, 0.01f);

						for (auto ShapeDetailsIt = StatSoundInfo.ShapeDetailsMap.CreateConstIterator(); ShapeDetailsIt; ++ShapeDetailsIt)
						{
							const FBaseAttenuationSettings::AttenuationShapeDetails& ShapeDetails = ShapeDetailsIt.Value();
							switch (ShapeDetailsIt.Key())
							{
							case EAttenuationShape::Sphere:
								if (ShapeDetails.Falloff > 0.f)
								{
									DrawDebugSphere(World, SoundTransform.GetTranslation(), ShapeDetails.Extents.X + ShapeDetails.Falloff, 10, FColor(155, 155, 255));
									DrawDebugSphere(World, SoundTransform.GetTranslation(), ShapeDetails.Extents.X, 10, FColor(55, 55, 255));
								}
								else
								{
									DrawDebugSphere(World, SoundTransform.GetTranslation(), ShapeDetails.Extents.X, 10, FColor(155, 155, 255));
								}
								break;

							case EAttenuationShape::Box:
								if (ShapeDetails.Falloff > 0.f)
								{
									DrawDebugBox(World, SoundTransform.GetTranslation(), ShapeDetails.Extents + FVector(ShapeDetails.Falloff), SoundTransform.GetRotation(), FColor(155, 155, 255));
									DrawDebugBox(World, SoundTransform.GetTranslation(), ShapeDetails.Extents, SoundTransform.GetRotation(), FColor(55, 55, 255));
								}
								else
								{
									DrawDebugBox(World, SoundTransform.GetTranslation(), ShapeDetails.Extents, SoundTransform.GetRotation(), FColor(155, 155, 255));
								}
								break;

							case EAttenuationShape::Capsule:

								if (ShapeDetails.Falloff > 0.f)
								{
									DrawDebugCapsule(World, SoundTransform.GetTranslation(), ShapeDetails.Extents.X + ShapeDetails.Falloff, ShapeDetails.Extents.Y + ShapeDetails.Falloff, SoundTransform.GetRotation(), FColor(155, 155, 255));
									DrawDebugCapsule(World, SoundTransform.GetTranslation(), ShapeDetails.Extents.X, ShapeDetails.Extents.Y, SoundTransform.GetRotation(), FColor(55, 55, 255));
								}
								else
								{
									DrawDebugCapsule(World, SoundTransform.GetTranslation(), ShapeDetails.Extents.X, ShapeDetails.Extents.Y, SoundTransform.GetRotation(), FColor(155, 155, 255));
								}
								break;

							case EAttenuationShape::Cone:
							{
								const FVector Origin = SoundTransform.GetTranslation() - (SoundTransform.GetUnitAxis(EAxis::X) * ShapeDetails.ConeOffset);

								if (ShapeDetails.Falloff > 0.f || ShapeDetails.Extents.Z > 0.f)
								{
									const float OuterAngle = FMath::DegreesToRadians(ShapeDetails.Extents.Y + ShapeDetails.Extents.Z);
									const float InnerAngle = FMath::DegreesToRadians(ShapeDetails.Extents.Y);
									DrawDebugCone(World, Origin, SoundTransform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.Falloff + ShapeDetails.ConeOffset, OuterAngle, OuterAngle, 10, FColor(155, 155, 255));
									DrawDebugCone(World, Origin, SoundTransform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.ConeOffset, InnerAngle, InnerAngle, 10, FColor(55, 55, 255));
								}
								else
								{
									const float Angle = FMath::DegreesToRadians(ShapeDetails.Extents.Y);
									DrawDebugCone(World, Origin, SoundTransform.GetUnitAxis(EAxis::X), ShapeDetails.Extents.X + ShapeDetails.ConeOffset, Angle, Angle, 10, FColor(155, 155, 255));
								}
								break;
							}

							default:
								check(false);
							}
						}
					}
				}
			}
		}
	}
	return Y;
}
#endif // !UE_BUILD_SHIPPING

// AI
int32 UEngine::RenderStatAI(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Pick a larger font on console.
	UFont* Font = FPlatformProperties::SupportsWindowedMode() ? GetSmallFont() : GetMediumFont();

	// gather numbers
	int32 NumAI = 0;
	int32 NumAIRendered = 0;
	for (FConstControllerIterator Iterator = World->GetControllerIterator(); Iterator; ++Iterator)
	{
		AController* Controller = Iterator->Get();
		if (!Cast<APlayerController>(Controller))
		{
			++NumAI;
			if (Controller->GetPawn() != NULL && World->GetTimeSeconds() - Controller->GetPawn()->GetLastRenderTime() < 0.08f)
			{
				++NumAIRendered;
			}
		}
	}


#define MAXDUDES 20
#define BADAMTOFDUDES 12
	FColor TotalColor = FColor::Green;
	if (NumAI > BADAMTOFDUDES)
	{
		float Scalar = 1.0f - FMath::Clamp<float>((float)NumAI / (float)MAXDUDES, 0.f, 1.f);

		TotalColor = FColor::MakeRedToGreenColorFromScalar(Scalar);
	}

	FColor RenderedColor = FColor::Green;
	if (NumAIRendered > BADAMTOFDUDES)
	{
		float Scalar = 1.0f - FMath::Clamp<float>((float)NumAIRendered / (float)MAXDUDES, 0.f, 1.f);

		RenderedColor = FColor::MakeRedToGreenColorFromScalar(Scalar);

	}

	const int32 RowHeight = FMath::TruncToInt(Font->GetMaxCharHeight() * 1.1f);
	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%i AI"), NumAI),
		Font,
		TotalColor
		);
	Y += RowHeight;

	Canvas->DrawShadowedString(
		X,
		Y,
		*FString::Printf(TEXT("%i AI Rendered"), NumAIRendered),
		Font,
		RenderedColor
		);
	Y += RowHeight;
	return Y;
}

// SLATEBATCHES
#if STATS
int32 UEngine::RenderStatSlateBatches(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	/* @todo Slate Rendering
	UFont* Font = SmallFont;
	
	const TArray<FBatchStats>& Stats = FSlateApplication::Get().GetRenderer()->GetBatchStats();
	
	// Start drawing the various counters.
	const int32 RowHeight = FMath::Trunc( Font->GetMaxCharHeight() * 1.1f );
	
	X = Viewport->GetSizeXY().X - 350;
	
	Canvas->DrawShadowedString(
		X,
		Y,
		TEXT("Slate Batches:"),
		Font, 
		FColor::Green );
	
	Y+=RowHeight;
	
	
	for( int32 I = 0; I < Stats.Num(); ++I )
	{
		const FBatchStats& Stat = Stats(I);
	
		// Draw a box representing the debug color of the batch
		DrawTriangle2D(Canvas, FVector2D(X,Y), FVector2D(0,0), FVector2D(X+10,Y), FVector2D(0,0), FVector2D(X+10,Y+7), FVector2D(0,0), Stat.BatchColor );
		DrawTriangle2D(Canvas, FVector2D(X,Y), FVector2D(0,0), FVector2D(X,Y+7), FVector2D(0,0), FVector2D(X+10,Y+7), FVector2D(0,0), Stat.BatchColor );
	
		Canvas->DrawShadowedString(
			X+15,
			Y,
			*FString::Printf(TEXT("Layer: %d, Elements: %d, Vertices: %d"), Stat.Layer, Stat.NumElementsInBatch, Stat.NumVertices ), 
			Font,
			FColor::Green );
		Y += RowHeight;
	}*/
	return Y;
}
#endif

#undef LOCTEXT_NAMESPACE
