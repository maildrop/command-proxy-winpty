#if defined( _MSC_VER )
# pragma once
#endif defined( _MSC_VER ) 

#if !defined( WH_PARSE_OPTION_H_HEADER_GUARD )
#define WH_PARSE_OPTION_H_HEADER_GUARD 1
/**
   wh-parse-option.h -- a part of Windows Helper library.
   コマンドラインのオプションパーザーと、コマンドラインの組み立てのためのビルダー
 */

#if defined( WIN32 )

# include <shellapi.h>
// CommandLineToArgvW 
# if defined( _MSC_VER )
#  pragma comment( lib ,"shell32.lib" )
# endif /* defined( _MSC_VER ) */

#else

# error not Win32

#endif /* !defined( WIN32 ) */

#if defined( __cplusplus )
# include <cassert>
#else /* defined( __cplusplus )*/
# include <assert.h>
#endif /* defined( __cplusplus ) */

#if !defined( VERIFY )
# if defined( NDEBUG )
#  define VERIFY( exp ) (void)(exp)
# else 
#  define VERIFY( exp ) assert( (exp) )
# endif /* defined( NDEBUG ) */
#endif /* !defined( VERIFY ) */

#if defined( __cplusplus )

#include <vector>
#include <tuple>
#include <string>
#include <sstream>
#include <cctype>

namespace wh {
  /**
     コマンドラインオプションのパーザー Win32 用

     RuntimeOption に用意された applyメソッドをコールする形でパースする。
     パースの順番は、左から順番。
     runtime_config_t::apply で適用していく。
   
     以下の optionW と RuntimeOption の「実装」は仕様の外。テンプレートで使用する型なので、順番や追加要素は、実装側で定義の事。
   
     example)
   
     std::vector<std::wstring> fileList{}; // コマンドラインオプションの残りのファイルリスト
     
     struct optionW{
     const wchar_t short_option; // 必須
     const wchar_t* long_option; // 必須
     const wchar_t* descriotion;
     int option_require; // 必須
     };

     struct RuntimeOption{
     bool showHelp;
     inline bool apply( const struct optionW& opt , const std::wstring& value ) noexcept
     {
     if( (!!opt.long_option) && std::wstring{L"help"} == opt.long_option ){
     this->showHelp = true;
     }else{
     std::wcout << "RuntimeOption::apply( ";
     if( opt.short_option ){
     std::wcout << opt.short_option << ", ";
     }else{
     std::wcout << opt.long_option << ", ";
     }
     std::wcout << "\"" << value << "\" )" << std::endl;
     }
     return true;
     }
     } runtimeOption{};
  
     {
     constexpr struct optionW opt[] = { {L'h'  , L"help"   , L"show help message"   } ,
     {L'd'  , L"debug"  , L"enable debug flag"   } ,
     {L'v'  , L"verbose", L"enable verbose flag" } , 
     {L'\0' , L"stdin"  , L"standard input"   , 1} ,
     {L'\0' , L"stdout" , L"standard output"  , 1} ,
     {L'\0' , L"stderr" , L"standard error"   , 1}};
    
     std::vector<std::wstring> errors;
     if( ParseCommandLineOption{}( fileList , opt, runtimeOption , errors ) ){
      
     }
     }

  */
  struct ParseCommandLineOption{

    enum{
      OPTION_ARGUMENT_NONE = 0,
      OPTION_ARGUMENT_REQUIRE = 1,
      OPTION_ARGUMENT_OPTIONAL = 2
    };
  
    template<typename option_t , size_t n , typename runtime_config_t>
    inline bool
    operator()( std::vector<std::wstring> &fileList ,
                const option_t (&opt)[n] ,
                runtime_config_t& config ,
                std::vector<std::wstring> *errors = nullptr ) const noexcept 
    {
      if ( errors ) {
        return (*this)( fileList , opt , config , *errors );
      }
      std::vector<std::wstring> err;
      return (*this)( fileList , opt , config , err );
    }

    template<typename option_t , size_t n , typename runtime_config_t>
    inline bool
    operator()( std::vector<std::wstring> &fileList ,
                const option_t (&opt)[n] ,
                runtime_config_t& config ,
                std::vector<std::wstring> &errorMessages ) const noexcept
    {
      // 引数の数
      int nArgs{};

      // ::LocalFree で削除を行う unique_ptr 用の deletor 
      struct LocalFreeDeletor{
        inline void
        operator()( LPWSTR* ptr ) const noexcept {
          VERIFY( NULL == ::LocalFree( ptr ) );
          return;
        }
      };

      std::unique_ptr<LPWSTR,LocalFreeDeletor> szArgList{ ::CommandLineToArgvW( ::GetCommandLineW() , &nArgs ) };

      if ( static_cast<bool>( szArgList ) ) {
        return (*this)( nArgs , szArgList.get() ,
                        fileList, opt , config ,
                        errorMessages );
      }

      return false;
    }

    /**
       @param fileList 戻り値 オプション以外のファイルのリスト
       @param config config.apply() でオプションの適用を行う
       @param errorMessages エラーがあった場合には、メッセージが入る
       @return エラーがあった場合には false そうでない場合は true 
    */
    template<typename option_t, size_t n , typename runtime_config_t >
    inline bool
    operator()( const size_t nArgs ,
                LPWSTR *szArgList ,
                std::vector<std::wstring> &fileList ,
                const option_t (&opt)[n] ,
                runtime_config_t& config ,
                std::vector<std::wstring> &errorMessages ) const noexcept
    {
      assert( szArgList );
      for( int i = 1 /* 最初は、プログラム名 */ ; i < nArgs ; ++i ){ 
        std::wistringstream wis{ szArgList[i] };
      
        auto isOption = []( const std::wstring& token )->bool{
          return ( (L'-' == token.at(0) ) && 1 < token.length() ) ?
            true : false;
        };
      
        auto peek = [&i,&nArgs,&szArgList](){
          return std::wstring{(i+1 < nArgs) ? szArgList[i+1] : L""};
        };
      
        auto next = [&i,&nArgs,&szArgList](){
          return std::wstring{(i+1 < nArgs) ? szArgList[++i] : L""};
        };
      
        wchar_t c = wis.get();

        if( wis.good() ){
          if ( L'-' == c ) {
            c = wis.get();
            if( !wis.good() ){
              c = L'-';
            } else {
              
              if( L'-' == c ){
                // long option;
                std::wstring option_token{};
                while( c = wis.get() , wis.good() ){
                  if( L'=' == c ){
                    break;
                  }else{
                    option_token.push_back(c);
                  }
                }
              
                auto ite =
                  std::find_if( std::begin( opt ), std::end( opt ),
                                [&option_token]( auto &p )->bool{
                                  return (!!p.long_option) && option_token == p.long_option ;
                                });
              
                if( std::end( opt ) == ite ){
                  // ERROR
                  errorMessages.push_back( std::wstring{L"unknown long option: "} + L"--" + option_token );
                }else{
                  if( ite->option_require ){
                    assert( ite->option_require != 0 || !"ite->option_require is not OPTION_ARGUMENT_NONE(=0)" );
                    if( wis.good() ){
                      std::wstring option_arg{};
                      while( c = wis.get() , wis.good() ){
                        option_arg.push_back( c );
                      }
                      config.apply( *ite , option_arg );
                    }else{
                      assert( 0 != ite->option_require );
                    
                      switch( static_cast<int>(ite->option_require) ){
                      case OPTION_ARGUMENT_REQUIRE:
                        {
                          if( !isOption( peek() ) ){
                            config.apply( *ite , next() );
                          }else{
                            // error
                            errorMessages.push_back( std::wstring{ L"option require argument: " } + L"--" + option_token );
                          }
                        }
                        break;
                      case OPTION_ARGUMENT_OPTIONAL:
                        {
                          if( !isOption( peek() ) ){
                            config.apply( *ite , next() );
                          }else{
                            config.apply( *ite , std::wstring{L""} );
                          }
                        }
                        break;
                      default:
                        assert( !"program error: option_require is unknown value" );
                        abort();
                        break;
                      }
                    
                    }
                  }else{
                    assert( ite->option_require == 0 || !"ite->option_require is OPTION_ARGUMENT_NONE" );
                    config.apply( *ite , std::wstring{L""} );
                  }
                }
                continue;
              }else{
                wis.unget();
                // short option;
                while( c = wis.get(), wis.good() ){
                  auto ite =
                    std::find_if( std::begin( opt ) , std::end( opt ) ,
                                  [&c]( auto &p )->bool{
                                    if( L'\0' == p.short_option ){
                                      return false;
                                    }
                                    assert( L'\0' != c || !"c is not terminate caractor" );
                                    return c == p.short_option;
                                  });
                  if( std::end(opt) != ite ){
                    switch( static_cast<int>(ite->option_require) ){
                    case OPTION_ARGUMENT_NONE:
                      config.apply( *ite , std::wstring{L""} );
                      break;
                    case OPTION_ARGUMENT_REQUIRE:
                      if( !isOption( peek() ) ){
                        config.apply( *ite , next() );
                      }else{
                        errorMessages.push_back( std::wstring{ L"option require argument: " } + L"-" + c );
                      }
                      break;
                    case OPTION_ARGUMENT_OPTIONAL:
                      {
                        if( !isOption( peek() ) ){
                          config.apply( *ite , next() );
                        }else{
                          config.apply( *ite , std::wstring{L""} );
                        }
                      }
                      break;
                    default:
                      assert( !"program error: option_require is unknown value" );
                      abort();
                      break;
                    }
                  }else{
                    // ERROR
                    errorMessages.push_back( std::wstring{L"unknown short option: "} + L"-" + c );
                  }
                }
                continue;
              }
            }
          }
          
          fileList.push_back( wis.str() );

        }
      }
      return errorMessages.empty();
    }
  }; // end of ParseCommandLineOption 

  /**
     
     bug: 引数の中に " ダブルクォートがある時に正しく作成されない。
   */
  struct CommandArgumentsBuilder{
    std::vector< std::tuple<std::wstring, std::wstring > > arguments;

    inline
    CommandArgumentsBuilder() noexcept 
      : arguments{}
    {}

    inline 
    CommandArgumentsBuilder(const std::wstring& cmd) noexcept
      : arguments{}
    {
      arguments.push_back( std::make_tuple( L"" , cmd ) );
    }
  
    inline CommandArgumentsBuilder&
    add(const std::wstring& option , const std::wstring& value ) noexcept
    {
      arguments.push_back( std::make_tuple( option , value ) );
      return *this;
    }

    inline CommandArgumentsBuilder&
    add_value( const std::wstring& value ) noexcept
    {
      return this->add( std::wstring{L""} , value );
    }
    
    inline CommandArgumentsBuilder&
    add_option( const std::wstring& option ) noexcept
    {
      return this->add( option , std::wstring{L""} );
    }
    
    inline std::wstring
    build() noexcept
    {
      std::wostringstream stream{};
      for( std::tuple<std::wstring,std::wstring>& arg : arguments ){
        if( !std::get<0>( arg ).empty() ){
          auto pos = std::find_if( std::begin( std::get<0>( arg )  ) ,
                                   std::end( std::get<0>( arg ) ),
                                   [](const wchar_t &c ) -> bool{
                                     return iswspace( c );
                                   } );

          if( !stream.str().empty() ){
            stream << L' ';
          }
          
          if( pos == std::end( std::get<0>( arg ) )){
            stream << std::get<0>( arg );
          }else{
            stream << L'"' << std::get<0>( arg ) << L'"'; 
          }
        }

        if( !std::get<1>( arg ).empty() ){

          if( !stream.str().empty() ){
            stream << L' ';
          }
          
          auto pos = std::find_if( std::begin( std::get<1>( arg ) ),
                                   std::end( std::get<1>( arg ) ),
                                   [](const wchar_t &c) -> bool {
                                     return iswspace( c ) ;
                                   } );
          if( pos == std::end( std::get<1>( arg ))){
            stream << std::get<1>( arg );
          }else{
            stream << L'"' << std::get<1>( arg ) << L'"'; 
          }
        }
      }
      return std::wstring{stream.str()};
    }
  }; // end of CommandArgumentsBuilder

}; // end of namespcae wh 

#endif /* defined( __cplusplus ) */
#endif /* !defined( WH_PARSE_OPTION_H_HEADER_GUARD ) */
