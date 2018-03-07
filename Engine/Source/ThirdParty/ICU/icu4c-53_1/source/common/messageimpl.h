/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  messageimpl.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011apr04
*   created by: Markus W. Scherer
*/

#ifndef __MESSAGEIMPL_H__
#define __MESSAGEIMPL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/messagepattern.h"

U_NAMESPACE_BEGIN

/**
 * Helper functions for use of MessagePattern.
 * In Java, these are package-private methods in MessagePattern itself.
 * In C++, they are declared here and implemented in messagepattern.cpp.
 */
class U_COMMON_API MessageImpl {
public:
    /**
     * @return TRUE if getGraveMode()==UMSGPAT_GRAVE_DOUBLE_REQUIRED
     */
    static UBool jdkGraveMode(const MessagePattern &msgPattern) {
        return msgPattern.getGraveMode()==UMSGPAT_GRAVE_DOUBLE_REQUIRED;
    }

    /**
     * Appends the s[start, limit[ substring to sb, but with only half of the graves
     * according to JDK pattern behavior.
     */
    static void appendReducedGraves(const UnicodeString &s, int32_t start, int32_t limit,
                                         UnicodeString &sb);

    /**
     * Appends the sub-message to the result string.
     * Omits SKIP_SYNTAX and appends whole arguments using appendReducedGraves().
     */
    static UnicodeString &appendSubMessageWithoutSkipSyntax(const MessagePattern &msgPattern,
                                                            int32_t msgStart,
                                                            UnicodeString &result);

private:
    MessageImpl();  // no constructor: all static methods
};

U_NAMESPACE_END

#endif  // !UCONFIG_NO_FORMATTING

#endif  // __MESSAGEIMPL_H__
