/*
 * Copyright 1995, 1997 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * ntmangle.h -- demangle paths on NT
 *
 * Public functions:
 *
 *	NtDemanglePath() - set a StrBuf's value using a mangled path.
 *			The demangled value is actually saved.
 */

void NtDemanglePath( char *path83, StrBuf *dest );

