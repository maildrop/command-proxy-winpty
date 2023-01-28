#include <tchar.h>
#include <windows.h>
#include <iostream>
#include <ios>
#include <sstream>
#include <string>
#include <type_traits>
#include <iterator>
#include <utility>
#include <locale>
#include <cassert>

#define ESC (0x1b)
#define BEL (0x07)

int main(int,char*[])
{
  std::locale::global( std::locale("") );
  std::wstringstream buf{};
  buf << wchar_t( ESC ) ;

  do{
    wchar_t w;
    while( buf.get( w ).good() ){
      std::wcout << w ;
      // .... 
    }
    std::wcout << std::endl;
    buf.clear();
    std::wstringstream{}.swap( buf );
    std::copy( std::istream_iterator<wchar_t,wchar_t>{ (std::wcin >> std::noskipws) } ,
               std::istream_iterator<wchar_t,wchar_t>{} ,
               std::ostream_iterator<wchar_t,wchar_t>( buf ) );
  }while( 0 < buf.str().size() || std::wcin.good() );
  
  return EXIT_SUCCESS;
}
