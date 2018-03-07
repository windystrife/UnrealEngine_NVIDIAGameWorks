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

#define NOMINMAX
#include "harness_defs.h"
#include "test_concurrent_queue.h"
#include "tbb/concurrent_queue.h"
#include "tbb/tick_count.h"
#include "harness.h"
#include "harness_allocator.h"

#if _MSC_VER==1500 && !__INTEL_COMPILER
    // VS2008/VC9 seems to have an issue; limits pull in math.h
    #pragma warning( push )
    #pragma warning( disable: 4985 )
#endif
#include <limits>
#if _MSC_VER==1500 && !__INTEL_COMPILER
    #pragma warning( pop )
#endif


static tbb::atomic<long> FooConstructed;
static tbb::atomic<long> FooDestroyed;

class Foo {
    enum state_t{
        LIVE=0x1234,
        DEAD=0xDEAD
    };
    state_t state;
public:
    int thread_id;
    int serial;
    Foo() : state(LIVE), thread_id(0), serial(0) {
        ++FooConstructed;
    }
    Foo( const Foo& item ) : state(LIVE) {
        ASSERT( item.state==LIVE, NULL );
        ++FooConstructed;
        thread_id = item.thread_id;
        serial = item.serial;
    }
    ~Foo() {
        ASSERT( state==LIVE, NULL );
        ++FooDestroyed;
        state=DEAD;
        thread_id=0xDEAD;
        serial=0xDEAD;
    }
    void operator=( const Foo& item ) {
        ASSERT( item.state==LIVE, NULL );
        ASSERT( state==LIVE, NULL );
        thread_id = item.thread_id;
        serial = item.serial;
    }
    bool is_const() {return false;}
    bool is_const() const {return true;}
    static void clear_counters() { FooConstructed = 0; FooDestroyed = 0; }
    static long get_n_constructed() { return FooConstructed; }
    static long get_n_destroyed() { return FooDestroyed; }
};

// problem size
static const int N = 50000;     // # of bytes

#if TBB_USE_EXCEPTIONS
//! Exception for concurrent_queue
class Foo_exception : public std::bad_alloc {
public:
    virtual const char *what() const throw() { return "out of Foo limit"; }
    virtual ~Foo_exception() throw() {}
};

static tbb::atomic<long> FooExConstructed;
static tbb::atomic<long> FooExDestroyed;
static tbb::atomic<long> serial_source;
static long MaxFooCount = 0;
static const long Threshold = 400;

class FooEx {
    enum state_t{
        LIVE=0x1234,
        DEAD=0xDEAD
    };
    state_t state;
public:
    int serial;
    FooEx() : state(LIVE) {
        ++FooExConstructed;
        serial = serial_source++;
    }

    FooEx( const FooEx& item ) : state(LIVE) {
        ++FooExConstructed;
        if( MaxFooCount && (FooExConstructed-FooExDestroyed) >= MaxFooCount ) // in push()
            throw Foo_exception();
        serial = item.serial;
    }
    ~FooEx() {
        ASSERT( state==LIVE, NULL );
        ++FooExDestroyed;
        state=DEAD;
        serial=0xDEAD;
    }
    void operator=( FooEx& item ) {
        ASSERT( item.state==LIVE, NULL );
        ASSERT( state==LIVE, NULL );
        serial = item.serial;
        if( MaxFooCount==2*Threshold && (FooExConstructed-FooExDestroyed) <= MaxFooCount/4 ) // in pop()
            throw Foo_exception();
    }
} ;
#endif /* TBB_USE_EXCEPTIONS */

const size_t MAXTHREAD = 256;

static int Sum[MAXTHREAD];

//! Count of various pop operations
/** [0] = pop_if_present that failed
    [1] = pop_if_present that succeeded
    [2] = pop */
static tbb::atomic<long> PopKind[3];

const int M = 10000;

#if TBB_DEPRECATED
#define CALL_BLOCKING_POP(q,v) (q)->pop(v)
#define CALL_TRY_POP(q,v,i) (((i)&0x2)?q->try_pop(v):q->pop_if_present(v))
#else
#define CALL_BLOCKING_POP(q,v) while( !(q)->try_pop(v) ) __TBB_Yield()
#define CALL_TRY_POP(q,v,i) q->try_pop(v)
#endif

template<typename CQ,typename T>
struct Body: NoAssign {
    CQ* queue;
    const int nthread;
    Body( int nthread_ ) : nthread(nthread_) {}
    void operator()( int thread_id ) const {
        long pop_kind[3] = {0,0,0};
        int serial[MAXTHREAD+1];
        memset( serial, 0, nthread*sizeof(int) );
        ASSERT( thread_id<nthread, NULL );

        long sum = 0;
        for( long j=0; j<M; ++j ) {
            T f;
            f.thread_id = 0xDEAD;
            f.serial = 0xDEAD;
            bool prepopped = false;
            if( j&1 ) {
                prepopped = CALL_TRY_POP(queue,f,j);
                ++pop_kind[prepopped];
            }
            T g;
            g.thread_id = thread_id;
            g.serial = j+1;
            queue->push( g );
            if( !prepopped ) {
                CALL_BLOCKING_POP(queue,f);
                ++pop_kind[2];
            }
            ASSERT( f.thread_id<=nthread, NULL );
            ASSERT( f.thread_id==nthread || serial[f.thread_id]<f.serial, "partial order violation" );
            serial[f.thread_id] = f.serial;
            sum += f.serial-1;
        }
        Sum[thread_id] = sum;
        for( int k=0; k<3; ++k )
            PopKind[k] += pop_kind[k];
    }
};

#if !TBB_DEPRECATED
// Define wrapper classes to test tbb::concurrent_queue<T>
template<typename T, typename A = tbb::cache_aligned_allocator<T> >
class ConcQWithSizeWrapper : public tbb::concurrent_queue<T> {
public:
    ConcQWithSizeWrapper() {}
    template<typename InputIterator>
    ConcQWithSizeWrapper( InputIterator begin, InputIterator end, const A& a = A()) : tbb::concurrent_queue<T>(begin,end,a) {}
    size_t size() const { return this->unsafe_size(); }
};

template<typename T>
class ConcQPushPopWrapper : public tbb::concurrent_queue<T> {
public:
    ConcQPushPopWrapper() : my_capacity( size_t(-1)/(sizeof(void*)+sizeof(T)) ) {}
    size_t size() const { return this->unsafe_size(); }
    void   set_capacity( const ptrdiff_t n ) { my_capacity = n; }
    bool   try_push( const T& source ) { return this->push( source ); }
    bool   try_pop( T& dest ) { return this->tbb::concurrent_queue<T>::try_pop( dest ); }
    size_t my_capacity;
};

template<typename T>
class ConcQWithCapacity : public tbb::concurrent_queue<T> {
public:
    ConcQWithCapacity() : my_capacity( size_t(-1)/(sizeof(void*)+sizeof(T)) ) {}
    size_t size() const { return this->unsafe_size(); }
    size_t capacity() const { return my_capacity; }
    void   set_capacity( const int n ) { my_capacity = n; }
    bool   try_push( const T& source ) { this->push( source ); return (size_t)source.serial<my_capacity; }
    //bool   push_if_not_full( const T& source ) { return try_push(source); }
    bool   try_pop( T& dest ) { this->tbb::concurrent_queue<T>::try_pop( dest ); return (size_t)dest.serial<my_capacity; }
    //void   pop( T& dest ) { this->try_pop( dest ); }
    size_t my_capacity;
};
#endif /* !TBB_DEPRECATED */

template<typename CQ, typename T>
void TestPushPop( size_t prefill, ptrdiff_t capacity, int nthread ) {
    ASSERT( nthread>0, "nthread must be positive" );
    ptrdiff_t signed_prefill = ptrdiff_t(prefill);
    if( signed_prefill+1>=capacity )
        return;
    bool success = false;
    for( int k=0; k<3; ++k )
        PopKind[k] = 0;
    for( int trial=0; !success; ++trial ) {
        T::clear_counters();
        Body<CQ,T> body(nthread);
        CQ queue;
        queue.set_capacity( capacity );
        body.queue = &queue;
        for( size_t i=0; i<prefill; ++i ) {
            T f;
            f.thread_id = nthread;
            f.serial = 1+int(i);
            queue.push(f);
            ASSERT( unsigned(queue.size())==i+1, NULL );
            ASSERT( !queue.empty(), NULL );
        }
        tbb::tick_count t0 = tbb::tick_count::now();
        NativeParallelFor( nthread, body );
        tbb::tick_count t1 = tbb::tick_count::now();
        double timing = (t1-t0).seconds();
        REMARK("prefill=%d capacity=%d threads=%d time = %g = %g nsec/operation\n", int(prefill), int(capacity), nthread, timing, timing/(2*M*nthread)*1.E9);
        int sum = 0;
        for( int k=0; k<nthread; ++k )
            sum += Sum[k];
        int expected = int(nthread*((M-1)*M/2) + ((prefill-1)*prefill)/2);
        for( int i=int(prefill); --i>=0; ) {
            ASSERT( !queue.empty(), NULL );
            T f;
            bool result = queue.try_pop(f);
            ASSERT( result, NULL );
            ASSERT( int(queue.size())==i, NULL );
            sum += f.serial-1;
        }
        ASSERT( queue.empty(), NULL );
        ASSERT( queue.size()==0, NULL );
        if( sum!=expected )
            REPORT("sum=%d expected=%d\n",sum,expected);
        ASSERT( T::get_n_constructed()==T::get_n_destroyed(), NULL );
        // TODO: checks by counting allocators

        success = true;
        if( nthread>1 && prefill==0 ) {
            // Check that pop_if_present got sufficient exercise
            for( int k=0; k<2; ++k ) {
#if (_WIN32||_WIN64)
                // The TBB library on Windows seems to have a tough time generating
                // the desired interleavings for pop_if_present, so the code tries longer, and settles
                // for fewer desired interleavings.
                const int max_trial = 100;
                const int min_requirement = 20;
#else
                const int min_requirement = 100;
                const int max_trial = 20;
#endif /* _WIN32||_WIN64 */
                if( PopKind[k]<min_requirement ) {
                    if( trial>=max_trial ) {
                        if( Verbose )
                            REPORT("Warning: %d threads had only %ld pop_if_present operations %s after %d trials (expected at least %d). "
                               "This problem may merely be unlucky scheduling. "
                               "Investigate only if it happens repeatedly.\n",
                               nthread, long(PopKind[k]), k==0?"failed":"succeeded", max_trial, min_requirement);
                        else
                            REPORT("Warning: the number of %s pop_if_present operations is less than expected for %d threads. Investigate if it happens repeatedly.\n",
                               k==0?"failed":"succeeded", nthread );

                    } else {
                        success = false;
                    }
               }
            }
        }
    }
}

class Bar {
    enum state_t {
        LIVE=0x1234,
        DEAD=0xDEAD
    };
    state_t state;
public:
    ptrdiff_t my_id;
    Bar() : state(LIVE), my_id(-1) {}
    Bar(size_t _i) : state(LIVE), my_id(_i) {}
    Bar( const Bar& a_bar ) : state(LIVE) {
        ASSERT( a_bar.state==LIVE, NULL );
        my_id = a_bar.my_id;
    }
    ~Bar() {
        ASSERT( state==LIVE, NULL );
        state = DEAD;
        my_id = DEAD;
    }
    void operator=( const Bar& a_bar ) {
        ASSERT( a_bar.state==LIVE, NULL );
        ASSERT( state==LIVE, NULL );
        my_id = a_bar.my_id;
    }
    friend bool operator==(const Bar& bar1, const Bar& bar2 ) ;
} ;

bool operator==(const Bar& bar1, const Bar& bar2) {
    ASSERT( bar1.state==Bar::LIVE, NULL );
    ASSERT( bar2.state==Bar::LIVE, NULL );
    return bar1.my_id == bar2.my_id;
}

class BarIterator
{
    Bar* bar_ptr;
    BarIterator(Bar* bp_) : bar_ptr(bp_) {}
public:
    ~BarIterator() {}
    BarIterator& operator=( const BarIterator& other ) {
        bar_ptr = other.bar_ptr;
        return *this;
    }
    Bar& operator*() const {
        return *bar_ptr;
    }
    BarIterator& operator++() {
        ++bar_ptr;
        return *this;
    }
    Bar* operator++(int) {
        Bar* result = &operator*();
        operator++();
        return result;
    }
    friend bool operator==(const BarIterator& bia, const BarIterator& bib) ;
    friend bool operator!=(const BarIterator& bia, const BarIterator& bib) ;
    template<typename CQ, typename T, typename TIter, typename CQ_EX, typename T_EX>
    friend void TestConstructors ();
} ;

bool operator==(const BarIterator& bia, const BarIterator& bib) {
    return bia.bar_ptr==bib.bar_ptr;
}

bool operator!=(const BarIterator& bia, const BarIterator& bib) {
    return bia.bar_ptr!=bib.bar_ptr;
}

#if TBB_USE_EXCEPTIONS
class Bar_exception : public std::bad_alloc {
public:
    virtual const char *what() const throw() { return "making the entry invalid"; }
    virtual ~Bar_exception() throw() {}
};

class BarEx {
    enum state_t {
        LIVE=0x1234,
        DEAD=0xDEAD
    };
    static int count;
public:
    state_t state;
    typedef enum {
        PREPARATION,
        COPY_CONSTRUCT
    } mode_t;
    static mode_t mode;
    ptrdiff_t my_id;
    ptrdiff_t my_tilda_id;
    static int button;
    BarEx() : state(LIVE), my_id(-1), my_tilda_id(-1) {}
    BarEx(size_t _i) : state(LIVE), my_id(_i), my_tilda_id(my_id^(-1)) {}
    BarEx( const BarEx& a_bar ) : state(LIVE) {
        ASSERT( a_bar.state==LIVE, NULL );
        my_id = a_bar.my_id;
        if( mode==PREPARATION )
            if( !( ++count % 100 ) )
                throw Bar_exception();
        my_tilda_id = a_bar.my_tilda_id;
    }
    ~BarEx() {
        ASSERT( state==LIVE, NULL );
        state = DEAD;
        my_id = DEAD;
    }
    static void set_mode( mode_t m ) { mode = m; }
    void operator=( const BarEx& a_bar ) {
        ASSERT( a_bar.state==LIVE, NULL );
        ASSERT( state==LIVE, NULL );
        my_id = a_bar.my_id;
        my_tilda_id = a_bar.my_tilda_id;
    }
    friend bool operator==(const BarEx& bar1, const BarEx& bar2 ) ;
} ;

int    BarEx::count = 0;
BarEx::mode_t BarEx::mode = BarEx::PREPARATION;

bool operator==(const BarEx& bar1, const BarEx& bar2) {
    ASSERT( bar1.state==BarEx::LIVE, NULL );
    ASSERT( bar2.state==BarEx::LIVE, NULL );
    ASSERT( (bar1.my_id ^ bar1.my_tilda_id) == -1, NULL );
    ASSERT( (bar2.my_id ^ bar2.my_tilda_id) == -1, NULL );
    return bar1.my_id==bar2.my_id && bar1.my_tilda_id==bar2.my_tilda_id;
}
#endif /* TBB_USE_EXCEPTIONS */

#if TBB_DEPRECATED

#if __INTEL_COMPILER==1200 && _MSC_VER==1600
// A workaround due to ICL 12.0 with /Qvc10 generating buggy code in TestIterator
#define CALL_BEGIN(q,i) q.begin()
#define CALL_END(q,i)   q.end()
#else
#define CALL_BEGIN(q,i) (((i)&0x1)?q.begin():q.unsafe_begin())
#define CALL_END(q,i)   (((i)&0x1)?q.end():q.unsafe_end())
#endif

#else
#define CALL_BEGIN(q,i) q.unsafe_begin()
#define CALL_END(q,i)   q.unsafe_end()
#endif /* TBB_DEPRECATED */

template<typename CQ, typename T, typename TIter, typename CQ_EX, typename T_EX>
void TestConstructors ()
{
    CQ src_queue;
    typename CQ::const_iterator dqb;
    typename CQ::const_iterator dqe;
    typename CQ::const_iterator iter;

    for( size_t size=0; size<1001; ++size ) {
        for( size_t i=0; i<size; ++i )
            src_queue.push(T(i+(i^size)));
        typename CQ::const_iterator sqb( CALL_BEGIN(src_queue,size) );
        typename CQ::const_iterator sqe( CALL_END(src_queue,size));

        CQ dst_queue(sqb, sqe);

        ASSERT(src_queue.size()==dst_queue.size(), "different size");

        src_queue.clear();
    }

    T bar_array[1001];
    for( size_t size=0; size<1001; ++size ) {
        for( size_t i=0; i<size; ++i )
            bar_array[i] = T(i+(i^size));

        const TIter sab(bar_array+0);
        const TIter sae(bar_array+size);

        CQ dst_queue2(sab, sae);

        ASSERT( size==unsigned(dst_queue2.size()), NULL );
        ASSERT( sab==TIter(bar_array+0), NULL );
        ASSERT( sae==TIter(bar_array+size), NULL );

        dqb = CALL_BEGIN(dst_queue2,size);
        dqe = CALL_END(dst_queue2,size);
        TIter v_iter(sab);
        for( ; dqb != dqe; ++dqb, ++v_iter )
            ASSERT( *dqb == *v_iter, "unexpected element" );
        ASSERT( v_iter==sae, "different size?" );
    }

    src_queue.clear();

    CQ dst_queue3( src_queue );
    ASSERT( src_queue.size()==dst_queue3.size(), NULL );
    ASSERT( 0==dst_queue3.size(), NULL );

    int k=0;
    for( size_t i=0; i<1001; ++i ) {
        T tmp_bar;
        src_queue.push(T(++k));
        src_queue.push(T(++k));
        src_queue.try_pop(tmp_bar);

        CQ dst_queue4( src_queue );

        ASSERT( src_queue.size()==dst_queue4.size(), NULL );

        dqb = CALL_BEGIN(dst_queue4,i);
        dqe = CALL_END(dst_queue4,i);
        iter = CALL_BEGIN(src_queue,i);

        for( ; dqb != dqe; ++dqb, ++iter )
            ASSERT( *dqb == *iter, "unexpected element" );

        ASSERT( iter==CALL_END(src_queue,i), "different size?" );
    }

    CQ dst_queue5( src_queue );

    ASSERT( src_queue.size()==dst_queue5.size(), NULL );
    dqb = dst_queue5.unsafe_begin();
    dqe = dst_queue5.unsafe_end();
    iter = src_queue.unsafe_begin();
    for( ; dqb != dqe; ++dqb, ++iter )
        ASSERT( *dqb == *iter, "unexpected element" );

    for( size_t i=0; i<100; ++i) {
        T tmp_bar;
        src_queue.push(T(i+1000));
        src_queue.push(T(i+1000));
        src_queue.try_pop(tmp_bar);

        dst_queue5.push(T(i+1000));
        dst_queue5.push(T(i+1000));
        dst_queue5.try_pop(tmp_bar);
    }

    ASSERT( src_queue.size()==dst_queue5.size(), NULL );
    dqb = dst_queue5.unsafe_begin();
    dqe = dst_queue5.unsafe_end();
    iter = src_queue.unsafe_begin();
    for( ; dqb != dqe; ++dqb, ++iter )
        ASSERT( *dqb == *iter, "unexpected element" );
    ASSERT( iter==src_queue.unsafe_end(), "different size?" );

#if __TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN || __TBB_PLACEMENT_NEW_EXCEPTION_SAFETY_BROKEN
    REPORT("Known issue: part of the constructor test is skipped.\n");
#elif TBB_USE_EXCEPTIONS
    k = 0;
    typename CQ_EX::size_type n_elements=0;
    CQ_EX src_queue_ex;
    for( size_t size=0; size<1001; ++size ) {
        T_EX tmp_bar_ex;
        typename CQ_EX::size_type n_successful_pushes=0;
        T_EX::set_mode( T_EX::PREPARATION );
        try {
            src_queue_ex.push(T_EX(k+(k^size)));
            ++n_successful_pushes;
        } catch (...) {
        }
        ++k;
        try {
            src_queue_ex.push(T_EX(k+(k^size)));
            ++n_successful_pushes;
        } catch (...) {
        }
        ++k;
        src_queue_ex.try_pop(tmp_bar_ex);
        n_elements += (n_successful_pushes - 1);
        ASSERT( src_queue_ex.size()==n_elements, NULL);

        T_EX::set_mode( T_EX::COPY_CONSTRUCT );
        CQ_EX dst_queue_ex( src_queue_ex );

        ASSERT( src_queue_ex.size()==dst_queue_ex.size(), NULL );

        typename CQ_EX::const_iterator dqb_ex  = CALL_BEGIN(dst_queue_ex, size);
        typename CQ_EX::const_iterator dqe_ex  = CALL_END(dst_queue_ex, size);
        typename CQ_EX::const_iterator iter_ex = CALL_BEGIN(src_queue_ex, size);

        for( ; dqb_ex != dqe_ex; ++dqb_ex, ++iter_ex )
            ASSERT( *dqb_ex == *iter_ex, "unexpected element" );
        ASSERT( iter_ex==CALL_END(src_queue_ex,size), "different size?" );
    }
#endif /* TBB_USE_EXCEPTIONS */
}

template<typename Iterator1, typename Iterator2>
void TestIteratorAux( Iterator1 i, Iterator2 j, int size ) {
    // Now test iteration
    Iterator1 old_i;
    for( int k=0; k<size; ++k ) {
        ASSERT( i!=j, NULL );
        ASSERT( !(i==j), NULL );
        Foo f;
        if( k&1 ) {
            // Test pre-increment
            f = *old_i++;
            // Test assignment
            i = old_i;
        } else {
            // Test post-increment
            f=*i++;
            if( k<size-1 ) {
                // Test "->"
                ASSERT( k+2==i->serial, NULL );
            }
            // Test assignment
            old_i = i;
        }
        ASSERT( k+1==f.serial, NULL );
    }
    ASSERT( !(i!=j), NULL );
    ASSERT( i==j, NULL );
}

template<typename Iterator1, typename Iterator2>
void TestIteratorAssignment( Iterator2 j ) {
    Iterator1 i(j);
    ASSERT( i==j, NULL );
    ASSERT( !(i!=j), NULL );
    Iterator1 k;
    k = j;
    ASSERT( k==j, NULL );
    ASSERT( !(k!=j), NULL );
}

template<typename Iterator, typename T>
void TestIteratorTraits() {
    AssertSameType( static_cast<typename Iterator::difference_type*>(0), static_cast<ptrdiff_t*>(0) );
    AssertSameType( static_cast<typename Iterator::value_type*>(0), static_cast<T*>(0) );
    AssertSameType( static_cast<typename Iterator::pointer*>(0), static_cast<T**>(0) );
    AssertSameType( static_cast<typename Iterator::iterator_category*>(0), static_cast<std::forward_iterator_tag*>(0) );
    T x;
    typename Iterator::reference xr = x;
    typename Iterator::pointer xp = &x;
    ASSERT( &xr==xp, NULL );
}

//! Test the iterators for concurrent_queue
template<typename CQ>
void TestIterator() {
    CQ queue;
    const CQ& const_queue = queue;
    for( int j=0; j<500; ++j ) {
        TestIteratorAux( CALL_BEGIN(queue,j)      , CALL_END(queue,j)      , j );
        TestIteratorAux( CALL_BEGIN(const_queue,j), CALL_END(const_queue,j), j );
        TestIteratorAux( CALL_BEGIN(const_queue,j), CALL_END(queue,j)      , j );
        TestIteratorAux( CALL_BEGIN(queue,j)      , CALL_END(const_queue,j), j );
        Foo f;
        f.serial = j+1;
        queue.push(f);
    }
    TestIteratorAssignment<typename CQ::const_iterator>( const_queue.unsafe_begin() );
    TestIteratorAssignment<typename CQ::const_iterator>( queue.unsafe_begin() );
    TestIteratorAssignment<typename CQ::iterator>( queue.unsafe_begin() );
    TestIteratorTraits<typename CQ::const_iterator, const Foo>();
    TestIteratorTraits<typename CQ::iterator, Foo>();
}

template<typename CQ>
void TestConcurrentQueueType() {
    AssertSameType( typename CQ::value_type(), Foo() );
    Foo f;
    const Foo g;
    typename CQ::reference r = f;
    ASSERT( &r==&f, NULL );
    ASSERT( !r.is_const(), NULL );
    typename CQ::const_reference cr = g;
    ASSERT( &cr==&g, NULL );
    ASSERT( cr.is_const(), NULL );
}

template<typename CQ, typename T>
void TestEmptyQueue() {
    const CQ queue;
    ASSERT( queue.size()==0, NULL );
    ASSERT( queue.capacity()>0, NULL );
    ASSERT( size_t(queue.capacity())>=size_t(-1)/(sizeof(void*)+sizeof(T)), NULL );
}

#ifdef CALL_TRY_POP
#undef CALL_TRY_POP
#endif
#if TBB_DEPRECATED
#define CALL_TRY_PUSH(q,f,i) (((i)&0x1)?(q).push_if_not_full(f):(q).try_push(f))
#define CALL_TRY_POP(q,f) (q).pop_if_present(f)
#else
#define CALL_TRY_PUSH(q,f,i) (q).try_push(f)
#define CALL_TRY_POP(q,f) (q).try_pop(f)
#endif

template<typename CQ,typename T>
void TestFullQueue() {
    for( int n=0; n<10; ++n ) {
        T::clear_counters();
        CQ queue;
        queue.set_capacity(n);
        for( int i=0; i<=n; ++i ) {
            T f;
            f.serial = i;
            bool result = CALL_TRY_PUSH( queue, f, i );
            ASSERT( result==(i<n), NULL );
        }
        for( int i=0; i<=n; ++i ) {
            T f;
            bool result = CALL_TRY_POP( queue, f );
            ASSERT( result==(i<n), NULL );
            ASSERT( !result || f.serial==i, NULL );
        }
        ASSERT( T::get_n_constructed()==T::get_n_destroyed(), NULL );
    }
}

#if TBB_DEPRECATED
#define CALL_PUSH_IF_NOT_FULL(q,v,i) (((i)&0x1)?q.push_if_not_full(v):(q.push(v), true))
#else
#define CALL_PUSH_IF_NOT_FULL(q,v,i) (q.push(v), true)
#endif

template<typename CQ>
void TestClear() {
    FooConstructed = 0;
    FooDestroyed = 0;
    const unsigned int n=5;

    CQ queue;
    const int q_capacity=10;
    queue.set_capacity(q_capacity);
    for( size_t i=0; i<n; ++i ) {
        Foo f;
        f.serial = int(i);
        bool result = CALL_PUSH_IF_NOT_FULL(queue, f, i);
        ASSERT( result, NULL );
    }
    ASSERT( unsigned(queue.size())==n, NULL );
    queue.clear();
    ASSERT( queue.size()==0, NULL );
    for( size_t i=0; i<n; ++i ) {
        Foo f;
        f.serial = int(i);
        bool result = CALL_PUSH_IF_NOT_FULL(queue, f, i);
        ASSERT( result, NULL );
    }
    ASSERT( unsigned(queue.size())==n, NULL );
    queue.clear();
    ASSERT( queue.size()==0, NULL );
    for( size_t i=0; i<n; ++i ) {
        Foo f;
        f.serial = int(i);
        bool result = CALL_PUSH_IF_NOT_FULL(queue, f, i);
        ASSERT( result, NULL );
    }
    ASSERT( unsigned(queue.size())==n, NULL );
}

#if TBB_DEPRECATED
template<typename T>
struct TestNegativeQueueBody: NoAssign {
    tbb::concurrent_queue<T>& queue;
    const int nthread;
    TestNegativeQueueBody( tbb::concurrent_queue<T>& q, int n ) : queue(q), nthread(n) {}
    void operator()( int k ) const {
        if( k==0 ) {
            int number_of_pops = nthread-1;
            // Wait for all pops to pend.
            while( queue.size()>-number_of_pops ) {
                __TBB_Yield();
            }
            for( int i=0; ; ++i ) {
                ASSERT( queue.size()==i-number_of_pops, NULL );
                ASSERT( queue.empty()==(queue.size()<=0), NULL );
                if( i==number_of_pops ) break;
                // Satisfy another pop
                queue.push( T() );
            }
        } else {
            // Pop item from queue
            T item;
            queue.pop(item);
        }
    }
};

//! Test a queue with a negative size.
template<typename T>
void TestNegativeQueue( int nthread ) {
    tbb::concurrent_queue<T> queue;
    NativeParallelFor( nthread, TestNegativeQueueBody<T>(queue,nthread) );
}
#endif /* TBB_DEPRECATED */

#if TBB_USE_EXCEPTIONS
template<typename CQ,typename A1,typename A2,typename T>
void TestExceptionBody() {
    enum methods {
        m_push = 0,
        m_pop
    };

    REMARK("Testing exception safety\n");
    MaxFooCount = 5;
    // verify 'clear()' on exception; queue's destructor calls its clear()
    // Do test on queues of two different types at the same time to
    // catch problem with incorrect sharing between templates.
    {
        CQ queue0;
        tbb::concurrent_queue<int,A1> queue1;
        for( int i=0; i<2; ++i ) {
            bool caught = false;
            try {
                A2::init_counters();
                A2::set_limits(N/2);
                for( int k=0; k<N; k++ ) {
                    if( i==0 )
                        queue0.push( T() );
                    else
                        queue1.push( k );
                }
            } catch (...) {
                caught = true;
            }
            ASSERT( caught, "call to push should have thrown exception" );
        }
    }
    REMARK("... queue destruction test passed\n");

    try {
        int n_pushed=0, n_popped=0;
        for(int t = 0; t <= 1; t++)// exception type -- 0 : from allocator(), 1 : from Foo's constructor
        {
            CQ queue_test;
            for( int m=m_push; m<=m_pop; m++ ) {
                // concurrent_queue internally rebinds the allocator to one with 'char'
                A2::init_counters();

                if(t) MaxFooCount = MaxFooCount + 400;
                else A2::set_limits(N/2);

                try {
                    switch(m) {
                    case m_push:
                            for( int k=0; k<N; k++ ) {
                                queue_test.push( T() );
                                n_pushed++;
                            }
                            break;
                    case m_pop:
                            n_popped=0;
                            for( int k=0; k<n_pushed; k++ ) {
                                T elt;
                                queue_test.try_pop( elt );
                                n_popped++;
                            }
                            n_pushed = 0;
                            A2::set_limits(); 
                            break;
                    }
                    if( !t && m==m_push ) ASSERT(false, "should throw an exception");
                } catch ( Foo_exception & ) {
                    switch(m) {
                    case m_push: {
                                ASSERT( ptrdiff_t(queue_test.size())==n_pushed, "incorrect queue size" );
                                long tc = MaxFooCount;
                                MaxFooCount = 0;
                                for( int k=0; k<(int)tc; k++ ) {
                                    queue_test.push( T() );
                                    n_pushed++;
                                }
                                MaxFooCount = tc;
                            }
                            break;
                    case m_pop:
                            MaxFooCount = 0; // disable exception
                            n_pushed -= (n_popped+1); // including one that threw an exception
                            ASSERT( n_pushed>=0, "n_pushed cannot be less than 0" );
                            for( int k=0; k<1000; k++ ) {
                                queue_test.push( T() );
                                n_pushed++;
                            }
                            ASSERT( !queue_test.empty(), "queue must not be empty" );
                            ASSERT( ptrdiff_t(queue_test.size())==n_pushed, "queue size must be equal to n pushed" );
                            for( int k=0; k<n_pushed; k++ ) {
                                T elt;
                                queue_test.try_pop( elt );
                            }
                            ASSERT( queue_test.empty(), "queue must be empty" );
                            ASSERT( queue_test.size()==0, "queue must be empty" );
                            break;
                    }
                } catch ( std::bad_alloc & ) {
                    A2::set_limits(); // disable exception from allocator
                    size_t size = queue_test.size();
                    switch(m) {
                    case m_push:
                            ASSERT( size>0, "incorrect queue size");
                            break;
                    case m_pop:
                            if( !t ) ASSERT( false, "should not throw an exceptin" );
                            break;
                    }
                }
                REMARK("... for t=%d and m=%d, exception test passed\n", t, m);
            }
        }
    } catch(...) {
        ASSERT(false, "unexpected exception");
    }
}
#endif /* TBB_USE_EXCEPTIONS */

void TestExceptions() {
#if __TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN
    REPORT("Known issue: exception safety test is skipped.\n");
#elif TBB_USE_EXCEPTIONS
    typedef static_counting_allocator<std::allocator<FooEx>, size_t> allocator_t;
    typedef static_counting_allocator<std::allocator<char>, size_t> allocator_char_t;
#if !TBB_DEPRECATED
    TestExceptionBody<ConcQWithSizeWrapper<FooEx, allocator_t>,allocator_t,allocator_char_t,FooEx>();
    TestExceptionBody<tbb::concurrent_bounded_queue<FooEx, allocator_t>,allocator_t,allocator_char_t,FooEx>();
#else
    TestExceptionBody<tbb::concurrent_queue<FooEx, allocator_t>,allocator_t,allocator_char_t,FooEx>();
#endif
#endif /* TBB_USE_EXCEPTIONS */
}

template<typename CQ, typename T>
struct TestQueueElements: NoAssign {
    CQ& queue;
    const int nthread;
    TestQueueElements( CQ& q, int n ) : queue(q), nthread(n) {}
    void operator()( int k ) const {
        for( int i=0; i<1000; ++i ) {
            if( (i&0x1)==0 ) {
                ASSERT( T(k)<T(nthread), NULL );
                queue.push( T(k) );
            } else {
                // Pop item from queue
                T item = 0;
                queue.try_pop(item);
                ASSERT( item<=T(nthread), NULL );
            }
        }
    }
};

//! Test concurrent queue with primitive data type
template<typename CQ, typename T>
void TestPrimitiveTypes( int nthread, T exemplar )
{
    CQ queue;
    for( int i=0; i<100; ++i )
        queue.push( exemplar );
    NativeParallelFor( nthread, TestQueueElements<CQ,T>(queue,nthread) );
}

#include "harness_m128.h"

#if HAVE_m128 || HAVE_m256

//! Test concurrent queue with vector types
/** Type Queue should be a queue of ClassWithSSE/ClassWithAVX. */
template<typename ClassWithVectorType, typename Queue>
void TestVectorTypes() {
    Queue q1;
    for( int i=0; i<100; ++i ) {
        // VC8 does not properly align a temporary value; to work around, use explicit variable
        ClassWithVectorType bar(i);
        q1.push(bar);
    }

    // Copy the queue
    Queue q2 = q1;
    // Check that elements of the copy are correct
    typename Queue::const_iterator ci = q2.unsafe_begin();
    for( int i=0; i<100; ++i ) {
        ClassWithVectorType foo = *ci;
        ClassWithVectorType bar(i);
        ASSERT( *ci==bar, NULL );
        ++ci;
    }

    for( int i=0; i<101; ++i ) {
        ClassWithVectorType tmp;
        bool b = q1.try_pop( tmp );
        ASSERT( b==(i<100), NULL );
        ClassWithVectorType bar(i);
        ASSERT( !b || tmp==bar, NULL );
    }
}
#endif /* HAVE_m128 || HAVE_m256 */

void TestEmptiness()
{
    REMARK(" Test Emptiness\n");
#if !TBB_DEPRECATED
    TestEmptyQueue<ConcQWithCapacity<char>, char>();
    TestEmptyQueue<ConcQWithCapacity<Foo>, Foo>();
    TestEmptyQueue<tbb::concurrent_bounded_queue<char>, char>();
    TestEmptyQueue<tbb::concurrent_bounded_queue<Foo>, Foo>();
#else
    TestEmptyQueue<tbb::concurrent_queue<char>, char>();
    TestEmptyQueue<tbb::concurrent_queue<Foo>, Foo>();
#endif
}

void TestFullness()
{
    REMARK(" Test Fullness\n");
#if !TBB_DEPRECATED
    TestFullQueue<ConcQWithCapacity<Foo>,Foo>();
    TestFullQueue<tbb::concurrent_bounded_queue<Foo>,Foo>();
#else
    TestFullQueue<tbb::concurrent_queue<Foo>,Foo>();
#endif
}

void TestClearWorks() 
{
    REMARK(" Test concurrent_queue::clear() works\n");
#if !TBB_DEPRECATED
    TestClear<ConcQWithCapacity<Foo> >();
    TestClear<tbb::concurrent_bounded_queue<Foo> >();
#else
    TestClear<tbb::concurrent_queue<Foo> >();
#endif
}

void TestQueueTypeDeclaration()
{
    REMARK(" Test concurrent_queue's types work\n");
#if !TBB_DEPRECATED
    TestConcurrentQueueType<tbb::concurrent_queue<Foo> >();
    TestConcurrentQueueType<tbb::concurrent_bounded_queue<Foo> >();
#else
    TestConcurrentQueueType<tbb::concurrent_queue<Foo> >();
#endif
}

void TestQueueIteratorWorks()
{
    REMARK(" Test concurrent_queue's iterators work\n");
#if !TBB_DEPRECATED
    TestIterator<tbb::concurrent_queue<Foo> >();
    TestIterator<tbb::concurrent_bounded_queue<Foo> >();
#else
    TestIterator<tbb::concurrent_queue<Foo> >();
#endif
}

#if TBB_USE_EXCEPTIONS
#define BAR_EX BarEx
#else
#define BAR_EX Empty  /* passed as template arg but should not be used */
#endif
class Empty;

void TestQueueConstructors() 
{
    REMARK(" Test concurrent_queue's constructors work\n");
#if !TBB_DEPRECATED
    TestConstructors<ConcQWithSizeWrapper<Bar>,Bar,BarIterator,ConcQWithSizeWrapper<BAR_EX>,BAR_EX>();
    TestConstructors<tbb::concurrent_bounded_queue<Bar>,Bar,BarIterator,tbb::concurrent_bounded_queue<BAR_EX>,BAR_EX>();
#else
    TestConstructors<tbb::concurrent_queue<Bar>,Bar,BarIterator,tbb::concurrent_queue<BAR_EX>,BAR_EX>();
#endif
}

void TestQueueWorksWithPrimitiveTypes()
{
    REMARK(" Test concurrent_queue works with primitive types\n");
#if !TBB_DEPRECATED
    TestPrimitiveTypes<tbb::concurrent_queue<char>, char>( MaxThread, (char)1 );
    TestPrimitiveTypes<tbb::concurrent_queue<int>, int>( MaxThread, (int)-12 );
    TestPrimitiveTypes<tbb::concurrent_queue<float>, float>( MaxThread, (float)-1.2f );
    TestPrimitiveTypes<tbb::concurrent_queue<double>, double>( MaxThread, (double)-4.3 );
    TestPrimitiveTypes<tbb::concurrent_bounded_queue<char>, char>( MaxThread, (char)1 );
    TestPrimitiveTypes<tbb::concurrent_bounded_queue<int>, int>( MaxThread, (int)-12 );
    TestPrimitiveTypes<tbb::concurrent_bounded_queue<float>, float>( MaxThread, (float)-1.2f );
    TestPrimitiveTypes<tbb::concurrent_bounded_queue<double>, double>( MaxThread, (double)-4.3 );
#else
    TestPrimitiveTypes<tbb::concurrent_queue<char>, char>( MaxThread, (char)1 );
    TestPrimitiveTypes<tbb::concurrent_queue<int>, int>( MaxThread, (int)-12 );
    TestPrimitiveTypes<tbb::concurrent_queue<float>, float>( MaxThread, (float)-1.2f );
    TestPrimitiveTypes<tbb::concurrent_queue<double>, double>( MaxThread, (double)-4.3 );
#endif
}

void TestQueueWorksWithSSE()
{
    REMARK(" Test concurrent_queue works with SSE data\n");
#if HAVE_m128
#if !TBB_DEPRECATED
    TestVectorTypes<ClassWithSSE, tbb::concurrent_queue<ClassWithSSE> >();
    TestVectorTypes<ClassWithSSE, tbb::concurrent_bounded_queue<ClassWithSSE> >();
#else
    TestVectorTypes<ClassWithSSE, tbb::concurrent_queue<ClassWithSSE> >();
#endif
#endif /* HAVE_m128 */
#if HAVE_m256
    if( have_AVX() ) {
#if !TBB_DEPRECATED
        TestVectorTypes<ClassWithAVX, tbb::concurrent_queue<ClassWithAVX> >();
        TestVectorTypes<ClassWithAVX, tbb::concurrent_bounded_queue<ClassWithAVX> >();
#else
        TestVectorTypes<ClassWithAVX, tbb::concurrent_queue<ClassWithAVX> >();
#endif
    }
#endif /* HAVE_m256 */
}

void TestConcurrentPushPop()
{
    REMARK(" Test concurrent_queue's concurrent push and pop\n");
    for( int nthread=MinThread; nthread<=MaxThread; ++nthread ) {
        REMARK(" Testing with %d thread(s)\n", nthread );
#if TBB_DEPRECATED
        TestNegativeQueue<Foo>(nthread);
        for( size_t prefill=0; prefill<64; prefill+=(1+prefill/3) ) {
            TestPushPop<tbb::concurrent_queue<Foo>,Foo>(prefill,ptrdiff_t(-1),nthread);
            TestPushPop<tbb::concurrent_queue<Foo>,Foo>(prefill,ptrdiff_t(1),nthread);
            TestPushPop<tbb::concurrent_queue<Foo>,Foo>(prefill,ptrdiff_t(2),nthread);
            TestPushPop<tbb::concurrent_queue<Foo>,Foo>(prefill,ptrdiff_t(10),nthread);
            TestPushPop<tbb::concurrent_queue<Foo>,Foo>(prefill,ptrdiff_t(100),nthread);
        }
#else
        for( size_t prefill=0; prefill<64; prefill+=(1+prefill/3) ) {
            TestPushPop<ConcQPushPopWrapper<Foo>,Foo>(prefill,ptrdiff_t(-1),nthread);
            TestPushPop<ConcQPushPopWrapper<Foo>,Foo>(prefill,ptrdiff_t(1),nthread);
            TestPushPop<ConcQPushPopWrapper<Foo>,Foo>(prefill,ptrdiff_t(2),nthread);
            TestPushPop<ConcQPushPopWrapper<Foo>,Foo>(prefill,ptrdiff_t(10),nthread);
            TestPushPop<ConcQPushPopWrapper<Foo>,Foo>(prefill,ptrdiff_t(100),nthread);
        }
        for( size_t prefill=0; prefill<64; prefill+=(1+prefill/3) ) {
            TestPushPop<tbb::concurrent_bounded_queue<Foo>,Foo>(prefill,ptrdiff_t(-1),nthread);
            TestPushPop<tbb::concurrent_bounded_queue<Foo>,Foo>(prefill,ptrdiff_t(1),nthread);
            TestPushPop<tbb::concurrent_bounded_queue<Foo>,Foo>(prefill,ptrdiff_t(2),nthread);
            TestPushPop<tbb::concurrent_bounded_queue<Foo>,Foo>(prefill,ptrdiff_t(10),nthread);
            TestPushPop<tbb::concurrent_bounded_queue<Foo>,Foo>(prefill,ptrdiff_t(100),nthread);
        }
#endif /* !TBB_DEPRECATED */
    }
}

#if TBB_USE_EXCEPTIONS
tbb::atomic<size_t> num_pushed;
tbb::atomic<size_t> num_popped;
tbb::atomic<size_t> failed_pushes;
tbb::atomic<size_t> failed_pops;

class SimplePushBody {
    tbb::concurrent_bounded_queue<int>* q;
    int max;
public:
    SimplePushBody(tbb::concurrent_bounded_queue<int>* _q, int hi_thr) : q(_q), max(hi_thr) {}
    void operator()(int thread_id) const {
        if (thread_id == max) {
            Harness::Sleep(50);
            q->abort();
            return;
        }
        try {
            q->push(42);
            ++num_pushed;
        } catch ( tbb::user_abort& ) {
            ++failed_pushes;
        }
    }
};

class SimplePopBody {
    tbb::concurrent_bounded_queue<int>* q;
    int max;
public:
    SimplePopBody(tbb::concurrent_bounded_queue<int>* _q, int hi_thr) : q(_q), max(hi_thr) {}
    void operator()(int thread_id) const {
        int e;
        if (thread_id == max) {
            Harness::Sleep(50);
            q->abort();
            return;
        }
        try {
            q->pop(e);
            ++num_popped;
        } catch ( tbb::user_abort& ) {
            ++failed_pops;
        }
    }
};
#endif /* TBB_USE_EXCEPTIONS */

void TestAbort() {
#if TBB_USE_EXCEPTIONS
    for (int nthreads=MinThread; nthreads<=MaxThread; ++nthreads) {
        REMARK("Testing Abort on %d thread(s).\n", nthreads);

        REMARK("...testing pushing to zero-sized queue\n");
        tbb::concurrent_bounded_queue<int> iq1;
        iq1.set_capacity(0);
        for (int i=0; i<10; ++i) {
            num_pushed = num_popped = failed_pushes = failed_pops = 0;
            SimplePushBody my_push_body1(&iq1, nthreads);
            NativeParallelFor( nthreads+1, my_push_body1 );
            ASSERT(num_pushed == 0, "no elements should have been pushed to zero-sized queue");
            ASSERT((int)failed_pushes == nthreads, "All threads should have failed to push an element to zero-sized queue");
        }
        
        REMARK("...testing pushing to small-sized queue\n");
        tbb::concurrent_bounded_queue<int> iq2;
        iq2.set_capacity(2);
        for (int i=0; i<10; ++i) {
            num_pushed = num_popped = failed_pushes = failed_pops = 0;
            SimplePushBody my_push_body2(&iq2, nthreads);
            NativeParallelFor( nthreads+1, my_push_body2 );
            ASSERT(num_pushed <= 2, "at most 2 elements should have been pushed to queue of size 2");
            if (nthreads >= 2) 
                ASSERT((int)failed_pushes == nthreads-2, "nthreads-2 threads should have failed to push an element to queue of size 2");
            int e;
            while (iq2.try_pop(e)) ;
        }
        
        REMARK("...testing popping from small-sized queue\n");
        tbb::concurrent_bounded_queue<int> iq3;
        iq3.set_capacity(2);
        for (int i=0; i<10; ++i) {
            num_pushed = num_popped = failed_pushes = failed_pops = 0;
            iq3.push(42);
            iq3.push(42);
            SimplePopBody my_pop_body(&iq3, nthreads);
            NativeParallelFor( nthreads+1, my_pop_body);
            ASSERT(num_popped <= 2, "at most 2 elements should have been popped from queue of size 2");
            if (nthreads >= 2)
                ASSERT((int)failed_pops == nthreads-2, "nthreads-2 threads should have failed to pop an element from queue of size 2");
            else {
                int e;
                iq3.pop(e);
            }
        }

        REMARK("...testing pushing and popping from small-sized queue\n");
        tbb::concurrent_bounded_queue<int> iq4;
        int cap = nthreads/2;
        if (!cap) cap=1;
        iq4.set_capacity(cap);
        for (int i=0; i<10; ++i) {
            num_pushed = num_popped = failed_pushes = failed_pops = 0;
            SimplePushBody my_push_body2(&iq4, nthreads);
            NativeParallelFor( nthreads+1, my_push_body2 );
            ASSERT((int)num_pushed <= cap, "at most cap elements should have been pushed to queue of size cap");
            if (nthreads >= cap) 
                ASSERT((int)failed_pushes == nthreads-cap, "nthreads-cap threads should have failed to push an element to queue of size cap");
            SimplePopBody my_pop_body(&iq4, nthreads);
            NativeParallelFor( nthreads+1, my_pop_body);
            ASSERT((int)num_popped <= cap, "at most cap elements should have been popped from queue of size cap");
            if (nthreads >= cap)
                ASSERT((int)failed_pops == nthreads-cap, "nthreads-cap threads should have failed to pop an element from queue of size cap");
            else {
                int e;
                while (iq4.try_pop(e)) ;
            }
        }
    }
#endif
}


template <typename Q>
class FloggerBody : NoAssign {
    Q *q;
public:
    FloggerBody(Q *q_) : q(q_) {}  
    void operator()(const int threadID) const {
        typedef typename Q::value_type value_type;
        value_type elem = value_type(threadID);
        for (size_t i=0; i<275; ++i) {
            q->push(elem);
            (void) q->try_pop(elem);
        }
    }
};

template <typename HackedQRep, typename Q>
void TestFloggerHelp(HackedQRep* hack_rep, Q* q, size_t items_per_page) {
    size_t nq = HackedQRep::n_queue;
    size_t hack_val = std::numeric_limits<std::size_t>::max() & ~( nq * items_per_page - 1 );
    hack_rep->head_counter = hack_val;
    hack_rep->tail_counter = hack_val;
    size_t k = hack_rep->tail_counter & -(ptrdiff_t)nq;

    for (size_t i=0; i<nq; ++i) {
        hack_rep->array[i].head_counter = k;
        hack_rep->array[i].tail_counter = k;
    }
    NativeParallelFor(MaxThread, FloggerBody<Q>(q));
    ASSERT(q->empty(), "FAILED flogger/empty test.");
    delete q;
}

template <typename T>
void TestFlogger(T /*max*/) {
    { // test strict_ppl::concurrent_queue or deprecated::concurrent_queue
        tbb::concurrent_queue<T>* q = new tbb::concurrent_queue<T>;
#if !TBB_DEPRECATED
        REMARK("Wraparound on strict_ppl::concurrent_queue...");
        hacked_concurrent_queue_rep* hack_rep = ((hacked_concurrent_queue<T>*)(void*)q)->my_rep;
        TestFloggerHelp(hack_rep, q, hack_rep->items_per_page);
        REMARK(" works.\n");
#else
        REMARK("Wraparound on deprecated::concurrent_queue...");
        hacked_bounded_concurrent_queue* hack_q = (hacked_bounded_concurrent_queue*)(void*)q;
        hacked_bounded_concurrent_queue_rep* hack_rep = hack_q->my_rep;
        TestFloggerHelp(hack_rep, q, hack_q->items_per_page);
        REMARK(" works.\n");
#endif
    }
    { // test tbb::concurrent_bounded_queue
        tbb::concurrent_bounded_queue<T>* q = new tbb::concurrent_bounded_queue<T>;
        REMARK("Wraparound on tbb::concurrent_bounded_queue...");
        hacked_bounded_concurrent_queue* hack_q = (hacked_bounded_concurrent_queue*)(void*)q;
        hacked_bounded_concurrent_queue_rep* hack_rep = hack_q->my_rep;
        TestFloggerHelp(hack_rep, q, hack_q->items_per_page);
        REMARK(" works.\n");
    }
}

void TestWraparound() {
    REMARK("Testing Wraparound...\n");
    TestFlogger(std::numeric_limits<int>::max());
    TestFlogger(std::numeric_limits<unsigned char>::max());
    REMARK("Done Testing Wraparound.\n");
}

int TestMain () {
    TestEmptiness();

    TestFullness();

    TestClearWorks();

    TestQueueTypeDeclaration();

    TestQueueIteratorWorks();

    TestQueueConstructors();

    TestQueueWorksWithPrimitiveTypes();

    TestQueueWorksWithSSE();

    // Test concurrent operations
    TestConcurrentPushPop();

    TestExceptions();

    TestAbort();

    TestWraparound();

    return Harness::Done;
}
