///////////////////////////////////////////////////////////////////////  
//	Types.inl
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		http://www.idvinc.com


//	Half-to-float and Float-to-half code taken from "OpenGL ES 2.0
//	Programming Guide" by Aaftab Munshi, Dan Ginsburg, and Dave Shreiner.


//////////////////////////////////////////////////////////////////////
// Constants

// -15 stored using a single precision bias of 127
const unsigned int  HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP = 0x38000000;

// max exponent value in single precision that will be converted 
// to Inf or Nan when stored as a half-float
const unsigned int  HALF_FLOAT_MAX_BIASED_EXP_AS_SINGLE_FP_EXP = 0x47800000;

// 255 is the max exponent biased value
const unsigned int  FLOAT_MAX_BIASED_EXP = (0xFF << 23);
const unsigned int  HALF_FLOAT_MAX_BIASED_EXP = (0x1F << 10);


//////////////////////////////////////////////////////////////////////
// st_float16::st_float16

inline st_float16::st_float16( )
{
	*this = st_float16(0.0f);
}


//////////////////////////////////////////////////////////////////////
// st_float16::st_float16

inline st_float16::st_float16(st_float32 fSinglePrecision)
{
	union
	{
		st_float32 f;
		st_uint32 x;
	};

    f = fSinglePrecision;
    st_uint32 sign = (st_uint16) (x >> 31);
    st_uint32 mantissa;
    st_uint32 exp;

    // get mantissa
    mantissa = x & ((1 << 23) - 1);

    // get exponent bits
    exp = x & FLOAT_MAX_BIASED_EXP;

    if (exp >= HALF_FLOAT_MAX_BIASED_EXP_AS_SINGLE_FP_EXP)
    {
        // check if the original single precision float number is a NaN
        if (mantissa && (exp == FLOAT_MAX_BIASED_EXP))
        {
            // we have a single precision NaN
            mantissa = (1 << 23) - 1;
        }
        else
        {
            // 16-bit half-float representation stores number as Inf
            mantissa = 0;
        }
        m_uiValue = (((st_uint16)sign) << 15) | (st_uint16)(HALF_FLOAT_MAX_BIASED_EXP) |
              (st_uint16)(mantissa >> 13);
    }
    // check if exponent is <= -15
    else if (exp <= HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP)
    {
       // store a denorm half-float value or zero
        exp = (HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP - exp) >> 23;
        mantissa >>= (14 + exp);
        m_uiValue = (((st_uint16)sign) << 15) | (st_uint16)(mantissa);
    }
    else
    {
        m_uiValue = (((st_uint16)sign) << 15) | 
                      (st_uint16)((exp - HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP) >> 13) |
                      (st_uint16)(mantissa >> 13);
    }
}


//////////////////////////////////////////////////////////////////////
// st_float16::st_float16

inline st_float16::st_float16(const st_float16& hfCopy)
{
	m_uiValue = hfCopy.m_uiValue;
}


//////////////////////////////////////////////////////////////////////
// st_float16::operator st_float32

inline st_float16::operator st_float32(void) const
{
    st_uint32 sign = (st_uint32) (m_uiValue >> 15);
    st_uint32 mantissa = (st_uint32) (m_uiValue & ((1 << 10) - 1));
    st_uint32 exp = (st_uint32) (m_uiValue & HALF_FLOAT_MAX_BIASED_EXP);
    union
	{
		st_float32 f;
		st_uint32 x;
	};

    if (exp == HALF_FLOAT_MAX_BIASED_EXP)
    {
        // we have a half-float NaN or Inf
        // half-float NaNs will be converted to a single precision NaN
        // half-float Infs will be converted to a single precision Inf
        exp = FLOAT_MAX_BIASED_EXP;

        if (mantissa)
            mantissa = (1 << 23) - 1;   // set all bits to indicate a NaN
    }
    else if (exp == 0x0)
    {
        // convert half-float zero/denorm to single precision value
        if (mantissa)
        {
            mantissa <<= 1;
            exp = HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP;

            // check for leading 1 in denorm mantissa
            while ((mantissa & (1 << 10)) == 0)
            {
                // for every leading 0, decrement single precision exponent by 1
                // and shift half-float mantissa value to the left
                mantissa <<= 1;
                exp -= (1 << 23);
            }

            // clamp the mantissa to 10-bits
            mantissa &= ((1 << 10) - 1);

            // shift left to generate single-precision mantissa of 23-bits
            mantissa <<= 13;
        }
    }
    else
    {
        // shift left to generate single-precision mantissa of 23-bits
        mantissa <<= 13;

        // generate single precision biased exponent value
        exp = (exp << 13) + HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP;
    }

    x = (sign << 31) | exp | mantissa;

    return f;
}


//////////////////////////////////////////////////////////////////////
// Enumeration::Enumeration

template<class E, class T>
inline Enumeration<E, T>::Enumeration( )
	: m_eEnum(E( ))
{
}


//////////////////////////////////////////////////////////////////////
// Enumeration::Enumeration

template<class E, class T>
inline Enumeration<E, T>::Enumeration(E eValue)
	: m_eEnum(static_cast<T>(eValue))
{
}


//////////////////////////////////////////////////////////////////////
// Enumeration::Enumeration

template<class E, class T>
inline Enumeration<E, T>::Enumeration(st_int32 nValue)
	: m_eEnum(static_cast<T>(nValue))
{
}


//////////////////////////////////////////////////////////////////////
// Enumeration::operator E

template<class E, class T>
inline Enumeration<E, T>::operator E(void) const
{
	return static_cast<E>(m_eEnum);
}



