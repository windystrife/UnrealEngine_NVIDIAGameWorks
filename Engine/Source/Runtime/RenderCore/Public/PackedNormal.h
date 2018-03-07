// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"

/** A normal vector, quantized and packed into 32-bits. */
struct FPackedNormal
{
	union
	{
		struct
		{
#if PLATFORM_LITTLE_ENDIAN
			uint8	X,
			Y,
			Z,
			W;
#else
			uint8	W,
			Z,
			Y,
			X;
#endif
		};
		uint32		Packed;
	}				Vector;

	// Constructors.

	FPackedNormal() { Vector.Packed = 0; }
	FPackedNormal(uint32 InPacked) { Vector.Packed = InPacked; }
	FPackedNormal(const FVector& InVector) { *this = InVector; }
	FPackedNormal(uint8 InX, uint8 InY, uint8 InZ, uint8 InW) { Vector.X = InX; Vector.Y = InY; Vector.Z = InZ; Vector.W = InW; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	void operator=(const FVector4& InVector);
	operator FVector() const;
	operator FVector4() const;
	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set(const FVector& InVector) { *this = InVector; }

	// Equality operator.

	bool operator==(const FPackedNormal& B) const;
	bool operator!=(const FPackedNormal& B) const;

	// Serializer.

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FPackedNormal& N);

	FString ToString() const
	{
		return FString::Printf(TEXT("X=%d Y=%d Z=%d W=%d"), Vector.X, Vector.Y, Vector.Z, Vector.W);
	}

	// Zero Normal
	static RENDERCORE_API FPackedNormal ZeroNormal;
};

/** X=127.5, Y=127.5, Z=1/127.5f, W=-1.0 */
extern RENDERCORE_API const VectorRegister GVectorPackingConstants;

FORCEINLINE void FPackedNormal::operator=(const FVector& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 127.5f + 127.5f),0,255);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 127.5f + 127.5f),0,255);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 127.5f + 127.5f),0,255);
	Vector.W = 128;
}

FORCEINLINE void FPackedNormal::operator=(const FVector4& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 127.5f + 127.5f),0,255);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 127.5f + 127.5f),0,255);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 127.5f + 127.5f),0,255);
	Vector.W = FMath::Clamp(FMath::TruncToInt(InVector.W * 127.5f + 127.5f),0,255);
}

FORCEINLINE bool FPackedNormal::operator==(const FPackedNormal& B) const
{
	FVector	V1 = *this;
	FVector	V2 = B;

	if(FMath::Abs(V1.X - V2.X) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if(FMath::Abs(V1.Y - V2.Y) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if(FMath::Abs(V1.Z - V2.Z) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}

FORCEINLINE bool FPackedNormal::operator!=(const FPackedNormal& B) const
{
	return !(*this == B);
}

FORCEINLINE FPackedNormal::operator FVector() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector and return it.
	FVector UnpackedVector;
	VectorStoreFloat3( VectorToUnpack, &UnpackedVector );
	return UnpackedVector;
}

FORCEINLINE FPackedNormal::operator FVector4() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector4 and return it.
	FVector4 UnpackedVector;
	VectorStore(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE VectorRegister FPackedNormal::GetVectorRegister() const
{
	// Rescale [0..255] range to [-1..1]
	VectorRegister VectorToUnpack		= VectorLoadByte4( this );
	VectorToUnpack						= VectorMultiplyAdd( VectorToUnpack, VectorReplicate(GVectorPackingConstants,2), VectorReplicate(GVectorPackingConstants,3) );
	VectorResetFloatRegisters();
	// Return unpacked vector register.
	return VectorToUnpack;
}

/** A vector, quantized and packed into 32-bits. */
struct FPackedRGB10A2N
{
	union
	{
		struct
		{
#if PLATFORM_LITTLE_ENDIAN
			uint32 X : 10;
			uint32 Y : 10;
			uint32 Z : 10;
			uint32 W : 2;
#else
			uint32 W : 2;
			uint32 Z : 10;
			uint32 Y : 10;
			uint32 X : 10;
#endif
		};

		struct  
		{
			uint32 Packed;
		};
	} Vector;

	// Constructors.

	FPackedRGB10A2N() { Vector.Packed = 0; }
	FPackedRGB10A2N(uint32 InPacked) { Vector.Packed = InPacked; }
	FPackedRGB10A2N(const FVector& InVector) { *this = InVector; }
	FPackedRGB10A2N(const FVector4& InVector) { *this = InVector; }
	FPackedRGB10A2N(uint32 InX, uint32 InY, uint32 InZ, uint32 InW) { Vector.X = InX; Vector.Y = InY; Vector.Z = InZ; Vector.W = InW; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	void operator=(const FVector4& InVector);
	operator FVector() const;
	operator FVector4() const;

	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set(const FVector& InVector) { *this = InVector; }
	void Set(const FVector4& InVector) { *this = InVector; }

	// Equality operator.

	bool operator==(const FPackedRGB10A2N& B) const;
	bool operator!=(const FPackedRGB10A2N& B) const;

	// Serializer.

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FPackedRGB10A2N& N);

	FString ToString() const
	{
		return FString::Printf(TEXT("X=%d Y=%d Z=%d W=%d"), Vector.X, Vector.Y, Vector.Z, Vector.W);
	}

	static RENDERCORE_API FPackedRGB10A2N ZeroVector;
};

FORCEINLINE void FPackedRGB10A2N::operator=(const FVector& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 511.5f + 511.5f), 0, 1023);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 511.5f + 511.5f), 0, 1023);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 511.5f + 511.5f), 0, 1023);
	Vector.W = 3;
}

FORCEINLINE void FPackedRGB10A2N::operator=(const FVector4& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 511.5f + 511.5f), 0, 1023);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 511.5f + 511.5f), 0, 1023);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 511.5f + 511.5f), 0, 1023);
	Vector.W = FMath::Clamp(FMath::TruncToInt(InVector.W * 1.5f   + 1.5f),   0, 3);
}

FORCEINLINE bool FPackedRGB10A2N::operator==(const FPackedRGB10A2N& B) const
{
	FVector	V1 = *this;
	FVector V2 = B;

	if (FMath::Abs(V1.X - V2.X) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if (FMath::Abs(V1.Y - V2.Y) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if (FMath::Abs(V1.Z - V2.Z) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}

FORCEINLINE bool FPackedRGB10A2N::operator!=(const FPackedRGB10A2N& B) const
{
	return !(*this == B);
}

FORCEINLINE FPackedRGB10A2N::operator FVector() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector and return it.
	FVector UnpackedVector;
	VectorStoreFloat3(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE FPackedRGB10A2N::operator FVector4() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector4 and return it.
	FVector4 UnpackedVector;
	VectorStore(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE VectorRegister FPackedRGB10A2N::GetVectorRegister() const
{
	VectorRegister VectorToUnpack = VectorLoadURGB10A2N((void*)this);
	VectorToUnpack = VectorMultiplyAdd(VectorToUnpack, MakeVectorRegister(2.0f, 2.0f, 2.0f, 2.0f), MakeVectorRegister(-1.0f, -1.0f, -1.0f, -1.0f));
	VectorResetFloatRegisters();
	// Return unpacked vector register.
	return VectorToUnpack;
}

/** A vector, quantized and packed into 32-bits. */
struct FPackedRGBA16N
{
	struct
	{
		uint16 X;
		uint16 Y;
		uint16 Z;
		uint16 W;
	};

	// Constructors.

	FPackedRGBA16N() { X = 0; Y = 0; Z = 0; W = 0; }
	FPackedRGBA16N(const FVector& InVector) { *this = InVector; }
	FPackedRGBA16N(const FVector4& InVector) { *this = InVector; }
	FPackedRGBA16N(uint16 InX, uint16 InY, uint16 InZ, uint16 InW) { X = InX; Y = InY; Z = InZ; W = InW; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	void operator=(const FVector4& InVector);
	operator FVector() const;
	operator FVector4() const;

	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set(const FVector& InVector) { *this = InVector; }
	void Set(const FVector4& InVector) { *this = InVector; }

	// Equality operator.

	bool operator==(const FPackedRGBA16N& B) const;
	bool operator!=(const FPackedRGBA16N& B) const;

	// Serializer.

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FPackedRGBA16N& N);

	FString ToString() const
	{
		return FString::Printf(TEXT("X=%d Y=%d Z=%d W=%d"), X, Y, Z, W);
	}

	static RENDERCORE_API FPackedRGBA16N ZeroVector;
};

FORCEINLINE void FPackedRGBA16N::operator=(const FVector& InVector)
{
	X = FMath::Clamp(FMath::TruncToInt(InVector.X * 32767.5f + 32767.5f), 0, 65535);
	Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 32767.5f + 32767.5f), 0, 65535);
	Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 32767.5f + 32767.5f), 0, 65535);
	W = 65535;
}

FORCEINLINE void FPackedRGBA16N::operator=(const FVector4& InVector)
{
	X = FMath::Clamp(FMath::TruncToInt(InVector.X * 32767.5f + 32767.5f), 0, 65535);
	Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 32767.5f + 32767.5f), 0, 65535);
	Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 32767.5f + 32767.5f), 0, 65535);
	W = FMath::Clamp(FMath::TruncToInt(InVector.W * 32767.5f + 32767.5f), 0, 65535);
}

FORCEINLINE bool FPackedRGBA16N::operator==(const FPackedRGBA16N& B) const
{
	FVector	V1 = *this;
	FVector	V2 = B;

	if (FMath::Abs(V1.X - V2.X) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if (FMath::Abs(V1.Y - V2.Y) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if (FMath::Abs(V1.Z - V2.Z) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}

FORCEINLINE bool FPackedRGBA16N::operator!=(const FPackedRGBA16N& B) const
{
	return !(*this == B);
}

FORCEINLINE FPackedRGBA16N::operator FVector() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector and return it.
	FVector UnpackedVector;
	VectorStoreFloat3(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE FPackedRGBA16N::operator FVector4() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector4 and return it.
	FVector4 UnpackedVector;
	VectorStore(VectorToUnpack, &UnpackedVector);
	return UnpackedVector;
}

FORCEINLINE VectorRegister FPackedRGBA16N::GetVectorRegister() const
{
	VectorRegister VectorToUnpack = VectorLoadURGBA16N((void*)this);
	VectorToUnpack = VectorMultiplyAdd(VectorToUnpack, MakeVectorRegister(2.0f, 2.0f, 2.0f, 2.0f), MakeVectorRegister(-1.0f, -1.0f, -1.0f, -1.0f));
	VectorResetFloatRegisters();
	// Return unpacked vector register.
	return VectorToUnpack;
}
