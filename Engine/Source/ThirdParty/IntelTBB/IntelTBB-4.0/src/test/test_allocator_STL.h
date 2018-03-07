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

// Tests for compatibility with the host's STL.

#include "harness.h"

template<typename Container>
void TestSequence(const typename Container::allocator_type &a) {
    Container c(a);
    for( int i=0; i<1000; ++i )
        c.push_back(i*i);    
    typename Container::const_iterator p = c.begin();
    for( int i=0; i<1000; ++i ) {
        ASSERT( *p==i*i, NULL );
        ++p;
    }
    // regression test against compilation error for GCC 4.6.2
    c.resize(1000);
}

template<typename Set>
void TestSet(const typename Set::allocator_type &a) {
    Set s(typename Set::key_compare(), a);
    typedef typename Set::value_type value_type;
    for( int i=0; i<100; ++i ) 
        s.insert(value_type(3*i));
    for( int i=0; i<300; ++i ) {
        ASSERT( s.erase(i)==size_t(i%3==0), NULL );
    }
}

template<typename Map>
void TestMap(const typename Map::allocator_type &a) {
    Map m(typename Map::key_compare(), a);
    typedef typename Map::value_type value_type;
    for( int i=0; i<100; ++i ) 
        m.insert(value_type(i,i*i));
    for( int i=0; i<100; ++i )
        ASSERT( m.find(i)->second==i*i, NULL );
}

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    // Suppress "C++ exception handler used, but unwind semantics are not enabled" warning in STL headers
    #pragma warning (push)
    #pragma warning (disable: 4530)
#endif

#include <deque>
#include <list>
#include <map>
#include <set>
#include <vector>

#if !TBB_USE_EXCEPTIONS && _MSC_VER
    #pragma warning (pop)
#endif

template<typename Allocator>
void TestAllocatorWithSTL(const Allocator &a = Allocator() ) {
    typedef typename Allocator::template rebind<int>::other Ai;
    typedef typename Allocator::template rebind<const int>::other Aci;
    typedef typename Allocator::template rebind<std::pair<const int, int> >::other Acii;
    typedef typename Allocator::template rebind<std::pair<int, int> >::other Aii;

    // Sequenced containers
    TestSequence<std::deque <int,Ai> >(a);
    TestSequence<std::list  <int,Ai> >(a);
    TestSequence<std::vector<int,Ai> >(a);

    // Associative containers
    TestSet<std::set     <int, std::less<int>, Ai> >(a);
    TestSet<std::multiset<int, std::less<int>, Ai> >(a);
    TestMap<std::map     <int, int, std::less<int>, Acii> >(a);
    TestMap<std::multimap<int, int, std::less<int>, Acii> >(a);

#if _MSC_VER
    // Test compatibility with Microsoft's implementation of std::allocator for some cases that
    // are undefined according to the ISO standard but permitted by Microsoft.
    TestSequence<std::deque <const int,Aci> >(a);
#if _CPPLIB_VER>=500
    TestSequence<std::list  <const int,Aci> >(a);
#endif
    TestSequence<std::vector<const int,Aci> >(a);
    TestSet<std::set<const int, std::less<int>, Aci> >(a);
    TestMap<std::map<int, int, std::less<int>, Aii> >(a);
    TestMap<std::map<const int, int, std::less<int>, Acii> >(a);
    TestMap<std::multimap<int, int, std::less<int>, Aii> >(a);
    TestMap<std::multimap<const int, int, std::less<int>, Acii> >(a);
#endif /* _MSC_VER */
}
