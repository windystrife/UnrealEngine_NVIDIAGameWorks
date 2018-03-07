// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/**
	* FInt24
	* Helper class to deal with 24 bit integers (converts to and from 32 bit integers)
	*/
	class FInt24
	{
	public:

		FInt24()
		{
			memset((void *)Data, 0, sizeof(Data));
		}

		FInt24(const FInt24& Other)
		{
			*this = Other;
		}

		FInt24(const int32& Value)
		{
			*this = Value;
		}

		FInt24& operator=(const int32& Value)
		{
			static const uint32 MASK_BYTE_0 = 0x000000FF;
			static const uint32 MASK_BYTE_1 = 0x0000FF00;
			static const uint32 MASK_BYTE_2 = 0x00FF0000;

			Data[0] = (Value & MASK_BYTE_0);
			Data[1] = (Value & MASK_BYTE_1) >> 8;
			Data[2] = (Value & MASK_BYTE_2) >> 16;
			return *this;
		}

		int32 operator+(const double& Value)
		{
			int32 IntValue = GetInt32();
			return IntValue + (int32)Value;
		}

		double operator*(const double& Value)
		{
			int32 IntValue = GetInt32();
			return IntValue * Value;
		}

		operator int()
		{
			return GetInt32();
		}

		int32 GetInt32()
		{
			// copy data to front of integer, then shift to right 8 bits for sign extension
			int32 Value = ((Data[2] << 24) | (Data[1] << 16) | (Data[1] << 8)) >> 8;
			return Value;
		}

	private:

		/** 3 bytes to represent the 24 bit integer */
		uint8 Data[3];
	};

	// typedef for the special 24 bit integer type
	typedef FInt24 int24;

	/**
	* EStreamDataFormat
	*
	* An enumeration to specify the state of the audio stream.
	*
	*/
	namespace EStreamFormat
	{
		enum Type
		{
			UNKNOWN,	// Unknown format type (indicates error or uninitialized values)
			FLT,
			DBL,
			INT_16,
			INT_24,
			INT_32,
			UNSUPPORTED,
		};

		/** @return the stringified version of the enum passed in */
		inline const TCHAR* ToString(EStreamFormat::Type StreamFormat)
		{
			switch (StreamFormat)
			{
				default:
				case UNKNOWN:	return TEXT("UNKNOWN");
				case FLT:		return TEXT("FLOAT");
				case DBL:		return TEXT("DOUBLE");
				case INT_16:	return TEXT("INT_16");
				case INT_24:	return TEXT("INT_24");
				case INT_32:	return TEXT("INT_32");
			}
		}
	}

	/** Template base for various data formats */
	template<typename T>
	struct TDataFormat
	{
		static FORCEINLINE EStreamFormat::Type GetTypeEnum()			{ return EStreamFormat::UNKNOWN;	}
		static FORCEINLINE double GetMaxValue()							{ return 0.0;						}
		static FORCEINLINE T* GetDataBuffer(TArray<uint8>& Buffer)		{ return (T*)Buffer.GetData();		}
		static FORCEINLINE bool IsInteger()								{ return false;						}
		static FORCEINLINE uint32 GetByteMask()							{ return 0;							}
	};

	/* Float template specialization */
	template<>
	struct TDataFormat<float>
	{
		static FORCEINLINE EStreamFormat::Type GetTypeEnum()			{ return EStreamFormat::FLT;		}
		static FORCEINLINE double GetMaxValue()							{ return 1.0;						}
		static FORCEINLINE float* GetDataBuffer(TArray<uint8>& Buffer)	{ return (float*)Buffer.GetData();	}
		static FORCEINLINE bool IsInteger()								{ return false;						}
	};

	/* Double template specialization */
	template<>
	struct TDataFormat<double>
	{
		static FORCEINLINE EStreamFormat::Type GetTypeEnum()			{ return EStreamFormat::DBL;		}
		static FORCEINLINE double GetMaxValue()							{ return 1.0;						}
		static FORCEINLINE double* GetDataBuffer(TArray<uint8>& Buffer) { return (double*)Buffer.GetData(); }
		static FORCEINLINE bool IsInteger()								{ return false;						}
	};

	/* Int8 template specialization */
	template<>
	struct TDataFormat<int16>
	{
		static FORCEINLINE EStreamFormat::Type GetTypeEnum()			{ return EStreamFormat::INT_16;		}
		static FORCEINLINE double GetMaxValue()							{ return 32767;						}
		static FORCEINLINE int16* GetDataBuffer(TArray<uint8>& Buffer)	{ return (int16*)Buffer.GetData();	}
		static FORCEINLINE bool IsInteger()								{ return true;						}
		static FORCEINLINE uint32 GetByteMask()							{ return 0x0000FFFF;				}
	};

	/* int24 template specialization */
	template<>
	struct TDataFormat<int24>
	{
		static FORCEINLINE EStreamFormat::Type GetTypeEnum()			{ return EStreamFormat::INT_24;		}
		static FORCEINLINE double GetMaxValue()							{ return 8388607;					}
		static FORCEINLINE int24* GetDataBuffer(TArray<uint8>& Buffer)	{ return (int24*)Buffer.GetData();	}
		static FORCEINLINE bool IsInteger()								{ return true;						}
		static FORCEINLINE uint32 GetByteMask()							{ return 0x00FFFFFF;				}
	};

	/* int24 template specialization */
	template<>
	struct TDataFormat<int32>
	{
		static FORCEINLINE EStreamFormat::Type GetTypeEnum()			{ return EStreamFormat::INT_32;		}
		static FORCEINLINE double GetMaxValue()							{ return 2147483647;				}
		static FORCEINLINE int32* GetDataBuffer(TArray<uint8>& Buffer)	{ return (int32*)Buffer.GetData();	}
		static FORCEINLINE bool IsInteger()								{ return true;						}
		static FORCEINLINE uint32 GetByteMask()							{ return 0xFFFFFFFF;				}
	};

}

#endif // #if ENABLE_UNREAL_AUDIO

