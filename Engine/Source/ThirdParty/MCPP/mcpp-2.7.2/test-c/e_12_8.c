/* e_12_8.c:    Out of range of integer pp-token in #if expression. */

/* 12.8:    Preprocessing number perhaps out of range of unsigned long. */
#if     123456789012345678901
#endif

main( void)
{
    return  0;
}

