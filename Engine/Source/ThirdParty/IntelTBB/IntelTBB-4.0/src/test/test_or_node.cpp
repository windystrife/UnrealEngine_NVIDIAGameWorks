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

#include "harness.h"
#define TBB_PREVIEW_GRAPH_NODES 1
#include "tbb/flow_graph.h"

#if !__SUNPRO_CC
//
// Tests
//

const int Count = 150;
const int MaxPorts = 10;
const int MaxNSources = 5; // max # of source_nodes to register for each or_node input in parallel test
bool outputCheck[MaxPorts][Count];  // for checking output

void
check_outputCheck( int nUsed, int maxCnt) {
    for(int i=0; i < nUsed; ++i) {
        for( int j = 0; j < maxCnt; ++j) {
            ASSERT(outputCheck[i][j], NULL);
        }
    }
}

void
reset_outputCheck( int nUsed, int maxCnt) {
    for(int i=0; i < nUsed; ++i) {
        for( int j = 0; j < maxCnt; ++j) {
            outputCheck[i][j] = false;
        }
    }
}

class test_class {
    public:
        test_class() { my_val = 0; }
        test_class(int i) { my_val = i; }
        operator int() { return my_val; }
    private:
        int my_val;
};

template<typename T>
class name_of {
public:
    static const char* name() { return  "Unknown"; }
};
template<>
class name_of<int> {
public:
    static const char* name() { return  "int"; }
};
template<>
class name_of<float> {
public:
    static const char* name() { return  "float"; }
};
template<>
class name_of<double> {
public:
    static const char* name() { return  "double"; }
};
template<>
class name_of<long> {
public:
    static const char* name() { return  "long"; }
};
template<>
class name_of<short> {
public:
    static const char* name() { return  "short"; }
};
template<>
class name_of<test_class> {
public:
    static const char* name() { return  "test_class"; }
};

// TT must be arithmetic, and shouldn't wrap around for reasonable sizes of Count (which is now 150, and maxPorts is 10,
// so the max number generated right now is 1500 or so.)  Source will generate a series of TT with value
// (init_val + (i-1)*addend) * my_mult, where i is the i-th invocation of the body.  We are attaching addend
// source nodes to a or_port, and each will generate part of the numerical series the port is expecting
// to receive.  If there is only one source node, the series order will be maintained; if more than one,
// this is not guaranteed.
template<typename TT>
class source_body {
    const TT my_mult;
    int my_count;
    const int addend;
    source_body& operator=( const source_body& other);
public:
    source_body(TT multiplier, int init_val, int addto) : my_mult(multiplier), my_count(init_val), addend(addto) { }
    bool operator()( TT &v) {
        int lc = my_count;
        v = my_mult * (TT)my_count;
        my_count += addend;
        return lc < Count;
    }
};

// allocator for or_node.

template<typename OType>
class makeOr {
public:
    static OType *create() {
        OType *temp = new OType();
        return temp;
    }
    static void destroy(OType *p) { delete p; }
};

template<int ELEM, typename ONT>
struct getval_helper {

    typedef typename ONT::output_type OT;
    static int get_integer_val(OT &o) {
        return int(std::get<ELEM-1>(o.result));
    }
};

// holder for source_node pointers for eventual deletion

static void* all_source_nodes[MaxPorts][MaxNSources];

template<int ELEM, typename ONT>
class source_node_helper {
public:
    typedef ONT or_node_type;
    typedef typename or_node_type::output_type TT;
    typedef typename std::tuple_element<ELEM-1,typename ONT::tuple_types>::type IT;
    typedef typename tbb::flow::source_node<IT> my_source_node_type;
    static void print_remark() {
        source_node_helper<ELEM-1,ONT>::print_remark();
        REMARK(", %s", name_of<IT>::name());
    }
    static void add_source_nodes(or_node_type &my_or, tbb::flow::graph &g, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *new_node = new my_source_node_type(g, source_body<IT>((IT)(ELEM+1), i, nInputs));
            ASSERT(new_node->register_successor(tbb::flow::input_port<ELEM-1>(my_or)), NULL);
            all_source_nodes[ELEM-1][i] = (void *)new_node;
        }
        // add the next source_node
        source_node_helper<ELEM-1, ONT>::add_source_nodes(my_or, g, nInputs);
    }
    static void check_value(TT &v) {
        if(v.indx == ELEM-1) {
            int ival = getval_helper<ELEM,ONT>::get_integer_val(v);
            ASSERT(!(ival%(ELEM+1)), NULL);
            ival /= (ELEM+1);
            ASSERT(!outputCheck[ELEM-1][ival], NULL);
            outputCheck[ELEM-1][ival] = true;
        }
        else {
            source_node_helper<ELEM-1,ONT>::check_value(v);
        }
    }

    static void remove_source_nodes(or_node_type& my_or, int nInputs) {
        for(int i=0; i< nInputs; ++i) {
            my_source_node_type *dp = reinterpret_cast<my_source_node_type *>(all_source_nodes[ELEM-1][i]);
            dp->remove_successor(tbb::flow::input_port<ELEM-1>(my_or));
            delete dp;
        }
        source_node_helper<ELEM-1, ONT>::remove_source_nodes(my_or, nInputs);
    }
};

template<typename ONT>
class source_node_helper<1, ONT> {
    typedef ONT or_node_type;
    typedef typename or_node_type::output_type TT;
    typedef typename std::tuple_element<0, typename ONT::tuple_types>::type IT;
    typedef typename tbb::flow::source_node<IT> my_source_node_type;
public:
    static void print_remark() {
        REMARK("Parallel test of or_node< %s", name_of<IT>::name());
    }
    static void add_source_nodes(or_node_type &my_or, tbb::flow::graph &g, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *new_node = new my_source_node_type(g, source_body<IT>((IT)2, i, nInputs));
            ASSERT(new_node->register_successor(tbb::flow::input_port<0>(my_or)), NULL);
            all_source_nodes[0][i] = (void *)new_node;
        }
    }
    static void check_value(TT &v) {
        int ival = getval_helper<1,ONT>::get_integer_val(v);
        ASSERT(!(ival%2), NULL);
        ival /= 2;
        ASSERT(!outputCheck[0][ival], NULL);
        outputCheck[0][ival] = true;
    }
    static void remove_source_nodes(or_node_type& my_or, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *dp = reinterpret_cast<my_source_node_type *>(all_source_nodes[0][i]);
            dp->remove_successor(tbb::flow::input_port<0>(my_or));
            delete dp;
        }
    }
};

template<typename OType>
class parallel_test {
public:
    typedef typename OType::output_type TType;
    typedef typename OType::tuple_types union_types;
    static const int SIZE = std::tuple_size<union_types>::value;
    static void test() {
        TType v;
        source_node_helper<SIZE,OType>::print_remark();
        REMARK(" >\n");
        for(int i=0; i < MaxPorts; ++i) {
            for(int j=0; j < MaxNSources; ++j) {
                all_source_nodes[i][j] = NULL;
            }
        }
        for(int nInputs = 1; nInputs <= MaxNSources; ++nInputs) {
            tbb::flow::graph g;
            OType* my_or = new OType(g); //makeOr<OType>::create(); 
            tbb::flow::queue_node<TType> outq1(g);
            tbb::flow::queue_node<TType> outq2(g);

            ASSERT((*my_or).register_successor(outq1), NULL);  // register outputs first, so they both get all
            ASSERT((*my_or).register_successor(outq2), NULL);  // the results

            source_node_helper<SIZE, OType>::add_source_nodes((*my_or), g, nInputs);

            g.wait_for_all();

            reset_outputCheck(SIZE, Count);
            for(int i=0; i < Count*SIZE; ++i) {
                ASSERT(outq1.try_get(v), NULL);
                source_node_helper<SIZE, OType>::check_value(v);
            }

            check_outputCheck(SIZE, Count);
            reset_outputCheck(SIZE, Count);

            for(int i=0; i < Count*SIZE; i++) {
                ASSERT(outq2.try_get(v), NULL);;
                source_node_helper<SIZE, OType>::check_value(v);
            }
            check_outputCheck(SIZE, Count);

            ASSERT(!outq1.try_get(v), NULL);
            ASSERT(!outq2.try_get(v), NULL);

            source_node_helper<SIZE, OType>::remove_source_nodes((*my_or), nInputs);
            (*my_or).remove_successor(outq1);
            (*my_or).remove_successor(outq2);
            makeOr<OType>::destroy(my_or);
        }
    }
};

std::vector<int> last_index_seen;

template<int ELEM, typename OType>
class serial_queue_helper {
public:
    typedef typename OType::output_type OT;
    typedef typename OType::tuple_types TT;
    typedef typename std::tuple_element<ELEM-1,TT>::type IT;
    static void print_remark() {
        serial_queue_helper<ELEM-1,OType>::print_remark();
        REMARK(", %s", name_of<IT>::name());
    }
    static void fill_one_queue(int maxVal, OType &my_or) {
        // fill queue to "left" of me
        serial_queue_helper<ELEM-1,OType>::fill_one_queue(maxVal,my_or);
        for(int i = 0; i < maxVal; ++i) {
            ASSERT(tbb::flow::input_port<ELEM-1>(my_or).try_put((IT)(i*(ELEM+1))), NULL);
        }
    }
    static void put_one_queue_val(int myVal, OType &my_or) {
        // put this val to my "left".
        serial_queue_helper<ELEM-1,OType>::put_one_queue_val(myVal, my_or);
        ASSERT(tbb::flow::input_port<ELEM-1>(my_or).try_put((IT)(myVal*(ELEM+1))), NULL);
    }
    static void check_queue_value(OT &v) {
        if(ELEM - 1 == v.indx) {
            // this assumes each or node input is queueing.
            int rval = getval_helper<ELEM,OType>::get_integer_val(v);
            ASSERT( rval == (last_index_seen[ELEM-1]+1)*(ELEM+1), NULL);
            last_index_seen[ELEM-1] = rval / (ELEM+1);
        }
        else {
            serial_queue_helper<ELEM-1,OType>::check_queue_value(v);
        }
    }
};

template<typename OType>
class serial_queue_helper<1, OType> {
public:
    typedef typename OType::output_type OT;
    typedef typename OType::tuple_types TT;
    typedef typename std::tuple_element<0,TT>::type IT;
    static void print_remark() {
        REMARK("Serial test of or_node< %s", name_of<IT>::name());
    }
    static void fill_one_queue(int maxVal, OType &my_or) {
        for(int i = 0; i < maxVal; ++i) {
            ASSERT(tbb::flow::input_port<0>(my_or).try_put((IT)(i*2)), NULL);
        }
    }
    static void put_one_queue_val(int myVal, OType &my_or) {
        ASSERT(tbb::flow::input_port<0>(my_or).try_put((IT)(myVal*2)), NULL);
    }
    static void check_queue_value(OT &v) {
        ASSERT(v.indx == 0, NULL);  // won't get here unless true
        int rval = getval_helper<1,OType>::get_integer_val(v);
        ASSERT( rval == (last_index_seen[0]+1)*2, NULL);
        last_index_seen[0] = rval / 2;
    }
};

template<typename OType, typename TType, int SIZE>
void test_one_serial( OType &my_or, tbb::flow::graph &g) {
    last_index_seen.clear();
    for(int ii=0; ii < SIZE; ++ii) last_index_seen.push_back(-1);

    typedef TType q3_input_type;
    tbb::flow::queue_node< q3_input_type >  q3(g);
    q3_input_type v;

    ASSERT((my_or).register_successor( q3 ), NULL);

    // fill each queue with its value one-at-a-time
    for (int i = 0; i < Count; ++i ) {
        serial_queue_helper<SIZE,OType>::put_one_queue_val(i,my_or);
    }

    g.wait_for_all();
    for (int i = 0; i < Count * SIZE; ++i ) {
        g.wait_for_all();
        ASSERT(q3.try_get( v ), "Error in try_get()");
        {
            serial_queue_helper<SIZE,OType>::check_queue_value(v);
        }
    }
    ASSERT(!q3.try_get( v ), "extra values in output queue");
    for(int ii=0; ii < SIZE; ++ii) last_index_seen[ii] = -1;

    // fill each queue completely before filling the next.
    serial_queue_helper<SIZE, OType>::fill_one_queue(Count,my_or);

    g.wait_for_all();
    for (int i = 0; i < Count*SIZE; ++i ) {
        g.wait_for_all();
        ASSERT(q3.try_get( v ), "Error in try_get()");
        {
            serial_queue_helper<SIZE,OType>::check_queue_value(v);
        }
    }
    ASSERT(!q3.try_get( v ), "extra values in output queue");
}

//
// Single predecessor at each port, single accepting successor
//   * put to buffer before port0, then put to buffer before port1, ...
//   * fill buffer before port0 then fill buffer before port1, ...

template<typename OType>
class serial_test {
    typedef typename OType::output_type TType;  // this is the union
    typedef typename OType::tuple_types union_types;
    static const int SIZE = std::tuple_size<union_types>::value;
public:
static void test() {
    tbb::flow::graph g;
    static const int ELEMS = 3;
    OType* my_or = new OType(g); //makeOr<OType>::create(g);

    serial_queue_helper<SIZE, OType>::print_remark(); REMARK(" >\n");

    test_one_serial<OType,TType,SIZE>(*my_or, g);

    std::vector<OType> or_vector(ELEMS, *my_or);

    makeOr<OType>::destroy(my_or);

    for(int e = 0; e < ELEMS; ++e) {
        test_one_serial<OType,TType,SIZE>(or_vector[e], g);
    }
}

}; // serial_test

template<
      template<typename> class TestType,  // serial_test or parallel_test
      typename InputTupleType>           // type of the inputs to the or_node
class generate_test {
public:
    typedef tbb::flow::or_node<InputTupleType> or_node_type;
    static void do_test() {
        TestType<or_node_type>::test();
    }
};

int TestMain() {
    REMARK("Testing or_node, ");
#if __TBB_USE_TBB_TUPLE
    REMARK("using TBB tuple\n");
#else
    REMARK("using platform tuple\n");
#endif

   for (int p = 0; p < 2; ++p) {
       generate_test<serial_test, std::tuple<float, test_class> >::do_test();
       generate_test<serial_test, std::tuple<float, double, int, long> >::do_test();
#if __TBB_VARIADIC_MAX >= 6
       generate_test<serial_test, std::tuple<double, double, int, long, int, short> >::do_test();
#endif
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 8
       generate_test<serial_test, std::tuple<float, double, double, double, float, int, float, long> >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 10
       generate_test<serial_test, std::tuple<float, double, int, double, double, float, long, int, float, long> >::do_test();
#endif
#endif
       generate_test<parallel_test, std::tuple<float, double> >::do_test();
       generate_test<parallel_test, std::tuple<float, int, long> >::do_test();
       generate_test<parallel_test, std::tuple<double, double, int, int, short> >::do_test();
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 7
       generate_test<parallel_test, std::tuple<float, int, double, float, long, float, long> >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 9
       generate_test<parallel_test, std::tuple<float, double, int, double, double, long, int, float, long> >::do_test();
#endif
#endif
   }
   return Harness::Done;
}
#else  // __SUNPRO_CC
int TestMain() {
    return Harness::Skipped;
}
#endif  // __SUNPRO_CC
