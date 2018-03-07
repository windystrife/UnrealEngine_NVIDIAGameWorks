// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	LinuxPlatformThreading.h: Linux platform threading functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"

class Error;

/**
* Linux implementation of the Process OS functions
**/
class FRunnableThreadLinux : public FRunnableThreadPThread
{
	struct EConstants
	{
		enum
		{
			LinuxThreadNameLimit = 15,			// the limit for thread name is just 15 chars :( http://man7.org/linux/man-pages/man3/pthread_setname_np.3.html
			CrashHandlerStackSize = SIGSTKSZ + 192 * 1024	// should be at least SIGSTKSIZE, plus 192K because we do logging and symbolication in crash handler
		};
	};

	/** Each thread needs a separate stack for the signal handler, so possible stack overflows in the thread are handled */
	char ThreadCrashHandlingStack[EConstants::CrashHandlerStackSize];

	/** Address of stack guard page - if nullptr, the page wasn't set */
	void* StackGuardPageAddress;

public:

	/** Separate stack for the signal handler (so possible stack overflows don't go unnoticed), for the main thread specifically. */
	static char MainThreadSignalHandlerStack[EConstants::CrashHandlerStackSize];

	FRunnableThreadLinux()
		:	FRunnableThreadPThread()
		,	StackGuardPageAddress(nullptr)
	{
	}

	~FRunnableThreadLinux()
	{
		// Call the parent destructor body before the parent does it - see comment on that function for explanation why.
		FRunnableThreadPThread_DestructorBody();
	}

	/**
	 * Sets up an alt stack for signal (including crash) handling on this thread.
	 *
	 * This includes guard page at the end of the stack to make running out of stack more obvious.
	 * Should be run in the context of the thread.
	 *
	 * @param StackBuffer pointer to the beginning of the stack buffer (note: on x86_64 will be the bottom of the stack, not its beginning)
	 * @param StackSize size of the stack buffer
	 * @param OutStackGuardPageAddress pointer to the variable that will receive the address of the guard page. Can be null. Will not be set if guard page wasn't successfully set.
	 *
	 * @return true if setting the alt stack succeeded. Inability to set guard page will not affect success of the operation.
	 */
	static bool SetupSignalHandlerStack(void* StackBuffer, const size_t StackBufferSize, void** OutStackGuardPageAddress)
	{
		// find an address close to begin of the stack and protect it
		uint64 StackGuardPage = reinterpret_cast<uint64>(StackBuffer);

		// align by page
		const uint64 PageSize = sysconf(_SC_PAGESIZE);
		const uint64 Remainder = StackGuardPage % PageSize;
		if (Remainder != 0)
		{
			StackGuardPage += (PageSize - Remainder);
			checkf(StackGuardPage % PageSize == 0, TEXT("StackGuardPage is not aligned on page size"));
		}

		checkf(StackGuardPage + PageSize - reinterpret_cast<uint64>(StackBuffer) < StackBufferSize,
			TEXT("Stack size is too small for the extra guard page!"));

		void* StackGuardPageAddr = reinterpret_cast<void*>(StackGuardPage);
		if (FPlatformMemory::PageProtect(StackGuardPageAddr, PageSize, true, false))
		{
			if (OutStackGuardPageAddress)
			{
				*OutStackGuardPageAddress = StackGuardPageAddr;
			}
		}
		else
		{
			// cannot use UE_LOG - can run into deadlocks in output device code
			fprintf(stderr, "Unable to set a guard page on the alt stack\n");
		}

		// set up the buffer to be used as stack
		stack_t SignalHandlerStack;
		FMemory::Memzero(SignalHandlerStack);
		SignalHandlerStack.ss_sp = StackBuffer;
		SignalHandlerStack.ss_size = StackBufferSize;

		bool bSuccess = (sigaltstack(&SignalHandlerStack, nullptr) == 0);
		if (!bSuccess)
		{
			int ErrNo = errno;
			// cannot use UE_LOG - can run into deadlocks in output device code
			fprintf(stderr, "Unable to set alternate stack for crash handler, sigaltstack() failed with errno=%d (%s)\n", ErrNo, strerror(ErrNo));
		}

		return bSuccess;
	}

private:

	/**
	 * Allows a platform subclass to setup anything needed on the thread before running the Run function
	 */
	virtual void PreRun() override
	{
		FString SizeLimitedThreadName = ThreadName;

		if (SizeLimitedThreadName.Len() > EConstants::LinuxThreadNameLimit)
		{
			// first, attempt to cut out common and meaningless substrings
			SizeLimitedThreadName = SizeLimitedThreadName.Replace(TEXT("Thread"), TEXT(""));
			SizeLimitedThreadName = SizeLimitedThreadName.Replace(TEXT("Runnable"), TEXT(""));

			// if still larger
			if (SizeLimitedThreadName.Len() > EConstants::LinuxThreadNameLimit)
			{
				FString Temp = SizeLimitedThreadName;

				// cut out the middle and replace with a substitute
				const TCHAR Dash[] = TEXT("-");
				const int32 DashLen = ARRAY_COUNT(Dash) - 1;
				int NumToLeave = (EConstants::LinuxThreadNameLimit - DashLen) / 2;

				SizeLimitedThreadName = Temp.Left(EConstants::LinuxThreadNameLimit - (NumToLeave + DashLen));
				SizeLimitedThreadName += Dash;
				SizeLimitedThreadName += Temp.Right(NumToLeave);

				check(SizeLimitedThreadName.Len() <= EConstants::LinuxThreadNameLimit);
			}
		}

		int ErrCode = pthread_setname_np(Thread, TCHAR_TO_ANSI(*SizeLimitedThreadName));
		if (ErrCode != 0)
		{
			UE_LOG(LogHAL, Warning, TEXT("pthread_setname_np(, '%s') failed with error %d (%s)."), *ThreadName, ErrCode, ANSI_TO_TCHAR(strerror(ErrCode)));
		}

		// set the alternate stack for handling crashes due to stack overflow
		SetupSignalHandlerStack(ThreadCrashHandlingStack, sizeof(ThreadCrashHandlingStack), &StackGuardPageAddress);
	}

	virtual void PostRun() override
	{
		if (StackGuardPageAddress != nullptr)
		{
			// we protected one page only
			const uint64 PageSize = sysconf(_SC_PAGESIZE);

			if (!FPlatformMemory::PageProtect(StackGuardPageAddress, PageSize, true, true))
			{
				UE_LOG(LogLinux, Error, TEXT("Unable to remove a guard page from the alt stack"));
			}

			StackGuardPageAddress = nullptr;
		}
	}

	/**
	 * Allows platforms to adjust stack size
	 */
	virtual uint32 AdjustStackSize(uint32 InStackSize)
	{
		InStackSize = FRunnableThreadPThread::AdjustStackSize(InStackSize);

		// If it's set, make sure it's at least 128 KB or stack allocations (e.g. in Logf) may fail
		if (InStackSize && InStackSize < 128 * 1024)
		{
			InStackSize = 128 * 1024;
		}

		return InStackSize;
	}
};
