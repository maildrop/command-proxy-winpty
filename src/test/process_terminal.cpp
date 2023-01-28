#include <memory>
#include <array>
#include <tuple>
#include <tchar.h>
#include <windows.h>

SHORT
WINAPI process( HANDLE in, HANDLE out ){
  std::ignore = out;

  std::unique_ptr<std::array<char,4096>> buffer{new std::array<char,4096>{} };
  {
    size_t offset = 0;
    for(;;){
      DWORD read_size;
      BOOL read_value = ReadFile( in , buffer->data() + offset , DWORD( std::size( *buffer ) - offset) , &read_size , nullptr);
      if( read_value ){
        size_t end = offset+read_size;
      }else{
        break;
      }
    }
  }
  return EXIT_SUCCESS;
}
