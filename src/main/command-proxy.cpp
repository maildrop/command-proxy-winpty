/**
   Windows Console Program
   
   (C) 2010-2023 TOGURO Mikito, 
*/

#ifndef WINVER
#define WINVER _WIN32_WINNT_WIN10 
#endif /* WINVER */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif /* _WIN32_WINNT */

/*  マクロ値のチェック  */
#if (WINVER != _WIN32_WINNT )
#error it was set different value between WINVER and _WIN32_WINNT. 
#endif 

#include <iostream>
#include <limits>
#include <type_traits>
#include <array>
#include <vector>
#include <tuple>
#include <locale>
#include <sstream>

/* Windows SDK */
#include <tchar.h>
#include <windows.h>

/* CRT */
#include <process.h>

#if defined(__cplusplus )
# include <cassert>
#else /* defined(__cplusplus ) */
# include <assert.h>
#endif /* defined(__cplusplus ) */

#if !defined( VERIFY )
# if defined( NDEBUG )
#  define VERIFY( exp ) (void)(exp)
# else 
#  define VERIFY( exp ) assert( (exp) )
# endif /* defined( NDEBUG ) */
#endif /* !defined( VERIFY ) */

#pragma comment( lib , "ole32.lib" )

static HANDLE currentThreadHandle(void);
static BOOL CtrlHandler( DWORD fdwCtrlType );
static SHORT entry( int argc , char** argv);

/* for PIPE */
#define READ_SIDE (0)
#define WRITE_SIDE (1)

static
unsigned ( __stdcall pty_out_read_thread)( void *handle_value )
{
  HANDLE handle = reinterpret_cast<HANDLE>( handle_value );
  if( handle != INVALID_HANDLE_VALUE ){
    
    std::unique_ptr<std::array<char,4096>> buffer{new std::array<char,4096>()};

    DWORD numberOfBytesRead;
    while( ReadFile( handle , buffer->data() , DWORD(std::size( *buffer )) , &numberOfBytesRead , nullptr ) ){
      std::wstringstream out{};
      wchar_t wide[4096] = {L'\0'} ;
      int wnumber = MultiByteToWideChar( CP_UTF8 , 0 , buffer->data(), numberOfBytesRead , wide , sizeof( wide )/sizeof( wide[0] ));

      /* シーケンスを処理しないといけない
       * TODO とりあえずの適当な実装 真面目にあとでやる
       */
      do{
        wchar_t *cur = &wide[0];
        for( auto&& c = *cur ; cur < &wide[wnumber] ; c=(*++cur) ){
          if( 0x00 == c ){
            OutputDebugString( L"Soft Return\n" );
            continue;
          }
          if( L'\r' == c ){
            continue;
          }else if( 0x1b == c ){
            OutputDebugString( TEXT("ESC\n" ));
            if( L']' == *(cur+1) ){
              OutputDebugString( TEXT("ESC]\n" ));
              if( L'0' == *(cur+2) ){
                OutputDebugString( TEXT("ESC]0\n" ));
                if( L';' == *(cur+3) ){
                  wchar_t * const begin = cur+4;
                  wchar_t* idx = begin;
                  for( ; 0x07 != *idx ; ++idx );
                  std::wstring title{ begin , idx };
                  OutputDebugString( (L"ConsoleWindowTitle [" + title + L"]" ).c_str() );
                  cur += 4+(idx-begin);
                  continue;
                }
              }else if( L'4' == *(cur+2) ){
                OutputDebugString( L"ESC]4\n" );
              }else{
                std::wstring msg{L"ESC]0"};
                msg += (*cur+2);
                OutputDebugStringW( msg.c_str() );
              }
            }else if( L'[' == *(cur+1)){
              if( L'?' == *(cur+2) ){
                if( L'2' == *(cur+3) ){
                  if( L'5' == *(cur+4) ){
                    if( L'h' == *(cur+5) ){ // テキストカーソル有効化モード表示
                      cur += 5;
                      continue;
                    }else if( L'l' == *(cur+5 )){ // テキストカーソル有効化モード 非表示
                      cur += 5;
                      continue;
                    }
                  }
                }
              }else if( L'H' == *(cur+2) ){ // カーソル位置の移動
                cur+=2;
                continue;
              }else if( L'm' == *(cur+2) ){ // 色
                cur+=2;
                continue;
              }else if( L'1' == *(cur+2) ){
                if( L'0' == *(cur+3) ){
                  if( L';' == *(cur+4) ){
                    if( L'1' == *(cur+5) ){
                      if( L'H' == *(cur+6) ){
                        cur += 6;
                        out << L"\n";
                        continue;
                      }
                    }
                  }
                }
              }else if( L'2' == *(cur+2) ){
                if( L'J' == *(cur+3) ){
                  cur += 3;
                  continue;
                }else if( L'5' == *(cur+3) ){
                  if( L'H' == *(cur+4)){
                    cur += 4;
                    continue;
                  }
                }
              }else if( L'4' == *(cur+2)){
                if( L';' == *(cur+3) ){
                  if( L'1' == *(cur+4 ) ){
                    if( L'H' == *(cur+5)){
                      cur += 5;
                      out << '\n' << '\n';
                      continue;
                    }
                  }
                }
              }else if( L'8' == *(cur+2)){
                if( L';' == *(cur+3) ){
                  if( L'1' == *(cur+4 ) ){
                    if( L'H' == *(cur+5)){
                      cur += 5;
                      out << '\n' << '\n';
                      continue;
                    }
                  }
                }
              }
            }
          }
          out << c;
        }
      }while( 0 );

      {
        char narrow[4096] = {'\0'};
        std::wstring out_str = out.str();
        
        int nnumber = WideCharToMultiByte( CP_ACP , 0 , out_str.c_str() , (int)out_str.size() , narrow , sizeof( narrow )/sizeof( narrow[0] ),nullptr,nullptr );
        if( 0 < nnumber ){
          std::cout.write( narrow , nnumber );
          std::cout.flush();
        }
      }
    }
  }
  return EXIT_SUCCESS;
}

static HRESULT prepare_startup_information(HPCON hpc, STARTUPINFOEX& si)
{
    // Prepare Startup Information structure
    static_assert( sizeof( std::remove_reference<decltype( si )>::type ) == sizeof( STARTUPINFOEX ) );
    si = {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);
    si.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = INVALID_HANDLE_VALUE;
    si.StartupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    si.StartupInfo.hStdError = INVALID_HANDLE_VALUE;
    
    // Discover the size required for the list
    size_t bytesRequired = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

    // Allocate memory to represent the list
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc( GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList)
    {
        return E_OUTOFMEMORY;
    }

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)){
      DWORD lastError = GetLastError();
      HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
      return HRESULT_FROM_WIN32(lastError);
    }

    // Set the pseudoconsole information into the list
    if (!UpdateProcThreadAttribute(si.lpAttributeList,
                                   0,
                                   PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   hpc,
                                   sizeof(HPCON),
                                   NULL,
                                   NULL) ){
      DWORD lastError = GetLastError();
      HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
      return HRESULT_FROM_WIN32(lastError);
    }
    return S_OK;
}

HRESULT spawn_with_pseudo_console(COORD size)
{
  wchar_t cmdLineMutable[] = L"c:\\windows\\system32\\cmd.exe";

  HANDLE pty_in[2]  = { INVALID_HANDLE_VALUE , INVALID_HANDLE_VALUE };
  HANDLE pty_out[2] = { INVALID_HANDLE_VALUE , INVALID_HANDLE_VALUE };

  if( CreatePipe ( &pty_in[READ_SIDE] , &pty_in[WRITE_SIDE], nullptr , 0 ) ){
    if( CreatePipe ( &pty_out[READ_SIDE], &pty_out[WRITE_SIDE], nullptr , 0 )){
      HPCON hpc = {};

      {
        DWORD dwFlags = 0;
        if( GetHandleInformation( pty_in[READ_SIDE] , &dwFlags ) ){
          VERIFY( SetHandleInformation( pty_in[READ_SIDE], HANDLE_FLAG_INHERIT , ( dwFlags & HANDLE_FLAG_INHERIT )));
        }
      }
      {
        DWORD dwFlags = 0;
        if( GetHandleInformation( pty_out[WRITE_SIDE] , &dwFlags ) ){
          VERIFY( SetHandleInformation( pty_out[WRITE_SIDE], HANDLE_FLAG_INHERIT , ( dwFlags & HANDLE_FLAG_INHERIT )));
        }
      }
      
      // call CreatePseudoConsole( );

      HRESULT hr = CreatePseudoConsole( size , pty_in[READ_SIDE] , pty_out[WRITE_SIDE] , 0 , &hpc );
      
      VERIFY( CloseHandle( pty_out[WRITE_SIDE] ));
      VERIFY( CloseHandle( pty_in[READ_SIDE] ));
      
      if( !FAILED( hr ) ){
        assert( S_OK == hr );

        PROCESS_INFORMATION pi = {};
        STARTUPINFOEX si = {};
        si.StartupInfo.cb = sizeof( STARTUPINFOEX );
        
        VERIFY( S_OK == prepare_startup_information( hpc , si ) );

        // https://learn.microsoft.com/ja-jp/windows/win32/procthread/changing-environment-variables
        std::basic_ostringstream<TCHAR> process_env_block{};
        {
          boolean require_LC_CTYPE = true;
          { // copy current environment block
            TCHAR* env_block = GetEnvironmentStrings();
            if( env_block ){
              for( TCHAR* ptr = env_block ; TEXT('\0') != *ptr ; ptr += (_tcslen( ptr ) + 1 )){
                // TODO check LC_CTYPE 
                process_env_block << ptr << TEXT('\0');
              }
              VERIFY( FreeEnvironmentStrings(env_block) );
            }
          }
          if( require_LC_CTYPE ){
            std::basic_string<TCHAR> ctypes{};
            {
              ULONG numLanguages = 0;
              ULONG cchLanguagesBuffer = 0;
              if(GetUserPreferredUILanguages(MUI_LANGUAGE_NAME,&numLanguages , nullptr , &cchLanguagesBuffer )){
                TCHAR* language_buffer = reinterpret_cast<TCHAR*>(HeapAlloc( reinterpret_cast<HANDLE>(_get_heap_handle()) , HEAP_ZERO_MEMORY , cchLanguagesBuffer * sizeof( TCHAR ) ));
                if( language_buffer ){
                  if( GetUserPreferredUILanguages( MUI_LANGUAGE_NAME , &numLanguages , language_buffer , &cchLanguagesBuffer )){
                    for( TCHAR *ptr = language_buffer ; TEXT('\0') != *ptr ; ptr += (_tcslen( ptr ) +1)){
                      ctypes = ptr;
                      assert( ptr ); if( ptr ) break; // for suppress warning C4702. always break at first one. 
                    }
                  }
                  HeapFree( reinterpret_cast<HANDLE>(_get_heap_handle()),0, language_buffer );
                }
              }
              auto begin = ctypes.find(TEXT("-"));
              if( 0<= begin ){
                ctypes.replace( begin, 1 , TEXT("_"));
                ctypes = TEXT("LC_CTYPE=") + ctypes + TEXT(".UTF-8");
                process_env_block << ctypes << TEXT('\0');
              }
            }
          }
          process_env_block << TEXT('\0');
        }
        
        if( CreateProcess( NULL ,
                           cmdLineMutable ,
                           NULL ,
                           NULL ,
                           FALSE ,
                           /* dwFlags begin */
                           EXTENDED_STARTUPINFO_PRESENT 
#if defined( UNICODE )
                           | CREATE_UNICODE_ENVIRONMENT  /* UNICODE 場合 */
#endif /* defined( UNICODE ) */
                           /* dwFlags end */,
                           (LPVOID)( process_env_block.str().c_str() ),
                           NULL ,
                           &si.StartupInfo ,
                           &pi ) ){
          
          assert( 0 != pi.hProcess );
          assert( 0 != pi.hThread );
          
          std::vector<HANDLE> waitlist{};
          waitlist.push_back( pi.hProcess );
          VERIFY( CloseHandle( pi.hThread ) );

          // TODO console read thread の起動
          
          uintptr_t console_read_thread =
            _beginthreadex( nullptr , 0 ,
                            pty_out_read_thread, 
                            pty_out[READ_SIDE],
                            0,
                            nullptr );
          if( console_read_thread ){
            waitlist.push_back( reinterpret_cast<HANDLE>(console_read_thread));
          }

          struct pump_handles{
            HANDLE in;
            HANDLE out;
          } handles = { GetStdHandle( STD_INPUT_HANDLE ) , pty_in[WRITE_SIDE] };
          
          uintptr_t stdinput_reader = 
            _beginthreadex( nullptr , 0 , []( void* handle_value)->unsigned{
              const struct pump_handles * const handles = reinterpret_cast<const struct pump_handles * const>( handle_value );
              if( INVALID_HANDLE_VALUE != handles->in ){
                auto buffer = std::make_unique<std::array<char,4096>>();
                
                for(;;){
                  if( WAIT_IO_COMPLETION == SleepEx( 0 , TRUE ) ){
                    break;
                  }
                  DWORD numberOfBytesRead = 0;
                  if(! ReadFile( handles->in ,
                                 reinterpret_cast<LPVOID>(buffer->data()),
                                 DWORD( std::size( *buffer ) ),
                                 &numberOfBytesRead ,
                                 nullptr )){
                    DWORD lastError = GetLastError();
                    if( ERROR_OPERATION_ABORTED == lastError ){
                      // Input Operation was canceled.
                      OutputDebugString(TEXT(" stdinput_reader cancel" ));
                      break;
                    }
                    break;
                  }
                  std::wstringstream msg{};
                  msg << "numberOfBytesRead " << numberOfBytesRead << "  :" ;
                  wchar_t outbuffer[1024];
                  int number_of_converted_wchar = 0;
                  if( 0 < (number_of_converted_wchar =
                           MultiByteToWideChar ( GetACP() , 0 ,
                                                 buffer->data(), numberOfBytesRead ,
                                                 outbuffer, sizeof( outbuffer )/sizeof( outbuffer[0] )))) {
                    msg << std::wstring{ outbuffer , outbuffer+number_of_converted_wchar };
                    int require_size = WideCharToMultiByte( CP_UTF8 , 0 ,
                                                            outbuffer , number_of_converted_wchar  ,
                                                            nullptr , 0,
                                                            nullptr , nullptr );
                    msg << ", require_size = " << require_size;
                    if( 0 < require_size ){
                      std::unique_ptr<char[]> narrow{ new char[require_size] };
                      if( narrow ){
                        VERIFY( require_size == WideCharToMultiByte( CP_UTF8 , 0 ,
                                                                     outbuffer , number_of_converted_wchar ,
                                                                     narrow.get() , require_size ,
                                                                     nullptr , nullptr ) );
                        DWORD numberOfBytesWrite;
                        WriteFile( handles->out ,
                                   reinterpret_cast<LPVOID>(narrow.get()) ,
                                   require_size ,
                                   &numberOfBytesWrite ,
                                   nullptr );
                        if(! FlushFileBuffers( handles->out ) ){
                          DWORD lastError = GetLastError();
                          (void)(lastError);
                        }
                      }
                    }
                  }
                  OutputDebugStringW( msg.str().c_str()  );
                }
                SleepEx( 0 , TRUE );
              }
              return EXIT_SUCCESS;
            } , reinterpret_cast<void*>(&handles) , 0 , nullptr );

          if( stdinput_reader ){
            waitlist.push_back( reinterpret_cast<HANDLE>(console_read_thread));
          }
          
          for(;;){
            static_assert( (std::numeric_limits<DWORD>::max)() <=
                           (std::numeric_limits<decltype( waitlist.size() )>::max)() );
            assert( waitlist.size() < (std::numeric_limits<DWORD>::max)()  );
            DWORD const nCount = DWORD(waitlist.size());
            DWORD const wait_value = WaitForMultipleObjects( nCount , waitlist.data() , FALSE , INFINITE );
            
            if( WAIT_OBJECT_0 <= wait_value &&
                wait_value < WAIT_OBJECT_0 + nCount ){
              HANDLE handle = waitlist.at( wait_value - WAIT_OBJECT_0 );
              if( handle == pi.hProcess ){
                OutputDebugString(TEXT(" child process is terminated\n" ));
                goto END_OF_CONSOLE;
              }
            }
          }
        END_OF_CONSOLE:
          VERIFY( CloseHandle( pi.hProcess ) );
        }else{
          DWORD lastError = GetLastError();
          std::wcerr << "lastError : " << lastError << std::endl;
        }
        VERIFY( HeapFree( GetProcessHeap() , 0 , si.lpAttributeList ));
        
        ClosePseudoConsole( hpc );
      }
      VERIFY( CloseHandle( pty_out[READ_SIDE] ));
    }
    VERIFY( CloseHandle( pty_in[WRITE_SIDE] ) );
  }
  return S_OK;
}

static SHORT entry( int argc , char** argv)
{
  std::ignore = argc ;
  std::ignore = argv ;
  
  // MSYS2 のコマンドを実行するのには、環境変数LC_CTYPE を追加してやるべき
  // wmic os get OSLanguage /Value
  // GetUserPreferredUILanguages で取得するのがよさそう
  VERIFY( SetEnvironmentVariable( TEXT("LC_CTYPE") , TEXT("ja_JP.UTF-8") ) );

  if(0){
    HANDLE hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );
    assert( hStdOut != INVALID_HANDLE_VALUE );
    if( hStdOut != INVALID_HANDLE_VALUE ){
      DWORD dwFlags = 0;
      if( GetHandleInformation( hStdOut , &dwFlags ) ){
        std::cout << "hStdOut: " << ( (dwFlags & HANDLE_FLAG_INHERIT)  ? "HANDLE_FLAG_INHERIT" : "" ) << std::endl;
      }
    }
  }
  
  spawn_with_pseudo_console( COORD{200,100} );
  return EXIT_SUCCESS;
}

static CRITICAL_SECTION primary_thread_handle_section = {};
static HANDLE primary_thread_handle = NULL;

static BOOL CtrlHandler( DWORD fdwCtrlType )
{
  switch( fdwCtrlType ){
  case CTRL_C_EVENT:
    {
      EnterCriticalSection( &primary_thread_handle_section );
      if( primary_thread_handle ){
        if( !QueueUserAPC( []( ULONG_PTR )->void{
          return ;
        }, primary_thread_handle , 0 ) ){
          DWORD lastError = GetLastError();
          assert( !(lastError == ERROR_ACCESS_DENIED) || !"DuplicateHandle() require ACCESS flag. use dwOptions as DUPLICATE_SAME_ACCESS" );
          return FALSE;
        }
      }
      LeaveCriticalSection( &primary_thread_handle_section );
    }
    return TRUE;
  default:
    break;
  }
  return FALSE;
}

static HANDLE currentThreadHandle(void)
{
  HANDLE h = NULL;
  if( DuplicateHandle( GetCurrentProcess() , GetCurrentThread() ,
                       GetCurrentProcess() , &h ,
                       0 ,
                       FALSE ,
                       DUPLICATE_SAME_ACCESS ) ){
    return h;
  }
  return NULL;
}


#if defined( __cplusplus )
namespace wh{
  namespace util{

    /*
      今、 SFINAE を使って CRTの機能である、_get_heap_handle() が使えるかどうかを判定したいのだが、
      decltype( _get_heap_handle() ) とすると この部分は、template（の型引数）に依存しなくなるので、
      即時コンパイルエラーになる。
      このために、ダミーのパラメータパック引数 type_t を追加して、_get_heap_handle() をテンプレート型依存にする。
      
      使うときは
      decltype( enable_get_heap_handle(nullptr) ) と、nullptrを引数に渡す
      
      _get_heap_handle() が存在しない環境の場合、このテンプレートはSFINAEで除去される。
    */
    template<typename ...type_t >
    static auto enable_get_heap_handle( std::nullptr_t&&, type_t ...args )->
      decltype( _get_heap_handle( args... ) , std::true_type{} );
    static auto enable_get_heap_handle( ... )->std::false_type;

    template <typename type_t>
    struct CRTHeapEnableTerminateionOnCorruption{
      inline
      void operator()(){
# if !defined( NDEBUG )
        OutputDebugString(TEXT("disable HeapEnableTerminationOnCorruption\n"));
# endif
        return;
      }
    };
    
    template<>
    struct CRTHeapEnableTerminateionOnCorruption<std::true_type>
    {
      inline
      void operator()(){
# if !defined( NDEBUG )
        OutputDebugString(TEXT("enable HeapEnableTerminationOnCorruption\n"));
# endif /* !defined( NDEBUG ) */
        HANDLE const crtHeapHandle = reinterpret_cast<HANDLE>( _get_heap_handle()); 
        VERIFY( crtHeapHandle != NULL );
        if( crtHeapHandle ){
          if( GetProcessHeap() == crtHeapHandle ){// CRT のバージョンによって ProcessHeap を直接使っていることがある。
            // MSVC 201? あたりで変更されている。
# if !defined( NDEBUG )
            OutputDebugString(TEXT(" CRT uses ProcessHeap directly.\n" ));
#endif /* !defined( NDEBUG ) */
          }else{
            VERIFY( HeapSetInformation(crtHeapHandle, HeapEnableTerminationOnCorruption, NULL, 0) );
            
            // 追加でLow-Fragmentation-Heap を使用するように指示する。
            ULONG heapInformaiton(2L); // Low Fragmention Heap を指示
            VERIFY( HeapSetInformation(crtHeapHandle, HeapCompatibilityInformation, 
                                       &heapInformaiton, sizeof( heapInformaiton)));
          }
        }
        return;
      }
    };
  }
}

#endif /* defined( __cplusplus ) */

int main(int argc,char* argv[])
{
  /**********************************
   // まず、ヒープ破壊の検出で直ぐ死ぬように設定
   ***********************************/
  // HeapSetInformation() は Kernel32.dll 
  { // プロセスヒープの破壊をチェック Kernel32.dll依存だが、これは KnownDLL
    HANDLE processHeapHandle = ::GetProcessHeap(); // こちらの関数は、HANDLE が戻り値
    VERIFY( processHeapHandle != NULL );
    if( processHeapHandle ){
      VERIFY( HeapSetInformation(processHeapHandle, HeapEnableTerminationOnCorruption, NULL, 0) );
    }
  }
  
  // 可能であれば、CRT Heap に対しても HeapEnableTerminationOnCorruption を設定する。
  wh::util::CRTHeapEnableTerminateionOnCorruption<decltype(wh::util::enable_get_heap_handle(nullptr))>{}();
  
  /***************************************************
  DLL の読み込み先から、カレントワーキングディレクトリの排除 
  ***************************************************/
  { 
    /* DLL探索リストからカレントディレクトリの削除
       http://codezine.jp/article/detail/5442?p=2  */
    HMODULE kernel32ModuleHandle = GetModuleHandle( _T("kernel32")); // kernel32 は 実行中アンロードされないからこれでよし
    if( NULL == kernel32ModuleHandle){
      return 1;
    }
    FARPROC pFarProc = GetProcAddress( kernel32ModuleHandle , "SetDllDirectoryW");
    if( NULL == pFarProc ){
      return 1;
    }
    decltype(SetDllDirectoryW) *pSetDllDirectoryW(reinterpret_cast<decltype(SetDllDirectoryW)*>(pFarProc) );
    VERIFY( 0 != pSetDllDirectoryW(L"") ); /* サーチパスからカレントワーキングディレクトリを排除する */
  }

  std::locale::global( std::locale(""));

  int return_value = 0;
  InitializeCriticalSection( &primary_thread_handle_section );
  struct section_raii_type{
    ~section_raii_type(){
      DeleteCriticalSection( &primary_thread_handle_section );
    }
  } section_raii;
  
  struct arg_type{
    int argc;
    char** argv;
  } arg = { argc , argv} ;

  const HRESULT hr = CoInitializeEx( nullptr , COINIT_MULTITHREADED );
  if( ! SUCCEEDED( hr ) ){
    return 3;
  }

  assert( S_OK == hr );
  
  struct com_raii_type{
    ~com_raii_type(){
      (void)CoUninitialize();
    }
  } const raii{};
  
  const uintptr_t ptr = 
    _beginthreadex( nullptr , 0 , []( void * arg_ptr ) -> unsigned{
      HRESULT hr = CoInitializeEx( nullptr , COINIT_APARTMENTTHREADED );
      if( SUCCEEDED(hr) ){
        
        assert( S_OK == hr );
        
        const com_raii_type raii{};
        
        struct arg_type &arg = *reinterpret_cast<arg_type*>( arg_ptr );
        return entry( arg.argc , arg.argv );
      }
      return EXIT_FAILURE;
    }, &arg , 0 , nullptr );
  
  if( ! ptr ){
    return EXIT_FAILURE;
  }
  
  assert( ptr );
  
  if( SetConsoleCtrlHandler( CtrlHandler , TRUE ) ){
    EnterCriticalSection( &primary_thread_handle_section );
    {
      primary_thread_handle = currentThreadHandle();
    }
    LeaveCriticalSection( &primary_thread_handle_section );
    
    for(;;){
      DWORD waitValue = WaitForSingleObjectEx( reinterpret_cast<HANDLE>( ptr ) , INFINITE , TRUE );
      if( WAIT_OBJECT_0 == waitValue ){
        break;
      }else if( WAIT_IO_COMPLETION == waitValue ){
        OutputDebugString( TEXT(" WAIT_IO_COMPLETION \n"));
        continue;
      }
      break;
    }
    EnterCriticalSection( &primary_thread_handle_section );
    {
      VERIFY( CloseHandle( primary_thread_handle ) );
      primary_thread_handle = NULL;
    }
    LeaveCriticalSection( &primary_thread_handle_section );
    
    VERIFY( SetConsoleCtrlHandler( nullptr , FALSE ) );
  }
  
  {
    DWORD exitCode = 0;
    if( ! GetExitCodeThread( reinterpret_cast<HANDLE>( ptr ) , &exitCode ) ){
      // failed GetExitCodeThread()
      DWORD lastError = GetLastError();
      assert( lastError );
    }else{
      return_value = exitCode;
    }
    VERIFY( CloseHandle( reinterpret_cast<HANDLE>(ptr) ) );
  }

  return return_value;
}
