#pragma once
#include <NvCloth/Callbacks.h>
#include <foundation/PxAllocatorCallback.h>
#include <foundation/PxErrorCallback.h>
#include <foundation/PxAssert.h>
#include <foundation/PxProfiler.h>
#include <vector>
#include <gtest/gtest.h>
#include <map>
#if USE_CUDA
#include <cuda.h>
#endif
#include <mutex>
#include <NvCloth/DxContextManagerCallback.h>

class TestAllocator : public physx::PxAllocatorCallback
{
public:
	TestAllocator()
	{
		mEnableLeakDetection = false;
		mMemoryAllocated = 0;
		mPeakMemory = 0;
	}
	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line)
	{
#ifdef _MSC_VER
		void* ptr = _aligned_malloc(size, 16);
#else 
		void* ptr;
		if(posix_memalign(&ptr, 16, size)) ptr = 0;
#endif
		if(mEnableLeakDetection)
		{
			std::lock_guard<std::mutex> lock(mAllocationsMapLock);
			mAllocations[ptr] = Allocation(size, typeName, filename, line);
			mMemoryAllocated += size;
			mPeakMemory = mPeakMemory>mMemoryAllocated?mPeakMemory:mMemoryAllocated;
		}
		return ptr;
	}
	virtual void deallocate(void* ptr)
	{
		if(mEnableLeakDetection && ptr)
		{
			std::lock_guard<std::mutex> lock(mAllocationsMapLock);
			auto i = mAllocations.find(ptr);
			if(i == mAllocations.end())
			{
				printf("Tried to deallocate %p which was not allocated with this allocator callback.\n", ptr);
			}
			else
			{
				mMemoryAllocated -= i->second.mSize;
				mAllocations.erase(i);
			}
		}
#ifdef _MSC_VER
		_aligned_free(ptr);
#else 
		free(ptr);
#endif
	}

	void StartTrackingLeaks()
	{
		std::lock_guard<std::mutex> lock(mAllocationsMapLock);
		mAllocations.clear();
		mEnableLeakDetection = true;
		mMemoryAllocated = 0;
		mPeakMemory = 0;
	}

	void StopTrackingLeaksAndReport()
	{
		std::lock_guard<std::mutex> lock(mAllocationsMapLock);
		mEnableLeakDetection = false;

		size_t totalBytes = 0;
		std::stringstream message;
		message << "Memory leaks detected:\n";
		for(auto it = mAllocations.begin(); it != mAllocations.end(); ++it)
		{
			const Allocation& alloc = it->second;
			message << "* Allocated ptr " << it->first << " of " << alloc.mSize << "bytes (type=" << alloc.mTypeName << ") at " << alloc.mFileName << ":" << alloc.mLine << "\n";
			totalBytes += alloc.mSize;
		}
		if(mAllocations.size()>0)
		{
			message << "=====Total of " << totalBytes << " bytes in " << mAllocations.size() << " allocations leaked=====";
			const std::string& tmp = message.str();
			printf("%s\n", tmp.c_str());
		}
		printf("Peak memory usage = %zd (%.2fkb or %.2f mb)\n", mPeakMemory, mPeakMemory / 1024.0, mPeakMemory / 1024.0 / 1024.0);
		mAllocations.clear();
	}
private:
	bool mEnableLeakDetection;
	struct Allocation
	{
		Allocation() {}
		Allocation(size_t size, const char* typeName, const char* filename, int line)
			: mSize(size), mTypeName(typeName), mFileName(filename), mLine(line)
		{

		}
		size_t mSize;
		std::string mTypeName;
		std::string mFileName;
		int mLine;
	};
	std::map<void*, Allocation> mAllocations;
	std::mutex mAllocationsMapLock;
	size_t mMemoryAllocated;
	size_t mPeakMemory;
};

class PerfAllocator : public physx::PxAllocatorCallback
{
	public:
		virtual void* allocate(size_t size, const char* typeName, const char* filename, int line)
		{
		PX_UNUSED(typeName);
		PX_UNUSED(filename);
		PX_UNUSED(line);
		#ifdef _MSC_VER 
			void* ptr = _aligned_malloc(size, 16);
		#else 
			void* ptr;
			if(posix_memalign(&ptr, 16, size)) ptr = 0;
		#endif
			return ptr;
		}
		virtual void deallocate(void* ptr)
		{
		#ifdef _MSC_VER 
			_aligned_free(ptr);
		#else 
			free(ptr);
		#endif
		}

		void StartTrackingLeaks()
		{
		}

		void StopTrackingLeaksAndReport()
		{
		}
	private:
};

#ifdef PERF_TEST
typedef TestAllocator Allocator;
#else
typedef TestAllocator Allocator;
#endif

class ExpectErrorMessage;
#if PERF_TEST && NDEBUG
class ErrorCallback : public physx::PxErrorCallback
{
	public:
		ErrorCallback(){}
		virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
		{
			PX_UNUSED(code);
			PX_UNUSED(message);
			PX_UNUSED(file);
			PX_UNUSED(line);
		}

	private:
		friend class ExpectErrorMessage;

		void PushExpectedMessage(ExpectErrorMessage* em){PX_UNUSED(em);}
		void PopExpectedMessage(ExpectErrorMessage* em){PX_UNUSED(em);}
};
#else
class ErrorCallback : public physx::PxErrorCallback
{
public:
	ErrorCallback() {}
	virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line);

private:
	friend class ExpectErrorMessage;

	void PushExpectedMessage(ExpectErrorMessage* em) { mExpectedErrorMessages.push_back(em); }
	void PopExpectedMessage(ExpectErrorMessage* em)
	{
		ASSERT_TRUE(mExpectedErrorMessages.size()>0) << "Internal unit test error. ExpectErrorMessage stack is not lifo";
		ASSERT_EQ(em, mExpectedErrorMessages.back()) << "Internal unit test error. ExpectErrorMessage stack is not lifo";
		mExpectedErrorMessages.pop_back();
	}

private:
	std::vector<ExpectErrorMessage*> mExpectedErrorMessages;
};
#endif


class TestAssertHandler : public physx::PxAssertHandler
{
	public:
		virtual void operator()(const char* exp, const char* file, int line, bool& ignore)
		{
			PX_UNUSED(ignore);
			ADD_FAILURE() << "NV_CLOTH_ASSERT(" << exp << ") from file:" << file << ":" << line << " Failed" << std::endl;
		}
};

class PerfAssertHandler : public physx::PxAssertHandler
{
	public:
		virtual void operator()(const char* exp, const char* file, int line, bool& ignore)
		{
			PX_UNUSED(ignore);
			ADD_FAILURE() << "NV_CLOTH_ASSERT(" << exp << ") from file:" << file << ":" << line << " Failed" << std::endl;
		}
};

#ifdef PERF_TEST
typedef PerfAssertHandler AssertHandler;
#else
typedef TestAssertHandler AssertHandler;
#endif


class NvClothEnvironment : public ::testing::Environment
{
		NvClothEnvironment(){}
		virtual ~NvClothEnvironment() {}
		static NvClothEnvironment* sEnv;
	public:
		static void AllocateEnv(){ sEnv = new NvClothEnvironment; }
		static void FreeEnv(){ delete sEnv; sEnv = nullptr; }
		static void ReportEnvFreed(){ sEnv = nullptr; } //google test will free it for us, so we just reset the value
		static NvClothEnvironment* GetEnv(){ return sEnv; }

		virtual void SetUp()
		{
			mAllocator = new Allocator;
			mFoundationAllocator = new Allocator;
			mErrorCallback = new ErrorCallback;
			mAssertHandler = new AssertHandler;
			nv::cloth::InitializeNvCloth(mAllocator, mErrorCallback, mAssertHandler, nullptr);
#if USE_CUDA
			cuInit(0);
#endif
		}
		virtual void TearDown()
		{
			delete mErrorCallback;
			delete mFoundationAllocator;
			delete mAllocator;
			delete mAssertHandler;
		}

		Allocator* GetAllocator(){ return mAllocator; }
		Allocator* GetFoundationAllocator(){ return mFoundationAllocator; }
		ErrorCallback* GetErrorCallback(){ return mErrorCallback; }
		AssertHandler* GetAssertHandler(){ return mAssertHandler; }

	private:
		Allocator* mAllocator;
		Allocator* mFoundationAllocator;
		ErrorCallback* mErrorCallback;
		AssertHandler* mAssertHandler;
};

#if USE_DX11
class DxContextManagerCallbackImpl : public nv::cloth::DxContextManagerCallback
{
public:
	DxContextManagerCallbackImpl(ID3D11Device* device, bool synchronizeResources = false);
	~DxContextManagerCallbackImpl();
	virtual void acquireContext() override;
	virtual void releaseContext() override;
	virtual ID3D11Device* getDevice() const override;
	virtual ID3D11DeviceContext* getContext() const override;
	virtual bool synchronizeResources() const override;

private:
	std::recursive_mutex mMutex;
	ID3D11Device* mDevice;
	ID3D11DeviceContext* mContext;
	bool mSynchronizeResources;
#ifdef _DEBUG
	uint32_t mLockCountTls;
#endif
};
#endif
