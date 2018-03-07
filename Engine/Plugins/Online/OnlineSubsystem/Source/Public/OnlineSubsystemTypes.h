// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "OnlineSubsystemPackage.h"

/** Maximum players supported on a given platform */
#if PLATFORM_XBOXONE
#define MAX_LOCAL_PLAYERS 4
#elif PLATFORM_PS4
#define MAX_LOCAL_PLAYERS 4
#elif PLATFORM_SWITCH
#define MAX_LOCAL_PLAYERS 8
#else
#define MAX_LOCAL_PLAYERS 1
#endif

/** TODO: Yuck. Public headers should not depend on redefining platform-specific macros like ERROR_SUCCESS below */
#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#endif

#ifndef E_FAIL
#define E_FAIL (uint32)-1
#endif

#ifndef E_NOTIMPL
#define E_NOTIMPL (uint32)-2
#endif

#ifndef ERROR_IO_PENDING
#define ERROR_IO_PENDING 997
#endif

#ifndef S_OK
#define S_OK 0
#endif

/**
 * Generates a random nonce (number used once) of the desired length
 *
 * @param Nonce the buffer that will get the randomized data
 * @param Length the number of bytes to generate random values for
 */
inline void GenerateNonce(uint8* Nonce, uint32 Length)
{
//@todo joeg -- switch to CryptGenRandom() if possible or something equivalent
	// Loop through generating a random value for each byte
	for (uint32 NonceIndex = 0; NonceIndex < Length; NonceIndex++)
	{
		Nonce[NonceIndex] = (uint8)(FMath::Rand() & 255);
	}
}

/**
 * Environment for the current online platform
 */
namespace EOnlineEnvironment
{
	enum Type
	{
		/** Dev environment */
		Development,
		/** Cert environment */
		Certification,
		/** Prod environment */
		Production,
		/** Not determined yet */
		Unknown
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineEnvironment::Type EnvironmentType)
	{
		switch (EnvironmentType)
		{
			case Development: return TEXT("Development");
			case Certification: return TEXT("Certification");
			case Production: return TEXT("Production");
			case Unknown: default: return TEXT("Unknown");
		};
	}
}

/** Possible login states */
namespace ELoginStatus
{
	enum Type
	{
		/** Player has not logged in or chosen a local profile */
		NotLoggedIn,
		/** Player is using a local profile but is not logged in */
		UsingLocalProfile,
		/** Player has been validated by the platform specific authentication service */
		LoggedIn
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELoginStatus::Type EnumVal)
	{
		switch (EnumVal)
		{
			case NotLoggedIn:
			{
				return TEXT("NotLoggedIn");
			}
			case UsingLocalProfile:
			{
				return TEXT("UsingLocalProfile");
			}
			case LoggedIn:
			{
				return TEXT("LoggedIn");
			}
		}
		return TEXT("");
	}
};

/** Possible connection states */
namespace EOnlineServerConnectionStatus
{
	enum Type : uint8
	{
		/** System normal (used for default state) */
		Normal = 0,
		/** Gracefully disconnected from the online servers */
		NotConnected,
		/** Connected to the online servers just fine */
		Connected,
		/** Connection was lost for some reason */
		ConnectionDropped,
		/** Can't connect because of missing network connection */
		NoNetworkConnection,
		/** Service is temporarily unavailable */
		ServiceUnavailable,
		/** An update is required before connecting is possible */
		UpdateRequired,
		/** Servers are too busy to handle the request right now */
		ServersTooBusy,
		/** Disconnected due to duplicate login */
		DuplicateLoginDetected,
		/** Can't connect because of an invalid/unknown user */
		InvalidUser,
		/** Not authorized */
		NotAuthorized,
		/** Session has been lost on the backend */
		InvalidSession
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineServerConnectionStatus::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Normal:
			{
				return TEXT("Normal");
			}
			case NotConnected:
			{
				return TEXT("NotConnected");
			}
			case Connected:
			{
				return TEXT("Connected");
			}
			case ConnectionDropped:
			{
				return TEXT("ConnectionDropped");
			}
			case NoNetworkConnection:
			{
				return TEXT("NoNetworkConnection");
			}
			case ServiceUnavailable:
			{
				return TEXT("ServiceUnavailable");
			}
			case UpdateRequired:
			{
				return TEXT("UpdateRequired");
			}
			case ServersTooBusy:
			{
				return TEXT("ServersTooBusy");
			}
			case DuplicateLoginDetected:
			{
				return TEXT("DuplicateLoginDetected");
			}
			case InvalidUser:
			{
				return TEXT("InvalidUser");
			}
			case NotAuthorized:
			{
				return TEXT("Not Authorized");
			}
			case InvalidSession:
			{
				return TEXT("Invalid Session");
			}
		}
		return TEXT("");
	}
};

/** Possible feature privilege access levels */
namespace EFeaturePrivilegeLevel
{
	enum Type
	{
		/** Not defined for the platform service */
		Undefined,
		/** Parental controls have disabled this feature */
		Disabled,
		/** Parental controls allow this feature only with people on their friends list */
		EnabledFriendsOnly,
		/** Parental controls allow this feature everywhere */
		Enabled
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EFeaturePrivilegeLevel::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Undefined:
			{
				return TEXT("Undefined");
			}
			case Disabled:
			{
				return TEXT("Disabled");
			}
			case EnabledFriendsOnly:
			{
				return TEXT("EnabledFriendsOnly");
			}
			case Enabled:
			{
				return TEXT("Enabled");
			}
		}
		return TEXT("");
	}
};

/** The state of an async task (read friends, read content, write cloud file, etc) request */
namespace EOnlineAsyncTaskState
{
	enum Type
	{
		/** The task has not been started */
		NotStarted,
		/** The task is currently being processed */
		InProgress,
		/** The task has completed successfully */
		Done,
		/** The task failed to complete */
		Failed
	};  

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineAsyncTaskState::Type EnumVal)
	{
		switch (EnumVal)
		{
			case NotStarted:
			{
				return TEXT("NotStarted");
			}
			case InProgress:
			{
				return TEXT("InProgress");
			}
			case Done:
			{
				return TEXT("Done");
			}
			case Failed:
			{
				return TEXT("Failed");
			}
		}
		return TEXT("");
	}
};

/** The possible friend states for a friend entry */
namespace EOnlineFriendState
{
	enum Type
	{
		/** Not currently online */
		Offline,
		/** Signed in and online */
		Online,
		/** Signed in, online, and idle */
		Away,
		/** Signed in, online, and asks to be left alone */
		Busy
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineFriendState::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Offline:
			{
				return TEXT("Offline");
			}
			case Online:
			{
				return TEXT("Online");
			}
			case Away:
			{
				return TEXT("Away");
			}
			case Busy:
			{
				return TEXT("Busy");
			}
		}
		return TEXT("");
	}
};

/** Leaderboard entry sort types */
namespace ELeaderboardSort
{
	enum Type
	{
		/** Don't sort at all */
		None,
		/** Sort ascending */
		Ascending,
		/** Sort descending */
		Descending
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELeaderboardSort::Type EnumVal)
	{
		switch (EnumVal)
		{
		case None:
			{
				return TEXT("None");
			}
		case Ascending:
			{
				return TEXT("Ascending");
			}
		case Descending:
			{
				return TEXT("Descending");
			}
		}
		return TEXT("");
	}
};

/** Leaderboard display format */
namespace ELeaderboardFormat
{
	enum Type
	{
		/** A raw number */
		Number,
		/** Time, in seconds */
		Seconds,
		/** Time, in milliseconds */
		Milliseconds
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELeaderboardFormat::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Number:
			{
				return TEXT("Number");
			}
		case Seconds:
			{
				return TEXT("Seconds");
			}
		case Milliseconds:
			{
				return TEXT("Milliseconds");
			}
		}
		return TEXT("");
	}
};

/** How to upload leaderboard score updates */
namespace ELeaderboardUpdateMethod
{
	enum Type
	{
		/** If current leaderboard score is better than the uploaded one, keep the current one */
		KeepBest,
		/** Leaderboard score is always replaced with uploaded value */
		Force
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELeaderboardUpdateMethod::Type EnumVal)
	{
		switch (EnumVal)
		{
		case KeepBest:
			{
				return TEXT("KeepBest");
			}
		case Force:
			{
				return TEXT("Force");
			}
		}
		return TEXT("");
	}
};

/** Enum indicating the state the LAN beacon is in */
namespace ELanBeaconState
{
	enum Type
	{
		/** The lan beacon is disabled */
		NotUsingLanBeacon,
		/** The lan beacon is responding to client requests for information */
		Hosting,
		/** The lan beacon is querying servers for information */
		Searching
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(ELanBeaconState::Type EnumVal)
	{
		switch (EnumVal)
		{

		case NotUsingLanBeacon:
			{
				return TEXT("NotUsingLanBeacon");
			}
		case Hosting:
			{
				return TEXT("Hosting");
			}
		case Searching:
			{
				return TEXT("Searching");
			}
		}
		return TEXT("");
	}
};

/** Enum indicating the current state of the online session (in progress, ended, etc.) */
namespace EOnlineSessionState
{
	enum Type
	{
		/** An online session has not been created yet */
		NoSession,
		/** An online session is in the process of being created */
		Creating,
		/** Session has been created but the session hasn't started (pre match lobby) */
		Pending,
		/** Session has been asked to start (may take time due to communication with backend) */
		Starting,
		/** The current session has started. Sessions with join in progress disabled are no longer joinable */
		InProgress,
		/** The session is still valid, but the session is no longer being played (post match lobby) */
		Ending,
		/** The session is closed and any stats committed */
		Ended,
		/** The session is being destroyed */
		Destroying
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineSessionState::Type EnumVal)
	{
		switch (EnumVal)
		{

		case NoSession:
			{
				return TEXT("NoSession");
			}
		case Creating:
			{
				return TEXT("Creating");
			}
		case Pending:
			{
				return TEXT("Pending");
			}
		case Starting:
			{
				return TEXT("Starting");
			}
		case InProgress:
			{
				return TEXT("InProgress");
			}
		case Ending:
			{
				return TEXT("Ending");
			}
		case Ended:
			{
				return TEXT("Ended");
			}
		case Destroying:
			{
				return TEXT("Destroying");
			}
		}
		return TEXT("");
	}
};

/** The types of advertisement of settings to use */
namespace EOnlineDataAdvertisementType
{
	enum Type
	{
		/** Don't advertise via the online service or QoS data */
		DontAdvertise,
		/** Advertise via the server ping data only */
		ViaPingOnly,
		/** Advertise via the online service only */
		ViaOnlineService,
		/** Advertise via the online service and via the ping data */
		ViaOnlineServiceAndPing
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineDataAdvertisementType::Type EnumVal)
	{
		switch (EnumVal)
		{
		case DontAdvertise:
			{
				return TEXT("DontAdvertise");
			}
		case ViaPingOnly:
			{
				return TEXT("ViaPingOnly");
			}
		case ViaOnlineService:
			{
				return TEXT("OnlineService");
			}
		case ViaOnlineServiceAndPing:
			{
				return TEXT("OnlineServiceAndPing");
			}
		}
		return TEXT("");
	}
}

/** The types of comparison operations for a given search query */
namespace EOnlineComparisonOp
{
	enum Type
	{
		Equals,
		NotEquals,
		GreaterThan,
		GreaterThanEquals,
		LessThan,
		LessThanEquals,
		Near,
		In,
		NotIn
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineComparisonOp::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Equals:
			{
				return TEXT("Equals");
			}
		case NotEquals:
			{
				return TEXT("NotEquals");
			}
		case GreaterThan:
			{
				return TEXT("GreaterThan");
			}
		case GreaterThanEquals:
			{
				return TEXT("GreaterThanEquals");
			}
		case LessThan:
			{
				return TEXT("LessThan");
			}
		case LessThanEquals:
			{
				return TEXT("LessThanEquals");
			}
		case Near:
			{
				return TEXT("Near");
			}
		case In:
			{
				return TEXT("In");
			}
		case NotIn:
			{
				return TEXT("NotIn");
			}
		}
		return TEXT("");
	}
}

/** Return codes for the GetCached functions in the various subsystems. */
namespace EOnlineCachedResult
{
	enum Type
	{
		Success, /** The requested data was found and returned successfully. */
		NotFound /** The requested data was not found in the cache, and the out parameter was not modified. */
	};

	/**
	 * @param EnumVal the enum to convert to a string
	 * @return the stringified version of the enum passed in
	 */
	inline const TCHAR* ToString(EOnlineCachedResult::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Success:
			{
				return TEXT("Success");
			}
		case NotFound:
			{
				return TEXT("NotFound");
			}
		}
		return TEXT("");
	}
}

/*
 *	Base class for anything meant to be opaque so that the data can be passed around 
 *  without consideration for the data it contains.
 *	A human readable version of the data is available via the ToString() function
 *	Otherwise, nothing but platform code should try to operate directly on the data
 */
class IOnlinePlatformData
{
protected:

	/** Hidden on purpose */
	IOnlinePlatformData()
	{
	}

	/** Hidden on purpose */
	IOnlinePlatformData(const IOnlinePlatformData& Src)
	{
	}

	/** Hidden on purpose */
	IOnlinePlatformData& operator=(const IOnlinePlatformData& Src)
	{
		return *this;
	}

	virtual bool Compare(const IOnlinePlatformData& Other) const
	{
		return (GetSize() == Other.GetSize()) &&
			(FMemory::Memcmp(GetBytes(), Other.GetBytes(), GetSize()) == 0);
	}

public:

	virtual ~IOnlinePlatformData() {}

	/**
	 *	Comparison operator
	 */
	bool operator==(const IOnlinePlatformData& Other) const
	{
		return Other.Compare(*this);
	}

	bool operator!=(const IOnlinePlatformData& Other) const
	{
		return !(IOnlinePlatformData::operator==(Other));
	}
	
	/** 
	 * Get the raw byte representation of this opaque data
	 * This data is platform dependent and shouldn't be manipulated directly
	 *
	 * @return byte array of size GetSize()
	 */
	virtual const uint8* GetBytes() const = 0;

	/** 
	 * Get the size of the opaque data
	 *
	 * @return size in bytes of the data representation
	 */
	virtual int32 GetSize() const = 0;

	/** 
	 * Check the validity of the opaque data
	 *
	 * @return true if this is well formed data, false otherwise
	 */
	virtual bool IsValid() const = 0;

	/** 
	 * Platform specific conversion to string representation of data
	 *
	 * @return data in string form 
	 */
	virtual FString ToString() const = 0;

	/** 
	 * Get a human readable representation of the opaque data
	 * Shouldn't be used for anything other than logging/debugging
	 *
	 * @return data in string form 
	 */
	virtual FString ToDebugString() const = 0;
};

/**
 * TArray helper for IndexOfByPredicate() function
 */
struct FUniqueNetIdMatcher
{
private:
	/** Target for comparison in the TArray */
	const FUniqueNetId& UniqueIdTarget;

public:
	FUniqueNetIdMatcher(const FUniqueNetId& InUniqueIdTarget) :
		UniqueIdTarget(InUniqueIdTarget)
	{
	}

	/**
	 * Match a given unique Id against the one stored in this struct
	 *
	 * @return true if they are an exact match, false otherwise
	 */
	bool operator()(const FUniqueNetId& Candidate) const
	{
		return UniqueIdTarget == Candidate;
	}
 
	/**
	 * Match a given unique Id against the one stored in this struct
	 *
	 * @return true if they are an exact match, false otherwise
	 */
	bool operator()(const TSharedPtr<const FUniqueNetId>& Candidate) const
	{
		return UniqueIdTarget == *Candidate;
	}

	/**
	 * Match a given unique Id against the one stored in this struct
	 *
	 * @return true if they are an exact match, false otherwise
	 */
	bool operator()(const TSharedRef<const FUniqueNetId>& Candidate) const
	{
		return UniqueIdTarget == *Candidate;
	}
};

/**
 * Unique net id wrapper for a string
 */
class FUniqueNetIdString : public FUniqueNetId
{
public:
	/** Holds the net id for a player */
	FString UniqueNetIdStr;

#if PLATFORM_COMPILER_HAS_DEFAULTED_FUNCTIONS
	// Define these to increase visibility to public (from parent's protected)
	FUniqueNetIdString() = default;
	virtual ~FUniqueNetIdString() = default;
	FUniqueNetIdString(const FUniqueNetIdString&) = default;
	FUniqueNetIdString(FUniqueNetIdString&&) = default;
	FUniqueNetIdString& operator=(const FUniqueNetIdString&) = default;
	FUniqueNetIdString& operator=(FUniqueNetIdString&&) = default;
#else
	/** Default constructor */
	FUniqueNetIdString()
	{
	}

	/** Destructor */
	virtual ~FUniqueNetIdString()
	{
	}

	/**
	 * Copy Constructor
	 *
	 * @param Src the id to copy
	 */
	FUniqueNetIdString(const FUniqueNetIdString& Src)
		: UniqueNetIdStr(Src.UniqueNetIdStr)
	{
	}

	/**
	 * Move Constructor
	 *
	 * @param Src the id to copy
	 */
	FUniqueNetIdString(FUniqueNetIdString&& Src)
		: UniqueNetIdStr(MoveTemp(Src.UniqueNetIdStr))
	{
	}

	/**
	 * Copy Assignment Operator
	 *
	 * @param Src the id to copy
	 */
	FUniqueNetIdString& operator=(const FUniqueNetIdString& Src)
	{
		if (this != &Src)
		{
			UniqueNetIdStr = Src.UniqueNetIdStr;
		}
		return *this;
	}

	/**
	 * Move Assignment Operator
	 *
	 * @param Src the id to copy
	 */
	FUniqueNetIdString& operator=(FUniqueNetIdString&& Src)
	{
		if (this != &Src)
		{
			UniqueNetIdStr = MoveTemp(Src.UniqueNetIdStr);
		}
		return *this;
	}
#endif

	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdString(const FString& InUniqueNetId)
		: UniqueNetIdStr(InUniqueNetId)
	{
	}

	/**
	 * Constructs this object with the specified net id
	 *
	 * @param InUniqueNetId the id to set ours to
	 */
	explicit FUniqueNetIdString(FString&& InUniqueNetId)
		: UniqueNetIdStr(MoveTemp(InUniqueNetId))
	{
	}

	/**
	 * Constructs this object with the string value of the specified net id
	 *
	 * @param Src the id to copy
	 */
	explicit FUniqueNetIdString(const FUniqueNetId& Src)
		: UniqueNetIdStr(Src.ToString())
	{
	}

	// IOnlinePlatformData

	virtual const uint8* GetBytes() const override
	{
		return (const uint8*)UniqueNetIdStr.GetCharArray().GetData();
	}

	virtual int32 GetSize() const override
	{
		return UniqueNetIdStr.GetCharArray().GetTypeSize() * UniqueNetIdStr.GetCharArray().Num();
	}

	virtual bool IsValid() const override
	{
		return !UniqueNetIdStr.IsEmpty();
	}

	virtual FString ToString() const override
	{
		return UniqueNetIdStr;
	}

	virtual FString ToDebugString() const override
	{
		return UniqueNetIdStr;
	}

	/** Needed for TMap::GetTypeHash() */
	friend uint32 GetTypeHash(const FUniqueNetIdString& A)
	{
		return ::GetTypeHash(A.UniqueNetIdStr);
	}
};

/** 
 * Abstraction of a profile service shared file handle
 * The class is meant to be opaque (see IOnlinePlatformData)
 */
class FSharedContentHandle : public IOnlinePlatformData
{
protected:

	/** Hidden on purpose */
	FSharedContentHandle()
	{
	}

	/** Hidden on purpose */
	FSharedContentHandle(const FSharedContentHandle& Src)
	{
	}

	/** Hidden on purpose */
	FSharedContentHandle& operator=(const FSharedContentHandle& Src)
	{
		return *this;
	}

public:

	virtual ~FSharedContentHandle() {}
};

/** 
 * Abstraction of a session's platform specific info
 * The class is meant to be opaque (see IOnlinePlatformData)
 */
class FOnlineSessionInfo : public IOnlinePlatformData
{
protected:

	/** Hidden on purpose */
	FOnlineSessionInfo()
	{
	}

	/** Hidden on purpose */
	FOnlineSessionInfo(const FOnlineSessionInfo& Src)
	{
	}

	/** Hidden on purpose */
	FOnlineSessionInfo& operator=(const FOnlineSessionInfo& Src)
	{
		return *this;
	}

public:

	virtual ~FOnlineSessionInfo() {}

	/**
	 * Get the session id associated with this session
	 *
	 * @return session id for this session
	 */
	virtual const FUniqueNetId& GetSessionId() const = 0;
};

/**
 * Paging info needed for a request that can return paged results
 */
class FPagedQuery
{
public:
	FPagedQuery(int32 InStart = 0, int32 InCount = -1)
		: Start(InStart)
		, Count(InCount)
	{}

	/** @return true if valid range */
	bool IsValidRange() const
	{
		return Start >= 0 && Count >= 0;
	}

	/** first entry to fetch */
	int32 Start;
	/** total entries to fetch. -1 means ALL */
	int32 Count;
};

/**
 * Info for a response with paged results
 */
class FOnlinePagedResult
{
public:
	FOnlinePagedResult()
		: Start(0)
		, Count(0)
		, Total(0)
	{}
	virtual ~FOnlinePagedResult() {}

	/** Starting entry */
	int32 Start;
	/** Number returned */
	int32 Count;
	/** Total available */
	int32 Total;
};

/** Locale and country code */
class FRegionInfo
{
public:

	FRegionInfo(const FString& InCountry = FString(), const FString& InLocale = FString())
		: Country(InCountry)
		, Locale(InLocale)
	{}

	/** country code used for configuring things like currency/pricing specific to a country. eg. US */
	FString Country;
	/** local code used to select the localization language. eg. en_US */
	FString Locale;
};

/** Holds metadata about a given downloadable file */
struct FCloudFileHeader
{
	/** Hash value, if applicable, of the given file contents */
	FString Hash;
	/** The hash algorithm used to sign this file */
	FName HashType;
	/** Filename as downloaded */
	FString DLName;
	/** Logical filename, maps to the downloaded filename */
	FString FileName;
	/** File size */
	int32 FileSize;
	/** The full URL to download the file if it is stored in a CDN or separate host site */
	FString URL;
	/** The chunk id this file represents */
	uint32 ChunkID;

	/** Constructors */
	FCloudFileHeader() :
		FileSize(0),
		ChunkID(0)
	{}

	FCloudFileHeader(const FString& InFileName, const FString& InDLName, int32 InFileSize) :
		DLName(InDLName),
		FileName(InFileName),
		FileSize(InFileSize),
		ChunkID(0)
	{}

	bool operator==(const FCloudFileHeader& Other) const
	{
		return FileSize == Other.FileSize &&
			Hash == Other.Hash &&
			HashType == Other.HashType &&
			DLName == Other.DLName &&
			FileName == Other.FileName &&
			URL == Other.URL &&
			ChunkID == Other.ChunkID;
	}

	bool operator<(const FCloudFileHeader& Other) const
	{
		return FileName.Compare(Other.FileName, ESearchCase::IgnoreCase) < 0;
	}
};

/** Holds the data used in downloading a file asynchronously from the online service */
struct FCloudFile
{
	/** The name of the file as requested */
	FString FileName;
	/** The async state the file download is in */
	EOnlineAsyncTaskState::Type AsyncState;
	/** The buffer of data for the file */
	TArray<uint8> Data;

	/** Constructors */
	FCloudFile() :
		AsyncState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	FCloudFile(const FString& InFileName) :
		FileName(InFileName),
		AsyncState(EOnlineAsyncTaskState::NotStarted)
	{
	}

	virtual ~FCloudFile() {}
};

/**
 * Base for all online user info
 */
class FOnlineUser
{
public:
	/**
	 * destructor
	 */
	virtual ~FOnlineUser() {}

	/** 
	 * @return Id associated with the user account provided by the online service during registration 
	 */
	virtual TSharedRef<const FUniqueNetId> GetUserId() const = 0;
	/**
	 * @return the real name for the user if known
	 */
	virtual FString GetRealName() const = 0;
	/**
	 * @return the nickname of the user if known
	 */
	virtual FString GetDisplayName(const FString& Platform = FString()) const = 0;
	/** 
	 * @return Any additional user data associated with a registered user
	 */
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const = 0;
};

/**
 * User account information returned via IOnlineIdentity interface
 */
class FUserOnlineAccount : public FOnlineUser
{
public:
	/**
	 * @return Access token which is provided to user once authenticated by the online service
	 */
	virtual FString GetAccessToken() const = 0;
	/** 
	 * @return Any additional auth data associated with a registered user
	 */
	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const = 0;
	/** 
	 * @return True, if the data has been changed
	 */
	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) = 0;
};

/** 
 * Friend list invite states 
 */
namespace EInviteStatus
{
	enum Type
	{
		/** unknown state */
		Unknown,
		/** Friend has accepted the invite */
		Accepted,
		/** Friend has sent player an invite, but it has not been accepted/rejected */
		PendingInbound,
		/** Player has sent friend an invite, but it has not been accepted/rejected */
		PendingOutbound,
		/** Player has been blocked */
		Blocked,
	};

	/** 
	 * @return the stringified version of the enum passed in 
	 */
	inline const TCHAR* ToString(EInviteStatus::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Unknown:
			{
				return TEXT("Unknown");
			}
			case Accepted:
			{
				return TEXT("Accepted");
			}
			case PendingInbound:
			{
				return TEXT("PendingInbound");
			}
			case PendingOutbound:
			{
				return TEXT("PendingOutbound");
			}
			case Blocked:
			{
				return TEXT("Blocked");
			}
		}
		return TEXT("");
	}
};

/**
 * Friend user info returned via IOnlineFriends interface
 */
class FOnlineFriend : public FOnlineUser
{
public:
	
	/**
	 * @return the current invite status of a friend wrt to user that queried
	 */
	virtual EInviteStatus::Type GetInviteStatus() const = 0;
	
	/**
	 * @return presence info for an online friend
	 */
	virtual const class FOnlineUserPresence& GetPresence() const = 0;
};

/**
 * Recent player user info returned via IOnlineFriends interface
 */
class FOnlineRecentPlayer : public FOnlineUser
{
public:
	
	/**
	 * @return last time the player was seen by the current user
	 */
	virtual FDateTime GetLastSeen() const = 0;
};

/**
 * Blocked user info returned via IOnlineFriends interface
 */
class FOnlineBlockedPlayer : public FOnlineUser
{
};

/** Valid states for user facing permissions */
enum class EOnlineSharingPermissionState : uint8
{
	/** Permission has not been requested yet */
	Unknown = 0,
	/** Permission has been requested but declined by the user */
	Declined = 1,
	/** Permission has been granted by the user */
	Granted = 2,
};

/**
 * First 16 bits are reading permissions
 * Second 16 bits are writing/publishing permissions
 */
enum class EOnlineSharingCategory : uint32
{
	None = 0x00,
	// Read access to posts on the users feeds
	ReadPosts = 0x01,
	// Read access for a users friend information, and all data about those friends. e.g. Friends List and Individual Friends Birthday
	Friends = 0x02,
	// Read access to a user's email address
	Email = 0x04,
	// Read access to a users mailbox
	Mailbox = 0x08,
	// Read the current online status of a user
	OnlineStatus = 0x10,
	// Read a users profile information, e.g. Users Birthday
	ProfileInfo = 0x20,
	// Read information about the users locations and location history
	LocationInfo = 0x40,

	ReadPermissionMask = 0x0000FFFF,
	DefaultRead = ProfileInfo | LocationInfo,

	// Permission to post to a users news feed
	SubmitPosts = 0x010000,
	// Permission to manage a users friends list. Add/Remove contacts
	ManageFriends = 0x020000,
	// Manage a users account settings, such as pages they subscribe to, or which notifications they receive
	AccountAdmin = 0x040000,
	// Manage a users events. This features the capacity to create events as well as respond to events.
	Events = 0x080000,
	
	PublishPermissionMask = 0xFFFF0000,
	DefaultPublish = None
};

ENUM_CLASS_FLAGS(EOnlineSharingCategory);

inline const TCHAR* ToString(EOnlineSharingCategory CategoryType)
{
	switch (CategoryType)
	{
		case EOnlineSharingCategory::None:
		{
			return TEXT("Category undefined");
		}
		case EOnlineSharingCategory::ReadPosts:
		{
			return TEXT("ReadPosts");
		}
		case EOnlineSharingCategory::Friends:
		{
			return TEXT("Friends");
		}
		case EOnlineSharingCategory::Mailbox:
		{
			return TEXT("Mailbox");
		}
		case EOnlineSharingCategory::OnlineStatus:
		{
			return TEXT("Online Status");
		}
		case EOnlineSharingCategory::ProfileInfo:
		{
			return TEXT("Profile Information");
		}
		case EOnlineSharingCategory::LocationInfo:
		{
			return TEXT("Location Information");
		}
		case EOnlineSharingCategory::SubmitPosts:
		{
			return TEXT("SubmitPosts");
		}
		case EOnlineSharingCategory::ManageFriends:
		{
			return TEXT("ManageFriends");
		}
		case EOnlineSharingCategory::AccountAdmin:
		{
			return TEXT("Account Admin");
		}
		case EOnlineSharingCategory::Events:
		{
			return TEXT("Events");
		}
	}
	return TEXT("");
}

/** Privacy permissions used for Online Status updates */
enum class EOnlineStatusUpdatePrivacy : uint8
{
	// Post will only be visible to the user alone
	OnlyMe,
	// Post will only be visible to the user and the users friends
	OnlyFriends,
	// Post will be visible to everyone
	Everyone,
};

inline const TCHAR* ToString(EOnlineStatusUpdatePrivacy PrivacyType)
{
	switch (PrivacyType)
	{
		case EOnlineStatusUpdatePrivacy::OnlyMe:
			return TEXT("Only Me");
		case EOnlineStatusUpdatePrivacy::OnlyFriends:
			return TEXT("Only Friends");
		case EOnlineStatusUpdatePrivacy::Everyone:
			return TEXT("Everyone");
	}
}

/**
 * unique identifier for notification transports
 */
typedef FString FNotificationTransportId;

/**
 * Id of a party instance
 */
class FOnlinePartyId : public IOnlinePlatformData, public TSharedFromThis<FOnlinePartyId>
{
protected:
	/** Hidden on purpose */
	FOnlinePartyId()
	{
	}

	/** Hidden on purpose */
	FOnlinePartyId(const FOnlinePartyId& Src)
	{
	}

	/** Hidden on purpose */
	FOnlinePartyId& operator=(const FOnlinePartyId& Src)
	{
		return *this;
	}

public:
	virtual ~FOnlinePartyId() {}
};

/**
 * Id of a party's type
 */
class FOnlinePartyTypeId
{
public:
	typedef uint32 TInternalType;
	explicit FOnlinePartyTypeId(const TInternalType InValue = 0) : Value(InValue) {}
	FOnlinePartyTypeId(const FOnlinePartyTypeId& Other) : Value(Other.Value) {}

	bool operator==(const FOnlinePartyTypeId Rhs) const { return Value == Rhs.Value; }
	bool operator!=(const FOnlinePartyTypeId Rhs) const { return Value != Rhs.Value; }

	TInternalType GetValue() const { return Value; }

	friend bool IsValid(const FOnlinePartyTypeId Value);

protected:
	TInternalType Value;
};

inline bool IsValid(const FOnlinePartyTypeId Id) { return Id.GetValue() != 0; }

inline uint32 GetTypeHash(const FOnlinePartyTypeId Id)
{
	return Id.GetValue();
}
