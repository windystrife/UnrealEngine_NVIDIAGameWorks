/*
 * Copyright 1995, 2001 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * CharMan - Character manipulation support for i18n environments
 */

# include <ctype.h>

# define isAhighchar(x)	( (0x80 & *(x)) != 0 )

# define isAcntrl(x)	( ! isAhighchar(x) && iscntrl(*(x)) )

# define isAdigit(x)	( ! isAhighchar(x) && isdigit(*(x)) )

# define isAprint(x)	( isAhighchar(x) || isprint(*(x)) )

# define isAspace(x)	( ! isAhighchar(x) && isspace(*(x)) )

# define isAalnum(x)	( isAhighchar(x) || isalnum(*(x)) )

# define toAupper(x)	( isAhighchar(x) ? *(x) : toupper(*(x)) )

# define toAlower(x)	( isAhighchar(x) ? *(x) : tolower(*(x)) )

/*
 * tolowerq -- quick version for NT that doesn't use SLOW locale stuff
 * toupperq -- quick version for NT that doesn't use SLOW locale stuff
 */

# define tolowerq(x) ((x)>='A'&&(x)<='Z'?(x)-'A'+'a':(x))
# define toupperq(x) ((x)>='a'&&(x)<='z'?(x)-'a'+'A':(x))

class CharStep {
public:
	CharStep( char * p ) : ptr(p) {}

	virtual char *Next();
	char *Next( int );

	char *Ptr() const { return ptr; }

	int CountChars( char *e );

	static CharStep * Create ( char * p, int charset = 0 );
protected:
	char * ptr;
};

class CharStepUTF8 : public CharStep {
public:
	CharStepUTF8( char * p ) : CharStep( p ) {}
	char *Next();
};

class CharStepShiftJis : public CharStep {
public:
	CharStepShiftJis( char * p ) : CharStep( p ) {}
	char *Next();
};

class CharStepEUCJP : public CharStep {
public:
	CharStepEUCJP( char * p ) : CharStep( p ) {}
	char *Next();
};

class CharStepCP949 : public CharStep {
public:
	CharStepCP949( char * p ) : CharStep( p ) {}
	char *Next();
};

class CharStepCN : public CharStep {
public:
	CharStepCN( char * p ) : CharStep( p ) {}
	char *Next();
};
