// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneEventSection.h"
#include "EngineGlobals.h"
#include "IMovieScenePlayer.h"
#include "ReleaseObjectVersion.h"
#include "LinkerLoad.h"
#include "Serialization/MemoryArchive.h"

#include "Curves/KeyFrameAlgorithms.h"

/* Custom version specifically for event parameter struct serialization (serialized into FMovieSceneEventParameters::StructBytes) */
namespace EEventParameterVersion
{
	enum Type
	{
		// First version, serialized with either FMemoryWriter or FEventParameterWriter (both are compatible with FEventParameterReader)
		First = 0,

		// -------------------------------------------------------------------
		LastPlusOne,
		LatestVersion = LastPlusOne - 1
	};
}

namespace
{
	/* Register the custom version so that we can easily make changes to this serialization in future */
	const FGuid EventParameterVersionGUID(0x509D354F, 0xF6E6492F, 0xA74985B2, 0x073C631C);
	FCustomVersionRegistration GRegisterEventParameterVersion(EventParameterVersionGUID, EEventParameterVersion::LatestVersion, TEXT("EventParameter"));

	/** Magic number that is always added to the start of a serialized event parameter to signify that it has a custom version header. Absense implies no custom version (before version info was added) */
	static const uint32 VersionMagicNumber = 0xA1B2C3D4;
}

/** Custom archive overloads for serializing event struct parameter payloads */
class FEventParameterArchive : public FMemoryArchive
{
public:
	/** Serialize a string asset reference */
	virtual FArchive& operator<<(FStringAssetReference& AssetPtr) override
	{
		AssetPtr.SerializePath(*this);
		return *this;
	}

	/** Serialize an asset ptr */
	virtual FArchive& operator<<(FSoftObjectPtr& AssetPtr) override
	{
		FSoftObjectPath Ref = AssetPtr.ToSoftObjectPath();
		*this << Ref;

		if (IsLoading())
		{
			AssetPtr = FSoftObjectPtr(Ref);
		}

		return *this;
	}

	// Unsupported serialization
	virtual FArchive& operator<<(UObject*& Res) override 					{ ArIsError = true; return *this; }
	virtual FArchive& operator<<(FLazyObjectPtr& LazyObjectPtr) override 	{ ArIsError = true; return *this; }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override 			{ ArIsError = true; return *this; }
};

/** Custom archive used for writing event parameter struct payloads */
class FEventParameterWriter : public FEventParameterArchive
{
public:
	/** Constructor from a destination byte array */
	FEventParameterWriter(TArray<uint8>& InBytes)
		: Bytes(InBytes)
	{
		ArIsSaving = true;
		ArIsPersistent = true;
		UsingCustomVersion(EventParameterVersionGUID);
	}

	/**
	 * Write the specified source ptr (an instance of StructPtr) into the destination byte array
	 * @param StructPtr 	The struct representing the type of Source
	 * @param Source 		Source pointer to the instance of the payload to write
	 */
	void Write(UStruct* StructPtr, uint8* Source)
	{
		FArchive& Ar = *this;

		// Write the magic number to signify that we have the custom version info
		uint32 Magic = VersionMagicNumber;
		Ar << Magic;

		// Store the position of the serialized CVOffset
		int64 CVOffsetPos = Tell();

		int32 CVOffset = 0;
		Ar << CVOffset;

		// Write the struct itself
		StructPtr->SerializeTaggedProperties(Ar, Source, StructPtr, nullptr);

		CVOffset = Tell();

		// Write the custom version info at the end (it may have changed as a result of SerializeTaggedProperties if they use custom versions)
		FCustomVersionContainer CustomVersions = GetCustomVersions();
		CustomVersions.Serialize(*this);

		// Seek back to the offset pos, and write the custom version info offset
		Seek(CVOffsetPos);
		Ar << CVOffset;
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("FEventParameterWriter");
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		const int64 NumBytesToAdd = Offset + Num - Bytes.Num();
		if (NumBytesToAdd > 0)
		{
			const int64 NewArrayCount = Bytes.Num() + NumBytesToAdd;
			check(NewArrayCount < MAX_int32);

			Bytes.AddUninitialized( (int32)NumBytesToAdd );
		}

		if (Num)
		{
			FMemory::Memcpy(&Bytes[Offset], Data, Num);
			Offset += Num;
		}
	}

private:
	TArray<uint8>& Bytes;
};

class FEventParameterReader : public FEventParameterArchive
{
public:
	FEventParameterReader(const TArray<uint8>& InBytes)
		: Bytes(InBytes)
	{
		ArIsLoading = true;
		UsingCustomVersion(EventParameterVersionGUID);
	}

	/**
	 * Read the source data buffer as a StructPtr type, into the specified destination instance
	 * @param StructPtr 	The struct representing the type of Dest
	 * @param Dest 			Destination instance to receive the deserialized data
	 */
	void Read(UStruct* StructPtr, uint8* Dest)
	{
		bool bHasCustomVersion = false;
		// Optionally deserialize the custom version header, provided it was serialized
		if (Bytes.Num() >= 8)
		{
			uint32 Magic = 0;
			*this << Magic;

			if (Magic == VersionMagicNumber)
			{
				int32 CVOffset = 0;
				*this << CVOffset;

				int64 DataStartPos = Tell();
				Seek(CVOffset);

				// Read the custom version info
				FCustomVersionContainer CustomVersions;
				CustomVersions.Serialize(*this);
				SetCustomVersions(CustomVersions);

				// Seek back to the start of the struct data
				Seek(DataStartPos);

				bHasCustomVersion = true;
			}
		}

		if (!bHasCustomVersion)
		{
			// Force the very first custom version
			SetCustomVersion(EventParameterVersionGUID, EEventParameterVersion::First, "EventParameter");
			// The magic number was not valid, so ensure we're right at the start (this data pre-dates the custom version info)
			Seek(0);
		}

		// Serialize the struct itself
		StructPtr->SerializeTaggedProperties(*this, Dest, StructPtr, nullptr);
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("FEventParameterReader");
	}

	virtual void Serialize(void* Data, int64 Num) override
	{
		if (Num && !ArIsError)
		{
			// Only serialize if we have the requested amount of data
			if (Offset + Num <= Bytes.Num())
			{
				FMemory::Memcpy(Data, &Bytes[Offset], Num);
				Offset += Num;
			}
			else
			{
				ArIsError = true;
			}
		}
	}

private:
	const TArray<uint8>& Bytes;
};

void FMovieSceneEventParameters::OverwriteWith(uint8* InstancePtr)
{
	check(InstancePtr);

	if (UStruct* StructPtr = GetStructType())
	{
		FEventParameterWriter(StructBytes).Write(StructPtr, InstancePtr);
	}
	else
	{
		StructBytes.Reset();
	}
}

void FMovieSceneEventParameters::GetInstance(FStructOnScope& OutStruct) const
{
	UStruct* StructPtr = GetStructType();
	OutStruct.Initialize(StructPtr);
	uint8* Memory = OutStruct.GetStructMemory();
	if (StructPtr && StructPtr->GetStructureSize() > 0 && StructBytes.Num())
	{
		// Deserialize the struct bytes into the struct memory
		FEventParameterReader(StructBytes).Read(StructPtr, Memory);
	}
}

bool FMovieSceneEventParameters::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	if (Ar.IsLoading() && Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::EventSectionParameterStringAssetRef)
	{
		UStruct* StructPtr = nullptr;
		Ar << StructPtr;
		StructType = StructPtr;
	}
	else
	{
		Ar << StructType;
	}
	
	Ar << StructBytes;

	return true;
}

/* UMovieSceneSection structors
 *****************************************************************************/

UMovieSceneEventSection::UMovieSceneEventSection()
#if WITH_EDITORONLY_DATA
	: CurveInterface(TCurveInterface<FEventPayload, float>(&EventData.KeyTimes, &EventData.KeyValues, &EventData.KeyHandles))
#else
	: CurveInterface(TCurveInterface<FEventPayload, float>(&EventData.KeyTimes, &EventData.KeyValues))
#endif
{
	SetIsInfinite(true);
}

void UMovieSceneEventSection::PostLoad()
{
	for (FNameCurveKey EventKey : Events_DEPRECATED.GetKeys())
	{
		EventData.KeyTimes.Add(EventKey.Time);
		EventData.KeyValues.Add(FEventPayload(EventKey.Value));
	}

	if (Events_DEPRECATED.GetKeys().Num())
	{
		MarkAsChanged();
	}

	Super::PostLoad();
}

/* UMovieSceneSection overrides
 *****************************************************************************/

void UMovieSceneEventSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	KeyFrameAlgorithms::Scale(CurveInterface.GetValue(), Origin, DilationFactor, KeyHandles);
}


void UMovieSceneEventSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles, TRange<float> TimeRange) const
{
	for (auto It(CurveInterface->IterateKeys()); It; ++It)
	{
		if (TimeRange.Contains(*It))
		{
			KeyHandles.Add(It.GetKeyHandle());
		}
	}
}


void UMovieSceneEventSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaPosition, KeyHandles);

	KeyFrameAlgorithms::Translate(CurveInterface.GetValue(), DeltaPosition, KeyHandles);
}


TOptional<float> UMovieSceneEventSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	return CurveInterface->GetKeyTime(KeyHandle);
}


void UMovieSceneEventSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	CurveInterface->SetKeyTime(KeyHandle, Time);
}
