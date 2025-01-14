#include <tchar.h>
#include <windows.h>
#include <processenv.h>

#include <iostream>
#include <locale>
#include <string>
#include <vector>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <cassert>

#ifndef VERIFY
# ifdef NDEBUG 
#  define VERIFY(expression) do{ ( expression ); }while( 0 )
# else /* NDEBUG */
#  define VERIFY(expression) assert( expression )
# endif /* NDEBUG */
#endif  /* VERIFY*/

std::wstring
comspec() noexcept
{
  size_t const required_length =
    size_t{ GetEnvironmentVariableW( L"COMSPEC" , nullptr , 0 )};

  if( ! required_length ){
    const DWORD lastError = GetLastError();
    if ( ERROR_ENVVAR_NOT_FOUND == lastError ) {
      return std::wstring{};
    }
  }
  
  if( 0 < required_length ){
    std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>( required_length );
    GetEnvironmentVariable( L"COMSPEC" , buffer.get() , DWORD(required_length) );
    return std::wstring{ buffer.get() };
  }
  return std::wstring{};
}

std::vector<std::wstring>&
setupEnvList( std::vector<std::wstring>& envList ) noexcept
{
  wchar_t* envBlock = GetEnvironmentStringsW();
  if( envBlock ){
    for( wchar_t *env = envBlock ;
         *env != L'\0' ;
         env += std::char_traits<wchar_t>::length( env ) + 1) {
      envList.emplace_back( env );
    }
    VERIFY( FreeEnvironmentStringsW ( envBlock ) );
  }
  return envList;
}

std::vector<std::wstring>&
replaceEnvListElement( std::vector<std::wstring>& envList ,
                       const wchar_t* key ,
                       const wchar_t* value ) noexcept
{
  assert( key != nullptr );
  if( key == nullptr ){
    return envList;
  }
  
  envList.erase( std::remove_if( std::begin( envList ) ,
                                 std::end( envList ) ,
                                 std::bind( []( const std::wstring &val , const wchar_t* param ) ->int {
                                   auto const pos = val.find_first_of( L'=' );
                                   return ( (std::wstring::npos == pos ) ? val : val.substr( 0, pos ) ) == param ;
                                 } , std::placeholders::_1 , key ) ), 
                 std::end( envList ));
  if( value != nullptr && std::wstring{L""} != value ){
    envList.push_back( (std::wstring{ key } + L"=" + std::wstring{ value }) );
  }

  std::sort( std::begin( envList ) , std::end( envList ) ,
             []( const std::wstring& l , const std::wstring& r ) -> int {
               auto const lpos = l.find_first_of( L'=' );
               auto const rpos = r.find_first_of( L'=' );
               return
                 (std::wstring::npos == lpos ? l : l.substr(0, lpos)) < (std::wstring::npos == rpos ? r : r.substr(0, rpos));
             } );
  return envList;
}

std::unique_ptr<wchar_t[]> 
buildEnvBlock( std::vector<std::wstring>& envList ) noexcept
{
  size_t n = 1;
  std::for_each( std::begin( envList ), std::end( envList ),
                 [&n](const std::wstring& l)-> void {
                   n+=(l.length()+1);
                 });
  std::unique_ptr<wchar_t[]> envBlock = std::make_unique<wchar_t[]>(n) ;
  {
    auto ite = envBlock.get();
    std::for_each( std::begin( envList ), std::end( envList ),
                   [&ite]( const std::wstring& l)-> void {
                     ite = std::copy( std::begin( l ) , std::end( l ),
                                      ite );
                     *ite++ = L'\0';
                   });
    *ite++ = L'\0';
    assert( ( envBlock.get() + n ) == ite &&  "check end of memory block");
  }
  return envBlock;
}

int
doProcess() noexcept
{
  std::wcout << L"\033[31m" << "Hello world with Termanl Color " << L"\033[0m" << std::endl;
  std::wcout << comspec() << std::endl;
  std::wstring com =
    std::wstring{L"\""} + comspec() + std::wstring{L"\""} ;
  
  std::unique_ptr< wchar_t[] > commandLine = std::make_unique<wchar_t[]>( com.size() + 1  );
  std::char_traits<wchar_t>::copy( commandLine.get() , com.c_str() , com.length() );


  STARTUPINFO startupInfo{};
  startupInfo.cb = sizeof( startupInfo );
  PROCESS_INFORMATION pi{};
  
  std::unique_ptr< wchar_t[] > enviroment{};
  {
    std::vector<std::wstring> envList{};
    setupEnvList( envList );
    replaceEnvListElement( envList, L"LANG" , L"ja_JP.utf-8" );
    enviroment = buildEnvBlock( envList ) ;
    
    if( !CreateProcessW( nullptr ,
                         commandLine.get(),
                         nullptr,
                         nullptr,
                         FALSE,
                         NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT,
                         ( static_cast<bool>( enviroment ) ? enviroment.get() : nullptr ),
                         nullptr ,
                         &startupInfo ,
                         &pi ) ){
      return EXIT_FAILURE;
    }
  }

  VERIFY( ::CloseHandle( pi.hThread ) );
  WaitForSingleObject( pi.hProcess , INFINITE );
  VERIFY( ::CloseHandle( pi.hProcess ) );

  std::wcout << "Goodbye World"  << std::endl;

  return EXIT_SUCCESS;
}

int main(int, char*[])
{
  std::locale::global( std::locale{"", std::locale::ctype } );

  HANDLE std_output_handle = GetStdHandle( STD_OUTPUT_HANDLE );

  if( INVALID_HANDLE_VALUE != std_output_handle ){
    // std::wcout << "STD_OUTPUT_HANDLE " << std_output_handle << " " ; 
    DWORD const fileType = GetFileType( std_output_handle );
    switch( fileType ){
    case FILE_TYPE_CHAR:
      // 指定されたファイルは、文字ファイルであり通常は LPTデバイスかコンソールである
      //std::wcout << "FILE_TYPE_CHAR" << std::endl;
      do{
        DWORD consoleMode;
        if( ! GetConsoleMode( std_output_handle , &consoleMode ) ){
          break;
        }
        if( ! SetConsoleMode( std_output_handle , consoleMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING  ) ){
          break;
        }
        UINT consoleOutputCP = GetConsoleOutputCP();
        std::ignore = consoleOutputCP;

        doProcess();

        std::ignore = SetConsoleMode( std_output_handle , consoleMode );
      }while( false );
      return EXIT_SUCCESS;

    case FILE_TYPE_PIPE:
      // 指定されたファイルは、ソケット 名前付きパイプ または匿名パイプである
      //std::wcout << "FILE_TYPE_PIPE" << std::endl;
      return doProcess();
    default:
      // それ以外
      std::wcout << fileType << std::endl;
      break;
    }
  }

  
  return EXIT_SUCCESS;
}
