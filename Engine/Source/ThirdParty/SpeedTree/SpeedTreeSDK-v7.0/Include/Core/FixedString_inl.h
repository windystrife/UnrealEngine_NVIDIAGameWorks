///////////////////////////////////////////////////////////////////////
//	FixedString.inl
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

#if !defined(__GNUC__) && !defined(__CELLOS_LV2__) && !defined(__APPLE__) && !defined(__SNC__) && !defined(NDEV) && !defined(vsnprintf)
	#define SpeedTreeVsnPrintF _vsnprintf
#else
	#define SpeedTreeVsnPrintF vsnprintf
#endif


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::CBasicFixedString

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize>::CBasicFixedString(void)
{
	assert(uiDataSize > 0);
	*this = "";
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::CBasicFixedString

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize>::CBasicFixedString(const CBasicFixedString& cCopy)
{
	assert(uiDataSize > 0);
	*this = cCopy;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::CBasicFixedString

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize>::CBasicFixedString(const char* pchData)
{
	assert(uiDataSize > 0);
	*this = pchData;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::~CBasicFixedString

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize>::~CBasicFixedString(void)
{
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::clear

template <size_t uiDataSize>
inline void CBasicFixedString<uiDataSize>::clear(void)
{
	m_uiSize = 0;
	m_aData[0] = 0;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::empty

template <size_t uiDataSize>
bool CBasicFixedString<uiDataSize>::empty(void) const
{
	return (m_uiSize == 0);
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::resize

template <size_t uiDataSize>
inline bool CBasicFixedString<uiDataSize>::resize(size_t uiSize)
{
	if (uiSize < uiDataSize - 1)
	{
		m_uiSize = uiSize;
		m_aData[m_uiSize] = 0;
		return true;
	}

	m_uiSize = uiDataSize - 1;
	m_aData[m_uiSize] = 0;
	return false;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::c_str

template <size_t uiDataSize>
inline const char* CBasicFixedString<uiDataSize>::c_str(void) const
{
	return m_aData;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::length

template <size_t uiDataSize>
inline size_t CBasicFixedString<uiDataSize>::length(void) const
{
	return m_uiSize;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::size

template <size_t uiDataSize>
inline size_t CBasicFixedString<uiDataSize>::size(void) const
{
	return m_uiSize;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::substr

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::substr(size_t uiStart, size_t uiCount)
{
	CBasicFixedString strReturn;

	if (m_uiSize > 0 && uiStart < m_uiSize)
	{
		if (uiCount == (size_t)-1)
			uiCount = m_uiSize - uiStart;
		else if (uiStart + uiCount > m_uiSize)
			uiCount = m_uiSize - uiStart;
		strReturn.resize(uiCount);
		memmove(strReturn.m_aData, m_aData + uiStart, uiCount * sizeof(char));
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::find

template <size_t uiDataSize>
inline size_t CBasicFixedString<uiDataSize>::find(char chFind, size_t uiStart) const
{
	size_t uiReturn = size_t(npos);
	for (size_t i = uiStart; i < m_uiSize; ++i)
	{
		if (m_aData[i] == chFind)
		{
			uiReturn = i;
			break;
		}
	}

	return uiReturn;
}


/////////////////////////////////////////////////////////////////////
// CBasicFixedString::erase_all

template <size_t uiDataSize>
inline void CBasicFixedString<uiDataSize>::erase_all(const char chErase)
{
	for (size_t i = 0; i < m_uiSize; )
	{
		size_t uiCheck = m_uiSize - i - 1;
		if (m_aData[uiCheck] == chErase)
		{
			memmove(&(m_aData[uiCheck]), &(m_aData[uiCheck + 1]), (m_uiSize - uiCheck) * sizeof(char));
			--m_uiSize;
		}
		else
			++i;
	}
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator +=

template <size_t uiDataSize>
inline void CBasicFixedString<uiDataSize>::operator += (const char* pchData)
{
	size_t uiSize = 0;
	if (pchData != NULL)
		uiSize = strlen(pchData);
	if (uiSize > 0)
	{
		assert(m_uiSize + uiSize < uiDataSize - 1);
		memmove(&(m_aData[m_uiSize]), pchData, uiSize * sizeof(char));
		m_uiSize += uiSize;
		m_aData[m_uiSize] = 0;
	}
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator +

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::operator + (const char* pchData) const
{
	CBasicFixedString strReturn(*this);
	strReturn += pchData;

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator +=

template <size_t uiDataSize>
inline void CBasicFixedString<uiDataSize>::operator += (const CBasicFixedString& strRight)
{
	if (strRight.m_uiSize > 0)
	{
		assert(m_uiSize + strRight.m_uiSize < uiDataSize - 1);
		memmove(&(m_aData[m_uiSize]), strRight.m_aData, strRight.m_uiSize * sizeof(char));
		m_uiSize += strRight.m_uiSize;
		m_aData[m_uiSize] = 0;
	}
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator +

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::operator + (const CBasicFixedString& strRight) const
{
	CBasicFixedString strReturn(*this);
	strReturn += strRight;

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator +=

template <size_t uiDataSize>
inline void CBasicFixedString<uiDataSize>::operator += (const char& chRight)
{
	assert(m_uiSize + 1 < uiDataSize - 1);
	m_aData[m_uiSize] = chRight;
	m_aData[++m_uiSize] = 0;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator +

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::operator + (const char& chRight) const
{
	CBasicFixedString strReturn(*this);
	strReturn += chRight;

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator =

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize>& CBasicFixedString<uiDataSize>::operator = (const CBasicFixedString& strRight)
{
	m_uiSize = strRight.m_uiSize;
	if (strRight.m_uiSize > 0)
		memmove(m_aData, strRight.m_aData, m_uiSize * sizeof(char));
	m_aData[m_uiSize] = 0;

	return *this;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator =

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize>& CBasicFixedString<uiDataSize>::operator = (const char* pchData)
{
	if (pchData == NULL)
	{
		m_uiSize = 0;
		m_aData[0] = 0;
	}
	else
	{
		size_t uiSize = strlen(pchData);
		assert(uiSize < uiDataSize - 1);
		if (uiSize > 0)
			memmove(m_aData, pchData, uiSize * sizeof(char));
		m_uiSize = uiSize;
		m_aData[m_uiSize] = 0;
	}

	return *this;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator ==

template <size_t uiDataSize>
inline bool CBasicFixedString<uiDataSize>::operator == (const CBasicFixedString& strRight) const
{
	char* pThis = (char*)m_aData;
	char* pRight = (char*)strRight.m_aData;
	for ( ; *pThis == *pRight; ++pThis, ++pRight)
		if (*pThis == 0)
			return true;

	return false;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator !=

template <size_t uiDataSize>
inline bool CBasicFixedString<uiDataSize>::operator != (const CBasicFixedString& strRight) const
{
	return !(*this == strRight);
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator <

template <size_t uiDataSize>
inline bool CBasicFixedString<uiDataSize>::operator < (const CBasicFixedString& strRight) const
{
	char* pThis = (char*)m_aData;
	char* pRight = (char*)strRight.m_aData;
	for ( ; *pThis == *pRight; ++pThis, ++pRight)
		if (*pThis == 0)
			return false;
	return *pThis < *pRight;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator >

template <size_t uiDataSize>
inline bool CBasicFixedString<uiDataSize>::operator > (const CBasicFixedString& strRight) const
{
	char* pThis = m_aData;
	char* pRight = strRight.m_aData;
	for ( ; *pThis == *pRight; ++pThis, ++pRight)
		if (*pThis == 0)
			return false;
	return *pThis > *pRight;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator >

template <size_t uiDataSize>
inline char& CBasicFixedString<uiDataSize>::operator [] (size_t uiIndex)
{
	assert(uiIndex < m_uiSize);
	return m_aData[uiIndex];
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::operator >

template <size_t uiDataSize>
inline const char& CBasicFixedString<uiDataSize>::operator [] (size_t uiIndex) const
{
	assert(uiIndex < m_uiSize);
	return m_aData[uiIndex];
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::Format

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::Format(const char* pchFormat, ...)
{
	CBasicFixedString strReturn;

	if (pchFormat != NULL)
	{
		va_list vlArgs;
		va_start(vlArgs, pchFormat);

		st_int32 nCharsWritten = st_vsnprintf(strReturn.m_aData, uiDataSize - 1, pchFormat, vlArgs);
		assert(nCharsWritten < st_int32(uiDataSize) - 1);
		strReturn.m_uiSize = nCharsWritten;

		va_end(vlArgs);
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::Extension

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::Extension(char chExtensionChar) const
{
	CBasicFixedString strReturn;

	const char* pCurrent = m_aData + m_uiSize - 1;
	while (pCurrent >= m_aData)
	{
		if (*pCurrent == chExtensionChar)
		{
			++pCurrent;
			size_t uiSize = (m_aData + m_uiSize - pCurrent);
			strReturn.resize(uiSize);
			memmove(strReturn.m_aData, pCurrent, uiSize * sizeof(char));
			break;
		}
		--pCurrent;
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::NoExtension

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::NoExtension(char chExtensionChar) const
{
	CBasicFixedString strReturn = *this;

	const char* pCurrent = m_aData + m_uiSize - 1;
	while (pCurrent >= m_aData)
	{
		if (*pCurrent == chExtensionChar)
		{
			size_t uiSize = (pCurrent - m_aData);
			strReturn.resize(uiSize);
			memmove(strReturn.m_aData, m_aData, uiSize * sizeof(char));
			break;
		}
		--pCurrent;
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::Path

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::Path(CBasicFixedString strDelimiters) const
{
	CBasicFixedString strReturn;

	const char* pCurrent = m_aData + m_uiSize - 1;
	while (pCurrent >= m_aData)
	{
		for (const char* pDelim = strDelimiters.m_aData; *pDelim != '\0'; ++pDelim)
		{
			if (*pCurrent == *pDelim)
			{
				++pCurrent;
				size_t uiSize = (pCurrent - m_aData);
				strReturn.resize(uiSize);
				memmove(strReturn.m_aData, m_aData, uiSize * sizeof(char));
				pCurrent = m_aData;
				break;
			}
		}
		--pCurrent;
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::NoPath

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::NoPath(CBasicFixedString strDelimiters) const
{
	CBasicFixedString strReturn = *this;

	const char* pCurrent = m_aData + m_uiSize - 1;
	while (pCurrent >= m_aData)
	{
		for (const char* pDelim = strDelimiters.m_aData; *pDelim != '\0'; ++pDelim)
		{
			if (*pCurrent == *pDelim)
			{
				++pCurrent;
				size_t uiSize = (m_aData + m_uiSize - pCurrent);
				strReturn.resize(uiSize);
				memmove(strReturn.m_aData, pCurrent, uiSize * sizeof(char));
				pCurrent = m_aData;
				break;
			}
		}
		--pCurrent;
	}

	return strReturn;
}


//////////////////////////////////////////////////////////////////////
// CBasicFixedString::MakePlatformCompliantPath

template <size_t uiDataSize>
inline CBasicFixedString<uiDataSize> CBasicFixedString<uiDataSize>::MakePlatformCompliantPath(void) const
{
	CBasicFixedString strCompliantPath = *this;

	const size_t c_siLength = strCompliantPath.length( );
	#ifdef _XBOX
		for (size_t i = 0; i < c_siLength; ++i)
			if (strCompliantPath[i] == '/')
				strCompliantPath[i] = '\\';
	#else
		for (size_t i = 0; i < c_siLength; ++i)
			if (strCompliantPath[i] == '\\')
				strCompliantPath[i] = '/';
	#endif

	return strCompliantPath;
}


