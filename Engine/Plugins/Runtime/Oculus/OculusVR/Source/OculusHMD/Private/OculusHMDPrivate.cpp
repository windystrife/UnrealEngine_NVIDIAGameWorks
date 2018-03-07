// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMDPrivate.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// Utility functions
//-------------------------------------------------------------------------------------------------

bool InGameThread()
{
	if (GIsGameThreadIdInitialized)
	{
		return FPlatformTLS::GetCurrentThreadId() == GGameThreadId;
	}
	else
	{
		return true;
	}
}


bool InRenderThread()
{
	if (GRenderingThread && !GIsRenderingThreadSuspended)
	{
		return FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID();
	}
	else
	{
		return InGameThread();
	}
}


bool InRHIThread()
{
	if (GRenderingThread && !GIsRenderingThreadSuspended)
	{
		if (GRHIThreadId)
		{
			if (FPlatformTLS::GetCurrentThreadId() == GRHIThreadId)
			{
				return true;
			}
			
			if (FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID())
			{
				return GetImmediateCommandList_ForRenderCommand().Bypass();
			}

			return false;
		}
		else
		{
			return FPlatformTLS::GetCurrentThreadId() == GRenderingThread->GetThreadID();
		}
	}
	else
	{
		return InGameThread();
	}
}


void ExecuteOnRenderThread(const std::function<void()>& Function)
{
	CheckInGameThread();

	if (GIsThreadedRendering && !GIsRenderingThreadSuspended)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ExecuteOnRenderThread,
			std::function<void()>, FunctionArg, Function,
			{
				FunctionArg();
			});

		FlushRenderingCommands();
	}
	else
	{
		Function();
	}
}


void ExecuteOnRenderThread_DoNotWait(const std::function<void()>& Function)
{
	CheckInGameThread();

	if (GIsThreadedRendering && !GIsRenderingThreadSuspended)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ExecuteOnRenderThread,
			std::function<void()>, FunctionArg, Function,
			{
				FunctionArg();
			});
	}
	else
	{
		Function();
	}
}


void ExecuteOnRenderThread(const std::function<void(FRHICommandListImmediate&)>& Function)
{
	CheckInGameThread();

	if (GIsThreadedRendering && !GIsRenderingThreadSuspended)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ExecuteOnRenderThread,
			std::function<void(FRHICommandListImmediate&)>, FunctionArg, Function,
			{
				FunctionArg(RHICmdList);
			});

		FlushRenderingCommands();
	}
	else
	{
		Function(GetImmediateCommandList_ForRenderCommand());
	}
}


void ExecuteOnRenderThread_DoNotWait(const std::function<void(FRHICommandListImmediate&)>& Function)
{
	CheckInGameThread();

	if (GIsThreadedRendering && !GIsRenderingThreadSuspended)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ExecuteOnRenderThread,
			std::function<void(FRHICommandListImmediate&)>, FunctionArg, Function,
			{
				FunctionArg(RHICmdList);
			});
	}
	else
	{
		Function(GetImmediateCommandList_ForRenderCommand());
	}
}


struct FRHICommandExecute_Void : public FRHICommand<FRHICommandExecute_Void>
{
	std::function<void()> Function;

	FRHICommandExecute_Void(const std::function<void()>& InFunction)
		: Function(InFunction)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		Function();
	}
};


void ExecuteOnRHIThread(const std::function<void()>& Function)
{
	CheckInRenderThread();

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	if (GRHIThreadId && !RHICmdList.Bypass())
	{
		new (RHICmdList.AllocCommand<FRHICommandExecute_Void>()) FRHICommandExecute_Void(Function);

		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
	else
	{
		Function();
	}
}


void ExecuteOnRHIThread_DoNotWait(const std::function<void()>& Function)
{
	CheckInRenderThread();

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	if (GRHIThreadId && !RHICmdList.Bypass())
	{
		new (RHICmdList.AllocCommand<FRHICommandExecute_Void>()) FRHICommandExecute_Void(Function);
	}
	else
	{
		Function();
	}
}


struct FRHICommandExecute_RHICmdList : public FRHICommand<FRHICommandExecute_RHICmdList>
{
	std::function<void(FRHICommandList&)> Function;

	FRHICommandExecute_RHICmdList(const std::function<void(FRHICommandList&)>& InFunction)
		: Function(InFunction)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		Function(*(FRHICommandList*) &RHICmdList);
	}
};


void ExecuteOnRHIThread(const std::function<void(FRHICommandList&)>& Function)
{
	CheckInRenderThread();

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	if (GRHIThreadId && !RHICmdList.Bypass())
	{
		new (RHICmdList.AllocCommand<FRHICommandExecute_RHICmdList>()) FRHICommandExecute_RHICmdList(Function);

		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
	else
	{
		Function(RHICmdList);
	}
}


void ExecuteOnRHIThread_DoNotWait(const std::function<void(FRHICommandList&)>& Function)
{
	CheckInRenderThread();

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

	if (GRHIThreadId && !RHICmdList.Bypass())
	{
		new (RHICmdList.AllocCommand<FRHICommandExecute_RHICmdList>()) FRHICommandExecute_RHICmdList(Function);
	}
	else
	{
		Function(RHICmdList);
	}
}


#if OCULUS_HMD_SUPPORTED_PLATFORMS
bool IsOculusServiceRunning()
{
#if PLATFORM_WINDOWS
	HANDLE hEvent = ::OpenEventW(SYNCHRONIZE, 0 /*FALSE*/, L"OculusHMDConnected");

	if (!hEvent)
	{
		return false;
	}

	::CloseHandle(hEvent);
#endif

	return true;
}


bool IsOculusHMDConnected()
{
#if PLATFORM_WINDOWS
	HANDLE hEvent = ::OpenEventW(SYNCHRONIZE, 0 /*FALSE*/, L"OculusHMDConnected");

	if (!hEvent)
	{
		return false;
	}

	uint32 dwWait = ::WaitForSingleObject(hEvent, 0);

	::CloseHandle(hEvent);

	if (WAIT_OBJECT_0 != dwWait)
	{
		return false;
	}
#endif

	return true;
}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

} // namespace OculusHMD
