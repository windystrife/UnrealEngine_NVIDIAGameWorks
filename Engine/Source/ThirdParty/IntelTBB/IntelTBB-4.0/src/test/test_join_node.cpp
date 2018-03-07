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

#if _MSC_VER
// Name length is limited to avoid "decorated name length exceeded, name was truncated" warning.
#define _VARIADIC_MAX 8
#endif

#include "harness.h"

#if !__SUNPRO_CC

#include "tbb/flow_graph.h"
#include "tbb/task_scheduler_init.h"

// the tuple-based tests with more inputs take a long time to compile.  If changes
// are made to the tuple implementation or any switch that controls it, the test
// should be compiled with COMPREHENSIVE_TEST == 1 to ensure all tuple sizes are tested.
#ifndef COMPREHENSIVE_TEST
#define COMPREHENSIVE_TEST 0
#endif

//
// Tests
//

const int Count = 150;
const int Recirc_count = 1000;  // number of tuples to be generated
const int MaxPorts = 10;
const int MaxNSources = 5; // max # of source_nodes to register for each join_node input in parallel test
bool outputCheck[MaxPorts][Count];  // for checking output

using tbb::flow::NO_TAG;

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

// for recirculating tags, input is tuple<index,continue_msg>
// output is index*my_mult cast to the right type
template<typename TT>
class recirc_func_body {
    TT my_mult;
public:
    typedef std::tuple<int, tbb::flow::continue_msg> input_type;
    recirc_func_body(TT multiplier ) : my_mult(multiplier) {}
    recirc_func_body(const recirc_func_body &other) : my_mult(other.my_mult) { }
    void operator=( const recirc_func_body &other) { my_mult = other.my_mult; }
    TT operator()(const input_type &v) {
        return TT(std::get<0>(v)) * my_mult;
    }
};

static int input_count;  // source_nodes are serial
static tbb::atomic<int> output_count;

// emit input_count continue_msg
class recirc_source_node_body {
public:
    bool operator()(tbb::flow::continue_msg &v ) {
        --input_count;
        v = tbb::flow::continue_msg();
        return 0 <= input_count; 
    }
};

// T must be arithmetic, and shouldn't wrap around for reasonable sizes of Count (which is now 150, and maxPorts is 10,
// so the max number generated right now is 1500 or so.)  Source will generate a series of TT with value
// (init_val + (i-1)*addend) * my_mult, where i is the i-th invocation of the body.  We are attaching addend
// source nodes to a join_port, and each will generate part of the numerical series the port is expecting
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

template<typename TT>
class tag_func {
    const TT my_mult;
    tag_func& operator=( const tag_func& other);
public:
    tag_func(TT multiplier) : my_mult(multiplier) { }
    // operator() will return [0 .. Count) 
    tbb::flow::tag_value operator()( TT v) {
        tbb::flow::tag_value t = tbb::flow::tag_value(v / my_mult);
        return t;
    }
};

// allocator for join_node.  This is specialized for tag_matching joins because they  require a variable number
// of tag_value methods passed to the constructor

template<int N, typename JType, tbb::flow::graph_buffer_policy JP>
class makeJoin {
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g);
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};


template<typename JType>
class makeJoin<2,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};

template<typename JType>
class makeJoin<3,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};

template<typename JType>
class makeJoin<4,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
    typedef typename std::tuple_element<3, TType>::type T3;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4)),
            tag_func<T3>(T3(5))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};

template<typename JType>
class makeJoin<5,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
    typedef typename std::tuple_element<3, TType>::type T3;
    typedef typename std::tuple_element<4, TType>::type T4;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4)),
            tag_func<T3>(T3(5)),
            tag_func<T4>(T4(6))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};
#if __TBB_VARIADIC_MAX >= 6
template<typename JType>
class makeJoin<6,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
    typedef typename std::tuple_element<3, TType>::type T3;
    typedef typename std::tuple_element<4, TType>::type T4;
    typedef typename std::tuple_element<5, TType>::type T5;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4)),
            tag_func<T3>(T3(5)),
            tag_func<T4>(T4(6)),
            tag_func<T5>(T5(7))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};
#endif

#if __TBB_VARIADIC_MAX >= 7
template<typename JType>
class makeJoin<7,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
    typedef typename std::tuple_element<3, TType>::type T3;
    typedef typename std::tuple_element<4, TType>::type T4;
    typedef typename std::tuple_element<5, TType>::type T5;
    typedef typename std::tuple_element<6, TType>::type T6;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4)),
            tag_func<T3>(T3(5)),
            tag_func<T4>(T4(6)),
            tag_func<T5>(T5(7)),
            tag_func<T6>(T6(8))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};
#endif

#if __TBB_VARIADIC_MAX >= 8
template<typename JType>
class makeJoin<8,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
    typedef typename std::tuple_element<3, TType>::type T3;
    typedef typename std::tuple_element<4, TType>::type T4;
    typedef typename std::tuple_element<5, TType>::type T5;
    typedef typename std::tuple_element<6, TType>::type T6;
    typedef typename std::tuple_element<7, TType>::type T7;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4)),
            tag_func<T3>(T3(5)),
            tag_func<T4>(T4(6)),
            tag_func<T5>(T5(7)),
            tag_func<T6>(T6(8)),
            tag_func<T7>(T7(9))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};
#endif

#if __TBB_VARIADIC_MAX >= 9
template<typename JType>
class makeJoin<9,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
    typedef typename std::tuple_element<3, TType>::type T3;
    typedef typename std::tuple_element<4, TType>::type T4;
    typedef typename std::tuple_element<5, TType>::type T5;
    typedef typename std::tuple_element<6, TType>::type T6;
    typedef typename std::tuple_element<7, TType>::type T7;
    typedef typename std::tuple_element<8, TType>::type T8;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4)),
            tag_func<T3>(T3(5)),
            tag_func<T4>(T4(6)),
            tag_func<T5>(T5(7)),
            tag_func<T6>(T6(8)),
            tag_func<T7>(T7(9)),
            tag_func<T8>(T8(10))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};
#endif

#if __TBB_VARIADIC_MAX >= 10
template<typename JType>
class makeJoin<10,JType,tbb::flow::tag_matching> {
    typedef typename JType::output_type TType;
    typedef typename std::tuple_element<0, TType>::type T0;
    typedef typename std::tuple_element<1, TType>::type T1;
    typedef typename std::tuple_element<2, TType>::type T2;
    typedef typename std::tuple_element<3, TType>::type T3;
    typedef typename std::tuple_element<4, TType>::type T4;
    typedef typename std::tuple_element<5, TType>::type T5;
    typedef typename std::tuple_element<6, TType>::type T6;
    typedef typename std::tuple_element<7, TType>::type T7;
    typedef typename std::tuple_element<8, TType>::type T8;
    typedef typename std::tuple_element<9, TType>::type T9;
public:
    static JType *create(tbb::flow::graph& g) {
        JType *temp = new JType(g, 
            tag_func<T0>(T0(2)),
            tag_func<T1>(T1(3)),
            tag_func<T2>(T2(4)),
            tag_func<T3>(T3(5)),
            tag_func<T4>(T4(6)),
            tag_func<T5>(T5(7)),
            tag_func<T6>(T6(8)),
            tag_func<T7>(T7(9)),
            tag_func<T8>(T8(10)),
            tag_func<T9>(T9(11))
            );
        return temp;
    }
    static void destroy(JType *p) { delete p; }
};
#endif

// holder for source_node pointers for eventual deletion

static void* all_source_nodes[MaxPorts][MaxNSources];

template<int ELEM, typename JNT>
class source_node_helper {
public:
    typedef JNT join_node_type;
    typedef tbb::flow::join_node<std::tuple<int, tbb::flow::continue_msg>, tbb::flow::reserving> input_join_type;
    typedef typename join_node_type::output_type TT;
    typedef typename std::tuple_element<ELEM-1,TT>::type IT;
    typedef typename tbb::flow::source_node<IT> my_source_node_type;
    typedef typename tbb::flow::function_node<std::tuple<int,tbb::flow::continue_msg>, IT> my_recirc_function_type;
    static void print_remark(const char * str) {
        source_node_helper<ELEM-1,JNT>::print_remark(str);
        REMARK(", %s", name_of<IT>::name());
    }
    static void add_source_nodes(join_node_type &my_join, tbb::flow::graph &g, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *new_node = new my_source_node_type(g, source_body<IT>((IT)(ELEM+1), i, nInputs));
            tbb::flow::make_edge( *new_node, tbb::flow::input_port<ELEM-1>(my_join) );
            all_source_nodes[ELEM-1][i] = (void *)new_node;
        }
        // add the next source_node
        source_node_helper<ELEM-1, JNT>::add_source_nodes(my_join, g, nInputs);
    }

    static void add_recirc_func_nodes(join_node_type &my_join, input_join_type &my_input, tbb::flow::graph &g) {
        my_recirc_function_type *new_node = new my_recirc_function_type(g, tbb::flow::unlimited, recirc_func_body<IT>((IT)(ELEM+1)));
        tbb::flow::make_edge(*new_node, tbb::flow::input_port<ELEM-1>(my_join));
        tbb::flow::make_edge(my_input, *new_node);
        all_source_nodes[ELEM-1][0] = (void *)new_node;
        source_node_helper<ELEM-1, JNT>::add_recirc_func_nodes(my_join, my_input, g);
    }

    static void only_check_value(const int i, const TT &v) {
        ASSERT( std::get<ELEM-1>(v) == (IT)(i*(ELEM+1)), NULL);
        source_node_helper<ELEM-1,JNT>::only_check_value(i, v);
    }

    static void check_value(int i, TT &v, bool is_serial) {
        // the fetched value will match only if there is only one source_node.
        ASSERT(!is_serial || std::get<ELEM-1>(v) == (IT)(i*(ELEM+1)), NULL);
        // tally the fetched value.
        int ival = (int)std::get<ELEM-1>(v);
        ASSERT(!(ival%(ELEM+1)), NULL);
        ival /= (ELEM+1);
        ASSERT(!outputCheck[ELEM-1][ival], NULL);
        outputCheck[ELEM-1][ival] = true;
        source_node_helper<ELEM-1,JNT>::check_value(i, v, is_serial);
    }
    static void remove_source_nodes(join_node_type& my_join, int nInputs) {
        for(int i=0; i< nInputs; ++i) {
            my_source_node_type *dp = reinterpret_cast<my_source_node_type *>(all_source_nodes[ELEM-1][i]);
            tbb::flow::remove_edge( *dp, tbb::flow::input_port<ELEM-1>(my_join) );
            delete dp;
        }
        source_node_helper<ELEM-1, JNT>::remove_source_nodes(my_join, nInputs);
    }

    static void remove_recirc_func_nodes(join_node_type& my_join, input_join_type &my_input) {
        my_recirc_function_type *fn = reinterpret_cast<my_recirc_function_type *>(all_source_nodes[ELEM-1][0]);
        tbb::flow::remove_edge( *fn, tbb::flow::input_port<ELEM-1>(my_join) );
        tbb::flow::remove_edge( my_input, *fn);
        delete fn;
        source_node_helper<ELEM-1, JNT>::remove_recirc_func_nodes(my_join,my_input);
    }
};

template<typename JNT>
class source_node_helper<1, JNT> {
    typedef JNT join_node_type;
    typedef tbb::flow::join_node<std::tuple<int, tbb::flow::continue_msg>, tbb::flow::reserving> input_join_type;
    typedef typename join_node_type::output_type TT;
    typedef typename std::tuple_element<0,TT>::type IT;
    typedef typename tbb::flow::source_node<IT> my_source_node_type;
    typedef typename tbb::flow::function_node<std::tuple<int,tbb::flow::continue_msg>, IT> my_recirc_function_type;
public:
    static void print_remark(const char * str) {
        REMARK("%s< %s", str, name_of<IT>::name());
    }
    static void add_source_nodes(join_node_type &my_join, tbb::flow::graph &g, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *new_node = new my_source_node_type(g, source_body<IT>((IT)2, i, nInputs));
            tbb::flow::make_edge( *new_node, tbb::flow::input_port<0>(my_join) );
            all_source_nodes[0][i] = (void *)new_node;
        }
    }

    static void add_recirc_func_nodes(join_node_type &my_join, input_join_type &my_input, tbb::flow::graph &g) {
        my_recirc_function_type *new_node = new my_recirc_function_type(g, tbb::flow::unlimited, recirc_func_body<IT>((IT)(2)));
        tbb::flow::make_edge(*new_node, tbb::flow::input_port<0>(my_join));
        tbb::flow::make_edge(my_input, *new_node);
        all_source_nodes[0][0] = (void *)new_node;
    }

    static void only_check_value(const int i, const TT &v) {
        ASSERT( std::get<0>(v) == (IT)(i*2), NULL);
    }

    static void check_value(int i, TT &v, bool is_serial) {
        ASSERT(!is_serial || std::get<0>(v) == (IT)(i*(2)), NULL);
        int ival = (int)std::get<0>(v);
        ASSERT(!(ival%2), NULL);
        ival /= 2;
        ASSERT(!outputCheck[0][ival], NULL);
        outputCheck[0][ival] = true;
    }
    static void remove_source_nodes(join_node_type& my_join, int nInputs) {
        for(int i=0; i < nInputs; ++i) {
            my_source_node_type *dp = reinterpret_cast<my_source_node_type *>(all_source_nodes[0][i]);
            tbb::flow::remove_edge( *dp, tbb::flow::input_port<0>(my_join) );
            delete dp;
        }
    }

    static void remove_recirc_func_nodes(join_node_type& my_join, input_join_type &my_input) {
        my_recirc_function_type *fn = reinterpret_cast<my_recirc_function_type *>(all_source_nodes[0][0]);
        tbb::flow::remove_edge( *fn, tbb::flow::input_port<0>(my_join) );
        tbb::flow::remove_edge( my_input, *fn);
        delete fn;
    }
};

// get the tag from the output tuple and emit it.
// the first tuple component is tag * 2 cast to the type
template<typename OutputTupleType>
class recirc_output_func_body {
public:
    // we only need this to use source_node_helper
    typedef typename tbb::flow::join_node<OutputTupleType, tbb::flow::tag_matching> join_node_type;
    static const int N = std::tuple_size<OutputTupleType>::value;
    int operator()(const OutputTupleType &v) {
        int out = int(std::get<0>(v)) / 2;
        source_node_helper<N,join_node_type>::only_check_value(out,v);
        ++output_count;
        return out;
    }
};

template<typename JType>
class tag_recirculation_test {
public:
    typedef typename JType::output_type TType;
    typedef typename std::tuple<int, tbb::flow::continue_msg> input_tuple_type;
    typedef tbb::flow::join_node<input_tuple_type,tbb::flow::reserving> input_join_type;
    static const int N = std::tuple_size<TType>::value;
    static void test() {
        source_node_helper<N,JType>::print_remark("Recirculation test of tag-matching join");
        REMARK(" >\n");
        for(int maxTag = 1; maxTag <10; maxTag *= 3) {
            for(int i=0; i < N; ++i) all_source_nodes[i][0] = NULL;

            tbb::flow::graph g;
            // this is the tag-matching join we're testing
            JType * my_join = makeJoin<N,JType, tbb::flow::tag_matching>::create(g);
            // source_node for continue messages
            tbb::flow::source_node<tbb::flow::continue_msg> snode(g, recirc_source_node_body(), false);
            // reserving join that matches recirculating tags with continue messages.
            input_join_type * my_input_join = makeJoin<2,input_join_type,tbb::flow::reserving>::create(g);
            // tbb::flow::make_edge(snode, tbb::flow::input_port<1>(*my_input_join));
            tbb::flow::make_edge(snode, std::get<1>(my_input_join->input_ports()));
            // queue to hold the tags
            tbb::flow::queue_node<int> tag_queue(g);
            tbb::flow::make_edge(tag_queue, tbb::flow::input_port<0>(*my_input_join));
            // add all the function_nodes that are inputs to the tag-matching join
            source_node_helper<N,JType>::add_recirc_func_nodes(*my_join, *my_input_join, g);
            // add the function_node that accepts the output of the join and emits the int tag it was based on
            tbb::flow::function_node<TType, int> recreate_tag(g, tbb::flow::unlimited, recirc_output_func_body<TType>());
            tbb::flow::make_edge(*my_join, recreate_tag);
            // now the recirculating part (output back to the queue)
            tbb::flow::make_edge(recreate_tag, tag_queue);

            // put the tags into the queue
            for(int t = 1; t <= maxTag; ++t) tag_queue.try_put(t);

            input_count = Recirc_count;
            output_count = 0;

            // start up the source node to get things going
            snode.activate();

            // wait for everything to stop
            g.wait_for_all();

            ASSERT(output_count == Recirc_count, "not all instances were received");

            int j;
            // grab the tags from the queue, record them
            std::vector<bool> out_tally(maxTag, false);
            for(int i = 0; i < maxTag; ++i) {
                ASSERT(tag_queue.try_get(j), "not enough tags in queue");
                ASSERT(!out_tally.at(j-1), "duplicate tag from queue");
                out_tally[j-1] = true;
            }
            ASSERT(!tag_queue.try_get(j), "Extra tags in recirculation queue");

            // deconstruct graph
            source_node_helper<N, JType>::remove_recirc_func_nodes(*my_join, *my_input_join);
            tbb::flow::remove_edge(*my_join, recreate_tag);
            makeJoin<N,JType,tbb::flow::tag_matching>::destroy(my_join);
            tbb::flow::remove_edge(tag_queue, tbb::flow::input_port<0>(*my_input_join));
            tbb::flow::remove_edge(snode, tbb::flow::input_port<1>(*my_input_join));
            makeJoin<2,input_join_type,tbb::flow::reserving>::destroy(my_input_join);
        }
    }
};

template<typename JType, tbb::flow::graph_buffer_policy JP>
class parallel_test {
public:
    typedef typename JType::output_type TType;
    static const int SIZE = std::tuple_size<TType>::value;
    static const tbb::flow::graph_buffer_policy jp = JP;
    static void test() {
        TType v;
        source_node_helper<SIZE,JType>::print_remark("Parallel test of join_node");
        REMARK(" >\n");
        for(int i=0; i < MaxPorts; ++i) {
            for(int j=0; j < MaxNSources; ++j) {
                all_source_nodes[i][j] = NULL;
            }
        }
        for(int nInputs = 1; nInputs <= MaxNSources; ++nInputs) {
            tbb::flow::graph g;
            // JType my_join(g);
            bool not_out_of_order = (nInputs == 1) && (jp != tbb::flow::tag_matching);
            JType* my_join = makeJoin<SIZE,JType,JP>::create(g);
            tbb::flow::queue_node<TType> outq1(g);
            tbb::flow::queue_node<TType> outq2(g);

            tbb::flow::make_edge( *my_join, outq1 );
            tbb::flow::make_edge( *my_join, outq2 );

            source_node_helper<SIZE, JType>::add_source_nodes((*my_join), g, nInputs);

            g.wait_for_all();

            reset_outputCheck(SIZE, Count);
            for(int i=0; i < Count; ++i) {
                ASSERT(outq1.try_get(v), NULL);
                source_node_helper<SIZE, JType>::check_value(i, v, not_out_of_order);
            }

            check_outputCheck(SIZE, Count);
            reset_outputCheck(SIZE, Count);

            for(int i=0; i < Count; i++) {
                ASSERT(outq2.try_get(v), NULL);;
                source_node_helper<SIZE, JType>::check_value(i, v, not_out_of_order);
            }
            check_outputCheck(SIZE, Count);

            ASSERT(!outq1.try_get(v), NULL);
            ASSERT(!outq2.try_get(v), NULL);

            source_node_helper<SIZE, JType>::remove_source_nodes((*my_join), nInputs);
            tbb::flow::remove_edge( *my_join, outq1 );
            tbb::flow::remove_edge( *my_join, outq2 );
            makeJoin<SIZE,JType,JP>::destroy(my_join);
        }
    }
};


template<int ELEM, typename JType>
class serial_queue_helper {
public:
    typedef typename JType::output_type TT;
    typedef typename std::tuple_element<ELEM-1,TT>::type IT;
    typedef typename tbb::flow::queue_node<IT> my_queue_node_type;
    static void print_remark() {
        serial_queue_helper<ELEM-1,JType>::print_remark();
        REMARK(", %s", name_of<IT>::name());
    }
    static void add_queue_nodes(tbb::flow::graph &g, JType &my_join) {
        serial_queue_helper<ELEM-1,JType>::add_queue_nodes(g, my_join);
        my_queue_node_type *new_node = new my_queue_node_type(g);
        tbb::flow::make_edge( *new_node, std::get<ELEM-1>(my_join.input_ports()) );
        all_source_nodes[ELEM-1][0] = (void *)new_node;
    }
    static void fill_one_queue(int maxVal) {
        // fill queue to "left" of me
        my_queue_node_type *qptr = reinterpret_cast<my_queue_node_type *>(all_source_nodes[ELEM-1][0]);
        serial_queue_helper<ELEM-1,JType>::fill_one_queue(maxVal);
        for(int i = 0; i < maxVal; ++i) {
            ASSERT(qptr->try_put((IT)(i*(ELEM+1))), NULL);
        }
    }
    static void put_one_queue_val(int myVal) {
        // put this val to my "left".
        serial_queue_helper<ELEM-1,JType>::put_one_queue_val(myVal);
        my_queue_node_type *qptr = reinterpret_cast<my_queue_node_type *>(all_source_nodes[ELEM-1][0]);
        ASSERT(qptr->try_put((IT)(myVal*(ELEM+1))), NULL);
    }
    static void check_queue_value(int i, TT &v) {
        serial_queue_helper<ELEM-1,JType>::check_queue_value(i, v);
        ASSERT( std::get<ELEM-1>(v) == (IT)(i * (ELEM+1)), NULL);
    }
    static void remove_queue_nodes(JType &my_join) {
        my_queue_node_type *vptr = reinterpret_cast<my_queue_node_type *>(all_source_nodes[ELEM-1][0]);
        tbb::flow::remove_edge( *vptr, std::get<ELEM-1>(my_join.input_ports()) );
        serial_queue_helper<ELEM-1, JType>::remove_queue_nodes(my_join);
        delete vptr;
    }
};

template<typename JType>
class serial_queue_helper<1, JType> {
public:
    typedef typename JType::output_type TT;
    typedef typename std::tuple_element<0,TT>::type IT;
    typedef typename tbb::flow::queue_node<IT> my_queue_node_type;
    static void print_remark() {
        REMARK("Serial test of join_node< %s", name_of<IT>::name());
    }
    static void add_queue_nodes(tbb::flow::graph &g, JType &my_join) {
        my_queue_node_type *new_node = new my_queue_node_type(g);
        tbb::flow::make_edge( *new_node, tbb::flow::input_port<0>(my_join) );
        all_source_nodes[0][0] = (void *)new_node;
    }
    static void fill_one_queue(int maxVal) {
        my_queue_node_type *qptr = reinterpret_cast<my_queue_node_type *>(all_source_nodes[0][0]);
        for(int i = 0; i < maxVal; ++i) {
            ASSERT(qptr->try_put((IT)(i*2)), NULL);
        }
    }
    static void put_one_queue_val(int myVal) {
        my_queue_node_type *qptr = reinterpret_cast<my_queue_node_type *>(all_source_nodes[0][0]);
        ASSERT(qptr->try_put((IT)(myVal*2)), NULL);
    }
    static void check_queue_value(int i, TT &v) {
        ASSERT( std::get<0>(v) == (IT)(i*2), NULL);
    }
    static void remove_queue_nodes(JType &my_join) {
        my_queue_node_type *vptr = reinterpret_cast<my_queue_node_type *>(all_source_nodes[0][0]);
        tbb::flow::remove_edge( *vptr, std::get<0>(my_join.input_ports()) );
        delete vptr;
    }
};

//
// Single reservable predecessor at each port, single accepting successor
//   * put to buffer before port0, then put to buffer before port1, ...
//   * fill buffer before port0 then fill buffer before port1, ...

template<typename JType, tbb::flow::graph_buffer_policy JP>
void test_one_serial( JType &my_join, tbb::flow::graph &g) {
    typedef typename JType::output_type TType;
    static const int SIZE = std::tuple_size<TType>::value;
    std::vector<bool> flags;
    serial_queue_helper<SIZE, JType>::add_queue_nodes(g,my_join);
    typedef TType q3_input_type;
    tbb::flow::queue_node< q3_input_type >  q3(g);

    tbb::flow::make_edge( my_join, q3 );

    // fill each queue with its value one-at-a-time
    flags.clear();
    for (int i = 0; i < Count; ++i ) {
        serial_queue_helper<SIZE,JType>::put_one_queue_val(i);
        flags.push_back(false);
    }

    g.wait_for_all();
    tbb::flow::graph_buffer_policy jp = JP;
    for (int i = 0; i < Count; ++i ) {
        q3_input_type v;
        g.wait_for_all();
        ASSERT(q3.try_get( v ), "Error in try_get()");
        if(jp == tbb::flow::tag_matching) {
            // because we look up tags in the hash table, the output may be out of order.
            int j = int(std::get<0>(v)) / 2;  // figure what the index should be
            serial_queue_helper<SIZE,JType>::check_queue_value(j, v);
            flags[j] = true;
        }
        else {
            serial_queue_helper<SIZE,JType>::check_queue_value(i, v);
        }
    }

    if(jp == tbb::flow::tag_matching) {
        for(int i = 0; i < Count; ++i) {
            ASSERT(flags[i], NULL);
            flags[i] = false;
        }
    }

    // fill each queue completely before filling the next.
    serial_queue_helper<SIZE, JType>::fill_one_queue(Count);

    g.wait_for_all();
    for (int i = 0; i < Count; ++i ) {
        q3_input_type v;
        g.wait_for_all();
        ASSERT(q3.try_get( v ), "Error in try_get()");
        if(jp == tbb::flow::tag_matching) {
            int j = int(std::get<0>(v)) / 2;
            serial_queue_helper<SIZE,JType>::check_queue_value(j, v);
            flags[i] = true;
        }
        else {
            serial_queue_helper<SIZE,JType>::check_queue_value(i, v);
        }
    }

    if(jp == tbb::flow::tag_matching) {
        for(int i = 0; i < Count; ++i) {
            ASSERT(flags[i], NULL);
        }
    }

    serial_queue_helper<SIZE, JType>::remove_queue_nodes(my_join);

}

template<typename JType, tbb::flow::graph_buffer_policy JP>
class serial_test {
    typedef typename JType::output_type TType;
    static const int SIZE = std::tuple_size<TType>::value;
    static const int ELEMS = 3;
public:
static void test() {
    tbb::flow::graph g;
    std::vector<bool> flags;
    flags.reserve(Count);
    JType* my_join = makeJoin<SIZE,JType,JP>::create(g);
    serial_queue_helper<SIZE, JType>::print_remark(); REMARK(" >\n");

    test_one_serial<JType,JP>( *my_join, g);
    // build the vector with copy construction from the used join node.
    std::vector<JType>join_vector(ELEMS, *my_join);
    // destroy the tired old join_node in case we're accidentally reusing pieces of it.
    makeJoin<SIZE,JType,JP>::destroy(my_join);


    for(int e = 0; e < ELEMS; ++e) {  // exercise each of the vector elements
        test_one_serial<JType,JP>( join_vector[e], g);
    }
}

}; // serial_test

template<
      template<typename, tbb::flow::graph_buffer_policy> class TestType,  // serial_test or parallel_test
      typename OutputTupleType,           // type of the output of the join
      tbb::flow::graph_buffer_policy J>                 // graph_buffer_policy (reserving, queueing or tag_matching)
class generate_test {
public:
    typedef tbb::flow::join_node<OutputTupleType,J> join_node_type;
    static void do_test() {
        TestType<join_node_type,J>::test();
    }
};

template<typename JType>
class generate_recirc_test {
public:
    typedef tbb::flow::join_node<JType, tbb::flow::tag_matching> join_node_type;
    static void do_test() {
        tag_recirculation_test<join_node_type>::test();
    }
};

template<tbb::flow::graph_buffer_policy JP>
void test_input_port_policies();

// join_node (reserving) does not consume inputs until an item is available at
// every input.  It tries to reserve each input, and if any fails it releases the
// reservation.  When it builds a tuple it broadcasts to all its successors and
// consumes all the inputs.
//
// So our test will put an item at one input port, then attach another node to the
// same node (a queue node in this case).  The second successor should receive the
// item in the queue, emptying it.
//
// We then place an item in the second input queue, and check the output queues; they
// should still be empty.  Then we place an item in the first queue; the output queues
// should then receive a tuple.
//
// we then attach another function node to the second input.  It should not receive
// an item, verifying that the item in the queue is consumed.
template<>
void test_input_port_policies<tbb::flow::reserving>() {
    tbb::flow::graph g;
    typedef tbb::flow::join_node<std::tuple<int, int>, tbb::flow::reserving > JType; // two-phase is the default policy
    // create join_node<type0,type1> jn
    JType jn(g);
    // create output_queue oq0, oq1
    typedef JType::output_type OQType;
    tbb::flow::queue_node<OQType> oq0(g);
    tbb::flow::queue_node<OQType> oq1(g);
    // create iq0, iq1
    typedef tbb::flow::queue_node<int> IQType;
    IQType iq0(g);
    IQType iq1(g);
    // create qnp, qnq
    IQType qnp(g);
    IQType qnq(g);
    REMARK("Testing policies of join_node<reserving> input ports\n");
    // attach jn to oq0, oq1
    tbb::flow::make_edge( jn, oq0 );
    tbb::flow::make_edge( jn, oq1 );
    // attach iq0, iq1 to jn
    tbb::flow::make_edge( iq0, std::get<0>(jn.input_ports()) );
    tbb::flow::make_edge( iq1, std::get<1>(jn.input_ports()) );
    for(int loop = 0; loop < 3; ++loop) {
        // place one item in iq0
        ASSERT(iq0.try_put(1), "Error putting to iq1");
        // attach iq0 to qnp
        tbb::flow::make_edge( iq0, qnp );
        // qnp should have an item in it.
        g.wait_for_all();
        {
            int i;
            ASSERT(qnp.try_get(i) && i == 1, "Error in item fetched by qnp");
        }
        // place item in iq1
        ASSERT(iq1.try_put(2), "Error putting to iq1");
        // oq0, oq1 should be empty
        g.wait_for_all();
        {
            OQType t1;
            ASSERT(!oq0.try_get(t1) && !oq1.try_get(t1), "oq0 and oq1 not empty");
        }
        // detach qnp from iq0
        tbb::flow::remove_edge( iq0, qnp); // if we don't remove qnp it will gobble any values we put in iq0
        // place item in iq0
        ASSERT(iq0.try_put(3), "Error on second put to iq0");
        // oq0, oq1 should have items in them
        g.wait_for_all();
        {
            OQType t0;
            OQType t1;
            ASSERT(oq0.try_get(t0) && std::get<0>(t0) == 3 && std::get<1>(t0) == 2, "Error in oq0 output");
            ASSERT(oq1.try_get(t1) && std::get<0>(t1) == 3 && std::get<1>(t1) == 2, "Error in oq1 output");
        }
        // attach qnp to iq0, qnq to iq1
        // qnp and qnq should be empty
        tbb::flow::make_edge( iq0, qnp );
        tbb::flow::make_edge( iq1, qnq );
        g.wait_for_all();
        {
            int i;
            ASSERT(!qnp.try_get(i), "iq0 still had value in it");
            ASSERT(!qnq.try_get(i), "iq1 still had value in it");
        }
        tbb::flow::remove_edge( iq0, qnp );
        tbb::flow::remove_edge( iq1, qnq );
    } // for ( int loop ...
}

// join_node (queueing) consumes inputs as soon as they are available at
// any input.  When it builds a tuple it broadcasts to all its successors and
// discards the broadcast values.
//
// So our test will put an item at one input port, then attach another node to the
// same node (a queue node in this case).  The second successor should not receive
// an item (because the join consumed it).
//
// We then place an item in the second input queue, and check the output queues; they
// should each have a tuple.
//
// we then attach another function node to the second input.  It should not receive
// an item, verifying that the item in the queue is consumed.
template<>
void test_input_port_policies<tbb::flow::queueing>() {
    tbb::flow::graph g;
    typedef tbb::flow::join_node<std::tuple<int, int>, tbb::flow::queueing > JType;
    // create join_node<type0,type1> jn
    JType jn(g);
    // create output_queue oq0, oq1
    typedef JType::output_type OQType;
    tbb::flow::queue_node<OQType> oq0(g);
    tbb::flow::queue_node<OQType> oq1(g);
    // create iq0, iq1
    typedef tbb::flow::queue_node<int> IQType;
    IQType iq0(g);
    IQType iq1(g);
    // create qnp, qnq
    IQType qnp(g);
    IQType qnq(g);
    REMARK("Testing policies of join_node<queueing> input ports\n");
    // attach jn to oq0, oq1
    tbb::flow::make_edge( jn, oq0 );
    tbb::flow::make_edge( jn, oq1 );
    // attach iq0, iq1 to jn
    tbb::flow::make_edge( iq0, std::get<0>(jn.input_ports()) );
    tbb::flow::make_edge( iq1, std::get<1>(jn.input_ports()) );
    for(int loop = 0; loop < 3; ++loop) {
        // place one item in iq0
        ASSERT(iq0.try_put(1), "Error putting to iq1");
        // attach iq0 to qnp
        tbb::flow::make_edge( iq0, qnp );
        // qnp should have an item in it.
        g.wait_for_all();
        {
            int i;
            ASSERT(!qnp.try_get(i), "Item was received by qnp");
        }
        // place item in iq1
        ASSERT(iq1.try_put(2), "Error putting to iq1");
        // oq0, oq1 should have items
        g.wait_for_all();
        {
            OQType t0;
            OQType t1;
            ASSERT(oq0.try_get(t0) && std::get<0>(t0) == 1 && std::get<1>(t0) == 2, "Error in oq0 output");
            ASSERT(oq1.try_get(t1) && std::get<0>(t1) == 1 && std::get<1>(t1) == 2, "Error in oq1 output");
        }
        // attach qnq to iq1
        // qnp and qnq should be empty
        tbb::flow::make_edge( iq1, qnq );
        g.wait_for_all();
        {
            int i;
            ASSERT(!qnp.try_get(i), "iq0 still had value in it");
            ASSERT(!qnq.try_get(i), "iq1 still had value in it");
        }
        tbb::flow::remove_edge( iq0, qnp );
        tbb::flow::remove_edge( iq1, qnq );
    } // for ( int loop ...
}

tbb::flow::tag_value myTagValue(int i) { return tbb::flow::tag_value(i); }

// join_node (tag_matching) consumes inputs as soon as they are available at
// any input.  When it builds a tuple it broadcasts to all its successors and
// discards the broadcast values.
//
// It chooses the tuple it broadcasts by matching the tag values returned by the
// methods given the constructor of the join, in this case the method just casts
// the value in each port to tag_value.
//
// So our test will put an item at one input port, then attach another node to the
// same node (a queue node in this case).  The second successor should not receive
// an item (because the join consumed it).
//
// We then place an item in the second input queue, and check the output queues; they
// should each have a tuple.
//
// we then attach another queue node to the second input.  It should not receive
// an item, verifying that the item in the queue is consumed.
//
// We will then exercise the join with a bunch of values, and the output order should
// be determined by the order we insert items into the second queue.  (Each tuple set
// corresponding to a tag will be complete when the second item is inserted.)
template<>
void test_input_port_policies<tbb::flow::tag_matching>() {
    tbb::flow::graph g;
    typedef tbb::flow::join_node<std::tuple<int, int>, tbb::flow::tag_matching > JType;
    JType jn(g, myTagValue, myTagValue);
    // create output_queue oq0, oq1
    typedef JType::output_type OQType;
    tbb::flow::queue_node<OQType> oq0(g);
    tbb::flow::queue_node<OQType> oq1(g);
    // create iq0, iq1
    typedef tbb::flow::queue_node<int> IQType;
    IQType iq0(g);
    IQType iq1(g);
    // create qnp, qnq
    IQType qnp(g);
    IQType qnq(g);

    REMARK("Testing policies of join_node<tag_matching> input ports\n");
    // attach jn to oq0, oq1
    tbb::flow::make_edge( jn, oq0 );
    tbb::flow::make_edge( jn, oq1 );
    // attach iq0, iq1 to jn
    tbb::flow::make_edge( iq0, tbb::flow::input_port<0>(jn) );
    tbb::flow::make_edge( iq1, tbb::flow::input_port<1>(jn) );
    // we'll put four discrete values in the inputs to the join_node.  Each
    // set of inputs should result in one output.  (NO_TAG is currently defined
    // to be tag_value(-1), so zero is an allowed tag_value.)
    for(int loop = 0; loop < 4; ++loop) {
        // place one item in iq0
        ASSERT(iq0.try_put(loop), "Error putting to iq1"); 
        // attach iq0 to qnp
        tbb::flow::make_edge( iq0, qnp );
        // qnp should not have an item in it.  (the join consumed it.)
        g.wait_for_all();
        {
            int i;
            ASSERT(!qnp.try_get(i), "Item was received by qnp");
        }
        // place item in iq1
        ASSERT(iq1.try_put(loop), "Error putting to iq1");
        // oq0, oq1 should have items
        g.wait_for_all();
        {
            OQType t0;
            OQType t1;
            ASSERT(oq0.try_get(t0) && std::get<0>(t0) == loop && std::get<1>(t0) == loop, "Error in oq0 output");
            ASSERT(oq1.try_get(t1) && std::get<0>(t1) == loop && std::get<1>(t1) == loop, "Error in oq1 output");
            ASSERT(!oq0.try_get(t0), "extra object in output queue oq0");
            ASSERT(!oq1.try_get(t0), "extra object in output queue oq1");
        }
        // attach qnq to iq1
        // qnp and qnq should be empty
        tbb::flow::make_edge( iq1, qnq );
        g.wait_for_all();
        {
            int i;
            ASSERT(!qnp.try_get(i), "iq0 still had value in it");
            ASSERT(!qnq.try_get(i), "iq1 still had value in it");
        }
        tbb::flow::remove_edge( iq0, qnp );
        tbb::flow::remove_edge( iq1, qnq );
    } // for ( int loop ...

    // Now we'll put [4 .. nValues - 1] in iq0, and then put [4 .. nValues - 1] in iq1 in
    // a different order.  We should see tuples in the output queues in the order we inserted
    // the integers into iq1.

    const int nValues = 100;
    const int nIncr = 31;  // relatively prime to nValues

    for(int loop = 4; loop < 4+nValues; ++loop) {
        // place one item in iq0
        ASSERT(iq0.try_put(loop), "Error putting to iq1"); 
        g.wait_for_all();
        {
            OQType t3;
            ASSERT(!oq0.try_get(t3), "Object in output queue");
            ASSERT(!oq1.try_get(t3), "Object in output queue");
        }
    } // for ( int loop ...

    for(int loop = 1; loop <= nValues; ++loop) {
        int lp1 = 4 + (loop * nIncr)%nValues;
        // place item in iq1
        ASSERT(iq1.try_put(lp1), "Error putting to iq1");
        // oq0, oq1 should have items
        g.wait_for_all();
        {
            OQType t0;
            OQType t1;
            ASSERT(oq0.try_get(t0) && std::get<0>(t0) == lp1 && std::get<1>(t0) == lp1, "Error in oq0 output");
            ASSERT(oq1.try_get(t1) && std::get<0>(t1) == lp1 && std::get<1>(t1) == lp1, "Error in oq1 output");
            ASSERT(!oq0.try_get(t0), "extra object in output queue oq0");
            ASSERT(!oq1.try_get(t0), "extra object in output queue oq1");
        }
    } // for ( int loop ...
}

int TestMain() {
#if __TBB_USE_TBB_TUPLE
    REMARK("  Using TBB tuple\n");
#else
    REMARK("  Using platform tuple\n");
#endif
    test_input_port_policies<tbb::flow::reserving>();
    test_input_port_policies<tbb::flow::queueing>();
    test_input_port_policies<tbb::flow::tag_matching>();

   for (int p = 0; p < 2; ++p) {
       REMARK("reserving\n");
       generate_test<serial_test, std::tuple<float, double>, tbb::flow::reserving >::do_test();
       generate_test<serial_test, std::tuple<float, double, int, long>, tbb::flow::reserving >::do_test();
#if __TBB_VARIADIC_MAX >= 6
       generate_test<serial_test, std::tuple<double, double, int, long, int, short>, tbb::flow::reserving >::do_test();
#endif
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 8
       generate_test<serial_test, std::tuple<float, double, double, double, float, int, float, long>, tbb::flow::reserving >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 10
       generate_test<serial_test, std::tuple<float, double, int, double, double, float, long, int, float, long>, tbb::flow::reserving >::do_test();
#endif
#endif
       generate_test<parallel_test, std::tuple<float, double>, tbb::flow::reserving >::do_test();
       generate_test<parallel_test, std::tuple<float, int, long>, tbb::flow::reserving >::do_test();
       generate_test<parallel_test, std::tuple<double, double, int, int, short>, tbb::flow::reserving >::do_test();
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 7
       generate_test<parallel_test, std::tuple<float, int, double, float, long, float, long>, tbb::flow::reserving >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 9
       generate_test<parallel_test, std::tuple<float, double, int, double, double, long, int, float, long>, tbb::flow::reserving >::do_test();
#endif
#endif
       REMARK("queueing\n");
       generate_test<serial_test, std::tuple<float, double>, tbb::flow::queueing >::do_test();
       generate_test<serial_test, std::tuple<float, double, int, long>, tbb::flow::queueing >::do_test();
#if __TBB_VARIADIC_MAX >= 6
       generate_test<serial_test, std::tuple<double, double, int, long, int, short>, tbb::flow::queueing >::do_test();
#endif
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 8
       generate_test<serial_test, std::tuple<float, double, double, double, float, int, float, long>, tbb::flow::queueing >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 10
       generate_test<serial_test, std::tuple<float, double, int, double, double, float, long, int, float, long>, tbb::flow::queueing >::do_test();
#endif
#endif
       generate_test<parallel_test, std::tuple<float, double>, tbb::flow::queueing >::do_test();
       generate_test<parallel_test, std::tuple<float, int, long>, tbb::flow::queueing >::do_test();
       generate_test<parallel_test, std::tuple<double, double, int, int, short>, tbb::flow::queueing >::do_test();
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 7
       generate_test<parallel_test, std::tuple<float, int, double, float, long, float, long>, tbb::flow::queueing >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 9
       generate_test<parallel_test, std::tuple<float, double, int, double, double, long, int, float, long>, tbb::flow::queueing >::do_test();
#endif
#endif
       REMARK("tag_matching\n");
       generate_test<serial_test, std::tuple<float, double>, tbb::flow::tag_matching >::do_test();
       generate_test<serial_test, std::tuple<float, double, int, long>, tbb::flow::tag_matching >::do_test();
#if __TBB_VARIADIC_MAX >= 6
       generate_test<serial_test, std::tuple<double, double, int, long, int, short>, tbb::flow::tag_matching >::do_test();
#endif
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 8
       generate_test<serial_test, std::tuple<float, double, double, double, float, int, float, long>, tbb::flow::tag_matching >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 10
       generate_test<serial_test, std::tuple<float, double, int, double, double, float, long, int, float, long>, tbb::flow::tag_matching >::do_test();
#endif
#endif
       generate_test<parallel_test, std::tuple<float, double>, tbb::flow::tag_matching >::do_test();
       generate_test<parallel_test, std::tuple<float, int, long>, tbb::flow::tag_matching >::do_test();
       generate_test<parallel_test, std::tuple<double, double, int, int, short>, tbb::flow::tag_matching >::do_test();
#if COMPREHENSIVE_TEST
#if __TBB_VARIADIC_MAX >= 7
       generate_test<parallel_test, std::tuple<float, int, double, float, long, float, long>, tbb::flow::tag_matching >::do_test();
#endif
#if __TBB_VARIADIC_MAX >= 9
       generate_test<parallel_test, std::tuple<float, double, int, double, double, long, int, float, long>, tbb::flow::tag_matching >::do_test();
#endif
#endif

       generate_recirc_test<std::tuple<float,double> >::do_test();
       generate_recirc_test<std::tuple<double, double, int, int, short> >::do_test();
   }
   return Harness::Done;
}
#else  // __SUNPRO_CC

int TestMain() {
    return Harness::Skipped;
}

#endif  // SUNPRO_CC
