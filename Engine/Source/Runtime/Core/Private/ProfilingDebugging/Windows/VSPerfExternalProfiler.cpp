// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AssertionMacros.h"
#include "ProfilingDebugging/ExternalProfiler.h"
#include "Features/IModularFeatures.h"
#include "Templates/ScopedPointer.h"
#include "UniquePtr.h"

// Not all versions of Visual Studio include the profiler SDK headers
#if WITH_VS_PERF_PROFILER

#define VSPERF_NO_DEFAULT_LIB	// Don't use #pragma lib to import the library, we'll handle this stuff ourselves
#define PROFILERAPI				// We won't be statically importing anything (we're dynamically binding), so define PROFILERAPI to a empty value
#include "VSPerf.h"				// NOTE: This header is in <Visual Studio install directory>/Team Tools/Performance Tools/x64/PerfSDK


/**
 * Visual Studio Profiler implementation of FExternalProfiler
 */
class FVSPerfExternalProfiler : public FExternalProfiler
{

public:

	/** Constructor */
	FVSPerfExternalProfiler()
		: DLLHandle( NULL )
		, StopProfileFunction( NULL )
		, StartProfileFunction( NULL )
	{
		// Register as a modular feature
		IModularFeatures::Get().RegisterModularFeature( FExternalProfiler::GetFeatureName(), this );
	}


	/** Destructor */
	virtual ~FVSPerfExternalProfiler()
	{
		if( DLLHandle != NULL )
		{
			FPlatformProcess::FreeDllHandle( DLLHandle );
			DLLHandle = NULL;
		}
		IModularFeatures::Get().UnregisterModularFeature( FExternalProfiler::GetFeatureName(), this );
	}

	virtual void FrameSync() override
	{

	}


	/** Gets the name of this profiler as a string.  This is used to allow the user to select this profiler in a system configuration file or on the command-line */
	virtual const TCHAR* GetProfilerName() const override
	{
		return TEXT( "VSPerf" );
	}


	/** Pauses profiling. */
	virtual void ProfilerPauseFunction() override
	{
		const int ProcessOrThreadId = PROFILE_CURRENTID;
		PROFILE_COMMAND_STATUS Result = StopProfileFunction( PROFILE_GLOBALLEVEL, ProcessOrThreadId );
		if( Result != PROFILE_OK )
		{
			// ...
		}
	}


	/** Resumes profiling. */
	virtual void ProfilerResumeFunction() override
	{
		const int ProcessOrThreadId = PROFILE_CURRENTID;
		PROFILE_COMMAND_STATUS Result = StartProfileFunction( PROFILE_GLOBALLEVEL, ProcessOrThreadId );
		if( Result != PROFILE_OK )
		{
			// ...
		}
	}


	/**
	* Initializes profiler hooks. It is not valid to call pause/ Start on an uninitialized
	 * profiler and the profiler implementation is free to assert or have other undefined
	 * behavior.
	 *
	 * @return true if successful, false otherwise.
	 */
	bool Initialize()
	{
		check( DLLHandle == NULL );

		// Try to load the VSPerf DLL
		// NOTE: VSPerfXXX.dll is installed into /Windows/System32 when Visual Studio is installed.  The XXX is the version number of
		// Visual Studio.  For example, for Visual Studio 2013, the file name is VSPerf120.dll.
#if _MSC_VER >= 1900
		DLLHandle = FPlatformProcess::GetDllHandle(TEXT("VSPerf140.dll"));	// Visual Studio 2015
#elif _MSC_VER >= 1800
		DLLHandle = FPlatformProcess::GetDllHandle(TEXT("VSPerf120.dll"));	// Visual Studio 2013
#else
		// Older versions of Visual Studio did not support profiling, or did not include the profiling tools with the professional edition
#endif

		if( DLLHandle != NULL )
		{
			// Get API function pointers of interest
			{
				StopProfileFunction = (StopProfileFunctionPtr)FPlatformProcess::GetDllExport( DLLHandle, TEXT( "StopProfile" ) );
				StartProfileFunction = (StartProfileFunctionPtr)FPlatformProcess::GetDllExport( DLLHandle, TEXT( "StartProfile" ) );
			}

			if( StopProfileFunction == NULL || StartProfileFunction == NULL )
			{
				// Couldn't find the functions we need.  VSPerf support will not be active.
				FPlatformProcess::FreeDllHandle( DLLHandle );
				DLLHandle = NULL;
				StopProfileFunction = NULL;
				StartProfileFunction = NULL;
			}
		}

		return DLLHandle != NULL;
	}


private:


	/** DLL handle for VSPerf.DLL */
	void* DLLHandle;

	/** Pointer to StopProfile function. */
	typedef PROFILE_COMMAND_STATUS( *StopProfileFunctionPtr )( PROFILE_CONTROL_LEVEL Level, unsigned int dwId );
	StopProfileFunctionPtr StopProfileFunction;

	/** Pointer to StartProfile function. */
	typedef PROFILE_COMMAND_STATUS( *StartProfileFunctionPtr )( PROFILE_CONTROL_LEVEL Level, unsigned int dwId );
	StartProfileFunctionPtr StartProfileFunction;
};



namespace VSPerfProfiler
{
	struct FAtModuleInit
	{
		FAtModuleInit()
		{
			static TUniquePtr<FVSPerfExternalProfiler> ProfilerVSPerf = MakeUnique<FVSPerfExternalProfiler>();
			if( !ProfilerVSPerf->Initialize() )
			{
				ProfilerVSPerf.Reset();
			}
		}
	};

	static FAtModuleInit AtModuleInit;
}



#endif	// WITH_VS_PERF_PROFILER