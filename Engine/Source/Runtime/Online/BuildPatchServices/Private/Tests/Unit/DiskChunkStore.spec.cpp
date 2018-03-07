// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationTest.h"
#include "Tests/TestHelpers.h"
#include "Tests/Fake/ChunkDataAccess.fake.h"
#include "Tests/Mock/ChunkDataSerialization.mock.h"
#include "Tests/Mock/DiskChunkStoreStat.mock.h"
#include "Installer/DiskChunkStore.h"
#include "BuildPatchHash.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FDiskChunkStoreSpec, "BuildPatchServices.Unit", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
// Unit
TUniquePtr<BuildPatchServices::IDiskChunkStore> DiskChunkStore;
// Mock
TUniquePtr<BuildPatchServices::FMockChunkDataSerialization> MockChunkDataSerialization;
TUniquePtr<BuildPatchServices::FMockDiskChunkStoreStat> MockDiskChunkStoreStat;
TUniquePtr<BuildPatchServices::FFakeChunkDataAccess> MockChunkDataAccess;
// Data
FString StoreRootPath;
FGuid SomeChunk;
END_DEFINE_SPEC(FDiskChunkStoreSpec)

void FDiskChunkStoreSpec::Define()
{
	using namespace BuildPatchServices;

	// Data setup.
	FRollingHashConst::Init();
	StoreRootPath = TEXT("RootPath");
	SomeChunk = FGuid::NewGuid();

	// Specs.
	BeforeEach([this]()
	{
		MockChunkDataSerialization.Reset(new FMockChunkDataSerialization());
		MockDiskChunkStoreStat.Reset(new FMockDiskChunkStoreStat());
		MockChunkDataAccess.Reset(new FFakeChunkDataAccess());
		DiskChunkStore.Reset(FDiskChunkStoreFactory::Create(
			MockChunkDataSerialization.Get(),
			MockDiskChunkStoreStat.Get(),
			StoreRootPath));
	});

	Describe("DiskChunkStore", [this]()
	{
		Describe("Put", [this]()
		{
			It("should save some chunk to the provided store root path.", [this]()
			{
				DiskChunkStore->Put(SomeChunk, TUniquePtr<IChunkDataAccess>(new FFakeChunkDataAccess()));
				TEST_EQUAL(MockChunkDataSerialization->RxSaveToFile.Num(), 1);
				if (MockChunkDataSerialization->RxSaveToFile.Num() == 1)
				{
					TEST_TRUE(MockChunkDataSerialization->RxSaveToFile[0].Get<0>().StartsWith(StoreRootPath + TEXT("/")));
				}
			});

			It("should not save some chunk that was previously saved.", [this]()
			{
				DiskChunkStore->Put(SomeChunk, TUniquePtr<IChunkDataAccess>(new FFakeChunkDataAccess()));
				DiskChunkStore->Put(SomeChunk, TUniquePtr<IChunkDataAccess>(new FFakeChunkDataAccess()));
				TEST_EQUAL(MockChunkDataSerialization->RxSaveToFile.Num(), 1);
			});
		});

		Describe("Get", [this]()
		{
			Describe("when some chunk was not previously Put", [this]()
			{
				It("should not attempt to load some chunk.", [this]()
				{
					TEST_NULL(DiskChunkStore->Get(SomeChunk));
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 0);
				});
			});

			Describe("when some chunk was previously Put", [this]()
			{
				BeforeEach([this]()
				{
					DiskChunkStore->Put(SomeChunk, TUniquePtr<IChunkDataAccess>(new FFakeChunkDataAccess()));
				});

				It("should load some chunk from the provided store root path.", [this]()
				{
					DiskChunkStore->Get(SomeChunk);
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 1);
					if (MockChunkDataSerialization->RxLoadFromFile.Num() == 1)
					{
						TEST_TRUE(MockChunkDataSerialization->RxLoadFromFile[0].Get<0>().StartsWith(StoreRootPath + TEXT("/")));
					}
				});

				Describe("and LoadFromFile will be successful", [this]()
				{
					BeforeEach([this]()
					{
						MockChunkDataSerialization->TxLoadFromFile.Emplace(MockChunkDataAccess.Release(), EChunkLoadResult::Success);
					});

					It("should not load some chunk twice in a row.", [this]()
					{
						TEST_EQUAL(DiskChunkStore->Get(SomeChunk), DiskChunkStore->Get(SomeChunk));
						TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 1);
					});
				});

				Describe("and LoadFromFile will not be successful", [this]()
				{
					BeforeEach([this]()
					{
						MockChunkDataSerialization->TxLoadFromFile.Emplace(nullptr, EChunkLoadResult::SerializationError);
					});

					It("should return nullptr.", [this]()
					{
						TEST_NULL(DiskChunkStore->Get(SomeChunk));
					});

					It("should only attempt to load some chunk once.", [this]()
					{
						DiskChunkStore->Get(SomeChunk);
						DiskChunkStore->Get(SomeChunk);
						TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 1);
					});
				});
			});
		});

		Describe("Remove", [this]()
		{
			Describe("when some chunk was not previously Put", [this]()
			{
				It("should not attempt to load some chunk.", [this]()
				{
					TUniquePtr<IChunkDataAccess> Removed = DiskChunkStore->Remove(SomeChunk);
					TEST_FALSE(Removed.IsValid());
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 0);
				});
			});

			Describe("when some chunk was previously Put", [this]()
			{
				BeforeEach([this]()
				{
					DiskChunkStore->Put(SomeChunk, TUniquePtr<IChunkDataAccess>(new FFakeChunkDataAccess()));
				});

				It("should load the chunk from the provided store root path.", [this]()
				{
					DiskChunkStore->Remove(SomeChunk);
					TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 1);
					if (MockChunkDataSerialization->RxLoadFromFile.Num() == 1)
					{
						TEST_TRUE(MockChunkDataSerialization->RxLoadFromFile[0].Get<0>().StartsWith(StoreRootPath + TEXT("/")));
					}
				});

				Describe("and LoadFromFile will be successful", [this]()
				{
					BeforeEach([this]()
					{
						MockChunkDataSerialization->TxLoadFromFile.Emplace(MockChunkDataAccess.Release(), EChunkLoadResult::Success);
						MockChunkDataAccess.Reset(new FFakeChunkDataAccess());
					});

					Describe("and when some chunk was last used with Get", [this]()
					{
						BeforeEach([this]()
						{
							DiskChunkStore->Get(SomeChunk);
							MockChunkDataSerialization->RxLoadFromFile.Empty();
						});

						It("should return some chunk without loading it.", [this]()
						{
							TUniquePtr<IChunkDataAccess> Removed = DiskChunkStore->Remove(SomeChunk);
							TEST_TRUE(Removed.IsValid());
							TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 0);
						});
					});

					Describe("and when some chunk was last used with Remove", [this]()
					{
						BeforeEach([this]()
						{
							DiskChunkStore->Remove(SomeChunk);
							MockChunkDataSerialization->TxLoadFromFile.Emplace(MockChunkDataAccess.Release(), EChunkLoadResult::Success);
							MockChunkDataAccess.Reset(new FFakeChunkDataAccess());
							MockChunkDataSerialization->RxLoadFromFile.Empty();
						});

						It("should need to reload some chunk.", [this]()
						{
							TUniquePtr<IChunkDataAccess> Removed = DiskChunkStore->Remove(SomeChunk);
							TEST_TRUE(Removed.IsValid());
							TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 1);
						});
					});
				});

				Describe("and LoadFromFile will not be successful", [this]()
				{
					BeforeEach([this]()
					{
						MockChunkDataSerialization->TxLoadFromFile.Emplace(nullptr, EChunkLoadResult::SerializationError);
					});

					It("should return invalid ptr.", [this]()
					{
						TEST_FALSE(DiskChunkStore->Remove(SomeChunk).IsValid());
					});

					It("should only attempt to load some chunk once.", [this]()
					{
						DiskChunkStore->Remove(SomeChunk);
						DiskChunkStore->Remove(SomeChunk);
						TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 1);
					});
				});
			});
		});

		Describe("GetSlack", [this]()
		{
			It("should always return MAX_int32.", [this]()
			{
				FGuid ChunkId = FGuid::NewGuid();
				TEST_EQUAL(DiskChunkStore->GetSlack(), MAX_int32);
				DiskChunkStore->Put(ChunkId, TUniquePtr<IChunkDataAccess>(new FFakeChunkDataAccess()));
				TEST_EQUAL(DiskChunkStore->GetSlack(), MAX_int32);
				DiskChunkStore->Remove(ChunkId);
				TEST_EQUAL(DiskChunkStore->GetSlack(), MAX_int32);
			});
		});

		It("should use the same filename for Put and Get.", [this]()
		{
			FGuid ChunkId = FGuid::NewGuid();
			DiskChunkStore->Put(ChunkId, TUniquePtr<IChunkDataAccess>(new FFakeChunkDataAccess()));
			DiskChunkStore->Get(ChunkId);
			TEST_EQUAL(MockChunkDataSerialization->RxSaveToFile.Num(), 1);
			TEST_EQUAL(MockChunkDataSerialization->RxLoadFromFile.Num(), 1);
			if (MockChunkDataSerialization->RxSaveToFile.Num() == 1 && MockChunkDataSerialization->RxLoadFromFile.Num() == 1)
			{
				TEST_EQUAL(MockChunkDataSerialization->RxSaveToFile[0].Get<0>(), MockChunkDataSerialization->RxLoadFromFile[0].Get<0>());
			}
		});
	});

	AfterEach([this]()
	{
		DiskChunkStore.Reset();
	});
}

#endif //WITH_DEV_AUTOMATION_TESTS
