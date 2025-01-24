#include <tchar.h>
#include <windows.h>

#include <iostream>
#include <locale>
#include <string>
#include <vector>
#include <tuple>

#include "wh-parse-option.h"

template<typename runtime_option_t>
static inline 
int io_wrap( HANDLE standard_input , HANDLE standard_output , HANDLE standard_error,
             const runtime_option_t& runtime_option ) noexcept
{
  std::wcout << standard_input << "," << standard_output << "," << standard_error << std::endl;
  std::ignore = runtime_option;
  return EXIT_SUCCESS;
}

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
  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof( SECURITY_ATTRIBUTES );
  security_attributes.bInheritHandle = TRUE;
  
  CREATEFILE2_EXTENDED_PARAMETERS extended_parameters{};
  extended_parameters.dwSize = sizeof( CREATEFILE2_EXTENDED_PARAMETERS );
  extended_parameters.lpSecurityAttributes = &security_attributes;

  HANDLE error = ::CreateFile2( runtime_option.stderr_path.c_str() ,
                                GENERIC_WRITE ,
                                0 ,
                                OPEN_EXISTING ,
                                &extended_parameters );
  
  if( INVALID_HANDLE_VALUE != error ){
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
    std::wstring stdin_path;
    std::wstring stdout_path;
    std::wstring stderr_path;
    std::vector<std::wstring> fileList{}; 
    RuntimeOption()
      : showHelp{false},
        debug{ false },
        stdin_path{},
        stdout_path{},
        stderr_path{},
        fileList{}
    {
    }
    
    inline bool apply( const struct optionW& opt , const std::wstring& value ) noexcept
    {
      if( (!!opt.long_option) && std::wstring{L"help"} == opt.long_option ){
        this->showHelp = true;
        return true;
      }else if( (!!opt.long_option) && std::wstring{L"debug"} == opt.long_option ){
        this->debug = true;
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
      
      std::wcout << "RuntimeOption::apply( ";
      if( opt.short_option ){
        std::wcout << opt.short_option << ", ";
      }else{
        std::wcout << opt.long_option << ", ";
      }
      std::wcout << "\"" << value << "\" )" << std::endl;
      return false;
    }
  } runtimeOption{};

  {
    constexpr struct optionW opt[] = { {L'h'  , L"help"   , L"show help message"   } ,
                                       {L'd'  , L"debug"  , L"enable debug flag"   } ,
                                       {L'\0' , L"stdin"  , L"standard input"   , 1} ,
                                       {L'\0' , L"stdout" , L"standard output"  , 1} ,
                                       {L'\0' , L"stderr" , L"standard error"   , 1}};
    
    std::vector<std::wstring> errors;
    if( wh::ParseCommandLineOption{}( runtimeOption.fileList , opt, runtimeOption , errors ) ){
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
