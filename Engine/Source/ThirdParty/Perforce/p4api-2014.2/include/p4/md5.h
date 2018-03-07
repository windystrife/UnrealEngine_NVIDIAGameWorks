/*
 * md5.h -- MD5 (message digest algorithm) interface
 */

#if defined(__GNUC__)
#	if defined(__x86_64__) || defined(__i686__) || defined(__sparc__) || \
        defined(__ia64__) || defined(__powerpc__) || defined(__i386__) || \
	defined(_ARCH_PPC)
#		define ALIGN(x) __attribute__ ((aligned (x)))
#	else
#		define ALIGN(x)
#	endif
#elif defined(_MSC_VER)
#	if defined(_M_X64) || defined(_M_IA64) || defined(_M_IX86) || defined(_M_ARM)
#		define ALIGN(x) __declspec(align(x))
#	endif
#else
#	define ALIGN(x)
#endif

# ifdef OS_UNICOS
# define INT_TOO_BIG
# endif

# ifndef INT_TOO_BIG
typedef unsigned int uint32;
# endif

# ifdef INT_TOO_BIG

/*
 * For machines without 32 bit ints (64, not 16!) this definition
 * of uint32 suffices for the md5 code.  It masks itself down to
 * 32 bits when used as a value.
 *
 * NB: because += and + return values not masked, this code is not
 * usable for the count64 implementation below.  INT_TOO_BIG should
 * always be used with a real P4INT64!
 */

typedef unsigned int xint;

class uint32 {

    public:
		uint32() {}
		operator xint() const { return value & 0xffffffff; }
		uint32( const P4INT64 x ) { value = x; }
	void	operator =( const xint x ) { value = x; }
	void	operator =( const uint32 &x ) { value = x.value; }
	xint	operator +=( const xint x ) { return value += x; }
	xint	operator +( const xint x ) const { return value + x; }

    private:
	xint	value;

} ;

# endif

# ifdef P4INT64

typedef P4INT64 count64;

# else

/*
 * For machines without a 64 bit long long, this definition of count64
 * suffices for the md5 code.  >> only works for 0 and 32!
 */

class count64 {

    public:
	uint32	operator >>( int s ) { return s ? hi32 : low32; }

	void	operator =( unsigned int x )
		{ low32 = x; hi32 = 0; }

	void	operator +=( unsigned int x )
		{ uint32 t = low32; if( ( low32 += x ) < t ) hi32 += 1; }

    private:
	uint32	low32;
	uint32	hi32;

} ;

# endif

enum BufSelector {
	USE_INBUF,
	USE_ODDBUF,
	USE_WORKBUF
};

typedef union {
    unsigned char *c;
    uint32 *i;
} MD5BufUnion;

/*
 * MD5 - compute md5 checksum given a bunch of blocks of data
 */

class MD5 {

    public:
			MD5();

	void		Update( const StrPtr &buf );
	void		Final( StrBuf &output );
	void 		Final( unsigned char digest[16] );

    private:

	void 		Transform();

	count64		bits;		// total

	uint32 		ALIGN(8)	md5[4];	    // result of Transform()
	uint32		ALIGN(8)	work[16];   // work for Transform()
	unsigned char	ALIGN(8)	oddbuf[64]; // to span Update() calls
	unsigned char   *inbuf;		// pointer to incoming data

	int		bytes;		// mod 64

	BufSelector	bufSelector;	// where to point inbuf at

};

# undef	ALIGN
