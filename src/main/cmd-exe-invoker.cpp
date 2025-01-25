#include <tchar.h>
#include <windows.h>

#include <iostream>
#include <locale>
#include <memory>
#include <string>
#include <vector>
#include <tuple>

#include "wh-parse-option.h"

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
int io_wrap( HANDLE standard_input , HANDLE standard_output , HANDLE standard_error,
             const runtime_option_t& runtime_option ) noexcept
{
  if( runtime_option.debug ){
    std::wcout << standard_input << "," << standard_output << "," << standard_error << std::endl;
  }

  std::wstring commandline{
    wh::CommandArgumentsBuilder{ application::environment::comspec() }
    .build()
  };

  std::unique_ptr<wchar_t[]> commandlineParam{ std::make_unique<wchar_t[]>(commandline.size()+1) };

  std::ignore =
    std::char_traits<wchar_t>::copy( commandlineParam.get() , commandline.c_str() , commandline.size() + 1 );

  STARTUPINFOW startupInfo{};
  startupInfo.cb = sizeof( startupInfo );
  
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdInput = standard_input ;
  startupInfo.hStdOutput = standard_output;
  startupInfo.hStdError = standard_error;
  
  PROCESS_INFORMATION process_information{};
  
  CreateProcessW( nullptr,
                  commandlineParam.get(),
                  nullptr ,
                  nullptr ,
                  TRUE,
                  NORMAL_PRIORITY_CLASS , 
                  nullptr ,
                  nullptr ,
                  &startupInfo ,
                  &process_information );

  WaitForSingleObject( process_information.hProcess, INFINITE );

  VERIFY( ::CloseHandle( process_information.hProcess ) );
  VERIFY( ::CloseHandle( process_information.hThread ) );

  return EXIT_SUCCESS;
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
      
      error = INVALID_HANDLE_VALUE;
    }else{
      std::wcout << "same file "
                 << "duplicate standard_input = " << standard_input
                 << ", duplicate_handle = " << h << std::endl;
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
  // std::wcout << "CreateFile2( " << runtime_option.stdin_path << ") = " << input << std::endl;
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
    bool showHelp;
    bool debug;
    bool interactive_shell;
    std::wstring stdin_path;
    std::wstring stdout_path;
    std::wstring stderr_path;
    std::vector<std::wstring> fileList{}; 

    RuntimeOption()
      : showHelp{false},
        debug{ false },
        interactive_shell{ false },
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
      
      if( (!!opt.long_option) && std::wstring{L"help"} == opt.long_option ){
        this->showHelp = true;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"debug"} == opt.long_option ){
        this->debug = true;
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
      }
      
      return false;
    }
  } runtimeOption{};

  {
    constexpr struct optionW opt[] = { {L'h'  , L"help"   , L"show help message"   } ,
                                       {L'd'  , L"debug"  , L"enable debug flag"   } ,
                                       {L'i'  , L"interactive" , L"interactive shell" },
                                       {L'I'  , L"interactive" , L"interactive shell" },
                                       {L'\0' , L"stdin"  , L"standard input"   , 1} ,
                                       {L'\0' , L"stdout" , L"standard output"  , 1} ,
                                       {L'\0' , L"stderr" , L"standard error"   , 1}};
    
    std::vector<std::wstring> errors;
    if( wh::ParseCommandLineOption{}( runtimeOption.fileList , opt, runtimeOption , errors ) ){
      if( runtimeOption.showHelp ){
        std::wcout << "Help" << std::endl;
        return EXIT_SUCCESS;
      }
      return io_wrap( runtimeOption );
    }
  }
  return EXIT_SUCCESS;
}

int main(int,char*[])
{
  std::locale::global( std::locale{""} );
  return entry_point();
}
