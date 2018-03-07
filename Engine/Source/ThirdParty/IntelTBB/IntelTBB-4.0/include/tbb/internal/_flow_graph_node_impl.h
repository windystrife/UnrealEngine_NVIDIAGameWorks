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

#ifndef __TBB__flow_graph_node_impl_H
#define __TBB__flow_graph_node_impl_H

#ifndef __TBB_flow_graph_H
#error Do not #include this internal file directly; use public TBB headers instead.
#endif

#include "_flow_graph_item_buffer_impl.h"

//! @cond INTERNAL
namespace internal {

    using tbb::internal::aggregated_operation;
    using tbb::internal::aggregating_functor;
    using tbb::internal::aggregator;

     template< typename T, typename A >
     class function_input_queue : public item_buffer<T,A> {
     public:
         bool pop( T& t ) {
             return this->pop_front( t );
         }

         bool push( T& t ) {
             return this->push_back( t );
         }
     };

    //! Input and scheduling for a function node that takes a type Input as input
    //  The only up-ref is apply_body_impl, which should implement the function 
    //  call and any handling of the result.
    template< typename Input, typename A, typename ImplType >
    class function_input_base : public receiver<Input>, tbb::internal::no_assign {
        typedef sender<Input> predecessor_type;
        enum op_stat {WAIT=0, SUCCEEDED, FAILED};
        enum op_type {reg_pred, rem_pred, app_body, tryput, try_fwd};
        typedef function_input_base<Input, A, ImplType> my_class;
        
    public:

        //! The input type of this receiver
        typedef Input input_type;
        
        //! Constructor for function_input_base
        function_input_base( graph &g, size_t max_concurrency, function_input_queue<input_type,A> *q = NULL )
            : my_root_task(g.root_task()), my_max_concurrency(max_concurrency), my_concurrency(0),
              my_queue(q), forwarder_busy(false) {
            my_predecessors.set_owner(this);
            my_aggregator.initialize_handler(my_handler(this));
        }
        
        //! Copy constructor
        function_input_base( const function_input_base& src, function_input_queue<input_type,A> *q = NULL ) :
            receiver<Input>(), tbb::internal::no_assign(),
            my_root_task( src.my_root_task), my_max_concurrency(src.my_max_concurrency),
            my_concurrency(0), my_queue(q), forwarder_busy(false)
        {
            my_predecessors.set_owner(this);
            my_aggregator.initialize_handler(my_handler(this));
        }

        //! Destructor
        virtual ~function_input_base() { 
            if ( my_queue ) delete my_queue;
        }
        
        //! Put to the node
        virtual bool try_put( const input_type &t ) {
           if ( my_max_concurrency == 0 ) {
               spawn_body_task( t );
               return true;
           } else {
               my_operation op_data(t, tryput);
               my_aggregator.execute(&op_data);
               return op_data.status == SUCCEEDED;
           }
        }
        
        //! Adds src to the list of cached predecessors.
        /* override */ bool register_predecessor( predecessor_type &src ) {
            my_operation op_data(reg_pred);
            op_data.r = &src;
            my_aggregator.execute(&op_data);
            return true;
        }
        
        //! Removes src from the list of cached predecessors.
        /* override */ bool remove_predecessor( predecessor_type &src ) {
            my_operation op_data(rem_pred);
            op_data.r = &src;
            my_aggregator.execute(&op_data);
            return true;
        }

    protected:

        void reset_function_input_base() {
            my_concurrency = 0;
            if(my_queue) {
                my_queue->reset();
            }
            my_predecessors.reset();
        }

        task *my_root_task;
        const size_t my_max_concurrency;
        size_t my_concurrency;
        function_input_queue<input_type, A> *my_queue;
        predecessor_cache<input_type, null_mutex > my_predecessors;
        
        /*override*/void reset_receiver() {
            my_predecessors.reset();
        }

    private:

        friend class apply_body_task< my_class, input_type >;
        friend class forward_task< my_class >;
        
        class my_operation : public aggregated_operation< my_operation > {
        public:
            char type;
            union {
                input_type *elem;
                predecessor_type *r;
            };
            my_operation(const input_type& e, op_type t) :
                type(char(t)), elem(const_cast<input_type*>(&e)) {}
            my_operation(op_type t) : type(char(t)), r(NULL) {}
        };
        
        bool forwarder_busy;
        typedef internal::aggregating_functor<my_class, my_operation> my_handler;
        friend class internal::aggregating_functor<my_class, my_operation>;
        aggregator< my_handler, my_operation > my_aggregator;
        
        void handle_operations(my_operation *op_list) {
            my_operation *tmp;
            while (op_list) {
                tmp = op_list;
                op_list = op_list->next;
                switch (tmp->type) {
                case reg_pred:
                    my_predecessors.add(*(tmp->r));
                    __TBB_store_with_release(tmp->status, SUCCEEDED);
                    if (!forwarder_busy) {
                        forwarder_busy = true;
                        spawn_forward_task();
                    }
                    break;
                case rem_pred:
                    my_predecessors.remove(*(tmp->r));
                    __TBB_store_with_release(tmp->status, SUCCEEDED);
                    break;
                case app_body:
                    __TBB_ASSERT(my_max_concurrency != 0, NULL);
                    --my_concurrency;
                    __TBB_store_with_release(tmp->status, SUCCEEDED);
                    if (my_concurrency<my_max_concurrency) {
                        input_type i;
                        bool item_was_retrieved = false;
                        if ( my_queue )
                            item_was_retrieved = my_queue->pop(i);
                        else
                            item_was_retrieved = my_predecessors.get_item(i);
                        if (item_was_retrieved) {
                            ++my_concurrency;
                            spawn_body_task(i);
                        }
                    }
                    break;
                case tryput: internal_try_put(tmp);  break;
                case try_fwd: internal_forward(tmp);  break;
                    }
            }
        }
        
        //! Put to the node
        void internal_try_put(my_operation *op) {
            __TBB_ASSERT(my_max_concurrency != 0, NULL);
            if (my_concurrency < my_max_concurrency) {
               ++my_concurrency;
               spawn_body_task(*(op->elem));
               __TBB_store_with_release(op->status, SUCCEEDED);
           } else if ( my_queue && my_queue->push(*(op->elem)) ) { 
               __TBB_store_with_release(op->status, SUCCEEDED);
           } else {
               __TBB_store_with_release(op->status, FAILED);
           }
        }
        
        //! Tries to spawn bodies if available and if concurrency allows
        void internal_forward(my_operation *op) {
            if (my_concurrency<my_max_concurrency || !my_max_concurrency) {
                input_type i;
                bool item_was_retrieved = false;
                if ( my_queue )
                    item_was_retrieved = my_queue->pop(i);
                else
                    item_was_retrieved = my_predecessors.get_item(i);
                if (item_was_retrieved) {
                    ++my_concurrency;
                    __TBB_store_with_release(op->status, SUCCEEDED);
                    spawn_body_task(i);
                    return;
                }
            }
            __TBB_store_with_release(op->status, FAILED);
            forwarder_busy = false;
        }
        
        //! Applies the body to the provided input
        void apply_body( input_type &i ) {
            static_cast<ImplType *>(this)->apply_body_impl(i);
            if ( my_max_concurrency != 0 ) {
                my_operation op_data(app_body);
                my_aggregator.execute(&op_data);
            }
        }
        
       //! Spawns a task that calls apply_body( input )
       inline void spawn_body_task( const input_type &input ) {
           task::enqueue(*new(task::allocate_additional_child_of(*my_root_task)) apply_body_task< my_class, input_type >(*this, input));
       }
        
       //! This is executed by an enqueued task, the "forwarder"
       void forward() {
           my_operation op_data(try_fwd);
           do {
               op_data.status = WAIT;
               my_aggregator.execute(&op_data);
           } while (op_data.status == SUCCEEDED);
       }
        
       //! Spawns a task that calls forward()
       inline void spawn_forward_task() {
           task::enqueue(*new(task::allocate_additional_child_of(*my_root_task)) forward_task< my_class >(*this));
       }
    };  // function_input_base

    //! Implements methods for a function node that takes a type Input as input and sends
    //  a type Output to its successors.
    template< typename Input, typename Output, typename A>
    class function_input : public function_input_base<Input, A, function_input<Input,Output,A> > {
    public:
        typedef Input input_type;
        typedef Output output_type;
        typedef function_input<Input,Output,A> my_class;
        typedef function_input_base<Input, A, my_class> base_type;
        typedef function_input_queue<input_type, A> input_queue_type;


        // constructor
        template<typename Body>
        function_input( graph &g, size_t max_concurrency, Body& body, function_input_queue<input_type,A> *q = NULL ) :
            base_type(g, max_concurrency, q),
            my_body( new internal::function_body_leaf< input_type, output_type, Body>(body) ) {
        }

        //! Copy constructor
        function_input( const function_input& src, input_queue_type *q = NULL ) : 
                base_type(src, q),
                my_body( src.my_body->clone() ) {
        }

        ~function_input() {
            delete my_body;
        }

        template< typename Body >
        Body copy_function_object() {
            internal::function_body<input_type, output_type> &body_ref = *this->my_body;
            return dynamic_cast< internal::function_body_leaf<input_type, output_type, Body> & >(body_ref).get_body(); 
        } 

        void apply_body_impl( const input_type &i) {
            successors().try_put( (*my_body)(i) );
        }

    protected:

        void reset_function_input() { 
            base_type::reset_function_input_base();
        }

        function_body<input_type, output_type> *my_body;
        virtual broadcast_cache<output_type > &successors() = 0;

    };

    //! Implements methods for a function node that takes a type Input as input
    //  and has a tuple of output ports specified.  
    template< typename Input, typename OutputPortSet, typename A>
    class multifunction_input : public function_input_base<Input, A, multifunction_input<Input,OutputPortSet,A> > {
    public:
        typedef Input input_type;
        typedef OutputPortSet output_ports_type;
        typedef multifunction_input<Input,OutputPortSet,A> my_class;
        typedef function_input_base<Input, A, my_class> base_type;
        typedef function_input_queue<input_type, A> input_queue_type;


        // constructor
        template<typename Body>
        multifunction_input( 
                graph &g, 
                size_t max_concurrency, 
                Body& body,
                function_input_queue<input_type,A> *q = NULL ) :
            base_type(g, max_concurrency, q),
            my_body( new internal::multifunction_body_leaf<input_type, output_ports_type, Body>(body) ) {
        }

        //! Copy constructor
        multifunction_input( const multifunction_input& src, input_queue_type *q = NULL ) : 
                base_type(src, q),
                my_body( src.my_body->clone() ) {
        }

        ~multifunction_input() {
            delete my_body;
        }

        template< typename Body >
        Body copy_function_object() {
            internal::multifunction_body<input_type, output_ports_type> &body_ref = *this->my_body;
            return dynamic_cast< internal::multifunction_body_leaf<input_type, output_ports_type, Body> & >(body_ref).get_body(); 
        } 

        void apply_body_impl( const input_type &i) {
            (*my_body)(i, my_output_ports);
        }

        output_ports_type &output_ports(){ return my_output_ports; }

    protected:

        void reset() {
            base_type::reset_function_input_base();
        }

        multifunction_body<input_type, output_ports_type> *my_body;
        output_ports_type my_output_ports;

    };

    // template to refer to an output port of a multifunction_node
    template<size_t N, typename MOP>
    typename std::tuple_element<N, typename MOP::output_ports_type>::type &output_port(MOP &op) {
        return std::get<N>(op.output_ports()); 
    }

// helper structs for split_node
    template<int N>
    struct emit_element {
        template<typename T, typename P>
        static void emit_this(const T &t, P &p) {
            (void)std::get<N-1>(p).try_put(std::get<N-1>(t));
            emit_element<N-1>::emit_this(t,p);
        }
    };

    template<>
    struct emit_element<1> {
        template<typename T, typename P>
        static void emit_this(const T &t, P &p) {
            (void)std::get<0>(p).try_put(std::get<0>(t));
        }
    };

    //! Implements methods for an executable node that takes continue_msg as input
    template< typename Output >
    class continue_input : public continue_receiver {
    public:
        
        //! The input type of this receiver
        typedef continue_msg input_type;
            
        //! The output type of this receiver
        typedef Output output_type;
        
        template< typename Body >
        continue_input( graph &g, Body& body )
            : my_root_task(g.root_task()), 
             my_body( new internal::function_body_leaf< input_type, output_type, Body>(body) ) { }
        
        template< typename Body >
        continue_input( graph &g, int number_of_predecessors, Body& body )
            : continue_receiver( number_of_predecessors ), my_root_task(g.root_task()), 
             my_body( new internal::function_body_leaf< input_type, output_type, Body>(body) ) { }

        continue_input( const continue_input& src ) : continue_receiver(src), 
            my_root_task(src.my_root_task), my_body( src.my_body->clone() ) {}

        template< typename Body >
        Body copy_function_object() {
            internal::function_body<input_type, output_type> &body_ref = *my_body;
            return dynamic_cast< internal::function_body_leaf<input_type, output_type, Body> & >(body_ref).get_body(); 
        } 

    protected:
        
        task *my_root_task;
        function_body<input_type, output_type> *my_body;
        
        virtual broadcast_cache<output_type > &successors() = 0; 
        
        friend class apply_body_task< continue_input< Output >, continue_msg >;
        
        //! Applies the body to the provided input
        /* override */ void apply_body( input_type ) {
            successors().try_put( (*my_body)( continue_msg() ) );
        }
        
        //! Spawns a task that applies the body
        /* override */ void execute( ) {
            task::enqueue( * new ( task::allocate_additional_child_of( *my_root_task ) ) 
               apply_body_task< continue_input< Output >, continue_msg >( *this, continue_msg() ) ); 
        }

    };
        
    //! Implements methods for both executable and function nodes that puts Output to its successors
    template< typename Output >
    class function_output : public sender<Output> {
    public:
        
        typedef Output output_type;
        
        function_output() { my_successors.set_owner(this); }
        function_output(const function_output & /*other*/) : sender<output_type>() {
            my_successors.set_owner(this);
        }
        
        //! Adds a new successor to this node
        /* override */ bool register_successor( receiver<output_type> &r ) {
            successors().register_successor( r );
            return true;
        }
        
        //! Removes a successor from this node
        /* override */ bool remove_successor( receiver<output_type> &r ) {
            successors().remove_successor( r );
            return true;
        }

        // for multifunction_node.  The function_body that implements
        // the node will have an input and an output tuple of ports.  To put
        // an item to a successor, the body should
        //
        //    get<I>(output_ports).try_put(output_value);
        //
        // return value will be bool returned from successors.try_put.
        bool try_put(const output_type &i) { return my_successors.try_put(i); }
          
    protected:
        broadcast_cache<output_type> my_successors;
        broadcast_cache<output_type > &successors() { return my_successors; } 
        
    };

}  // internal

#endif // __TBB__flow_graph_node_impl_H
