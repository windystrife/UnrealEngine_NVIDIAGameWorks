// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "HAL/ThreadSafeCounter.h"
#include "Math/NumericLimits.h"
#include "HAL/ThreadSingleton.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/PlatformTime.h"
#include "UObject/NameTypes.h"
#include "Containers/LockFreeList.h"
#include "Containers/ChunkedArray.h"
#include "Delegates/Delegate.h"

class FScopeCycleCounter;
class FThreadStats;
struct TStatId;

template < class T > class TThreadSingleton;

/**
* This is thread-private information about the thread idle stats, which we always collect, even in final builds
*/
class CORE_API FThreadIdleStats : public TThreadSingleton<FThreadIdleStats>
{
	friend class TThreadSingleton<FThreadIdleStats>;

	FThreadIdleStats()
		: Waits(0)
	{}

public:

	/** Total cycles we waited for sleep or event. **/
	uint32 Waits;

	struct FScopeIdle
	{
		/** Starting cycle counter. */
		const uint32 Start;

		/** If true, we ignore this thread idle stats. */
		const bool bIgnore;

		FScopeIdle( bool bInIgnore = false )
			: Start(FPlatformTime::Cycles())
			, bIgnore( bInIgnore )
		{
		}

		~FScopeIdle()
		{
			if( !bIgnore )
			{
				FThreadIdleStats::Get().Waits += FPlatformTime::Cycles() - Start;
			}
		}
	};
};

// Pass a console command directly to the stats system, return true if it is known command, false means it might be a stats command
CORE_API bool DirectStatsCommand(const TCHAR* Cmd, bool bBlockForCompletion = false, FOutputDevice* Ar = nullptr);

/** Helper struct that contains method available even when the stats are disabled. */
struct CORE_API FStats
{
	/** Delegate to fire every time we need to advance the stats for the rendering thread. */
	DECLARE_DELEGATE_ThreeParams( FOnAdvanceRenderingThreadStats, bool /*bDiscardCallstack*/, int64 /*StatsFrame*/, int32 /*MasterDisableChangeTagStartFrame*/ );

	/** Advances stats for the current frame. */
	static void AdvanceFrame( bool bDiscardCallstack, const FOnAdvanceRenderingThreadStats& AdvanceRenderingThreadStatsDelegate = FOnAdvanceRenderingThreadStats() );

	/** Advances stats for commandlets, only valid if the command line has the proper token. @see HasStatsForCommandletsToken */
	static void TickCommandletStats();

	/**
	* @return true, if the command line has the LoadTimeStatsForCommandlet or LoadTimeFileForCommandlet token which enables stats in the commandlets.
	* !!!CAUTION!!! You need to manually advance stats frame in order to maintain the data integrity and not to leak the memory.
	*/
	static bool EnabledForCommandlet();

	/**
	* @return true, if the command line has the LoadTimeStatsForCommandlet token which enables LoadTimeStats equivalent for commandlets.
	* All collected stats will be dumped to the log file at the end of running the specified commandlet.
	*/
	static bool HasLoadTimeStatsForCommandletToken();

	/**
	* @return true, if the command line has the LoadTimeFileForCommandlet token which enables LoadTimeFile equivalent for commandlets.
	*/
	static bool HasLoadTimeFileForCommandletToken();

	/** Current game thread stats frame. */
	static int32 GameThreadStatsFrame;
};

#if STATS

MS_ALIGN(8)
struct TStatIdData
{
	FORCEINLINE TStatIdData()
		: AnsiString(0)
		, WideString(0)
	{
	}

	FORCEINLINE bool IsNone() const
	{
		return Name.Index == 0 && Name.Number == 0;
	}

	/** Name of the active stat; stored as a minimal name to minimize the data size */
	FMinimalName Name;

	/** const ANSICHAR* pointer to a string; stored as a uint64 so it doesn't change size and affect TStatIdData alignment between 32 and 64-bit builds) */
	uint64 AnsiString;

	/** const WIDECHAR* pointer to a string; stored as a uint64 so it doesn't change size and affect TStatIdData alignment between 32 and 64-bit builds) */
	uint64 WideString;
} GCC_ALIGN(8);

struct TStatId
{
	FORCEINLINE TStatId()
		: StatIdPtr(&TStatId_NAME_None)
	{
	}
	FORCEINLINE TStatId(TStatIdData const* InStatIdPtr)
		: StatIdPtr(InStatIdPtr)
	{
	}
	FORCEINLINE bool IsValidStat() const
	{
		return !IsNone();
	}
	FORCEINLINE bool IsNone() const
	{
		return StatIdPtr->IsNone();
	}
	FORCEINLINE TStatIdData const* GetRawPointer() const
	{
		return StatIdPtr;
	}

	FORCEINLINE FName GetName() const
	{
		return MinimalNameToName(StatIdPtr->Name);
	}

	FORCEINLINE static const FMinimalName* GetStatNone()
	{
		return &TStatId_NAME_None.Name;
	}

	/**
	 * @return a stat description as an ansi string.
	 * StatIdPtr must point to a valid FName pointer.
	 * @see FStatGroupEnableManager::GetHighPerformanceEnableForStat
	 */
	FORCEINLINE const ANSICHAR* GetStatDescriptionANSI() const
	{
		return reinterpret_cast<const ANSICHAR*>(StatIdPtr->AnsiString);
	}

	/**
	 * @return a stat description as a wide string.
	 * StatIdPtr must point to a valid FName pointer.
	 * @see FStatGroupEnableManager::GetHighPerformanceEnableForStat
	 */
	FORCEINLINE const WIDECHAR* GetStatDescriptionWIDE() const
	{
		return reinterpret_cast<const WIDECHAR*>(StatIdPtr->WideString);
	}

private:
	/** NAME_None. */
	CORE_API static TStatIdData TStatId_NAME_None;

	/**
	 *	Holds a pointer to the stat long name if enabled, or to the NAME_None if disabled.
	 *	@see FStatGroupEnableManager::EnableStat
	 *	@see FStatGroupEnableManager::DisableStat
	 *	
	 *	Next pointer points to the ansi string with a stat description
	 *	Next pointer points to the wide string with a stat description
	 *	@see FStatGroupEnableManager::GetHighPerformanceEnableForStat 
	 */
	TStatIdData const* StatIdPtr;
};

/**
 * For packet messages, this indicated what sort of thread timing we use. 
 * Game and Other use CurrentGameFrame, Renderer is CurrentRenderFrame
 */
namespace EThreadType
{
	enum Type
	{
		Invalid,
		Game,
		Renderer,
		Other,
	};
}


/**
 * What the type of the payload is
 */
struct EStatDataType
{
	enum Type
	{
		Invalid,
		/** Not defined. */
		ST_None,
		/** int64. */
		ST_int64,
		/** double. */
		ST_double,
		/** FName. */
		ST_FName,
		/** Memory pointer, stored as uint64. */
		ST_Ptr,

		Num,
		Mask = 0x7,
		Shift = 0,
		NumBits = 3
	};
};

/**
 * The operation being performed by this message
 */
struct EStatOperation
{
	enum Type
	{
		Invalid,
		/** Indicates metadata message. */
		SetLongName,
		/** Special message for advancing the stats frame from the game thread. */
		AdvanceFrameEventGameThread,    
		/** Special message for advancing the stats frame from the render thread. */
		AdvanceFrameEventRenderThread,  
		/** Indicates begin of the cycle scope. */
		CycleScopeStart,
		/** Indicates end of the cycle scope. */
		CycleScopeEnd,
		/** This is not a regular stat operation, but just a special message marker to determine that we encountered a special data in the stat file. */
		SpecialMessageMarker,
		/** Set operation. */
		Set,
		/** Clear operation. */
		Clear,
		/** Add operation. */
		Add,
		/** Subtract operation. */
		Subtract,

		// these are special ones for processed data
		ChildrenStart,
		ChildrenEnd,
		Leaf,
		MaxVal,

		/** This is a memory operation. @see EMemoryOperation. */
		Memory,

		Num,
		Mask = 0xf,
		Shift = EStatDataType::Shift + EStatDataType::NumBits,
		NumBits = 4
	};
};

/**
 * Message flags
 */
struct EStatMetaFlags
{
	enum Type
	{
		Invalid							= 0x00,
		/** this bit is always one and is used for error checking. */
		DummyAlwaysOne					= 0x01,  
		/** if true, then this message contains all meta data as well and the name is long. NOT USED. */
		HasLongNameAndMetaInfo			= 0x02,
		/** if true, then this message contains and int64 cycle or IsPackedCCAndDuration. */
		IsCycle							= 0x04,	
		/** if true, then this message contains a memory stat. */
		IsMemory						= 0x08, 
		/** if true, then this is actually two uint32s, the cycle count and the call count, see FromPackedCallCountDuration_Duration. */
		IsPackedCCAndDuration			= 0x10,
		/** if true, then this stat is cleared every frame. */
		ShouldClearEveryFrame			= 0x20, 
		/** used only on disk on on the wire, indicates that we serialized the FName string. */
		SendingFName					= 0x40,

		Num								= 0x80,
		Mask							= 0xff,
		Shift = EStatOperation::Shift + EStatOperation::NumBits,
		NumBits = 8
	};
};

//@todo merge these two after we have redone the user end
/**
 * Wrapper for memory region
 */
struct EMemoryRegion
{
	enum Type
	{
		Invalid = FPlatformMemory::MCR_Invalid,

		Num = FPlatformMemory::MCR_MAX,
		Mask = 0xf,
		Shift = EStatMetaFlags::Shift + EStatMetaFlags::NumBits,
		NumBits = 4
	};
	static_assert(FPlatformMemory::MCR_MAX < (1 << NumBits), "Need to expand memory region field.");
};

/** Memory operation for STAT_Memory_AllocPtr. */
enum class EMemoryOperation : uint8
{
	/** Invalid. */
	Invalid,	
	/** Alloc. */
	Alloc,
	/** Free. */
	Free,
	/** Realloc. */
	Realloc,

	Num,
	Mask = 0x7,
	/** Every allocation is aligned to 8 or 16 bytes, so we have 3/4 bits free to use. */
	NumBits = 3,
};

/**
 * A few misc final bit packing computations
 */
namespace EStatAllFields
{
	enum Type
	{
		NumBits = EMemoryRegion::Shift + EMemoryRegion::NumBits,
		StartShift = 28 - NumBits,
	};
}

static_assert(EStatAllFields::StartShift > 0, "Too many stat fields.");

FORCEINLINE int64 ToPackedCallCountDuration(uint32 CallCount, uint32 Duration)
{
	return (int64(CallCount) << 32) | Duration;
}

FORCEINLINE uint32 FromPackedCallCountDuration_CallCount(int64 Both)
{
	return uint32(Both >> 32);
}

FORCEINLINE uint32 FromPackedCallCountDuration_Duration(int64 Both)
{
	return uint32(Both & MAX_uint32);
}

/**
 * Helper class that stores an FName and all meta information in 8 bytes. Kindof icky.
 */
class FStatNameAndInfo
{
	/**
	 * An FName, but the high bits of the Number are used for other fields.
	 */
	FMinimalName NameAndInfo;
public:
	FORCEINLINE_STATS FStatNameAndInfo()
	{
	}

	/**
	 * Copy constructor
	 */
	FORCEINLINE_STATS FStatNameAndInfo(FStatNameAndInfo const& Other)
		: NameAndInfo(Other.NameAndInfo)
	{
		static_assert(EStatAllFields::StartShift >= 0, "Too many fields.");
		CheckInvariants();
	}

	/**
	 * Build from a raw FName
	 */
	FORCEINLINE_STATS FStatNameAndInfo(FName Other, bool bAlreadyHasMeta)
		: NameAndInfo(NameToMinimalName(Other))
	{
		if (!bAlreadyHasMeta)
		{
			int32 Number = NameAndInfo.Number;
			// ok, you can't have numbered stat FNames too large
			checkStats(!(Number >> EStatAllFields::StartShift));
			Number |= EStatMetaFlags::DummyAlwaysOne << (EStatMetaFlags::Shift + EStatAllFields::StartShift);
			NameAndInfo.Number = Number;
		}
		CheckInvariants();
	}

	/**
	 * Build with stat metadata
	 */
	FORCEINLINE_STATS FStatNameAndInfo(FName InStatName, char const* InGroup, char const* InCategory, TCHAR const* InDescription, EStatDataType::Type InStatType, bool bShouldClearEveryFrame, bool bCycleStat, FPlatformMemory::EMemoryCounterRegion MemoryRegion = FPlatformMemory::MCR_Invalid)
		: NameAndInfo(NameToMinimalName(ToLongName(InStatName, InGroup, InCategory, InDescription)))
	{
		int32 Number = NameAndInfo.Number;
		// ok, you can't have numbered stat FNames too large
		checkStats(!(Number >> EStatAllFields::StartShift));
		Number |= (EStatMetaFlags::DummyAlwaysOne | EStatMetaFlags::HasLongNameAndMetaInfo) << (EStatMetaFlags::Shift + EStatAllFields::StartShift);
		NameAndInfo.Number = Number;

		SetField<EStatDataType>(InStatType);
		SetFlag(EStatMetaFlags::ShouldClearEveryFrame, bShouldClearEveryFrame);
		SetFlag(EStatMetaFlags::IsCycle, bCycleStat);
		if (MemoryRegion != FPlatformMemory::MCR_Invalid)
		{
			SetFlag(EStatMetaFlags::IsMemory, true);
			SetField<EMemoryRegion>(EMemoryRegion::Type(MemoryRegion));
		}

		CheckInvariants();
	}

	/**
	 * Internal use, used by the deserializer
	 */
	FORCEINLINE_STATS void SetNumberDirect(int32 Number)
	{
		NameAndInfo.Number = Number;
	}

	/**
	* Internal use, used by the serializer
	 */
	FORCEINLINE_STATS int32 GetRawNumber() const
	{
		CheckInvariants();
		return NameAndInfo.Number;
	}

	/**
	 * Internal use by FStatsThreadState to force an update to the long name
	 */
	FORCEINLINE_STATS void SetRawName(FName RawName)
	{
		// ok, you can't have numbered stat FNames too large
		checkStats(!(RawName.GetNumber() >> EStatAllFields::StartShift));
		CheckInvariants();
		int32 Number = NameAndInfo.Number;
		Number &= ~((1 << EStatAllFields::StartShift) - 1);
		NameAndInfo = NameToMinimalName(RawName);
		NameAndInfo.Number = (Number | RawName.GetNumber());
	}

	/**
	 * The encoded FName with the correct, original Number
	 * The original number usually is 0
	 */
	FORCEINLINE_STATS FName GetRawName() const
	{
		CheckInvariants();
		FMinimalName Result(NameAndInfo);
		int32 Number = NameAndInfo.Number;
		Number &= ((1 << EStatAllFields::StartShift) - 1);
		Result.Number = Number;
		return MinimalNameToName(Result);
	}

	/**
	 * The encoded FName with the encoded, new Number
	 * The number contains all encoded metadata
	 */
	FORCEINLINE_STATS FName GetEncodedName() const
	{
		CheckInvariants();
		return MinimalNameToName(NameAndInfo);
	}

	/**
	 * Expensive! Extracts the shortname if this is a long name or just returns the name
	 */
	FORCEINLINE_STATS FName GetShortName() const
	{
		CheckInvariants();
		return GetShortNameFrom(GetRawName());
	}

	/**
	 * Expensive! Extracts the group name if this is a long name or just returns none
	 */
	FORCEINLINE_STATS FName GetGroupName() const
	{
		CheckInvariants();
		return GetGroupNameFrom(GetRawName());
	}

	/**
	 * Expensive! Extracts the group category if this is a long name or just returns none
	 */
	FORCEINLINE_STATS FName GetGroupCategory() const
	{
		CheckInvariants();
		return GetGroupCategoryFrom(GetRawName());
	}

	/**
	 * Expensive! Extracts the description if this is a long name or just returns the empty string
	 */
	FORCEINLINE_STATS FString GetDescription() const
	{
		CheckInvariants();
		return GetDescriptionFrom(GetRawName());
	}

	/**
	 * Makes sure this object is in good shape
	 */
	FORCEINLINE_STATS void CheckInvariants() const
	{
		checkStats((NameAndInfo.Number & (EStatMetaFlags::DummyAlwaysOne << (EStatAllFields::StartShift + EStatMetaFlags::Shift)))
			&& NameAndInfo.Index);
	}

	/**
	 * returns an encoded field
	 * @return the field
	 */
	template<typename TField>
	typename TField::Type GetField() const
	{
		CheckInvariants();
		int32 Number = NameAndInfo.Number;
		Number = (Number >> (EStatAllFields::StartShift + TField::Shift)) & TField::Mask;
		checkStats(Number != TField::Invalid && Number < TField::Num);
		return typename TField::Type(Number);
	}

	/**
	 * sets an encoded field
	 * @param Value, value to set
	 */
	template<typename TField>
	void SetField(typename TField::Type Value)
	{
		int32 Number = NameAndInfo.Number;
		CheckInvariants();
		checkStats(Value < TField::Num && Value != TField::Invalid);
		Number &= ~(TField::Mask << (EStatAllFields::StartShift + TField::Shift));
		Number |= Value << (EStatAllFields::StartShift + TField::Shift);
		NameAndInfo.Number = Number;
		CheckInvariants();
	}

	/**
	 * returns an encoded flag
	 * @param Bit, flag to read
	 */
	bool GetFlag(EStatMetaFlags::Type Bit) const
	{
		int32 Number = NameAndInfo.Number;
		CheckInvariants();
		checkStats(Bit < EStatMetaFlags::Num && Bit != EStatMetaFlags::Invalid);
		return !!((Number >> (EStatAllFields::StartShift + EStatMetaFlags::Shift)) & Bit);
	}

	/**
	 * sets an encoded flag
	 * @param Bit, flag to set
	 * @param Value, value to set
	 */
	void SetFlag(EStatMetaFlags::Type Bit, bool Value)
	{
		int32 Number = NameAndInfo.Number;
		CheckInvariants();
		checkStats(Bit < EStatMetaFlags::Num && Bit != EStatMetaFlags::Invalid);
		if (Value)
		{
			Number |= (Bit << (EStatAllFields::StartShift + EStatMetaFlags::Shift));
		}
		else
		{
			Number &= ~(Bit << (EStatAllFields::StartShift + EStatMetaFlags::Shift));
		}
		NameAndInfo.Number = Number;
		CheckInvariants();
	}


	/**
	 * Builds a long name from the three parts
	 * @param InStatName, Short name
	 * @param InGroup, Group name
	 * @param InCategory, Category name
	 * @param InDescription, Description
	 * @return the packed FName
	 */
	CORE_API static FName ToLongName(FName InStatName, char const* InGroup, char const* InCategory, TCHAR const* InDescription);
	CORE_API static FName GetShortNameFrom(FName InLongName);
	CORE_API static FName GetGroupNameFrom(FName InLongName);
	CORE_API static FName GetGroupCategoryFrom(FName InLongName);
	CORE_API static FString GetDescriptionFrom(FName InLongName);
};


/** Union for easier debugging. */
union UStatData
{
private:
	/** For ST_double. */
	double	Float;
	/** For ST_int64 and IsCycle or IsMemory. */
	int64	Cycles;
	/** For ST_Ptr. */
	uint64	Ptr;
	/** ST_int64 and IsPackedCCAndDuration. */
	uint32	CCAndDuration[2];
	/** For FName. */
	CORE_API const FString GetName() const
	{
		return FName::SafeString( (int32)Cycles );
	}
};

/**
* 16 byte stat message. Everything is a message
*/
struct FStatMessage
{
	/**
	* Generic payload
	*/
	enum
	{
		DATA_SIZE=8,
		DATA_ALIGN=8,
	};
	union
	{
#if	UE_BUILD_DEBUG
		UStatData								DebugStatData;
#endif // UE_BUILD_DEBUG
		TAlignedBytes<DATA_SIZE, DATA_ALIGN>	StatData;
	};

	/**
	* Name and the meta info.
	*/
	FStatNameAndInfo						NameAndInfo;

	FStatMessage()
	{
	}

	/**
	* Copy constructor
	*/
	FORCEINLINE_STATS FStatMessage(FStatMessage const& Other)
		: StatData(Other.StatData)
		, NameAndInfo(Other.NameAndInfo)
	{
	}
	
	/**
	* Build a meta data message
	*/
	FStatMessage(FName InStatName, EStatDataType::Type InStatType, char const* InGroup, char const* InCategory, TCHAR const* InDescription, bool bShouldClearEveryFrame, bool bCycleStat, FPlatformMemory::EMemoryCounterRegion MemoryRegion = FPlatformMemory::MCR_Invalid)
		: NameAndInfo(InStatName, InGroup, InCategory, InDescription, InStatType, bShouldClearEveryFrame, bCycleStat, MemoryRegion)
	{
		NameAndInfo.SetField<EStatOperation>(EStatOperation::SetLongName);
	}

	explicit FORCEINLINE_STATS FStatMessage(FStatNameAndInfo InStatName)
		: NameAndInfo(InStatName)
	{
	}

	/**
	* Clock operation
	*/
	FORCEINLINE_STATS FStatMessage(FName InStatName, EStatOperation::Type InStatOperation)
		: NameAndInfo(InStatName, true)
	{
		NameAndInfo.SetField<EStatOperation>(InStatOperation);
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) == true);

		// these branches are FORCEINLINE_STATS of constants in almost all cases, so they disappear
		if (InStatOperation == EStatOperation::CycleScopeStart || InStatOperation == EStatOperation::CycleScopeEnd)
		{
			GetValue_int64()= int64(FPlatformTime::Cycles());
		}
		else
		{
			checkStats(0);
		}
	}

	/**
	* int64 operation
	*/
	FORCEINLINE_STATS FStatMessage(FName InStatName, EStatOperation::Type InStatOperation, int64 Value, bool bIsCycle)
		: NameAndInfo(InStatName, true)
	{
		NameAndInfo.SetField<EStatOperation>(InStatOperation);
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) == bIsCycle);
		GetValue_int64() = Value;
	}

	/**
	* double operation
	*/
	FORCEINLINE_STATS FStatMessage(FName InStatName, EStatOperation::Type InStatOperation, double Value, bool)
		: NameAndInfo(InStatName, true)
	{
		NameAndInfo.SetField<EStatOperation>(InStatOperation);
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double);
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) == false);
		GetValue_double() = Value;
	}

	/**
	* name operation
	*/
	FORCEINLINE_STATS FStatMessage(FName InStatName, EStatOperation::Type InStatOperation, FName Value, bool)
		: NameAndInfo(InStatName, true)
	{
		NameAndInfo.SetField<EStatOperation>(InStatOperation);
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_FName);
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) == false);
		GetValue_FMinimalName() = NameToMinimalName(Value);
	}

	/**
	 * Ptr operation
	 */
	FORCEINLINE_STATS FStatMessage(FName InStatName, EStatOperation::Type InStatOperation, uint64 Value, bool)
		: NameAndInfo(InStatName, true)
	{
		NameAndInfo.SetField<EStatOperation>(InStatOperation);
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_Ptr);
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) == false);
		GetValue_Ptr() = Value;
	}

	/**
	* Clear any data type
	*/
	FORCEINLINE_STATS void Clear()
	{
		static_assert(sizeof(uint64) == DATA_SIZE, "Bad clear.");
		*(int64*)&StatData = 0;
	}

	/**
	* Payload retrieval and setting methods
	*/
	FORCEINLINE_STATS int64& GetValue_int64()
	{
		static_assert(sizeof(int64) <= DATA_SIZE && alignof(int64) <= DATA_ALIGN, "Bad data for stat message.");
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		return *(int64*)&StatData;
	}
	FORCEINLINE_STATS int64 GetValue_int64() const
	{
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		return *(int64 const*)&StatData;
	}

	FORCEINLINE_STATS uint64& GetValue_Ptr()
	{
		static_assert(sizeof(uint64) <= DATA_SIZE && alignof(uint64) <= DATA_ALIGN, "Bad data for stat message.");
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_Ptr);
		return *(uint64*)&StatData;
	}
	FORCEINLINE_STATS uint64 GetValue_Ptr() const
	{
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_Ptr);
		return *(uint64 const*)&StatData;
	}

	FORCEINLINE_STATS int64 GetValue_Duration() const
	{
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) && NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		if (NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			return FromPackedCallCountDuration_Duration(*(int64 const*)&StatData);
		}
		return *(int64 const*)&StatData;
	}

	FORCEINLINE_STATS uint32 GetValue_CallCount() const
	{
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration) && NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) && NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		return FromPackedCallCountDuration_CallCount(*(int64 const*)&StatData);
	}

	FORCEINLINE_STATS double& GetValue_double()
	{
		static_assert(sizeof(double) <= DATA_SIZE && alignof(double) <= DATA_ALIGN, "Bad data for stat message.");
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double);
		return *(double*)&StatData;
	}

	FORCEINLINE_STATS double GetValue_double() const
	{
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double);
		return *(double const*)&StatData;
	}

	FORCEINLINE_STATS FMinimalName& GetValue_FMinimalName()
	{
		static_assert(sizeof(FMinimalName) <= DATA_SIZE && alignof(FMinimalName) <= DATA_ALIGN, "Bad data for stat message.");
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_FName);
		return *(FMinimalName*)&StatData;
	}

	FORCEINLINE_STATS FMinimalName GetValue_FMinimalName() const
	{
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_FName);
		return *(FMinimalName const*)&StatData;
	}

	FORCEINLINE_STATS FName GetValue_FName() const
	{
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_FName);
		return MinimalNameToName(*(FMinimalName const*)&StatData);
	}
};
template<> struct TIsPODType<FStatMessage> { enum { Value = true }; };

CORE_API void GetPermanentStats(TArray<FStatMessage>& OutStats);

/**
 *	Based on FStatMessage, but supports more than 8 bytes of stat data storage.
 */
template< typename TEnum >
struct TStatMessage
{
	typedef TEnum TStructEnum;
	static const int32 EnumCount = TEnum::Num;

	/**
	* Generic payload
	*/
	enum
	{
		DATA_SIZE=8*EnumCount,
		DATA_ALIGN=8,
	};

	union
	{
#if	UE_BUILD_DEBUG
		UStatData								DebugStatData[EnumCount];
#endif // UE_BUILD_DEBUG
		TAlignedBytes<DATA_SIZE, DATA_ALIGN>	StatData;
	};

	/**
	* Name and the meta info.
	*/
	FStatNameAndInfo							NameAndInfo;

	TStatMessage()
	{}

	/**
	* Copy constructor from FStatMessage
	*/
	explicit FORCEINLINE_STATS TStatMessage(const FStatMessage& Other)
		: NameAndInfo(Other.NameAndInfo)
	{
		// Reset data type and clear all fields.
		NameAndInfo.SetField<EStatDataType>( EStatDataType::ST_None );
		Clear();
	}

	/**
	* Copy constructor
	*/
	explicit FORCEINLINE_STATS TStatMessage(const TStatMessage& Other)
		: NameAndInfo(Other.NameAndInfo)
	{
		// Copy all fields.
		for( int32 FieldIndex = 0; FieldIndex < EnumCount; ++FieldIndex )
		{
			*((int64*)&StatData+FieldIndex) = *((int64*)&Other.StatData+FieldIndex);
		}
	}

	/** Assignment operator for FStatMessage. */
	TStatMessage& operator=(const FStatMessage& Other)
	{
		NameAndInfo = Other.NameAndInfo;
		// Reset data type and clear all fields.
		NameAndInfo.SetField<EStatDataType>( EStatDataType::ST_None );
		Clear();
		return *this;
	}

	/** Fixes stat data type for all fields. */
	void FixStatData( const EStatDataType::Type NewType )
	{
		const EStatDataType::Type OldType = NameAndInfo.GetField<EStatDataType>();
		if( OldType != NewType )
		{
			// Convert from the old type to the new type.
			if( OldType == EStatDataType::ST_int64 && NewType == EStatDataType::ST_double )
			{
				// Get old values.
				int64 OldValues[TEnum::Num];
				for( int32 FieldIndex = 0; FieldIndex < EnumCount; ++FieldIndex )
				{
					OldValues[FieldIndex] = GetValue_int64((typename TEnum::Type)FieldIndex);
				}
				
				// Set new stat data type
				NameAndInfo.SetField<EStatDataType>(NewType);
				for( int32 FieldIndex = 0; FieldIndex < EnumCount; ++FieldIndex )
				{
					GetValue_double((typename TEnum::Type)FieldIndex) = (double)OldValues[FieldIndex];
				}
			}
			else if( OldType == EStatDataType::ST_double && NewType == EStatDataType::ST_int64 )
			{
				// Get old values.
				double OldValues[TEnum::Num];
				for( int32 FieldIndex = 0; FieldIndex < EnumCount; ++FieldIndex )
				{
					OldValues[FieldIndex] = GetValue_double((typename TEnum::Type)FieldIndex);
				}

				// Set new stat data type
				NameAndInfo.SetField<EStatDataType>(NewType);
				for( int32 FieldIndex = 0; FieldIndex < EnumCount; ++FieldIndex )
				{
					GetValue_int64((typename TEnum::Type)FieldIndex) = (int64)OldValues[FieldIndex];
				}
			}
		}
	}

	/**
	* Clear any data type
	*/
	FORCEINLINE_STATS void Clear()
	{
		static_assert(sizeof(uint64) == DATA_SIZE / EnumCount, "Bad clear.");

		for( int32 FieldIndex = 0; FieldIndex < EnumCount; ++FieldIndex )
		{
			*((int64*)&StatData+FieldIndex) = 0;
		}
	}

	/**
	* Payload retrieval and setting methods
	*/
	FORCEINLINE_STATS int64& GetValue_int64( typename TEnum::Type Index )
	{
		static_assert(sizeof(int64) <= DATA_SIZE && alignof(int64) <= DATA_ALIGN, "Bad data for stat message.");
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		checkStats(Index<EnumCount);
		int64& Value = *((int64*)&StatData+(uint32)Index);
		return Value;
	}
	FORCEINLINE_STATS int64 GetValue_int64( typename TEnum::Type Index ) const
	{
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		checkStats(Index<EnumCount);
		const int64 Value = *((int64*)&StatData+(uint32)Index);
		return Value;
	}

	FORCEINLINE_STATS int64 GetValue_Duration( typename TEnum::Type Index ) const
	{
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) && NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		checkStats(Index<EnumCount);
		if (NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			const uint32 Value = FromPackedCallCountDuration_Duration(*((int64 const*)&StatData+(uint32)Index));
			return Value;
		}
		const int64 Value = *((int64 const*)&StatData+(uint32)Index);
		return Value;
	}

	FORCEINLINE_STATS uint32 GetValue_CallCount( typename TEnum::Type Index ) const
	{
		checkStats(NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration) && NameAndInfo.GetFlag(EStatMetaFlags::IsCycle) && NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
		checkStats(Index<EnumCount);
		const uint32 Value = FromPackedCallCountDuration_CallCount(*((int64 const*)&StatData+(uint32)Index));
		return Value;
	}

	FORCEINLINE_STATS double& GetValue_double( typename TEnum::Type Index )
	{
		static_assert(sizeof(double) <= DATA_SIZE && alignof(double) <= DATA_ALIGN, "Bad data for stat message.");
		checkStats(Index<EnumCount);
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double);
		double& Value = *((double*)&StatData+(uint32)Index);
		return Value;
	}

	FORCEINLINE_STATS double GetValue_double( typename TEnum::Type Index ) const
	{
		checkStats(NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double);
		checkStats(Index<EnumCount);
		const double Value = *((double const*)&StatData+(uint32)Index);
		return Value;
	}

	FORCEINLINE_STATS FName GetShortName() const
	{
		return NameAndInfo.GetShortName();
	}

	FORCEINLINE_STATS FString GetDescription() const
	{
		return NameAndInfo.GetDescription();
	}
};

/** Enumerates fields of the FComplexStatMessage. */
struct EComplexStatField
{
	enum Type
	{
		/** Summed inclusive time. */
		IncSum,
		/** Average inclusive time. */
		IncAve,
		/** Maximum inclusive time. */
		IncMax,
		/** Summed exclusive time. */
		ExcSum,
		/** Average exclusive time. */
		ExcAve,
		/** Maximum exclusive time. */
		ExcMax,
		/** Number of enumerates. */
		Num,
	};
};

/**
 *	This type of stat message holds data defined by associated enumeration @see EComplexStatField
 *	By default any of these messages contain a valid data, so check before accessing the data.
 */
typedef TStatMessage<EComplexStatField> FComplexStatMessage;

template<> struct TIsPODType<FComplexStatMessage> { enum { Value = true }; };


struct EStatMessagesArrayConstants
{
	enum
	{
		MESSAGES_CHUNK_SIZE = 64*1024,
	};
};
typedef TChunkedArray<FStatMessage,(uint32)EStatMessagesArrayConstants::MESSAGES_CHUNK_SIZE> FStatMessagesArray;

/**
* A stats packet. Sent between threads. Includes and array of messages and some information about the thread. 
*/
struct FStatPacket
{
	/** Assigned later, this is the frame number this packet is for. @see FStatsThreadState::ScanForAdvance or FThreadStats::DetectAndUpdateCurrentGameFrame */
	int64 Frame;
	/** ThreadId this packet came from **/
	uint32 ThreadId;
	/** type of thread this packet came from **/
	EThreadType::Type ThreadType;
	/** true if this packet has broken callstacks **/
	bool bBrokenCallstacks;
	/** messages in this packet **/
	FStatMessagesArray StatMessages;
	/** Size we presize the message buffer to, currently the max of what we have seen for the last PRESIZE_MAX_NUM_ENTRIES. **/
	TArray<int32> StatMessagesPresize;

	/** constructor **/
	FStatPacket()
		: Frame(1)
		, ThreadId(0)
		, ThreadType(EThreadType::Invalid)
		, bBrokenCallstacks(false)
	{
	}

	/** Copy constructor. !!!CAUTION!!! does not copy the data **/
	FStatPacket(FStatPacket const& Other)
		: Frame(Other.Frame)
		, ThreadId(Other.ThreadId)
		, ThreadType(Other.ThreadType)
		, bBrokenCallstacks(false)
		, StatMessagesPresize(Other.StatMessagesPresize)
	{
	}

	/** Initializes thread related properties for the stats packet. */
	void SetThreadProperties()
	{
		ThreadId = FPlatformTLS::GetCurrentThreadId();
		if (ThreadId == GGameThreadId)
		{
			ThreadType = EThreadType::Game;
		}
		else if (ThreadId == GRenderThreadId)
		{
			ThreadType = EThreadType::Renderer;
		}
		else
		{
			ThreadType = EThreadType::Other;
		}
	}


	void AssignFrame( int64 InFrame )
	{
		Frame = InFrame;
	}
};

/** Helper struct used to monitor the scope of the message. */
struct FStatMessageLock
{
	FStatMessageLock(int32& InMessageScope)
		: MessageScope(InMessageScope)
	{
		MessageScope++;
	}

	~FStatMessageLock()
	{
		MessageScope--;
	}

protected:
	int32& MessageScope;
};

/** Preallocates FThreadStats to avoid dynamic memory allocation. */
struct FThreadStatsPool
{
private:
	enum
	{
		/**
		 *	Number of elements preallocated in the pool.
		 *  Should be enough to handle all threads.
		 *  Increase if you encounter a crash.
		 */
		NUM_ELEMENTS_IN_POOL = 512
	};

	/** Lock free pool of FThreadStats instances. */
	TLockFreePointerListUnordered<FThreadStats, 0> Pool;

public:
	/** Default constructor. */
	CORE_API FThreadStatsPool();

	/** Singleton accessor. */
	CORE_API static FThreadStatsPool& Get()
	{
		static FThreadStatsPool Singleton;
		return Singleton;
	}

	/** Gets an instance from the pool and call the default constructor on it. */
	CORE_API FThreadStats* GetFromPool();

	/** Return an instance to the pool. */
	CORE_API void ReturnToPool(FThreadStats* Instance);
};

/** Fake type to distinguish constructors. */
enum EConstructor
{
	FOR_POOL
};

/**
* This is thread-private information about the stats we are acquiring. Pointers to these objects are held in TLS.
*/
class FThreadStats : FNoncopyable
{
	friend class FStatsMallocProfilerProxy;
	friend class FStatsThreadState;
	friend class FStatsThread;
	friend struct FThreadStatsPool;

	/** Used to control when we are collecting stats. User of the stats system increment and decrement this counter as they need data. **/
	CORE_API static FThreadSafeCounter MasterEnableCounter;
	/** Every time bMasterEnable changes, we update this. This is used to determine frames that have complete data. **/
	CORE_API static FThreadSafeCounter MasterEnableUpdateNumber;
	/** while bMasterEnable (or other things affecting stat collection) is chaning, we lock this. This is used to determine frames that have complete data. **/
	CORE_API static FThreadSafeCounter MasterDisableChangeTagLock;
	/** TLS slot that holds a FThreadStats. **/
	CORE_API static uint32 TlsSlot;
	/** Computed by CheckEnable, the current "master control" for stats collection, based on MasterEnableCounter and a few other things. **/
	CORE_API static bool bMasterEnable;
	/** Set to permanently disable the stats system. **/
	CORE_API static bool bMasterDisableForever;
	/** True if we running in the raw stats mode, all stats processing is disabled, captured stats messages are written in timely manner, memory overhead is minimal. */
	CORE_API static bool bIsRawStatsActive;

	/** The data we are eventually going to send to the stats thread. **/
	FStatPacket Packet;

	/** Current game frame for this thread stats. */
	int32 CurrentGameFrame;

	/** Tracks current stack depth for cycle counters. **/
	int32 ScopeCount;

	/** Tracks current stack depth for cycle counters. **/
	int32 bWaitForExplicitFlush;

	/** 
	 * Tracks the progress of adding a new memory stat message and prevents from using the memory profiler in the scope.
	 * Mostly related to ignoring all memory allocations in AddStatMessage().
	 * Memory usage of the stats messages is handled by the STAT_StatMessagesMemory.
	 */
	int32 MemoryMessageScope;

	/** We have a scoped cycle counter in the FlushRawStats which causes an infinite recursion, ReentranceGuard will solve that issue. */
	bool bReentranceGuard;

	/** Tracks current stack depth for cycle counters. **/
	bool bSawExplicitFlush;

	/** True if this is the stats thread, which needs special handling. **/
	bool bIsStatsThread;

	/** Gathers information about the current thread and sets up the TLS value. **/
	CORE_API FThreadStats();

	/** Constructor used for the pool. */
	CORE_API FThreadStats(EConstructor);

public:
	/** Checks the TLS for a thread packet and if it isn't found, it makes a new one. **/
	static FORCEINLINE_STATS FThreadStats* GetThreadStats()
	{
		FThreadStats* Stats = (FThreadStats*)FPlatformTLS::GetTlsValue(TlsSlot);
		if (!Stats)
		{
			Stats = FThreadStatsPool::Get().GetFromPool();
		}
		return Stats;
	}

	/** This should be called when conditions have changed such that stat collection may now be enabled or not **/
	static CORE_API void CheckEnable();

	/**
	 *	Checks if the game frame has changed and updates the current game frame.
	 *	Used by the flushing mechanism to optimize the memory usage, only valid for packets from other threads.
	 *	@return true, if the frame has changed
	 */
	FORCEINLINE_STATS bool DetectAndUpdateCurrentGameFrame()
	{
		if( Packet.ThreadType == EThreadType::Other )
		{
			FPlatformMisc::MemoryBarrier();
			const bool bFrameHasChanged = FStats::GameThreadStatsFrame > CurrentGameFrame;
			if( bFrameHasChanged )
			{
				CurrentGameFrame = FStats::GameThreadStatsFrame;
				Packet.AssignFrame( CurrentGameFrame );
				return true;
			}
		}
				
		return false;
	}

	/** Maintains the explicit flush. */
	FORCEINLINE_STATS void UpdateExplicitFlush()
	{
		if (Packet.ThreadType != EThreadType::Other && bSawExplicitFlush)
		{
			bSawExplicitFlush = false;
			bWaitForExplicitFlush = 1;
			ScopeCount++; // prevent sends until the next explicit flush
		}
	}

	/** Send any outstanding packets to the stats thread. **/
	CORE_API void Flush(bool bHasBrokenCallstacks = false, bool bForceFlush = false);

	/** Flushes the regular stats, the realtime stats. */
	CORE_API void FlushRegularStats(bool bHasBrokenCallstacks, bool bForceFlush);

	/** Flushes the raw stats, low memory and performance overhead, but not realtime. */
	CORE_API void FlushRawStats(bool bHasBrokenCallstacks = false, bool bForceFlush = false);

	/** Checks the command line whether we want to enable collecting startup stats. */
	static CORE_API void CheckForCollectingStartupStats();

	FORCEINLINE_STATS void AddStatMessage( const FStatMessage& StatMessage )
	{
		FStatMessageLock MessageLock(MemoryMessageScope);
		Packet.StatMessages.AddElement(StatMessage);
	}

public:
	/** This should be called when a thread exits, this deletes FThreadStats from the heap and TLS. **/
	static void Shutdown()
	{
		FThreadStats* Stats = IsThreadingReady() ? (FThreadStats*)FPlatformTLS::GetTlsValue(TlsSlot) : nullptr;
		if (Stats)
		{
			// Send all remaining messages.
			Stats->Flush(false, true);
			FPlatformTLS::SetTlsValue(TlsSlot, nullptr);
			FThreadStatsPool::Get().ReturnToPool(Stats);
		}
	}

	/** Clock operation. **/
	static FORCEINLINE_STATS void AddMessage(FName InStatName, EStatOperation::Type InStatOperation)
	{
		checkStats((InStatOperation == EStatOperation::CycleScopeStart || InStatOperation == EStatOperation::CycleScopeEnd));
		FThreadStats* ThreadStats = GetThreadStats();
		// these branches are handled by the optimizer
		if (InStatOperation == EStatOperation::CycleScopeStart)
		{
			ThreadStats->ScopeCount++;
			ThreadStats->AddStatMessage(FStatMessage(InStatName, InStatOperation));

			if( bIsRawStatsActive )
			{
				ThreadStats->FlushRawStats();
			}
		}
		else if (InStatOperation == EStatOperation::CycleScopeEnd)
		{
			if (ThreadStats->ScopeCount > ThreadStats->bWaitForExplicitFlush)
			{
				ThreadStats->AddStatMessage(FStatMessage(InStatName, InStatOperation));
				ThreadStats->ScopeCount--;
				if (!ThreadStats->ScopeCount)
				{
					ThreadStats->Flush();
				}
				else if( bIsRawStatsActive )
				{
					ThreadStats->FlushRawStats();
				}
			}
			// else we dumped this frame without closing scope, so we just drop the closes on the floor
		}
	}

	/** Any non-clock operation with an ordinary payload. **/
	template<typename TValue>
	static FORCEINLINE_STATS void AddMessage(FName InStatName, EStatOperation::Type InStatOperation, TValue Value, bool bIsCycle = false)
	{
		if (!InStatName.IsNone() && WillEverCollectData() && IsThreadingReady())
		{
			FThreadStats* ThreadStats = GetThreadStats();
			ThreadStats->AddStatMessage(FStatMessage(InStatName, InStatOperation, Value, bIsCycle));
			if(!ThreadStats->ScopeCount)
			{
				ThreadStats->Flush();
			}
			else if( bIsRawStatsActive )
			{
				ThreadStats->FlushRawStats();
			}
		}
	}

	/** Pseudo-Memory operation. */
	template<typename TValue>
	FORCEINLINE_STATS void AddMemoryMessage( FName InStatName, TValue Value )
	{
		AddStatMessage(FStatMessage(InStatName, EStatOperation::Memory, Value, false));
	}

	/** 
	 * Used to force a flush at the next available opportunity. This is not useful for threads other than the main and render thread. 
	 * if DiscardCallstack is true, we also dump call stacks, making the next available opportunity at the next stat or stat close.
	**/
	static CORE_API void ExplicitFlush(bool DiscardCallstack = false);

	/** Return true if we are currently collecting data **/
	static FORCEINLINE_STATS bool IsCollectingData()
	{
		return bMasterEnable;
	}
	static FORCEINLINE_STATS bool IsCollectingData(TStatId StatId)
	{
		// we don't test StatId for nullptr here because we assume it is non-null. If it is nullptr, that indicates a problem with higher level code.
		return !StatId.IsNone() && IsCollectingData();
	}

	/** Return true if we are currently collecting data **/
	static FORCEINLINE_STATS bool WillEverCollectData()
	{
		return !bMasterDisableForever;
	}

	/** Return true if the threading is ready **/
	static FORCEINLINE_STATS bool IsThreadingReady()
	{
		return !!TlsSlot;
	}

	/** Indicate that you would like the system to begin collecting data, if it isn't already collecting data. Think reference count. **/
	static FORCEINLINE_STATS void MasterEnableAdd(int32 Value = 1)
	{
		MasterEnableCounter.Add(Value);
		CheckEnable();
	}

	/** Indicate that you no longer need stat data, if nobody else needs stat data, then no stat data will be collected. Think reference count. **/
	static FORCEINLINE_STATS void MasterEnableSubtract(int32 Value = 1)
	{
		MasterEnableCounter.Subtract(Value);
		CheckEnable();
	}

	/** Indicate that you no longer need stat data, forever. **/
	static FORCEINLINE_STATS void MasterDisableForever()
	{
		bMasterDisableForever = true;
		CheckEnable();
	}

	/** This is called before we start to change something that will invalidate. **/
	static FORCEINLINE_STATS void MasterDisableChangeTagLockAdd(int32 Value = 1)
	{
		MasterDisableChangeTagLock.Add(Value);
		FPlatformMisc::MemoryBarrier();
		MasterEnableUpdateNumber.Increment();
	}

	/** Indicate that you no longer need stat data, if nobody else needs stat data, then no stat data will be collected. Think reference count. **/
	static FORCEINLINE_STATS void MasterDisableChangeTagLockSubtract(int32 Value = 1)
	{
		FPlatformMisc::MemoryBarrier();
		MasterEnableUpdateNumber.Increment();
		FPlatformMisc::MemoryBarrier();
		MasterDisableChangeTagLock.Subtract(Value);
	}

	/** Everytime master enable changes, this number increases. This is used to determine full frames. **/
	static FORCEINLINE_STATS int32 MasterDisableChangeTag()
	{
		if (MasterDisableChangeTagLock.GetValue())
		{
			// while locked we are continually invalid, so we will just keep giving unique numbers
			return MasterEnableUpdateNumber.Increment();
		}
		return MasterEnableUpdateNumber.GetValue();
	}

	/** Call this if something disrupts data gathering. For example when the render thread is killed, data is abandoned.**/
	static FORCEINLINE_STATS void FrameDataIsIncomplete()
	{
		FPlatformMisc::MemoryBarrier();
		MasterEnableUpdateNumber.Increment();
		FPlatformMisc::MemoryBarrier();
	}

	/** Enables the raw stats mode. */
	static FORCEINLINE_STATS void EnableRawStats() TSAN_SAFE
	{
		bIsRawStatsActive = true;
		FPlatformMisc::MemoryBarrier();
	}

	/** Disables the raw stats mode. */
	static FORCEINLINE_STATS void DisableRawStats() TSAN_SAFE
	{
		bIsRawStatsActive = false;
		FPlatformMisc::MemoryBarrier();
	}

	/** Called by launch engine loop to start the stats thread **/
	static CORE_API void StartThread();
	/** Called by launch engine loop to stop the stats thread **/
	static CORE_API void StopThread();
	/** Called by the engine loop to make sure the stats thread isn't getting too far behind. **/
	static CORE_API void WaitForStats();
};


/**
 * This is a utility class for counting the number of cycles during the
 * lifetime of the object. It creates messages for the stats thread.
 */
class FCycleCounter
{
	/** Name of the stat, usually a short name **/
	FName StatId;

public:

	/**
	 * Pushes the specified stat onto the hierarchy for this thread. Starts
	 * the timing of the cycles used
	 */
	FORCEINLINE_STATS void Start( TStatId InStatId, bool bAlways = false )
	{
		if( (bAlways && FThreadStats::WillEverCollectData() && InStatId.IsValidStat()) || FThreadStats::IsCollectingData( InStatId ) )
		{
			StatId = InStatId.GetName();
			FThreadStats::AddMessage( StatId, EStatOperation::CycleScopeStart );

			// Emit named event for active cycle stat.
			if( GCycleStatsShouldEmitNamedEvents > 0 )
			{
#if	PLATFORM_USES_ANSI_STRING_FOR_EXTERNAL_PROFILING
				FPlatformMisc::BeginNamedEvent( FColor( 0 ), InStatId.GetStatDescriptionANSI() );
#else
				FPlatformMisc::BeginNamedEvent( FColor( 0 ), InStatId.GetStatDescriptionWIDE() );
#endif // PLATFORM_USES_ANSI_STRING_FOR_EXTERNAL_PROFILING
			}
		}
	}

	/**
	 * Stops the capturing and stores the result
	 */
	FORCEINLINE_STATS void Stop()
	{
		if( !StatId.IsNone() )
		{
			FThreadStats::AddMessage(StatId, EStatOperation::CycleScopeEnd);

			// End named event for active cycle stat.
			if( GCycleStatsShouldEmitNamedEvents > 0 )
			{
				FPlatformMisc::EndNamedEvent();
			}
		}
	}

	/**
	 * Stops the capturing and stores the result and resets the stat id.
	 */
	FORCEINLINE_STATS void StopAndResetStatId()
	{
		Stop();
		StatId = NAME_None;
	}
};

class FSimpleScopeSecondsStat
{
public:

	FSimpleScopeSecondsStat(TStatId InStatId)
		: StartTime(FPlatformTime::Seconds())
		, StatId(InStatId)
	{

	}

	virtual ~FSimpleScopeSecondsStat()
	{
		double TotalTime = FPlatformTime::Seconds() - StartTime;
		FThreadStats::AddMessage(StatId.GetName(), EStatOperation::Add, TotalTime);
	}

private:

	double StartTime;
	TStatId StatId;
};

/** Manages startup messages, usually to update the metadata. */
class FStartupMessages
{
	friend class FStatsThread;

	TArray<FStatMessage> DelayedMessages;
	FCriticalSection CriticalSection;

public:
	/** Adds a thread metadata. */
	CORE_API void AddThreadMetadata( const FName InThreadFName, uint32 InThreadID );

	/** Adds a regular metadata. */
	CORE_API void AddMetadata( FName InStatName, const TCHAR* InStatDesc, const char* InGroupName, const char* InGroupCategory, const TCHAR* InGroupDesc, bool bShouldClearEveryFrame, EStatDataType::Type InStatType, bool bCycleStat, FPlatformMemory::EMemoryCounterRegion InMemoryRegion = FPlatformMemory::MCR_Invalid );

	/** Access the singleton. */
	CORE_API static FStartupMessages& Get();
};

/**
* Single interface to control high performance stat disable
*/
class IStatGroupEnableManager
{
public:
	/** Return the singleton, must be called from the main thread. **/
	CORE_API static IStatGroupEnableManager& Get();

	/** virtual destructor. **/
	virtual ~IStatGroupEnableManager() 
	{
	}

	/**
	 * Returns a pointer to a bool (valid forever) that determines if this group is active
	 * This should be CACHED. We will get a few calls from different stats and different threads and stuff, but once things are "warmed up", this should NEVER be called.
	 * @param InGroup, group to look up
	 * @param InCategory, the category the group belongs to
	 * @param bDefaultEnable, If this is the first time this group has been set up, this sets the default enable value for this group.
	 * @param bShouldClearEveryFrame, If this is true, this is a memory counter or an accumulator
	 * @return a pointer to a FName (valid forever) that determines if this group is active
	 */
	virtual TStatId GetHighPerformanceEnableForStat(FName StatShortName, const char* InGroup, const char* InCategory, bool bDefaultEnable, bool bShouldClearEveryFrame, EStatDataType::Type InStatType, TCHAR const* InDescription, bool bCycleStat, FPlatformMemory::EMemoryCounterRegion MemoryRegion = FPlatformMemory::MCR_Invalid) = 0;

	/**
	 * Enables or disabled a particular group of stats
	 * Disabling a memory group, ever, is usually a bad idea
	 * @param Group, group to look up
	 * @param Enable, this should be true if we want to collect stats for this group
	 */
	virtual void SetHighPerformanceEnableForGroup(FName Group, bool Enable)=0;

	/**
	 * Enables or disabled all groups of stats
	 * Disabling a memory group, ever, is usually a bad idea. SO if you disable all groups, you will wreck memory stats usually.
	 * @param Enable, this should be true if we want to collect stats for all groups
	 */
	virtual void SetHighPerformanceEnableForAllGroups(bool Enable)=0;

	/**
	 * Resets all stats to their default collection state, which was set when they were looked up initially
	 */
	virtual void ResetHighPerformanceEnableForAllGroups()=0;

	/**
	 * Runs a group command
	 * @param Cmd, Command to run
	 */
	virtual void StatGroupEnableManagerCommand(FString const& Cmd)=0;

	/** Updates memory usage. */
	virtual void UpdateMemoryUsage() = 0;
};


/**
**********************************************************************************
*/
struct FThreadSafeStaticStatBase
{
protected:
	mutable TStatIdData* HighPerformanceEnable; // must be uninitialized, because we need atomic initialization
	CORE_API void DoSetup(const char* InStatName, const TCHAR* InStatDesc, const char* InGroupName, const char* InGroupCategory, const TCHAR* InGroupDesc, bool bDefaultEnable, bool bShouldClearEveryFrame, EStatDataType::Type InStatType, bool bCycleStat, FPlatformMemory::EMemoryCounterRegion InMemoryRegion) const;
};

template<class TStatData, bool TCompiledIn>
struct FThreadSafeStaticStatInner : public FThreadSafeStaticStatBase
{
	FORCEINLINE_STATS TStatId GetStatId() const
	{
		static_assert(sizeof(HighPerformanceEnable) == sizeof(TStatId), "Unsafe cast requires these to be the same thing.");
		if (!HighPerformanceEnable)
		{
			DoSetup(TStatData::GetStatName(), TStatData::GetDescription(), TStatData::TGroup::GetGroupName(), TStatData::TGroup::GetGroupCategory(), TStatData::TGroup::GetDescription(), TStatData::TGroup::IsDefaultEnabled(), TStatData::IsClearEveryFrame(), TStatData::GetStatType(), TStatData::IsCycleStat(), TStatData::GetMemoryRegion() );
		}
		return *(TStatId*)(&HighPerformanceEnable);
	}
	FORCEINLINE FName GetStatFName() const
	{
		return GetStatId().GetName();
	}
};

template<class TStatData>
struct FThreadSafeStaticStatInner<TStatData, false>
{
	FORCEINLINE TStatId GetStatId()
	{
		return TStatId();
	}
	FORCEINLINE FName GetStatFName() const
	{
		return FName();
	}
};

template<class TStatData>
struct FThreadSafeStaticStat : public FThreadSafeStaticStatInner<TStatData, TStatData::TGroup::CompileTimeEnable>
{
};

#define DECLARE_STAT_GROUP(Description, StatName, StatCategory, InDefaultEnable, InCompileTimeEnable) \
struct FStatGroup_##StatName\
{ \
	enum \
	{ \
		DefaultEnable = InDefaultEnable, \
		CompileTimeEnable = InCompileTimeEnable \
	}; \
	static FORCEINLINE const char* GetGroupName() \
	{ \
		return #StatName; \
	} \
	static FORCEINLINE const char* GetGroupCategory() \
	{ \
		return #StatCategory; \
	} \
	static FORCEINLINE const TCHAR* GetDescription() \
	{ \
		return Description; \
	} \
	static FORCEINLINE bool IsDefaultEnabled() \
	{ \
		return (bool)DefaultEnable; \
	} \
	static FORCEINLINE bool IsCompileTimeEnable() \
	{ \
		return (bool)CompileTimeEnable; \
	} \
};

#define DECLARE_STAT(Description, StatName, GroupName, StatType, bShouldClearEveryFrame, bCycleStat, MemoryRegion) \
struct FStat_##StatName\
{ \
	typedef FStatGroup_##GroupName TGroup; \
	static FORCEINLINE const char* GetStatName() \
	{ \
		return #StatName; \
	} \
	static FORCEINLINE const TCHAR* GetDescription() \
	{ \
		return Description; \
	} \
	static FORCEINLINE EStatDataType::Type GetStatType() \
	{ \
		return StatType; \
	} \
	static FORCEINLINE bool IsClearEveryFrame() \
	{ \
		return bShouldClearEveryFrame; \
	} \
	static FORCEINLINE bool IsCycleStat() \
	{ \
		return bCycleStat; \
	} \
	static FORCEINLINE FPlatformMemory::EMemoryCounterRegion GetMemoryRegion() \
	{ \
		return MemoryRegion; \
	} \
};

#define GET_STATID(Stat) (StatPtr_##Stat.GetStatId())
#define GET_STATFNAME(Stat) (StatPtr_##Stat.GetStatFName())
#define GET_STATDESCRIPTION(Stat) (FStat_##Stat::GetDescription())

#define STAT_GROUP_TO_FStatGroup(Group) FStatGroup_##Group

/*-----------------------------------------------------------------------------
	Local
-----------------------------------------------------------------------------*/

#define DEFINE_STAT(Stat) \
	struct FThreadSafeStaticStat<FStat_##Stat> StatPtr_##Stat;

#define RETURN_QUICK_DECLARE_CYCLE_STAT(StatId,GroupId) \
	DECLARE_STAT(TEXT(#StatId),StatId,GroupId,EStatDataType::ST_int64, true, true, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId) \
	return GET_STATID(StatId);

#define QUICK_USE_CYCLE_STAT(StatId,GroupId) [](){ RETURN_QUICK_DECLARE_CYCLE_STAT(StatId, GroupId); }()

#define DECLARE_CYCLE_STAT(CounterName,StatId,GroupId) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, true, true, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)
#define DECLARE_FLOAT_COUNTER_STAT(CounterName,StatId,GroupId) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_double, true, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)
#define DECLARE_DWORD_COUNTER_STAT(CounterName,StatId,GroupId) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, true, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)
#define DECLARE_FLOAT_ACCUMULATOR_STAT(CounterName,StatId,GroupId) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_double, false, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)
#define DECLARE_DWORD_ACCUMULATOR_STAT(CounterName,StatId,GroupId) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)

/** FName stat that allows sending a string based data. */
#define DECLARE_FNAME_STAT(CounterName,StatId,GroupId) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_FName, false, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)

/** This is a fake stat, mostly used to implement memory message or other custom stats that don't easily fit into the system. */
#define DECLARE_PTR_STAT(CounterName,StatId,GroupId)\
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_Ptr, false, false, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(StatId)

#define DECLARE_MEMORY_STAT(CounterName,StatId,GroupId) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_Physical); \
	static DEFINE_STAT(StatId)

#define DECLARE_MEMORY_STAT_POOL(CounterName,StatId,GroupId,Pool) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, Pool); \
	static DEFINE_STAT(StatId)

/*-----------------------------------------------------------------------------
	Extern
-----------------------------------------------------------------------------*/

#define DECLARE_CYCLE_STAT_EXTERN(CounterName,StatId,GroupId, APIX) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, true, true, FPlatformMemory::MCR_Invalid); \
	extern APIX DEFINE_STAT(StatId);
#define DECLARE_FLOAT_COUNTER_STAT_EXTERN(CounterName,StatId,GroupId, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_double, true, false, FPlatformMemory::MCR_Invalid); \
	extern API DEFINE_STAT(StatId);
#define DECLARE_DWORD_COUNTER_STAT_EXTERN(CounterName,StatId,GroupId, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, true, false, FPlatformMemory::MCR_Invalid); \
	extern API DEFINE_STAT(StatId);
#define DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(CounterName,StatId,GroupId, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_double, false, false, FPlatformMemory::MCR_Invalid); \
	extern API DEFINE_STAT(StatId);
#define DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(CounterName,StatId,GroupId, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_Invalid); \
	extern API DEFINE_STAT(StatId);

/** FName stat that allows sending a string based data. */
#define DECLARE_FNAME_STAT_EXTERN(CounterName,StatId,GroupId, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_FName, false, false, FPlatformMemory::MCR_Invalid); \
	extern API DEFINE_STAT(StatId);

/** This is a fake stat, mostly used to implement memory message or other custom stats that don't easily fit into the system. */
#define DECLARE_PTR_STAT_EXTERN(CounterName,StatId,GroupId, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_Ptr, false, false, FPlatformMemory::MCR_Invalid); \
	extern API DEFINE_STAT(StatId);

#define DECLARE_MEMORY_STAT_EXTERN(CounterName,StatId,GroupId, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, FPlatformMemory::MCR_Physical); \
	extern API DEFINE_STAT(StatId);

#define DECLARE_MEMORY_STAT_POOL_EXTERN(CounterName,StatId,GroupId,Pool, API) \
	DECLARE_STAT(CounterName,StatId,GroupId,EStatDataType::ST_int64, false, false, Pool); \
	extern API DEFINE_STAT(StatId);

/** Macro for declaring group factory instances */
#define DECLARE_STATS_GROUP(GroupDesc, GroupId, GroupCat) \
	DECLARE_STAT_GROUP(GroupDesc, GroupId, GroupCat, true, true);

#define DECLARE_STATS_GROUP_VERBOSE(GroupDesc, GroupId, GroupCat) \
	DECLARE_STAT_GROUP(GroupDesc, GroupId, GroupCat, false, true);

#define DECLARE_STATS_GROUP_MAYBE_COMPILED_OUT(GroupDesc, GroupId, GroupCat, CompileIn) \
	DECLARE_STAT_GROUP(GroupDesc, GroupId, GroupCat, false, CompileIn);

#define DECLARE_SCOPE_CYCLE_COUNTER(CounterName,Stat,GroupId) \
	DECLARE_STAT(CounterName,Stat,GroupId,EStatDataType::ST_int64, true, true, FPlatformMemory::MCR_Invalid); \
	static DEFINE_STAT(Stat) \
	FScopeCycleCounter CycleCount_##Stat(GET_STATID(Stat));

#define QUICK_SCOPE_CYCLE_COUNTER(Stat) \
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#Stat),Stat,STATGROUP_Quick)

#define SCOPE_CYCLE_COUNTER(Stat) \
	FScopeCycleCounter CycleCount_##Stat(GET_STATID(Stat));

#define CONDITIONAL_SCOPE_CYCLE_COUNTER(Stat,bCondition) \
	FScopeCycleCounter CycleCount_##Stat(bCondition ? GET_STATID(Stat) : TStatId());

#define SCOPE_SECONDS_ACCUMULATOR(Stat) \
	FSimpleScopeSecondsStat SecondsAccum_##Stat(GET_STATID(Stat));

#define SET_CYCLE_COUNTER(Stat,Cycles) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Set, int64(Cycles), true);\
}

#define INC_DWORD_STAT(Stat) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Add, int64(1));\
}
#define INC_FLOAT_STAT_BY(Stat, Amount) \
{\
	if (Amount != 0.0f) \
		FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Add, double(Amount));\
}
#define INC_DWORD_STAT_BY(Stat, Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Add, int64(Amount));\
}
#define INC_DWORD_STAT_FNAME_BY(StatFName, Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(StatFName, EStatOperation::Add, int64(Amount));\
}
#define INC_MEMORY_STAT_BY(Stat, Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Add, int64(Amount));\
}
#define DEC_DWORD_STAT(Stat) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Subtract, int64(1));\
}
#define DEC_FLOAT_STAT_BY(Stat,Amount) \
{\
	if (Amount != 0.0f) \
		FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Subtract, double(Amount));\
}
#define DEC_DWORD_STAT_BY(Stat,Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Subtract, int64(Amount));\
}
#define DEC_DWORD_STAT_FNAME_BY(StatFName,Amount) \
{\
	if (Amount != 0) \
 		FThreadStats::AddMessage(StatFName, EStatOperation::Subtract, int64(Amount));\
}
#define DEC_MEMORY_STAT_BY(Stat,Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Subtract, int64(Amount));\
}
#define SET_MEMORY_STAT(Stat,Value) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Set, int64(Value));\
}
#define SET_DWORD_STAT(Stat,Value) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Set, int64(Value));\
}
#define SET_FLOAT_STAT(Stat,Value) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::Set, double(Value));\
}
#define STAT_ADD_CUSTOMMESSAGE_NAME(Stat,Value) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::SpecialMessageMarker, FName(Value));\
}
#define STAT_ADD_CUSTOMMESSAGE_PTR(Stat,Value) \
{\
	FThreadStats::AddMessage(GET_STATFNAME(Stat), EStatOperation::SpecialMessageMarker, uint64(Value));\
}

#define SET_CYCLE_COUNTER_FName(Stat,Cycles) \
{\
	FThreadStats::AddMessage(Stat, EStatOperation::Set, int64(Cycles), true);\
}

#define INC_DWORD_STAT_FName(Stat) \
{\
	FThreadStats::AddMessage(Stat, EStatOperation::Add, int64(1));\
}
#define INC_FLOAT_STAT_BY_FName(Stat, Amount) \
{\
	if (Amount != 0.0f) \
		FThreadStats::AddMessage(Stat, EStatOperation::Add, double(Amount));\
}
#define INC_DWORD_STAT_BY_FName(Stat, Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(Stat, EStatOperation::Add, int64(Amount));\
}
#define INC_MEMORY_STAT_BY_FName(Stat, Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(Stat, EStatOperation::Add, int64(Amount));\
}
#define DEC_DWORD_STAT_FName(Stat) \
{\
	FThreadStats::AddMessage(Stat, EStatOperation::Subtract, int64(1));\
}
#define DEC_FLOAT_STAT_BY_FName(Stat,Amount) \
{\
	if (Amount != 0.0f) \
		FThreadStats::AddMessage(Stat, EStatOperation::Subtract, double(Amount));\
}
#define DEC_DWORD_STAT_BY_FName(Stat,Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(Stat, EStatOperation::Subtract, int64(Amount));\
}
#define DEC_MEMORY_STAT_BY_FName(Stat,Amount) \
{\
	if (Amount != 0) \
		FThreadStats::AddMessage(Stat, EStatOperation::Subtract, int64(Amount));\
}
#define SET_MEMORY_STAT_FName(Stat,Value) \
{\
	FThreadStats::AddMessage(Stat, EStatOperation::Set, int64(Value));\
}
#define SET_DWORD_STAT_FName(Stat,Value) \
{\
	FThreadStats::AddMessage(Stat, EStatOperation::Set, int64(Value));\
}
#define SET_FLOAT_STAT_FName(Stat,Value) \
{\
	FThreadStats::AddMessage(Stat, EStatOperation::Set, double(Value));\
}


/**
 * Unique group identifiers. Note these don't have to defined in this header
 * but they do have to be unique. You're better off defining these in your
 * own headers/cpp files
 */
DECLARE_STATS_GROUP(TEXT("AI"),STATGROUP_AI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Anim"),STATGROUP_Anim, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Async I/O"),STATGROUP_AsyncIO, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Audio"), STATGROUP_Audio, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Beam Particles"),STATGROUP_BeamParticles, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("CPU Stalls"), STATGROUP_CPUStalls, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Canvas"),STATGROUP_Canvas, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Character"), STATGROUP_Character, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Collision"),STATGROUP_Collision, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("CollisionTags"), STATGROUP_CollisionTags, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("CollisionVerbose"),STATGROUP_CollisionVerbose, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("D3D11RHI"),STATGROUP_D3D11RHI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("DDC"),STATGROUP_DDC, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Default Stat Group"),STATGROUP_Default, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Engine"),STATGROUP_Engine, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("FPS Chart"),STATGROUP_FPSChart, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("GPU"), STATGROUP_GPU, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("GPU Particles"),STATGROUP_GPUParticles, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Game"),STATGROUP_Game, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("GPU Defrag"), STATGROUP_GPUDEFRAG, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Gnm"),STATGROUP_PS4RHI, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("GnmVerbose"), STATGROUP_PS4RHIVERBOSE, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Init Views"),STATGROUP_InitViews, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Landscape"),STATGROUP_Landscape, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Light Rendering"),STATGROUP_LightRendering, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("LoadTime"), STATGROUP_LoadTime, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("LoadTimeVerbose"), STATGROUP_LoadTimeVerbose, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("MathVerbose"), STATGROUP_MathVerbose, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Memory Allocator"),STATGROUP_MemoryAllocator, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Memory Platform"),STATGROUP_MemoryPlatform, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Memory StaticMesh"),STATGROUP_MemoryStaticMesh, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Memory"),STATGROUP_Memory, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Mesh Particles"),STATGROUP_MeshParticles, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Metal"),STATGROUP_MetalRHI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Morph"),STATGROUP_MorphTarget, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Navigation"),STATGROUP_Navigation, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Net"),STATGROUP_Net, STATCAT_Advanced);

#if !UE_BUILD_SHIPPING
DECLARE_STATS_GROUP(TEXT("Packet"),STATGROUP_Packet, STATCAT_Advanced);
#endif

DECLARE_STATS_GROUP(TEXT("Object"),STATGROUP_Object, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("ObjectVerbose"),STATGROUP_ObjectVerbose, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("OpenGL RHI"),STATGROUP_OpenGLRHI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Pak File"),STATGROUP_PakFile, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Particle Mem"),STATGROUP_ParticleMem, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Particles"),STATGROUP_Particles, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Physics"),STATGROUP_Physics, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Platform"), STATGROUP_Platform, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Profiler"), STATGROUP_Profiler, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Quick"), STATGROUP_Quick, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("RHI"),STATGROUP_RHI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Render Thread"),STATGROUP_RenderThreadProcessing, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Render Target Pool"), STATGROUP_RenderTargetPool, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Scene Memory"),STATGROUP_SceneMemory, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Scene Rendering"),STATGROUP_SceneRendering, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Scene Update"),STATGROUP_SceneUpdate, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Server CPU"),STATGROUP_ServerCPU, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Shader Compiling"),STATGROUP_ShaderCompiling, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Shader Compression"),STATGROUP_Shaders, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Shadow Rendering"),STATGROUP_ShadowRendering, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Stat System"),STATGROUP_StatSystem, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Streaming Details"),STATGROUP_StreamingDetails, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Streaming"),STATGROUP_Streaming, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Target Platform"),STATGROUP_TargetPlatform, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Text"),STATGROUP_Text, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("ThreadPool Async Tasks"),STATGROUP_ThreadPoolAsyncTasks, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Threading"),STATGROUP_Threading, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Threads"),STATGROUP_Threads, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Tickables"),STATGROUP_Tickables, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Trail Particles"),STATGROUP_TrailParticles, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("UI"),STATGROUP_UI, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("UObjects"),STATGROUP_UObjects, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("User"),STATGROUP_User, STATCAT_Advanced);

// WaveWorks Start
DECLARE_STATS_GROUP(TEXT("WaveWorksD3D11"), STATGROUP_WaveWorksD3D11, STATCAT_Advanced);
// WaveWorks End

DECLARE_CYCLE_STAT_EXTERN(TEXT("FrameTime"),STAT_FrameTime,STATGROUP_Engine, CORE_API);
DECLARE_FNAME_STAT_EXTERN(TEXT("NamedMarker"),STAT_NamedMarker,STATGROUP_StatSystem, CORE_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("Seconds Per Cycle"),STAT_SecondsPerCycle,STATGROUP_Engine, CORE_API);

#endif
