#ifndef NDEBUG
#ifndef X64
# define ASSERT_M( isOK, message ) \
        if ( !(isOK) ) { \
        (void)printf("ERROR!! Assert ‘%s’ failed on line %d " \
        "in file ‘%s’\n%s\n", \
#isOK, __LINE__, __FILE__, #message); \
        __asm { int 3 } \
        }

# define ASSERT( isOK ) \
        if ( !(isOK) ) { \
        (void)printf("ERROR!! Assert ‘%s’ failed on line %d " \
        "in file ‘%s’\n%s\n", \
#isOK, __LINE__, __FILE__); \
        __asm { int 3 } \
        }
#else
# define ASSERT_M( unused, message ) do {} while ( false )
# define ASSERT( unused ) do {} while ( false )
#endif
#else
# define ASSERT_M( unused, message ) do {} while ( false )
# define ASSERT( unused ) do {} while ( false )
#endif
