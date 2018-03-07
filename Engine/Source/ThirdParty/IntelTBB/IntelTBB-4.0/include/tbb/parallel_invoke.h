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

#ifndef __TBB_parallel_invoke_H
#define __TBB_parallel_invoke_H

#include "task.h"

namespace tbb {

#if !__TBB_TASK_GROUP_CONTEXT
    /** Dummy to avoid cluttering the bulk of the header with enormous amount of ifdefs. **/
    struct task_group_context {};
#endif /* __TBB_TASK_GROUP_CONTEXT */

//! @cond INTERNAL
namespace internal {
    // Simple task object, executing user method
    template<typename function>
    class function_invoker : public task{
    public:
        function_invoker(const function& _function) : my_function(_function) {}
    private:
        const function &my_function;
        /*override*/
        task* execute()
        {
            my_function();
            return NULL;
        }
    };

    // The class spawns two or three child tasks
    template <size_t N, typename function1, typename function2, typename function3>
    class spawner : public task {
    private:
        const function1& my_func1;
        const function2& my_func2;
        const function3& my_func3;
        bool is_recycled;

        task* execute (){
            if(is_recycled){
                return NULL;
            }else{
                __TBB_ASSERT(N==2 || N==3, "Number of arguments passed to spawner is wrong");
                set_ref_count(N);
                recycle_as_safe_continuation();
                internal::function_invoker<function2>* invoker2 = new (allocate_child()) internal::function_invoker<function2>(my_func2);
                __TBB_ASSERT(invoker2, "Child task allocation failed");
                spawn(*invoker2);
                size_t n = N; // To prevent compiler warnings
                if (n>2) {
                    internal::function_invoker<function3>* invoker3 = new (allocate_child()) internal::function_invoker<function3>(my_func3);
                    __TBB_ASSERT(invoker3, "Child task allocation failed");
                    spawn(*invoker3);
                }
                my_func1();
                is_recycled = true;
                return NULL;
            }
        } // execute

    public:
        spawner(const function1& _func1, const function2& _func2, const function3& _func3) : my_func1(_func1), my_func2(_func2), my_func3(_func3), is_recycled(false) {}
    };

    // Creates and spawns child tasks
    class parallel_invoke_helper : public empty_task {
    public:
        // Dummy functor class
        class parallel_invoke_noop {
        public:
            void operator() () const {}
        };
        // Creates a helper object with user-defined number of children expected
        parallel_invoke_helper(int number_of_children)
        {
            set_ref_count(number_of_children + 1);
        }
        // Adds child task and spawns it
        template <typename function>
        void add_child (const function &_func)
        {
            internal::function_invoker<function>* invoker = new (allocate_child()) internal::function_invoker<function>(_func);
            __TBB_ASSERT(invoker, "Child task allocation failed");
            spawn(*invoker);
        }

        // Adds a task with multiple child tasks and spawns it
        // two arguments
        template <typename function1, typename function2>
        void add_children (const function1& _func1, const function2& _func2)
        {
            // The third argument is dummy, it is ignored actually.
            parallel_invoke_noop noop;
            internal::spawner<2, function1, function2, parallel_invoke_noop>& sub_root = *new(allocate_child())internal::spawner<2, function1, function2, parallel_invoke_noop>(_func1, _func2, noop);
            spawn(sub_root);
        }
        // three arguments
        template <typename function1, typename function2, typename function3>
        void add_children (const function1& _func1, const function2& _func2, const function3& _func3)
        {
            internal::spawner<3, function1, function2, function3>& sub_root = *new(allocate_child())internal::spawner<3, function1, function2, function3>(_func1, _func2, _func3);
            spawn(sub_root);
        }

        // Waits for all child tasks
        template <typename F0>
        void run_and_finish(const F0& f0)
        {
            internal::function_invoker<F0>* invoker = new (allocate_child()) internal::function_invoker<F0>(f0);
            __TBB_ASSERT(invoker, "Child task allocation failed");
            spawn_and_wait_for_all(*invoker);
        }
    };
    // The class destroys root if exception occurred as well as in normal case
    class parallel_invoke_cleaner: internal::no_copy {
    public:
#if __TBB_TASK_GROUP_CONTEXT
        parallel_invoke_cleaner(int number_of_children, tbb::task_group_context& context)
            : root(*new(task::allocate_root(context)) internal::parallel_invoke_helper(number_of_children))
#else
        parallel_invoke_cleaner(int number_of_children, tbb::task_group_context&)
            : root(*new(task::allocate_root()) internal::parallel_invoke_helper(number_of_children))
#endif /* !__TBB_TASK_GROUP_CONTEXT */
        {}

        ~parallel_invoke_cleaner(){
            root.destroy(root);
        }
        internal::parallel_invoke_helper& root;
    };
} // namespace internal
//! @endcond

/** \name parallel_invoke
    **/
//@{
//! Executes a list of tasks in parallel and waits for all tasks to complete.
/** @ingroup algorithms */

// parallel_invoke with user-defined context
// two arguments
template<typename F0, typename F1 >
void parallel_invoke(const F0& f0, const F1& f1, tbb::task_group_context& context) {
    internal::parallel_invoke_cleaner cleaner(2, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_child(f1);

    root.run_and_finish(f0);
}

// three arguments
template<typename F0, typename F1, typename F2 >
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, tbb::task_group_context& context) {
    internal::parallel_invoke_cleaner cleaner(3, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_child(f2);
    root.add_child(f1);

    root.run_and_finish(f0);
}

// four arguments
template<typename F0, typename F1, typename F2, typename F3>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3,
                     tbb::task_group_context& context)
{
    internal::parallel_invoke_cleaner cleaner(4, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_child(f3);
    root.add_child(f2);
    root.add_child(f1);

    root.run_and_finish(f0);
}

// five arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4 >
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     tbb::task_group_context& context)
{
    internal::parallel_invoke_cleaner cleaner(3, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_children(f4, f3);
    root.add_children(f2, f1);

    root.run_and_finish(f0);
}

// six arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4, typename F5>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4, const F5& f5,
                     tbb::task_group_context& context)
{
    internal::parallel_invoke_cleaner cleaner(3, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_children(f5, f4, f3);
    root.add_children(f2, f1);

    root.run_and_finish(f0);
}

// seven arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4, typename F5, typename F6>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6,
                     tbb::task_group_context& context)
{
    internal::parallel_invoke_cleaner cleaner(3, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_children(f6, f5, f4);
    root.add_children(f3, f2, f1);

    root.run_and_finish(f0);
}

// eight arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4,
         typename F5, typename F6, typename F7>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6, const F7& f7,
                     tbb::task_group_context& context)
{
    internal::parallel_invoke_cleaner cleaner(4, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_children(f7, f6, f5);
    root.add_children(f4, f3);
    root.add_children(f2, f1);

    root.run_and_finish(f0);
}

// nine arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4,
         typename F5, typename F6, typename F7, typename F8>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6, const F7& f7, const F8& f8,
                     tbb::task_group_context& context)
{
    internal::parallel_invoke_cleaner cleaner(4, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_children(f8, f7, f6);
    root.add_children(f5, f4, f3);
    root.add_children(f2, f1);

    root.run_and_finish(f0);
}

// ten arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4,
         typename F5, typename F6, typename F7, typename F8, typename F9>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6, const F7& f7, const F8& f8, const F9& f9,
                     tbb::task_group_context& context)
{
    internal::parallel_invoke_cleaner cleaner(4, context);
    internal::parallel_invoke_helper& root = cleaner.root;

    root.add_children(f9, f8, f7);
    root.add_children(f6, f5, f4);
    root.add_children(f3, f2, f1);

    root.run_and_finish(f0);
}

// two arguments
template<typename F0, typename F1>
void parallel_invoke(const F0& f0, const F1& f1) {
    task_group_context context;
    parallel_invoke<F0, F1>(f0, f1, context);
}
// three arguments
template<typename F0, typename F1, typename F2>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2) {
    task_group_context context;
    parallel_invoke<F0, F1, F2>(f0, f1, f2, context);
}
// four arguments
template<typename F0, typename F1, typename F2, typename F3 >
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3) {
    task_group_context context;
    parallel_invoke<F0, F1, F2, F3>(f0, f1, f2, f3, context);
}
// five arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4) {
    task_group_context context;
    parallel_invoke<F0, F1, F2, F3, F4>(f0, f1, f2, f3, f4, context);
}
// six arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4, typename F5>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4, const F5& f5) {
    task_group_context context;
    parallel_invoke<F0, F1, F2, F3, F4, F5>(f0, f1, f2, f3, f4, f5, context);
}
// seven arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4, typename F5, typename F6>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6)
{
    task_group_context context;
    parallel_invoke<F0, F1, F2, F3, F4, F5, F6>(f0, f1, f2, f3, f4, f5, f6, context);
}
// eigth arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4,
         typename F5, typename F6, typename F7>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6, const F7& f7)
{
    task_group_context context;
    parallel_invoke<F0, F1, F2, F3, F4, F5, F6, F7>(f0, f1, f2, f3, f4, f5, f6, f7, context);
}
// nine arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4,
         typename F5, typename F6, typename F7, typename F8>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6, const F7& f7, const F8& f8)
{
    task_group_context context;
    parallel_invoke<F0, F1, F2, F3, F4, F5, F6, F7, F8>(f0, f1, f2, f3, f4, f5, f6, f7, f8, context);
}
// ten arguments
template<typename F0, typename F1, typename F2, typename F3, typename F4,
         typename F5, typename F6, typename F7, typename F8, typename F9>
void parallel_invoke(const F0& f0, const F1& f1, const F2& f2, const F3& f3, const F4& f4,
                     const F5& f5, const F6& f6, const F7& f7, const F8& f8, const F9& f9)
{
    task_group_context context;
    parallel_invoke<F0, F1, F2, F3, F4, F5, F6, F7, F8, F9>(f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, context);
}

//@}

} // namespace

#endif /* __TBB_parallel_invoke_H */
