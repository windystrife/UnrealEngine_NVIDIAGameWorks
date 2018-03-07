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

// tbb::tuple

#include "harness.h"
// this test should match that in graph.h, so we test whatever tuple is
// being used by the join_node.
#if !__SUNPRO_CC
#if __TBB_CPP11_TUPLE_PRESENT
#define __TESTING_STD_TUPLE__ 1
#include <tuple>
#else
#define __TESTING_STD_TUPLE__ 0
#include "tbb/compat/tuple"
#endif /*!__TBB_CPP11_TUPLE_PRESENT*/
#include <string>
#include <iostream>

using namespace std;

class non_trivial {
public:
    non_trivial() {}
    ~non_trivial() {}
    non_trivial(const non_trivial& other) : my_int(other.my_int), my_float(other.my_float) { }
    int get_int() const { return my_int; }
    float get_float() const { return my_float; }
    void set_int(int newval) { my_int = newval; }
    void set_float(float newval) { my_float = newval; }
private:
    int my_int;
    float my_float;
};

void RunTests() {

#if __TESTING_STD_TUPLE__
    REMARK("Testing platform tuple\n");
#else
    REMARK("Testing compat/tuple\n");
#endif
    tuple<int> ituple1(3);
    tuple<int> ituple2(5);
    tuple<double> ftuple2(4.1);

    ASSERT(!(ituple1 == ituple2), NULL);
    ASSERT(ituple1 != ituple2, NULL);
    ASSERT(!(ituple1 > ituple2), NULL);
    ASSERT(ituple1 < ituple2, NULL);
    ASSERT(ituple1 <= ituple2, NULL);
    ASSERT(!(ituple1 >= ituple2), NULL);
    ASSERT(ituple1 < ftuple2, NULL);

    typedef tuple<int,double,float> tuple_type1;
    typedef tuple<int,int,int> int_tuple_type;
    typedef tuple<int,non_trivial,int> non_trivial_tuple_type;
    typedef tuple<double,std::string,char> stringy_tuple_type;
    const tuple_type1 tup1(42,3.14159,2.0f);
    int_tuple_type int_tup(4, 5, 6);
    non_trivial_tuple_type nti;
    stringy_tuple_type stv;
    get<1>(stv) = "hello";
    get<2>(stv) = 'x';

    ASSERT(get<0>(stv) == 0.0, NULL);
    ASSERT(get<1>(stv) == "hello", NULL);
    ASSERT(get<2>(stv) == 'x', NULL);

    ASSERT(tuple_size<tuple_type1>::value == 3, NULL);
    ASSERT(get<0>(tup1) == 42, NULL);
    ASSERT(get<1>(tup1) == 3.14159, NULL);
    ASSERT(get<2>(tup1) == 2.0, NULL);

    get<1>(nti).set_float(1.0);
    get<1>(nti).set_int(32);
    ASSERT(get<1>(nti).get_int() == 32, NULL);
    ASSERT(get<1>(nti).get_float() == 1.0, NULL);

    // converting constructor
    tuple<double,double,double> tup2(1,2.0,3.0f);
    tuple<double,double,double> tup3(9,4.0,7.0f);
    ASSERT(tup2 != tup3, NULL);

    ASSERT(tup2 < tup3, NULL);

    // assignment
    tup2 = tup3;
    ASSERT(tup2 == tup3, NULL);

    tup2 = int_tup;
    ASSERT(get<0>(tup2) == 4, NULL);
    ASSERT(get<1>(tup2) == 5, NULL);
    ASSERT(get<2>(tup2) == 6, NULL);

    // increment component of tuple
    get<0>(tup2) += 1;
    ASSERT(get<0>(tup2) == 5, NULL);

    std::pair<int,int> two_pair( 4, 8);
    tuple<int,int> two_pair_tuple;
    two_pair_tuple = two_pair;
    ASSERT(get<0>(two_pair_tuple) == 4, NULL);
    ASSERT(get<1>(two_pair_tuple) == 8, NULL);

    //relational ops
    ASSERT(int_tuple_type(1,1,0) == int_tuple_type(1,1,0),NULL);
    ASSERT(int_tuple_type(1,0,1) <  int_tuple_type(1,1,1),NULL);
    ASSERT(int_tuple_type(1,0,0) >  int_tuple_type(0,1,0),NULL);
    ASSERT(int_tuple_type(0,0,0) != int_tuple_type(1,0,1),NULL);
    ASSERT(int_tuple_type(0,1,0) <= int_tuple_type(0,1,1),NULL);
    ASSERT(int_tuple_type(0,0,1) <= int_tuple_type(0,0,1),NULL);
    ASSERT(int_tuple_type(1,1,1) >= int_tuple_type(1,0,0),NULL);
    ASSERT(int_tuple_type(0,1,1) >= int_tuple_type(0,1,1),NULL);
    typedef tuple<int,float,double,char> mixed_tuple_left;
    typedef tuple<float,int,char,double> mixed_tuple_right;
    ASSERT(mixed_tuple_left(1,1.f,1,1) == mixed_tuple_right(1.f,1,1,1),NULL);
    ASSERT(mixed_tuple_left(1,0.f,1,1) <  mixed_tuple_right(1.f,1,1,1),NULL);
    ASSERT(mixed_tuple_left(1,1.f,1,1) >  mixed_tuple_right(1.f,1,0,1),NULL);
    ASSERT(mixed_tuple_left(1,1.f,1,0) != mixed_tuple_right(1.f,1,1,1),NULL);
    ASSERT(mixed_tuple_left(1,0.f,1,1) <= mixed_tuple_right(1.f,1,0,1),NULL);
    ASSERT(mixed_tuple_left(1,0.f,0,1) <= mixed_tuple_right(1.f,0,0,1),NULL);
    ASSERT(mixed_tuple_left(1,1.f,1,0) >= mixed_tuple_right(1.f,0,1,1),NULL);
    ASSERT(mixed_tuple_left(0,1.f,1,0) >= mixed_tuple_right(0.f,1,1,0),NULL);

}

int TestMain() {
    RunTests();
    return Harness::Done;
}
#else  // __SUNPRO_CC

int TestMain() {
    return Harness::Skipped;
}

#endif  // __SUNPRO_CC
