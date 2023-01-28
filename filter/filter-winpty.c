/**



 */
#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <assert.h>

#include <tchar.h>
#include <windows.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */
  extern FILE* yyin;
  extern int yylex(void);
  int yywrap(void);
  
#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

int main( int argc, char* argv[] )
{
  (void)(argc);
  (void)(argv);
  setlocale(LC_CTYPE,"");

  yyin = stdin;
  yylex();

  return EXIT_SUCCESS;
}

int yywrap(void)
{
  return(1);
}
