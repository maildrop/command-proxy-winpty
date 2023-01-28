
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <assert.h>

#include <tchar.h>
#include <windows.h>

#include <process.h>

#include "verify.h"

static 
int hputs_impl( HANDLE handle , const char *s )
{
  if( !s ){
    return 0;
  }
  const size_t length = strlen( s );
  size_t total_write_length = 0;
  while(0 < (length - total_write_length)){
    DWORD write_length = 0;
    if( ! WriteFile ( handle , (s + total_write_length) , (DWORD)(length-total_write_length), &write_length , NULL ) ){
      break;
    }
    total_write_length += (size_t)( write_length );
  }
  return (int)total_write_length;
}

#define hputs( s ) ( hputs_impl( GetStdHandle ( STD_OUTPUT_HANDLE ) , (s) ))

int main(int argc ,char* argv[] )
{
  (void)(argc);
  (void)(argv);

  setlocale( LC_ALL, "" );
  if( 1 ){
    SetConsoleCP ( GetACP () );
    SetConsoleOutputCP ( GetACP () );
  }else{
    SetConsoleCP ( CP_UTF8 );
    SetConsoleOutputCP( CP_UTF8 );
  }

  HRESULT (WINAPI * const createPseudoConsole)( COORD , HANDLE, HANDLE, DWORD, HPCON* ) = 
    (HRESULT (WINAPI *)( COORD , HANDLE, HANDLE, DWORD, HPCON* )) GetProcAddress( GetModuleHandle( TEXT( "kernel32" )) , "CreatePseudoConsole" );
  HRESULT (WINAPI * const closePseudoConsole)( HPCON ) =
    (HRESULT (WINAPI *)( HPCON )) GetProcAddress( GetModuleHandle( TEXT("kernel32") ), "ClosePseudoConsole" );
  
  if( createPseudoConsole && closePseudoConsole ){

    OutputDebugString( TEXT("Ready API CreatePseudoConsole and ClosePseudoConsole\n"  ));
    STARTUPINFOEX si = {0};
    si.StartupInfo.cb = sizeof( si );
    si.StartupInfo.hStdInput = INVALID_HANDLE_VALUE;
    si.StartupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    si.StartupInfo.hStdError = INVALID_HANDLE_VALUE;

    HANDLE writePipe         = INVALID_HANDLE_VALUE;
    HANDLE consoleInputPipe  = INVALID_HANDLE_VALUE;
    HANDLE readPipe          = INVALID_HANDLE_VALUE;
    HANDLE consoleOutputPipe = INVALID_HANDLE_VALUE;

    if( CreatePipe( &consoleInputPipe , &writePipe , NULL , 0 ) ){
      if( CreatePipe( &readPipe , &consoleOutputPipe , NULL , 0 ) ){

        assert( writePipe != INVALID_HANDLE_VALUE );
        assert( consoleInputPipe != INVALID_HANDLE_VALUE );
        assert( readPipe != INVALID_HANDLE_VALUE );
        assert( consoleOutputPipe != INVALID_HANDLE_VALUE );

        COORD coord = {120,30};
        HPCON hpcon = {0};
        HRESULT hResult = createPseudoConsole( coord , consoleInputPipe , consoleOutputPipe , 0, &hpcon );
        if( S_OK == hResult ){
          hputs( "hello world こんにちは世界\n" );


          char* pathext = NULL;
          size_t number_of_pathext = 0;


          if( 0 == _dupenv_s( &pathext , &number_of_pathext , "PATHEXT" )){
            assert( pathext );
            assert( strlen( pathext ) + 1 == number_of_pathext );
            printf( "%s ( %zd )\n" , pathext , number_of_pathext );
            free( pathext );
          }

          
          {
            size_t size = 0;
            InitializeProcThreadAttributeList( NULL , 1, 0 , &size );
            assert( 0 < size );
            LPPROC_THREAD_ATTRIBUTE_LIST proc_thread_attribute_list =
              (LPPROC_THREAD_ATTRIBUTE_LIST) HeapAlloc( GetProcessHeap() , 0 , size );
            assert( proc_thread_attribute_list );
            
            if( proc_thread_attribute_list ){
              RtlSecureZeroMemory( proc_thread_attribute_list , size );
              if( InitializeProcThreadAttributeList( proc_thread_attribute_list , 1, 0 , &size )){
                if( UpdateProcThreadAttribute( proc_thread_attribute_list , 0 ,
                                               PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ,
                                               hpcon, sizeof( hpcon ) ,
                                               NULL, NULL ) ){
                  printf("UpdateProcThreadAttribute\n");
                }

                /* attach PROC_THREAD_ATTRIBUTE_LIST */
                si.lpAttributeList = proc_thread_attribute_list;
                
                
                fflush( stdout );
                {
                  char* comspec = NULL;
                  size_t number_of_comspec = 0;
                  if( 0 == _dupenv_s( &comspec , &number_of_comspec , "COMSPEC" ) ){
                    assert( comspec );
                    assert( strlen( comspec ) +1 == number_of_comspec );
                    printf( "%s ( %zd )\n" , comspec , number_of_comspec );


                    static TCHAR commandline[] = TEXT("c:\\windows\\system32\\cmd.exe");
                    //static TCHAR commandline[] = TEXT("C:\\msys64\\usr\\bin\\ls.exe" );

                    PROCESS_INFORMATION piClient = {0};
                    if( CreateProcess( NULL , commandline ,
                                       NULL , NULL ,
                                       FALSE ,
                                       EXTENDED_STARTUPINFO_PRESENT | CREATE_NEW_PROCESS_GROUP  | CREATE_BREAKAWAY_FROM_JOB,
                                       NULL ,
                                       NULL ,
                                       &(si.StartupInfo),
                                       &piClient ) ){

                      if( 1 ){
                        const char term[] = "echo hello world && exit\r\n" ; // cmd.exe へ送る改行コードは \r\n でなければならない
                        DWORD write_size = 0;
                        OutputDebugString(TEXT("enter WriteFile"));
                        if( WriteFile( writePipe , term , sizeof( term ) - 1 , &write_size , NULL ) ){
                          printf( "write_size = %d\n" , write_size );
                          fflush( stdout );
                        }else{
                          assert( !"WriteFile() Failed" );
                        }
                        OutputDebugString(TEXT("return WriteFile"));
                      }

                      {
                        OutputDebugString(TEXT("enter ReadFile"));
                        char buf[1024];
                        DWORD read_size = 0;
                        
                        ReadFile( readPipe , buf , sizeof( buf ) , &read_size , NULL ) ;
                        if( 0 ){
                          DWORD write_size;
                          WriteFile( GetStdHandle( STD_OUTPUT_HANDLE ), buf , read_size , &write_size , NULL );
                        }
                        OutputDebugString( TEXT("return ReadFile" ));
                      }

                      OutputDebugString( TEXT("enter WaitForSingleObject") );
                      WaitForSingleObject( piClient.hProcess , INFINITE );
                      OutputDebugString( TEXT("leave WaitForSingleObject") );
                      VERIFY( CloseHandle( piClient.hThread ) );
                      VERIFY( CloseHandle( piClient.hProcess ));
                    }
                    free( comspec );
                  }
                }
                DeleteProcThreadAttributeList( proc_thread_attribute_list );
              }
              VERIFY( HeapFree( GetProcessHeap() , 0 , proc_thread_attribute_list) );
            }
          }
          closePseudoConsole( hpcon );
        }
        VERIFY( CloseHandle( writePipe ) );
        VERIFY( CloseHandle( consoleInputPipe ) );
      }
      VERIFY( CloseHandle( readPipe ) );
      VERIFY( CloseHandle( consoleOutputPipe ) );
    }
  }
  return EXIT_SUCCESS;
}
