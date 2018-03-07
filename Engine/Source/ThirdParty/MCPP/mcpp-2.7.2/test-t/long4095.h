/* long4095.c:  logical source line of 4095 bytes long. */

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

