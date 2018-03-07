// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/CircularQueue.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioEntityManager.h"

#if ENABLE_UNREAL_AUDIO

// Useful debug check macros

#if ENABLE_UNREAL_AUDIO_EXTRA_DEBUG_CHECKS
#define DEBUG_AUDIO_CHECK_AUDIO_THREAD(AUDIO_MODULE)	(AUDIO_MODULE->CheckAudioThread())
#define DEBUG_AUDIO_CHECK_MAIN_THREAD(AUDIO_MODULE)		(AUDIO_MODULE->CheckMainThread())
#define DEBUG_AUDIO_CHECK(EXP)							check(EXP)
#define DEBUG_AUDIO_CHECK_MSG(EXP, MSG)					checkf(EXP, MSG)
#else // ENABLE_UNREAL_AUDIO_EXTRA_DEBUG_CHECKS
#define DEBUG_AUDIO_CHECK_AUDIO_THREAD(AUDIO_MODULE)
#define DEBUG_AUDIO_CHECK_MAIN_THREAD(AUDIO_MODULE)
#define DEBUG_AUDIO_CHECK(EXP)
#define DEBUG_AUDIO_CHECK_MSG(EXP, MSG)
#endif // #else // ENABLE_UNREAL_AUDIO_EXTRA_DEBUG_CHECKS

namespace UAudio
{
	/**
	* TSafeQueue
	* A simple (minimal lock) thread-safe multi-producer/multi-consumer FIFO queue
	* Based on H. Sutter's queue described in:
	* http://www.drdobbs.com/parallel/writing-a-generalized-concurrent-queue/211601363
	*/
	template<class ElementType>
	class TSafeQueue
	{
	public:
		TSafeQueue()
		{
			First = new FNode(nullptr);
			Last = First;
			ProducerLock = 0;
			ConsumerLock = 0;
		}

		~TSafeQueue()
		{
			while (First != nullptr)
			{
				FNode* Temp = First;
				First = Temp->Next;
				delete Temp->Element;
				delete Temp;
			}
		}

		void Enqueue(const ElementType& Element)
		{
			FNode* Node = new FNode(new ElementType(Element));

			// Allow multiple threads to enqueue, but only let one do it at a time
			while (FPlatformAtomics::InterlockedExchange(&ProducerLock, 1))
			{
				// spin while acquiring exclusivity. This will only spin if 
				// a different thread has already set the ProducerLock to 1
			}

			FPlatformAtomics::InterlockedExchangePtr((void**)&Last->Next, Node);
			FPlatformAtomics::InterlockedExchangePtr((void**)&Last, Node);

			// Free the lock
			ProducerLock = 0;
		}

		bool Dequeue(ElementType& OutElement)
		{
			while (FPlatformAtomics::InterlockedExchange(&ConsumerLock, 1))
			{
				// spin while acquiring exclusivity. This will only spin if 
				// a different thread has already set the ConsumerLock to 1
			}

			FNode* Head = First;
			FNode* Next = First->Next;
			if (Next != nullptr)
			{
				ElementType* Element = Next->Element;
				Next->Element = nullptr;
				First = Next;
				ConsumerLock = 0;
				OutElement = *Element;
				delete Element;
				delete Head;
				return true;
			}

			ConsumerLock = 0;
			return false;
		}

	private:

		// Align the size of the FNode struct to ensure that two 
		// FNode objects won't occupy the same cache line (to avoid "cache-line ping-ponging")
		struct MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FNode
		{
			FNode(ElementType* InElement)
				: Element(InElement)
				, Next(nullptr)
			{}

			ElementType* Element;
			FNode* Next;
		};

		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FNode*			First;
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) volatile int32	ConsumerLock;
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FNode*			Last;
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) volatile int32	ProducerLock;
	};

	/**
	* TLocklessQueue
	* A thread-safe lockless single-producer/single-consumer FIFO queue
	* Based on H. Sutter's queue described in:
	* http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448?pgno=3
	*/
	template<class ElementType>
	class TLocklessQueue
	{
	public:
		TLocklessQueue()
		{
			First = new FNode(nullptr);
			Last = First;
			Divider = First;
		}

		~TLocklessQueue()
		{
			while (First != nullptr)
			{
				FNode* Temp = First;
				First = Temp->NextNode;
				delete Temp;
			}
		}

		void Enqueue(const ElementType& Element)
		{
			FNode* Node = new FNode(Element);
			Last->Next = Node;

			// Publish the new node to the last node, now the consumer thread can read the node
			FPlatformAtomics::InterlockedExchangePtr((void**)&Last, Last->Next);

			// Trim unused nodes lazily
			while (First != Divider)
			{
				FNode* Temp = First;
				First = First->Next;
				delete Temp;
			}
		}

		bool Dequeue(ElementType& OutElement)
		{
			if (Divider != Last)
			{
				OutElement = Divider->Next->Element;
				// Publish that we took this node
				FPlatformAtomics::InterlockedExchangePtr((void**)&Divider, Divider->Next);
				return true;
			}
			return false;
		}

	private:
		struct FNode
		{
			FNode(const ElementType& InElement)
			: Element(InElement)
			, Next(nullptr)
			{}

			ElementType Element;
			FNode* Next;
		};

		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FNode* First;		// For producer
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FNode* Last;		// Shared
		MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FNode* Divider;	// Shared
	};

	class FSafeFloat
	{
	public:
		FSafeFloat(float InitValue);
		void Set(float Value);
		float Get() const;

	private:
		FThreadSafeCounter SafeValue;
	};

	/**
	* ECommandData
	* Enumerated list of types used in audio commands.
	*/
	namespace ECommandData
	{
		enum Type
		{
			INVALID,
			POINTER,
			HANDLE,
			FLOAT_32,
			FLOAT_64,
			BOOL,
			UINT8,
			UINT32,
			UINT64,
			INT32,
			INT64
		};
	}

	/** 
	* FCommandData
	* Simple type union for use as arguments in commands.
	*/
	struct FCommandData
	{
		FCommandData();
		FCommandData(void* Val);
		FCommandData(const FEntityHandle& EntityHandle);
		FCommandData(float Val);
		FCommandData(double Val);
		FCommandData(uint8 Val);
		FCommandData(uint32 Val);
		FCommandData(uint64 Val);
		FCommandData(bool Val);
		FCommandData(int32 Val);
		FCommandData(int64 Val);

		ECommandData::Type DataType;

		union
		{
			void* PtrVal;
			float Float32Val;
			double Float64Val;
			bool BoolVal;
			uint8 UnsignedInt8;
			uint32 UnsignedInt32;
			uint64 UnsignedInt64;
			int32 Int32Val;
			int64 Int64Val;
//			struct
//			{
				FEntityHandle Handle;
//			};
		} Data;
	};

	/**
	* TCommand
	* A struct used to send commands between threads for audio events.
	*/
	struct FCommand
	{
		uint32 Id;
		FCommandData Arguments[5];
		int32 NumArguments;

		FCommand();
		FCommand(uint32 InCommand);
		FCommand(uint32 InCommand, const FCommandData& Arg0);
		FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1);
		FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1, const FCommandData& Arg2);
		FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1, const FCommandData& Arg2, const FCommandData& Arg3);
		FCommand(uint32 InCommand, const FCommandData& Arg0, const FCommandData& Arg1, const FCommandData& Arg2, const FCommandData& Arg3, const FCommandData& Arg4);
	};

	template <class CommandType>
	class TCommandQueue
	{
	public:
		TCommandQueue(uint32 CircularQueueSize)
			: CircularCommandQueue(CircularQueueSize)
		{}

		void Enqueue(const CommandType& InCommand)
		{
			if (!CircularCommandQueue.Enqueue(InCommand))
			{
				// if circular queue is full, then push to fallback queue (which performs allocations)
				SafeCommandQueue.Enqueue(InCommand);
			}
		}

		bool Dequeue(CommandType& OutCommand)
		{
			if (CircularCommandQueue.Dequeue(OutCommand))
			{
				// Empty the fallback command queue and push to circular
				CommandType TempCommand;
				if (SafeCommandQueue.Dequeue(TempCommand))
				{
					CircularCommandQueue.Enqueue(TempCommand);
				}
				return true;
			}
			return false;
		}

	private:
		TCircularQueue<CommandType> CircularCommandQueue;
		TSafeQueue<CommandType> SafeCommandQueue;
	};

	struct FDynamicParam
	{
		float StartValue;
		float EndValue;
		float CurrentValue;
		float StartTime;
		float DeltaTime;
		bool bIsDone;
	};

	struct FDynamicParamData
	{
		TArray<float> StartValue;
		TArray<float> EndValue;
		TArray<float> CurrentValue;
		TArray<double> StartTime;
		TArray<double> DeltaTime;
		TArray<bool> bIsDone;

		void Init(uint32 NumElements);
		float Compute(uint32 Index, float CurrentTimeSec);
		void InitEntry(uint32 Index);
		void SetValue(uint32 Index, float InValue, float InStartTime, float InDeltaTimeSec);
	};

	class FThreadChecker
	{
	public:
		FThreadChecker()
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
			: ThreadId(INDEX_NONE)
#endif
		{}

		void InitThread()
		{
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
			ThreadId = FPlatformTLS::GetCurrentThreadId();
#endif
		}

		void CheckThread() const
		{
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
			uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
			checkf(CurrentThreadId == ThreadId,
					TEXT("Function called on wrong thread with id '%d' but supposed to be called on main thread (id=%d)."),
					CurrentThreadId,
					ThreadId
					);
#endif
		}

		void CheckThread()
		{
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
			if (ThreadId == INDEX_NONE)
			{
				ThreadId = FPlatformTLS::GetCurrentThreadId();
			}
			else
			{
				uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
				checkf(CurrentThreadId == ThreadId,
					   TEXT("Function called on wrong thread with id '%d' but supposed to be called on main thread (id=%d)."),
					   CurrentThreadId,
					   ThreadId
					);
			}
#endif
		}

	private:
#if ENABLE_UNREAL_AUDIO_THREAD_DEBUGGING
		uint32 ThreadId;
#endif
	};

}

#endif // #if ENABLE_UNREAL_AUDIO


