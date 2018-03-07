///////////////////////////////////////////////////////////////////////  
//  Fixed.cpp
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with 
//  the terms of that agreement.
//
//      Copyright (c) 2003-2014 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com


///////////////////////////////////////////////////////////////////////  
//  Preprocessor

#include "Core/Fixed.h"
using namespace SpeedTree;


///////////////////////////////////////////////////////////////////////
//  Static variables

st_uint32 CFixedNumber::m_uiBitsUsedForFraction = 8;
st_float32 CFixedNumber::m_fOneOverStep = float(1 << CFixedNumber::m_uiBitsUsedForFraction);
st_float32 CFixedNumber::m_fStep = (1.0f / CFixedNumber::m_fOneOverStep);

