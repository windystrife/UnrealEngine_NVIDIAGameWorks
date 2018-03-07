/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

#ifndef __TBB_cache_aligned_allocator_H
#define __TBB_cache_aligned_allocator_H

#include <new>
#include "tbb_stddef.h"
#if __TBB_CPP11_RVALUE_REF_PRESENT && !__TBB_CPP11_STD_FORWARD_BROKEN
 #include <utility> // std::forward
#endif

namespace tbb {

//! @cond INTERNAL
namespace internal {
    //! Cache/sector line size.
    /** @ingroup memory_allocation */
    size_t __TBB_EXPORTED_FUNC NFS_GetLineSize();

    //! Allocate memory on cache/sector line boundary.
    /** @ingroup memory_allocation */
    void* __TBB_EXPORTED_FUNC NFS_Allocate( size_t n_element, size_t element_size, void* hint );

    //! Free memory allocated by NFS_Allocate.
    /** Freeing a NULL pointer is allowed, but has no effect.
        @ingroup memory_allocation */
    void __TBB_EXPORTED_FUNC NFS_Free( void* );
}
//! @endcond

#if _MSC_VER && !defined(__INTEL_COMPILER)
    // Workaround for erroneous "unreferenced parameter" warning in method destroy.
    #pragma warning (push)
    #pragma warning (disable: 4100)
#endif

//! Meets "allocator" requirements of ISO C++ Standard, Section 20.1.5
/** The members are ordered the same way they are in section 20.4.1
    of the ISO C++ standard.
    @ingroup memory_allocation */
template<typename T>
class cache_aligned_allocator {
public:
    typedef typename internal::allocator_type<T>::value_type value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    template<typename U> struct rebind {
        typedef cache_aligned_allocator<U> other;
    };

    cache_aligned_allocator() throw() {}
    cache_aligned_allocator( const cache_aligned_allocator& ) throw() {}
    template<typename U> cache_aligned_allocator(const cache_aligned_allocator<U>&) throw() {}

    pointer address(reference x) const {return &x;}
    const_pointer address(const_reference x) const {return &x;}
    
    //! Allocate space for n objects, starting on a cache/sector line.
    pointer allocate( size_type n, const void* hint=0 ) {
        // The "hint" argument is always ignored in NFS_Allocate thus const_cast shouldn't hurt
        return pointer(internal::NFS_Allocate( n, sizeof(value_type), const_cast<void*>(hint) ));
    }

    //! Free block of memory that starts on a cache line
    void deallocate( pointer p, size_type ) {
        internal::NFS_Free(p);
    }

    //! Largest value for which method allocate might succeed.
    size_type max_size() const throw() {
        return (~size_t(0)-internal::NFS_MaxLineSize)/sizeof(value_type);
    }

    //! Copy-construct value at location pointed to by p.
#if __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT && __TBB_CPP11_RVALUE_REF_PRESENT
    template<typename... Args>
    void construct(pointer p, Args&&... args)
 #if __TBB_CPP11_STD_FORWARD_BROKEN
        { ::new((void *)p) T((args)...); }
 #else
        { ::new((void *)p) T(std::forward<Args>(args)...); }
 #endif
#else // __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT && __TBB_CPP11_RVALUE_REF_PRESENT
    void construct( pointer p, const value_type& value ) {::new((void*)(p)) value_type(value);}
#endif // __TBB_CPP11_VARIADIC_TEMPLATES_PRESENT && __TBB_CPP11_RVALUE_REF_PRESENT

    //! Destroy value at location pointed to by p.
    void destroy( pointer p ) {p->~value_type();}
};

#if _MSC_VER && !defined(__INTEL_COMPILER)
    #pragma warning (pop)
#endif // warning 4100 is back

//! Analogous to std::allocator<void>, as defined in ISO C++ Standard, Section 20.4.1
/** @ingroup memory_allocation */
template<> 
class cache_aligned_allocator<void> {
public:
    typedef void* pointer;
    typedef const void* const_pointer;
    typedef void value_type;
    template<typename U> struct rebind {
        typedef cache_aligned_allocator<U> other;
    };
};

template<typename T, typename U>
inline bool operator==( const cache_aligned_allocator<T>&, const cache_aligned_allocator<U>& ) {return true;}

template<typename T, typename U>
inline bool operator!=( const cache_aligned_allocator<T>&, const cache_aligned_allocator<U>& ) {return false;}

} // namespace tbb

#endif /* __TBB_cache_aligned_allocator_H */
