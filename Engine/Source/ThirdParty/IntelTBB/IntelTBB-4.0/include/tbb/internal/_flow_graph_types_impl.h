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

#ifndef __TBB__flow_graph_types_impl_H
#define __TBB__flow_graph_types_impl_H

#ifndef __TBB_flow_graph_H
#error Do not #include this internal file directly; use public TBB headers instead.
#endif

namespace internal {
// wrap each element of a tuple in a template, and make a tuple of the result.

    template<int N, template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements;

    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<1, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type> >
            type;
    };

    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<2, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>, 
                PT<typename std::tuple_element<1,TypeTuple>::type> >
            type;
    };

    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<3, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type> >
            type;
    };

    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<4, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type>,
                PT<typename std::tuple_element<3,TypeTuple>::type> >
            type;
    };

    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<5, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type>,
                PT<typename std::tuple_element<3,TypeTuple>::type>,
                PT<typename std::tuple_element<4,TypeTuple>::type> >
            type;
    };

#if __TBB_VARIADIC_MAX >= 6
    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<6, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type>,
                PT<typename std::tuple_element<3,TypeTuple>::type>,
                PT<typename std::tuple_element<4,TypeTuple>::type>,
                PT<typename std::tuple_element<5,TypeTuple>::type> >
            type;
    };
#endif

#if __TBB_VARIADIC_MAX >= 7
    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<7, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type>,
                PT<typename std::tuple_element<3,TypeTuple>::type>,
                PT<typename std::tuple_element<4,TypeTuple>::type>,
                PT<typename std::tuple_element<5,TypeTuple>::type>,
                PT<typename std::tuple_element<6,TypeTuple>::type> >
            type;
    };
#endif

#if __TBB_VARIADIC_MAX >= 8
    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<8, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type>,
                PT<typename std::tuple_element<3,TypeTuple>::type>,
                PT<typename std::tuple_element<4,TypeTuple>::type>,
                PT<typename std::tuple_element<5,TypeTuple>::type>,
                PT<typename std::tuple_element<6,TypeTuple>::type>,
                PT<typename std::tuple_element<7,TypeTuple>::type> >
            type;
    };
#endif

#if __TBB_VARIADIC_MAX >= 9
    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<9, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type>,
                PT<typename std::tuple_element<3,TypeTuple>::type>,
                PT<typename std::tuple_element<4,TypeTuple>::type>,
                PT<typename std::tuple_element<5,TypeTuple>::type>,
                PT<typename std::tuple_element<6,TypeTuple>::type>,
                PT<typename std::tuple_element<7,TypeTuple>::type>,
                PT<typename std::tuple_element<8,TypeTuple>::type> >
            type;
    };
#endif

#if __TBB_VARIADIC_MAX >= 10
    template<template<class> class PT, typename TypeTuple>
    struct wrap_tuple_elements<10, PT, TypeTuple> {
        typedef typename std::tuple<
                PT<typename std::tuple_element<0,TypeTuple>::type>,
                PT<typename std::tuple_element<1,TypeTuple>::type>,
                PT<typename std::tuple_element<2,TypeTuple>::type>,
                PT<typename std::tuple_element<3,TypeTuple>::type>,
                PT<typename std::tuple_element<4,TypeTuple>::type>,
                PT<typename std::tuple_element<5,TypeTuple>::type>,
                PT<typename std::tuple_element<6,TypeTuple>::type>,
                PT<typename std::tuple_element<7,TypeTuple>::type>,
                PT<typename std::tuple_element<8,TypeTuple>::type>,
                PT<typename std::tuple_element<9,TypeTuple>::type> >
            type;
    };
#endif

}  // namespace internal
#endif  /* __TBB__flow_graph_types_impl_H */
