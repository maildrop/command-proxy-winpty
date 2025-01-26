#if defined( _MSC_VER )
# pragma once
#endif defined( _MSC_VER ) 

#if !defined( WH_ERROR_HANDLING_H_HEADER_GUARD )
#define WH_ERROR_HANDLING_H_HEADER_GUARD 1
/**
   

   
 */
#if defined( __cplusplus )

#include <tchar.h>
#include <windows.h>
#include <memory>
#include <tuple>

namespace wh {

  // application 定義のエラーは 29 ビット目が立つ
  constexpr DWORD WINAPI_ERROR_APPLICATION_DOMAIN_BEGIN{ (0x1 << (29-1) ) }; 
  enum{
    ERROR_APP_BEGIN = WINAPI_ERROR_APPLICATION_DOMAIN_BEGIN,
  };
  
  constexpr inline bool
  isApplicationDomainError( const DWORD& errorCode ) noexcept
  {
    // operator & の使用時は、 signed / unsigned に注意する 
    static_assert( std::is_unsigned< DWORD >::value , "DWORD is unsinged" );
    return ( WINAPI_ERROR_APPLICATION_DOMAIN_BEGIN & errorCode ) ? true : false;
  }

  inline std::wstring
  formatMessage( const DWORD& errorCode ) noexcept
  {
    if( isApplicationDomainError( errorCode ) ){

    }else{
      LPVOID lpMsgBuf{nullptr};
      DWORD countOfCharacterLength{};
      if( (countOfCharacterLength =
           ::FormatMessageW( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                             FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS ,
                             NULL ,
                             errorCode ,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             (LPTSTR) &lpMsgBuf,
                             0 , NULL ) )){
        struct LocalFreeDeletor{
          inline void operator()( wchar_t* ptr) noexcept
          {
            VERIFY( NULL == LocalFree( ptr ) );
            return;
          }
        };
        std::unique_ptr< wchar_t , LocalFreeDeletor > message_buffer{ reinterpret_cast<wchar_t*>(lpMsgBuf) } ;
        return std::wstring{ message_buffer.get() , size_t{countOfCharacterLength} };
      }
    }
    return std::wstring{};
  }
};
#endif /* defined( __cplusplus ) */

#endif /* !defined( WH_ERROR_HANDLING_H_HEADER_GUARD ) */
