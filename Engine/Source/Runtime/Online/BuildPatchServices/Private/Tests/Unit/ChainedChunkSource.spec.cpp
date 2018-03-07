// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationTest.h"
#include "Tests/TestHelpers.h"
#include "Tests/Fake/ChunkDataAccess.fake.h"
#include "Tests/Fake/ChunkSource.fake.h"
#include "Installer/ChainedChunkSource.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockChunkSourceCallCounted
		: public FFakeChunkSource
	{
	public:
		FMockChunkSourceCallCounted()
			: CallCount(0)
		{
		}

		virtual IChunkDataAccess* Get(const FGuid& DataId) override
		{
			++CallCount;
			return FFakeChunkSource::Get(DataId);
		}

	public:
		int32 CallCount;
	};
}

BEGIN_DEFINE_SPEC(FChainedChunkSourceSpec, "BuildPatchServices.Unit", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
// Unit
TUniquePtr<BuildPatchServices::IChainedChunkSource> ChainedChunkSource;
// Mock
TArray<BuildPatchServices::FMockChunkSourceCallCounted> MockChunkSources;
// Data
TArray<FGuid> AllChunks;
END_DEFINE_SPEC(FChainedChunkSourceSpec)

void FChainedChunkSourceSpec::Define()
{
	using namespace BuildPatchServices;

	// Data setup.
	for (int32 Idx = 0; Idx < 50; ++Idx)
	{
		AllChunks.Add(FGuid::NewGuid());
	}

	// Specs.
	BeforeEach([this]()
	{
		for (int32 Idx = 0; Idx < 5; ++Idx)
		{
			MockChunkSources.Add(FMockChunkSourceCallCounted());
			for (int32 ChunkIdx = 0; ChunkIdx < AllChunks.Num(); ++ChunkIdx)
			{
				if (((ChunkIdx + 1) % 5) == Idx)
				{
					FFakeChunkDataAccess* MockChunkDataAccess = new FFakeChunkDataAccess();
					MockChunkDataAccess->ChunkData = nullptr;
					MockChunkDataAccess->ChunkHeader.Guid = AllChunks[ChunkIdx];
					MockChunkSources.Last().ChunkDatas.Add(
						MockChunkDataAccess->ChunkHeader.Guid,
						TUniquePtr<IChunkDataAccess>(MockChunkDataAccess)
					);
				}
			}
		}
		TArray<IChunkSource*> ChunkSources;
		for (int32 Idx = 0; Idx < 5; ++Idx)
		{
			ChunkSources.Add(&MockChunkSources[Idx]);
		}
		ChainedChunkSource.Reset(FChainedChunkSourceFactory::Create(MoveTemp(ChunkSources)));
	});

	Describe("ChainedChunkSource", [this]()
	{
		Describe("Get", [this]()
		{
			Describe("when a chunk does not exist in any source", [this]()
			{
				It("should return nullptr.", [this]()
				{
					TEST_NULL(ChainedChunkSource->Get(FGuid::NewGuid()));
				});

				It("should have called Get on all provided sources.", [this]()
				{
					ChainedChunkSource->Get(FGuid::NewGuid());
					for (const FMockChunkSourceCallCounted& MockChunkSource : MockChunkSources)
					{
						TEST_EQUAL(MockChunkSource.CallCount, 1);
					}
				});
			});

			Describe("when a chunk exists in at least one source", [this]()
			{
				It("should successfully return.", [this]()
				{
					for (const FGuid& ChunkId : AllChunks)
					{
						TEST_NOT_NULL(ChainedChunkSource->Get(ChunkId));
					}
				});

				It("should return the correct chunk.", [this]()
				{
					for (const FGuid& ChunkId : AllChunks)
					{
						FFakeChunkDataAccess* ReturnValue = static_cast<FFakeChunkDataAccess*>(ChainedChunkSource->Get(ChunkId));
						if (ReturnValue != nullptr)
						{
							TEST_EQUAL(ReturnValue->GetGuid(), ChunkId);
						}
					}
				});
			});
		});
	});

	AfterEach([this]()
	{
		ChainedChunkSource.Reset();
		MockChunkSources.Reset();
	});
}

#endif //WITH_DEV_AUTOMATION_TESTS
