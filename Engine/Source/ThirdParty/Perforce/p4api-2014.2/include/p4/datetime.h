/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * DateTime - get and set the date as a string
 */

// size for Fmt, FmtDay

# define DateTimeBufSize 20 

// size for FmtTz, which can say things like '0700 - Pacific Standard Time'

# define DateTimeZoneBufSize 80 

class DateTime {

    public:
		DateTime() {}
		DateTime( const int date ) { Set( date ); }
		DateTime( const char *date, Error *e ) { Set( date, e ); }

 	void	Set( const char *date, Error *e );
 	void	Set( const int date ) { wholeDay = 0; tval = (time_t)date; }
	void	SetNow() { Set( (int)Now() ); }

	int 	Compare( const DateTime &t2 ) const { 
                return (int)(tval - t2.tval); };

	void	Fmt( char *buf ) const;
	void	FmtDay( char *buf ) const;
	void	FmtDayUTC( char *buf ) const;
	void	FmtTz( char *buf ) const;
	void	FmtUTC( char *buf ) const;
	void 	FmtElapsed( char *buf, const DateTime &t2 );
	void	FmtUnifiedDiff( char *buf ) const;

	int	Value() const { return (int)tval; }
	int	Tomorrow() const { return (int)tval + 24*60*60; }
	int	IsWholeDay() const { return wholeDay; }

	static int Never() { return 0; }
	static int Forever() { return 2147483647; }

	// for stat() and utime() conversion

	static time_t Localize( time_t centralTime );
	static time_t Centralize( time_t localTime );	
	int    TzOffset( int *isdst = 0 ) const;

    protected:
	time_t	Now();

    private:
	time_t	tval;
	int	wholeDay;

	int	ParseOffset( const char *s, const char *odate, Error *e );
};

class DateTimeNow : public DateTime {

    public:
		DateTimeNow() : DateTime( (int)Now() ) {}

} ;

// Pass a buffer of at least this size to DateTimeHighPrecision::Fmt():

# define DTHighPrecisionBufSize 40 

/*
 * Uses gettimeofday/clock_gettime/etc. to find more precise system time
 */
class DateTimeHighPrecision
{
    public:

	void	Now();
	void	Fmt( char *buf ) const;

	time_t	Seconds() const;
	int	Nanos() const;

    private:

	time_t	seconds; // Since 1/1/1970, natch
	int	nanos;
} ;

