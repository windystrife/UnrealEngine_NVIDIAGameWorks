// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This code is modified from that in the Mesa3D Graphics library available at
// http://mesa3d.org/
// The license for the original code follows:

/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ShaderCompilerCommon.h"
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "hlslcc.h"
#if __APPLE__
#else
#include <malloc.h>
#endif

/* Android defines SIZE_MAX in limits.h, instead of the standard stdint.h */
#ifdef ANDROID
#include <limits.h>
#endif

/* Some versions of MinGW are missing _vscprintf's declaration, although they
 * still provide the symbol in the import library. */
#ifdef __MINGW32__
_CRTIMP int _vscprintf(const char *format, va_list argptr);
#endif

#include "ralloc.h"

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)
#else
#define likely(x)       !!(x)
#define unlikely(x)     !!(x)
#endif

#ifndef va_copy
#ifdef __va_copy
#define va_copy(dest, src) __va_copy((dest), (src))
#else
#define va_copy(dest, src) (dest) = (src)
#endif
#endif

#define CANARY 0x5A1106

/**
 * If enabled, allocate large blocks of memory from the OS and use them as
 * simple linear allocators for ralloc contexts. This wastes some memory but
 * is much faster than asking the OS to manage tens of thousands of small
 * allocations.
 */
#define USE_MEM_BLOCKS 1

/**
 * The minimum size to allocate for blocks from the OS. There is a balance here
 * between memory and speed. 2048k blocks are a good balance that require only
 * a small memory overhead but greatly increase speed. 4096k blocks increase
 * speed by another order of magnitude but also increase memory overhead
 * significantly. 8192k is the sweet spot for speed but requires >3x the memory
 * of 2048k blocks.
 */
#define MIN_BLOCK_SIZE_LOG2 12
#define MIN_BLOCK_SIZE (1 << MIN_BLOCK_SIZE_LOG2)

#if USE_MEM_BLOCKS
struct mem_block
{
	/** Top of the memory stack. */
	char *top;
	/** End of the memory block. */
	char *end;
	/** Pointer to the next block in the list. */
	struct mem_block *next_block;
};
#endif // #if USE_MEM_BLOCKS

struct ralloc_header
{
   /* A canary value used to determine whether a pointer is ralloc'd. */
   unsigned canary;

#if USE_MEM_BLOCKS
   /* Size of the allocation. */
   unsigned size;
   /* The linked list of memory blocks (if any) used for this memory context. */
   struct mem_block *mem_blocks;
#endif // #if USE_MEM_BLOCKS

   struct ralloc_header *parent;

   /* The first child (head of a linked list) */
   struct ralloc_header *child;

   /* Linked list of siblings */
   struct ralloc_header *prev;
   struct ralloc_header *next;

   void (*destructor)(void *);
};

typedef struct ralloc_header ralloc_header;

static void unlink_block(ralloc_header *info);
static void unsafe_free(ralloc_header *info);

static ralloc_header *
get_header(const void *ptr)
{
   ralloc_header *info = (ralloc_header *) (((char *) ptr) -
					    sizeof(ralloc_header));
   check(info->canary == CANARY);
   return info;
}

#define PTR_FROM_HEADER(info) (((char *) info) + sizeof(ralloc_header))

static void
add_child(ralloc_header *parent, ralloc_header *info)
{
   if (parent != NULL) {
      info->parent = parent;
      info->next = parent->child;
      parent->child = info;

      if (info->next != NULL)
	 info->next->prev = info;
   }
}

void *
ralloc_context(const void *ctx)
{
   return ralloc_size(ctx, 0);
}

#if USE_MEM_BLOCKS
/**
 * Allocates a new block from the OS.
 */
static struct mem_block *
ralloc_new_mem_block(size_t size)
{
	struct mem_block *mblock;
	size_t block_size;
	
	block_size = size + sizeof(struct mem_block);
	block_size = (block_size + MIN_BLOCK_SIZE - 1) & (~(MIN_BLOCK_SIZE-1));
	mblock = (struct mem_block*)calloc(1, block_size);
	mblock->top = (char *)mblock + sizeof(struct mem_block);
	mblock->end = (char *)mblock + block_size;
	mblock->next_block = NULL;
	return mblock;
}

/** Alignment of block allocations. */
#define BLOCK_ALIGNMENT_LOG2 3
#define BLOCK_ALIGNMENT (1 << BLOCK_ALIGNMENT_LOG2)
#define BLOCK_ALIGN(x) (((x) + BLOCK_ALIGNMENT - 1) & (~(BLOCK_ALIGNMENT-1)))

/**
 * Allocate from a memory block.
 */
static void *
ralloc_block(ralloc_header *parent, size_t size)
{
	struct mem_block *mblock;
	ralloc_header *mem;

	size = BLOCK_ALIGN(size);

	if (unlikely(parent == NULL))
	{
		mblock = ralloc_new_mem_block(size);
		parent = (ralloc_header *)(mblock->top);
		parent->mem_blocks = mblock;
	}

	mblock = parent->mem_blocks;

	if (unlikely(mblock == NULL || (mblock->end - mblock->top) < size))
	{
		mblock = ralloc_new_mem_block(size);
		mblock->next_block = parent->mem_blocks;
		parent->mem_blocks = mblock;
	}

	mem = (ralloc_header*)mblock->top;
	mem->size = (unsigned)size;
	mblock->top += size;
	return mem;
}

/**
 * Resize an allocation residing in a memory block.
 */
static void *
ralloc_block_resize(ralloc_header *old, size_t size)
{
	struct mem_block *mblock;
	struct mem_block *unused_block = 0;
	ralloc_header *parent;
	ralloc_header *new_mem;
	size_t old_size;

	size = BLOCK_ALIGN(size);

	if (likely(old->parent != NULL))
		parent = old->parent;
	else
		parent = old;

	mblock = parent->mem_blocks;
	if (mblock &&
		(mblock->top - old->size) == (char*)old &&
		(mblock->end - (char*)old) >= size)
	{
		mblock->top = (char*)old + size;
		old->size = (unsigned)size;
		return old;
	}

	if (mblock &&
		(char*)old == (char*)mblock + sizeof(struct mem_block) &&
		(char*)old + old->size == mblock->top)
	{
		parent->mem_blocks = mblock->next_block;
		unused_block = mblock;		
	}

	new_mem = (ralloc_header*)ralloc_block(parent, size);
	old_size = old->size;
	memcpy(new_mem, old, old_size);
	new_mem->size = (unsigned)size;

	if (unused_block)
	{
		free(unused_block);
	}

	return new_mem;
}

/**
 * Free a memory block.
 */
void ralloc_block_free(struct mem_block *in_mblock)
{
	struct mem_block *mblock;
	struct mem_block *next_block;

	mblock = in_mblock;
	while (mblock)
	{
		next_block = mblock->next_block;
		free(mblock);
		mblock = next_block;
	}
}
#endif // #if USE_MEM_BLOCKS

void *
ralloc_size(const void *ctx, size_t size)
{
   ralloc_header *parent = ctx != NULL ? get_header(ctx) : NULL;

#if USE_MEM_BLOCKS
   void *block = ralloc_block(parent, size + sizeof(ralloc_header));
#else
   void *block = calloc(1, size + sizeof(ralloc_header));
#endif // #if !USE_MEM_BLOCKS

   ralloc_header *info = (ralloc_header *) block;

   add_child(parent, info);

   info->canary = CANARY;

   return PTR_FROM_HEADER(info);
}

void *
rzalloc_size(const void *ctx, size_t size)
{
   void *ptr = ralloc_size(ctx, size);
   if (likely(ptr != NULL))
      memset(ptr, 0, size);
   return ptr;
}

/* helper function - assumes ptr != NULL */
static void *
resize(void *ptr, size_t size)
{
   ralloc_header *child, *old, *info;

   old = get_header(ptr);
#if USE_MEM_BLOCKS
   info = (ralloc_header*)ralloc_block_resize(old, size + sizeof(ralloc_header));
#else
   info = realloc(old, size + sizeof(ralloc_header));
#endif

   if (info == NULL)
      return NULL;

   /* Update parent and sibling's links to the reallocated node. */
   if (info != old && info->parent != NULL) {
      if (info->parent->child == old)
	 info->parent->child = info;

      if (info->prev != NULL)
	 info->prev->next = info;

      if (info->next != NULL)
	 info->next->prev = info;
   }

   /* Update child->parent links for all children */
   for (child = info->child; child != NULL; child = child->next)
      child->parent = info;

   return PTR_FROM_HEADER(info);
}

void *
reralloc_size(const void *ctx, void *ptr, size_t size)
{
   if (unlikely(ptr == NULL))
      return ralloc_size(ctx, size);

   check(ralloc_parent(ptr) == ctx);
   return resize(ptr, size);
}

void *
ralloc_array_size(const void *ctx, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return ralloc_size(ctx, size * count);
}

void *
rzalloc_array_size(const void *ctx, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return rzalloc_size(ctx, size * count);
}

void *
reralloc_array_size(const void *ctx, void *ptr, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return reralloc_size(ctx, ptr, size * count);
}

void
ralloc_free(void *ptr)
{
   ralloc_header *info;

   if (ptr == NULL)
      return;

   info = get_header(ptr);
   unlink_block(info);
   unsafe_free(info);
}

static void
unlink_block(ralloc_header *info)
{
   /* Unlink from parent & siblings */
   if (info->parent != NULL) {
      if (info->parent->child == info)
	 info->parent->child = info->next;

      if (info->prev != NULL)
	 info->prev->next = info->next;

      if (info->next != NULL)
	 info->next->prev = info->prev;
   }
   info->parent = NULL;
   info->prev = NULL;
   info->next = NULL;
}

static void unsafe_free(ralloc_header *info)
{
   /* Recursively free any children...don't waste time unlinking them. */
	ralloc_header *temp;
	while (info->child != NULL)
	{
      temp = info->child;
      info->child = temp->next;
      unsafe_free(temp);
   }

   /* Free the block itself.  Call the destructor first, if any. */
   if (info->destructor != NULL)
      info->destructor(PTR_FROM_HEADER(info));

#if USE_MEM_BLOCKS
   ralloc_block_free(info->mem_blocks);
#else
   free(info);
#endif // #if USE_MEM_BLOCKS
}

void * ralloc_parent(const void *ptr)
{
   ralloc_header *info;

   if (unlikely(ptr == NULL))
      return NULL;

   info = get_header(ptr);
   return PTR_FROM_HEADER(info->parent);
}

static void *autofree_context = NULL;

static void autofree(void)
{
   ralloc_free(autofree_context);
   
   FCRTMemLeakScope Scope(true);
}

void * ralloc_autofree_context(void)
{
   if (unlikely(autofree_context == NULL))
	{
      autofree_context = ralloc_context(NULL);
      atexit(autofree);
   }
   return autofree_context;
}

void ralloc_set_destructor(const void *ptr, void(*destructor)(void *))
{
   ralloc_header *info = get_header(ptr);
   info->destructor = destructor;
}

char * ralloc_strdup(const void *ctx, const char *str)
{
   size_t n;
   char *ptr;

   if (unlikely(str == NULL))
      return NULL;

   n = strlen(str);
   ptr = ralloc_array(ctx, char, (unsigned int)n + 1);
   memcpy(ptr, str, n);
   ptr[n] = '\0';
   return ptr;
}

char * ralloc_strndup(const void *ctx, const char *str, size_t max)
{
   size_t n;
   char *ptr;

   if (unlikely(str == NULL))
      return NULL;

   n = strlen(str);
   if (n > max)
      n = max;

   ptr = ralloc_array(ctx, char, (unsigned int)n + 1);
   memcpy(ptr, str, n);
   ptr[n] = '\0';
   return ptr;
}

/* helper routine for strcat/strncat - n is the exact amount to copy */
static bool
cat(char **dest, const char *str, size_t n)
{
   char *both;
   size_t existing_length;
   check(dest != NULL && *dest != NULL);

   existing_length = strlen(*dest);
   both = (char*)resize(*dest, existing_length + n + 1);
   if (unlikely(both == NULL))
      return false;

   memcpy(both + existing_length, str, n);
   both[existing_length + n] = '\0';

   *dest = both;
   return true;
}


bool
ralloc_strcat(char **dest, const char *str)
{
   return cat(dest, str, strlen(str));
}

bool
ralloc_strncat(char **dest, const char *str, size_t n)
{
   /* Clamp n to the string length */
   size_t str_length = strlen(str);
   if (str_length < n)
      n = str_length;

   return cat(dest, str, n);
}

char *
ralloc_asprintf(const void *ctx, const char *fmt, ...)
{
   char *ptr;
   va_list args;
   va_start(args, fmt);
   ptr = ralloc_vasprintf(ctx, fmt, args);
   va_end(args);
   return ptr;
}

/* Return the length of the string that would be generated by a printf-style
 * format and argument list, not including the \0 byte.
 */
static size_t
printf_length(const char *fmt, va_list untouched_args)
{
   int size;
   char junk;

   /* Make a copy of the va_list so the original caller can still use it */
   va_list args;
   va_copy(args, untouched_args);

#ifdef _WIN32
   /* We need to use _vcsprintf to calculate the size as vsnprintf returns -1
    * if the number of characters to write is greater than count.
    */
   size = _vscprintf(fmt, args);
   (void)junk;
#else
   size = vsnprintf(&junk, 1, fmt, args);
#endif
   check(size >= 0);

   va_end(args);

   return size;
}

char *
ralloc_vasprintf(const void *ctx, const char *fmt, va_list args)
{
   size_t size = printf_length(fmt, args) + 1;

   char *ptr = (char*)ralloc_size(ctx, size);
   if (ptr != NULL)
      vsnprintf(ptr, size, fmt, args);

   return ptr;
}

bool
ralloc_asprintf_append(char **str, const char *fmt, ...)
{
   bool success;
   va_list args;
   va_start(args, fmt);
   success = ralloc_vasprintf_append(str, fmt, args);
   va_end(args);
   return success;
}

bool
ralloc_vasprintf_append(char **str, const char *fmt, va_list args)
{
   size_t existing_length;
   check(str != NULL);
   existing_length = *str ? strlen(*str) : 0;
   return ralloc_vasprintf_rewrite_tail(str, &existing_length, fmt, args);
}

bool
ralloc_asprintf_rewrite_tail(char **str, size_t *start, const char *fmt, ...)
{
   bool success;
   va_list args;
   va_start(args, fmt);
   success = ralloc_vasprintf_rewrite_tail(str, start, fmt, args);
   va_end(args);
   return success;
}

bool
ralloc_vasprintf_rewrite_tail(char **str, size_t *start, const char *fmt,
			      va_list args)
{
   size_t new_length;
   char *ptr;

   check(str != NULL);

   if (unlikely(*str == NULL)) {
      // Assuming a NULL context is probably bad, but it's expected behavior.
      *str = ralloc_vasprintf(NULL, fmt, args);
      return true;
   }

   new_length = printf_length(fmt, args);

   ptr = (char*)resize(*str, *start + new_length + 1);
   if (unlikely(ptr == NULL))
      return false;

   vsnprintf(ptr + *start, new_length + 1, fmt, args);
   *str = ptr;
   *start += new_length;
   return true;
}
