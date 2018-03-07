// @third party code - BEGIN HairWorks
#include "HairWorksSDK.h"
#include "EngineLogs.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "CanvasTypes.h"

#include "AllowWindowsPlatformTypes.h"
#include <Nv/Common/Render/Dx11/NvCoDx11Handle.h>
#pragma warning(push)
#pragma warning(disable: 4191)	// For DLL function pointer conversion
#undef DWORD	// To fix a compiling error
#include <Nv/HairWorks/Platform/Win/NvHairWinLoadSdk.h>
#pragma warning(pop)
#include "HideWindowsPlatformTypes.h"

DEFINE_LOG_CATEGORY(LogHairWorks);

namespace HairWorks{
	// Logger
#if !NO_LOGGING
	class Logger: public NvCo::Logger
	{
		virtual void log(NvCo::ELogSeverity severity, const NvChar* text, const NvChar* function, const NvChar* filename, NvInt lineNumber) override
		{
			ELogVerbosity::Type UELogVerb;

			switch(severity)
			{
			case NvCo::ELogSeverity::DEBUG_INFO:
				UELogVerb = ELogVerbosity::Log;
				break;
			case NvCo::ELogSeverity::INFO:
				UELogVerb = ELogVerbosity::Display;
				break;
			case NvCo::ELogSeverity::WARNING:
				UELogVerb = ELogVerbosity::Warning;
				break;
			case NvCo::ELogSeverity::NON_FATAL_ERROR:
				UELogVerb = ELogVerbosity::Error;
				break;
			case NvCo::ELogSeverity::FATAL_ERROR:
				UELogVerb = ELogVerbosity::Fatal;
				break;
			default:
				UELogVerb = ELogVerbosity::All;
				break;
			}

			if(!LogHairWorks.IsSuppressed(UELogVerb))
			{
				FMsg::Logf(filename, lineNumber, LogHairWorks.GetCategoryName(), UELogVerb, TEXT("%s"), ANSI_TO_TCHAR(text));
			}
		}
	};
#endif

	FD3DHelper* D3DHelper = nullptr;

	ENGINE_API NvHair::Sdk* SDK = nullptr;

	ENGINE_API NvHair::ConversionSettings AssetConversionSettings;

	ENGINE_API NvHair::Sdk* GetSDK()
	{
		return SDK;
	}

	ENGINE_API const NvHair::ConversionSettings& GetAssetConversionSettings()
	{
		return AssetConversionSettings;
	}

	ENGINE_API FD3DHelper& GetD3DHelper()
	{
		check(GetSDK());
		return *D3DHelper;
	}

	ENGINE_API void Initialize(ID3D11Device& D3DDevice, ID3D11DeviceContext& D3DContext, FD3DHelper& InD3DHelper)
	{
		// Check feature level.
		if(D3DDevice.GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
		{
			UE_LOG(LogHairWorks, Error, TEXT("Need D3D_FEATURE_LEVEL_11_0."));
			return;
		}

		// Check multi thread support
		if((D3DDevice.GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0)
		{
			UE_LOG(LogHairWorks, Error, TEXT("Can't work with D3D11_CREATE_DEVICE_SINGLETHREADED."));
			return;
		}

#if !NO_LOGGING
		static Logger logger;
#endif

		// Load dependency DLL
		const auto HairWorksBinaryDir = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/HairWorks");
		FPlatformProcess::GetDllHandle(*(HairWorksBinaryDir / TEXT("d3dcompiler_47.dll")));

		// Initialize SDK
		FString LibPath = HairWorksBinaryDir / TEXT("NvHairWorksDx11.win");

#if PLATFORM_64BITS
		LibPath += TEXT("64");
#else
		LibPath += TEXT("32");
#endif
		static TAutoConsoleVariable<int32> CVarHairLoadDebugDll(TEXT("r.HairWorks.LoadDebugDll"), 0, TEXT(""), 0);
		if(CVarHairLoadDebugDll.GetValueOnAnyThread())
			LibPath += TEXT(".D");

		LibPath += TEXT(".dll");

		SDK = NvHair::loadSdk(TCHAR_TO_ANSI(*LibPath), NV_HAIR_VERSION, nullptr
#if !NO_LOGGING
			, &logger
#endif
		);
		if(SDK == nullptr)
		{
			UE_LOG(LogHairWorks, Error, TEXT("Failed to initialize HairWorks."));
			return;
		}

		SDK->initRenderResources(NvCo::Dx11Type::wrap(&D3DDevice), NvCo::Dx11Type::wrap(&D3DContext));

		D3DHelper = &InD3DHelper;

		AssetConversionSettings.m_targetHandednessHint = NvHair::HandednessHint::LEFT;
		AssetConversionSettings.m_targetUpAxisHint = NvHair::AxisHint::Z_UP;
	}

	ENGINE_API void ShutDown()
	{
		if(SDK == nullptr)
			return;

		SDK->release();
		SDK = nullptr;
	}

#if STATS
	static struct
	{
		int32 FaceNum = 0;
		int32 HairNum = 0;
		int32 CvNum = 0;
	}GAccumulatedStats;

	static TAutoConsoleVariable<int> CVarHairStats(TEXT("r.HairWorks.Stats"), 0, TEXT(""), ECVF_RenderThreadSafe);

	void RenderStats(int32 X, int32& Y, FCanvas * Canvas)
	{
		if(GetSDK() == nullptr)
			return;

		if(CVarHairStats.GetValueOnAnyThread() == 0)
			return;

		UFont* Font = UEngine::GetMediumFont();

		const auto TotalHeight = Canvas->DrawShadowedString(
			X,
			Y,
			*FString::Printf(TEXT("HairWorks:\nFaceNum: %d\nHairNum: %d\nCvNum: %d\n"), GAccumulatedStats.FaceNum, GAccumulatedStats.HairNum, GAccumulatedStats.CvNum),
			Font,
			FColor::White
		);

		Y += TotalHeight;

		// Reset stats
		GAccumulatedStats.FaceNum = 0;
		GAccumulatedStats.HairNum = 0;
		GAccumulatedStats.CvNum = 0;
	}

	void AccumulateStats(const NvHair::Stats & HairStats)
	{
		// Accumulate stats
		GAccumulatedStats.FaceNum += HairStats.m_numFaces;
		GAccumulatedStats.HairNum += HairStats.m_numHairs;
		GAccumulatedStats.CvNum += HairStats.m_averageNumCvsPerHair * HairStats.m_numHairs;
	}
#endif
}
// @third party code - END HairWorks
