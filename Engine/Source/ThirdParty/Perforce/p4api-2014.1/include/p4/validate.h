/*
 * Copyright 1995, 2003 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * ValidateCharSet
 */

class CharSetValid {
    public:
	virtual ~CharSetValid();
	virtual void Reset() = 0;
	virtual int Valid( const char *buf, int len, const char **retp = 0 ) = 0;
};

class CharSetUTF8Valid : public CharSetValid {
	int	followcnt;
	int	magic;
	static unsigned char validmap[256];
    public:
	CharSetUTF8Valid();
	void Reset();
	int Valid( const char *buf, int len, const char **retp = 0 );
};
