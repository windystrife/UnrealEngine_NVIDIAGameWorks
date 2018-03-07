/*
 * mangle.h -- Mangle (encrypt/decrypt string with private key) interface
 */

class Mangle {

	public:
			Mangle();

	void		In( const StrPtr &data, const StrPtr &key, 
			    StrBuf &result, Error *e ) ;

	void		InMD5( const StrPtr &data, const StrPtr &key, 
			    StrBuf &result, Error *e )
			{ 
			    DoIt( data, key, result, 0, 1, e ); 
			};

	void		Out( const StrPtr &data, const StrPtr  &key, 
			    StrBuf &result, Error *e ) ;

	void		OutMD5( const StrPtr &data, const StrPtr &key, 
			    StrBuf &result, Error *e )
			{ 
			    DoIt( data, key, result, 1, 1, e ); 
			};

	void		XOR( StrBuf &data, const StrPtr &key, Error *e );

	void		Mix( const char *buf )
			{
			    s2[0] = (int) *buf++; s2[1] = (int) *buf++;
			    s2[2] = (int) *buf++; s2[3] = (int) *buf++;
			    s2[4] = (int) *buf++; s2[5] = (int) *buf++;
			    s2[6] = (int) *buf++; s2[7] = (int) *buf++;
			};

	private:

	void 		DoIt( const StrPtr &data, const StrPtr &key, 
			    StrBuf &result, int direction, int digest, 
			    Error *e );

	void 		Getdval( int direction, int m[], int k[] );


	int		o[8];
	int		pr[8];
	int		s0[16];
	int		s1[16];
	int		s2[8];
};
