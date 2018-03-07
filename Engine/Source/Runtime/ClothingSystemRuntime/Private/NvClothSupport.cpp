// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NvClothSupport.h"

#if WITH_NVCLOTH

#include "NvClothIncludes.h"
#include "PhysicsPublic.h"

namespace NvClothSupport
{
	class FNvClothErrorCallback* GNvClothErrorCallback;
	class FNvClothAssertHandler* GNvClothAssertHandler;

	ELogVerbosity::Type PxErrorToLogVerbosity(physx::PxErrorCode::Enum InEnum)
	{
		switch(InEnum)
		{
			case physx::PxErrorCode::eDEBUG_INFO:
				return ELogVerbosity::Display;
				break;
			case physx::PxErrorCode::eDEBUG_WARNING:
				return ELogVerbosity::Warning;
				break;
			case physx::PxErrorCode::eINVALID_PARAMETER:
				return ELogVerbosity::Warning;
				break;
			case physx::PxErrorCode::eINVALID_OPERATION:
				return ELogVerbosity::Error;
				break;
			case physx::PxErrorCode::eOUT_OF_MEMORY:
				return ELogVerbosity::Fatal;
				break;
			case physx::PxErrorCode::eINTERNAL_ERROR:
				return ELogVerbosity::Error;
				break;
			case physx::PxErrorCode::eABORT:
				return ELogVerbosity::Fatal;
				break;
			case physx::PxErrorCode::ePERF_WARNING:
				return ELogVerbosity::Warning;
				break;
			default:
				break;
		}

		return ELogVerbosity::Log;
	}

	FString PxErrorToString(physx::PxErrorCode::Enum InEnum)
	{
		switch(InEnum)
		{
			case physx::PxErrorCode::eDEBUG_INFO:
				return TEXT("Info");
				break;
			case physx::PxErrorCode::eDEBUG_WARNING:
				return TEXT("Warning");
				break;
			case physx::PxErrorCode::eINVALID_PARAMETER:
				return TEXT("Invalid Parameter");
				break;
			case physx::PxErrorCode::eINVALID_OPERATION:
				return TEXT("Invalid Operation");
				break;
			case physx::PxErrorCode::eOUT_OF_MEMORY:
				return TEXT("Out of Memory");
				break;
			case physx::PxErrorCode::eINTERNAL_ERROR:
				return TEXT("Internal Error");
				break;
			case physx::PxErrorCode::eABORT:
				return TEXT("Abort");
				break;
			case physx::PxErrorCode::ePERF_WARNING:
				return TEXT("Performance Warning");
				break;
			default:
				break;
		}

		return TEXT("Unknown");
	}

	class FNvClothErrorCallback : public physx::PxErrorCallback
	{
	public:

		FNvClothErrorCallback()
		{}

		virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override
		{
			if(code != PxErrorCode::eNO_ERROR)
			{
				// Actual error occurred, push to log
				ELogVerbosity::Type Verbosity = PxErrorToLogVerbosity(code);
				GLog->Logf(Verbosity, TEXT("NvCloth: %s, [%s:%d]"), message, file, line);
			}
		}

	};

	class FNvClothAssertHandler : public physx::PxAssertHandler
	{

	public:

		FNvClothAssertHandler()
		{}

		virtual void operator()(const char* exp, const char* file, int line, bool& ignore)
		{
			checkf(false, TEXT("NvCloth Assert: &s [%s:%d]"), exp, file, line);
		}
	};

	physx::PxAllocatorCallback* GetAllocator()
	{
		return (physx::PxAllocatorCallback*)GPhysXAllocator;
	}

	void InitializeNvClothingSystem()
	{
		GNvClothErrorCallback = new FNvClothErrorCallback();
		GNvClothAssertHandler = new FNvClothAssertHandler();

		nv::cloth::InitializeNvCloth(GetAllocator(), GNvClothErrorCallback, GNvClothAssertHandler, nullptr);
	}

	void ShutdownNvClothingSystem()
	{
		delete GNvClothAssertHandler;
		delete GNvClothErrorCallback;
	}

	ClothParticleScopeLock::ClothParticleScopeLock() : LockedCloth(nullptr)
	{

	}

	ClothParticleScopeLock::ClothParticleScopeLock(nv::cloth::Cloth* InCloth)
		: LockedCloth(InCloth)
	{
		check(LockedCloth);
		LockedCloth->lockParticles();
	}

	ClothParticleScopeLock::~ClothParticleScopeLock()
	{
		LockedCloth->unlockParticles();
	}

} // namespace NvClothSupport

#endif // WITH_NVCLOTH
