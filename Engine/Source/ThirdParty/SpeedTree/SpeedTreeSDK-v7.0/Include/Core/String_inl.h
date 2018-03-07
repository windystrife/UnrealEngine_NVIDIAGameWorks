///////////////////////////////////////////////////////////////////////  
//	String.inl
//
//	This file is part of IDV's lightweight STL-like container classes.
//	Like STL, these containers are templated and portable, but are thin
//	enough not to have some of the portability issues of STL nor will
//	it conflict with an application's similar libraries.  These containers
//	also contain some mechanisms for mitigating the inordinate number of
//	of heap allocations associated with the standard STL.
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


//////////////////////////////////////////////////////////////////////
// CBasicString::CBasicString

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator>::CBasicString( )
{
	CArray<char, bUseCustomAllocator>::SetHeapDescription("CString");
	*this = "";
}


//////////////////////////////////////////////////////////////////////
// CBasicString::CBasicString

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator>::CBasicString(const CBasicString& cCopy) :
	CArray<char, bUseCustomAllocator>( )
{
	CArray<char, bUseCustomAllocator>::SetHeapDescription("CString");
	*this = cCopy;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::CBasicString

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator>::CBasicString(const char* pchData)
{
	CArray<char, bUseCustomAllocator>::SetHeapDescription("CString");
	*this = pchData;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::~CBasicString

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator>::~CBasicString( )
{
	super::clear( );
}


//////////////////////////////////////////////////////////////////////
// CBasicString::clear

template <bool bUseCustomAllocator>
inline void CBasicString<bUseCustomAllocator>::clear(void)
{
	super::clear( );
	*this = "";
}


//////////////////////////////////////////////////////////////////////
// CBasicString::resize

template <bool bUseCustomAllocator>
inline void CBasicString<bUseCustomAllocator>::resize(size_t uiSize)
{
	super::reserve(uiSize + 1);
	super::m_uiSize = uiSize;
	super::m_pData[super::m_uiSize] = 0;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::c_str

template <bool bUseCustomAllocator>
inline const char* CBasicString<bUseCustomAllocator>::c_str(void) const
{
	return super::m_pData;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::clip

template <bool bUseCustomAllocator>
inline void CBasicString<bUseCustomAllocator>::clip(void)
{
	if (super::m_uiDataSize > super::m_uiSize + 1)
	{
		char* pNewData = super::Allocate(super::m_uiSize + 1);
		char* pNew = pNewData;
		char* pOld = super::m_pData;
		for (size_t i = 0; i < super::m_uiSize; ++pNew, ++pOld, ++i)
			*pNew = *pOld;
		
		super::Deallocate(super::m_pData);

		super::m_pData = pNewData;
		super::m_uiDataSize = super::m_uiSize + 1;
		super::m_pData[super::m_uiSize] = 0;
	}
}


//////////////////////////////////////////////////////////////////////
// CBasicString::length

template <bool bUseCustomAllocator>
inline size_t CBasicString<bUseCustomAllocator>::length(void) const
{
	return super::size( );
}


//////////////////////////////////////////////////////////////////////
// CBasicString::substr

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::substr(size_t uiStart, size_t uiCount) const
{
	CBasicString strReturn;

	size_t uiLength = super::size( );
	if (uiLength > 0 && uiStart < uiLength)
	{
		if (uiCount == (size_t)-1)
			uiCount = uiLength - uiStart;
		else if (uiStart + uiCount > uiLength)
			uiCount = uiLength - uiStart;
		strReturn.resize(uiCount);
		memmove(strReturn.m_pData, super::m_pData + uiStart, uiCount * sizeof(char));
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::find

template <bool bUseCustomAllocator>
inline size_t CBasicString<bUseCustomAllocator>::find(char chFind, size_t uiStart) const
{
	size_t uiReturn = (size_t)npos;
	for (size_t i = uiStart; i < super::size( ); ++i)
		if (super::m_pData[i] == chFind)
		{
			uiReturn = i;
			break;
		}

	return uiReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator +=

template <bool bUseCustomAllocator>
inline void CBasicString<bUseCustomAllocator>::operator += (const char* pchData)
{
	size_t uiSize = 0;
	if (pchData != 0)
		uiSize = strlen(pchData);
	if (uiSize > 0)
	{
		super::reserve(super::m_uiSize + uiSize + 1);
		memmove(&(super::m_pData[super::m_uiSize]), pchData, uiSize * sizeof(char));
		super::m_uiSize += uiSize;
		super::m_pData[super::m_uiSize] = 0;
	}
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator +

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::operator + (const char* pchData) const
{
	CBasicString strReturn;
	size_t uiSize = 0;
	if (pchData != 0)
		uiSize = strlen(pchData);
	if (uiSize > 0)
	{
		strReturn.reserve(super::m_uiSize + uiSize + 1);
		strReturn = *this;
		strReturn += pchData;
	}
	
	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator +=

template <bool bUseCustomAllocator>
inline void CBasicString<bUseCustomAllocator>::operator += (const CBasicString& strRight)
{
	if (strRight.m_uiSize > 0)
	{
		super::reserve(super::m_uiSize + strRight.m_uiSize + 1);
		memmove(&(super::m_pData[super::m_uiSize]), strRight.m_pData, strRight.m_uiSize * sizeof(char));
		super::m_uiSize += strRight.m_uiSize;
		super::m_pData[super::m_uiSize] = 0;
	}
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator +

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::operator + (const CBasicString& strRight) const
{
	CBasicString strReturn(*this);
	strReturn += strRight;

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator +=

template <bool bUseCustomAllocator>
inline void CBasicString<bUseCustomAllocator>::operator += (const char& chRight)
{
	super::reserve(super::m_uiSize + 2);
	super::m_pData[super::m_uiSize] = chRight;
	super::m_pData[++super::m_uiSize] = 0;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator +

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::operator + (const char& chRight) const
{
	CBasicString strReturn(*this);
	strReturn += chRight;

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator =

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator>& CBasicString<bUseCustomAllocator>::operator = (const CBasicString& strRight)
{
	super::reserve(strRight.m_uiSize + 1);
	if (strRight.m_uiSize > 0)
		memmove(super::m_pData, strRight.m_pData, strRight.m_uiSize * sizeof(char));
	super::m_uiSize = strRight.m_uiSize;
	super::m_pData[super::m_uiSize] = 0;

	return *this;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator =

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator>& CBasicString<bUseCustomAllocator>::operator = (const char* pchData)
{
	if (pchData == 0)
		*this = "";
	else
	{
		size_t uiSize = strlen(pchData);
		super::reserve(uiSize + 1);
		if (uiSize > 0)
			memmove(super::m_pData, pchData, uiSize * sizeof(char));
		super::m_uiSize = uiSize;
		super::m_pData[super::m_uiSize] = 0;
	}

	return *this;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator ==

template <bool bUseCustomAllocator>
inline bool CBasicString<bUseCustomAllocator>::operator == (const CBasicString& strRight) const
{
	char* pThis = super::m_pData;
	char* pRight = strRight.m_pData;
	for ( ; *pThis == *pRight; ++pThis, ++pRight)
		if (*pThis == 0)
			return true;

	return false;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator !=

template <bool bUseCustomAllocator>
inline bool CBasicString<bUseCustomAllocator>::operator != (const CBasicString& strRight) const
{
	return !(*this == strRight);
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator <

template <bool bUseCustomAllocator>
inline bool CBasicString<bUseCustomAllocator>::operator < (const CBasicString& strRight) const
{
	char* pThis = super::m_pData;
	char* pRight = strRight.m_pData;
	for ( ; *pThis == *pRight; ++pThis, ++pRight)
		if (*pThis == 0)
			return false;
	return *pThis < *pRight;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::operator >

template <bool bUseCustomAllocator>
inline bool CBasicString<bUseCustomAllocator>::operator > (const CBasicString& strRight) const
{
	char* pThis = super::m_pData;
	char* pRight = strRight.m_pData;
	for ( ; *pThis == *pRight; ++pThis, ++pRight)
		if (*pThis == 0)
			return false;
	return *pThis > *pRight;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::Format

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::Format(const char* pchFormat, ...)
{
	const st_int32 c_nMaxStringSize = 2048;
	st_char szBuffer[c_nMaxStringSize];
	va_list vlArgs;
	va_start(vlArgs, pchFormat);
	(void) st_vsnprintf(szBuffer, c_nMaxStringSize - 1, pchFormat, vlArgs);
	szBuffer[c_nMaxStringSize - 1] = '\0';

	return CBasicString(szBuffer);
}


//////////////////////////////////////////////////////////////////////
// CBasicString::Extension

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::Extension(char chExtensionChar) const 
{ 
	CBasicString strReturn;

	char* pCurrent = super::m_pData + super::m_uiSize - 1;
	while (pCurrent >= super::m_pData)
	{
		if (*pCurrent == chExtensionChar)
		{
			++pCurrent;
			size_t uiSize = (super::m_pData + super::m_uiSize - pCurrent);
			strReturn.resize(uiSize);
			memmove(strReturn.m_pData, pCurrent, uiSize * sizeof(char));
			break;
		}
		--pCurrent;
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::NoExtension

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::NoExtension(char chExtensionChar) const 
{ 
	CBasicString strReturn = *this;

	char* pCurrent = super::m_pData + super::m_uiSize - 1;
	while (pCurrent >= super::m_pData)
	{
		if (*pCurrent == chExtensionChar)
		{
			size_t uiSize = (pCurrent - super::m_pData);
			strReturn.resize(uiSize);
			memmove(strReturn.m_pData, super::m_pData, uiSize * sizeof(char));
			break;
		}
		--pCurrent;
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::Path

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::Path(CBasicString strDelimiters) const 
{
	CBasicString strReturn;

	char* pCurrent = super::m_pData + super::m_uiSize - 1;
	while (pCurrent >= super::m_pData)
	{
		for (char* pDelim = strDelimiters.m_pData; *pDelim != 0; ++pDelim)
		{
			if (*pCurrent == *pDelim)
			{
				++pCurrent;
				size_t uiSize = (pCurrent - super::m_pData);
				strReturn.resize(uiSize);
				memmove(strReturn.m_pData, super::m_pData, uiSize * sizeof(char));
				pCurrent = super::m_pData;
				break;
			}
		}
		--pCurrent;
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicString::NoPath

template <bool bUseCustomAllocator>
inline CBasicString<bUseCustomAllocator> CBasicString<bUseCustomAllocator>::NoPath(CBasicString strDelimiters) const 
{ 
	CBasicString strReturn = *this;

	char* pCurrent = super::m_pData + super::m_uiSize - 1;
	while (pCurrent >= super::m_pData)
	{
		for (char* pDelim = strDelimiters.m_pData; *pDelim != 0; ++pDelim)
		{
			if (*pCurrent == *pDelim)
			{
				++pCurrent;
				size_t uiSize = (super::m_pData + super::m_uiSize - pCurrent);
				strReturn.resize(uiSize);
				memmove(strReturn.m_pData, pCurrent, uiSize * sizeof(char));
				pCurrent = super::m_pData;
				break;
			}
		}
		--pCurrent;
	}

	return strReturn;
}


