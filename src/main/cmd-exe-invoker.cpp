#include <tchar.h>
#include <windows.h>

#include <iostream>
#include <locale>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>
#include <tuple>

#include "wh-parse-option.h"
#include "wh-error-handling.h"

namespace application{
  namespace environment{
    static std::wstring
    comspec() noexcept
    {
      constexpr const wchar_t* comspec_string = L"COMSPEC";

      size_t const required_length =
        size_t{ GetEnvironmentVariableW( comspec_string , nullptr , 0 )};
      
      if( ! required_length ){
        const DWORD lastError = GetLastError();
        if ( ERROR_ENVVAR_NOT_FOUND == lastError ) {
          return std::wstring{};
        }
      }
      
      if( 0 < required_length ){
        std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>( required_length );
        DWORD const dword = GetEnvironmentVariableW( comspec_string , buffer.get() , DWORD(required_length) ) ;
        if( 0 < dword ){
          return std::wstring{ buffer.get(), size_t{ dword }  };
        }
        
        const DWORD lastError{ GetLastError() };
        assert( ERROR_ENVVAR_NOT_FOUND == lastError );

      }
      return std::wstring{};
    }
  }; // end of namespace environment 
}; // end of namespace application

template<typename runtime_option_t>
static inline
int create_command_process( HANDLE& standard_input , HANDLE& standard_output , HANDLE& standard_error ,
                            const runtime_option_t& runtime_option ,
                            std::unique_ptr<wchar_t[]> &commandlineParam ) noexcept
{
  std::ignore = runtime_option;
  
  STARTUPINFOW startupInfo{};
  startupInfo.cb = sizeof( startupInfo );
  
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdInput = standard_input ;
  startupInfo.hStdOutput = standard_output;
  startupInfo.hStdError = standard_error;
  
  PROCESS_INFORMATION process_information{};
  if( CreateProcessW( nullptr,
                      commandlineParam.get(),
                      nullptr ,
                      nullptr ,
                      TRUE,
                      NORMAL_PRIORITY_CLASS , 
                      nullptr ,
                      nullptr ,
                      &startupInfo ,
                      &process_information ) ){

    assert( NULL != process_information.hProcess );
    assert( NULL != process_information.hThread );

    // Thread は使わないので先に閉じる。
    VERIFY( ::CloseHandle( process_information.hThread ) );

    DWORD waitResult{};
    do{
      waitResult = WaitForSingleObject( process_information.hProcess, INFINITE );
      switch( waitResult ){
      case WAIT_ABANDONED:
        goto END_OF_WAIT_LOOP;
        break;
      case WAIT_TIMEOUT:
        break;
      case WAIT_FAILED:
        {
          DWORD const lastError = GetLastError();
          std::ignore = lastError; // TODO 
        }
        goto END_OF_WAIT_LOOP;
      case WAIT_OBJECT_0:
        break;
      default:
        assert( !"unknown result" );
        goto END_OF_WAIT_LOOP;
      }
    }while( WAIT_OBJECT_0 != waitResult );
  END_OF_WAIT_LOOP:
    VERIFY( ::CloseHandle( process_information.hProcess ) );
  }else{
    DWORD const lastError{ ::GetLastError() };
    std::ignore = lastError ; // TODO;
  }
  return EXIT_SUCCESS;
}

template<typename runtime_option_t>
static inline 
int io_wrap( HANDLE standard_input , HANDLE standard_output , HANDLE standard_error,
             const runtime_option_t& runtime_option ) noexcept
{
  std::wstring const commandline{ ([&runtime_option]()->std::wstring{
    wh::CommandArgumentsBuilder builder{ application::environment::comspec() };

    for( auto&& file : runtime_option.fileList ){
      builder.add_value( file );
    }
    
    return builder.build();
  })() };

  std::unique_ptr<wchar_t[]> commandlineParam = ([&commandline](){
    /* std::wstring の commandLine を wchar_t[] の中に納める*/
    const size_t array_size{ commandline.size()+1 };
    std::unique_ptr<wchar_t[]> commandlineParam{ std::make_unique<wchar_t[]>( array_size ) };
    if( static_cast<bool>( commandlineParam ) ){
      VERIFY( commandlineParam.get() ==
              std::char_traits<wchar_t>::copy( commandlineParam.get() , commandline.c_str() , array_size ) );
    }else{
      assert( nullptr == commandlineParam.get() );// メモリ確保に失敗した
    }
    return commandlineParam;
  })();

  if( ! static_cast<bool>( commandlineParam ) ){
    ::SetLastError( ERROR_OUTOFMEMORY ); // メモリが確保できなかった
    return EXIT_FAILURE;
  }
  
  if( runtime_option.debug ){
    std::wcout << L" CreateProcess() [" << commandlineParam.get() << L"]" << std::endl;
    std::wcout << L"  STDIN  handle(" << standard_input << ")" << std::endl;
    std::wcout << L"  STDOUT handle(" << standard_output << ")" << std::endl;
    std::wcout << L"  STDERR handle(" << standard_error << ")" << std::endl;
  }

  int exit_code_of_child_process{EXIT_SUCCESS};
  
  if( true ){
    HANDLE std_output_handle = GetStdHandle( STD_OUTPUT_HANDLE );
    switch( GetFileType( std_output_handle ) ){
    case FILE_TYPE_CHAR:
      {
        if( !FlushFileBuffers( std_output_handle ) ){
          // Console の場合は FlushFileBuffers は失敗する。
          DWORD const lastError = GetLastError();
          assert( lastError ==  ERROR_INVALID_HANDLE );
          if( ! (ERROR_INVALID_HANDLE == lastError ) ){
            std::wcerr << "FlushFileBuffers( std_output_handle = " << std_output_handle<<" ) "
                       << "GetLastError() = "<< lastError << std::endl;
          }
        }
        UINT const consoleOutputCP = GetConsoleOutputCP();
        SetConsoleOutputCP( 65001u );

        if( runtime_option.debug ){ // UTF-8 での出力が正しくできていることの確認 のためのコード
          const char buf[] = u8"UTF8:日本語\n";
          DWORD byteOfWrite{};
          WriteFile( GetStdHandle( STD_OUTPUT_HANDLE ) ,
                     buf , sizeof( buf ) - 1 , &byteOfWrite , nullptr );
        }

        exit_code_of_child_process = 
          create_command_process( standard_input , standard_output , standard_error ,
                                  runtime_option ,
                                  commandlineParam );
        
        if( !FlushFileBuffers( std_output_handle ) ){
          // Console の場合は FlushFileBuffers は失敗する。
          DWORD const lastError = GetLastError();
          assert( lastError ==  ERROR_INVALID_HANDLE );
          if( ! (ERROR_INVALID_HANDLE == lastError ) ){
            std::wcerr << "FlushFileBuffers( std_output_handle = " << std_output_handle<<" ) "
                       << "GetLastError() = "<< lastError << std::endl;
          }
        }
        if( runtime_option.debug ){
          std::wcout << std::endl;
          std::wcout << "restore ConsoleOutputCP " << consoleOutputCP << std::endl;
        }
        SetConsoleOutputCP( consoleOutputCP );
        if( runtime_option.debug ){
          std::wcout << "done" << std::endl;
        }
      }
      break;
    case FILE_TYPE_DISK:
      break;
    case FILE_TYPE_PIPE:
      {
        exit_code_of_child_process = 
          create_command_process( standard_input , standard_output , standard_error ,
                                  runtime_option ,
                                  commandlineParam );
      }
      break;
    case FILE_TYPE_REMOTE:
      break;
    case FILE_TYPE_UNKNOWN:
      break;
    default:
      assert( !"GetFileType() return unkonwn" );
      break;
    }
  }
  return exit_code_of_child_process;
}

struct Win32FileHandleDeletor{
  HANDLE fileHandle;
  Win32FileHandleDeletor(const HANDLE& fileHandle ) noexcept 
    :fileHandle{ fileHandle }  
  {}
  ~Win32FileHandleDeletor()  noexcept
  {
    if( INVALID_HANDLE_VALUE != fileHandle ){
      VERIFY(::CloseHandle( fileHandle ) );
    }
  }
};

template<typename runtime_option_t>
static inline 
int io_wrap( HANDLE standard_input , HANDLE standard_output, 
             const runtime_option_t& runtime_option ) noexcept
{
  if( runtime_option.stderr_path.empty() ){
    return io_wrap( standard_input ,
                    standard_output ,
                    ::GetStdHandle( STD_ERROR_HANDLE ),
                    runtime_option );
  }

  HANDLE error{ INVALID_HANDLE_VALUE };
  
  if( runtime_option.stdout_path == runtime_option.stderr_path ){
    // 同じファイルを示しているので DuplicateHandle で同一のファイルハンドルを示す用にする
    HANDLE h{INVALID_HANDLE_VALUE};
    if( !DuplicateHandle( GetCurrentProcess() ,
                          standard_output ,
                          GetCurrentProcess() ,
                          &h ,
                          0,
                          TRUE ,
                          DUPLICATE_SAME_ACCESS ) ){
      const auto lastError{GetLastError()};
      std::ignore = lastError;
      
      assert( error == INVALID_HANDLE_VALUE );
      error = INVALID_HANDLE_VALUE;
    }else{
      if( runtime_option.debug ){
        std::wcout << "same file "
                   << "duplicate standard_output = " << standard_input
                   << ", duplicate_handle = " << h << std::endl;
      }
      error = h ;
    }
  }else{
    SECURITY_ATTRIBUTES security_attributes{};
    security_attributes.nLength = sizeof( SECURITY_ATTRIBUTES );
    security_attributes.bInheritHandle = TRUE;
    
    CREATEFILE2_EXTENDED_PARAMETERS extended_parameters{};
    extended_parameters.dwSize = sizeof( CREATEFILE2_EXTENDED_PARAMETERS );
    extended_parameters.lpSecurityAttributes = &security_attributes;
    
    error = ::CreateFile2( runtime_option.stderr_path.c_str() ,
                                  GENERIC_WRITE ,
                                  0 ,
                                  OPEN_EXISTING ,
                                  &extended_parameters );
  }

  if( INVALID_HANDLE_VALUE != error ){
    Win32FileHandleDeletor deletor{ error };
    return io_wrap( standard_input , standard_output , error, runtime_option );
  }
  return EXIT_FAILURE;
}

template<typename runtime_option_t>
int io_wrap( const HANDLE standard_input ,
             const runtime_option_t& runtime_option ) noexcept
{

  if( runtime_option.stdout_path.empty() ){
    return io_wrap( standard_input ,
                    ::GetStdHandle( STD_OUTPUT_HANDLE ),
                    runtime_option );
  }

  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof( SECURITY_ATTRIBUTES );
  security_attributes.bInheritHandle = TRUE;
  
  CREATEFILE2_EXTENDED_PARAMETERS extended_parameters{};
  extended_parameters.dwSize = sizeof( CREATEFILE2_EXTENDED_PARAMETERS );
  extended_parameters.lpSecurityAttributes = &security_attributes;

  HANDLE output = ::CreateFile2( runtime_option.stdout_path.c_str() ,
                                 GENERIC_WRITE ,
                                 0 ,
                                 OPEN_EXISTING ,
                                 &extended_parameters );
  if( INVALID_HANDLE_VALUE != output ){
    Win32FileHandleDeletor deletor{ output };
    return io_wrap( standard_input , output , runtime_option );
  }
  return EXIT_FAILURE;
}

template<typename runtime_option_t>
int io_wrap( const runtime_option_t& runtime_option ) noexcept
{
  if( runtime_option.stdin_path.empty() ){
    return io_wrap( ::GetStdHandle( STD_INPUT_HANDLE ) ,
                    runtime_option );
  }
  
  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof( SECURITY_ATTRIBUTES );
  security_attributes.bInheritHandle = TRUE;
  
  CREATEFILE2_EXTENDED_PARAMETERS extended_parameters{};
  extended_parameters.dwSize = sizeof( CREATEFILE2_EXTENDED_PARAMETERS );
  extended_parameters.lpSecurityAttributes = &security_attributes;
  
  HANDLE input = ::CreateFile2( runtime_option.stdin_path.c_str() ,
                                GENERIC_READ,
                                0,
                                OPEN_EXISTING ,
                                &extended_parameters );
  if( INVALID_HANDLE_VALUE == input ){
    std::wcout << GetLastError() << std::endl;
    return EXIT_FAILURE;
  }
  Win32FileHandleDeletor deletor{ input };

  return io_wrap( input , runtime_option );
}

static int
entry_point() noexcept
{
  
  struct optionW{
    const wchar_t short_option;
    const wchar_t* long_option;
    const wchar_t* descriotion;
    int option_require; 
  };
  
  struct RuntimeOption{
    bool debug;
    bool showHelp;
    bool listCodePages;
    bool interactive_shell;
    std::wstring input_codepage;
    std::wstring output_codepage;
    std::wstring stdin_path;
    std::wstring stdout_path;
    std::wstring stderr_path;
    std::vector<std::wstring> fileList{}; 
    
    RuntimeOption()
      : debug{ false },
        showHelp{false},
        listCodePages{ false }, 
        interactive_shell{ false },
        input_codepage{L"utf-8"},
        output_codepage{L"utf-8"},
        stdin_path{},
        stdout_path{},
        stderr_path{},
        fileList{}
    { }
    
    inline bool apply( const struct optionW& opt , const std::wstring& value ) noexcept
    {
#if 0 
      std::wcout << "RuntimeOption::apply(";
      if( opt.short_option ){
        std::wcout << "'" <<  opt.short_option << "'" ;
      }else{
        std::wcout << opt.long_option ;
      }
      if( opt.option_require ){
        std::wcout << "," ;
        std::wcout << "\"" << value << "\"" ;
      }
      std::wcout << ")" << std::endl;
#endif /* 0 */
      if( (!!opt.long_option) && std::wstring{L"debug"} == opt.long_option ){
        this->debug = true;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"help"} == opt.long_option ){
        this->showHelp = true;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"list-codepages"} == opt.long_option ){
        this->showHelp = true;
        this->listCodePages = true;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"interactive"} == opt.long_option ){
        this->interactive_shell = true;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"stdin"} == opt.long_option ){
        this->stdin_path = value;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"stdout"} == opt.long_option ){
        this->stdout_path = value;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"stderr"} == opt.long_option ){
        this->stderr_path = value;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"input-codepage" } == opt.long_option ){
        this->input_codepage = value;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"output-codepage" } == opt.long_option ){
        this->output_codepage = value;
        return true;
      }
      return false;
    }
  } runtimeOption{};

  {
    constexpr struct optionW opt[] = { {L'h'  , L"help"           , L"show help message"    } ,
                                       {L'd'  , L"debug"          , L"enable debug flag"    } ,
                                       {L'i'  , L"interactive"    , L"interactive shell"    } ,
                                       {L'I'  , L"interactive"    , L"interactive shell"    } ,
                                       {L'\0' , L"stdin"          , L"standard input"   , 1 } ,
                                       {L'\0' , L"stdout"         , L"standard output"  , 1 } ,
                                       {L'\0' , L"stderr"         , L"standard error"   , 1 } ,
                                       {L'\0' , L"list-codepages" , L"list codepage of system installed" , 0 }, 
                                       {L'\0' , L"input-codepage" , L"input code page"  , 1 } ,
                                       {L'\0' , L"output-codepage", L"output code page" , 1 } }; 

    std::vector<std::wstring> errors;
    if( wh::ParseCommandLineOption{}( runtimeOption.fileList , opt, runtimeOption , errors ) ){
      if( runtimeOption.debug ){
        std::wcout << "enable debug mode" << std::endl;
        std::wcout << " interactive: " << runtimeOption.interactive_shell << std::endl;
        std::wcout << " stdin:  "
                   << ( runtimeOption.stdin_path.empty() ?
                        std::wstring{L"(Console)"} : (L"\"" + runtimeOption.stdin_path + L"\"" ))
                   << ", input-codepage: " << runtimeOption.input_codepage
                   << std::endl;
        
        std::wcout << L" stdout: "
                   << (runtimeOption.stdout_path.empty() ?
                       std::wstring{L"(Console)"} : (L"\"" + runtimeOption.stdout_path + L"\"" ))
                   << L", output-codepage: " << runtimeOption.output_codepage 
                   << std::endl;

        std::wcout << L" stderr: "
                   << (runtimeOption.stderr_path.empty() ?
                       std::wstring{L"(Console)"} : (L"\"" + runtimeOption.stderr_path + L"\"" ))
                   << std::endl;
      }
      if( runtimeOption.debug ){
        

      }
      if( runtimeOption.showHelp ){
        std::wcout << "Help" << std::endl;
        if( runtimeOption.listCodePages ){

          EnumSystemCodePagesW( []( LPWSTR lpCodePageString )->BOOL{
            UINT codePage{ (UINT)std::stoi( lpCodePageString ) };
            
            std::unique_ptr< CPINFOEXW > cpinfoEx{ std::make_unique<CPINFOEXW>() };
            if( GetCPInfoEx( codePage , 0, cpinfoEx.get() ) ){
              std::wcout << cpinfoEx->CodePageName << std::endl;;
            }else{
              std::wcout << lpCodePageString << std::endl;
            }
            return TRUE;
          }, CP_INSTALLED);
          
        }
        return EXIT_SUCCESS;
      }
      return io_wrap( runtimeOption );
    }
  }
  return EXIT_SUCCESS;
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

int main(int,char*[])
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

  
  std::locale::global( std::locale{""} );

  return entry_point();
}
