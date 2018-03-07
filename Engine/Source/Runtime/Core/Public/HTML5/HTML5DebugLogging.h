// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

extern "C" {

/* If specified, logs directly to the browser console/inspector 
 * window. If not specified, logs via the application Module. */
#define EM_LOG_CONSOLE   1
/* If specified, prints a warning message. */
#define EM_LOG_WARN      2
/* If specified, prints an error message. If neither EM_LOG_WARN 
 * or EM_LOG_ERROR is specified, an info message is printed.
 * EM_LOG_WARN and EM_LOG_ERROR are mutually exclusive. */
#define EM_LOG_ERROR     4
/* If specified, prints a callstack that contains filenames referring 
 * to original C sources using source map information. */
#define EM_LOG_C_STACK   8
/* If specified, prints a callstack that contains filenames referring
 * to lines to the built .js/.html file along with the message. The 
 * flags EM_LOG_C_STACK and EM_LOG_JS_STACK can be combined to output 
 * both untranslated and translated file+line information. */
#define EM_LOG_JS_STACK 16
/* If specified, C/C++ function names are demangled before printing. 
 * Otherwise, the mangled post-compilation JS function names are 
 * displayed. */
#define EM_LOG_DEMANGLE 32
/* If specified, the pathnames of the file information in the call 
 * stack will be omitted. */
#define EM_LOG_NO_PATHS 64

/**
 * Prints out a message to the console, optionally with the 
 * callstack information.
 * @param flags A binary OR of items from the list of EM_LOG_xxx 
 *                 flags that specify printing options.
 * @param '...' A printf-style "format, ..." parameter list that 
 *                 is parsed according to the printf formatting rules.
 */
void emscripten_log(int flags, ...);

/**
 * Programmatically obtains the current callstack.
 * @param flags    A binary OR of items from the list of EM_LOG_xxx 
 *                    flags that specify printing options. The 
 *                    items EM_LOG_CONSOLE, EM_LOG_WARN and 
 *                    EM_LOG_ERROR do not apply in this function and 
 *                    are ignored.
 * @param out      A pointer to a memory region where the callstack 
 *                    string will be written to. The string outputted 
 *                    by this function will always be null-terminated.
 * @param maxbytes The maximum number of bytes that this function can
 *                    write to the memory pointed to by 'out'. If 
 *                    there is no enough space, the output will be 
 *                    truncated (but always null-terminated).
 * @return Returns the number of bytes written. (not number of 
 *         characters, so this will also include the terminating zero)
 
 * To query the amount of bytes needed for a callstack without writing 
 * it, pass 0 to 'out' and 'maxbytes', in which case the function will
 * return the number of bytes (including the terminating zero) that 
 * will be needed to hold the full callstack. Note that this might be 
 * fully accurate since subsequent calls will carry different line 
 * numbers, so it is best to allocate a few bytes extra to be safe.
 */
int emscripten_get_callstack(int flags, char *out, int maxbytes);

}
