#pragma once
#define ASSERT_PTR(ptr) ASSERT_NE((decltype(ptr))0, ptr);
#define EXPECT_PTR(ptr) EXPECT_NE((decltype(ptr))0, ptr);
#define ASSERT_NULLPTR(ptr) ASSERT_EQ((decltype(ptr))0, ptr);
#define EXPECT_NULLPTR(ptr) EXPECT_EQ((decltype(ptr))0, ptr);
#include <gtest/gtest.h>
#include <foundation/PxErrorCallback.h>
#include <regex>
#include "utilities/CallbackImplementations.h"
#include <mutex>
#include <condition_variable>
#include <NvCloth/Range.h>
#include <NvCloth/Factory.h>
#include <PsFoundation.h>
#include <thread>
#include <queue>
#include <NvCloth/DxContextManagerCallback.h>
#include <functional>
#include <atomic>

//Helper function to create a range from an std::vector
template<typename T>
nv::cloth::Range<T> CreateRange(std::vector<T>& vector, int offset = 0)
{
	T* begin = &vector.front() + offset;
	T* end = &vector.front() + vector.size();

	return nv::cloth::Range<T>(begin, end);
}

template<typename T>
nv::cloth::Range<const T> CreateConstRange(const std::vector<T>& vector, int offset = 0)
{
	const T* begin = &vector.front() + offset;
	const T* end = &vector.front() + vector.size();

	return nv::cloth::Range<const T>(begin, end);
}

class ErrorCallback;
/**
 * Silences all errors/messages matched by regex needle for the scope of its life.
 * If the count of matched errors/messages is not equal to count at the end of the scope, the test will fail.
 * filter can be used to select different error levels taken in to account. (eNO_ERROR is always ignored)
 */
class ExpectErrorMessage
{
		//This object should be on the stack, as we depend on correct lifo order
		void* operator new (std::size_t size)throw(){ PX_UNUSED(size); return nullptr;} //can't use = delete in vc2012
	public:
		ExpectErrorMessage(const char* needle, int count, physx::PxErrorCode::Enum filter = physx::PxErrorCode::Enum::eMASK_ALL);

		~ExpectErrorMessage();

		bool TestMessage(physx::PxErrorCode::Enum code, const char* codeName, const char* message, const char* file, int line);
	private:
		std::regex mNeedle;
		std::string mNeedlePattern;
		int mCount;
		physx::PxErrorCode::Enum mFilter;

		int mMatchCount; //this will increase every time a TestMessage succeeds

		ErrorCallback* mErrorCallback;
		bool mRestoreExpectingMessage;
		physx::PxErrorCode::Enum mRestoreExpectingMessageFilter;
};

//Derive from TestLeakDetector if want your test fixture to use leak detection.
//Make sure the TestLeakDetector SetUp and TearDown functions in the derived class.
class TestLeakDetector : public ::testing::Test
{
	public:
		virtual void SetUp() override
		{
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetAllocator())->StartTrackingLeaks();
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetFoundationAllocator())->StartTrackingLeaks();
		}
		virtual void TearDown() override
		{
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetAllocator())->StopTrackingLeaksAndReport();
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetFoundationAllocator())->StopTrackingLeaksAndReport();
		}
};

//Same as TestLeakDetector but with parameterized test support
template <typename T>
class TestLeakDetectorWithParam : public ::testing::TestWithParam<T>
{
	public:
		virtual void SetUp() override
		{
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetAllocator())->StartTrackingLeaks();
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetFoundationAllocator())->StartTrackingLeaks();
		}
		virtual void TearDown() override
		{
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetAllocator())->StopTrackingLeaksAndReport();
			static_cast<Allocator*>(NvClothEnvironment::GetEnv()->GetFoundationAllocator())->StopTrackingLeaksAndReport();
		}
};



//Use this to create a named empty test fixture that enables leak detection. 
#define TEST_LEAK_FIXTURE(group) class group : public TestLeakDetector {};
//Now you must use TEST_F(group,name) instead of TEST(group,name) for tests

#define TEST_LEAK_FIXTURE_WITH_PARAM(group,paramType) class group : public TestLeakDetectorWithParam<paramType> {};
//Now you must use TEST_P(group,name) instead of TEST(group,name) for tests

#if USE_CUDA
struct ScopedCudaContext
{
	ScopedCudaContext(CUcontext* context)
	{
		int deviceCount = 0;
		mSuccess = true;
		mSuccess = (cuDeviceGetCount(&deviceCount) == CUDA_SUCCESS);
		mSuccess &= deviceCount >= 1;
		if(mSuccess)
			mSuccess  = cuCtxCreate(context, 0, 0) == CUDA_SUCCESS;
		mContext = context;
	}
	~ScopedCudaContext()
	{
		if(mSuccess)
			cuCtxDestroy(*mContext);
	}

	CUcontext* mContext;
	bool mSuccess;
};

//Automatically initializes context with a cuda context and destroys it at the end of this scope
#define INIT_CUDA_SCOPED(context) ScopedCudaContext scopedCudaContext##__LINE__(context); if(!scopedCudaContext##__LINE__##.mSuccess) {printf("This test only works on machines with a cuda enabled device.\n"); return;}
#endif

inline const char* GetPlatformName(nv::cloth::Platform platform)
{
	switch(platform)
	{
		case nv::cloth::Platform::CPU:
			return "CPU";
		case nv::cloth::Platform::DX11:
			return "DX11";
		case nv::cloth::Platform::CUDA:
			return "CUDA";
		default:
			return "UNKNOWN";
	}
}

//Helper managing foundation
struct ScopedFoundation
{
	ScopedFoundation()
	{
		mSuccess = nullptr!=physx::shdfnd::Foundation::createInstance(1 << 24, *NvClothEnvironment::GetEnv()->GetErrorCallback(), *NvClothEnvironment::GetEnv()->GetFoundationAllocator());
	}
	~ScopedFoundation()
	{
		if(mSuccess)
			physx::shdfnd::Foundation::getInstance().release();
	}

	bool mSuccess;
};

//Automatically initializes context with a cuda context and destroys it at the end of this scope
#define INIT_FOUNDATION_SCOPED() ScopedFoundation scopedFoundation; ASSERT_TRUE(scopedFoundation.mSuccess) << "Error initializing foundation";


//Helper functions setting up cuda/dx factories
class FactoryHelper //Base class
{
public:
	virtual ~FactoryHelper() {}
	virtual nv::cloth::Factory* CreateFactory() = 0;
	static FactoryHelper* CreateFactoryHelper(nv::cloth::Platform platform);
	virtual void FlushDevice() {}
};

class FactoryHelperCPU : public FactoryHelper
{
public:
	virtual nv::cloth::Factory* CreateFactory() override;
};

#if USE_CUDA
class FactoryHelperCUDA : public FactoryHelper
{
public:
	FactoryHelperCUDA();
	~FactoryHelperCUDA();
	virtual nv::cloth::Factory* CreateFactory() override;
	virtual void FlushDevice() override { cuEventQuery(0); }
private:
	bool mCUDAInitialized;
	CUcontext mCUDAContext;
};
#endif

struct ScopedFactoryHelper
{
	ScopedFactoryHelper(nv::cloth::Platform platform)
	{
		mFactoryHelper = FactoryHelper::CreateFactoryHelper(platform);
	}
	~ScopedFactoryHelper()
	{
		delete mFactoryHelper;
	}
	nv::cloth::Factory* CreateFactory(){ return mFactoryHelper->CreateFactory(); }
	FactoryHelper* mFactoryHelper;
};

struct ID3D11Device;
struct ID3D11DeviceContext;
class FactoryHelperDX : public FactoryHelper
{
public:
	FactoryHelperDX();
	~FactoryHelperDX();
	virtual nv::cloth::Factory* CreateFactory() override;
	virtual void FlushDevice() override;

private:
	bool mDXInitialized;
	ID3D11Device* mDXDevice;
	ID3D11DeviceContext* mDXDeviceContext;
	nv::cloth::DxContextManagerCallback* mGraphicsContextManager;

};

class JobManager;
class Job
{
public:
	Job() = default;
	Job(const Job&);
	Job& operator=(const Job&);
	virtual ~Job() {}
	void Initialize(JobManager* parent, std::function<void(Job*)> function = std::function<void(Job*)>(), int refcount = 1);
	void Reset(int refcount = 1); //Call this before reusing a job that doesn't need to be reinitialized
	void Execute();
	void AddReference();
	void RemoveReference();
	void Wait(); //Block until job is finished
private:
	virtual void ExecuteInternal() {}

	std::function<void(Job*)> mFunction;
	JobManager* mParent;
	std::atomic_int mRefCount;

	bool mFinished;
	std::mutex mFinishedLock;
	std::condition_variable mFinishedEvent;
};

class JobManager
{
public:
	JobManager()
	{
		mWorkerCount = 8;
		mWorkerThreads = new std::thread[mWorkerCount];
		mQuit = false;

		for(int i = 0; i < mWorkerCount; i++)
			mWorkerThreads[i] = std::thread(JobManager::WorkerEntryPoint, this);
	}
	~JobManager()
	{
		std::unique_lock<std::mutex> lock(mJobQueueLock);
		mQuit = true;
		lock.unlock();
		mJobQueueEvent.notify_all();
		for(int i = 0; i < mWorkerCount; i++)
		{
			mWorkerThreads[i].join();
		}
		delete[] mWorkerThreads;
	}

	template <int count, typename F>
	void ParallelLoop(F const& function)
	{
		/*for(int i = 0; i < count; i++)
			function(i);*/
		Job finalJob;
		finalJob.Initialize(this, std::function<void(Job*)>(), count);
		Job jobs[count];
		for(int j = 0; j < count; j++)
		{
			jobs[j].Initialize(this, [j, &finalJob, function](Job*) {function(j); finalJob.RemoveReference(); });
			jobs[j].RemoveReference();
		}
		finalJob.Wait();
	}

	static void WorkerEntryPoint(JobManager* parrent);
private:
	friend class Job;
	void Submit(Job* job);

	int mWorkerCount;
	std::thread* mWorkerThreads;

	std::mutex mJobQueueLock;
	std::queue<Job*> mJobQueue;
	std::condition_variable mJobQueueEvent;
	bool mQuit;
};

class MultithreadedSolverHelper
{
public:
	void Initialize(nv::cloth::Solver* solver, JobManager* jobManager);
	void StartSimulation(float dt);
	void WaitForSimulation();
private:
	Job mStartSimulationJob;
	Job mEndSimulationJob;
	std::vector<Job> mSimulationChunkJobs;

	float mDt;

	nv::cloth::Solver* mSolver;
	JobManager* mJobManager;
};

struct PlatformTestParameter
{
	PlatformTestParameter(nv::cloth::Platform platform)
	{
		mPlatform = platform;
	}
	testing::Message& GetShortName(testing::Message& msg) const
	{
		return msg<<GetPlatformName(mPlatform);
	}
	friend std::ostream& operator<<(std::ostream& os, const PlatformTestParameter& obj)
	{
		os << "Platform: "<<GetPlatformName(obj.mPlatform);
		return os;
	}

	nv::cloth::Platform mPlatform;
};

#if USE_CUDA
#define DX11_TEST_CASE_P ,PlatformTestParameter(nv::cloth::Platform::DX11)
#else
#define DX11_TEST_CASE_P 
#endif

#if USE_CUDA
#define CUDA_TEST_CASE_P ,PlatformTestParameter(nv::cloth::Platform::CUDA)
#else
#define CUDA_TEST_CASE_P 
#endif

#define INSTANTIATE_PLATFORM_TEST_CASE_P(Test) INSTANTIATE_TEST_CASE_P(Platforms,Test,::testing::Values(PlatformTestParameter(nv::cloth::Platform::CPU) DX11_TEST_CASE_P CUDA_TEST_CASE_P));
