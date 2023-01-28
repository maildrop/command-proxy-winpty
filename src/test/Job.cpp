/**

 */
#include <iostream>
#include <type_traits>
#include <memory>
#include <array>

#include <cassert>

#include <tchar.h>
#include <windows.h>
#include <process.h>

#pragma comment(lib , "Ole32.lib") 

struct runtime_config{
  BOOL interactive;
};


BOOL spawn( const TCHAR* command_line ,
            PROCESS_INFORMATION* ppinfo ,
            STARTUPINFOEX *startupinfo )
{
  (void)( command_line );
  (void)( ppinfo );
  (void)( startupinfo );
  return TRUE;
}

static int entry(int argc, char** argv)
{
  struct runtime_config config = {};
  
  for( int i = 0 ; i < argc ; ++i ){
    if( 0 == strcmp( argv[i] , "-i" ) ){
      config.interactive = TRUE;
      continue;
    }
  }

  BOOL result = 0;
  if( IsProcessInJob( GetCurrentProcess() , nullptr , &result ) ){

    if( result ){
      std::wcerr << "Process In Job" << std::endl;
    }else{
      std::wcerr << "Process is free" << std::endl;
    }

    if( config.interactive ){
      std::wcerr << "Interactive" << std::endl;
      std::unique_ptr<TCHAR[]> moduleFileName{new TCHAR[ MAX_PATH ]};
      
      if( GetModuleFileName( GetModuleHandle(NULL) , moduleFileName.get() , MAX_PATH ) ){

        std::wcout << moduleFileName.get() << std::endl;

        PROCESS_INFORMATION ppinfo = {};
        STARTUPINFOEX startupinfo = {};
        startupinfo.StartupInfo.cb = sizeof( startupinfo );

        if( CreateProcess( moduleFileName.get() ,
                           NULL ,
                           NULL ,
                           NULL ,
                           FALSE ,
                           CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
                           NULL ,
                           NULL ,
                           &startupinfo.StartupInfo ,
                           &ppinfo ) ){
          std::wcerr << "spawn" << std::endl;
          ResumeThread( ppinfo.hThread );
          CloseHandle( ppinfo.hThread );


          
          for(;;){
            std::wcerr << "Enter WaitForSingleObjectEx" << std::endl;
            
            const DWORD waitValue = WaitForSingleObjectEx( ppinfo.hProcess , INFINITE , TRUE );
            if( WAIT_FAILED == waitValue ){
              const DWORD lastError = GetLastError();
              (void)(lastError);
              break;
            }
            
            switch( waitValue ){
            case WAIT_IO_COMPLETION:
              std::wcerr << "WAIT_IO_COMPLETION" << std::endl;
              continue;
            case WAIT_OBJECT_0:
              {
                // GetExitCode();
              }
              break;
            case WAIT_ABANDONED:
              break;
            default:
              break;
            }
            goto END_OF_LOOP;
          }
        END_OF_LOOP:
          ;
          CloseHandle( ppinfo.hProcess );
        }else{
          std::wcerr << "fail CreateProcess" << std::endl;
        }
      }
    }
  }
  SleepEx(1000 , TRUE);
  return EXIT_SUCCESS;
}


static CRITICAL_SECTION thread_handle_critical_section = {};
static HANDLE working_thread = NULL;

BOOL WINAPI console_control_handler( DWORD fdwCtrlType )
{
  switch( fdwCtrlType ){
  case CTRL_C_EVENT:
    {
      std::wcout << "CTRL_C_EVENT" << std::endl;
      HANDLE handle = NULL;
      {
        EnterCriticalSection( &thread_handle_critical_section );
        handle = working_thread;
        LeaveCriticalSection( &thread_handle_critical_section );
      }
      if( handle ){
        if( !QueueUserAPC ( []( ULONG_PTR )->void{ return; }, handle , NULL ) ){
          return FALSE;
        }
      }else{
        return FALSE;
      }
    }
    return TRUE;
  default:
    return FALSE;
  }
}

int main(int argc,char *argv[])
{
  InitializeCriticalSectionAndSpinCount( &thread_handle_critical_section , 400 );
  {
    
    SetConsoleCP ( GetACP () );
    SetConsoleOutputCP ( GetACP () );
    
    std::locale::global( std::locale("") );
    
    HRESULT hr = CoInitializeEx( nullptr , COINIT_MULTITHREADED );
    assert( S_OK == hr );
    if(! SUCCEEDED( hr ) ){
      return 3;
    }
    
    struct raii_t{
      ~raii_t(){
        CoUninitialize();
      }
    } raii;
    
    struct {
      int argc;
      char** argv;
    } args = { argc , argv };
    
    
    if( SetConsoleCtrlHandler( console_control_handler , TRUE ) ){
      
    }
    
    unsigned threadaddr;
    uintptr_t thread_handle_intptr =
      _beginthreadex( nullptr , 0,
                      [](void* ptr)->unsigned{
                        auto const arg = reinterpret_cast<std::add_pointer<decltype( args )>::type>( ptr );
                        int argc = arg->argc;
                        char** argv = arg->argv;
                        
                        HRESULT hr = CoInitializeEx( nullptr , COINIT_APARTMENTTHREADED );
                        assert( S_OK == hr );
                        if( !SUCCEEDED( hr ) ){
                          return 3;
                      }else{
                          raii_t raii;
                          return entry( argc, argv );
                        }
                      },
                      reinterpret_cast<void*>(&args),
                      0 ,
                      &threadaddr );
    if( thread_handle_intptr ){
      HANDLE thread_handle = reinterpret_cast<HANDLE>( thread_handle_intptr );
      {
        EnterCriticalSection( &thread_handle_critical_section );
        working_thread = thread_handle;
        LeaveCriticalSection( &thread_handle_critical_section );
      }
      WaitForSingleObject( thread_handle , INFINITE );
      CloseHandle( thread_handle );
    }
  }
  DeleteCriticalSection( &thread_handle_critical_section );
  return EXIT_SUCCESS;
}
