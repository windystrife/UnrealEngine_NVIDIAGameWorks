// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Color.h"

/** 
 * 3 component vector corresponding to DXGI_FORMAT_R11G11B10_FLOAT. 
 * Conversion code from XMFLOAT3PK in DirectXPackedVector.h
 */
class FFloat3Packed
{
public: 
	union
    {
        struct
        {
            uint32_t xm : 6; // x-mantissa
            uint32_t xe : 5; // x-exponent
            uint32_t ym : 6; // y-mantissa
            uint32_t ye : 5; // y-exponent
            uint32_t zm : 5; // z-mantissa
            uint32_t ze : 5; // z-exponent
        };
        uint32_t v;
    };

	FFloat3Packed() {}

	explicit FFloat3Packed(const FLinearColor& Src);

	FLinearColor ToLinearColor() const;
};


FORCEINLINE FFloat3Packed::FFloat3Packed(const FLinearColor& Src)
{
	uint32 IValue[4];
	IValue[0] = *(uint32*)&Src.R;
	IValue[1] = *(uint32*)&Src.G;
	IValue[2] = *(uint32*)&Src.B;
	IValue[3] = *(uint32*)&Src.A;

	uint32 Result[3];

    // X & Y Channels (5-bit exponent, 6-bit mantissa)
    for (uint32 j=0; j < 2; ++j)
    {
        uint32 Sign = IValue[j] & 0x80000000;
        uint32 I = IValue[j] & 0x7FFFFFFF;

        if ((I & 0x7F800000) == 0x7F800000)
        {
            // INF or NAN
            Result[j] = 0x7c0;
            if (( I & 0x7FFFFF ) != 0)
            {
                Result[j] = 0x7c0 | (((I>>17)|(I>>11)|(I>>6)|(I))&0x3f);
            }
            else if ( Sign )
            {
                // -INF is clamped to 0 since 3PK is positive only
                Result[j] = 0;
            }
        }
        else if ( Sign )
        {
            // 3PK is positive only, so clamp to zero
            Result[j] = 0;
        }
        else if (I > 0x477E0000U)
        {
            // The number is too large to be represented as a float11, set to max
            Result[j] = 0x7BF;
        }
        else
        {
            if (I < 0x38800000U)
            {
                // The number is too small to be represented as a normalized float11
                // Convert it to a denormalized value.
                uint32 Shift = 113U - (I >> 23U);
                I = (0x800000U | (I & 0x7FFFFFU)) >> Shift;
            }
            else
            {
                // Rebias the exponent to represent the value as a normalized float11
                I += 0xC8000000U;
            }
     
            Result[j] = ((I + 0xFFFFU + ((I >> 17U) & 1U)) >> 17U)&0x7ffU;
        }
    }

    // Z Channel (5-bit exponent, 5-bit mantissa)
    uint32 Sign = IValue[2] & 0x80000000;
    uint32 I = IValue[2] & 0x7FFFFFFF;

    if ((I & 0x7F800000) == 0x7F800000)
    {
        // INF or NAN
        Result[2] = 0x3e0;
        if ( I & 0x7FFFFF )
        {
            Result[2] = 0x3e0 | (((I>>18)|(I>>13)|(I>>3)|(I))&0x1f);
        }
        else if ( Sign )
        {
            // -INF is clamped to 0 since 3PK is positive only
            Result[2] = 0;
        }
    }
    else if ( Sign )
    {
        // 3PK is positive only, so clamp to zero
        Result[2] = 0;
    }
    else if (I > 0x477C0000U)
    {
        // The number is too large to be represented as a float10, set to max
        Result[2] = 0x3df;
    }
    else
    {
        if (I < 0x38800000U)
        {
            // The number is too small to be represented as a normalized float10
            // Convert it to a denormalized value.
            uint32 Shift = 113U - (I >> 23U);
            I = (0x800000U | (I & 0x7FFFFFU)) >> Shift;
        }
        else
        {
            // Rebias the exponent to represent the value as a normalized float10
            I += 0xC8000000U;
        }
     
        Result[2] = ((I + 0x1FFFFU + ((I >> 18U) & 1U)) >> 18U)&0x3ffU;
    }

    // Pack Result into memory
    v = (Result[0] & 0x7ff)
        | ( (Result[1] & 0x7ff) << 11 )
        | ( (Result[2] & 0x3ff) << 22 );
}

FORCEINLINE FLinearColor FFloat3Packed::ToLinearColor() const
{
	uint32 Result[4];
    uint32 Mantissa;
    uint32 Exponent;

	const FFloat3Packed* pSource = this;

	// X Channel (6-bit mantissa)
    Mantissa = pSource->xm;

    if ( pSource->xe == 0x1f ) // INF or NAN
    {
        Result[0] = 0x7f800000 | (pSource->xm << 17);
    }
    else
    {
        if ( pSource->xe != 0 ) // The value is normalized
        {
            Exponent = pSource->xe;
        }
        else if (Mantissa != 0) // The value is denormalized
        {
            // Normalize the value in the resulting float
            Exponent = 1;
    
            do
            {
                Exponent--;
                Mantissa <<= 1;
            } while ((Mantissa & 0x40) == 0);
    
            Mantissa &= 0x3F;
        }
        else // The value is zero
        {
            Exponent = (uint32)-112;
        }
    
        Result[0] = ((Exponent + 112) << 23) | (Mantissa << 17);
    }

    // Y Channel (6-bit mantissa)
    Mantissa = pSource->ym;

    if ( pSource->ye == 0x1f ) // INF or NAN
    {
        Result[1] = 0x7f800000 | (pSource->ym << 17);
    }
    else
    {
        if ( pSource->ye != 0 ) // The value is normalized
        {
            Exponent = pSource->ye;
        }
        else if (Mantissa != 0) // The value is denormalized
        {
            // Normalize the value in the resulting float
            Exponent = 1;
    
            do
            {
                Exponent--;
                Mantissa <<= 1;
            } while ((Mantissa & 0x40) == 0);
    
            Mantissa &= 0x3F;
        }
        else // The value is zero
        {
            Exponent = (uint32)-112;
        }
    
        Result[1] = ((Exponent + 112) << 23) | (Mantissa << 17);
    }

    // Z Channel (5-bit mantissa)
    Mantissa = pSource->zm;

    if ( pSource->ze == 0x1f ) // INF or NAN
    {
        Result[2] = 0x7f800000 | (pSource->zm << 17);
    }
    else
    {
        if ( pSource->ze != 0 ) // The value is normalized
        {
            Exponent = pSource->ze;
        }
        else if (Mantissa != 0) // The value is denormalized
        {
            // Normalize the value in the resulting float
            Exponent = 1;
    
            do
            {
                Exponent--;
                Mantissa <<= 1;
            } while ((Mantissa & 0x20) == 0);
    
            Mantissa &= 0x1F;
        }
        else // The value is zero
        {
            Exponent = (uint32)-112;
        }

        Result[2] = ((Exponent + 112) << 23) | (Mantissa << 18);
    }

	FLinearColor ResultColor;
	ResultColor.R = *(float*)&Result[0];
	ResultColor.G = *(float*)&Result[1];
	ResultColor.B = *(float*)&Result[2];
	ResultColor.A = 0;
	return ResultColor;
}

/** 
 * 4 component vector corresponding to PF_R8G8B8A8_SNORM. 
 * This differs from FColor which is BGRA.
 */
class FFixedRGBASigned8
{
public: 
	union
    {
        struct
        {
			int8 R;
			int8 G;
			int8 B;
			int8 A;
        };
        uint32 Packed;
    };

	FFixedRGBASigned8() {}

	explicit FFixedRGBASigned8(const FLinearColor& Src);

	FLinearColor ToLinearColor() const;
};

inline FFixedRGBASigned8::FFixedRGBASigned8(const FLinearColor& Src)
{
	const float Scale = MAX_int8;
	R = (int8)FMath::Clamp<int32>(FMath::RoundToInt(Src.R * Scale), MIN_int8, MAX_int8);
	G = (int8)FMath::Clamp<int32>(FMath::RoundToInt(Src.G * Scale), MIN_int8, MAX_int8);
	B = (int8)FMath::Clamp<int32>(FMath::RoundToInt(Src.B * Scale), MIN_int8, MAX_int8);
	A = (int8)FMath::Clamp<int32>(FMath::RoundToInt(Src.A * Scale), MIN_int8, MAX_int8);
}

inline FLinearColor FFixedRGBASigned8::ToLinearColor() const
{
	const float Scale = 1.0f / MAX_int8;
	return FLinearColor(R * Scale, G * Scale, B * Scale, A * Scale);
}