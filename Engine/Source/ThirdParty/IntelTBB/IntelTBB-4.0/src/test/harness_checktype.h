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

// type that checks construction and destruction.

static tbb::atomic<int> check_type_counter;

template<class Counter>
class check_type : Harness::NoAfterlife {
    Counter id;
    bool am_ready;
public:

    check_type(int _n = 0) : id(_n), am_ready(false) {
        ++check_type_counter;
    }

    check_type(const check_type& other) : Harness::NoAfterlife(other) {
        other.AssertLive();
        AssertLive();
        id = other.id;
        am_ready = other.am_ready;
        ++check_type_counter;
    }

    operator int() const { return (int)my_id(); }

    ~check_type() { 
        AssertLive(); 
        --check_type_counter;
        ASSERT(check_type_counter >= 0, "too many destructions");
    }

    check_type &operator=(const check_type &other) {
        other.AssertLive();
        AssertLive();
        id = other.id;
        am_ready = other.am_ready;
        return *this;
    }

    unsigned int my_id() const { AssertLive(); return id; }
    bool is_ready() { AssertLive(); return am_ready; }
    void function() {
        AssertLive();
        if( id == 0 ) {
            id = 1;
            am_ready = true;
        }
    }

};

// provide a default check method that just returns true.
template<class MyClass>
struct Check {
    static bool check_destructions() {return true; }
};

template<class Counttype>
class Check<check_type< Counttype > > {
    static bool check_destructions () { 
            return check_type_counter == 0;
        }
};

