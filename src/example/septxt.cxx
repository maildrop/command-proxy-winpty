#include <iostream>
#include <type_traits>

#include <tuple>
#include <array>
#include <memory>
#include <string>
#include <algorithm>
#include <iterator>
#include <cassert>

#include <tchar.h>
#include <windows.h>
#include <mlang.h>
#include <process.h>

#define VERIFY( exp ) (exp)

struct mlang_raii_t{
  HMODULE m_module;
  std::add_pointer<decltype( ConvertINetMultiByteToUnicode )>::type pConvertINetMultiByteToUnicode;
  std::add_pointer<decltype( ConvertINetUnicodeToMultiByte )>::type pConvertINetUnicodeToMultiByte;
  std::add_pointer<decltype( IsConvertINetStringAvailable  )>::type pIsConvertINetStringAvailable;
  mlang_raii_t()
    : m_module{ NULL },
      pConvertINetMultiByteToUnicode{ nullptr },
      pConvertINetUnicodeToMultiByte{ nullptr }
  {
    m_module = LoadLibraryEx( TEXT("mlang"), NULL , LOAD_LIBRARY_SEARCH_SYSTEM32 );
    if( m_module ){
      pIsConvertINetStringAvailable =
        reinterpret_cast<decltype(pIsConvertINetStringAvailable)>(GetProcAddress( m_module , "IsConvertINetStringAvailable" ));
      pConvertINetMultiByteToUnicode =
        reinterpret_cast<decltype(pConvertINetMultiByteToUnicode)>(GetProcAddress( m_module , "ConvertINetMultiByteToUnicode"));
      pConvertINetUnicodeToMultiByte =
        reinterpret_cast<decltype(pConvertINetUnicodeToMultiByte)>(GetProcAddress( m_module , "ConvertINetUnicodeToMultiByte" ));
    }
  }

  mlang_raii_t( const mlang_raii_t& ) = delete;
  mlang_raii_t( mlang_raii_t&& ) = delete;
  const mlang_raii_t&
  operator=( const mlang_raii_t& ) = delete;
                           
  const mlang_raii_t&
  operator=( mlang_raii_t&& ) = delete;
                           
  ~mlang_raii_t()
  {
    FreeLibrary( m_module );
  }

  inline 
  operator bool() const {
    return (m_module 
            && pIsConvertINetStringAvailable 
            && pConvertINetMultiByteToUnicode
            && pConvertINetUnicodeToMultiByte ) ? true : false;
  }

  inline 
  HRESULT 
  ConvertINetMultiByteToUnicode( LPDWORD lpdwMode,
                                 DWORD   dwSrcEncoding,
                                 LPCSTR  lpSrcStr,
                                 LPINT   lpnMultiCharCount,
                                 LPWSTR  lpDstStr,
                                 LPINT   lpnWideCharCount ) const
  {
    return pConvertINetMultiByteToUnicode( lpdwMode , dwSrcEncoding ,
                                           lpSrcStr , lpnMultiCharCount ,
                                           lpDstStr , lpnWideCharCount );
  }
  
  inline
  HRESULT
  ConvertINetUnicodeToMultiByte( LPDWORD lpdwMode,
                                 DWORD   dwEncoding,
                                 LPCWSTR lpSrcStr,
                                 LPINT   lpnWideCharCount,
                                 LPSTR   lpDstStr,
                                 LPINT   lpnMultiCharCount ) const
  {
    return pConvertINetUnicodeToMultiByte( lpdwMode ,dwEncoding ,
                                           lpSrcStr , lpnWideCharCount ,
                                           lpDstStr , lpnMultiCharCount );
  }

  inline
  HRESULT
  IsConvertINetStringAvailable( DWORD dwSrcEncoding,
                                DWORD dwDstEncoding ) const
  {

    return pIsConvertINetStringAvailable( dwSrcEncoding , dwDstEncoding );
  }
};

struct PumpHandle
{
  HANDLE in;
  DWORD codepage_in;
  HANDLE out;
  DWORD codepage_out;
};

HANDLE pump_handle( PumpHandle& pumpHandle )
{
  HANDLE ready = CreateEvent( nullptr , TRUE , FALSE , nullptr);
  if( !ready ){
    return 0;
  }

  uintptr_t thread_handle = 0;
  {
    std::tuple< decltype( pumpHandle ) , HANDLE> thread_argument{ pumpHandle , ready };
    
    thread_handle = 
      _beginthreadex ( nullptr , 0 ,
                       [](void * ptr)->unsigned{
                         struct mlang_raii_t mlang_raii{};
                         if( !static_cast<bool>(mlang_raii) ){
                           return EXIT_FAILURE;
                         }else{
                           auto const arg = *(reinterpret_cast<decltype( &thread_argument )>( ptr ));
                           const struct PumpHandle pumpHandle = std::get<0>( arg );
                           VERIFY( SetEvent( std::get<1>( arg ) ) );
                           
                           DWORD in_mode = 0;
                           //DWORD out_mode = 0;
                           std::unique_ptr< std::array<BYTE,512> > in_buffer{ new std::array<BYTE,512>() };
                           decltype(in_buffer)::element_type::size_type offset = 0;
                           
                           for(;;){
                             DWORD read_size = 0;
                             if( ! ReadFile(pumpHandle.in,
                                            in_buffer->data() + offset ,
                                            std::tuple_size<decltype(in_buffer)::element_type>::value - DWORD(offset),
                                            &read_size , nullptr )){
                               break;
                             }
                             int multiCharCount = int(offset + read_size);
                             int widechar_require = 0;
                             HRESULT hr = mlang_raii.ConvertINetMultiByteToUnicode( &in_mode ,
                                                                                    pumpHandle.codepage_in,
                                                                                    (LPCSTR)in_buffer->data() ,
                                                                                    &multiCharCount ,
                                                                                    nullptr,
                                                                                    &widechar_require);
                             if( S_OK == hr ){
                               std::unique_ptr<wchar_t[]> widechar_buffer{ new wchar_t[ widechar_require ] };
                               if( widechar_buffer ){
                                 int widechar_count = widechar_require;
                                 hr = mlang_raii.ConvertINetMultiByteToUnicode( &in_mode ,
                                                                                pumpHandle.codepage_in,
                                                                                (LPCSTR)in_buffer->data() ,
                                                                                &multiCharCount ,
                                                                                widechar_buffer.get(), 
                                                                                &widechar_count);
                                 if( S_OK == hr ){
                                   std::rotate( std::begin( *in_buffer ) ,
                                                std::next( std::begin( *in_buffer ), multiCharCount ),
                                                std::next( std::begin( *in_buffer ), offset + read_size ) );
                                   offset = (read_size + offset ) - multiCharCount;
                                 }
                               }
                             }
                           }
                           return EXIT_SUCCESS;
                         }
                         return EXIT_FAILURE;
                       },
                       reinterpret_cast<void*>( &thread_argument ),
                       0 ,
                       nullptr );
    if( thread_handle ){
      HANDLE handles[2] = { ready , reinterpret_cast<HANDLE>( thread_handle ) };
      for(;;){
        DWORD waitValue = WaitForMultipleObjects( sizeof( handles ) / sizeof( handles[0] ) , handles , FALSE , INFINITE );
        if( WAIT_OBJECT_0 == waitValue){
        }else if( (WAIT_OBJECT_0 + 1) == waitValue ){
          VERIFY( CloseHandle( reinterpret_cast<HANDLE>( thread_handle ) ) );
          thread_handle = 0;
        }else if( (WAIT_TIMEOUT) == waitValue ){
          continue;
        }
        break;
      }
    }
    // dispose object thread_handle 
  }
  VERIFY( CloseHandle( ready ) );
  return reinterpret_cast<HANDLE>(thread_handle);
}

int main(int,char*[])
{
  std::locale::global( std::locale(""));

  std::add_pointer<decltype( ConvertINetMultiByteToUnicode )>::type pConvertINetMultiByteToUnicode = nullptr;

  HMODULE const mlang = LoadLibraryEx( TEXT("mlang"), NULL , LOAD_LIBRARY_SEARCH_SYSTEM32 );

  if( mlang ){
    pConvertINetMultiByteToUnicode = 
      reinterpret_cast<std::add_pointer<decltype(ConvertINetMultiByteToUnicode)>::type>(GetProcAddress( mlang , "ConvertINetMultiByteToUnicode"));
    if( !pConvertINetMultiByteToUnicode ){
      std::cerr << "ConvertINetMultiByteToUnicode" << std::endl;
      return 1;
    }
  
    HANDLE input = GetStdHandle( STD_INPUT_HANDLE );
    {
      DWORD mode = 0;
      std::unique_ptr< std::array<BYTE,512> > in_buffer{ new std::array<BYTE,512>() };
      static_assert( std::tuple_size<decltype(in_buffer)::element_type>::value == 512 );
    
      decltype(in_buffer)::element_type::size_type offset = 0;

      const DWORD codepage = GetACP();
      for(;;){
        DWORD read_size = 0;
        if( ! ReadFile(input,
                       in_buffer->data() + offset ,
                       std::tuple_size<decltype(in_buffer)::element_type>::value - DWORD(offset),
                       &read_size , nullptr )){
          break;
        }
        INT multiCharCount = INT(offset + read_size);
        
        int requireWideCharCount = 0;
        {
          int afterMultiCharCount = multiCharCount;
          HRESULT hr = pConvertINetMultiByteToUnicode( &mode ,
                                                             codepage ,
                                                             (LPCSTR)in_buffer->data(),
                                                             &afterMultiCharCount ,
                                                             nullptr ,
                                                             &requireWideCharCount );
          assert( multiCharCount == INT(offset+read_size) );
          switch( hr ){
          case S_OK:
            {
              std::unique_ptr<wchar_t[]> out_buffer{ new wchar_t[requireWideCharCount] };
              int wideCharCount = requireWideCharCount;
              hr = pConvertINetMultiByteToUnicode( &mode ,
                                                   codepage ,
                                                   (LPCSTR)in_buffer->data() ,
                                                   &multiCharCount ,
                                                   out_buffer.get(),
                                                   &wideCharCount ); 
              switch( hr ){
              case S_OK:
                {
                  std::rotate( std::begin( *in_buffer )  ,
                               std::next( std::begin( *in_buffer ), multiCharCount ) ,
                               std::next( std::begin( *in_buffer ), offset + read_size ));
                  offset = (read_size + offset) - multiCharCount ;
                  std::wcout << std::wstring{ out_buffer.get() ,(size_t) wideCharCount } ;
                  std::wcout << std::flush;
                }
                break;
              case S_FALSE:
                std::wcout << "S_FALSE" << std::endl;
                break;
              case E_FAIL:
                std::wcout << "E_FAIL" << std::endl;
                break;
              default:
                std::wcout << hr << std::endl;
                break;
              }
            }
            break;
          case S_FALSE:
            std::wcout << "S_FALSE" << std::endl;
            break;
          case E_FAIL:
            std::wcout << "E_FAIL" << std::endl;
            break;
          default:
            break;
          }
        }
      }
    }
    FreeLibrary( mlang );
  }
  return 0;
}
