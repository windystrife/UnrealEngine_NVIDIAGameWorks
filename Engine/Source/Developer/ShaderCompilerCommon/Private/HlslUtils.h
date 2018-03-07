// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslUtils.h - Utilities for Hlsl.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

#define USE_UNREAL_ALLOCATOR	0
#define USE_PAGE_POOLING		0

namespace CrossCompiler
{
	namespace Memory
	{
		struct FPage
		{
			int8* Current;
			int8* Begin;
			int8* End;

			FPage(SIZE_T Size)
			{
				check(Size > 0);
				Begin = new int8[Size];
				End = Begin + Size;
				Current = Begin;
			}

			~FPage()
			{
				delete[] Begin;
			}

			static FPage* AllocatePage(SIZE_T PageSize);
			static void FreePage(FPage* Page);
		};

		enum
		{
			MinPageSize = 64 * 1024
		};
	}

	struct FLinearAllocator
	{
		FLinearAllocator()
		{
			auto* Initial = Memory::FPage::AllocatePage(Memory::MinPageSize);
			Pages.Add(Initial);
		}

		~FLinearAllocator()
		{
			for (auto* Page : Pages)
			{
				Memory::FPage::FreePage(Page);
			}
		}

		inline void* Alloc(SIZE_T NumBytes)
		{
			auto* Page = Pages.Last();
			if (Page->Current + NumBytes > Page->End)
			{
				SIZE_T PageSize = FMath::Max<SIZE_T>(Memory::MinPageSize, NumBytes);
				Page = Memory::FPage::AllocatePage(PageSize);
				Pages.Add(Page);
			}

			void* Ptr = Page->Current;
			Page->Current += NumBytes;
			return Ptr;
		}

		inline void* Alloc(SIZE_T NumBytes, SIZE_T Align)
		{
			void* Data = Alloc(NumBytes + Align - 1);
			UPTRINT Address = (UPTRINT)Data;
			Address += (Align - (Address % (UPTRINT)Align)) % Align;
			return (void*)Address;
		}

		/*inline*/ TCHAR* Strdup(const TCHAR* String)
		{
			if (!String)
			{
				return nullptr;
			}

			const auto Length = FCString::Strlen(String);
			const auto Size = (Length + 1) * sizeof(TCHAR);
			auto* Data = (TCHAR*)Alloc(Size, sizeof(TCHAR));
			FCString::Strcpy(Data, Length, String);
			return Data;
		}

		inline TCHAR* Strdup(const FString& String)
		{
			return Strdup(*String);
		}

		TArray<Memory::FPage*, TInlineAllocator<8> > Pages;
	};

#if !USE_UNREAL_ALLOCATOR
	class FLinearAllocatorPolicy
	{
	public:
		// Unreal allocator magic
		enum { NeedsElementType = false };
		enum { RequireRangeCheck = true };

		template<typename ElementType>
		class ForElementType
		{
		public:

			/** Default constructor. */
			ForElementType() :
				LinearAllocator(nullptr),
				Data(nullptr)
			{}

			// FContainerAllocatorInterface
			/*FORCEINLINE*/ ElementType* GetAllocation() const
			{
				return Data;
			}
			void ResizeAllocation(int32 PreviousNumElements, int32 NumElements, int32 NumBytesPerElement)
			{
				void* OldData = Data;
				if (NumElements)
				{
					// Allocate memory from the stack.
					Data = (ElementType*)LinearAllocator->Alloc(NumElements * NumBytesPerElement,
						FMath::Max((uint32)sizeof(void*), (uint32)alignof(ElementType))
						);

					// If the container previously held elements, copy them into the new allocation.
					if (OldData && PreviousNumElements)
					{
						const int32 NumCopiedElements = FMath::Min(NumElements, PreviousNumElements);
						FMemory::Memcpy(Data, OldData, NumCopiedElements * NumBytesPerElement);
					}
				}
			}
			int32 CalculateSlackReserve(int32 NumElements, int32 NumBytesPerElement) const
			{
				return DefaultCalculateSlackReserve(NumElements, NumBytesPerElement, false);
			}
			int32 CalculateSlackShrink(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
			{
				return DefaultCalculateSlackShrink(NumElements, NumAllocatedElements, NumBytesPerElement, false);
			}
			int32 CalculateSlackGrow(int32 NumElements, int32 NumAllocatedElements, int32 NumBytesPerElement) const
			{
				return DefaultCalculateSlackGrow(NumElements, NumAllocatedElements, NumBytesPerElement, false);
			}

			int32 GetAllocatedSize(int32 NumAllocatedElements, int32 NumBytesPerElement) const
			{
				return NumAllocatedElements * NumBytesPerElement;
			}

			bool HasAllocation()
			{
				return !!Data;
			}

			FLinearAllocator* LinearAllocator;

		private:

			/** A pointer to the container's elements. */
			ElementType* Data;
		};

		typedef ForElementType<FScriptContainerElement> ForAnyElementType;
	};

	class FLinearBitArrayAllocator
		: public TInlineAllocator<4, FLinearAllocatorPolicy>
	{
	};

	class FLinearSparseArrayAllocator
		: public TSparseArrayAllocator<FLinearAllocatorPolicy, FLinearBitArrayAllocator>
	{
	};

	class FLinearSetAllocator
		: public TSetAllocator<FLinearSparseArrayAllocator, TInlineAllocator<1, FLinearAllocatorPolicy> >
	{
	};

	template <typename TType>
	class TLinearArray : public TArray<TType, FLinearAllocatorPolicy>
	{
	public:
		TLinearArray(FLinearAllocator* Allocator)
		{
			TArray<TType, FLinearAllocatorPolicy>::AllocatorInstance.LinearAllocator = Allocator;
		}
	};

	/*
	template <typename TType>
	struct TLinearSet : public TSet<typename TType, DefaultKeyFuncs<typename TType>, FLinearSetAllocator>
	{
	TLinearSet(FLinearAllocator* InAllocator)
	{
	Elements.AllocatorInstance.LinearAllocator = InAllocator;
	}
	};*/
#endif

	struct FSourceInfo
	{
		FString* Filename;
		int32 Line;
		int32 Column;

		FSourceInfo() : Filename(nullptr), Line(0), Column(0) {}
	};

	struct FCompilerMessages
	{
		struct FMessage
		{
			//FSourceInfo SourceInfo;
			bool bIsError;
			FString Message;

			FMessage(bool bInIsError, const FString& InMessage) :
				bIsError(bInIsError),
				Message(InMessage)
			{
			}
		};
		TArray<FMessage> MessageList;

		inline void AddMessage(bool bIsError, const FString& Message)
		{
			auto* NewMessage = new(MessageList) FMessage(bIsError, Message);
		}

		inline void SourceError(const FSourceInfo& SourceInfo, const TCHAR* String)
		{
			if (SourceInfo.Filename)
			{
				AddMessage(true, FString::Printf(TEXT("%s(%d): (%d) %s\n"), **SourceInfo.Filename, SourceInfo.Line, SourceInfo.Column, String));
			}
			else
			{
				AddMessage(true, FString::Printf(TEXT("<unknown>(%d): (%d) %s\n"), SourceInfo.Line, SourceInfo.Column, String));
			}
		}

		inline void SourceError(const TCHAR* String)
		{
			AddMessage(true, FString::Printf(TEXT("%s\n"), String));
		}

		inline void SourceWarning(const FSourceInfo& SourceInfo, const TCHAR* String)
		{
			if (SourceInfo.Filename)
			{
				AddMessage(false, FString::Printf(TEXT("%s(%d): (%d) %s\n"), **SourceInfo.Filename, SourceInfo.Line, SourceInfo.Column, String));
			}
			else
			{
				AddMessage(false, FString::Printf(TEXT("<unknown>(%d): (%d) %s\n"), SourceInfo.Line, SourceInfo.Column, String));
			}
		}

		inline void SourceWarning(const TCHAR* String)
		{
			AddMessage(false, FString::Printf(TEXT("%s\n"), String));
		}
	};
}
