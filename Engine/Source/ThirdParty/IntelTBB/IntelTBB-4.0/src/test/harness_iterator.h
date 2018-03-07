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

#ifndef harness_iterator_H
#define harness_iterator_H

#include <iterator>
#include <memory>

namespace Harness {

template <class T>
class InputIterator {
    T * my_ptr;
public:
#if HARNESS_EXTENDED_STD_COMPLIANCE
    typedef std::input_iterator_tag iterator_category;
    typedef T value_type;
    typedef typename std::allocator<T>::difference_type difference_type;
    typedef typename std::allocator<T>::pointer pointer;
    typedef typename std::allocator<T>::reference reference;
#endif /* HARNESS_EXTENDED_STD_COMPLIANCE */
   
    explicit InputIterator( T * ptr): my_ptr(ptr){}
    
    T& operator* () { return *my_ptr; }
    
    InputIterator& operator++ () { ++my_ptr; return *this; }

    bool operator== ( const InputIterator& r ) { return my_ptr == r.my_ptr; }
};

template <class T>
class ForwardIterator {
    T * my_ptr;
public:
#if HARNESS_EXTENDED_STD_COMPLIANCE
    typedef std::forward_iterator_tag iterator_category;
    typedef T value_type;
    typedef typename std::allocator<T>::difference_type difference_type;
    typedef typename std::allocator<T>::pointer pointer;
    typedef typename std::allocator<T>::reference reference;
#endif /* HARNESS_EXTENDED_STD_COMPLIANCE */
   
    explicit ForwardIterator ( T * ptr ) : my_ptr(ptr){}
    
    ForwardIterator ( const ForwardIterator& r ) : my_ptr(r.my_ptr){}
    
    T& operator* () { return *my_ptr; }
    
    ForwardIterator& operator++ () { ++my_ptr; return *this; }

    bool operator== ( const ForwardIterator& r ) { return my_ptr == r.my_ptr; }
};

template <class T>
class RandomIterator {
    T * my_ptr;
#if !HARNESS_EXTENDED_STD_COMPLIANCE
    typedef typename std::allocator<T>::difference_type difference_type;
#endif

public:
#if HARNESS_EXTENDED_STD_COMPLIANCE
    typedef std::random_access_iterator_tag iterator_category;
    typedef T value_type;
    typedef typename std::allocator<T>::pointer pointer;
    typedef typename std::allocator<T>::reference reference;
    typedef typename std::allocator<T>::difference_type difference_type;
#endif /* HARNESS_EXTENDED_STD_COMPLIANCE */

    explicit RandomIterator ( T * ptr ) : my_ptr(ptr){}
    RandomIterator ( const RandomIterator& r ) : my_ptr(r.my_ptr){}
    T& operator* () { return *my_ptr; }
    RandomIterator& operator++ () { ++my_ptr; return *this; }
    bool operator== ( const RandomIterator& r ) { return my_ptr == r.my_ptr; }
    difference_type operator- (const RandomIterator &r) {return my_ptr - r.my_ptr;}
    RandomIterator operator+ (difference_type n) {return RandomIterator(my_ptr + n);}
};

template <class T>
class ConstRandomIterator {
    const T * my_ptr;
#if !HARNESS_EXTENDED_STD_COMPLIANCE
    typedef typename std::allocator<T>::difference_type difference_type;
#endif

public:
#if HARNESS_EXTENDED_STD_COMPLIANCE
    typedef std::random_access_iterator_tag iterator_category;
    typedef T value_type;
    typedef typename std::allocator<T>::const_pointer pointer;
    typedef typename std::allocator<T>::const_reference reference;
    typedef typename std::allocator<T>::difference_type difference_type;
#endif /* HARNESS_EXTENDED_STD_COMPLIANCE */

    explicit ConstRandomIterator ( const T * ptr ) : my_ptr(ptr){}
    ConstRandomIterator ( const ConstRandomIterator& r ) : my_ptr(r.my_ptr){}
    const T& operator* () { return *my_ptr; }
    ConstRandomIterator& operator++ () { ++my_ptr; return *this; }
    bool operator== ( const ConstRandomIterator& r ) { return my_ptr == r.my_ptr; }
    difference_type operator- (const ConstRandomIterator &r) {return my_ptr - r.my_ptr;}
    ConstRandomIterator operator+ (difference_type n) {return ConstRandomIterator(my_ptr + n);}
};

} // namespace Harness

#if !HARNESS_EXTENDED_STD_COMPLIANCE
namespace std {
    template<typename T>
    struct iterator_traits< Harness::InputIterator<T> > {
        typedef std::input_iterator_tag iterator_category;
        typedef T value_type;
        typedef value_type& reference;
    };

    template<typename T>
    struct iterator_traits< Harness::ForwardIterator<T> > {
        typedef std::forward_iterator_tag iterator_category;
        typedef T value_type;
        typedef value_type& reference;
    };

    template<typename T>
    struct iterator_traits< Harness::RandomIterator<T> > {
        typedef std::random_access_iterator_tag iterator_category;
        typedef T value_type;
        typedef value_type& reference;
    };

    template<typename T>
    struct iterator_traits< Harness::ConstRandomIterator<T> > {
        typedef std::random_access_iterator_tag iterator_category;
        typedef T value_type;
        typedef const value_type& reference;
    };
} // namespace std
#endif /* !HARNESS_EXTENDED_STD_COMPLIANCE */

#endif //harness_iterator_H
