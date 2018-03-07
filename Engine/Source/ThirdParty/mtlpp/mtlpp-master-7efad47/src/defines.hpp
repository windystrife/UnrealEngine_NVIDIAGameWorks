/*
 * Copyright 2016-2017 Nikolay Aleksiev. All rights reserved.
 * License: https://github.com/naleksiev/mtlpp/blob/master/LICENSE
 */
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modifications for Unreal Engine

#pragma once

#include <stdint.h>
#include <assert.h>
#include <functional>

#ifndef __has_feature
#   define __has_feature(x) 0
#endif

#ifndef MTLPP_CONFIG_RVALUE_REFERENCES
#   define MTLPP_CONFIG_RVALUE_REFERENCES __has_feature(cxx_rvalue_references)
#endif

#ifndef MTLPP_CONFIG_VALIDATE
#   define MTLPP_CONFIG_VALIDATE 1
#endif

#ifndef MTLPP_CONFIG_USE_AVAILABILITY
#   define MTLPP_CONFIG_USE_AVAILABILITY 0
#endif

#ifndef MTLPP_CONFIG_USE_SDK_AVAILABILITY
#	define MTLPP_CONFIG_USE_SDK_AVAILABILITY 0
#endif

#if MTLPP_CONFIG_USE_AVAILABILITY
#   if __has_feature(attribute_availability_with_version_underscores) || (__has_feature(attribute_availability_with_message) && __clang__ && __clang_major__ >= 7)
#       include <CoreFoundation/CFAvailability.h>
#       define MTLPP_AVAILABLE(mac, ios)                            CF_AVAILABLE(mac, ios)
#       define MTLPP_AVAILABLE_MAC(mac)                             CF_AVAILABLE_MAC(mac)
#       define MTLPP_AVAILABLE_IOS(ios)                             CF_AVAILABLE_IOS(ios)
#       define MTLPP_AVAILABLE_TVOS(tvos)
#       define MTLPP_DEPRECATED(macIntro, macDep, iosIntro, iosDep) CF_DEPRECATED(macIntro, macDep, iosIntro, iosDep)
#       define MTLPP_DEPRECATED_MAC(macIntro, macDep)               CF_DEPRECATED_MAC(macIntro, macDep)
#       define MTLPP_DEPRECATED_IOS(iosIntro, iosDep)               CF_DEPRECATED_IOS(iosIntro, iosDep)
#   endif
#endif

#ifndef MTLPP_AVAILABLE
#   define MTLPP_AVAILABLE(mac, ios)
#   define MTLPP_AVAILABLE_MAC(mac)
#   define MTLPP_AVAILABLE_IOS(ios)
#   define MTLPP_AVAILABLE_TVOS(tvos)
#   define MTLPP_DEPRECATED(macIntro, macDep, iosIntro, iosDep)
#   define MTLPP_DEPRECATED_MAC(macIntro, macDep)
#   define MTLPP_DEPRECATED_IOS(iosIntro, iosDep)
#endif

#ifndef __DARWIN_ALIAS_STARTING_MAC___MAC_10_11
#   define __DARWIN_ALIAS_STARTING_MAC___MAC_10_11(x)
#endif
#ifndef __DARWIN_ALIAS_STARTING_MAC___MAC_10_12
#   define __DARWIN_ALIAS_STARTING_MAC___MAC_10_12(x)
#endif
#ifndef __DARWIN_ALIAS_STARTING_MAC___MAC_10_13
#   define __DARWIN_ALIAS_STARTING_MAC___MAC_10_13(x)
#endif
#ifndef __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_8_0
#   define __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_8_0(x)
#endif
#ifndef __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_9_0
#   define __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_9_0(x)
#endif
#ifndef __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_10_0
#   define __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_10_0(x)
#endif
#ifndef __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_10_3
#   define __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_10_3(x)
#endif
#ifndef __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_11_0
#   define __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_11_0(x)
#endif

#if MTLPP_CONFIG_USE_SDK_AVAILABILITY
#	define MTLPP_IS_AVAILABLE_MAC(mac)  (0 || (defined(__MAC_##mac) && __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_##mac))
#	define MTLPP_IS_AVAILABLE_IOS(ios)  (0 || (defined(__IPHONE_##ios) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_##ios))
#else
#	define MTLPP_IS_AVAILABLE_MAC(mac)  (0 __DARWIN_ALIAS_STARTING_MAC___MAC_##mac( || 1 ))
#	define MTLPP_IS_AVAILABLE_IOS(ios)  (0 __DARWIN_ALIAS_STARTING_IPHONE___IPHONE_##ios( || 1 ))
#endif
#define MTLPP_IS_AVAILABLE(mac, ios) (MTLPP_IS_AVAILABLE_MAC(mac) || MTLPP_IS_AVAILABLE_IOS(ios))

#define MTLPP_PLATFORM_MAC !TARGET_OS_IPHONE
#define MTLPP_PLATFORM_IOS TARGET_OS_IPHONE

#ifdef __OBJC__
#define MTLPP_IF_AVAILABLE(macvers, iosvers) @available(macOS macvers, iOS iosvers, *)
#define MTLPP_IF_AVAILABLE_MAC(vers) @available(macOS vers, *)
#define MTLPP_IF_AVAILABLE_IOS(vers) @available(iOS vers, *)
#else
#define MTLPP_IF_AVAILABLE(mac, ios) false
#define MTLPP_IF_AVAILABLE_MAC(mac) false
#define MTLPP_IF_AVAILABLE_IOS(ios) false
#endif

