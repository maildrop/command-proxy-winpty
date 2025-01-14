#include <iostream>

#include <tchar.h>
#include <windows.h>
#include <wrl.h>
#include <Mlang.h>

struct iconv_context;
extern "C"{
  typedef struct iconv_context* iconv_t;
  
  iconv_t
  iconv_open(const char*tocode , const char*fromcode);
  int
  iconv_close(iconv_t cd);
  size_t
  iconv(iconv_t cd ,
        char **inbuf  , size_t inbytesleft,
        char **outbuf , size_t outbytesleft );
};

struct iconv_context{
  Microsoft::WRL::ComPtr<IMultiLanguage3> imlang3;
  DWORD mode;
  UINT  toCode;   // codepage
  UINT  fromCode; // codepage
};

iconv_t
iconv_open( const char *tocode , const char *fromcode )
{
  iconv_context* context = new iconv_context();
  context->mode = 0;
  context->toCode = DWORD(uintptr_t(tocode));
  context->fromCode = DWORD(uintptr_t(fromcode));
  if( S_OK != CoCreateInstance( CLSID_CMultiLanguage , nullptr , CLSCTX_INPROC_SERVER , IID_PPV_ARGS( context->imlang3.GetAddressOf() )) ){
    delete context;
    return (iconv_t)(-1);
  }
  return context;
}

int
iconv_close( iconv_t cd )
{
  delete cd;
  return 0;
}

size_t
iconv( iconv_t cd ,
       char **inbuf  , size_t *inbytesleft,
       char **outbuf , size_t *outbytesleft )
{
  iconv_context* context = cd;

  if( nullptr == inbuf && nullptr == inbytesleft ){
    context->mode =0;
    return 0;
  }

  
  static_assert( sizeof( BYTE ) == sizeof( char ) );
  UINT inbytesleft_u = UINT(*inbytesleft);
  UINT outbytesleft_u = UINT(*outbytesleft);
  
  context->imlang3->ConvertString( &(context->mode ),
                                   context->fromCode ,
                                   context->toCode ,
                                   reinterpret_cast<BYTE*>(*inbuf) , &inbytesleft_u ,
                                   reinterpret_cast<BYTE*>(*outbuf) , &outbytesleft_u );
  *inbytesleft = size_t( inbytesleft_u );
  *outbytesleft = size_t( outbytesleft_u );
  
  return 0; // temporary
}
