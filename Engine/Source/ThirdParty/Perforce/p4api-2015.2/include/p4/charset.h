/*
 * Copyright 2005 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * GlobalCharSet -- a static charSet index across the process.
 */

class GlobalCharSet {

    public:
	static void	Set( int x = 0 ) { globCharSet = x; }
	static int	Get() { return globCharSet; }

    private:
	static int	globCharSet;
} ;
