// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Delegates/Delegate.h"

CORE_API DECLARE_LOG_CATEGORY_EXTERN( LogNetVersion, Log, All );

class FNetworkReplayVersion
{
public:
	FNetworkReplayVersion() : NetworkVersion( 0 ), Changelist( 0 )
	{
	}
	FNetworkReplayVersion( const FString& InAppString, const uint32 InNetworkVersion, const uint32 InChangelist ) : AppString( InAppString ), NetworkVersion( InNetworkVersion ), Changelist( InChangelist )
	{
	}

	FString		AppString;
	uint32		NetworkVersion;
	uint32		Changelist;
};

struct CORE_API FNetworkVersion
{
	/** Called in GetLocalNetworkVersion if bound */
	DECLARE_DELEGATE_RetVal( uint32, FGetLocalNetworkVersionOverride );
	static FGetLocalNetworkVersionOverride GetLocalNetworkVersionOverride;

	/** Called in IsNetworkCompatible if bound */
	DECLARE_DELEGATE_RetVal_TwoParams( bool, FIsNetworkCompatibleOverride, uint32, uint32 );
	static FIsNetworkCompatibleOverride IsNetworkCompatibleOverride;

	static uint32 GetNetworkCompatibleChangelist();
	static uint32 GetReplayCompatibleChangelist();
	static uint32 GetEngineNetworkProtocolVersion();
	static uint32 GetGameNetworkProtocolVersion();
	static uint32 GetEngineCompatibleNetworkProtocolVersion();
	static uint32 GetGameCompatibleNetworkProtocolVersion();

	/**
	* Generates a version number, that by default, is based on a checksum of the engine version + project name + project version string
	* Game/project code can completely override what this value returns through the GetLocalNetworkVersionOverride delegate
	* If called with AllowOverrideDelegate=false, we will not call the game project override. (This allows projects to call base implementation in their project implementation)
	*/
	static uint32 GetLocalNetworkVersion( bool AllowOverrideDelegate=true );

	/**
	* Determine if a connection is compatible with this instance
	*
	* @param bRequireEngineVersionMatch should the engine versions match exactly
	* @param LocalNetworkVersion current version of the local machine
	* @param RemoteNetworkVersion current version of the remote machine
	*
	* @return true if the two instances can communicate, false otherwise
	*/
	static bool IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion );

	/**
	* Generates a special struct that contains information to send to replay server
	*/
	static FNetworkReplayVersion GetReplayVersion();

	/**
	* Used to allow BP only projects to override network versions
	*/
	static FString ProjectVersion;

	static bool		bHasCachedNetworkChecksum;
	static uint32	CachedNetworkChecksum;

	static uint32	EngineNetworkProtocolVersion;
	static uint32	GameNetworkProtocolVersion;

	static uint32	EngineCompatibleNetworkProtocolVersion;
	static uint32	GameCompatibleNetworkProtocolVersion;
};
