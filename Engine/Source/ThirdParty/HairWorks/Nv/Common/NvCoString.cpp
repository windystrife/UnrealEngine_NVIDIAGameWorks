/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#include "NvCoString.h"

#include <stdio.h>

namespace nvidia {
namespace Common {

// Leave it writable -> it will only ever get written with zero. If it's made const, it might be put in const memory, and cause a fault on write
static Char s_emptyChars[] = { 0 };
/* static */const String String::s_empty(s_emptyChars, 0, 1, NV_NULL);

String::~String()
{
	if (m_allocator && m_capacity > 0)
	{
		m_allocator->deallocate(m_chars, m_capacity);
	}
}

/* static */IndexT String::calcInitialCapacity(IndexT size)
{
	return (size < 16) ? 16 : size;
}

/* static */IndexT String::calcCapacity(IndexT capacity, IndexT newCapacity)
{
	if (newCapacity < 16)
	{
		return 16;
	}
	// If the newCapacity more than doubles the old capacity -> assume it's enough
	if (newCapacity - capacity > (capacity >> 1))
	{
		return newCapacity;
	}
	if (newCapacity < 4096)
	{
		if (capacity * 2 > newCapacity)
		{
			return capacity * 2;
		}
	}
	// Assume it's going to get bigger
	return newCapacity + (newCapacity / 2);
}

void String::_ctor(const SubString& rhs)
{
	MemoryAllocator* allocator = MemoryAllocator::getInstance();
	if (rhs.m_size > 0)
	{
		// Need to make space
		IndexT capacity = calcInitialCapacity(rhs.m_size);
		m_chars = (Char*)allocator->allocate(capacity);
		m_capacity = capacity;
		m_size = rhs.m_size;

		Memory::copy(m_chars, rhs.m_chars, sizeof(Char) * rhs.m_size);
	}
	else
	{
		m_capacity = 0;
		m_size = 0;
		m_chars = NV_NULL;
	}
	m_allocator = allocator;
}

void String::_ctor(const SubString& rhs, MemoryAllocator* allocator)
{
	if (rhs.m_size > 0)
	{
		// Need to make space
		IndexT capacity = calcInitialCapacity(rhs.m_size);
		m_chars = (Char*)allocator->allocate(capacity);
		m_capacity = capacity;
		m_size = rhs.m_size;

		Memory::copy(m_chars, rhs.m_chars, sizeof(Char) * rhs.m_size);
	}
	else
	{
		m_capacity = 0;
		m_size = 0;
		m_chars = NV_NULL;
	}
	m_allocator = allocator;
}

Void String::swap(ThisType& rhs)
{
	Op::swap(m_chars, rhs.m_chars);
	Op::swap(m_size, rhs.m_size);
	Op::swap(m_capacity, rhs.m_capacity);
	Op::swap(m_allocator, rhs.m_allocator);
}

Void String::_concat(Char c)
{
	NV_CORE_ASSERT(m_size + 1 > m_capacity);
	NV_CORE_ASSERT(m_allocator);
	if (m_allocator)
	{
		IndexT newCapacity = calcCapacity(m_capacity, m_size + 1);
		m_chars = (Char*)m_allocator->reallocate(m_chars, m_capacity, m_size, newCapacity);
		m_capacity = newCapacity;
		m_chars[m_size++] = c;
	}
}

Void String::_concat(const SubString& rhs)
{
	// Only called if out of space
	NV_CORE_ASSERT(m_size + rhs.m_size > m_capacity);
	IndexT newCapacity = calcCapacity(m_capacity, m_size + rhs.m_size);
	Char* dstChars = (Char*)m_allocator->reallocate(m_chars, m_capacity, m_size, newCapacity);

	const Char* srcChars = containsMemory(rhs) ? (rhs.m_chars - m_chars + dstChars) : rhs.m_chars;
	
	Memory::copy(dstChars + m_size, srcChars, rhs.m_size * sizeof(Char));
	m_chars = dstChars;
	m_capacity = newCapacity;
	m_size += rhs.m_size;
}

Char* String::_getCstr()
{
	NV_CORE_ASSERT(m_size >= m_capacity);
	NV_CORE_ASSERT(m_allocator);
	if (m_allocator)
	{	
		const IndexT newCapacity = m_capacity + 1;
		m_chars = (Char*)m_allocator->reallocate(m_chars, m_capacity, m_size, newCapacity);
		m_capacity = newCapacity;
		m_chars[m_size] = 0;
		return m_chars;
	}
	return NV_NULL;
}

Void String::set(const SubString& rhs)
{
	if (this == &rhs ||
		rhs.m_chars == m_chars && rhs.m_size == m_size)
	{
		return;
	}
	// If it contains this substring, just move it
	if (containsMemory(rhs))
	{
		Memory::move(m_chars, rhs.m_chars, rhs.m_size * sizeof(Char));
		m_size = rhs.m_size;
		return;
	}
	// Check there is space to store new string
	if (m_capacity < rhs.m_size)
	{
		if (!m_allocator)
		{
			// Copy what we can and assert
			NV_CORE_ASSERT(false);
			Memory::copy(m_chars, rhs.m_chars, m_capacity * sizeof(Char));
			m_size = m_capacity;
			return;
		}
		const IndexT newCapacity = calcInitialCapacity(rhs.m_size);
		// Make sure we have space
		m_chars = (Char*)m_allocator->reallocate(m_chars, m_capacity, 0, newCapacity);
		m_capacity = newCapacity;
	}
	Memory::copy(m_chars, rhs.m_chars, rhs.m_size * sizeof(Char));
	m_size = rhs.m_size;
}

Void String::set(const SubString& rhs, MemoryAllocator* newAlloc)
{
	if (newAlloc == m_allocator)
	{
		return set(rhs);	
	}
	String work(rhs, newAlloc);
	swap(work);
}

Char* String::requireSpace(IndexT space)
{
	NV_CORE_ASSERT(space >= 0);
	const IndexT minCapacity = m_size + space;
	if (minCapacity > m_capacity)
	{
		NV_CORE_ASSERT(m_allocator);
		if (m_allocator)
		{
			const IndexT newCapacity = calcCapacity(m_capacity, minCapacity);
			m_chars = (Char*)m_allocator->reallocate(m_chars, m_capacity, m_size, newCapacity);
			m_capacity = newCapacity;	
		}
	}
	return m_chars + m_size;
}

Char* String::insertSpace(IndexT pos, IndexT size)
{
	NV_CORE_ASSERT(size >= 0);
	NV_CORE_ASSERT(pos >= 0 && pos <= m_size);
	if (size > 0)
	{	
		requireSpace(size);
		if (pos < m_size)
		{
			Memory::move(m_chars + pos + size, m_chars + pos, sizeof(Char) * (m_size - pos));
		}
		m_size += size;
	}
	return m_chars + pos;
}

Void String::insert(IndexT pos, const SubString& in)
{
	const IndexT size = in.getSize();
	if (size <= 0)
	{
		return;
	}
	if (containsMemory(in))
	{
		return insert(pos, String(in));
	}
	Memory::copy(insertSpace(pos, size), in.begin(), sizeof(Char) * size); 
}

String& String::concatInt(Int v)
{
	const Int bufSize = 24;
	Char* dst = requireSpace(bufSize);
	Int size = sprintf_s(dst, bufSize, "%d", int(v));
	changeSize(size);
	return *this;
}

String& String::concatFloat32(Float32 f)
{
	const Int bufSize = 80;
	Char* dst = requireSpace(bufSize);
	Int size = sprintf_s(dst, bufSize, "%g", double(f));
	changeSize(size);
	return *this;
}


String& String::concatFloat64(Float64 f)
{
	const Int bufSize = 80;
	Char* dst = requireSpace(bufSize);
	Int size = sprintf_s(dst, bufSize, "%g", double(f));
	changeSize(size);
	return *this;
}

String& String::concatJoin(Char c, const SubString* subStrs, IndexT size)
{
	if (size <= 0)
	{
		return *this;
	}
	if (size == 1)
	{
		return concat(subStrs[0]);
	}

	IndexT totalSize = size - 1; // Size of all middle chars
	for (IndexT i = 0; i < size; i++) 
	{
		totalSize += subStrs[i].getSize();
	}

	Char* dst = requireSpace(totalSize);

	dst = subStrs[0].storeConcat(dst);
	for (IndexT i = 1; i < size; i++)
	{
		*dst++ = c;
		dst = subStrs[i].storeConcat(dst);
	}

	changeSize(totalSize);
	return *this;
}

} // namespace Common 
} // namespace nvidia

