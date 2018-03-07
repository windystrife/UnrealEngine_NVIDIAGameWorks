/*
 * Copyright 2001 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * macfile.h - Abstract file layer to handle the many differences on Mac OS X and
 *             Classic Mac OS
 *
 * NOTE: -----------------------------------------------------------------
 *
 * Some of the following documentation is obsolete but is left for
 * historical purposes. The need for Mac OS 9 compatibility has gone
 * as has the need for an abstraction that works both with FSRef and FSSpec.
 * 
 * MacFile is now a single class with no subclasses and deals solely with
 * FSRefs. Supporting 'apple' files is more of a legacy compatibility
 * feature of Perforce so these classes have been simplified.
 *
 * TextEncodingConverter is no longer needed and the path delimiter is
 * always '/'
 *
 * In the platform table below, the only platform left is MOSX.
 *
 * Okay, on with the docs...
 *
 * -----------------------------------------------------------------------
 *
 * On the Macintosh, there are many different kinds of system apis available that
 * depend on what version of the OS is running, whether or not it is the Classic
 * Mac OS or Mac OS X, and finally, if you are using Carbon, certain APIs expect
 * different input based on if the Carbon app is running on Classic Mac OS or
 * Mac OS X.
 *
 * Some concepts and terminology:
 * ------------------------------
 * FSSpec API - the old file API from Apple that was limited to < 32 char, 
 *              system codepage filenames.
 * 
 * FSRef API - the new file API from Apple that allow
 *              for < 256 char, Unicode filenames.
 *
 * CFURL API - the API from Apple that allows us to interchange different
 *             path styles (ie: colons on Classic Mac and '/' on MOSX).
 *             Requires the use of CFStrings.
 *
 * CFString API - the API from Apple that allows easy manipulation of
 *                strings between different codepages and Unicode.
 *                Also allows modification of a string as an object.
 *
 * TextEncodingConverter - the API from Apple that allows conversion of
 *                         different runs of characters from one
 *                         codepage to another. The big difference between
 *                         it and CFString is it only works on raw byte arrays.
 *                         There is no concept of a string object. It is super
 *                         lo level.
 * Here is a table:
 * ----------------
 * 3 Binaries of P4 builds: Classic, Carbonized, Mac OS X
 * 
 * 'MOS8.6'      = Standard Mac 8.6 install running Classic P4
 * 'Carbon MOS'  = MacOS 8.6/9 with Carbon running Carbon P4
 * 'MOS9'        = Standard Mac OS 9 install running Classic P4
 * 'Carbon MOSX' = Mac OS X running Carbon P4
 * 'MOSX'        = Mac OS X running Mac OS X P4 (partial Std-C/Carbon calls (for speed))
 *
 *                         | MOS8.6 | Carbon MOS | MOS9  | Carbon MOSX | MOSX
 *----------------------------------------------------------------------------
 * File API                | FSSpec |   FSRef    | FSRef |   FSRef     | FSRef
 * Path delimiter          |  ':'   |    ':'     |  ':'  |    '/'      |  '/'
 * CFURL?                  |        |     X      |       |     X       |   X
 * CFString?               |        |     X      |       |     X       |   X
 * TextEncodingConverter?  |   X    |     X      |   X   |     X       |   X
 * Perforce Path Delimiter |  ':'   |    ':'     |  ':'  |    ':'      |  '/'
 *
 * There are many different compile options (static) and many different
 * cases to handle as the code runs (dynamic).
 * 
 * Static
 * ------
 * - Should use on CFString, CFURL? (yes, with Carbon/MOSX )
 * - What is the path delimiter from Perforce?
 * - Should only use FSRef (yes, with Carbon/MOSX)
 * 
 * Dynamic
 * -------
 * - Path delimiter of System (For Carbon)
 * - Are FSRefs available? Fallback to FSSpec (For Classic)
 * 
 *
 * Public methods:
 *
 *      MACFILE
 *      -------------
 *      MacFile is an object that represents a file on the disk. You can use this
 *      to read and write resource and data forks, set and get comment info, get and
 *      set additional Finder Info, and set and get type and creator info (although
 *      you can do that with the Finder Info as well).
 *      
 *      Creation
 *      --------
 *	GetFromPath() - takes a path, a style, and an encoding and returns an object to
 *                      represent it. Returns NULL if no object could be made from the path.
 *	GetFromFSRef() - If you alread have an FSRef, you can make a MacFile from it.
 *	CreateFromPath() - takes a path, style, and encoding and creates a file (or
 *                         directory) on the disk where the path points. It will not
 *                         create subdirectories automatically. Returns NULL if no object
 *                         could be made from the path. 
 *
 *      GetFileSystem
 *      -------------
 *	GetFileSystem() - Get the kind of file system the file is on. This only makes
 *                        sense on Mac OS X.
 *      IsDir
 *      --------
 *	IsDir() - Let's you know if the file is a firectory or not. Some methods should
 *                not be called if the file is a directory.
 *
 *      Metadata
 *      --------
 *	GetFInfo() - returns the finder information for this file
 *	SetFInfo() - sets the finder information for this file
 *	GetFXInfo() - gets the extended finder information for this file
 *	SetFXInfo() - sets the extended finder information for this file
 *
 *	IsHidden() - determines if the file has its hidden attribute set
 *      CreateBundle() - Creates a Core Foundation Bundle from this file. Returns
 *			 NULL if there is no bundle for this file.
 *      IsUnbundledApp() - determines if the file is an application in a single file
 *                         (the traditional Mac way of making an app)
 *      IsBundledApp() - determines if the file is an application directory. (the new
 *                       way of packaging an application. Like the ".app" programs
 *                       you get on Mac OS X)
 *
 *	IsLocked() - determines if the file has its immutable bit set
 *	SetLocked() - sets the file's locked attribute
 *
 *	GetTypeAndCreator() - retrieves the type and creator. You can pass NULL if you are
 *                            not interested in one of them.
 *	SetTypeAndCreator() - sets the type and creator.
 *
 *	GetComment() - returns the comment and the length.
 *	SetComment() - fills in the file comment with the bytes. You pass in the buffer
 *                     and the size of it
 *
 *      Reading and Writing
 *      -------------------
 *	HasDataFork() - returns if the file has a data fork
 *	GetDataForkSize() - returns the size in bytes of the data fork
 *	OpenDataFork() - opens the data fork for writing
 *	ReadDataFork() - reads data out of the fork. Tries to read requestCount bytes
 *                       into buffer and returns actualCount bytes as the number acutally
 *                       read.
 *	WriteDataFork() - reads data out of the fork. Tries to write requestCount bytes
 *                        from buffer and returns actualCount bytes as the number acutally
 *                        written.
 *	SetDataForkPosition() - sets the position relative to the beginning of the file.
 *	CloseDataFork() - closes the data for for writing.
 *
 *	HasResourceFork() - returns if the file has a rsrc fork
 *	GetResourceForkSize() - returns the size in bytes of the rsrc fork
 *	OpenResourceFork() - opens the rsrc fork for writing
 *	ReadResourceFork() - reads data out of the fork. Tries to read requestCount bytes
 *                           into buffer and returns actualCount bytes as the number
 *                           acutally read.
 *	WriteResourceFork() - reads data out of the fork. Tries to write requestCount
 *                            bytes from buffer and returns actualCount bytes as the
 *                            number acutally written.
 *	CloseResourceFork() - closes the fork for for writing.
 *
 *      Misc
 *      ----
 *	GetPrintableFullPath() - returns a C String that you can use for printing which is
 *                             in the native path format and encoding for the filesystem.
 *	GetFSSpec() - returns an FSSpec for the file. If you want a const one, you can
 *                    capture the return value. If you want a changeable one, pass it in
 *                    to the method. You can pass NULL (0) in if you like.
 *	GetFSRef() - returns an GetFSRef for the file. Can return NULL if one is not
 *                   available. If you want a const one, you can capture the return value.
 *                   If you want a changeable one, pass it in to the method. You can
 *                   pass NULL (0) in if you like.
 *
 *
 */

# if defined (OS_MACOSX)

# ifndef __MACFILE_H__
# define __MACFILE_H__

# include <CoreServices/CoreServices.h>

# define kMacPathStylePerforce kCFURLPOSIXPathStyle

// This switch prefers the 64-bit compatible FSIORefNum type (OS 10.5 and
// later) to the standard SInt16 type (OS 10.4 and earlier)

#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
  #if __MAC_OS_X_VERSION_MIN_ALLOWED <= 1050
    # define P4IORefNum FSIORefNum
  # else
    # define P4IORefNum SInt16
  # endif
# else
  # define P4IORefNum SInt16
#endif
/* ============================= *\
 *
 *    ABSTRACT BASE CLASSES
 *
\* ============================= */

class MacFile
{

public:

    enum {
	FS_HFS = 0,
	FS_UFS,
	FS_NFS
    };

    //
    // Creation
    //
    static MacFile *
    GetFromPath(
    const char * path,
	OSErr * error );

    static MacFile *
    GetFromFSRef(
	const FSRef * ref,
	OSErr * error );

    static MacFile *
    CreateFromPath(
	const char * path,
	Boolean isDirectory,
	OSErr * error );

    static MacFile *
    CreateFromDirAndName(
    const FSRef &       dir,
    CFStringRef         name,
    Boolean             isDirectory,
    OSErr *             outErr );

    virtual ~MacFile();
    
    //
    // File Deletion
    //
    
    OSErr Delete();
    
    //
    // Determing the file system.
    //
    int GetFileSystemType() const;

    //
    // Is Directory
    //
    Boolean IsDir() const;

    //
    // Metadata
    //
    const FInfo *  GetFInfo() const;
    OSErr          SetFInfo( const FInfo * );
    static  void	   SwapFInfo( FInfo * );
    const FXInfo * GetFXInfo() const;
    OSErr          SetFXInfo( const FXInfo * );
    static  void	   SwapFXInfo( FXInfo * );
    
    Boolean     IsHidden() const;
    CFBundleRef CreateBundle() const;
    Boolean     IsUnbundledApp() const;
    Boolean     IsBundledApp() const;
    
    Boolean IsLocked() const;
    OSErr   SetLocked( Boolean lock );
    
    OSErr GetTypeAndCreator( UInt32 * type, UInt32 * creator ) const;
    OSErr SetTypeAndCreator( UInt32 type, UInt32 creator );

    const char * GetComment( int *bufferLength ) const;
    OSErr        SetComment( char * buffer, int bufferLength );
    
    //
    // Reading and Writing
    //
    Boolean   HasDataFork() const;
    ByteCount GetDataForkSize() const;
    OSErr     OpenDataFork( SInt8 permissions );

    OSErr     ReadDataFork( ByteCount requestCount, void * buffer,
    				ByteCount * actualCount );

    OSErr     WriteDataFork( ByteCount requestCount, const void * buffer,
    				 ByteCount * actualCount );

    OSErr     SetDataForkPosition( ByteCount offset );
    OSErr     CloseDataFork();
    

    Boolean   HasResourceFork() const;
    ByteCount GetResourceForkSize() const;
    OSErr     OpenResourceFork( SInt8 permissions );

    OSErr     ReadResourceFork( ByteCount requestCount, void * buffer,
    				    ByteCount * actualCount );

    OSErr     WriteResourceFork( ByteCount requestCount, const void * buffer,
    				     ByteCount * actualCount );

    OSErr     CloseResourceFork();

    //
    // Misc
    //
    const char *   GetPrintableFullPath() const;
    const FSRef *  GetFSRef( FSRef * spec ) const;
    
    
private:

    MacFile( FSRef * file );
    
    OSErr LoadCatalogInfo() const;
    OSErr SaveCatalogInfo();
    OSErr PreloadComment() const;

    
    char *   fullPath;
    FSRef *  file;
    
    mutable FSCatalogInfo fileInfo;
    FSCatalogInfoBitmap changedInfoBitmap;
    
    P4IORefNum dataForkRef;
    P4IORefNum rsrcForkRef;
   
    mutable char * comment;
    mutable short  commentLength;
};




/* ============================= *\
 *
 *       UTILIY METHODS
 *
\* ============================= */

UniChar GetSystemFileSeparator();

CFURLPathStyle GetSystemPathStyle();

char * FSRefToPath( const FSRef * ref );



# endif // __MACFILE_H__


# endif // defined (OS_MACOSX)
