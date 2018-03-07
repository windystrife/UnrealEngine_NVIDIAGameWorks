/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * StrBuf.h - multipurpose buffers
 *
 * StrPtr, StrRef, and StrBuf are used throughout the system, as buffers
 * for storing just about any variable length byte data. 
 *
 * StrPtr is a low-cost (no constructor, no destructor, 8 byte) 
 * pointer/length pair to mutable data.  It has a variety of methods
 * to mangle it.
 *
 * StrRef is a kind-of StrPtr that allows the buffer pointer to be set.
 * As StrPtr doesn't allow this, a StrPtr object itself isn't useful.
 *
 * StrNum is a kind-of StrPtr with a temporary buffer whose only purpose
 * is to hold the string representation of an int.
 *
 * StrBuf is a kind-of StrPtr that allocates and extends it own buffer.
 *
 * StrFixed is a kind-of StrPtr that points to a character array that
 * is fixed at construction.
 *
 * Classes:
 *
 *	StrPtr - a pointer/length for arbitrary data
 *	StrRef - StrPtr that can be set
 *	StrBuf - StrPtr of privately allocated data
 *	StrFixed - StrPtr to a fixed length char buffer
 *	StrNum - StrPtr that holds a string of an int
 *	StrHuman - StrPtr that holds a "human-readable" string of an int
 *
 * Methods:
 *
 *	StrPtr::Clear() - set length = 0
 *	StrPtr::Text() - return buffer pointer
 *	StrPtr::Value() - return buffer pointer (old name)
 *	StrPtr::Length() - return buffer length
 *	StrPtr::GetEnd() - return pointer to character past end
 *	StrPtr::Atoi() - convert to integer and return
 *	StrPtr::Atoi64() - convert to P4INT64 and return
 *	StrPtr::Itoa() - format an int given the end of a buffer
 *	StrPtr::Itoa64() - format a P4INT64 given the end of a buffer
 *	StrPtr::SetLength() - set only length
 *	StrPtr::SetEnd() - set length by calculating from start
 *	StrPtr::[] - get a single character
 *	StrPtr::XCompare() - case exact string compare
 *	StrPtr::CCompare() - case folding string compare
 *	StrPtr::SCompare() - case aware string compare -- see strbuf.cc
 *	StrPtr::SEqual() - case aware character compare -- see strbuf.cc
 *	StrPtr::Contains() - finds a substring
 *	StrPtr::== - compare contents with buffer
 *	StrPtr::!= - compare contents with buffer
 *	StrPtr::< - compare contents with buffer
 *	StrPtr::<= - compare contents with buffer
 *	StrPtr::> - compare contents with buffer
 *	StrPtr::>= - compare contents with buffer
 *	StrPtr::StrCpy() - copy string out to a buffer
 *	StrPtr::StrCat() - copy string out to end of a buffer
 *	StrPtr::CaseFolding() - (static) SCompare sorts A < a, a < B
 *	StrPtr::CaseIgnored() - (static) SCompare sorts A == a, a < B
 *	StrPtr::CaseHybrid() - (static) SCompare sorts Ax < ax, aa < AX
 *	StrPtr::SetCaseFolding() - (static) 0=UNIX, 1=NT, 2=HYBRID
 *
 *	---
 *
 *	StrRef::Set() - set pointer/length
 *	StrRef::+= - move pointer/length
 *
 *	---
 *
 *	StrBuf::StringInit() - mimic actions of constructor
 *	StrBuf::Set() - allocate and fill from buffer
 *	StrBuf::Append() - extend and terminate from buffer
 *	StrBuf::Extend() - append contents from buffer
 *	StrBuf::Terminate() - terminate buffer
 *	StrBuf::Alloc() - allocate space in buffer and return pointer
 *	StrBuf::<< - Append contents from buffer or number
 *	StrBuf::Indent() - fill by indenting contents of another buffer
 *	StrBuf::Expand() - expand a string doing %var substitutions
 *	
 */

class StrBuf;
class StrNum;

// On 64 bit platforms, the base 'size_t' type is 64 bits, which is much
// more than we need, or can handle. So we use our own size_t type instead;
// it's "p4size_t", defined in stdhdrs.h

// General String Buffer Sizes
# define SIZE_LINESTR     256
# define SIZE_SMALLSTR   1024
# define SIZE_MEDSTR     4096

class StrPtr {

    public:
	// Setting, getting

	char *	Text() const
		{ return buffer; }

	char *	Value() const
		{ return buffer; }

	unsigned char *UText() const
		{ return (unsigned char *)Text(); }

	p4size_t 	Length() const
		{ return length; }

	char *	End() const
		{ return Text() + length; }

	unsigned char *UEnd() const
		{ return UText() + length; }

	int	Atoi() const
		{ return Atoi( buffer ); }

	bool	IsNumeric() const;

	int	EndsWith( const char *s, int l ) const;

	P4INT64	Atoi64() const
		{ return Atoi64( buffer ); }

	void	SetLength() 
		{ length = (p4size_t)strlen( buffer ); }

	void	SetLength( p4size_t len )
		{ length = len; }

	void	SetEnd( char *p ) 
		{ length = (p4size_t)(p - buffer); }

	char	operator[]( p4size_t x ) const
		{ return buffer[x]; }

	// Compare -- p4ftp legacy

	int	Compare( const StrPtr &s ) const
		{ return SCompare( s ); }

	// CCompare/SCompare/XCompare

	int	CCompare( const StrPtr &s ) const
		{ return CCompare( buffer, s.buffer ); }

	int	SCompare( const StrPtr &s ) const
		{ return SCompare( buffer, s.buffer ); }

	int	NCompare( const StrPtr &s ) const
		{ return NCompare( buffer, s.buffer ); }

	static int CCompare( const char *a, const char *b );
	static int SCompare( const char *a, const char *b );
	static int NCompare( const char *a, const char *b );

	static int SCompare( unsigned char a, unsigned char b )
		{
		    return a==b ? 0 : SCompareF( a, b );
		}

	static int SEqual( unsigned char a, unsigned char b )
		{ 
		    switch( a^b ) 
		    { 
		    default: return 0;
		    case 0: return 1;
		    case 'A'^'a': return SEqualF( a, b );
		    }
		}

	int	SCompareN( const StrPtr &s ) const;

	int	XCompare( const StrPtr &s ) const
		{ return strcmp( buffer, s.buffer ); }

	static int XCompare( const char *a, const char *b )
		{ return strcmp( a, b ); }

	int	XCompareN( const StrPtr &s ) const
		{ return strncmp( buffer, s.buffer, length ); }

	// More comparing

	const char *Contains( const StrPtr &s ) const
		{ return strstr( Text(), s.Text() ); }

	bool	operator ==( const char *buf ) const
		{ return strcmp( buffer, buf ) == 0; }

	bool	operator !=( const char *buf ) const
		{ return strcmp( buffer, buf ) != 0; }

	bool	operator <( const char *buf ) const
		{ return strcmp( buffer, buf ) < 0; }

	bool	operator <=( const char *buf ) const
		{ return strcmp( buffer, buf ) <= 0; }

	bool	operator >( const char *buf ) const
		{ return strcmp( buffer, buf ) > 0; }

	bool	operator >=( const char *buf ) const
		{ return strcmp( buffer, buf ) >= 0; }

	bool	operator ==( const StrPtr &s ) const
		{ return strcmp( buffer, s.buffer ) == 0; }

	bool	operator !=( const StrPtr &s ) const
		{ return strcmp( buffer, s.buffer ) != 0; }

	bool	operator <( const StrPtr &s ) const
		{ return strcmp( buffer, s.buffer ) < 0; }

	bool	operator <=( const StrPtr &s ) const
		{ return strcmp( buffer, s.buffer ) <= 0; }

	bool	operator >( const StrPtr &s ) const
		{ return strcmp( buffer, s.buffer ) > 0; }

	bool	operator >=( const StrPtr &s ) const
		{ return strcmp( buffer, s.buffer ) >= 0; }

	// Copying out
	// Includes EOS

	void	StrCpy( char *b ) const
		{ memcpy( b, buffer, length + 1 ); }

	void	StrCat( char *b ) const
		{ memcpy( b + strlen( b ), buffer, length + 1 ); }

	// check for identical underlying objects or overlaps

	bool	CheckSame( const StrPtr *a ) const
	    { return Text() == a->Text() && Length() == a->Length(); }
	bool	CheckOverlap( const StrPtr *a ) const
	    { return End() > a->Text() && a->End() > Text(); }

	// Formatting and parsing numbers as strings

	static int Atoi( const char *b ) { return atoi( b ); }
	static char *Itoa( int v, char *e ) { return Itoa64( v, e ); }

	static P4INT64 Atoi64( const char *buffer );
	static char *Itoa64( P4INT64 v, char *endbuf );
	static char *Itox( unsigned int v, char *endbuf );

    friend class StrBuf;
    friend class StrRef;

    protected:
	char	*buffer;
	p4size_t	length;

    public:

	// Case sensitive server?

	static bool CaseFolding()
		{ return caseUse != ST_UNIX; }

	static bool CaseIgnored()
		{ return caseUse == ST_WINDOWS; }

	static bool CaseHybrid()
		{ return caseUse == ST_HYBRID; }

	static void SetCaseFolding( int c )
		{ caseUse = (CaseUse)c; foldingSet = true; }

	static bool CaseFoldingAlreadySet()
		{ return foldingSet; }

	enum CaseUse { ST_UNIX, ST_WINDOWS, ST_HYBRID };

	static CaseUse CaseUsage() { return caseUse; }

    private:

	static CaseUse caseUse;
	static bool foldingSet;

	static int SEqualF( unsigned char a, unsigned char b );
	static int SCompareF( unsigned char a, unsigned char b );

	static int NCompareLeft( const unsigned char *a, 
	                         const unsigned char *b );
	static int NCompareRight( const unsigned char *a, 
	                          const unsigned char *b );
} ;

class StrRef : public StrPtr {

    public:

		StrRef() {}

		StrRef( const StrRef &s )
		{ Set( &s ); }

		StrRef( const StrPtr &s )
		{ Set( &s ); }

		StrRef( const char *buf )
		{ Set( (char *)buf ); }

		StrRef( const char *buf, p4size_t len )
		{ Set( (char *)buf, len ); }

	static const StrPtr &Null()
		{ return null; }

	const StrRef & operator =(const StrRef &s)
		{ Set( &s ); return *this; }

	const StrRef & operator =(const StrPtr &s)
		{ Set( &s ); return *this; }

	const StrRef & operator =(const char *buf)
		{ Set( (char *)buf ); return *this; }

	void	operator +=( int l )
		{ buffer += l; length -= l; }

	void 	Set( char *buf )
		{ Set( buf, (p4size_t)strlen( buf ) ); }
		 
	void	Set( char *buf, p4size_t len )
		{ buffer = buf; length = len; }

	void	Set( const StrPtr *s )
		{ Set( s->buffer, s->length ); }

	void	Set( const StrPtr &s )
		{ Set( s.buffer, s.length ); }

    private:
    	static	StrRef null;

} ;

class StrBuf : public StrPtr {

    public:
		StrBuf() 
		{ StringInit(); }

	void	StringInit()
		{ length = size = 0; buffer = nullStrBuf; }

		~StrBuf()
		{ if( buffer != nullStrBuf ) delete []buffer; }

	// copy constructor, assignment

		StrBuf( const StrBuf &s )
		{ StringInit(); Set( &s ); }

		StrBuf( const StrRef &s )
		{ StringInit(); Set( &s ); }

		StrBuf( const StrPtr &s )
		{ StringInit(); Set( &s ); }

		StrBuf( const char *buf )
		{ StringInit(); Set( buf ); }

	const StrBuf & operator =(const StrBuf &s)
		{ Set( &s ); return *this; }

	const StrBuf & operator =(const StrRef &s)
		{ Set( &s ); return *this; }

	const StrBuf & operator =(const StrPtr &s)
		{ Set( &s ); return *this; }

	const StrBuf & operator =(const char *buf)
		{ if( (const char*)this != buf ) Set( buf ); return *this; }

	// Setting, getting

	void 	Clear( void )
		{ length = 0; }

	void 	Reset( void )
		{ 
		    if( buffer != nullStrBuf ) 
		    {
	                delete []buffer; 
		
		        length = size = 0; 
		        buffer = nullStrBuf; 
		    }
		}

	void	Reset( const char *buf )
		{ Reset(); UAppend( buf ); }

	void	Reset( const StrPtr *s )
		{ Reset(); UAppend( s ); }

	void 	Reset( const StrPtr &s )
		{ Reset(); UAppend( &s ); }

	void	Set( const char *buf )
	    { if( buf == Text() ) SetLength(); else { Clear(); Append( buf ); } }

	void	Set( const StrPtr *s )
	    { if( s->Text() != Text() ) { Clear(); UAppend( s ); } }

	void	Set( const StrPtr &s )
	    { if( s.Text() != Text() ) { Clear(); UAppend( &s ); } }

	void	Set( const char *buf, p4size_t len )
	    { if( buf == Text() ) SetLength( len ); else { Clear(); Append( buf, len ); } }

	void	Extend( const char *buf, p4size_t len )
		{ memcpy( Alloc( len ), buf, len ); }

	void	Extend( char c )
		{ *Alloc(1) = c; }

	void 	Terminate() 
		{ Extend(0); --length; }

	void	TruncateBlanks();     // Removes blanks just from the end
	void	TrimBlanks();         // Removes blanks from start and end

	void	Append( const char *buf );     

	void	Append( const StrPtr *s );

	void	Append( const char *buf, p4size_t len );

	void	UAppend( const char *buf );     

	void	UAppend( const StrPtr *s );

	void	UAppend( const char *buf, p4size_t len );

	// large-block append
	void	BlockAppend( const char *buf );

	void	BlockAppend( const StrPtr *s );

	void	BlockAppend( const char *buf, p4size_t len );

	void	UBlockAppend( const char *buf );

	void	UBlockAppend( const StrPtr *s );

	void	UBlockAppend( const char *buf, p4size_t len );

	char *	Alloc( p4size_t len )
		{
		    p4size_t oldlen = length;

		    if( ( length += len ) > size )
			Grow( oldlen );

		    return buffer + oldlen;
		}

	// large block (>= 128KB) allocation; no extra space is reserved
	char *	BlockAlloc( p4size_t len )
		{
		    p4size_t oldlen = length;

		    if( ( length += len ) > size )
			Reserve( oldlen );

		    return buffer + oldlen;
		}

        void    Fill( const char *buf, p4size_t len );

        void    Fill( const char *buf )
                {
		    Fill( buf, Length() );
		}

	p4size_t 	BufSize() const
		{ return size; }

	// leading-string compression

	void	Compress( StrPtr *s );
	void	UnCompress( StrPtr *s );

	// trailing-string compression
	int	EncodeTail( StrPtr &s, const char *replaceBytes );
	int	DecodeTail( StrPtr &s, const char *replaceBytes );

	// string << -- append string/number

	StrBuf& operator <<( const char *s )
		{ Append( s ); return *this; }

	StrBuf& operator <<( const StrPtr *s )
		{ Append( s ); return *this; }

	StrBuf& operator <<( const StrPtr &s )
		{ Append( &s ); return *this; }

	StrBuf& operator <<( const StrNum &s )
		{ UAppend( (const StrPtr *)&s ); return *this; }

	StrBuf& operator <<( int v );

    private:
	p4size_t	size;

	void	Grow( p4size_t len );

	// reserve a large block of memory (>= 128 KB); don't over-allocate
	void	Reserve( p4size_t oldlen );

	// Some DbCompare funcs memcpy from this, so it has be be big
	// enough that we aren't reaching past valid memory.  The
	// largest one seems to be DbInt64 (8 bytes.)
	static char nullStrBuf[ 8 ];
} ;

class StrFixed : public StrPtr {

    public:

		StrFixed( p4size_t l )
		{ this->length = l; this->buffer = new char[ l ]; }

		~StrFixed()
		{ delete []buffer; }

	void	SetBufferSize( p4size_t l );
} ;


class StrNum : public StrPtr {

    public:
		StrNum() {} 

		StrNum( int v ) 
		{ Set( v ); }

		StrNum( int ok, int v )
		{ if( ok ) Set( v ); else buffer = buf, length = 0; }

	void	Set( int v )
		{
		    buffer = Itoa( v, buf + sizeof( buf ) );
		    length = (p4size_t)(buf + sizeof( buf ) - buffer - 1);
		}

	void	SetHex( int v )
		{
		    buffer = Itox( v, buf + sizeof( buf ) );
		    length = (p4size_t)(buf + sizeof( buf ) - buffer - 1);
		}

	void	Set( int v, int digits )
		{
		    Set( v );

		    while( (int)length < digits )
			*--buffer = '0', ++length;
		}

# ifdef HAVE_INT64

		StrNum( long v ) { Set( (P4INT64)v ); }

		StrNum( P4INT64 v )
		{ Set( v ); }

	void	Set( P4INT64 v )
		{
		    buffer = Itoa64( v, buf + sizeof( buf ) );
		    length = (p4size_t)(buf + sizeof( buf ) - buffer - 1);
		}

# endif

    private:
		char buf[24];
} ;

class StrHuman : public StrPtr
{
	public:
	        StrHuman() {}
 
	        StrHuman( long v, int f = 1024 )
	        { Convert( (P4INT64)v, f ); }
 
	        StrHuman( P4INT64 v, int f = 1024 )
	        { Convert( v, f ); }
 
	        static char *Itoa64( P4INT64 v, char *endbuf, int f );
 
	private:
	        void	Convert( P4INT64 v, int f )
	        {
	            buffer = Itoa64( v, buf + sizeof( buf ), f );
	            length = (p4size_t)(buf + sizeof( buf ) - buffer - 1);
	        }

	    char buf[24];
} ;

inline StrBuf &
StrBuf::operator <<( int v )
{
	return operator <<( StrNum( v ) );
}
