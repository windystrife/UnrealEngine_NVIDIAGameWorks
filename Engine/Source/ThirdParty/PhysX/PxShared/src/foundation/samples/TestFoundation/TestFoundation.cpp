// Include all public foundation header files

#include "foundation/PxCTypes.h"
#include "foundation/PxFoundationInterface.h"

#include "PsGlobals.h"
#include "PsVersionNumber.h"
// Now include shared foundation header files
#include "Ps.h"
#include "PsAlignedMalloc.h"
#include "PsAlloca.h"
#include "PsAllocator.h"
#include "PsArray.h"
#include "PsAtomic.h"
#include "PsBasicTemplates.h"
#include "PsBitUtils.h"
#include "PsCpu.h"
#include "PsHash.h"
#include "PsHashInternals.h"
#include "PsHashMap.h"
#include "PsHashSet.h"
#include "PsInlineAllocator.h"
#include "PsInlineArray.h"
#include "PsIntrinsics.h"
#include "PsMutex.h"
#include "PsPool.h"
#include "PsSList.h"
#include "PsSort.h"
#include "PsSortInternals.h"
#include "PsString.h"
#include "PsSync.h"
#include "PsTempAllocator.h"
#include "PsThread.h"
#include "PsTime.h"
#include "PsUserAllocated.h"

#include <stdio.h>

#if defined(WIN32)
// on win32 we only have 8-byte alignment guaranteed, but the CRT provides special aligned allocation
// fns
#include <malloc.h>
#include <crtdbg.h>

static void* platformAlignedAlloc(size_t size)
{
	return _aligned_malloc(size, 16);
}

static void platformAlignedFree(void* ptr)
{
	_aligned_free(ptr);
}
#elif defined(PX_LINUX) || defined(PX_ANDROID)
static void* platformAlignedAlloc(size_t size)
{
	return ::memalign(16, size);
}

static void platformAlignedFree(void* ptr)
{
	::free(ptr);
}

#else

// on Win64 we get 16-byte alignment by default
static void* platformAlignedAlloc(size_t size)
{
	void* ptr = ::malloc(size);
	PX_ASSERT((reinterpret_cast<size_t>(ptr) & 15) == 0);
	return ptr;
}

static void platformAlignedFree(void* ptr)
{
	::free(ptr);
}
#endif

class DefaultErrorCallback : public physx::PxErrorCallback
{
  public:
	DefaultErrorCallback(void)
	{
	}

	virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
	{
		PX_UNUSED(code);
		printf("PhysX: %s : %s : %d\r\n", message, file, line);
	}

  private:
};

class DefaultAllocator : public physx::PxAllocatorCallback
{
  public:
	DefaultAllocator(void)
	{
	}

	~DefaultAllocator(void)
	{
	}

	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line)
	{
		PX_UNUSED(typeName);
		PX_UNUSED(filename);
		PX_UNUSED(line);
		void* ret = platformAlignedAlloc(size);
		return ret;
	}

	virtual void deallocate(void* ptr)
	{
		platformAlignedFree(ptr);
	}

  private:
};

DefaultAllocator gDefaultAllocator;
DefaultErrorCallback gDefaultErrorCallback;

int main(int argc, const char** argv)
{
	PX_UNUSED(argc);
	PX_UNUSED(argv);

	physx::shdfnd::initializeSharedFoundation(PX_FOUNDATION_VERSION, gDefaultAllocator, gDefaultErrorCallback);

	{
		physx::shdfnd::Array<int> alist;
		alist.pushBack(1);
		alist.pushBack(2);
		alist.pushBack(3);
		for(uint32_t i = 0; i < alist.size(); i++)
		{
			printf("%d\r\n", alist[i]);
		}
	}

	physx::shdfnd::terminateSharedFoundation();

	return 0;
}
