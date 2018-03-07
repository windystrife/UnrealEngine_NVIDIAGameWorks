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

// configuration:

// Size of final table (must be multiple of STEP_*)
int MAX_TABLE_SIZE = 2000000;

// Specify list of unique percents (5-30,100) to test against. Max 10 values
#define UNIQUE_PERCENTS PERCENT(5); PERCENT(10); PERCENT(20); PERCENT(30); PERCENT(100)

// enable/disable tests for:
#define BOX1 "CHMap"
#define BOX1TEST ValuePerSecond<Uniques<tbb::concurrent_hash_map<int,int> >, 1000000/*ns*/>
#define BOX1HEADER "tbb/concurrent_hash_map.h"

// enable/disable tests for:
#define BOX2 "CUMap"
#define BOX2TEST ValuePerSecond<Uniques<tbb::concurrent_unordered_map<int,int> >, 1000000/*ns*/>
#define BOX2HEADER "tbb/concurrent_unordered_map.h"

// enable/disable tests for:
//#define BOX3 "OLD"
#define BOX3TEST ValuePerSecond<Uniques<tbb::concurrent_hash_map<int,int> >, 1000000/*ns*/>
#define BOX3HEADER "tbb/concurrent_hash_map-5468.h"

#define TBB_USE_THREADING_TOOLS 0
//////////////////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <math.h>
#include "tbb/tbb_stddef.h"
#include <vector>
#include <map>
// needed by hash_maps
#include <stdexcept>
#include <iterator>
#include <algorithm>                 // std::swap
#include <utility>      // Need std::pair
#include <cstring>      // Need std::memset
#include <typeinfo>
#include "tbb/cache_aligned_allocator.h"
#include "tbb/tbb_allocator.h"
#include "tbb/spin_rw_mutex.h"
#include "tbb/aligned_space.h"
#include "tbb/atomic.h"
#include "tbb/internal/_concurrent_unordered_impl.h"
// for test
#include "tbb/spin_mutex.h"
#include "time_framework.h"


using namespace tbb;
using namespace tbb::internal;

/////////////////////////////////////////////////////////////////////////////////////////
// Input data built for test
int *Data;

// Main test class used to run the timing tests. All overridden methods are called by the framework
template<typename TableType>
struct Uniques : TesterBase {
    TableType Table;
    int n_items;

    // Initializes base class with number of test modes
    Uniques() : TesterBase(2), Table(MaxThread*16) {
        //Table->max_load_factor(1); // add stub into hash_map to uncomment it
    }
    ~Uniques() {}
    
    // Returns name of test mode specified by number
    /*override*/ std::string get_name(int testn) {
        if(testn == 1) return "find";
        return "insert";
    }

    // Informs the class that value and threads number become known
    /*override*/ void init() {
        n_items = value/threads_count; // operations
    }

    // Informs the class that the test mode for specified thread is about to start
    /*override*/ void test_prefix(int testn, int t) {
        barrier->wait();
        if(Verbose && !t && testn) printf("%s: inserted %u, %g%% of operations\n", tester_name, unsigned(Table.size()), 100.0*Table.size()/(value*testn));
    }

    // Executes test mode for a given thread. Return value is ignored when used with timing wrappers.
    /*override*/ double test(int testn, int t)
    {
        if( testn != 1 ) { // do insertions
            for(int i = testn*value+t*n_items, e = testn*value+(t+1)*n_items; i < e; i++) {
                Table.insert( std::make_pair(Data[i],t) );
            }
        } else { // do last finds
            for(int i = t*n_items, e = (t+1)*n_items; i < e; i++) {
                size_t c =
                    Table.count( Data[i] );
                ASSERT( c == 1, NULL ); // must exist
            }
        }
        return 0;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////
#include <limits>

// Using BOX declarations from configuration
#include "time_sandbox.h"

int rounds = 0;
// Prepares the input data for given unique percent
void execute_percent(test_sandbox &the_test, int p) {
    int input_size = MAX_TABLE_SIZE*100/p;
    Data = new int[input_size];
    int uniques = p==100?std::numeric_limits<int>::max() : MAX_TABLE_SIZE;
    ASSERT(p==100 || p <= 30, "Function is broken for %% > 30 except for 100%%");
    for(int i = 0; i < input_size; i++)
        Data[i] = rand()%uniques;
    for(int t = MinThread; t <= MaxThread; t++)
        the_test.factory(input_size, t); // executes the tests specified in BOX-es for given 'value' and threads
    the_test.report.SetRoundTitle(rounds++, "%d%%", p);
}
#define PERCENT(x) execute_percent(the_test, x)

int main(int argc, char* argv[]) {
    if(argc>1) Verbose = true;
    //if(argc>2) ExtraVerbose = true;
    MinThread = 1; MaxThread = task_scheduler_init::default_num_threads();
    ParseCommandLine( argc, argv );
    if(getenv("TABLE_SIZE"))
        MAX_TABLE_SIZE = atoi(getenv("TABLE_SIZE"));

    ASSERT(tbb_allocator<int>::allocator_type() == tbb_allocator<int>::scalable, "expecting scalable allocator library to be loaded. Please build it by:\n\t\tmake tbbmalloc");
    // Declares test processor
    test_sandbox the_test("time_hash_map_fill"/*, StatisticsCollector::ByThreads*/);
    srand(10101);
    UNIQUE_PERCENTS; // test the percents
    the_test.report.SetTitle("Operations per nanosecond");
    the_test.report.SetRunInfo("Items", MAX_TABLE_SIZE);
    the_test.report.Print(StatisticsCollector::HTMLFile|StatisticsCollector::ExcelXML); // Write files
    return 0;
}
