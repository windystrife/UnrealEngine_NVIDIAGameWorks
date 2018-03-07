// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace ImmediatePhysics
{
	const int PageBufferSize = 1024*64;	//This has to be here because of VS2013

struct FLinearBlockAllocator
{
	struct FPageStruct
	{
		//We assume FPageStruct is allocated with 16-byte alignment. Do not move Buffer. If we get full support for alignas(16) we could make this better
		uint8 Buffer[PageBufferSize];

		FPageStruct* NextPage;
		FPageStruct* PrevPage;
		int32 SeekPosition;

		FPageStruct()
			: NextPage(nullptr)
			, PrevPage(nullptr)
			, SeekPosition(0)
		{
		}
	};

	FPageStruct* FreePage;
	FPageStruct* FirstPage;

	FLinearBlockAllocator()
	{
		FreePage = AllocPage();
		FirstPage = FreePage;
	}
	
	FPageStruct* AllocPage()
	{
		FPageStruct* ReturnPage = (FPageStruct*)FMemory::Malloc(sizeof(FPageStruct), 16);
		new(ReturnPage) FPageStruct();
		
		FPlatformMisc::TagBuffer("ImmediatePhysicsSim", 0, (const void*)ReturnPage, sizeof(FPageStruct));
		return ReturnPage;
	}

	void ReleasePage(FPageStruct* Page)
	{
		FMemory::Free(Page);
	}

	uint8* Alloc(int32 Bytes)
	{
		check(Bytes < PageBufferSize);	//Page size needs to be increased since we don't allow spillover
		if (Bytes)
		{
			//Assumes 16 byte alignment
			int32 BytesLeft = PageBufferSize - FreePage->SeekPosition;	//don't switch to uint because negative implies we're out of 16 byte aligned space
			if (BytesLeft < Bytes)
			{
				//no space so allocate new page if needed
				if (FreePage->NextPage)
				{
					FreePage = FreePage->NextPage;
				}
				else
				{
					FPageStruct* NewPage = AllocPage();
					NewPage->PrevPage = FreePage;
					FreePage->NextPage = NewPage;
					FreePage = NewPage;
				}
			}

			uint32 ReturnSlot = FreePage->SeekPosition;
			FreePage->SeekPosition = (FreePage->SeekPosition + Bytes + 15)&(~15);

			return &FreePage->Buffer[ReturnSlot];
		}
		else
		{
			return nullptr;
		}
	}

	void Reset()
	{
		for (FPageStruct* Page = FirstPage; Page; Page = Page->NextPage)
		{
			Page->SeekPosition = 0;
		}

		FreePage = FirstPage;
	}

	void Empty()
	{
		FPageStruct* CurrentPage = FirstPage->NextPage;
		while (CurrentPage)
		{
			FPageStruct* OldPage = CurrentPage;
			CurrentPage = CurrentPage->NextPage;
			ReleasePage(OldPage);
		}

		FirstPage->NextPage = nullptr;
		FirstPage->SeekPosition = 0;
		FreePage = FirstPage;
		//Note we do not deallocate FirstPage until the allocator deallocates
	}

	~FLinearBlockAllocator()
	{
		Empty();
		ReleasePage(FirstPage);
	}

private:
	//Don't copy these around
	FLinearBlockAllocator(const FLinearBlockAllocator& Other);
	const FLinearBlockAllocator& operator=(const FLinearBlockAllocator& Other);
};

}