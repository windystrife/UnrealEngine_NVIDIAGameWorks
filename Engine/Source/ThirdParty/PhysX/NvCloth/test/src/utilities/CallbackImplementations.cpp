#include "CallbackImplementations.h"
#include "utilities/Utilities.h"
#define NOMINMAX
#ifdef _MSC_VER
#pragma warning(disable : 4668 4917 4365 4061 4005)
#if NV_XBOXONE
#include <d3d11_x.h>
#else
#include <d3d11.h>
#endif
#endif
#include <PsThread.h>
NvClothEnvironment* NvClothEnvironment::sEnv = nullptr;

//Tell orbis that we need more than the default 256kb heap size
size_t sceLibcHeapSize = 128 * 1024 * 1024;

#if !(PERF_TEST && NDEBUG)
void ErrorCallback::reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
{
	const char* codeName = "???";
	switch(code)
	{
#define CASE(x) case physx::PxErrorCode::Enum::x : codeName = #x; break;
		CASE(eNO_ERROR)
		CASE(eDEBUG_INFO)
		CASE(eDEBUG_WARNING)
		CASE(eINVALID_PARAMETER)
		CASE(eINVALID_OPERATION)
		CASE(eOUT_OF_MEMORY)
		CASE(eINTERNAL_ERROR)
		CASE(eABORT)
		CASE(ePERF_WARNING)
		default:
			ADD_FAILURE() << "Invalid error code used while printing to log file: code=" << code << "\n for:Log " << codeName << " from file:" << file << ":" << line << "\n MSG:" << message;
			;
#undef CASE
	}

	bool expected = false;
	for(ExpectErrorMessage* em : mExpectedErrorMessages)
	{
		expected |= em->TestMessage(code, codeName, message, file, line);
	}

	if(!expected)
	{
		std::cout << "Log " << codeName << " from file:" << file << ":" << line << "\n MSG:" << message << std::endl;

		if(code & (physx::PxErrorCode::Enum::eABORT | physx::PxErrorCode::Enum::eOUT_OF_MEMORY | physx::PxErrorCode::Enum::eINTERNAL_ERROR |
			physx::PxErrorCode::Enum::eINVALID_OPERATION | physx::PxErrorCode::Enum::eINVALID_PARAMETER | physx::PxErrorCode::eDEBUG_WARNING))
		{
			ADD_FAILURE() << "Log " << codeName << " from file:" << file << ":" << line << "\n MSG:" << message << std::endl;
		}
	}
}
#endif

#if USE_DX11
DxContextManagerCallbackImpl::DxContextManagerCallbackImpl(ID3D11Device* device, bool synchronizeResources)
	:
mDevice(device),
mSynchronizeResources(synchronizeResources)
{
	mDevice->AddRef();
	mDevice->GetImmediateContext(&mContext);
#ifdef _DEBUG
	mLockCountTls = physx::shdfnd::TlsAlloc();
#endif
}
DxContextManagerCallbackImpl::~DxContextManagerCallbackImpl()
{
	mContext->Release();

#if _DEBUG && !NV_XBOXONE
	ID3D11Debug* debugDevice;
	mDevice->QueryInterface(&debugDevice);
	if(debugDevice)
	{
		debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		debugDevice->Release();
	}
#endif

	mDevice->Release();

#if _DEBUG
	physx::shdfnd::TlsFree(mLockCountTls);
#endif
}

void DxContextManagerCallbackImpl::acquireContext()
{

	mMutex.lock();
#if _DEBUG
	physx::shdfnd::TlsSet(mLockCountTls, reinterpret_cast<void*>(reinterpret_cast<intptr_t>(physx::shdfnd::TlsGet(mLockCountTls)) + 1));
#endif
}
void DxContextManagerCallbackImpl::releaseContext()
{
#if _DEBUG
	physx::shdfnd::TlsSet(mLockCountTls, reinterpret_cast<void*>(reinterpret_cast<intptr_t>(physx::shdfnd::TlsGet(mLockCountTls)) - 1));
#endif
	mMutex.unlock();
}
ID3D11Device* DxContextManagerCallbackImpl::getDevice() const
{
	return mDevice;
}
ID3D11DeviceContext* DxContextManagerCallbackImpl::getContext() const
{
#if _DEBUG
	EXPECT_TRUE(reinterpret_cast<intptr_t>(physx::shdfnd::TlsGet(mLockCountTls)) > 0);
#endif
	return mContext;
}
bool DxContextManagerCallbackImpl::synchronizeResources() const
{
	return mSynchronizeResources;
}
#endif
