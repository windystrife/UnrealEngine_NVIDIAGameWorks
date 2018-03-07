// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"

// TSize integer values separated by '.' e.g. "120.210.11.22"
// made to compare different GPU driver versions
// handles whitespace
// if there are to too many numbers we take the left most ones
// comparison operators (for driver comparison)
template <int32 TSize>
class FMultiInt
{
public:
	// constructor
	FMultiInt()
	{
		for(uint32 i = 0; i < Size; ++i)
		{
			Value[i] = 0;
		}
	}

	FMultiInt(const TCHAR* In)
	{
		Parse(In);
	}

	// like Parse but it doesn't alter the pointer
	void GetValue(const TCHAR* In)
	{
		const TCHAR* Cpy = In;

		Parse(Cpy);
	}
	// @param In  "123.456" "132", pointer is advanced to be after the data
	// if there isn't enough input values, the rightmost value gets set
	void Parse(const TCHAR*& In)
	{
		check(In);
		// clear
		*this = FMultiInt();

		// find out how many numbers we have
		uint32 NumberCount = 1;
		{
			const TCHAR* p = In;

			while(!IsSeparator(*p))
			{
				// '.' separates numbers
				if(*p == TCHAR('.'))
				{
					++NumberCount;
				}
				++p;
			}
		}

		NumberCount = FMath::Min(NumberCount, Size);

		// parse the data
		{
			const TCHAR* p = In;
			for(uint32 i = 0; i < NumberCount; ++i)
			{
				Value[Size - NumberCount + i] = FCString::Atoi(p);

				// go to next '.', operator or end
				while(!IsSeparator(*p) && *p != TCHAR('.'))
				{
					++p;
				}

				// jump over '.'
				if(*p == TCHAR('.'))
				{
					++p;
				}
			}

			In = p;
		}
	}

	// the base comparison operator, we derive the others from it
	bool operator>(const FMultiInt<TSize>& rhs) const
	{
		for(uint32 i = 0 ; i < Size; ++i)
		{
			if(Value[i] > rhs.Value[i])
			{
				return true; 
			}
			if(Value[i] < rhs.Value[i])
			{
				return false; 
			}
		}

		return false;
	}
	// could be optimized but doesn't have to be fast
	bool operator==(const FMultiInt<TSize>& rhs) const
	{
		// map to existing operators
		return !(*this > rhs) && !(rhs > *this);
	}
	// could be optimized but doesn't have to be fast
	bool operator<(const FMultiInt<TSize>& rhs) const
	{
		// map to existing operators
		return rhs > *this;
	}
	// could be optimized but doesn't have to be fast
	bool operator!=(const FMultiInt<TSize>& rhs) const
	{
		// map to existing operators
		return !(*this == rhs);
	}
	// could be optimized but doesn't have to be fast
	bool operator>=(const FMultiInt<TSize>& rhs) const
	{
		// map to existing operators
		return (*this == rhs) || (*this > rhs);
	}
	// could be optimized but doesn't have to be fast
	bool operator<=(const FMultiInt<TSize>& rhs) const
	{
		// map to existing operators
		return (*this == rhs) || (*this < rhs);
	}

	static const uint32 Size = TSize;

	// [0]:left .. [Size-1]:right
	uint32 Value[Size];

private:
	bool IsSeparator(const TCHAR c)
	{
		// comparison operators are considered separators (e.g. "1.21 < 0.121")
		return c == 0
			|| c == TCHAR('=')
			|| c == TCHAR('!')
			|| c == TCHAR('<')
			|| c == TCHAR('&')
			|| c == TCHAR('|')
			|| c == TCHAR('>');
	}
};

// used to compare driver versions in Hardware.ini
enum EComparisonOp
{
	ECO_Unknown, ECO_Equal, ECO_NotEqual, ECO_Larger, ECO_LargerThan, ECO_Smaller, ECO_SmallerThan
};

// Find the comparison enum based on input text
// @param In pointer is advanced to be after the data
inline EComparisonOp ParseComparisonOp(const TCHAR*& In)
{
	if(In[0] == '=' && In[1] == '=')
	{
		In += 2;
		return ECO_Equal;
	}
	if(In[0] == '!' && In[1] == '=')
	{
		In += 2;
		return ECO_NotEqual;
	}
	if(*In == '>')
	{
		++In;
		if(*In == '=')
		{
			++In;
			return ECO_LargerThan;
		}
		return ECO_Larger;
	}
	if(*In == '<')
	{
		++In;
		if(*In == '=')
		{
			++In;
			return ECO_SmallerThan;
		}
		return ECO_Smaller;
	}

	return ECO_Unknown;
}

// general comparison with the comparison operator given as enum
template <class T> bool Compare(const T& A, EComparisonOp Op, const T& B)
{
	switch(Op)
	{
		case ECO_Equal:			return A == B;
		case ECO_NotEqual:		return A != B;
		case ECO_Larger:		return A >  B;
		case ECO_LargerThan:	return A >= B;
		case ECO_Smaller:		return A <  B;
		case ECO_SmallerThan:	return A <= B;
	}
	check(0);
	return false;
}

// useful to express very simple math, later we might extend to express a range e.g. ">10 && <12.121"
// @param InOpWithVersion e.g. "<=220.2"
// @param CurrentVersion e.g. "219.1"
inline bool CompareStringOp(const TCHAR* InOpWithMultiInt, const TCHAR* CurrentMultiInt)
{
	const TCHAR* p = InOpWithMultiInt;

	EComparisonOp Op = ParseComparisonOp(p);

	// it makes sense to compare for equal if there is no operator
	if(Op == ECO_Unknown)
	{
		Op = ECO_Equal;
	}

	FMultiInt<6> A, B;
		
	A.GetValue(CurrentMultiInt);
	B.Parse(p);

	return Compare(A, Op, B);
}


// video driver details
struct FGPUDriverInfo
{
	FGPUDriverInfo()
		: VendorId(0)
	{
	}

	// DirectX VendorId, 0 if not set, use functions below to set/get
	uint32 VendorId;
	// e.g. "NVIDIA GeForce GTX 680" or "AMD Radeon R9 200 / HD 7900 Series"
	FString DeviceDescription;
	// e.g. "NVIDIA" or "Advanced Micro Devices, Inc."
	FString ProviderName;
	// e.g. "15.200.1062.1004"(AMD)
	// e.g. "9.18.13.4788"(NVIDIA) first number is Windows version (e.g. 7:Vista, 6:XP, 4:Me, 9:Win8(1), 10:Win7), last 5 have the UserDriver version encoded
	// also called technical version number (https://wiki.mozilla.org/Blocklisting/Blocked_Graphics_Drivers)
	// TEXT("Unknown") if driver detection failed
	FString InternalDriverVersion;	
	// e.g. "Catalyst 15.7.1"(AMD) or "Crimson 15.7.1"(AMD) or "347.88"(NVIDIA)
	// also called commercial version number (https://wiki.mozilla.org/Blocklisting/Blocked_Graphics_Drivers)
	FString UserDriverVersion;
	// e.g. 3-13-2015
	FString DriverDate;

	bool IsValid() const
	{
		return !DeviceDescription.IsEmpty()
			&& VendorId
			&& (InternalDriverVersion != TEXT("Unknown"))		// if driver detection code fails
			&& (InternalDriverVersion != TEXT(""));				// if running on non Windows platform we don't fill in the driver version, later we need to check for the OS as well.
	}
	
	// set VendorId
	void SetAMD() { VendorId = 0x1002; }
	// set VendorId
	void SetIntel() { VendorId = 0x8086; }
	// set VendorId
	void SetNVIDIA() { VendorId = 0x10DE; }
	// get VendorId
	bool IsAMD() const { return VendorId == 0x1002; }
	// get VendorId
	bool IsIntel() const { return VendorId == 0x8086; }
	// get VendorId
	bool IsNVIDIA() const { return VendorId == 0x10DE; }

	FString GetUnifiedDriverVersion() const
	{
		// we use the internal version, not the user version to avoid problem where the name was altered 
		const FString& FullVersion = InternalDriverVersion;

		if(IsNVIDIA())
		{
			// on the internal driver number: https://forums.geforce.com/default/topic/378546/confusion-over-driver-version-numbers/
			//   The first 7 shows u that is a Vista driver, 6 that is an XP and 4 that is Me
			// we don't care about the windows version so we don't look at the front part of the driver version
			// "9.18.13.4788" -> "347.88"
			// "10.18.13.4788" -> "347.88"
			// the following code works with the current numbering scheme, if needed we have to update that

			// we don't care about the windows version so we don't look at the front part of the driver version
			// e.g. 36.143
			FString RightPart = FullVersion.Right(6);

			// move the dot
			RightPart = RightPart.Replace(TEXT("."), TEXT(""));
			RightPart.InsertAt(3, TEXT("."));
			return RightPart;
		}
		else if(IsAMD())
		{
			// examples for AMD: "13.12" "15.101.1007" "13.351"
		}
		else if(IsIntel())
		{
		}
		return FullVersion;
	}
};

// one entry in the Hardware.ini file
struct FBlackListEntry
{
	// required, e.g. "<=223.112.21.1", might includes comparison operators, later even things multiple ">12.22 <=12.44"
	FString DriverVersionString;
	// required
	FString Reason;

	// @param e.g. "DriverVersion=\"361.43\", Reason=\"UE-25096 Viewport flashes black and white when moving in the scene on latest Nvidia drivers\""
	void LoadFromINIString(const TCHAR* In)
	{
		FParse::Value(In, TEXT("DriverVersion="), DriverVersionString);
		ensure(!DriverVersionString.IsEmpty());

		// later:
//		FParse::Value(In, TEXT("DeviceId="), DeviceId);
//		FParse::Value(In, TEXT("OS="), OS);
//		FParse::Value(In, TEXT("API="), API);
//		ensure(API == TEXT("DX11"));

		FParse::Value(In, TEXT("Reason="), Reason);
		ensure(!Reason.IsEmpty());
	}

	// test if the given driver version is mentioned in the this blacklist entry
	// @return true:yes, inform used, false otherwise
	bool Test(const FGPUDriverInfo& Info) const
	{
		return CompareStringOp(*DriverVersionString, *Info.GetUnifiedDriverVersion());
	}

	bool IsValid() const
	{
		return !DriverVersionString.IsEmpty();
	}

	/**
	 * Returns true if the latest version of the driver is blacklisted by this entry,
	 * i.e. the comparison op is > or >=.
	 */
	bool IsLatestBlacklisted() const
	{
		bool bLatestBlacklisted = false;
		if (IsValid())
		{
			const TCHAR* DriverVersionTchar = *DriverVersionString;
			EComparisonOp ComparisonOp = ParseComparisonOp(DriverVersionTchar);
			bLatestBlacklisted = (ComparisonOp == ECO_Larger) || (ComparisonOp == ECO_LargerThan);
		}
		return bLatestBlacklisted;
	}
};

struct FGPUHardware
{
	// const as this is set in the constructor to be conveniently available
	const FGPUDriverInfo DriverInfo;

	// constructor
	FGPUHardware(const FGPUDriverInfo InDriverInfo)
		: DriverInfo(InDriverInfo)
	{
		// tests (should be very fast)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		{
			FMultiInt<2> A;
			check(A.Value[0] == 0 && A.Value[1] == 0 && A.Size == 2);
			A.GetValue(TEXT("18.98"));
			check(A.Value[0] == 18 && A.Value[1] == 98);
			A.GetValue(TEXT(""));
			check(A.Value[0] == 0 && A.Value[1] == 0);
			A.GetValue(TEXT("98"));
			check(A.Value[0] == 0 && A.Value[1] == 98);
			A.GetValue(TEXT("98.34.56"));
			check(A.Value[0] == 98 && A.Value[1] == 34);
			A.GetValue(TEXT(" 98 . 034 "));
			check(A.Value[0] == 98 && A.Value[1] == 34);
			A.GetValue(TEXT("\t 98\t.\t34\t"));
			check(A.Value[0] == 98 && A.Value[1] == 34);
		
			check(FMultiInt<2>(TEXT("3.07")) == FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("3.05")) < FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("3.05")) <= FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("3.07")) <= FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("3.08")) > FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("3.08")) >= FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("3.07")) >= FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("3.05")) != FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("4.05")) > FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("4.05")) >= FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("2.05")) < FMultiInt<2>(TEXT("3.07")));
			check(FMultiInt<2>(TEXT("2.05")) <= FMultiInt<2>(TEXT("3.07")));
			
			check(Compare(10, ECO_Equal, 10));
			check(Compare(10, ECO_NotEqual, 20));
			check(Compare(20, ECO_Larger, 10));
			check(Compare(20, ECO_LargerThan, 10));
			check(Compare(10, ECO_LargerThan, 10));
			check(Compare(10, ECO_Smaller, 20));
			check(Compare(10, ECO_SmallerThan, 10));

			check(CompareStringOp(TEXT("<20.10"), TEXT("19.12")));
			check(CompareStringOp(TEXT("<=20.10"), TEXT("19.12")));
			check(CompareStringOp(TEXT("<=19.12"), TEXT("19.12")));
			check(CompareStringOp(TEXT("==19.12"), TEXT("19.12")));
			check(CompareStringOp(TEXT(">=19.12"), TEXT("19.12")));
			check(CompareStringOp(TEXT(">=10.12"), TEXT("19.12")));
			check(CompareStringOp(TEXT("!=20.12"), TEXT("19.12")));
			check(CompareStringOp(TEXT(">10.12"), TEXT("19.12")));

			{
				FGPUDriverInfo Version;
				Version.SetNVIDIA();
				check(Version.IsNVIDIA());
				check(!Version.IsAMD());
				check(!Version.IsIntel());
				Version.InternalDriverVersion = TEXT("10.18.13.4788");
				check(Version.GetUnifiedDriverVersion() == TEXT("347.88"));
			}
			{
				FGPUDriverInfo Version;
				Version.SetAMD();
				check(Version.IsAMD());
				check(!Version.IsNVIDIA());
				check(!Version.IsIntel());
				Version.InternalDriverVersion = TEXT("15.200.1062.1004");
				check(Version.GetUnifiedDriverVersion() == TEXT("15.200.1062.1004"));
			}
			{
				FGPUDriverInfo Version;
				Version.SetIntel();
				check(Version.IsIntel());
				check(!Version.IsAMD());
				check(!Version.IsNVIDIA());
				Version.DeviceDescription = TEXT("Intel(R) HD Graphics 4600");
				Version.InternalDriverVersion = TEXT("9.18.10.3310");
				Version.DriverDate = TEXT("9-17-2013");
				check(Version.GetUnifiedDriverVersion() == TEXT("9.18.10.3310"));
			}
		}
#endif// !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}	
	
	// @return a driver version intended to be shown to the user e.g. "15.30.1025.1001 12/17/2015 (Crimson Edition 15.12)"
	FString GetSuggestedDriverVersion() const
	{
		const TCHAR* Section = GetVendorSectionName();

		FString Ret;

		if(Section)
		{
			GConfig->GetString(Section, TEXT("SuggestedDriverVersion"), Ret, GHardwareIni);
		}

		return Ret;
	}

	// @return 0 if there is none
	FBlackListEntry FindDriverBlacklistEntry() const
	{
		const TCHAR* Section = GetVendorSectionName();

		if(Section)
		{
			TArray<FString> BlacklistStrings;
			GConfig->GetArray(GetVendorSectionName(), TEXT("Blacklist"), BlacklistStrings, GHardwareIni);

			for(int32 i = 0; i < BlacklistStrings.Num(); ++i)
			{
				FBlackListEntry Entry;

				const TCHAR* Line = *BlacklistStrings[i];
			
				ensure(Line[0] == TCHAR('('));

				Entry.LoadFromINIString(&Line[1]);

				if(Entry.Test(DriverInfo))
				{
					return Entry;
				}
			}
		}

		return FBlackListEntry();
	}

	/**
	 * Returns true if the latest version of the driver has been blacklisted.
	 */
	bool IsLatestBlacklisted() const
	{
		bool bLatestBlacklisted = false;
		const TCHAR* Section = GetVendorSectionName();

		if(Section)
		{
			TArray<FString> BlacklistStrings;
			GConfig->GetArray(GetVendorSectionName(), TEXT("Blacklist"), BlacklistStrings, GHardwareIni);

			for(int32 i = 0; !bLatestBlacklisted && i < BlacklistStrings.Num(); ++i)
			{
				FBlackListEntry Entry;

				const TCHAR* Line = *BlacklistStrings[i];
			
				ensure(Line[0] == TCHAR('('));

				Entry.LoadFromINIString(&Line[1]);

				bLatestBlacklisted |= Entry.IsLatestBlacklisted();
			}
		}
		return bLatestBlacklisted;
	}

	// to get a section name in the Hardware.ini file
	// @return 0 if not found
	const TCHAR* GetVendorSectionName() const
	{
		if(DriverInfo.IsNVIDIA())
		{
			return TEXT("GPU_NVIDIA");
		}
		if(DriverInfo.IsAMD())
		{
			return TEXT("GPU_AMD");
		}
		else if(DriverInfo.IsIntel())
		{
			return TEXT("GPU_0x8086");
		}
		// more GPU vendors can be added on demand
		return 0;
	}
};


