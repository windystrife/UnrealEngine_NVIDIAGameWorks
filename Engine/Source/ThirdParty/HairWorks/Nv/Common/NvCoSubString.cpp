/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoSubString.h"

#include <Nv/Common/Util/NvCoCharUtil.h>

namespace nvidia {
namespace Common {

/* static */const SubString SubString::s_empty(NV_NULL, 0);

SubString::SubString(InitCstr, const Char* in)
{
	SizeT size = ::strlen(in);
	m_chars = const_cast<Char*>(in);
	m_size = IndexT(size);	
}

Bool SubString::equals(const ThisType& rhs) const
{
	if (this == &rhs)
	{
		return true;
	}
	if (rhs.m_size != m_size)
	{
		return false;
	}
	if (m_chars != rhs.m_chars)
	{
		const IndexT size = m_size;
		for (IndexT i = 0; i < size; i++)
		{
			if (m_chars[i] != rhs.m_chars[i])
			{
				return false;
			}
		}
	}
	return true;
}

IndexT SubString::indexOf(Char c) const
{
	const Char* chars = m_chars;
	const IndexT size = m_size;
	for (IndexT i = 0; i < size; i++)
	{
		if (chars[i] == c)
		{
			return i;
		}
	}
	return -1;
}

IndexT SubString::indexOf(Char c, IndexT from) const
{
	NV_CORE_ASSERT(from >= 0);
	const Char* chars = m_chars;
	const IndexT size = m_size;
	for (IndexT i = from; i < size; i++)
	{
		if (chars[i] == c)
		{
			return i;
		}
	}
	return -1;
}

IndexT SubString::reverseIndexOf(Char c) const
{
	const Char* start = m_chars;
	const Char* cur = start + m_size - 1;
	for (; cur >= start; --cur)
	{
		if (*cur == c)
		{
			return IndexT(PtrDiffT(cur - start));
		}
	}
	return -1;
}

SubString SubString::subStringWithEnd(IndexT start, IndexT end) const
{
	const IndexT size = m_size;
	// Handle negative indices
	start = (start < 0) ? (size + start) : start;
	end = (end < 0) ? (size + end) : end;

	NV_CORE_ASSERT(start >= 0 && end >= start);

	// Clamp to end
	end = (end >= size) ? size : end;
	start = (start > end) ? end : start;

	return SubString(m_chars + start, end - start);
}

SubString SubString::subStringWithStart(IndexT start, IndexT subSize) const
{
	NV_CORE_ASSERT(subSize >= 0);
	const IndexT size = m_size;
	// Handle negative wrap around, and clamp if off end
	if (start < 0)
	{
		start = start + size;
		start = (start < 0) ? 0 : start;
	}
	else
	{
		start = (start > size) ? size : start;
	}
	// Handle size
	subSize = (start + subSize > size) ? (size - start) : subSize;
	return SubString(m_chars + start, subSize);
}

SubString SubString::head(IndexT end) const
{
	const IndexT size = m_size;
	if (end < 0)
	{
		end = size + end;
		end = (end < 0) ? 0 : end;
	}
	else
	{
		end = (end > size) ? size : end;
	}
	return SubString(m_chars, end);
}

SubString SubString::tail(IndexT start) const
{
	const IndexT size = m_size;
	if (start < 0)
	{
		start = size + start;
		start = (start < 0) ? 0 : start;
	}
	else
	{
		start = (start > size) ? size : start;
	}
	return SubString(m_chars + start, size - start);
}

Bool SubString::isAscii() const
{
	NV_COMPILE_TIME_ASSERT(sizeof(Int8) == sizeof(Char));
	const Int8* chars = (const Int8*)(m_chars);
	const IndexT size = m_size;
	for (IndexT i = 0; i < size; i++)
	{
		if (chars[i] < 0)
		{
			return false;
		}
	}
	return true;
}

Bool SubString::equalsCstr(const Char* rhs) const
{
	const Char*const start = m_chars;
	const IndexT size = m_size;
	for (IndexT i = 0; i < size; i++)
	{
		const Char c = rhs[i];
		if (c != start[i] || c == 0)
		{
			return false;
		}
	}
	return rhs[size] == 0;
}

Bool SubString::equalsI(const ThisType& rhs) const
{
	if (&rhs == this)
	{
		return true;
	}
	if (m_size != rhs.m_size)
	{
		return false;
	}
	if (m_chars == rhs.m_chars)
	{
		return true;
	}
	for (IndexT i = 0; i < m_size; i++)
	{
		if (CharUtil::toLower(m_chars[i]) != CharUtil::toLower(rhs.m_chars[i]))
		{
			return false;
		}
	}
	return true;
}

Char* SubString::storeCstr(Char* out, SizeT size) const
{
	// size contains terminating zero!
	NV_CORE_ASSERT(size > 0);
	NV_CORE_ASSERT(SizeT(m_size) < size);

	size--;	// We need space for terminating zero
	size = (SizeT(m_size) < size) ? SizeT(m_size) : size;

	Memory::copy(out, m_chars, sizeof(Char) * size);
	out[m_size] = 0;
	return out;
}

Char* SubString::storeConcat(Char* out) const
{
	if (m_size > 0)
	{
		Memory::copy(out, m_chars, sizeof(Char) * m_size);
	}
	return out + m_size;
}

Void SubString::split(Char c, Array<SubString>& out) const
{
	out.clear();
	const Char* cur = m_chars;
	const Char* endPos = end();

	while (cur < endPos)
	{
		const Char* start = cur;
		
		while (*cur != c && cur < endPos) cur++;

		out.pushBack(SubString(start, IndexT(cur - start)));
	
		cur++;
	}
}

Int SubString::toInt() const
{
	return toInt(10); 
}

Int SubString::toInt(Int base) const
{
	const Char* cur = begin();
	const Char* term = end();

	// Can't be empty
	NV_CORE_ASSERT(cur < term);

	Int neg = 0;
	if (*cur == '-')
	{
		neg = Int(0) - 1;
		cur++;
		// Must be at least one digit
		NV_CORE_ASSERT(cur < term);
	}

	Int value = 0;
	if (base == 10)
	{
		while (cur < term)
		{
			const Int c = *cur++;
			NV_CORE_ASSERT(c >= '0' && c <= '9');
			value = value * 10 + (c - '0');
		}
	}
	else
	{
		while (cur < term)
		{
			const Int c = *cur++;
			Int digit;
			if (c >= '0' && c <= '9')
			{
				digit = c - '0';
			}
			else if (c >= 'a' && c <= 'z')
			{
				digit = c - 'a' + 10;
			}
			else if (c >= 'A' && c <= 'Z')
			{
				digit = c - 'A' + 10;
			}
			else
			{
				NV_CORE_ALWAYS_ASSERT("Invalid char");
				return -1;
			}
			if (digit >= base)
			{
				NV_CORE_ALWAYS_ASSERT("Invalid char");
				return -1;
			}
			value = value * base + digit;
		}
	}
	// If neg = -1, will negate the output
	return (value ^ neg) - neg;
}

} // namespace Common 
} // namespace nvidia

