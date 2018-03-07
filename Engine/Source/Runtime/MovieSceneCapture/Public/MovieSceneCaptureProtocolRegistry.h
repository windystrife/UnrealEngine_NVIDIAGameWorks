// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "IMovieSceneCaptureProtocol.h"
#include "MovieSceneCaptureProtocolSettings.h"
#include "MovieSceneCaptureProtocolRegistry.generated.h"

/** Structure used to uniquely identify a specific capture protocol */
USTRUCT()
struct FCaptureProtocolID
{
	FCaptureProtocolID(){}
	FCaptureProtocolID(const TCHAR* InName) : Identifier(InName) {}

	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=ID)
	FName Identifier;
};

inline bool operator==(FCaptureProtocolID A, FCaptureProtocolID B){ return A.Identifier == B.Identifier; }
inline bool operator!=(FCaptureProtocolID A, FCaptureProtocolID B){ return A.Identifier != B.Identifier; }
inline uint32 GetTypeHash(FCaptureProtocolID ID){ return GetTypeHash(ID.Identifier); }

/** Structure that defines a particular capture protocol */
struct FMovieSceneCaptureProtocolInfo
{
	/** This protocol's display name */
	FText DisplayName;
	/** Factory function called to create a new instance of this protocol */
	TFunction<TSharedRef<IMovieSceneCaptureProtocol>()> Factory;
	/** Custom settings class type to use for this protocol */
	UClass* SettingsClassType;
};

/** Class responsible for maintaining a list of available capture protocols */
class FMovieSceneCaptureProtocolRegistry
{
public:

	/** Check whether the specified ID corresponds to a valid protocol */
	bool IsValidProtocol(FCaptureProtocolID InProtocolID)
	{
		return Register.Contains(InProtocolID);
	}

	/** Iterate alll the protocols we are currently aware of */
	void IterateProtocols(TFunctionRef<void(FCaptureProtocolID, const FMovieSceneCaptureProtocolInfo&)> Iter)
	{
		for (auto& Pair : Register)
		{
			Iter(Pair.Key, Pair.Value);
		}
	}

	/** Create a new settings type for the specified protocol ID */
	UMovieSceneCaptureProtocolSettings* FactorySettingsType(FCaptureProtocolID InProtocolID, UObject* Outer)
	{
		if (const FMovieSceneCaptureProtocolInfo* Info = Register.Find(InProtocolID))
		{
			if (Info->SettingsClassType)
			{
				return NewObject<UMovieSceneCaptureProtocolSettings>(Outer, Info->SettingsClassType);
			}
		}
		return nullptr;
	}

	/** Create a new instance of the protocol that relates to the specified ID */
	TSharedPtr<IMovieSceneCaptureProtocol> Factory(FCaptureProtocolID InProtocolID)
	{
		const FMovieSceneCaptureProtocolInfo* Info = Register.Find(InProtocolID);
		if (Info)
		{
			return Info->Factory();
		}
		return nullptr;
	}

	/** Register a new protocol */
	void RegisterProtocol(FCaptureProtocolID InProtocolID, const FMovieSceneCaptureProtocolInfo& Info)
	{
		Register.Add(InProtocolID, Info);
	}

	/** Unregister a previously registered protocol */
	void UnRegisterProtocol(FCaptureProtocolID InProtocolID)
	{
		Register.Remove(InProtocolID);
	}

private:
	TMap<FCaptureProtocolID, FMovieSceneCaptureProtocolInfo> Register;
};
