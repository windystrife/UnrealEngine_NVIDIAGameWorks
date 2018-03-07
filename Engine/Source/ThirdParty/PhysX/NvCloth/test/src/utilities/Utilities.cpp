#include "Utilities.h"
#include <NvCloth/Factory.h>
#define NOMINMAX
#ifdef _MSC_VER
#pragma warning(disable : 4668 4917 4365 4061 4005)
#if NV_XBOXONE
#include <d3d11_x.h>
#else
#include <d3d11.h>
#endif
#endif
#include <gtest/gtest.h>
#include <NvCloth/Solver.h>

ExpectErrorMessage::ExpectErrorMessage(const char* needle, int count, physx::PxErrorCode::Enum filter)
{
	mNeedle = std::regex(needle);
	mNeedlePattern = needle;
	mCount = count;
	mFilter = filter;

	mMatchCount = 0;

	mErrorCallback = static_cast<ErrorCallback*>(NvClothEnvironment::GetEnv()->GetErrorCallback());
	mErrorCallback->PushExpectedMessage(this);
}

ExpectErrorMessage::~ExpectErrorMessage()
{
	EXPECT_EQ(mMatchCount, mCount) << "Expected " << mCount << " messages with pattern  " << mNeedlePattern << "  but found " << mMatchCount;

	mErrorCallback->PopExpectedMessage(this);
}

bool ExpectErrorMessage::TestMessage(physx::PxErrorCode::Enum code, const char* codeName, const char* message, const char* file, int line)
{
	if((code&mFilter) == 0)
		return false;

	//Compose string out of message info to test it against our regex
	std::stringstream ss;
	ss << codeName << "\t" << file << ":" << line << "\t" << message << "\n";

	const std::string s = ss.str();
	std::smatch m;
	if(std::regex_search(s, m, mNeedle))
	{
		mMatchCount++;
		return true;
	}
	return false;
}


FactoryHelper* FactoryHelper::CreateFactoryHelper(nv::cloth::Platform platform)
{
	switch(platform)
	{
		case nv::cloth::Platform::CPU:
			return new FactoryHelperCPU;
#if USE_DX11
		case nv::cloth::Platform::DX11:
			return new FactoryHelperDX;
#endif
#if USE_CUDA
		case nv::cloth::Platform::CUDA:
			return new FactoryHelperCUDA;
#endif
		default:
			return nullptr;
	}
}

nv::cloth::Factory* FactoryHelperCPU::CreateFactory()
{
	return NvClothCreateFactoryCPU();
}
#if USE_CUDA
FactoryHelperCUDA::FactoryHelperCUDA()
{
	int deviceCount = 0;
	mCUDAInitialized = true;
	mCUDAInitialized = (cuDeviceGetCount(&deviceCount) == CUDA_SUCCESS);
	mCUDAInitialized &= deviceCount >= 1;
	if(!mCUDAInitialized)
		return;
	mCUDAInitialized = cuCtxCreate(&mCUDAContext, 0, 0) == CUDA_SUCCESS;
	if(!mCUDAInitialized)
		return;
}

FactoryHelperCUDA::~FactoryHelperCUDA()
{
	if(mCUDAInitialized)
		cuCtxDestroy(mCUDAContext);
}

nv::cloth::Factory* FactoryHelperCUDA::CreateFactory()
{
	if(mCUDAInitialized)
		return NvClothCreateFactoryCUDA(mCUDAContext);
	else
		return nullptr;
}
#endif

#if USE_DX11
FactoryHelperDX::FactoryHelperDX()
{
	D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL featureLevelResult;
	HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 1, D3D11_SDK_VERSION, &mDXDevice, &featureLevelResult, &mDXDeviceContext);
	EXPECT_EQ(result, S_OK);
	EXPECT_EQ(featureLevelResult, D3D_FEATURE_LEVEL_11_0);
	mDXInitialized = result == S_OK && featureLevelResult == D3D_FEATURE_LEVEL_11_0;
	if(!mDXInitialized)
		return;
	mGraphicsContextManager = new DxContextManagerCallbackImpl(mDXDevice);
	EXPECT_PTR(mGraphicsContextManager);
	mDXInitialized &= mGraphicsContextManager != nullptr;
	if(!mDXInitialized)
		return;
}

FactoryHelperDX::~FactoryHelperDX()
{
	if(mDXInitialized)
	{
		mDXDeviceContext->Release();
		mDXDevice->Release();
	}
	if(mGraphicsContextManager)
	{
		delete mGraphicsContextManager;
	}
}
nv::cloth::Factory* FactoryHelperDX::CreateFactory()
{
	if(mDXInitialized)
		return NvClothCreateFactoryDX11(mGraphicsContextManager);
	else
		return nullptr;
}

void FactoryHelperDX::FlushDevice()
{
	mDXDeviceContext->Flush();
}
#endif

void Job::Initialize(JobManager* parent, std::function<void(Job*)> function, int refcount)
{
	mFunction = function;
	mParent = parent;
	Reset(refcount);
}

Job::Job(const Job& job)
{
	operator=(job);
}
Job& Job::operator=(const Job& job)
{
	mFunction = job.mFunction;
	mParent = job.mParent;
	mRefCount.store(job.mRefCount);
	mFinished = job.mFinished;
	return *this;
}

void Job::Reset(int refcount)
{
	mRefCount = refcount;
	mFinished = false;
}

void Job::Execute()
{
	if(mFunction)
		mFunction(this);
	else
		ExecuteInternal();

	mFinishedLock.lock();
	mFinished = true;
	mFinishedLock.unlock();
	mFinishedEvent.notify_one();
}

void Job::AddReference()
{
	mRefCount++;
}
void Job::RemoveReference()
{
	if(0 == --mRefCount)
	{
		mParent->Submit(this);
	}
}

void Job::Wait()
{
	std::unique_lock<std::mutex> lock(mFinishedLock);
	mFinishedEvent.wait(lock, [this](){return mFinished;} );
	return;
}

void JobManager::WorkerEntryPoint(JobManager* parrent)
{
	while(true)
	{
		Job* job;
		{
			std::unique_lock<std::mutex> lock(parrent->mJobQueueLock);
			while(parrent->mJobQueue.size() == 0 && !parrent->mQuit)
			{
				parrent->mJobQueueEvent.wait(lock);
			}
			if(parrent->mQuit)
				return;

			job = parrent->mJobQueue.front();
			parrent->mJobQueue.pop();
		}
		job->Execute();
	}
}

void JobManager::Submit(Job* job)
{
	mJobQueueLock.lock();
	mJobQueue.push(job);
	mJobQueueLock.unlock();
	mJobQueueEvent.notify_one();
}

void MultithreadedSolverHelper::Initialize(nv::cloth::Solver* solver, JobManager* jobManager)
{
	mSolver = solver;
	mJobManager = jobManager;
	mEndSimulationJob.Initialize(mJobManager, [this](Job*) {
		mSolver->endSimulation();
	});

	mStartSimulationJob.Initialize(mJobManager, [this](Job*) {
		mSolver->beginSimulation(mDt);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSimulationChunkJobs[j].RemoveReference();
	});
}

void MultithreadedSolverHelper::StartSimulation(float dt)
{
	mDt = dt;

	if(mSolver->getSimulationChunkCount() != mSimulationChunkJobs.size())
	{
		mSimulationChunkJobs.resize(mSolver->getSimulationChunkCount(), Job());
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSimulationChunkJobs[j].Initialize(mJobManager, [this, j](Job*) {mSolver->simulateChunk(j); mEndSimulationJob.RemoveReference(); });
	}
	else
	{
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSimulationChunkJobs[j].Reset();
	}
	mStartSimulationJob.Reset();
	mEndSimulationJob.Reset(mSolver->getSimulationChunkCount());
	mStartSimulationJob.RemoveReference();
	
}

void MultithreadedSolverHelper::WaitForSimulation()
{
	mEndSimulationJob.Wait();
}
