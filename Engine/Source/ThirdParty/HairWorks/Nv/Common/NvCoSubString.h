/* Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited. */

#ifndef NV_CO_SUB_STRING_H
#define NV_CO_SUB_STRING_H

#include <Nv/Common/NvCoCommon.h>
#include <Nv/Common/NvCoTypeMacros.h>
#include <Nv/Common/NvCoMemory.h>
#include <Nv/Common/Container/NvCoArray.H>

/** \addtogroup common
@{
*/

namespace nvidia {
namespace Common {

/*! A subpart of a string. In and of itself it has no memory management.
String is encoded in utf8. Note that a SubString is not designed to be zero terminated, allowing for substring to be taken without 
memory management */
class SubString
{
	friend class String;
	NV_CO_DECLARE_CLASS_BASE(SubString);
	
		/// Type to allow initializing from a C style string (ie zero terminated). 
	enum InitCstr { INIT_CSTR };
	enum InitSpan { INIT_SPAN };

		/// True if the memory of rhs is inside of this
	NV_FORCE_INLINE Bool containsMemory(const SubString& rhs) const { return rhs.m_chars >= m_chars && rhs.m_chars + rhs.m_size <= m_chars + m_size; } 

		/// For iterating over chars
	NV_FORCE_INLINE const Char* begin() const { return m_chars; }
		/// For iterating over chars
	NV_FORCE_INLINE const Char* end() const { return m_chars + m_size; }

		/// Get the size
	NV_FORCE_INLINE IndexT getSize() const { return m_size; }

		/// Returns first occurrence of character c, or -1 if not found
	IndexT indexOf(Char c) const;
	IndexT indexOf(Char c, IndexT from) const;

		/// Looks for first instance of c starting from the end. If not found returns -1.
	IndexT reverseIndexOf(Char c) const;

		/// Get a substring, from start up until (but not including end). Negative indices wrap around
	SubString subStringWithEnd(IndexT start, IndexT end) const;

		/// Get a substring from start, including size characters. Start can be negative and if so wraps around.
	SubString subStringWithStart(IndexT start, IndexT subSize) const;
		/// Takes the head number of characters, or size whichever is less. Can use negative numbers to wrap around.
	SubString head(IndexT end) const;
		/// Takes the characters from start until the end. Can use negative numbers to wrap around.
	SubString tail(IndexT start) const;

		// Get's the last character of the string, or 0 if it's empty
	Char getLast() const { return m_size > 0 ? m_chars[m_size - 1] : 0; }

		/// Clear the contents
	Void clear() { m_size = 0; }

		/// Substrings contain utf8. Returns true if the contents is all ascii encoded.
	Bool isAscii() const; 

		/// True if exactly equal
	Bool equals(const ThisType& rhs) const;
		/// True if equal case insensitive
	Bool equalsI(const ThisType& rhs) const;

		/// True if equals a c string (ie zero terminated)
	Bool equalsCstr(const Char* rhs) const;

		/// Split into multiple substrings using char to separate
	Void split(Char c, Array<SubString>& out) const;

		/// Store the contents as c string. If not enough space, will just truncate. Adds terminating zero.
	Char* storeCstr(Char* out, SizeT size) const;
	template <SizeT SIZE>
	NV_FORCE_INLINE Char* storeCstr(Char(&in)[SIZE]) const { return storeCstr(in, SIZE); }

		/// Stores contents (without terminating 0). Returns pointer to end of what was copied (ie the right location
		/// to concat another element)
	Char* storeConcat(Char* out) const;

		/// Converts to int. If input isn't a valid int, result is undefined.
	Int toInt() const;
		/// Convert to int, base can be 0 to 36 
	Int toInt(Int base) const;

		/// ==
	NV_FORCE_INLINE Bool operator==(const ThisType& rhs) const { return equals(rhs); }
		/// !=
	NV_FORCE_INLINE Bool operator!=(const ThisType& rhs) const { return !equals(rhs); }

		/// []
	NV_FORCE_INLINE const Char& operator[](IndexT index) const { NV_CORE_ASSERT(index >=0 && index < m_size); return m_chars[index]; }

		/// Construct from a zero terminated C style string
	SubString(InitCstr, const Char* in);
		/// Ctor with size
	NV_FORCE_INLINE SubString(const Char* chars, SizeT size):m_chars(const_cast<Char*>(chars)), m_size(IndexT(size)) {}
		/// String literal constructor 
	template <SizeT SIZE>
	NV_FORCE_INLINE SubString(const Char (&in)[SIZE]):m_chars(const_cast<Char*>(in)), m_size(IndexT(SIZE - 1)) {}
		/// Default Ctor
	SubString() :m_chars(NV_NULL), m_size(0) {}
		/// Set with start and end 'span'
	SubString(InitSpan, const Char* start, const Char* end):m_chars(const_cast<Char*>(start)), m_size(IndexT(end - start)) { NV_CORE_ASSERT(end >= start); }

		/// Get the empty substring
	static const SubString& getEmpty() { return s_empty; }

	protected:
	Char* m_chars;	///< Start of the string (NOTE! not const, because String derives from it)
	IndexT m_size;	///< Size

	private:

	static const SubString s_empty; 

	// Disabled! So not eroneously initialized with say a Char buffer[80]; type declaration 
	// If a buffer is to be used to store the characters and is zero terminated use SubString sub(SubString::INIT_CSTR, buffer);
	// If the length is known, just use SubString sub(buffer, size);
	template <SizeT SIZE> 
	NV_FORCE_INLINE SubString(Char* in);
};

} // namespace Common 
} // namespace nvidia

/** @} */

#endif // NV_CO_SUB_STRING_H