//
// Copyright 2008 Perforce Software.  All rights reserved.
//
// This file is part of Perforce - the FAST SCM System.
//
// StrXml:
//   XML output methods class

#ifndef StrXml_H
#define StrXml_H

class StrBuf;
class StrBufDict;

class StrXml : public StrBuf {

public:
	StrXml() {};
	virtual ~StrXml() {};

	void XMLHeader( const StrPtr *cmd, const StrPtr *args, const StrPtr *port, 
	                const StrPtr *user, const StrPtr *client, int bUnicode=0 );
	void XMLOutputStat( StrDict * varList );
	void XMLOutputError( char *data );
	void XMLOutputText( char *data );
	void XMLOutputInfo( char *data, char level );
	void XMLEnd();

private:

	int XMLlist( StrDict * varList, int i, char * remove=NULL, char *nextup=NULL );
	StrBuf&   EscapeHTML( const StrPtr &s, int isUnicode=0 );

	int       fUnicode;
	StrBuf    fP4Cmd;
	StrBuf    fExtraTag;
	StrBuf    fEscapeBuf;
};

#endif // StrXml_H
