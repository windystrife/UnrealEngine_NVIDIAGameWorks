/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_STRING_H
#define NV_CO_STRING_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoMemory.h>
#include <Nv/Common/Container/NvCoArray.H>

#include <Nv/Common/NvCoSubString.h>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! \brief String is a simple utf8 based string class. 

\details Normally characters are stored non zero terminated. Use getZeroTerminated to get a zero terminated string, for use in typical C style string functions.
The type derives from SubString, such that can be easily used as a SubString that is mutable and manages the characters storage. */
class String: public SubString
{
	NV_CO_DECLARE_CLASS(String, SubString);

		/// Immutable iteration
	NV_FORCE_INLINE const Char* begin() const { return m_chars; }
	NV_FORCE_INLINE const Char* end() const { return m_chars + m_size; }

		/// Mutable iteration 
	NV_FORCE_INLINE Char* begin() { return m_chars; }
	NV_FORCE_INLINE Char* end() { return m_chars + m_size; } 

		/// ==
		/// NOTE Equality is only on contents not on the allocator used
	NV_FORCE_INLINE Bool operator==(const ThisType& rhs) const { return equals(rhs); } 
		/// !=
		/// NOTE Inequality is only on contents not on the allocator used
	NV_FORCE_INLINE Bool operator!=(const ThisType& rhs) const { return !equals(rhs); }

		/// Concat a substring
	NV_FORCE_INLINE ThisType& concat(const SubString& rhs);
		/// Concat a char
	NV_FORCE_INLINE ThisType& concat(Char c);
		/// Concat the value v as text
	ThisType& concatInt(Int v);
	ThisType& concatFloat32(Float32 f);
	ThisType& concatFloat64(Float64 f);
		/// Concat chars
	ThisType& concat(const Char* in, SizeT size) { return concat(SubString(in, IndexT(size))); }

		/// Concat with the input substrings joined with c
	ThisType& concatJoin(Char c, const SubString* subStrs, IndexT size);

		/// Reduce the size 
	NV_FORCE_INLINE Void reduceSize(IndexT size);
		/// Change the size by the specified delta. 
	NV_FORCE_INLINE Void changeSize(IndexT delta);
		/// Set to a specific size (must be in range 0 - capacity)
	NV_FORCE_INLINE Void setSize(IndexT size);

		/// Makes sure there is space at the end of the size specified
	Char* requireSpace(IndexT space);
	
		/// Inserts space at pos of size 'size'. The new space is NOT initialized.
	Char* insertSpace(IndexT pos, IndexT size);
		/// Insert some text
	Void insert(IndexT pos, const SubString& in);

		/// Get as a C style string (ie zero terminated)
	NV_FORCE_INLINE Char* getCstr();
	const Char* getCstr() const { return const_cast<ThisType*>(this)->getCstr(); } 

		/// Swap
	Void swap(ThisType& rhs);

		/// Set contents
	Void set(const SubString& rhs);
		/// Set using the specific alloc
	Void set(const SubString& rhs, MemoryAllocator* alloc);

		/// Get the set memory allocator
	NV_FORCE_INLINE MemoryAllocator* getAllocator() const { return m_allocator; }
		/// The total capacity for storing chars. 
	NV_FORCE_INLINE IndexT getCapacity() const { return m_capacity; }

	
		/// []
	NV_FORCE_INLINE const Char& operator[](IndexT index) const { NV_CORE_ASSERT(index >= 0 && index < m_size); return m_chars[index]; }
	NV_FORCE_INLINE Char& operator[](IndexT index) { NV_CORE_ASSERT(index >= 0 && index < m_size); return m_chars[index]; }

		/// Assign
	ThisType& operator=(const SubString& rhs) { set(rhs); return *this; }
	ThisType& operator=(const ThisType& rhs) { set(rhs); return *this; }

		/// Ctor
	NV_FORCE_INLINE String(const SubString& rhs) { _ctor(rhs); }
	NV_FORCE_INLINE String(const SubString& rhs, MemoryAllocator* alloc) { _ctor(rhs, alloc); }

	NV_FORCE_INLINE String(const ThisType& rhs) { _ctor(rhs); }
	NV_FORCE_INLINE String(const ThisType& rhs, MemoryAllocator* alloc) { _ctor(rhs, alloc); }

	NV_FORCE_INLINE String(Char* chars, IndexT size, IndexT capacity, MemoryAllocator* alloc = NV_NULL);
		/// Default Ctor
	NV_FORCE_INLINE String():Parent(NV_NULL, 0), m_capacity(0), m_allocator(MemoryAllocator::getInstance()) {}

		/// Dtor
	~String();

#ifdef NV_HAS_MOVE_SEMANTICS
		/// Move Ctor
	NV_FORCE_INLINE String(ThisType&& rhs):Parent(rhs), m_capacity(rhs.m_capacity), m_allocator(rhs.m_allocator) { rhs.m_allocator = NV_NULL; }
		/// Move assign
	NV_FORCE_INLINE String& operator=(ThisType&& rhs) { swap(rhs); return *this; }
#endif

		/// Get the empty string
	static const String& getEmpty() { return s_empty; }
		/// Works out the appropriate initial capacity
	static IndexT calcInitialCapacity(IndexT size);
	static IndexT calcCapacity(IndexT oldCapacity, IndexT newCapacity);

	protected:

	Void _ctor(const SubString& rhs);
	Void _ctor(const SubString& rhs, MemoryAllocator* alloc);

	Void _concat(Char c);
	Void _concat(const SubString& rhs);
	Char* _getCstr();

	IndexT m_capacity;					///< Total capacity
	MemoryAllocator* m_allocator;		///< Allocator that backs memory (if NV_NULL memory is not freed)

	private:
	static const String s_empty;
};

// ---------------------------------------------------------------------------
NV_FORCE_INLINE String& String::concat(Char c)
{
	if (m_size < m_capacity)
	{
		m_chars[m_size++] = c;
	}
	else
	{
		_concat(c);
	}
	return *this;
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE String& String::concat(const SubString& rhs)
{
	if (m_size + rhs.m_size <= m_capacity)
	{
		if (rhs.m_size > 0)
		{
			Memory::copy(m_chars + m_size, rhs.m_chars, rhs.m_size * sizeof(Char));
		}
		m_size += rhs.m_size;
	}
	else
	{
		_concat(rhs);
	}
	return *this; 
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE Char* String::getCstr() 
{
	if (m_size < m_capacity)
	{
		m_chars[m_size] = 0;
		return m_chars;
	}
	else
	{
		return _getCstr();
	}
}
// ---------------------------------------------------------------------------
NV_FORCE_INLINE Void String::reduceSize(IndexT size)
{
	NV_CORE_ASSERT(size >= 0 && size <= m_size);
	m_size = size;
}
// ---------------------------------------------------------------------------
Void String::changeSize(IndexT delta)
{
	IndexT newSize = m_size + delta;
	NV_CORE_ASSERT(newSize >= 0 && newSize <= m_capacity);
	m_size = newSize;
}
// ---------------------------------------------------------------------------
Void String::setSize(IndexT size)
{
	NV_CORE_ASSERT(size >= 0 && size <= m_capacity);
	m_size = size;
}
// ---------------------------------------------------------------------------
String::String(Char* chars, IndexT size, IndexT capacity, MemoryAllocator* alloc):
	Parent(chars, size),
	m_capacity(capacity),
	m_allocator(alloc)
{
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! String << !!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

NV_INLINE String& operator<<(String& string, const SubString& rhs)
{
	return string.concat(rhs);
}

NV_INLINE String& operator<<(String& string, Int v)
{
	return string.concatInt(v);
}

NV_INLINE String& operator<<(String& string, Float32 v)
{
	return string.concatFloat32(v);
}

NV_INLINE String& operator<<(String& string, Float64 v)
{
	return string.concatFloat64(v);
}

NV_INLINE String& operator<<(String& string, Char c)
{
	return string.concat(c);
}

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_STRING_H