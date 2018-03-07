/*
 * Copyright 1995, 2003 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

# define DEBUG_MATCH	( p4debug.GetLevel( DT_MAP ) >= 3 )
# define DEBUG_LIST	( p4debug.GetLevel( DT_MAP ) >= 4 )

class StrArray;

class Ignore {

    public:
			Ignore();
			~Ignore();

	int		Reject( const StrPtr &path, const StrPtr &ignoreName );
	int		RejectCheck( const StrPtr &path );

    private:
	void		Insert( StrArray *subList, const char *ignore, 
			        const char *cwd );

	StrArray	*ignoreList;
	StrBuf		dirDepth;
};
