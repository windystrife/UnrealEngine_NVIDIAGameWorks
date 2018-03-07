#include "BlastModule.h"
#include "IPluginManager.h"

#include "PhysicsPublic.h"
#include "NvBlastGlobals.h"
#include "BlastGlobals.h"
#include "LogVerbosity.h"
#include "Paths.h"
#include "ModuleManager.h"

class BlastAllocatorCallback final : public Nv::Blast::AllocatorCallback
{
	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line)
	{
		void* ptr = FMemory::Malloc(size, 16);
		check((reinterpret_cast<size_t>(ptr) & 15) == 0);
		return ptr;

	}

	virtual void deallocate(void* ptr)
	{
		FMemory::Free(ptr);
	}
};

static BlastAllocatorCallback g_blastAllocatorCallback;

class BlastErrorCallback final : public Nv::Blast::ErrorCallback
{
	virtual void reportError(Nv::Blast::ErrorCode::Enum code, const char* message, const char* file, int line)
	{
#if !NO_LOGGING
		ELogVerbosity::Type verbosity = ELogVerbosity::Log;

		switch (code)
		{
		case Nv::Blast::ErrorCode::eINVALID_OPERATION:
			verbosity = ELogVerbosity::Error;
			break;
		case Nv::Blast::ErrorCode::eDEBUG_WARNING:
			verbosity = ELogVerbosity::Warning;
			break;
		case Nv::Blast::ErrorCode::eDEBUG_INFO:
			verbosity = ELogVerbosity::Log;
			break;
		case Nv::Blast::ErrorCode::eNO_ERROR:
			verbosity = ELogVerbosity::Log;
			break;
		}

		if ((verbosity & ELogVerbosity::VerbosityMask) <= FLogCategoryLogBlast::CompileTimeVerbosity)
		{
			if (!LogBlast.IsSuppressed(verbosity))
			{
				FMsg::Logf(file, line, LogBlast.GetCategoryName(), verbosity,
					TEXT("Blast Log : %s"), ANSI_TO_TCHAR(message));
			}
		}
#endif
	}
};

static BlastErrorCallback g_blastErrorCallback;
IMPLEMENT_MODULE(FBlastModule, Blast);

FBlastModule::FBlastModule()
{
}

void FBlastModule::StartupModule()
{
	// Set the allocator and log for serialization
	NvBlastGlobalSetAllocatorCallback(&g_blastAllocatorCallback);
	NvBlastGlobalSetErrorCallback(&g_blastErrorCallback);
}

void FBlastModule::ShutdownModule()
{
}
