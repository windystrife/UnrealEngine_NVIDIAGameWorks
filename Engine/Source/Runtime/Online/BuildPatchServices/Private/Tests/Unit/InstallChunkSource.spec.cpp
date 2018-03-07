// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationTest.h"
#include "Tests/TestHelpers.h"
#include "Tests/Fake/ChunkDataAccess.fake.h"
#include "Tests/Fake/FileSystem.fake.h"
#include "Tests/Fake/ChunkStore.fake.h"
#include "Tests/Fake/ChunkReferenceTracker.fake.h"
#include "Tests/Fake/InstallerError.fake.h"
#include "Tests/Mock/InstallChunkSourceStat.mock.h"
#include "Tests/Mock/Manifest.mock.h"
#include "Installer/InstallChunkSource.h"
#include "Math/RandomStream.h"
#include "BuildPatchHash.h"
#include "Misc/SecureHash.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FInstallChunkSourceSpec, "BuildPatchServices.Unit", EAutomationTestFlags::ProductFilter | EAutomationTestFlags::ApplicationContextMask)
// Unit.
TUniquePtr<BuildPatchServices::IInstallChunkSource> InstallChunkSource;
// Mock.
TUniquePtr<BuildPatchServices::FFakeFileSystem> FakeFileSystem;
TUniquePtr<BuildPatchServices::FFakeChunkStore> FakeChunkStore;
TUniquePtr<BuildPatchServices::FFakeChunkReferenceTracker> MockChunkReferenceTracker;
TUniquePtr<BuildPatchServices::FFakeInstallerError> MockInstallerError;
TUniquePtr<BuildPatchServices::FMockInstallChunkSourceStat> MockInstallChunkSourceStat;
BuildPatchServices::FMockManifestPtr MockManifest;
// Data.
BuildPatchServices::FInstallSourceConfig Configuration;
TMap<FString, FBuildPatchAppManifestRef> InstallationSources;
TSet<FGuid> SomeAvailableChunks;
FGuid SomeChunk;
float PauseTime;
bool bHasPaused;
// Test helpers.
void MakeUnit();
void InventUsableChunkData();
void SomeChunkAvailable();
void SomeChunkUnavailable();
void SomeChunkCorrupted();
TFuture<void> PauseFor(float Seconds);
END_DEFINE_SPEC(FInstallChunkSourceSpec)

void FInstallChunkSourceSpec::Define()
{
	using namespace BuildPatchServices;

	// Data setup.
	FRollingHashConst::Init();
	SomeChunk = FGuid::NewGuid();
	Configuration.BatchFetchMinimum = 10;
	Configuration.BatchFetchMaximum = 10;

	// Specs.
	BeforeEach([this]()
	{
		FakeFileSystem.Reset(new FFakeFileSystem());
		FakeChunkStore.Reset(new FFakeChunkStore());
		MockChunkReferenceTracker.Reset(new FFakeChunkReferenceTracker());
		MockInstallerError.Reset(new FFakeInstallerError());
		MockInstallChunkSourceStat.Reset(new FMockInstallChunkSourceStat());
		MockManifest = MakeShareable(new FMockManifest());
	});

	Describe("InstallChunkSource", [this]()
	{
		Describe("GetAvailableChunks", [this]()
		{
			Describe("when there are no chunks available", [this]()
			{
				BeforeEach([this]()
				{
					MakeUnit();
				});

				It("should return an empty set.", [this]()
				{
					TSet<FGuid> AvailableChunks = InstallChunkSource->GetAvailableChunks();
					TEST_TRUE(AvailableChunks.Num() == 0);
				});
			});

			Describe("when there are some available chunks", [this]()
			{
				BeforeEach([this]()
				{
					InventUsableChunkData();
					MakeUnit();
				});

				It("should return the available chunks.", [this]()
				{
					TSet<FGuid> AvailableChunks = InstallChunkSource->GetAvailableChunks();
					TEST_EQUAL(AvailableChunks, SomeAvailableChunks);
				});
			});
		});

		Describe("Get", [this]()
		{
			Describe("when some chunk is not available", [this]()
			{
				BeforeEach([this]()
				{
					InventUsableChunkData();
					SomeChunkUnavailable();
					MakeUnit();
				});

				Describe("when some chunk is not in the store", [this]()
				{
					It("should return nullptr.", [this]()
					{
						TEST_NULL(InstallChunkSource->Get(SomeChunk));
					});
				});

				Describe("when some chunk is in the store", [this]()
				{
					BeforeEach([this]()
					{
						FakeChunkStore->Store.Emplace(SomeChunk, new FFakeChunkDataAccess());
					});

					It("should return some chunk.", [this]()
					{
						TEST_NOT_NULL(InstallChunkSource->Get(SomeChunk));
					});
				});
			});

			Describe("when some chunk is available", [this]()
			{
				BeforeEach([this]()
				{
					InventUsableChunkData();
					SomeChunkAvailable();
					MakeUnit();
				});

				Describe("when some chunk is not in the store", [this]()
				{
					It("should return some chunk loading from disk.", [this]()
					{
						TEST_NOT_NULL(InstallChunkSource->Get(SomeChunk));
						TEST_EQUAL(MockInstallChunkSourceStat->RxLoadStarted.Num(), 1);
						TEST_EQUAL(MockInstallChunkSourceStat->RxLoadComplete.Num(), 1);
						TEST_TRUE(FakeFileSystem->RxCreateFileReader.Num() > 0);
					});

					Describe("when upcoming chunk references are known", [this]()
					{
						BeforeEach([this]()
						{
							MockChunkReferenceTracker->NextReferences = SomeAvailableChunks.Array();
						});

						It("should also load upcoming chunks according to provided configuration.", [this]()
						{
							InstallChunkSource->Get(SomeChunk);
							TEST_TRUE(FakeChunkStore->Store.Contains(SomeChunk));
							for (int32 NextReferenceIdx = 0; NextReferenceIdx < Configuration.BatchFetchMaximum; ++NextReferenceIdx)
							{
								TEST_TRUE(FakeChunkStore->Store.Contains(MockChunkReferenceTracker->NextReferences[NextReferenceIdx]));
							}
						});
					});

					Describe("when some chunk hashes are not known", [this]()
					{
						BeforeEach([this]()
						{
							for (TPair<FString, FBuildPatchAppManifestRef>& InstallationSourcePair : InstallationSources)
							{
								FMockManifest* MockInstallationManifest = (FMockManifest*)&InstallationSourcePair.Value.Get();
								MockInstallationManifest->ChunkHashes.Remove(SomeChunk);
								MockInstallationManifest->ChunkShaHashes.Remove(SomeChunk);
							}
						});

						It("should not have attempted to load some chunk from disk.", [this]()
						{
							InstallChunkSource->Get(SomeChunk);
							TEST_EQUAL(FakeFileSystem->RxCreateFileReader.Num(), 0);
						});
					});

					Describe("when some chunk sha is not known", [this]()
					{
						BeforeEach([this]()
						{
							for (TPair<FString, FBuildPatchAppManifestRef>& InstallationSourcePair : InstallationSources)
							{
								FMockManifest* MockInstallationManifest = (FMockManifest*)&InstallationSourcePair.Value.Get();
								MockInstallationManifest->ChunkShaHashes.Remove(SomeChunk);
							}
						});

						It("should still succeed to load some chunk from disk.", [this]()
						{
							TEST_NOT_NULL(InstallChunkSource->Get(SomeChunk));
							TEST_EQUAL(MockInstallChunkSourceStat->RxLoadStarted.Num(), 1);
							TEST_EQUAL(MockInstallChunkSourceStat->RxLoadComplete.Num(), 1);
							TEST_TRUE(FakeFileSystem->RxCreateFileReader.Num() > 0);
						});

						Describe("when data required for some chunk is corrupt", [this]()
						{
							BeforeEach([this]()
							{
								SomeChunkCorrupted();
							});

							It("should fail to load some chunk from disk, reporting IInstallChunkSourceStat::ELoadResult::HashCheckFailed.", [this]()
							{
								TEST_NULL(InstallChunkSource->Get(SomeChunk));
								TEST_EQUAL(MockInstallChunkSourceStat->RxLoadComplete.Num(), 1);
								if (MockInstallChunkSourceStat->RxLoadComplete.Num() == 1)
								{
									TEST_EQUAL(MockInstallChunkSourceStat->RxLoadComplete[0].Get<2>(), IInstallChunkSourceStat::ELoadResult::HashCheckFailed);
								}
							});
						});
					});

					Describe("when data required for some chunk is corrupt", [this]()
					{
						BeforeEach([this]()
						{
							SomeChunkCorrupted();
						});

						It("should fail to load some chunk from disk, reporting IInstallChunkSourceStat::ELoadResult::HashCheckFailed.", [this]()
						{
							TEST_NULL(InstallChunkSource->Get(SomeChunk));
							TEST_EQUAL(MockInstallChunkSourceStat->RxLoadComplete.Num(), 1);
							if (MockInstallChunkSourceStat->RxLoadComplete.Num() == 1)
							{
								TEST_EQUAL(MockInstallChunkSourceStat->RxLoadComplete[0].Get<2>(), IInstallChunkSourceStat::ELoadResult::HashCheckFailed);
							}
						});
					});
				});

				Describe("when some chunk is in the store", [this]()
				{
					BeforeEach([this]()
					{
						FakeChunkStore->Store.Emplace(SomeChunk, new FFakeChunkDataAccess());
					});

					It("should return some chunk without loading.", [this]()
					{
						TEST_NOT_NULL(InstallChunkSource->Get(SomeChunk));
						TEST_EQUAL(MockInstallChunkSourceStat->RxLoadStarted.Num(), 0);
						TEST_EQUAL(FakeFileSystem->RxCreateFileReader.Num(), 0);
					});
				});
			});
		});

		Describe("SetPaused", [this]()
		{
			BeforeEach([this]()
			{
				InventUsableChunkData();
				SomeChunkAvailable();
				MakeUnit();
				MockChunkReferenceTracker->NextReferences = SomeAvailableChunks.Array();
				bHasPaused = false;
				PauseTime = 0.1f;
				MockInstallChunkSourceStat->OnLoadCompleteFunc = [this](const FGuid& ChunkId, IInstallChunkSourceStat::ELoadResult Result)
				{
					if (!bHasPaused)
					{
						bHasPaused = true;
						PauseFor(PauseTime);
					}
				};
			});

			It("should delay the chunk load process.", [this]()
			{
				InstallChunkSource->Get(SomeChunk);
				double LongestDelay = 0.0f;
				for (int32 Idx = 1; Idx < MockInstallChunkSourceStat->RxLoadStarted.Num(); ++Idx)
				{
					double ThisDelay = MockInstallChunkSourceStat->RxLoadStarted[Idx].Get<0>() - MockInstallChunkSourceStat->RxLoadStarted[Idx - 1].Get<0>();
					if (ThisDelay > LongestDelay)
					{
						LongestDelay = ThisDelay;
					}
				}
				TEST_TRUE(LongestDelay >= PauseTime);
			});
		});

		Describe("Abort", [this]()
		{
			BeforeEach([this]()
			{
				InventUsableChunkData();
				SomeChunkAvailable();
				MakeUnit();
				MockChunkReferenceTracker->NextReferences = SomeAvailableChunks.Array();
				MockInstallChunkSourceStat->OnLoadStartedFunc = [this](const FGuid& ChunkId)
				{
					if (MockInstallChunkSourceStat->RxLoadStarted.Num() > 1)
					{
						InstallChunkSource->Abort();
					}
				};
			});

			It("should abort loading of chunks, reporting IInstallChunkSourceStat::ELoadResult::Aborted.", [this]()
			{
				InstallChunkSource->Get(SomeChunk);
				TSet<FGuid> StartedChunks;
				for (const FMockInstallChunkSourceStat::FLoadStarted& Call : MockInstallChunkSourceStat->RxLoadStarted)
				{
					StartedChunks.Add(Call.Get<1>());
				}
				TEST_TRUE(StartedChunks.Num() < Configuration.BatchFetchMinimum);
			});
		});
	});

	AfterEach([this]()
	{
		FakeFileSystem.Reset();
		FakeChunkStore.Reset();
		MockChunkReferenceTracker.Reset();
		MockInstallerError.Reset();
		MockInstallChunkSourceStat.Reset();
		MockManifest.Reset();
		InstallationSources.Reset();
		SomeAvailableChunks.Reset();
	});
}

void FInstallChunkSourceSpec::MakeUnit()
{
	using namespace BuildPatchServices;
	InstallChunkSource.Reset(FInstallChunkSourceFactory::Create(
		Configuration,
		FakeFileSystem.Get(),
		FakeChunkStore.Get(),
		MockChunkReferenceTracker.Get(),
		MockInstallerError.Get(),
		MockInstallChunkSourceStat.Get(),
		InstallationSources,
		MockManifest.ToSharedRef()));
}

void FInstallChunkSourceSpec::InventUsableChunkData()
{
	using namespace BuildPatchServices;
	for (int32 Count = 0; Count < 100; ++Count)
	{
		MockManifest->DataList.Add(FGuid::NewGuid());
	}
	FMockManifest* MockInstallationManifestA = new FMockManifest();
	FMockManifest* MockInstallationManifestB = new FMockManifest();
	InstallationSources.Add(TEXT("LocationA/"), MakeShareable(MockInstallationManifestA));
	InstallationSources.Add(TEXT("LocationB/"), MakeShareable(MockInstallationManifestB));
	for (int32 DataListIdx = 0; DataListIdx < MockManifest->DataList.Num(); ++DataListIdx)
	{
		const FGuid& TheChunk = MockManifest->DataList[DataListIdx];
		switch (DataListIdx % 3)
		{
			case 0:
			MockInstallationManifestA->ProducibleChunks.Add(TheChunk);
			SomeAvailableChunks.Add(TheChunk);
			break;
			case 1:
			MockInstallationManifestB->ProducibleChunks.Add(TheChunk);
			SomeAvailableChunks.Add(TheChunk);
			break;
		}
	}
	uint32 FileCounter = 1;
	for (FMockManifest* MockInstallationManifest : {MockInstallationManifestA, MockInstallationManifestB})
	{
		FString MockInstallLocation;
		for (const TPair<FString, FBuildPatchAppManifestRef>& InstallationSourcePair : InstallationSources)
		{
			if (&InstallationSourcePair.Value.Get() == MockInstallationManifest)
			{
				MockInstallLocation = InstallationSourcePair.Key;
			}
		}
		for (const FGuid& ProducibleChunk : MockInstallationManifest->ProducibleChunks)
		{
			uint32 ChunkSizeCounter = BuildPatchServices::ChunkDataSize;
			TArray<FFileChunkPart>& FileChunkParts = MockInstallationManifest->FilePartsForChunk.FindOrAdd(ProducibleChunk);
			for (int32 FileIdx = 0; FileIdx < 3; ++FileIdx)
			{
				FileChunkParts.AddDefaulted();
				FFileChunkPart& FileChunkPart = FileChunkParts.Last();
				FileChunkPart.Filename = FString::Printf(TEXT("File%d.dat"), FileCounter++);
				FileChunkPart.FileOffset = 0;
				FileChunkPart.ChunkPart.Guid = ProducibleChunk;
				FileChunkPart.ChunkPart.Offset = BuildPatchServices::ChunkDataSize - ChunkSizeCounter;
				FileChunkPart.ChunkPart.Size = BuildPatchServices::ChunkDataSize / 4;
				ChunkSizeCounter -= FileChunkPart.ChunkPart.Size;
			}
			FileChunkParts.AddDefaulted();
			FFileChunkPart& FileChunkPart = FileChunkParts.Last();
			FileChunkPart.Filename = FString::Printf(TEXT("File%d.dat"), FileCounter++);
			FileChunkPart.FileOffset = 0;
			FileChunkPart.ChunkPart.Guid = ProducibleChunk;
			FileChunkPart.ChunkPart.Offset = BuildPatchServices::ChunkDataSize - ChunkSizeCounter;
			FileChunkPart.ChunkPart.Size = ChunkSizeCounter;
			ChunkSizeCounter -= FileChunkPart.ChunkPart.Size;
			check(ChunkSizeCounter == 0);
			check(FileChunkPart.ChunkPart.Offset + FileChunkPart.ChunkPart.Size == BuildPatchServices::ChunkDataSize);
		}
		for (const TPair<FGuid, TArray<FFileChunkPart>>& ChunkFileParts : MockInstallationManifest->FilePartsForChunk)
		{
			for (const FFileChunkPart& FileChunkPart : ChunkFileParts.Value)
			{
				TArray<uint8>& FileData = FakeFileSystem->DiskData.FindOrAdd(MockInstallLocation / FileChunkPart.Filename);
				FileData.SetNumUninitialized(FMath::Max<int32>(FileData.Num(), FileChunkPart.FileOffset + FileChunkPart.ChunkPart.Size));
			}
		}
	}
	FRandomStream RandomData(0);
	for (TPair<FString, TArray<uint8>>& FileData : FakeFileSystem->DiskData)
	{
		uint8* Data = FileData.Value.GetData();
		for (int32 DataIdx = 0; DataIdx <= (FileData.Value.Num()-4); DataIdx += 4)
		{
			*((uint32*)(Data + DataIdx)) = RandomData.GetUnsignedInt();
		}
	}
	TArray<uint8> ChunkData;
	ChunkData.SetNumUninitialized(BuildPatchServices::ChunkDataSize);
	for (FMockManifest* MockInstallationManifest : {MockInstallationManifestA, MockInstallationManifestB})
	{
		FString MockInstallLocation;
		for (const TPair<FString, FBuildPatchAppManifestRef>& InstallationSourcePair : InstallationSources)
		{
			if (&InstallationSourcePair.Value.Get() == MockInstallationManifest)
			{
				MockInstallLocation = InstallationSourcePair.Key;
			}
		}
		for (const FGuid& ProducibleChunk : MockInstallationManifest->ProducibleChunks)
		{
			uint64 ChunkPolyHash;
			FSHAHashData ChunkShaHash;
			for (const FFileChunkPart& FileChunkPart : MockInstallationManifest->FilePartsForChunk[ProducibleChunk])
			{
				TArray<uint8>& FileData = FakeFileSystem->DiskData[MockInstallLocation / FileChunkPart.Filename];
				uint8* DataPtr = &FileData[FileChunkPart.FileOffset];
				FMemory::Memcpy(&ChunkData[FileChunkPart.ChunkPart.Offset], DataPtr, FileChunkPart.ChunkPart.Size);
			}
			ChunkPolyHash = FCycPoly64Hash::GetHashForDataSet(ChunkData.GetData(), ChunkData.Num());
			FSHA1::HashBuffer(ChunkData.GetData(), BuildPatchServices::ChunkDataSize, ChunkShaHash.Hash);
			MockInstallationManifest->ChunkHashes.Add(ProducibleChunk, ChunkPolyHash);
			MockInstallationManifest->ChunkShaHashes.Add(ProducibleChunk, ChunkShaHash);
		}
	}
}

void FInstallChunkSourceSpec::SomeChunkAvailable()
{
	SomeChunk = *SomeAvailableChunks.CreateConstIterator();
}

void FInstallChunkSourceSpec::SomeChunkUnavailable()
{
	SomeChunk = FGuid::NewGuid();
}

void FInstallChunkSourceSpec::SomeChunkCorrupted()
{
	using namespace BuildPatchServices;
	for (TPair<FString, FBuildPatchAppManifestRef>& InstallationSourcePair : InstallationSources)
	{
		FMockManifest* MockInstallationManifest = (FMockManifest*)&InstallationSourcePair.Value.Get();
		if (MockInstallationManifest->FilePartsForChunk.Contains(SomeChunk))
		{
			const TArray<FFileChunkPart>& FileChunkParts = MockInstallationManifest->FilePartsForChunk[SomeChunk];
			if (FileChunkParts.Num() > 0)
			{
				const FFileChunkPart& FileChunkPart = FileChunkParts[0];
				TArray<uint8>& FileData = FakeFileSystem->DiskData[InstallationSourcePair.Key / FileChunkPart.Filename];
				for (int32 DataIdx = 0; DataIdx < 10; ++DataIdx)
				{
					FileData[DataIdx] = FileData[DataIdx + 1];
				}
			}
		}
	}
}

TFuture<void> FInstallChunkSourceSpec::PauseFor(float Seconds)
{
	double PauseAt = BuildPatchServices::FStatsCollector::GetSeconds();
	InstallChunkSource->SetPaused(true);
	TFunction<void()> Task = [this, PauseAt, Seconds]()
	{
		while ((BuildPatchServices::FStatsCollector::GetSeconds() - PauseAt) < Seconds)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		InstallChunkSource->SetPaused(false);
	};
	return Async(EAsyncExecution::Thread, MoveTemp(Task));
}

#endif //WITH_DEV_AUTOMATION_TESTS
