// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "OnlineSessionSettings.h"
#include "OnlineKeyValuePair.h"
#include "IPAddress.h"

/**
 * Serializes data in network byte order form into a buffer
 */
class FNboSerializeToBuffer
{
	/** Hidden on purpose */
	FNboSerializeToBuffer(void);

protected:
	/**
	 * Holds the data as it is serialized
	 */
	TArray<uint8> Data;
	/**
	 * Tracks how many bytes have been written in the packet
	 */
	uint32 NumBytes;

	/** Indicates whether writing to the buffer caused an overflow or not */
	bool bHasOverflowed;

public:
	/**
	 * Inits the write tracking
	 */
	FNboSerializeToBuffer(uint32 Size) :
		NumBytes(0),
		bHasOverflowed(false)
	{
		Data.Empty(Size);
		Data.AddZeroed(Size);
	}

	/**
	 * Cast operator to get at the formatted buffer data
	 */
	inline operator uint8*(void) const
	{
		return (uint8*)Data.GetData();
	}

	/**
	 * Cast operator to get at the formatted buffer data
	 */
	inline const TArray<uint8>& GetBuffer(void) const
	{
		return Data;
	}

	/**
	 * Returns the size of the buffer we've built up
	 */
	inline uint32 GetByteCount(void) const
	{
		return NumBytes;
	}

	/**
	 * Returns the number of bytes preallocated in the array
	 */
	inline uint32 GetBufferSize(void) const
	{
		return (uint32)Data.Num();
	}

	/**
	 * Trim any extra space in the buffer that is not used
	 */
	inline void TrimBuffer()
	{
		if (GetBufferSize() > GetByteCount())
		{
			Data.RemoveAt(GetByteCount(), GetBufferSize()-GetByteCount());
		}
	}

	/**
	 * Adds a char to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const char Ch)
	{
		if (!Ar.HasOverflow() && Ar.NumBytes + 1 <= Ar.GetBufferSize())
		{
			Ar.Data[Ar.NumBytes++] = Ch;
		}
		else
		{
			Ar.bHasOverflowed = true;
		}

		return Ar;
	}

	/**
	 * Adds a byte to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const uint8& B)
	{
		if (!Ar.HasOverflow() && Ar.NumBytes + 1 <= Ar.GetBufferSize())
		{
			Ar.Data[Ar.NumBytes++] = B;
		}
		else
		{
			Ar.bHasOverflowed = true;
		}

		return Ar;
	}

	/**
	 * Adds a byte to the buffer
	 */
	template<class TEnum>
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const TEnumAsByte<TEnum>& B)
	{
		return Ar << *(uint8*)&B;
	}

	/**
	 * Adds an int32 to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const int32& I)
	{
		return Ar << *(uint32*)&I;
	}

	/**
	 * Adds a uint32 to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const uint32& D)
	{
		if (!Ar.HasOverflow() && Ar.NumBytes + 4 <= Ar.GetBufferSize())
		{
			Ar.Data[Ar.NumBytes + 0] = (D >> 24) & 0xFF;
			Ar.Data[Ar.NumBytes + 1] = (D >> 16) & 0xFF;
			Ar.Data[Ar.NumBytes + 2] = (D >> 8) & 0xFF;
			Ar.Data[Ar.NumBytes + 3] = D & 0xFF;
			Ar.NumBytes += 4;
		}
		else
		{
			Ar.bHasOverflowed = true;
		}

		return Ar;
	}

	/**
	 * Adds a uint64 to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const uint64& Q)
	{
		if (!Ar.HasOverflow() && Ar.NumBytes + 8 <= Ar.GetBufferSize())
		{
			Ar.Data[Ar.NumBytes + 0] = (Q >> 56) & 0xFF;
			Ar.Data[Ar.NumBytes + 1] = (Q >> 48) & 0xFF;
			Ar.Data[Ar.NumBytes + 2] = (Q >> 40) & 0xFF;
			Ar.Data[Ar.NumBytes + 3] = (Q >> 32) & 0xFF;
			Ar.Data[Ar.NumBytes + 4] = (Q >> 24) & 0xFF;
			Ar.Data[Ar.NumBytes + 5] = (Q >> 16) & 0xFF;
			Ar.Data[Ar.NumBytes + 6] = (Q >> 8) & 0xFF;
			Ar.Data[Ar.NumBytes + 7] = Q & 0xFF;
			Ar.NumBytes += 8;
		}
		else
		{
			Ar.bHasOverflowed = true;
		}

		return Ar;
	}

	/**
	 * Adds a float to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const float& F)
	{
		uint32 Value = *(uint32*)&F;
		return Ar << Value;
	}

	/**
	 * Adds a double to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const double& Dbl)
	{
		uint64 Value = *(uint64*)&Dbl;
		return Ar << Value;
	}

	/**
	 * Adds a FString to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FString& String)
	{
		// We send strings length prefixed
		FTCHARToUTF8 Converted(*String);
		int32 Len = Converted.Length();

		Ar << Len;

		if (!Ar.HasOverflow() && Ar.NumBytes + Len <= Ar.GetBufferSize())
		{
			// Handle empty strings
			if (Len > 0)
			{
				ANSICHAR* Ptr = (ANSICHAR*)Converted.Get();

				// memcpy it into the buffer
				FMemory::Memcpy(&Ar.Data[Ar.NumBytes], Ptr,Len);
				Ar.NumBytes += Len;
			}
		}
		else
		{
			Ar.bHasOverflowed = true;
		}

		return Ar;
	}

	/**
	 * Adds a string to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const TCHAR* String)
	{
		// We send strings length prefixed (conversion handles null pointers)
		FTCHARToUTF8 Converted(String);
		int32 Len = Converted.Length();

		Ar << Len;

		if (!Ar.HasOverflow() && Ar.NumBytes + Len <= Ar.GetBufferSize())
		{
			// Handle empty/null strings
			if (Len > 0)
			{
				ANSICHAR* Ptr = (ANSICHAR*)Converted.Get();

				// memcpy it into the buffer
				FMemory::Memcpy(&Ar.Data[Ar.NumBytes], Ptr, Len);
				Ar.NumBytes += Len;
			}
		}
		else
		{
			Ar.bHasOverflowed = true;
		}

		return Ar;
	}

	/**
	 * Adds a substring to the buffer
	 */
	inline FNboSerializeToBuffer& AddString(const ANSICHAR* String,const int32 Length)
	{
		// We send strings length prefixed
		int32 Len = Length;
		(*this) << Len;

		if (!HasOverflow() && NumBytes + Len <= GetBufferSize())
		{
			// Don't process if null
			if (String)
			{
				// memcpy it into the buffer
				FMemory::Memcpy(&Data[NumBytes],String,Len);
				NumBytes += Len;
			}
		}
		else
		{
			bHasOverflowed = true;
		}

		return *this;
	}

	/**
	 * Adds an FName to the buffer as a string
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FName& Name)
	{
		const FString NameString = Name.ToString();
		Ar << NameString;
		return Ar;
	}

	/**
	 * Writes a list of key value pairs to buffer
	 */
	template<class KeyType, class ValueType>
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar, const FOnlineKeyValuePairs<KeyType,ValueType>& KeyValuePairs)
	{
		Ar << KeyValuePairs.Num();
		for (typename FOnlineKeyValuePairs<KeyType,ValueType>::TConstIterator It(KeyValuePairs); It; ++It)
		{
			Ar << It.Key();
			Ar << It.Value();
		}
		return Ar;
	}

	/**
	 * Adds a key value pair to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FVariantData& KeyValuePair)
	{
		// Write the type
		uint8 Type = KeyValuePair.GetType();
		Ar << Type;
		// Now write out the held data
		switch (KeyValuePair.GetType())
		{
		case EOnlineKeyValuePairDataType::Float:
			{
				float Value;
				KeyValuePair.GetValue(Value);
				Ar << Value;
				break;
			}
		case EOnlineKeyValuePairDataType::Int32:
			{
				int32 Value;
				KeyValuePair.GetValue(Value);
				Ar << Value;
				break;
			}
		case EOnlineKeyValuePairDataType::Int64:
			{
				uint64 Value;
				KeyValuePair.GetValue(Value);
				Ar << Value;
				break;
			}
		case EOnlineKeyValuePairDataType::Double:
			{
				double Value;
				KeyValuePair.GetValue(Value);
				Ar << Value;
				break;
			}
		case EOnlineKeyValuePairDataType::Blob:
			{
				TArray<uint8> Value;
				KeyValuePair.GetValue(Value);

				// Write the length
				Ar << Value.Num();
				// Followed by each byte of data
				for (int32 Index = 0; Index < Value.Num(); Index++)
				{
					Ar << Value[Index];
				}
				break;
			}
		case EOnlineKeyValuePairDataType::String:
			{
				FString Value;
				KeyValuePair.GetValue(Value);
				// This will write a length prefixed string
				Ar << (TCHAR*)*Value;
				break;
			}
		case EOnlineKeyValuePairDataType::Bool:
			{
				bool Value;
				KeyValuePair.GetValue(Value);
				Ar << (uint8)(Value ? 1 : 0);
				break;
			}
		case EOnlineKeyValuePairDataType::Empty:
			break;
		default:
			checkfSlow(false, TEXT("Unsupported EOnlineKeyValuePairDataType: %d"), Type);
		}
		return Ar;
	}

	/**
	 * Adds a single online session setting to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FOnlineSessionSetting& Setting)
	{
		Ar << Setting.Data;
		
		uint8 Type;
		Type = Setting.AdvertisementType;
		Ar << Type;

		return Ar;
	}

	/**
	 * Adds an ip address to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FInternetAddr& Addr)
	{
		uint32 OutIp;
		Addr.GetIp(OutIp);
		Ar << OutIp;
		int32 OutPort;
		Addr.GetPort(OutPort);
		Ar << OutPort;
		return Ar;
	}

	/**
	 * Adds a FGuid to the buffer
	 */
	friend inline FNboSerializeToBuffer& operator<<(FNboSerializeToBuffer& Ar,const FGuid& Guid)
	{
		Ar << Guid.A;
		Ar << Guid.B;
		Ar << Guid.C;
		Ar << Guid.D;
		return Ar;
	}

	/**
	 * Writes a blob of data to the buffer
	 *
	 * @param Buffer the source data to append
	 * @param NumToWrite the size of the blob to write
	 */
	inline void WriteBinary(const uint8* Buffer,uint32 NumToWrite)
	{
		if (!HasOverflow() && NumBytes + NumToWrite <= GetBufferSize())
		{
			FMemory::Memcpy(&Data[NumBytes],Buffer,NumToWrite);
			NumBytes += NumToWrite;
		}
		else
		{
			bHasOverflowed = true;
		}
	}

	/**
	 * Gets the buffer at a specific point
	 */
	inline uint8* GetRawBuffer(uint32 Offset)
	{
		return &Data[Offset];
	}

	/**
	 * Skips forward in the buffer by the specified amount
	 *
	 * @param Amount the number of bytes to skip ahead
	 */
	inline void SkipAheadBy(uint32 Amount)
	{
		if (!HasOverflow() && NumBytes + Amount <= GetBufferSize())
		{
			NumBytes += Amount;
		}
		else
		{
			bHasOverflowed = true;
		}
	}

	/** Returns whether the buffer had an overflow when writing to it */
	inline bool HasOverflow(void) const
	{
		return bHasOverflowed;
	}
};

MSVC_PRAGMA(warning(push))
// Disable used without initialization warning because the reads are initializing
MSVC_PRAGMA(warning(disable : 4700))

/**
 * Class used to read data from a NBO data buffer
 */
class FNboSerializeFromBuffer
{
protected:
	/** Pointer to the data this reader is attached to */
	const uint8* Data;
	/** The size of the data in bytes */
	const int32 NumBytes;
	/** The current location in the byte stream for reading */
	int32 CurrentOffset;
	/** Indicates whether reading from the buffer caused an overflow or not */
	bool bHasOverflowed;

	/** Hidden on purpose */
	FNboSerializeFromBuffer(void);

public:
	/**
	 * Initializes the buffer, size, and zeros the read offset
	 *
	 * @param InData the buffer to attach to
	 * @param Length the size of the buffer we are attaching to
	 */
	FNboSerializeFromBuffer(const uint8* InData,int32 Length) :
		Data(InData),
		NumBytes(Length),
		CurrentOffset(0),
		bHasOverflowed(false)
	{
	}

	/**
	 * Reads a char from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,char& Ch)
	{
		if (!Ar.HasOverflow() && Ar.CurrentOffset + 1 <= Ar.NumBytes)
		{
			Ch = Ar.Data[Ar.CurrentOffset++];
		}
		else
		{
			Ar.bHasOverflowed = true;
		}
		return Ar;
	}

	/**
	 * Reads a byte from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,uint8& B)
	{
		if (!Ar.HasOverflow() && Ar.CurrentOffset + 1 <= Ar.NumBytes)
		{
			B = Ar.Data[Ar.CurrentOffset++];
		}
		else
		{
			Ar.bHasOverflowed = true;
		}
		return Ar;
	}
	/**
	 * Reads a byte from the buffer
	 */
	template<class TEnum>
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,TEnumAsByte<TEnum>& B)
	{
		return Ar >> *(uint8*)&B;
	}

	/**
	 * Reads an int32 from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,int32& I)
	{
		return Ar >> *(uint32*)&I;
	}

	/**
	 * Reads a uint32 from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,uint32& D)
	{
		if (!Ar.HasOverflow() && Ar.CurrentOffset + 4 <= Ar.NumBytes)
		{
			uint32 D1 = Ar.Data[Ar.CurrentOffset + 0];
			uint32 D2 = Ar.Data[Ar.CurrentOffset + 1];
			uint32 D3 = Ar.Data[Ar.CurrentOffset + 2];
			uint32 D4 = Ar.Data[Ar.CurrentOffset + 3];
			D = D1 << 24 | D2 << 16 | D3 << 8 | D4;
			Ar.CurrentOffset += 4;
		}
		else
		{
			Ar.bHasOverflowed = true;
		}
		return Ar;
	}

	/**
	 * Adds a uint64 to the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,uint64& Q)
	{
		if (!Ar.HasOverflow() && Ar.CurrentOffset + 8 <= Ar.NumBytes)
		{
			Q = ((uint64)Ar.Data[Ar.CurrentOffset + 0] << 56) |
				((uint64)Ar.Data[Ar.CurrentOffset + 1] << 48) |
				((uint64)Ar.Data[Ar.CurrentOffset + 2] << 40) |
				((uint64)Ar.Data[Ar.CurrentOffset + 3] << 32) |
				((uint64)Ar.Data[Ar.CurrentOffset + 4] << 24) |
				((uint64)Ar.Data[Ar.CurrentOffset + 5] << 16) |
				((uint64)Ar.Data[Ar.CurrentOffset + 6] << 8) |
				(uint64)Ar.Data[Ar.CurrentOffset + 7];
			Ar.CurrentOffset += 8;
		}
		else
		{
			Ar.bHasOverflowed = true;
		}
		return Ar;
	}

	/**
	 * Reads a float from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,float& F)
	{
		return Ar >> *(uint32*)&F;
	}

	/**
	 * Reads a double from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,double& Dbl)
	{
		return Ar >> *(uint64*)&Dbl;
	}

	/**
	 * Reads a FString from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FString& String)
	{
		// We send strings length prefixed
		int32 Len = 0;
		Ar >> Len;

		// Check this way to trust NumBytes and CurrentOffset to be more accurate than the packet Len value
		const bool bSizeOk = (Len >= 0) && (Len <= (Ar.NumBytes - Ar.CurrentOffset));
		if (!Ar.HasOverflow() && bSizeOk)
		{
			// Handle strings of zero length
			if (Len > 0)
			{
				char* Temp = (char*)FMemory_Alloca(Len + 1);
				// memcpy it in from the buffer
				FMemory::Memcpy(Temp, &Ar.Data[Ar.CurrentOffset], Len);
				Temp[Len] = '\0';

				FUTF8ToTCHAR Converted(Temp);
				TCHAR* Ptr = (TCHAR*)Converted.Get();
				String = Ptr;
				Ar.CurrentOffset += Len;
			}
			else
			{
				String.Empty();
			}
		}
		else
		{
			Ar.bHasOverflowed = true;
		}

		return Ar;
	}

	/**
	 * Reads an FName from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FName& Name)
	{
		FString NameString; 
		Ar >> NameString;
		Name = FName(*NameString);
		return Ar;
	}

	/**
	 * Reads a list of key value pairs from the buffer
	 */
	template<class KeyType, class ValueType>
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar, FOnlineKeyValuePairs<KeyType,ValueType>& KeyValuePairs)
	{
		int32 NumValues;
		Ar >> NumValues;		
		for (int32 Idx=0; Idx < NumValues; Idx++)
		{
			KeyType Key;
			ValueType Value;
			Ar >> Key;
			Ar >> Value;

			KeyValuePairs.Add(Key,Value);
		}
		return Ar;
	}

	/**
	 * Reads a key value pair from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FVariantData& KeyValuePair)
	{
		if (!Ar.HasOverflow())
		{
			// Read the type
			uint8 Type;
			Ar >> Type;

			// Now read in the held data
			switch ((EOnlineKeyValuePairDataType::Type)Type)
			{
			case EOnlineKeyValuePairDataType::Float:
				{
					float Value;
					Ar >> Value;
					KeyValuePair.SetValue(Value);
					break;
				}
			case EOnlineKeyValuePairDataType::Int32:
				{
					int32 Value;
					Ar >> Value;
					KeyValuePair.SetValue(Value);
					break;
				}
			case EOnlineKeyValuePairDataType::Int64:
				{
					uint64 Value;
					Ar >> Value;
					KeyValuePair.SetValue(Value);
					break;
				}
			case EOnlineKeyValuePairDataType::Double:
				{
					double Value;
					Ar >> Value;
					KeyValuePair.SetValue(Value);
					break;
				}
			case EOnlineKeyValuePairDataType::Blob:
				{
					int32 Length;
					Ar >> Length;

					// Check this way to trust NumBytes and CurrentOffset to be more accurate than the packet Len value
					const bool bSizeOk = (Length >= 0) && (Length <= (Ar.NumBytes - Ar.CurrentOffset));
					if (!Ar.HasOverflow() && bSizeOk)
					{
						// Now directly copy the blob data
						KeyValuePair.SetValue(Length, &Ar.Data[Ar.CurrentOffset]);
						Ar.CurrentOffset += Length;
					}
					else
					{
						Ar.bHasOverflowed = true;
					}

					break;
				}
			case EOnlineKeyValuePairDataType::String:
				{
					FString Value;
					Ar >> Value;
					KeyValuePair.SetValue(Value);
					break;
				}
			case EOnlineKeyValuePairDataType::Bool:
				{
					uint8 Value;
					Ar >> Value;
					KeyValuePair.SetValue(Value != 0);
					break;
				}
			case EOnlineKeyValuePairDataType::Empty:
				break;
			default:
				checkfSlow(false, TEXT("Unsupported EOnlineKeyValuePairDataType: %d"), Type);
			}
		}

		return Ar;
	}

	/**
	 * Reads a single session setting from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FOnlineSessionSetting& Setting)
	{
		Ar >> Setting.Data;

		if (!Ar.HasOverflow())
		{
			uint8 Type;
			Ar >> Type;
			Setting.AdvertisementType = (EOnlineDataAdvertisementType::Type) Type;
		}

		return Ar;
	}

	/**
	 * Reads an ip address from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FInternetAddr& Addr)
	{
		uint32 InIp = 0;
		Ar >> InIp;
		Addr.SetIp(InIp);
		int32 InPort = 0;
		Ar >> InPort;
		Addr.SetPort(InPort);
		return Ar;
	}

	/**
	 * Reads a FGuid from the buffer
	 */
	friend inline FNboSerializeFromBuffer& operator>>(FNboSerializeFromBuffer& Ar,FGuid& Guid)
	{
		Ar >> Guid.A;
		Ar >> Guid.B;
		Ar >> Guid.C;
		Ar >> Guid.D;
		return Ar;
	}

	/**
	 * Reads a blob of data from the buffer
	 *
	 * @param OutBuffer the destination buffer
	 * @param NumToRead the size of the blob to read
	 */
	void ReadBinary(uint8* OutBuffer,uint32 NumToRead)
	{
		if (!HasOverflow() && CurrentOffset + (int32)NumToRead <= NumBytes)
		{
			FMemory::Memcpy(OutBuffer,&Data[CurrentOffset],NumToRead);
			CurrentOffset += NumToRead;
		}
		else
		{
			bHasOverflowed = true;
		}
	}

	/**
	 * Seek to the desired position in the buffer
	 *
	 * @param Pos the offset from the start of the buffer
	 */
	void Seek(int32 Pos)
	{
		checkSlow(Pos >= 0);

		if (!HasOverflow() && Pos < NumBytes)
		{
			CurrentOffset = Pos;
		}
		else
		{
			bHasOverflowed = true;
		}
	}

	/** @return Current position of the buffer being to be read */
	inline int32 Tell(void) const
	{
		return CurrentOffset;
	}

	/** Returns whether the buffer had an overflow when reading from it */
	inline bool HasOverflow(void) const
	{
		return bHasOverflowed;
	}

	/** @return Number of bytes remaining to read from the current offset to the end of the buffer */
	inline int32 AvailableToRead(void) const
	{
		return FMath::Max<int32>(0,NumBytes - CurrentOffset);
	}

	/**
	 * Returns the number of total bytes in the buffer
	 */
	inline int32 GetBufferSize(void) const
	{
		return NumBytes;
	}
};

MSVC_PRAGMA(warning(pop))


