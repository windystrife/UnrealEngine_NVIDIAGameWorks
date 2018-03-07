// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerCategoryReplicator.h"
#include "GameFramework/PlayerController.h"
#include "GameplayDebuggerAddonBase.h"
#include "GameplayDebuggerAddonManager.h"
#include "GameplayDebuggerPlayerManager.h"
#include "GameplayDebuggerRenderingComponent.h"
#include "GameplayDebuggerExtension.h"
#include "Net/UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerCategoryReplicatorData

DEFINE_LOG_CATEGORY_STATIC(LogGameplayDebugReplication, Display, All);

static TAutoConsoleVariable<int32> CVarGameplayDebuggerRepDetails(
	TEXT("ai.debug.DetailedReplicationLogs"),
	0,
	TEXT("Enable or disable very verbose replication logs for gameplay debugger"),
	ECVF_Cheat);

class FNetFastCategoryBaseState : public INetDeltaBaseState
{
public:
	struct FDataPackState
	{
		int32 DataOffset;
		int16 DataVersion;
		int16 SyncCounter;

		FDataPackState() : DataOffset(0), DataVersion(0), SyncCounter(0) {}
		FDataPackState(const FGameplayDebuggerDataPack::FHeader& Header) : DataOffset(Header.DataOffset), DataVersion(Header.DataVersion), SyncCounter(Header.SyncCounter) {}

		FORCEINLINE bool IsEqual(const FDataPackState& OtherState) const
		{
			return (DataOffset == OtherState.DataOffset) && (DataVersion == OtherState.DataVersion) && (SyncCounter == OtherState.SyncCounter);
		}

		FORCEINLINE bool operator==(const FDataPackState& OtherState) const { return IsEqual(OtherState); }
		FORCEINLINE bool operator!=(const FDataPackState& OtherState) const { return !IsEqual(OtherState); }
	};

	struct FCategoryState
	{
		int32 TextLinesRepCounter;
		int32 ShapesRepCounter;
		TArray<FDataPackState> DataPackStates;

		FCategoryState() : TextLinesRepCounter(0), ShapesRepCounter(0) {}

		FORCEINLINE bool IsEqual(const FCategoryState& OtherState) const
		{
			return (TextLinesRepCounter == OtherState.TextLinesRepCounter) &&
				(ShapesRepCounter == OtherState.ShapesRepCounter) &&
				(DataPackStates == OtherState.DataPackStates);
		}

		FORCEINLINE bool operator==(const FCategoryState& OtherState) const { return IsEqual(OtherState); }
		FORCEINLINE bool operator!=(const FCategoryState& OtherState) const { return !IsEqual(OtherState); }
	};

	virtual bool IsStateEqual(INetDeltaBaseState* OtherState) override
	{
		FNetFastCategoryBaseState* Other = static_cast<FNetFastCategoryBaseState*>(OtherState);
		return (CategoryStates == Other->CategoryStates);
	}

	void DumpToLog()
	{
		for (int32 CategoryIdx = 0; CategoryIdx < CategoryStates.Num(); CategoryIdx++)
		{
			const FCategoryState& CategoryData = CategoryStates[CategoryIdx];
			UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("category[%d] TextLinesRepCounter:%d ShapesRepCounter:%d"),
				CategoryIdx, CategoryData.TextLinesRepCounter, CategoryData.ShapesRepCounter);

			for (int32 DataPackIdx = 0; DataPackIdx < CategoryData.DataPackStates.Num(); DataPackIdx++)
			{
				const FDataPackState& DataPack = CategoryData.DataPackStates[DataPackIdx];
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT(">>    data[%d] DataVersion:%d SyncCounter:%d DataOffset:%d"),
					DataPackIdx, DataPack.DataVersion, DataPack.SyncCounter, DataPack.DataOffset);
			}
		}
	}

	TArray<FCategoryState> CategoryStates;
};

bool FGameplayDebuggerNetPack::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	if (DeltaParms.bUpdateUnmappedObjects || Owner == nullptr)
	{
		return true;
	}

	if (DeltaParms.Writer)
	{
		FBitWriter& Writer = *DeltaParms.Writer;
		int32 NumChangedCategories = 0;

		FNetFastCategoryBaseState* OldState = static_cast<FNetFastCategoryBaseState*>(DeltaParms.OldState);
		FNetFastCategoryBaseState* NewState = nullptr;
		TArray<uint8> ChangedCategories;

		// find delta to replicate
		if (Owner->bIsEnabled && Owner->Categories.Num() == SavedData.Num())
		{
			NewState = new FNetFastCategoryBaseState();
			check(DeltaParms.NewState);
			*DeltaParms.NewState = TSharedPtr<INetDeltaBaseState>(NewState);
			NewState->CategoryStates.SetNum(SavedData.Num());
			ChangedCategories.AddZeroed(SavedData.Num());

			for (int32 Idx = 0; Idx < SavedData.Num(); Idx++)
			{
				FNetFastCategoryBaseState::FCategoryState& CategoryState = NewState->CategoryStates[Idx];
				FGameplayDebuggerCategory& CategoryOb = Owner->Categories[Idx].Get();
				FCategoryData& SavedCategory = SavedData[Idx];

				const bool bMissingOldState = (OldState == nullptr) || !OldState->CategoryStates.IsValidIndex(Idx);
				ChangedCategories[Idx] = bMissingOldState ? 1 : 0;

				if (SavedCategory.bIsEnabled != CategoryOb.bIsEnabled)
				{
					SavedCategory.bIsEnabled = CategoryOb.bIsEnabled;
					ChangedCategories[Idx]++;
				}

				const bool bTextLinesChanged = (SavedCategory.TextLines != CategoryOb.ReplicatedLines);
				CategoryState.TextLinesRepCounter = (bMissingOldState ? 0 : OldState->CategoryStates[Idx].TextLinesRepCounter) + (bTextLinesChanged ? 1 : 0);
				if (bTextLinesChanged)
				{
					SavedCategory.TextLines = CategoryOb.ReplicatedLines;
					ChangedCategories[Idx]++;
				}

				const bool bShapesChanged = (SavedCategory.Shapes != CategoryOb.ReplicatedShapes);
				CategoryState.ShapesRepCounter = (bMissingOldState ? 0 : OldState->CategoryStates[Idx].ShapesRepCounter) + (bShapesChanged ? 1 : 0);
				if (bShapesChanged)
				{
					SavedCategory.Shapes = CategoryOb.ReplicatedShapes;
					ChangedCategories[Idx]++;
				}

				const int32 NumDataPacks = CategoryOb.ReplicatedDataPacks.Num();
				SavedCategory.DataPacks.SetNum(NumDataPacks);
				CategoryState.DataPackStates.SetNum(NumDataPacks);
				for (int32 DataIdx = 0; DataIdx < CategoryOb.ReplicatedDataPacks.Num(); DataIdx++)
				{
					FGameplayDebuggerDataPack& DataPack = CategoryOb.ReplicatedDataPacks[DataIdx];
					const bool bHasOldStatePack = !bMissingOldState && OldState->CategoryStates[Idx].DataPackStates.IsValidIndex(DataIdx);

					if (DataPack.bNeedsConfirmation && !DataPack.bReceived && bHasOldStatePack)
					{
						FNetFastCategoryBaseState::FDataPackState& OldDataPackState = OldState->CategoryStates[Idx].DataPackStates[DataIdx];
						UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("Checking packet confirmation for Category[%d].DataPack[%d] OldState(DataVersion:%d DataOffset:%d complete:%s) current(DataVersion:%d DataOffset:%d)"),
							Idx, DataIdx,
							OldDataPackState.DataVersion, OldDataPackState.DataOffset,
							(OldDataPackState.DataOffset == DataPack.Header.DataSize) && (OldDataPackState.DataVersion == DataPack.Header.DataVersion) ? TEXT("yes") : TEXT("no"),
							DataPack.Header.DataVersion, DataPack.Header.DataOffset);

						DataPack.OnPacketRequest(OldDataPackState.DataVersion, OldDataPackState.DataOffset);
					}

					CategoryState.DataPackStates[DataIdx] = FNetFastCategoryBaseState::FDataPackState(DataPack.Header);
					const bool bDataPackChanged = (SavedCategory.DataPacks[DataIdx] != DataPack.Header);
					if (bDataPackChanged)
					{
						SavedCategory.DataPacks[DataIdx] = DataPack.Header;
						ChangedCategories[Idx]++;
					}
					else if (bHasOldStatePack)
					{
						FNetFastCategoryBaseState::FDataPackState& OldDataPackState = OldState->CategoryStates[Idx].DataPackStates[DataIdx];
						const bool bDataPackNotUpdatedOnClient = (OldDataPackState != DataPack.Header);
						if (bDataPackNotUpdatedOnClient)
						{
							ChangedCategories[Idx]++;
						}
					}
				}

				NumChangedCategories += ChangedCategories[Idx] ? 1 : 0;
			}
		}

		if (CVarGameplayDebuggerRepDetails.GetValueOnAnyThread())
		{
			if (OldState)
			{
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("NetDeltaSerialize DUMP OldState"));
				OldState->DumpToLog();
			}
			if (NewState)
			{
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("NetDeltaSerialize DUMP NewState"));
				NewState->DumpToLog();
			}
		}

		if (NumChangedCategories == 0)
		{
			return false;
		}

		int32 CategoryCount = SavedData.Num();
		Writer << CategoryCount;

		UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("NetDeltaSerialize WRITE START, OldState:%d"), OldState ? 1 : 0);
		for (int32 Idx = 0; Idx < SavedData.Num(); Idx++)
		{
			FGameplayDebuggerCategory& CategoryOb = Owner->Categories[Idx].Get();
			const bool bMissingOldState = (OldState == nullptr) || !OldState->CategoryStates.IsValidIndex(Idx);
			FCategoryData& SavedCategory = SavedData[Idx];

			uint8 BaseFlags = SavedCategory.bIsEnabled;
			uint8 ShouldUpdateTextLines = bMissingOldState || (OldState->CategoryStates[Idx].TextLinesRepCounter != NewState->CategoryStates[Idx].TextLinesRepCounter);
			uint8 ShouldUpdateShapes = bMissingOldState || (OldState->CategoryStates[Idx].ShapesRepCounter != NewState->CategoryStates[Idx].ShapesRepCounter);
			uint8 NumDataPacks = SavedCategory.DataPacks.Num();

			Writer.WriteBit(BaseFlags);
			Writer.WriteBit(ShouldUpdateTextLines);
			Writer.WriteBit(ShouldUpdateShapes);
			Writer << NumDataPacks;

			if (ChangedCategories[Idx])
			{
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  CATEGORY[%d]:%s"), Idx, *CategoryOb.GetCategoryName().ToString());
			}

			if (ShouldUpdateTextLines)
			{
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  >> replicate lines"));
				Writer << SavedCategory.TextLines;
			}

			if (ShouldUpdateShapes)
			{
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  >> replicate shapes"));
				Writer << SavedCategory.Shapes;
			}

			for (int32 DataIdx = 0; DataIdx < NumDataPacks; DataIdx++)
			{
				uint8 ShouldUpdateDataPack = bMissingOldState || !OldState->CategoryStates[Idx].DataPackStates.IsValidIndex(DataIdx) ||
					(OldState->CategoryStates[Idx].DataPackStates[DataIdx] != NewState->CategoryStates[Idx].DataPackStates[DataIdx]);

				Writer.WriteBit(ShouldUpdateDataPack);
				if (ShouldUpdateDataPack)
				{
					// send single packet
					FGameplayDebuggerDataPack& DataPack = CategoryOb.ReplicatedDataPacks[DataIdx];

					uint8 IsCompressed = DataPack.Header.bIsCompressed ? 1 : 0;
					Writer.WriteBit(IsCompressed);

					Writer << DataPack.Header.DataVersion;
					Writer << DataPack.Header.SyncCounter;
					Writer << DataPack.Header.DataSize;
					Writer << DataPack.Header.DataOffset;

					const int32 PacketSize = FMath::Min(FGameplayDebuggerDataPack::PacketSize, DataPack.Header.DataSize - DataPack.Header.DataOffset);
					if (PacketSize > 0)
					{
						Writer.Serialize(DataPack.Data.GetData() + DataPack.Header.DataOffset, PacketSize);
					}

					UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  >> replicate data pack[%d] progress:%0.f%% (offset:%d packet:%d)"),
						DataIdx, DataPack.Header.DataSize ? (100.0f * (DataPack.Header.DataOffset + PacketSize) / DataPack.Header.DataSize) : 100.0f,
						DataPack.Header.DataOffset, PacketSize);
				}
			}
		}
	}
	else if (DeltaParms.Reader)
	{
		FBitReader& Reader = *DeltaParms.Reader;
		UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("NetDeltaSerialize READ START"));

		int32 CategoryCount = 0;
		Reader << CategoryCount;

		if (CategoryCount != Owner->Categories.Num())
		{
			UE_LOG(LogGameplayDebugReplication, Error, TEXT("Category count mismtach! received:%d expected:%d"), CategoryCount, Owner->Categories.Num());
			Reader.SetError();
			return false;
		}

		bool bHasCategoryStateChanges = false;
		for (int32 Idx = 0; Idx < CategoryCount; Idx++)
		{
			FGameplayDebuggerCategory& CategoryOb = Owner->Categories[Idx].Get();
			UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  CATEGORY[%d]:%s"), Idx, *CategoryOb.GetCategoryName().ToString());

			uint8 BaseFlags = Reader.ReadBit();
			uint8 ShouldUpdateTextLines = Reader.ReadBit();
			uint8 ShouldUpdateShapes = Reader.ReadBit();

			uint8 NumDataPacks = 0;
			Reader << NumDataPacks;

			if ((int32)NumDataPacks != CategoryOb.ReplicatedDataPacks.Num())
			{
				UE_LOG(LogGameplayDebugReplication, Error, TEXT("Data pack count mismtach! received:%d expected:%d"), NumDataPacks, CategoryOb.ReplicatedDataPacks.Num());
				Reader.SetError();
				return false;
			}

			const bool bNewCategoryEnabled = (BaseFlags != 0);
			bHasCategoryStateChanges = bHasCategoryStateChanges || (CategoryOb.bIsEnabled != bNewCategoryEnabled);
			CategoryOb.bIsEnabled = bNewCategoryEnabled;

			if (ShouldUpdateTextLines)
			{
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  >> received lines"));
				Reader << CategoryOb.ReplicatedLines;
			}

			if (ShouldUpdateShapes)
			{
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  >> received shapes"));
				Reader << CategoryOb.ReplicatedShapes;
			}

			for (uint8 DataIdx = 0; DataIdx < NumDataPacks; DataIdx++)
			{
				uint8 ShouldUpdateDataPack = Reader.ReadBit();

				if (ShouldUpdateDataPack)
				{
					// receive single packet
					FGameplayDebuggerDataPack DataPacket;

					uint8 IsCompressed = Reader.ReadBit();
					DataPacket.Header.bIsCompressed = (IsCompressed != 0);

					Reader << DataPacket.Header.DataVersion;
					Reader << DataPacket.Header.SyncCounter;
					Reader << DataPacket.Header.DataSize;
					Reader << DataPacket.Header.DataOffset;

					const int32 PacketSize = FMath::Min(FGameplayDebuggerDataPack::PacketSize, DataPacket.Header.DataSize - DataPacket.Header.DataOffset);
					if (PacketSize > 0)
					{
						DataPacket.Data.AddUninitialized(PacketSize);
						Reader.Serialize(DataPacket.Data.GetData(), PacketSize);
					}

					Owner->OnReceivedDataPackPacket(Idx, DataIdx, DataPacket);
					UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("  >> replicate data pack[%d] progress:%.0f%%"), DataIdx, CategoryOb.ReplicatedDataPacks[DataIdx].GetProgress() * 100.0f);
				}
			}
		}

		// force scene proxy updates if categories changed state
		if (bHasCategoryStateChanges)
		{
			Owner->MarkComponentsRenderStateDirty();
		}
	}

	return true;
}

void FGameplayDebuggerNetPack::OnCategoriesChanged()
{
	SavedData.Reset();
	SavedData.SetNum(Owner->Categories.Num());
}

//////////////////////////////////////////////////////////////////////////
// AGameplayDebuggerCategoryReplicator

AGameplayDebuggerCategoryReplicator::AGameplayDebuggerCategoryReplicator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

#if WITH_EDITOR
	SetIsTemporarilyHiddenInEditor(true);
#endif

#if WITH_EDITORONLY_DATA
	bHiddenEdLevel = true;
	bHiddenEdLayer = true;
	bHiddenEd = true;
	bEditable = false;
#endif

	bIsEnabled = false;
	bIsEnabledLocal = false;
	bReplicates = true;

	ReplicatedData.Owner = this;
}

void AGameplayDebuggerCategoryReplicator::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	const ENetMode NetMode = World->GetNetMode();
	bHasAuthority = (NetMode != NM_Client);
	bIsLocal = (NetMode != NM_DedicatedServer);

	FGameplayDebuggerAddonManager& AddonManager = FGameplayDebuggerAddonManager::GetCurrent();
	AddonManager.OnCategoriesChanged.AddUObject(this, &AGameplayDebuggerCategoryReplicator::OnCategoriesChanged);
	AddonManager.OnExtensionsChanged.AddUObject(this, &AGameplayDebuggerCategoryReplicator::OnExtensionsChanged);

	OnCategoriesChanged();
	OnExtensionsChanged();

	AGameplayDebuggerPlayerManager& PlayerManager = AGameplayDebuggerPlayerManager::GetCurrent(GetWorld());
	PlayerManager.RegisterReplicator(*this);

	SetActorHiddenInGame(!bIsLocal);
	if (bIsLocal)
	{
		RenderingComp = NewObject<UGameplayDebuggerRenderingComponent>(this, TEXT("RenderingComp"));
		RenderingComp->RegisterComponentWithWorld(World);
		RootComponent = RenderingComp;
	}
	
	if (bHasAuthority)
	{
		SetEnabled(FGameplayDebuggerAddonBase::IsSimulateInEditor());
	}
}

void AGameplayDebuggerCategoryReplicator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Disable extensions to clear UI state
	NotifyCategoriesToolState(false);
	NotifyExtensionsToolState(false);

	FGameplayDebuggerAddonManager& AddonManager = FGameplayDebuggerAddonManager::GetCurrent();
	AddonManager.OnCategoriesChanged.RemoveAll(this);
	AddonManager.OnExtensionsChanged.RemoveAll(this);
}

void AGameplayDebuggerCategoryReplicator::OnCategoriesChanged()
{
	FGameplayDebuggerAddonManager& AddonManager = FGameplayDebuggerAddonManager::GetCurrent();
	AddonManager.CreateCategories(*this, Categories);

	ReplicatedData.OnCategoriesChanged();

	if (bIsLocal)
	{
		AGameplayDebuggerPlayerManager& PlayerManager = AGameplayDebuggerPlayerManager::GetCurrent(GetWorld());
		PlayerManager.RefreshInputBindings(*this);
	}
}

void AGameplayDebuggerCategoryReplicator::OnExtensionsChanged()
{
	FGameplayDebuggerAddonManager& AddonManager = FGameplayDebuggerAddonManager::GetCurrent();
	AddonManager.CreateExtensions(*this, Extensions);

	if (bIsLocal)
	{
		AGameplayDebuggerPlayerManager& PlayerManager = AGameplayDebuggerPlayerManager::GetCurrent(GetWorld());
		PlayerManager.RefreshInputBindings(*this);
	}
}

UNetConnection* AGameplayDebuggerCategoryReplicator::GetNetConnection() const
{
	return IsValid(OwnerPC) ? OwnerPC->GetNetConnection() : nullptr;
}

bool AGameplayDebuggerCategoryReplicator::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	return (RealViewer == OwnerPC);
}

/// @cond DOXYGEN_WARNINGS

void AGameplayDebuggerCategoryReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGameplayDebuggerCategoryReplicator, OwnerPC);
	DOREPLIFETIME(AGameplayDebuggerCategoryReplicator, DebugActor);
	DOREPLIFETIME(AGameplayDebuggerCategoryReplicator, bIsEnabled);
	DOREPLIFETIME(AGameplayDebuggerCategoryReplicator, ReplicatedData);
}

bool AGameplayDebuggerCategoryReplicator::ServerSetEnabled_Validate(bool bEnable)
{
	return true;
}

void AGameplayDebuggerCategoryReplicator::ServerSetEnabled_Implementation(bool bEnable)
{
	SetEnabled(bEnable);
}

bool AGameplayDebuggerCategoryReplicator::ServerSetDebugActor_Validate(AActor* Actor)
{
	return true;
}

void AGameplayDebuggerCategoryReplicator::ServerSetDebugActor_Implementation(AActor* Actor)
{
	SetDebugActor(Actor);
}

bool AGameplayDebuggerCategoryReplicator::ServerSetCategoryEnabled_Validate(int32 CategoryId, bool bEnable)
{
	return true;
}

void AGameplayDebuggerCategoryReplicator::ServerSetCategoryEnabled_Implementation(int32 CategoryId, bool bEnable)
{
	SetCategoryEnabled(CategoryId, bEnable);
}

bool AGameplayDebuggerCategoryReplicator::ServerSendCategoryInputEvent_Validate(int32 CategoryId, int32 HandlerId)
{
	return true;
}

void AGameplayDebuggerCategoryReplicator::ServerSendCategoryInputEvent_Implementation(int32 CategoryId, int32 HandlerId)
{
	SendCategoryInputEvent(CategoryId, HandlerId);
}

bool AGameplayDebuggerCategoryReplicator::ServerSendExtensionInputEvent_Validate(int32 ExtensionId, int32 HandlerId)
{
	return true;
}

void AGameplayDebuggerCategoryReplicator::ServerSendExtensionInputEvent_Implementation(int32 ExtensionId, int32 HandlerId)
{
	SendExtensionInputEvent(ExtensionId, HandlerId);
}

/// @endcond

void AGameplayDebuggerCategoryReplicator::OnReceivedDataPackPacket(int32 CategoryId, int32 DataPackId, const FGameplayDebuggerDataPack& DataPacket)
{
	if (Categories.IsValidIndex(CategoryId) && Categories[CategoryId]->ReplicatedDataPacks.IsValidIndex(DataPackId))
	{
		FGameplayDebuggerDataPack& DataPack = Categories[CategoryId]->ReplicatedDataPacks[DataPackId];
		bool bIsPacketValid = false;

		if (DataPack.Header.DataVersion != DataPacket.Header.DataVersion)
		{
			// new content of data pack:
			if (DataPacket.Header.DataOffset == 0)
			{
				// first packet of data, replace old data pack's intermediate data
				DataPack.Header = DataPacket.Header;
				DataPack.Data = DataPacket.Data;
				bIsPacketValid = true;
			}
			else
			{
				// somewhere in the middle, discard packet
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("Error: received packet from the middle of content with different version, discarding! (category[%d]:%s, packet.DataVersion:%d packet.DataOffset:%d, data[%d].DataVersion:%d)"),
					CategoryId, *Categories[CategoryId]->GetCategoryName().ToString(),
					DataPacket.Header.DataVersion,
					DataPacket.Header.DataOffset,
					DataPackId, DataPack.Header.DataVersion,
					DataPackId, DataPack.Header.DataOffset);
			}
		}
		else if (DataPack.Data.Num() < DataPacket.Header.DataSize)
		{
			// another packet for existing data pack
			if (DataPacket.Header.DataOffset == DataPack.Data.Num())
			{
				// offset matches, this is next expected packet
				DataPack.Data.Append(DataPacket.Data);
				DataPack.Header.DataOffset = DataPack.Data.Num();
				bIsPacketValid = true;
			}
			else
			{
				// offset mismatch, discard packet
				UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("Error: received packet doesn't match expected chunk, discarding! (category[%d]:%s, packet.DataOffset:%d, data[%d].DataOffset:%d data[%d].Data.Num:%d)"),
					CategoryId, *Categories[CategoryId]->GetCategoryName().ToString(),
					DataPacket.Header.DataOffset,
					DataPackId, DataPack.Header.DataOffset,
					DataPackId, DataPack.Data.Num());
			}
		}

		// check if data pack is now complete
		if (bIsPacketValid && (DataPack.Data.Num() == DataPack.Header.DataSize))
		{
			// complete
			UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("Category[%d].DataPack[%d] RECEIVED, DataVersion:%d DataSize:%d SyncCounter:%d"),
				CategoryId, DataPackId, DataPack.Header.DataVersion, DataPack.Header.DataSize, DataPack.Header.SyncCounter);

			DataPack.OnReplicated();
			Categories[CategoryId]->OnDataPackReplicated(DataPackId);
		}
	}
}

void AGameplayDebuggerCategoryReplicator::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	CollectCategoryData();
}

void AGameplayDebuggerCategoryReplicator::PostNetReceive()
{
	Super::PostNetReceive();

	// force scene proxy updates if tool changed state
	if (bIsEnabled != bIsEnabledLocal)
	{
		bIsEnabledLocal = bIsEnabled;
		MarkComponentsRenderStateDirty();
	}
}

void AGameplayDebuggerCategoryReplicator::CollectCategoryData(bool bForce)
{
	const float GameTime = GetWorld()->GetTimeSeconds();
	for (int32 Idx = 0; Idx < Categories.Num(); Idx++)
	{
		FGameplayDebuggerCategory& CategoryOb = Categories[Idx].Get();
		if (CategoryOb.bHasAuthority && CategoryOb.bIsEnabled && (bForce || (GameTime - CategoryOb.LastCollectDataTime) > CategoryOb.CollectDataInterval))
		{
			// prepare data packs before calling CollectData
			for (int32 DataPackIdx = 0; DataPackIdx < CategoryOb.ReplicatedDataPacks.Num(); DataPackIdx++)
			{
				FGameplayDebuggerDataPack& DataPack = CategoryOb.ReplicatedDataPacks[DataPackIdx];
				DataPack.bIsDirty = false;

				if ((DataPack.Flags == EGameplayDebuggerDataPack::ResetOnTick) ||
					(DataPack.Flags == EGameplayDebuggerDataPack::ResetOnActorChange && DataPack.Header.SyncCounter != DebugActor.SyncCounter))
				{
					DataPack.ResetDelegate.Execute();
				}
			}

			CategoryOb.ReplicatedLines.Reset();
			CategoryOb.ReplicatedShapes.Reset();

			CategoryOb.CollectData(OwnerPC, DebugActor.Actor);
			CategoryOb.LastCollectDataTime = GameTime;

			// update dirty data packs
			for (int32 DataPackIdx = 0; DataPackIdx < CategoryOb.ReplicatedDataPacks.Num(); DataPackIdx++)
			{
				FGameplayDebuggerDataPack& DataPack = CategoryOb.ReplicatedDataPacks[DataPackIdx];
				if (CategoryOb.bIsLocal)
				{
					const bool bWasDirty = DataPack.CheckDirtyAndUpdate();
					if (bWasDirty)
					{
						CategoryOb.OnDataPackReplicated(DataPackIdx);
					}

					if (CategoryOb.bHasAuthority)
					{
						// update sync counter for local & auth packs (no data replication), otherwise they can be reset
						DataPack.Header.SyncCounter = DebugActor.SyncCounter;
					}
				}
				else
				{
					const bool bWasDirty = DataPack.RequestReplication(DebugActor.SyncCounter);
					if (bWasDirty)
					{
						UE_LOG(LogGameplayDebugReplication, Verbose, TEXT("Category[%d].DataPack[%d] SENT, DataVersion:%d DataSize:%d SyncCounter:%d"),
							Idx, DataPackIdx, DataPack.Header.DataVersion, DataPack.Header.DataSize, DataPack.Header.SyncCounter);
					}
				}
			}
		}
	}
}

void AGameplayDebuggerCategoryReplicator::SetReplicatorOwner(APlayerController* InOwnerPC)
{
	if (!bIsEnabled)
	{
		// can't use bHasAuthority, BeginPlay was not called yet
		UWorld* World = GetWorld();
		const ENetMode NetMode = World->GetNetMode();

		if (NetMode != NM_Client)
		{
			OwnerPC = InOwnerPC;
		}
	}
}

void AGameplayDebuggerCategoryReplicator::SetEnabled(bool bEnable)
{
	if (bHasAuthority)
	{
		bIsEnabled = bEnable;
		bIsEnabledLocal = bEnable;
		SetActorTickEnabled(bEnable);
	}
	else
	{
		ServerSetEnabled(bEnable);
	}

	MarkComponentsRenderStateDirty();
	NotifyCategoriesToolState(bEnable);

	// extensions will NOT work with simulate mode, they are meant to handle additional input
	const bool bEnableExtensions = bEnable && !FGameplayDebuggerAddonBase::IsSimulateInEditor();
	NotifyExtensionsToolState(bEnableExtensions);
}

void AGameplayDebuggerCategoryReplicator::SetDebugActor(AActor* Actor)
{
	UE_LOG(LogGameplayDebugReplication, Log, TEXT("SetDebugActor %s"), *GetNameSafe(Actor));
	if (bHasAuthority)
	{
		if (DebugActor.Actor != Actor)
		{
			DebugActor.Actor = Actor;
			DebugActor.ActorName = Actor ? Actor->GetFName() : NAME_None;
			DebugActor.SyncCounter++;
		}
	}
	else
	{
		ServerSetDebugActor(Actor);
	}
}

void AGameplayDebuggerCategoryReplicator::SetCategoryEnabled(int32 CategoryId, bool bEnable)
{
	if (bHasAuthority)
	{
		if (Categories.IsValidIndex(CategoryId))
		{
			UE_LOG(LogGameplayDebugReplication, Log, TEXT("SetCategoryEnabled[%d]:%d (%s)"), CategoryId, bEnable ? 1 : 0, *Categories[CategoryId]->GetCategoryName().ToString());
			Categories[CategoryId]->bIsEnabled = bEnable;
		}
	}
	else
	{
		ServerSetCategoryEnabled(CategoryId, bEnable);
	}

	MarkComponentsRenderStateDirty();
}

void AGameplayDebuggerCategoryReplicator::SendCategoryInputEvent(int32 CategoryId, int32 HandlerId)
{
	if (HandlerId >= 0 && Categories.IsValidIndex(CategoryId) &&
		HandlerId < Categories[CategoryId]->GetNumInputHandlers())
	{
		// check enabled category only on local (instigating) side
		if (!bIsLocal || IsCategoryEnabled(CategoryId))
		{
			FGameplayDebuggerInputHandler& InputHandler = Categories[CategoryId]->GetInputHandler(HandlerId);
			if (InputHandler.Mode == EGameplayDebuggerInputMode::Local || bHasAuthority)
			{
				InputHandler.Delegate.ExecuteIfBound();
			}
			else
			{
				ServerSendCategoryInputEvent(CategoryId, HandlerId);
			}
		}
	}
}

void AGameplayDebuggerCategoryReplicator::SendExtensionInputEvent(int32 ExtensionId, int32 HandlerId)
{
	if (HandlerId >= 0 && Extensions.IsValidIndex(ExtensionId) &&
		HandlerId < Extensions[ExtensionId]->GetNumInputHandlers())
	{
		FGameplayDebuggerInputHandler& InputHandler = Extensions[ExtensionId]->GetInputHandler(HandlerId);
		if (InputHandler.Mode == EGameplayDebuggerInputMode::Local || bHasAuthority)
		{
			InputHandler.Delegate.ExecuteIfBound();
		}
		else
		{
			ServerSendExtensionInputEvent(ExtensionId, HandlerId);
		}
	}
}

void AGameplayDebuggerCategoryReplicator::NotifyCategoriesToolState(bool bIsActive)
{
	for (int32 Idx = 0; Idx < Categories.Num(); Idx++)
	{
		FGameplayDebuggerCategory& CategoryOb = Categories[Idx].Get();
		if (bIsActive)
		{
			CategoryOb.OnGameplayDebuggerActivated();
		}
		else
		{
			CategoryOb.OnGameplayDebuggerDeactivated();
		}
	}
}

void AGameplayDebuggerCategoryReplicator::NotifyExtensionsToolState(bool bIsActive)
{
	for (int32 Idx = 0; Idx < Extensions.Num(); Idx++)
	{
		FGameplayDebuggerExtension& ExtensionOb = Extensions[Idx].Get();
		if (bIsActive)
		{
			ExtensionOb.OnGameplayDebuggerActivated();
		}
		else
		{
			ExtensionOb.OnGameplayDebuggerDeactivated();
		}
	}
}

bool AGameplayDebuggerCategoryReplicator::IsCategoryEnabled(int32 CategoryId) const
{
	return Categories.IsValidIndex(CategoryId) && Categories[CategoryId]->IsCategoryEnabled(); 
}
