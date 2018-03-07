/* longline.c:  source of long logical line.    */

typedef  unsigned int  size_t; \
typedef struct __sfpos { \
char _pos[8]; \
} fpos_t; \
struct __sbuf { \
unsigned char *_base; \
int _size; \
}; \
typedef struct __sFILE { \
unsigned char *_p; \
int _r; \
int _w; \
short _flags; \
short _file; \
struct __sbuf _bf; \
int _lbfsize; \
 \
 \
void *_cookie; \
int (*_close)  (void *) ; \
int (*_read)  (void *, char *, int) ; \
fpos_t (*_seek)  (void *, fpos_t, int) ; \
int (*_write)  (void *, const char *, int) ; \
 \
 \
struct __sbuf _ub; \
unsigned char *_up; \
int _ur; \
 \
 \
unsigned char _ubuf[3]; \
unsigned char _nbuf[1]; \
 \
 \
struct __sbuf _lb; \
 \
 \
int _blksize; \
fpos_t _offset; \
} FILE; \
 \
   \
extern FILE __sF[]; \
   \
   \
void clearerr  (FILE *) ; \
int fclose  (FILE *) ; \
int feof  (FILE *) ; \
int ferror  (FILE *) ; \
int fflush  (FILE *) ; \
int fgetc  (FILE *) ; \
int fgetpos  (FILE *, fpos_t *) ; \
char *fgets  (char *, size_t, FILE *) ; \
FILE *fopen  (const char *, const char *) ; \
int fprintf  (FILE *, const char *, ...) ; \
int fputc  (int, FILE *) ; \
int fputs  (const char *, FILE *) ; \
                                             \

#ifndef X03FF

size_t fread  (void *, size_t, size_t, FILE *) ; \
FILE *freopen  (const char *, const char *, FILE *) ; \
int fscanf  (FILE *, const char *, ...) ; \
int fseek  (FILE *, long, int) ; \
int fsetpos  (FILE *, const fpos_t *) ; \
long ftell  (const FILE *) ; \
size_t fwrite  (const void *, size_t, size_t, FILE *) ; \
int getc  (FILE *) ; \
int getchar  (void) ; \
char *gets  (char *) ; \
 \
extern int sys_nerr; \
extern  const  char * const  sys_errlist[]; \
 \
void perror  (const char *) ; \
int printf  (const char *, ...) ; \
int putc  (int, FILE *) ; \
int putchar  (int) ; \
int puts  (const char *) ; \
int remove  (const char *) ; \
int rename  (const char *, const char *) ; \
void rewind  (FILE *) ; \
int scanf  (const char *, ...) ; \
void setbuf  (FILE *, char *) ; \
int setvbuf  (FILE *, char *, int, size_t) ; \
int sprintf  (char *, const char *, ...) ; \
int sscanf  (const char *, const char *, ...) ; \
FILE *tmpfile  (void) ; \
char *tmpnam  (char *) ; \
int ungetc  (int, FILE *) ; \
int vfprintf  (FILE *, const char *,  char * ) ; \
int vprintf  (const char *,  char * ) ; \
int vsprintf  (char *, const char *,  char * ) ; \
   \
   \
char *ctermid  (char *) ; \
FILE *fdopen  (int, const char *) ; \
int fileno  (FILE *) ; \
   \
   \
char *fgetln  (FILE *, size_t *) ; \
int fpurge  (FILE *) ; \
int getw  (FILE *) ; \
int pclose  (FILE *) ; \
FILE *popen  (const char *, const char *) ; \
int putw  (int, FILE *) ; \
void setbuffer  (FILE *, char *, int) ; \
int setlinebuf  (FILE *) ; \
char *tempnam  (const char *, const char *) ; \
int snprintf  (char *, size_t, const char *, ...) ; \
int vsnprintf  (char *, size_t, const char *,  char * ) ; \
int vscanf  (const char *,  char * ) ; \
int vsscanf  (const char *, const char *,  char * ) ; \
FILE *zopen  (const char *, const char *, int) ; \
   \
   \
FILE *funopen  (const void *, int (*)(void *, char *, int), int (*)(void *, const char *, int), fpos_t (*)(void *, fpos_t, int), int (*)(void *)) ; \
   \
   \
int __srget  (FILE *) ; \
int __svfscanf  (FILE *, const char *,  char * ) ; \
int __swbuf  (int, FILE *) ; \
   \
typedef  int  rune_t; \
                               \
 
#ifndef X07FF

typedef  int  wchar_t; \
typedef struct { \
rune_t min; \
rune_t max; \
rune_t map; \
unsigned long *types; \
} _RuneEntry; \
 \
typedef struct { \
int nranges; \
_RuneEntry *ranges; \
} _RuneRange; \
 \
typedef struct { \
char magic[8]; \
char encoding[32]; \
 \
rune_t (*sgetrune) \
 (const char *, unsigned int, char const **) ; \
int (*sputrune) \
 (rune_t, char *, unsigned int, char **) ; \
rune_t invalid_rune; \
 \
unsigned long runetype[ (1 <<8 ) ]; \
rune_t maplower[ (1 <<8 ) ]; \
rune_t mapupper[ (1 <<8 ) ]; \
_RuneRange runetype_ext; \
_RuneRange maplower_ext; \
_RuneRange mapupper_ext; \
 \
void *variable; \
int variable_len; \
} _RuneLocale; \
 \
 \
 \
extern _RuneLocale _DefaultRuneLocale; \
extern _RuneLocale *_CurrentRuneLocale; \
   \
unsigned long ___runetype  ( int ) ; \
 int  ___tolower  ( int ) ; \
 int  ___toupper  ( int ) ; \
   \
   \
int __istype  ( int , unsigned long) ; \
int __isctype  ( int , unsigned long) ; \
 int  toupper  ( int ) ; \
 int  tolower  ( int ) ; \
   \
extern int errno; \
   \
void *memchr  (const void *, int, size_t) ; \
int memcmp  (const void *, const void *, size_t) ; \
void *memcpy  (void *, const void *, size_t) ; \
void *memmove  (void *, const void *, size_t) ; \
void *memset  (void *, int, size_t) ; \
char *strcat  (char *, const char *) ; \
char *strchr  (const char *, int) ; \
int strcmp  (const char *, const char *) ; \
int strcoll  (const char *, const char *) ; \
char *strcpy  (char *, const char *) ; \
size_t strcspn  (const char *, const char *) ; \
char *strerror  (int) ; \
size_t strlen  (const char *) ; \
char *strncat  (char *, const char *, size_t) ; \
int strncmp  (const char *, const char *, size_t) ; \
char *strncpy  (char *, const char *, size_t) ; \
char *strpbrk  (const char *, const char *) ; \
char *strrchr  (const char *, int) ; \
size_t strspn  (const char *, const char *) ; \
char *strstr  (const char *, const char *) ; \
char *strtok  (char *, const char *) ; \
size_t strxfrm  (char *, const char *, size_t) ; \
 \
 \
 \
int bcmp  (const void *, const void *, size_t) ; \
void bcopy  (const void *, void *, size_t) ; \
void bzero  (void *, size_t) ; \
int ffs  (int) ; \
char *index  (const char *, int) ; \
void *memccpy  (void *, const void *, int, size_t) ; \
char *rindex  (const char *, int) ; \
int strcasecmp  (const char *, const char *) ; \
char *strdup  (const char *) ; \
void strmode  (int, char *) ; \
int strncasecmp  (const char *, const char *, size_t) ; \
char *strsep  (char **, const char *) ; \
void swab  (const void *, void *, size_t) ; \
 \
   \
typedef struct { \
int quot; \
int rem; \
} div_t; \
 \
typedef struct { \
long quot; \
long rem; \
} ldiv_t; \
extern int __mb_cur_max; \
 \
   \
   void \
abort  (void)    ; \
   int \
abs  (int) ; \
int atexit  (void (*)(void)) ; \
double atof  (const char *) ; \
int atoi  (const char *) ; \
long atol  (const char *) ; \
void *bsearch  (const void *, const void *, size_t, size_t, int (*)(const void *, const void *)) ; \
void *calloc  (size_t, size_t) ; \
   div_t \
div  (int, int) ; \
   void \
exit  (int)    ; \
void free  (void *) ; \
char *getenv  (const char *) ; \
   long \
labs  (long) ; \
   ldiv_t \
ldiv  (long, long) ; \
void *malloc  (size_t) ; \
void qsort  (void *, size_t, size_t, int (*)(const void *, const void *)) ; \
int rand  (void) ; \
void *realloc  (void *, size_t) ; \
void srand  (unsigned) ; \
double strtod  (const char *, char **) ; \
long strtol  (const char *, char **, int) ; \
unsigned long \
strtoul  (const char *, char **, int) ; \
int system  (const char *) ; \
 \
 \
int mblen  (const char *, size_t) ; \
size_t mbstowcs  (wchar_t *, const char *, size_t) ; \
int wctomb  (char *, wchar_t) ; \
int mbtowc  (wchar_t *, const char *, size_t) ; \
size_t wcstombs  (char *, const wchar_t *, size_t) ; \
 \
 \
int putenv  (const char *) ; \
int setenv  (const char *, const char *, int) ; \
 \
 \
 \
double drand48  (void) ; \
double erand48  (unsigned short[3]) ; \
long lrand48  (void) ; \
long nrand48  (unsigned short[3]) ; \
long mrand48  (void) ; \
long jrand48  (unsigned short[3]) ; \
void srand48  (long) ; \
unsigned short *seed48  (unsigned short[3]) ; \
void lcong48  (unsigned short[7]) ; \
 \
void *alloca  (size_t) ; \
 \
char *getbsize  (int *, long *) ; \
char *cgetcap  (char *, char *, int) ; \
int cgetclose  (void) ; \
                                                     \
 
#ifndef X0FFF

int cgetent  (char **, char **, char *) ; \
int cgetfirst  (char **, char **) ; \
int cgetmatch  (char *, char *) ; \
int cgetnext  (char **, char **) ; \
int cgetnum  (char *, char *, long *) ; \
int cgetset  (char *) ; \
int cgetstr  (char *, char *, char **) ; \
int cgetustr  (char *, char *, char **) ; \
 \
int daemon  (int, int) ; \
char *devname  (int, int) ; \
int getloadavg  (double [], int) ; \
 \
extern char *optarg; \
extern int opterr, optind, optopt; \
int getopt  (int, char * const *, const char *) ; \
 \
extern char *suboptarg; \
int getsubopt  (char **, char * const *, char **) ; \
 \
char *group_from_gid  (unsigned long, int) ; \
int heapsort  (void *, size_t, size_t, int (*)(const void *, const void *)) ; \
char *initstate  (unsigned, char *, int) ; \
int mergesort  (void *, size_t, size_t, int (*)(const void *, const void *)) ; \
int radixsort  (const unsigned char **, int, const unsigned char *, unsigned) ; \
int sradixsort  (const unsigned char **, int, const unsigned char *, unsigned) ; \
long random  (void) ; \
char *realpath  (const char *, char resolved_path[]) ; \
char *setstate  (char *) ; \
void srandom  (unsigned) ; \
char *user_from_uid  (unsigned long, int) ; \
void unsetenv  (const char *) ; \
 \
   \
typedef  int  ptrdiff_t; \
typedef  unsigned long  clock_t; \
 \
 \
 \
 \
typedef  long  time_t; \
struct tm { \
int tm_sec; \
int tm_min; \
int tm_hour; \
int tm_mday; \
int tm_mon; \
int tm_year; \
int tm_wday; \
int tm_yday; \
int tm_isdst; \
long tm_gmtoff; \
char *tm_zone; \
}; \
   \
char *asctime  (const struct tm *) ; \
clock_t clock  (void) ; \
char *ctime  (const time_t *) ; \
double difftime  (time_t, time_t) ; \
struct tm *gmtime  (const time_t *) ; \
struct tm *localtime  (const time_t *) ; \
time_t mktime  (struct tm *) ; \
size_t strftime  (char *, size_t, const char *, const struct tm *) ; \
time_t time  (time_t *) ; \
 \
 \
void tzset  (void) ; \
 \
 \
 \
char *timezone  (int, int) ; \
void tzsetwall  (void) ; \
 \
   \
extern int getopt( int argc, char * const * argv, const char * opts); \
extern char * stpcpy( char * dest, const char * src); \
typedef struct defbuf { \
struct defbuf * link; \
short nargs; \
 \
char * parmnames; \
 \
char * repl; \
char name[1]; \
} DEFBUF; \
typedef struct fileinfo { \
char * bptr; \
unsigned line; \
FILE * fp; \
long pos; \
struct fileinfo * parent; \
struct ifinfo * initif; \
char * filename; \
char * buffer; \
} FILEINFO; \
typedef struct ifinfo { \
int stat; \
unsigned ifline; \
unsigned elseline; \
} IFINFO; \
typedef struct val_sign { \
long val; \
int sign; \
} VAL_SIGN; \
extern    int cflag; \
extern int eflag; \
extern    int iflag; \
extern    int lflag; \
extern    int pflag; \
extern int qflag; \
 \
extern    int tflag; \
 \
 \
 \
extern    int digraphs; \
 \
extern    int stdc_val; \
extern long cplus; \
extern int stdc2; \
extern    int has_pragma; \
 \
extern    int std_line_prefix; \
extern    int warn_level; \
extern int errors; \
extern    unsigned line; \
extern    int wrongline; \
extern    int keepcomments; \
extern    int no_output; \
extern    int in_directive; \
extern    int in_define; \
extern    unsigned macro_line; \
extern    int openum; \
extern    IFINFO * ifptr; \
extern    FILEINFO * infile; \
extern int mkdep; \
 \
extern    char * workp; \
extern    char * const work_end; \
extern    char identifier[  0xFF  + 1]; \
extern const char type[]; \
extern IFINFO ifstack[]; \
extern char work[]; \
extern    int debug; \
extern void curfile( void); \
extern int openfile( const char * filename, int local); \
extern void unpredefine( int clearall); \
 \
extern int control( int newlines); \
extern DEFBUF * lookid( const char * name); \
extern DEFBUF * install( const char * name, int nargs, const char * parmnames \
, const char * repl); \
extern int undefine( const char * name); \
extern void dumpadef( const char * why, const DEFBUF * dp, int newdef); \
extern void dumpdef( void); \
 \
extern long eval( void); \
extern VAL_SIGN * evalnum( const char * nump); \
 \
extern DEFBUF * is_macro( char ** cp); \
extern char * expand( DEFBUF * defp, char * out, char * out_end); \
 \
extern int get_unexpandable( int c, int diag); \
extern void skipnl( void); \
extern int skipws( void); \
extern int scantoken( int c, char ** out_pp, char * out_end); \
extern char * scanquote( int delim, char * out, char * out_end, int diag); \
extern int get( void); \
 \
extern int trigraph( char * in); \
extern void unget( void); \
extern FILEINFO * ungetstring( const char * text, const char * name); \
extern char * savestring( const char * text); \
extern FILEINFO * getfile( const char * name, size_t bufsize); \
extern char * (xmalloc)( size_t size); \
extern char * (xrealloc)( char * ptr, size_t size); \
extern void cfatal( const char * format, const char * arg1, int arg2 \
, const char * arg3); \
extern void cerror( const char * format, const char * arg1, int arg2 \
, const char * arg3); \
extern void cwarn( const char * format, const char * arg1, int arg2 \
, const char * arg3); \
 \
extern void dooptions( int argc, char ** argv, char ** in_pp \
, char ** out_pp); \
extern int reopen_stdout( const char * filename); \
extern void setstdin( char * cp); \
extern void put_start_file( char * filename); \
extern void sharp( void); \
extern void putfname( char * filename); \
extern int getredirection( int argc, char ** argv); \
 \
extern void put_depend( const char * filename); \
 \
extern int doinclude( void); \
 \
extern void dopragma( void); \
extern void put_source( const char * src); \
extern void alloc_mem( void); \
 \
 \
 \
extern void dumpstring( const char * why, const char * text); \
extern void dumpunget( const char * why); \
 \
extern void print_heap( void); \
static void scanid( int c); \
static char * scannumber( int c, char * out, char * out_end); \
static char * scanop( int c, char * out); \
static char * parse_line( void); \
static char * read_a_comment( char * sp); \
static char * getline( int in_comment); \
static void at_eof( int in_comment); \
void dumptoken( int token_type, const char * cp); \
int \
 \
get_unexpandable( int c, int diag) \
{ \
DEFBUF * defp; \
FILEINFO * file; \
FILE * fp =  0 ; \
int token_type =  0 ; \
 \
while (c !=  '\0'  && c != '\n' \
&& (fp = infile->fp, \
(token_type = scantoken( c, (workp = work, &workp), work_end)) ==  65 ) \
&& fp !=  0  \
&& (defp = is_macro( (char **) 0 )) !=  0 ) { \
expand( defp, work, work_end); \
file = ungetstring( work, defp->name); \
c = skipws(); \
if (file != infile && macro_line !=  65535U  && (warn_level & 1)) \
cwarn( "Macro \"%s\" is expanded to 0 token" \
, defp->name, 0,  ((char *)  0 ) ); \
} \
 \
if (c == '\n' || c ==  '\0' ) { \
unget(); \
return  0 ; \
} \
 \
if (diag && fp ==  0  && token_type ==  65 ) { \
 \
if ( (strcmp(identifier, "defined") == 0)  && (warn_level & 1)) \
cwarn( "Macro \"%s\" is expanded to \"defined\"" \
, defp->name, 0,  ((char *)  0 ) ); \
} \
return token_type; \
} \
 \
void \
 \
skipnl( void) \
{ \
while (infile && infile->fp ==  0 ) { \
infile->bptr += strlen( infile->bptr); \
get(); \
} \
if (infile) \
infile->bptr += strlen( infile->bptr); \
} \
 \
int \
 \
skipws( void) \
{ \
   int c; \
 \
do { \
c = get(); \
} \
 \
while (c == ' ' || c ==  0x1F ); \
return c; \
} \
int \
 \
scantoken( int c, char ** out_pp, char * out_end) \
{ \
   char * out = *out_pp; \
int ch_type; \
int token_type = 0; \
int ch; \
 \
 \
ch_type = type[ c] & (~ ((char)128)  &  255 ); \
switch (ch_type) { \
case  1 : \
switch (c) { \
 \
case 'L': \
ch =  (*infile->bptr++ &  255 ) ; \
if (type[ ch] &  16 ) { \
if (ch == '"') \
token_type =  68 ; \
else \
token_type =  70 ; \
c = ch; \
*out++ = 'L'; \
break; \
} else { \
 (infile->bptr--) ; \
} \
 \
default: \
scanid( c); \
out = stpcpy( out, identifier); \
token_type =  65 ; \
break; \
} \
if (token_type ==  65 ) \
break; \
 \
case  16 : \
out = scanquote( c, out, out_end,  0 ); \
if (token_type == 0) { \
if (c == '"') \
token_type =  67 ; \
else \
token_type =  69 ; \
} \
break; \
case  4 : \
ch =  (*infile->bptr++ &  255 ) ; \
 (infile->bptr--) ; \
if (type[ ch] !=  2 ) \
goto operat; \
 \
case  2 : \
out = scannumber( c, out, out_end); \
token_type =  66 ; \
break; \
case  8 : \
operat: out = scanop( c, out); \
token_type =  71 ; \
break; \
default: \
if \
 \
 \
 \
((type[ c] &  32 ) || c ==  0x1E  || c ==  0x1D ) \
 \
token_type =  73 ; \
else \
token_type =  72 ; \
*out++ = c; \
*out =  '\0' ; \
break; \
} \
 \
if (out_end < out) \
cfatal( "Buffer overflow scanning token \"%s\"" \
, *out_pp, 0,  ((char *)  0 ) ); \
 \
if (debug &  2 ) \
dumptoken( token_type, *out_pp); \
*out_pp = out; \
 \
return token_type; \
} \
 \
static void \
 \
scanid( int c) \
{ \
static    char * const limit = &identifier[  0xFF ]; \
int long_ident =  0 ; \
   char * bp; \
 \
bp = identifier; \
 \
do { \
*bp++ = c; \
                                                            \
 
#ifndef X1FFF

c =  (*infile->bptr++ &  255 ) ; \
} while ((type[ c] & ( 1  |  2 )) && bp < limit); \
if (type[ c] & ( 1  |  2 )) { \
long_ident =  1 ; \
do { \
c =  (*infile->bptr++ &  255 ) ; \
} while (type[ c] & ( 1  |  2 )); \
} \
 (infile->bptr--) ; \
*bp =  '\0' ; \
 \
if (long_ident && (warn_level & 1)) \
cwarn( "Too long identifier truncated to \"%s\"" \
, identifier, 0,  ((char *)  0 ) ); \
 \
if (infile->fp && bp - identifier >  0x1F  && (warn_level & 2)) \
cwarn( "Identifier longer than %.0s%d bytes \"%s\"" \
,  ((char *)  0 ) ,  0x1F , identifier); \
} \
 \
char * \
 \
scanquote( int delim, char * out, char * out_end, int diag) \
{ \
const char * const skip_line = ", skipped the line"; \
const char * skip; \
   int c; \
int c1; \
char * out_p = out; \
 \
*out_p++ = delim; \
 \
if (delim == '<') \
delim = '>'; \
 \
 \
while ((c =  (*infile->bptr++ &  255 ) ) !=  '\0' ) { \
 \
 \
if (type[ c] &  64 ) { \
c1 =  (*infile->bptr++ &  255 ) ; \
if (type[ c1] &  ((char)128) ) { \
*out_p++ = c; \
*out_p++ = c1; \
goto chk_limit; \
} else { \
 (infile->bptr--) ; \
*out_p++ = c; \
if (infile->fp !=  0  &&  ifstack[0].stat  && diag) { \
*out_p = c1; \
*(out_p + 1) =  '\0' ; \
if (warn_level & 1) \
cwarn( \
"Illegal multi-byte character sequence \"%s\" in quotation", \
out_p - 1, 0,  ((char *)  0 ) ); \
} \
continue; \
} \
} \
 \
if (c == delim) { \
break; \
} else if (c == '\\' \
 \
&& delim != '>' \
 \
) { \
*out_p++ = c; \
c =  (*infile->bptr++ &  255 ) ; \
 \
if (type[ c] &  64 ) { \
 (infile->bptr--) ; \
continue; \
} \
} else if (c == ' ' && delim == '>' && infile->fp ==  0 ) { \
continue; \
 \
} else if (c == '\n') { \
break; \
} \
if (diag &&  __istype((c),  0x00000200L )  && ((type[ c] &  32 ) == 0) && (warn_level & 1)) \
cwarn( \
"Illegal control character %.0s0x%02x in quotation" \
,  ((char *)  0 ) , c,  ((char *)  0 ) ); \
*out_p++ = c; \
chk_limit: \
if (out_end < out_p) { \
*out_end =  '\0' ; \
cfatal( "Too long quotation %s", out, 0,  ((char *)  0 ) ); \
} \
} \
 \
if (c == '\n' || c ==  '\0' ) \
 (infile->bptr--) ; \
if (c == delim) \
*out_p++ = delim; \
*out_p =  '\0' ; \
if (diag) { \
skip = (infile->fp ==  0 ) ?  ((char *)  0 )  : skip_line; \
if (c != delim) { \
if (delim == '"') \
cerror( "Unterminated string literal %s%.0d%s" \
, out, 0, skip); \
else if (delim == '\'') \
cerror( "Unterminated character constant %s%.0d%s" \
, out, 0, skip); \
 \
else \
cerror( "Unterminated header name %s%.0d%s" \
, out, 0, skip); \
 \
out_p =  0 ; \
} else if (delim == '\'' && out_p - out <= 2) { \
cerror( "Empty character constant %s%.0d%s" \
, out, 0, skip); \
out_p =  0 ; \
} \
if (out_p - out >  0x1FD  && (warn_level & 2)) \
cwarn( "Quotation longer than %.0s%d bytes %s" \
,  ((char *)  0 ) ,  0x1FD , out); \
 \
} \
 \
return out_p; \
} \
static char * \
 \
scannumber( int c, char * out, char * out_end) \
{ \
char * out_p = out; \
 \
do { \
*out_p++ = c; \
if (c == 'E' || c == 'e') { \
c =  (*infile->bptr++ &  255 ) ; \
if (c == '+' || c == '-') { \
*out_p++ = c; \
c =  (*infile->bptr++ &  255 ) ; \
} \
} else { \
c =  (*infile->bptr++ &  255 ) ; \
} \
} while (type[ c] & ( 2  |  4  |  1 )); \
 \
*out_p =  '\0' ; \
if (out_end < out_p) \
cfatal( "Too long pp-number token \"%s\"" \
, out, 0,  ((char *)  0 ) ); \
 (infile->bptr--) ; \
return out_p; \
} \
static char * \
 \
scanop( int c, char * out) \
{ \
int c2, c3; \
 \
int c4; \
 \
 \
*out++ = c; \
 \
switch (c) { \
case '~': openum =  5 ; break; \
case '(': openum =  2 ; break; \
case ')': openum =  27 ; break; \
case '?': openum =  25 ; break; \
case ';': case '[': case ']': case '{': \
case '}': case ',': \
openum =  32 ; \
break; \
default: \
openum =  33 ; \
} \
 \
if (openum !=  33 ) { \
*out =  '\0' ; \
return out; \
} \
 \
c2 =  (*infile->bptr++ &  255 ) ; \
*out++ = c2; \
 \
switch (c) { \
case '=': \
openum = ((c2 == '=') ?  18  :  32 ); \
break; \
case '!': \
openum = ((c2 == '=') ?  19  :  6 ); \
break; \
case '&': \
switch (c2) { \
case '&': openum =  23 ; break; \
case '=': break; \
default : openum =  20 ; break; \
} \
break; \
case '|': \
switch (c2) { \
case '|': openum =  24 ; break; \
case '=': break; \
default : openum =  22 ; break; \
} \
break; \
case '<': \
switch (c2) { \
case '<': c3 =  (*infile->bptr++ &  255 ) ; \
if (c3 == '=') { \
openum =  34 ; \
*out++ = c3; \
} else { \
openum =  12 ; \
 (infile->bptr--) ; \
} \
break; \
case '=': openum =  15 ; break; \
 \
case ':': \
if (digraphs) \
openum =  0x42 ; \
else \
openum =  14 ; \
break; \
case '%': \
if (digraphs) \
openum =  0x40 ; \
else \
openum =  14 ; \
break; \
 \
default : openum =  14 ; break; \
} \
break; \
case '>': \
switch (c2) { \
case '>': c3 =  (*infile->bptr++ &  255 ) ; \
if (c3 == '=') { \
openum =  34 ; \
*out++ = c3; \
} else { \
openum =  13 ; \
 (infile->bptr--) ; \
} \
break; \
case '=': openum =  17 ; break; \
default : openum =  16 ; break; \
} \
break; \
case '#': \
 \
if (in_define) \
openum = ((c2 == '#') ?  31  :  30 ); \
else \
 \
openum =  32 ; \
break; \
case '+': \
switch (c2) { \
case '+': \
case '=': break; \
default : openum =  10 ; break; \
} \
break; \
case '-': \
switch (c2) { \
case '-': \
case '=': \
 \
break; \
case '>': \
 \
if (cplus) { \
if ((c3 =  (*infile->bptr++ &  255 ) ) == '*') { \
openum =  34 ; \
*out++ = c3; \
} else { \
 \
 (infile->bptr--) ; \
} \
} \
break; \
default : openum =  11 ; break; \
} \
break; \
case '%': \
switch (c2) { \
case '=': break; \
 \
case '>': \
if (digraphs) \
openum =  0x41 ; \
else \
openum =  9 ; \
break; \
case ':': \
if (! digraphs || ! in_define) { \
openum =  9 ; \
} else if ((c3 =  (*infile->bptr++ &  255 ) ) == '%') { \
if ((c4 =  (*infile->bptr++ &  255 ) ) == ':') { \
openum =  31 ; \
*out++ = c3; \
*out++ = c4; \
} else { \
 (infile->bptr--) ; \
 (infile->bptr--) ; \
openum =  30 ; \
} \
} else { \
 (infile->bptr--) ; \
openum =  30 ; \
} \
break; \
 \
default : openum =  9 ; break; \
} \
break; \
case '*': \
if (c2 != '=') \
openum =  7 ; \
 \
break; \
case '/': \
if (c2 != '=') \
openum =  8 ; \
 \
break; \
case '^': \
if (c2 != '=') \
openum =  21 ; \
 \
break; \
case '.': \
 \
if (c2 == '.') { \
c3 =  (*infile->bptr++ &  255 ) ; \
if (c3 == '.') { \
openum =  34 ; \
*out++ = c3; \
break; \
} else { \
 (infile->bptr--) ; \
openum =  32 ; \
} \
} \
 \
else if (cplus && c2 == '*') \
; \
 \
else \
openum =  32 ; \
break; \
case ':': \
 \
if (stdc2 && c2 == ':') \
; \
 \
else if (c2 == '>' && digraphs) \
openum =  0x43 ; \
 \
else \
openum =  26 ; \
break; \
default: \
 \
cfatal( "Bug: Punctuator is mis-implemented %.0s0x%x" \
,  ((char *)  0 ) , c,  ((char *)  0 ) ); \
 \
openum =  32 ; \
break; \
} \
 \
switch (openum) { \
 \
case  30 : \
 \
if (c == '%') break; \
 \
 \
case  32 : \
case  6 : case  20 : case  22 : case  14 : \
case  16 : case  10 : case  11 : case  9 : \
case  7 : case  8 : case  21 : case  5 : \
case  26 : \
 \
 (infile->bptr--) ; \
out--; \
break; \
default: \
break; \
} \
 \
*out =  '\0' ; \
return out; \
} \
int \
 \
get( void) \
{ \
int len; \
   int c; \
   FILEINFO * file; \
 \
if ((file = infile) ==  0 ) \
return  0 ; \
if (debug &  32 ) { \
printf( "get(%s), line %u, bptr = %d, buffer" \
, file->filename ? file->filename : "NULL" \
, line, (int) (file->bptr - file->buffer)); \
dumpstring(  ((char *)  0 ) , file->buffer); \
dumpunget( "get entrance"); \
} \
if ((c = (*file->bptr++ &  255 )) !=  '\0' ) { \
 \
return c; \
} \
if (file->fp && \
parse_line() !=  0 ) \
return get(); \
infile = file->parent; \
if (infile ==  0 ) \
return  0 ; \
free(( char *)file->buffer); \
if (file->filename) \
free( file->filename); \
if (file->fp) { \
fclose( file->fp); \
if (!  (strcmp("stdin", infile->filename) == 0) ) { \
infile->fp = fopen( infile->filename, "r"); \
fseek( infile->fp, infile->pos,  0 ); \
} \
len = (int) (infile->bptr - infile->buffer); \
infile->buffer = xrealloc( infile->buffer,  0x4000 ); \
 \
infile->bptr = infile->buffer + len; \
line = infile->line; \
wrongline =  1 ; \
} \
free(( char *)file); \
return get(); \
} \
 \
static char * \
 \
parse_line( void) \
{ \
char * temp; \
char * limit; \
char * tp; \
char * sp; \
   int c; \
 \
if ((sp = getline(  0 )) ==  0 ) \
return  0 ; \
tp = temp = xmalloc( (size_t)  0x4000 ); \
limit = temp +  0x4000  - 2; \
 \
while ((c = *sp++ &  255 ) != '\n') { \
 \
switch (c) { \
case '/': \
switch (*sp++) { \
case '*': \
if ((sp = read_a_comment( sp)) ==  0 ) { \
free( temp); \
return  0 ; \
} \
 \
 \
 \
 \
 \
if (temp < tp && *(tp - 1) != ' ') \
*tp++ = ' '; \
break; \
 \
case '/': \
if (stdc2) { \
 \
if (keepcomments) { \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = ('/')), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)('/'), (&__sF[1])) : (*((&__sF[1]))->_p = ('/'), (int)*((&__sF[1]))->_p++))   ; \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = ('*')), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)('*'), (&__sF[1])) : (*((&__sF[1]))->_p = ('*'), (int)*((&__sF[1]))->_p++))   ; \
while ((c = *sp++) != '\n') \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = (c)), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)(c), (&__sF[1])) : (*((&__sF[1]))->_p = (c), (int)*((&__sF[1]))->_p++))   ; \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = ('*')), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)('*'), (&__sF[1])) : (*((&__sF[1]))->_p = ('*'), (int)*((&__sF[1]))->_p++))   ; \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = ('/')), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)('/'), (&__sF[1])) : (*((&__sF[1]))->_p = ('/'), (int)*((&__sF[1]))->_p++))   ; \
} \
goto end_line; \
} \
 \
default: \
*tp++ = '/'; \
sp--; \
break; \
} \
break; \
case '\r': \
case '\f': \
 \
 \
 \
case '\v': \
 \
if (warn_level & 1) \
cwarn( "Converted %.0s0x%02x to a space" \
,  ((char *)  0 ) , c,  ((char *)  0 ) ); \
case '\t': \
case ' ': \
 \
if (temp < tp && *(tp - 1) != ' ') \
*tp++ = ' '; \
break; \
case '"': \
case '\'': \
infile->bptr = sp; \
 \
tp = scanquote( c, tp, limit,  1 ); \
if (tp ==  0 ) { \
free( temp); \
return parse_line(); \
} \
sp = infile->bptr; \
break; \
default: \
if ( __istype((c),  0x00000200L ) ) { \
cerror( \
"Illegal control character %.0s0x%x, skipped the character" \
,  ((char *)  0 ) , c,  ((char *)  0 ) ); \
} else { \
*tp++ = c; \
} \
break; \
} \
 \
if (limit < tp) { \
*tp =  '\0' ; \
cfatal( "Too long line spliced by comments :%s" \
, temp, 0,  ((char *)  0 ) ); \
} \
} \
 \
 \
end_line: \
 \
if (temp < tp && *(tp - 1) == ' ') \
tp--; \
*tp++ = '\n'; \
*tp =  '\0' ; \
infile->bptr = strcpy( infile->buffer, temp); \
free( temp); \
if (macro_line != 0 && macro_line !=  65535U ) { \
 \
 \
 \
 \
 \
 \
 \
 \
 \
 \
if (*(infile->buffer) == '#' \
 \
|| (*(infile->buffer) == '%' && *(infile->buffer + 1) == ':') \
 \
) \
 \
if (warn_level & 1) \
cwarn( \
"Macro started at line %.0s%u swallowed directive-like line" \
,  ((char *)  0 ) , macro_line,  ((char *)  0 ) ); \
} \
return infile->buffer; \
} \
 \
static char * \
 \
read_a_comment( char * sp) \
{ \
   int c; \
 \
if (keepcomments) { \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = ('/')), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)('/'), (&__sF[1])) : (*((&__sF[1]))->_p = ('/'), (int)*((&__sF[1]))->_p++))   ; \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = ('*')), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)('*'), (&__sF[1])) : (*((&__sF[1]))->_p = ('*'), (int)*((&__sF[1]))->_p++))   ; \
} \
c = *sp++; \
 \
while (1) { \
if (keepcomments) \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = (c)), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)(c), (&__sF[1])) : (*((&__sF[1]))->_p = (c), (int)*((&__sF[1]))->_p++))   ; \
 \
switch (c) { \
case '/': \
if ((c = *sp++) != '*') \
continue; \
if (warn_level & 1) \
cwarn( "\"/*\" in comment",  ((char *)  0 ) , 0,  ((char *)  0 ) ); \
if (keepcomments) \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = (c)), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)(c), (&__sF[1])) : (*((&__sF[1]))->_p = (c), (int)*((&__sF[1]))->_p++))   ; \
 \
case '*': \
if ((c = *sp++) != '/') \
continue; \
if (keepcomments) \
   (--((&__sF[1]))->_w < 0 ? ((&__sF[1]))->_w >= ((&__sF[1]))->_lbfsize ? (*((&__sF[1]))->_p = (c)), *((&__sF[1]))->_p != '\n' ? (int)*((&__sF[1]))->_p++ : __swbuf('\n', (&__sF[1])) : __swbuf((int)(c), (&__sF[1])) : (*((&__sF[1]))->_p = (c), (int)*((&__sF[1]))->_p++))   ; \
return sp; \
case '\n': \
if (! keepcomments) \
wrongline =  1 ; \
if ((sp = getline(  1 )) ==  0 ) \
return  0 ; \
break; \
default: \
break; \
} \
 \
c = *sp++; \
} \
 \
return sp; \
} \
 \
static char * \
 \
getline( int in_comment) \
{ \
 \
int converted =  0 ; \
 \
 \
int esc; \
 \
int len; \
char * ptr; \
 \
if (infile ==  0 ) \
return  0 ; \
ptr = infile->bptr = infile->buffer; \
 \
while (fgets( ptr, (int) (infile->buffer +  0x4000  - ptr), infile->fp) \
!=  0 ) { \
 \
line++; \
if (line ==  32768U  && (warn_level & 1)) \
cwarn( "Line number %.0s\"%u\" got beyond range" \
,  ((char *)  0 ) , line,  ((char *)  0 ) ); \
 \
if (debug & ( 2  |  32 )) { \
printf( "\n#line %u (%s)", line \
, infile->filename ? infile->filename : "NULL"); \
dumpstring(  ((char *)  0 ) , ptr); \
} \
 \
len = strlen( ptr); \
if ( 0x4000  - 1 <= ptr - infile->buffer + len \
&& *(ptr + len - 1) != '\n') { \
if ( 0x4000  - 1 <= len) \
cfatal( "Too long source line" \
,  ((char *)  0 ) , 0,  ((char *)  0 ) ); \
else \
cfatal( "Too long logical line" \
,  ((char *)  0 ) , 0,  ((char *)  0 ) ); \
} \
if (*(ptr + len - 1) != '\n') \
break; \
if (lflag) \
put_source( ptr); \
 \
if (tflag) \
converted = trigraph( ptr); \
if (converted) \
len = strlen( ptr); \
 \
 \
 \
len -= 2; \
if (len >= 0) { \
esc = (*(ptr + len) == '\\'); \
if (esc) { \
ptr = infile->bptr += len; \
wrongline =  1 ; \
continue; \
} \
} \
 \
 \
if (ptr - infile->buffer + len + 2 >  0x1FD  + 1 && (warn_level & 2)) \
 \
cwarn( "Logical source line longer than %.0s%d bytes" \
,  ((char *)  0 ) ,  0x1FD ,  ((char *)  0 ) ); \
 \
return infile->bptr = infile->buffer; \
} \
 \
 \
if (  (((infile->fp)->_flags &  0x0040 ) != 0)  ) \
cfatal( "File read error",  ((char *)  0 ) , 0,  ((char *)  0 ) ); \
at_eof( in_comment); \
if (iflag) { \
no_output--; \
keepcomments = cflag &&  ifstack[0].stat  && !no_output; \
} \
return  0 ; \
} \
 \
 \
 \
 \
 \
int \
 \
trigraph( char * in) \
{ \
const char * const tritext = "=(/)'<!>-\0#[\\]^{|}~"; \
int count = 0; \
   const char * tp; \
 \
while ((in = strchr( in, '?')) !=  0 ) { \
if (*++in != '?') \
continue; \
while (*++in == '?') \
; \
if ((tp = strchr( tritext, *in)) ==  0 ) \
continue; \
in[ -2] = tp[  10 ]; \
in--; \
memmove( in, in + 2, strlen( in + 1)); \
count++; \
} \
 \
if (count && (warn_level & 8)) \
cwarn( "%.0s%d trigraph(s) converted" \
,  ((char *)  0 ) , count,  ((char *)  0 ) ); \
return count; \
} \
static void \
 \
at_eof( int in_comment) \
{ \
   const char * const format \
= "End of %s with %.0d%s"; \
const char * const unterm_if_format \
= "End of %s within #if (#ifdef) section started at line %u"; \
const char * const unterm_macro_format \
= "End of %s within macro call started at line %u"; \
const char * const input \
= infile->parent ? "file" : "input"; \
const char * const no_newline \
= "no newline, skipped the line"; \
const char * const unterm_com \
= "unterminated comment, skipped the line"; \
 \
const char * const backsl = "\\, skipped the line"; \
int len; \
char * cp = infile->buffer; \
IFINFO * ifp; \
 \
len = strlen( cp); \
if (len && *(cp += (len - 1)) != '\n') { \
line++; \
*++cp = '\n'; \
*++cp =  '\0' ; \
cerror( format, input, 0, no_newline); \
} \
 \
 \
if (infile->buffer < infile->bptr) \
cerror( format, input, 0, backsl); \
 \
 \
if (in_comment) \
cerror( format, input, 0, unterm_com); \
 \
if (infile->initif < ifptr) { \
ifp = infile->initif + 1; \
 \
cerror( unterm_if_format, input, ifp->ifline,  ((char *)  0 ) ); \
ifptr = infile->initif; \
 ifstack[0].stat  = ifptr->stat; \
} \
 \
if (macro_line != 0 && macro_line !=  65535U ) { \
 \
cerror( unterm_macro_format, input, macro_line,  ((char *)  0 ) ); \
macro_line =  65535U ; \
} \
} \
 \
void \
 \
unget( void) \
{ \
if (infile !=  0 ) { \
--infile->bptr; \
 \
if (infile->bptr < infile->buffer) \
cfatal( "Bug: Too much pushback",  ((char *)  0 ) , 0,  ((char *)  0 ) ); \
 \
} \
 \
 \
if (debug &  32 ) \
dumpunget( "after unget"); \
 \
} \
 \
FILEINFO * \
 \
ungetstring( const char * text, const char * name) \
{ \
   FILEINFO * file; \
size_t size; \
 \
if (text) \
size = strlen( text) + 1; \
else \
size = 1; \
file = getfile( name, size); \
if (text) \
memcpy( file->buffer, text, size); \
else \
*file->buffer =  '\0' ; \
return file; \
} \
 \
char * \
 \
savestring( const char * text) \
{ \
   char * result; \
size_t size; \
 \
size = strlen( text) + 1; \
result = xmalloc( size); \
memcpy( result, text, size); \
return result; \
} \
 \
FILEINFO * \
 \
getfile( const char * name, size_t bufsize) \
{ \
   FILEINFO * file; \
size_t s_name; \
 \
file = (FILEINFO *) xmalloc( sizeof (FILEINFO)); \
file->buffer = xmalloc( bufsize); \
if (name) { \
s_name = strlen( name) + 1; \
file->filename = (char *) xmalloc( s_name); \
memcpy( file->filename, name, s_name); \
} else { \
file->filename =  0 ; \
} \
 \
file->bptr = file->buffer; \
return file; }                       \

#endif
#endif
#endif
#endif

