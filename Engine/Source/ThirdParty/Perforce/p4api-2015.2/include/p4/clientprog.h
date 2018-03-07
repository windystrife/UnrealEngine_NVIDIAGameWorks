/*
 * Copyright 1995, 2011 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

// client Progress type

#define CPT_SENDFILE	1
#define CPT_RECVFILE	2
#define CPT_FILESTRANS	3
#define CPT_COMPUTATION	4

#define CPU_UNSPECIFIED	0
#define CPU_PERCENT	1
#define	CPU_FILES	2
#define CPU_KBYTES	3
#define CPU_MBYTES	4

class ClientProgress
{
    public:
	virtual ~ClientProgress() {};
	virtual void	Description( const StrPtr *desc, int units ) = 0;
	virtual void	Total( long ) = 0;
	virtual int	Update( long ) = 0;
	virtual void	Done( int fail ) = 0;
};

class ClientProgressText : public ClientProgress
{
    public:
	ClientProgressText( int );
	virtual ~ClientProgressText();
	void	Description( const StrPtr *description, int units );
	void	Total( long );
	int	Update( long );
	void	Done( int fail );
    private:
	int	cnt;
	long	total;
	int	typeOfProgress;
	int	backup;
	StrBuf	desc;
};
