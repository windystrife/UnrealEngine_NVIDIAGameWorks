// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsSerializer.cpp
=============================================================================*/ 

#include "PhysicsSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "PhysicsEngine/PhysDerivedData.h"

UPhysicsSerializer::UPhysicsSerializer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FByteBulkData* UPhysicsSerializer::GetBinaryData(FName Format, const TArray<FBodyInstance*>& Bodies, const TArray<class UBodySetup*>& BodySetups, const TArray<class UPhysicalMaterial*>& PhysicalMaterials)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("PhysxSerialization")))
	{
		return nullptr;
	}

#if PLATFORM_MAC
	return nullptr;	//This is not supported right now
#endif

	QUICK_SCOPE_CYCLE_COUNTER(STAT_GetBinaryData);
	const bool bContainedData = BinaryFormatData.Contains(Format);
	FByteBulkData* Result = &BinaryFormatData.GetFormat(Format);
	if (!FParse::Param(FCommandLine::Get(), TEXT("NoPhysxAlignment")))
	{
		Result->SetBulkDataAlignment(PHYSX_SERIALIZATION_ALIGNMENT);
	}
	
	if (!bContainedData)
	{
#if WITH_EDITOR
#if WITH_PHYSX
		TArray<uint8> OutData;
		// Changed from raw pointer to unique pointer to fix static analysis warning, but unclear if this code path is used anymore.
		TUniquePtr<FDerivedDataPhysXBinarySerializer> DerivedPhysXSerializer(new FDerivedDataPhysXBinarySerializer(Format, Bodies, BodySetups, PhysicalMaterials, FGuid::NewGuid())); //TODO: Maybe it's worth adding this to the DDC. For now there's a lot of complexity with the guid invalidation so I've left it out.
		if (DerivedPhysXSerializer->CanBuild())
		{

			DerivedPhysXSerializer->Build(OutData);
#endif
			if (OutData.Num())
			{
				Result->Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Result->Realloc(OutData.Num()), OutData.GetData(), OutData.Num());
				Result->Unlock();
			}
		}
		else
#endif
		{
			UE_LOG(LogPhysics, Warning, TEXT("Attempt to use binary physics data but we are unable to."));
		}
	}

	return Result->GetBulkDataSize() > 0 ? Result : nullptr;
}

void UPhysicsSerializer::Serialize( FArchive& Ar )
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_Serialize);
	Super::Serialize(Ar);
	if (Ar.UE4Ver() >= VER_UE4_BODYINSTANCE_BINARY_SERIALIZATION)
	{
		bool bCooked = Ar.IsCooking();
		Ar << bCooked;

		if (bCooked)
		{
			if (Ar.IsCooking())
			{
				TArray<FName> ActualFormatsToSave;
				ActualFormatsToSave.Add(FPlatformProperties::GetPhysicsFormat());
				BinaryFormatData.Serialize(Ar, this, &ActualFormatsToSave);
			}
			else
			{
#if WITH_PHYSX
				const uint32 Alignment = PHYSX_SERIALIZATION_ALIGNMENT;
#else
				const uint32 Alignment = DEFAULT_ALIGNMENT;
#endif
				BinaryFormatData.Serialize(Ar, this, nullptr, false, Alignment);
			}
		}
	}
}

void UPhysicsSerializer::SerializePhysics(const TArray<FBodyInstance*>& Bodies, const TArray<UBodySetup*>& BodySetups, const TArray<UPhysicalMaterial*>& PhysicalMaterials)
{
#if WITH_EDITOR
	GetBinaryData(FPlatformProperties::GetPhysicsFormat(), Bodies, BodySetups, PhysicalMaterials);	//build the binary data
#endif
}

void UPhysicsSerializer::CreatePhysicsData(const TArray<UBodySetup*>& BodySetups, const TArray<UPhysicalMaterial*>& PhysicalMaterials)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("PhysxSerialization")))
	{
		return;
	}

#if PLATFORM_MAC
	return;	//This is not supported right now
#endif

#if !WITH_EDITOR
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PhysicsSerializer_CreatePhysicsData);

	const FName Format(FPlatformProperties::GetPhysicsFormat());
	if(BinaryFormatData.Contains(Format))
	{
		FByteBulkData& BinaryData = BinaryFormatData.GetFormat(Format);
#if WITH_PHYSX
		// Read serialized physics data
		uint8* SerializedData = (uint8*)BinaryData.Lock(LOCK_READ_ONLY);

		FBufferReader Ar(SerializedData, BinaryData.GetBulkDataSize(), false );
		uint8 bIsLittleEndian;
		uint64 BaseId;	//the starting index of the shared resources we didn't serialize out
		Ar << bIsLittleEndian;
		Ar.SetByteSwapping( PLATFORM_LITTLE_ENDIAN ? !bIsLittleEndian : !!bIsLittleEndian );
		Ar << BaseId;
		//Note that PhysX expects the binary data to be 128-byte aligned. Because of this we've padded so find the next spot in memory
		int32 BytesToPad = PHYSX_SERIALIZATION_ALIGNMENT - (Ar.Tell() % PHYSX_SERIALIZATION_ALIGNMENT);
		
		physx::PxSerializationRegistry* PRegistry =  PxSerialization::createSerializationRegistry(*GPhysXSDK);
		physx::PxCollection* PCollection = nullptr;
		PxCollection* PExternalData = MakePhysXCollection(PhysicalMaterials, BodySetups, BaseId);		
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_DeserializePhysics);
			PCollection = PxSerialization::createCollectionFromBinary(SerializedData + Ar.Tell() + BytesToPad, *PRegistry, PExternalData);	
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AddBodiesToMap);
			const uint32 NumObjects = PCollection->getNbObjects();
			for (uint32 ObjectIdx = 0; ObjectIdx < NumObjects; ++ObjectIdx)
			{
				PxBase& PObject = PCollection->getObject(ObjectIdx);
				if (PxRigidActor* PRigidActor = PObject.is<PxRigidActor>())
				{
					const PxSerialObjectId ObjectId = PCollection->getId(PObject);
					ActorsMap.Add(ObjectId, PRigidActor);
				}
				else if (PxShape* PShape = PObject.is<PxShape>())
				{
					PShape->release();	//we do not need to hold on to this reference because our actors always have the reference directly
				}
			}
		}

		PExternalData->release();
		PCollection->release();
		PRegistry->release();
#endif
	}else
	{
#if !WITH_EDITOR
		UE_LOG(LogPhysics, Warning, TEXT("PhysicsSerializer has no binary data. Body instances will fall back to slow creation path."));
#endif
	}
#endif
}

void UPhysicsSerializer::BeginDestroy()
{
	const FName Format(FPlatformProperties::GetPhysicsFormat());
	if (BinaryFormatData.Contains(Format))
	{
		FByteBulkData& BinaryData = BinaryFormatData.GetFormat(Format);
		if(BinaryData.IsLocked())
		{
			BinaryData.Unlock();
		}
	}

	Super::BeginDestroy();
}

#if WITH_PHYSX
PxRigidActor* UPhysicsSerializer::GetRigidActor(uint64 ObjectId) const
{
	PxRigidActor* const* PActor = ActorsMap.Find(ObjectId);

	return PActor ? *PActor : nullptr;
}
#endif
