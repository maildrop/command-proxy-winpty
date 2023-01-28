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
#include <sstream>
#include <type_traits>
#include <tuple>
#include <locale>
#include <array>
#include <memory>
#include <iterator>

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


class StandardInputDispatcher{
private:
  uintptr_t worker;
public:
  StandardInputDispatcher() :  worker( NULL )
  {
  }
  
  StandardInputDispatcher& invoke (HANDLE input , HANDLE output)
  {
    struct parameter_t{
      HANDLE input ;
      HANDLE output;
    } parameter = { input , output };
    return *this;
  }
  void dispose(void)
  {
    
    if( worker ){
      if( CloseHandle( reinterpret_cast<HANDLE>(worker) ) ){
        worker = NULL;
      }
    }
  }
  static
  StandardInputDispatcher cocreate( HANDLE input, HANDLE output )
  {
    return StandardInputDispatcher{}.invoke( input , output );
  }
};


static SHORT entry( int argc , char** argv)
{
  std::ignore = argc ;
  std::ignore = argv ;

  static_assert( sizeof( HANDLE ) == sizeof( void* ) );

  struct pump_handles{
    HANDLE in;
    HANDLE out;
  } handles = { GetStdHandle( STD_INPUT_HANDLE ) , GetStdHandle( STD_OUTPUT_HANDLE ) };
  
  uintptr_t stdinput_reader = 
    _beginthreadex( nullptr , 0 , []( void* handle_value)->unsigned{
      const struct pump_handles * const handles = reinterpret_cast<const struct pump_handles * const>( handle_value );
      if( INVALID_HANDLE_VALUE != handles->in ){
        auto buffer = std::make_unique<std::array<char,4096>>();
        DWORD numberOfBytesRead = 0;
        for(;;){
          if( WAIT_IO_COMPLETION == SleepEx( 0 , TRUE ) ){
            break;
          }
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


  // do something 
  {
    DWORD file_type = GetFileType( GetStdHandle( STD_INPUT_HANDLE ) );
    switch( file_type ){
    case FILE_TYPE_CHAR:
      OutputDebugString( TEXT("FILE_TYPE_CHAR\n" ));
      break;
    case FILE_TYPE_DISK:
      OutputDebugString( TEXT("FILE_TYPE_DISK\n" ));
      break;
    case FILE_TYPE_PIPE:
      OutputDebugString( TEXT("FILE_TYPE_PIPE\n" ));
      break;
    case FILE_TYPE_REMOTE: /* unused */
      OutputDebugString( TEXT("FILE_TYPE_REMOTE\n" ));
      break;
    case FILE_TYPE_UNKNOWN: 
      OutputDebugString( TEXT("FILE_TYPE_UNKNOWN\n" ));
      break;
    default:
      break;
    }
    Sleep( 10000);
  }
  
  if( stdinput_reader ){
    DWORD waitValue = WaitForSingleObject( reinterpret_cast<HANDLE>(stdinput_reader) , 0 );
    if( WAIT_OBJECT_0 == waitValue ){
      
    }else{
      do{
        if( WAIT_TIMEOUT == waitValue ){
          QueueUserAPC( [](ULONG_PTR)->void{return;} ,reinterpret_cast<HANDLE>( stdinput_reader ) , (ULONG_PTR)nullptr );
          // wakeup ReadFile() 
          if( !CancelSynchronousIo( reinterpret_cast<HANDLE>(stdinput_reader) )){
            DWORD lastError = GetLastError();
            std::cout << lastError << std::endl;
          }
        }else if( WAIT_OBJECT_0 == waitValue ){
          break;
        }else {
          break;
        }
        waitValue = WaitForSingleObject( reinterpret_cast<HANDLE>(stdinput_reader) , 1000 );
      }while( true ) ;
    }
    VERIFY( CloseHandle( reinterpret_cast<HANDLE>(stdinput_reader) ) );
  }
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
