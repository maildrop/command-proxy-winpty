#include <iostream>
#include <tchar.h>
#include <windows.h>
#include "verify.h"

HRESULT CreatePseudoConsoleAndPipes(HPCON* phPC, HANDLE* phPipeIn, HANDLE* phPipeOut)
{

  if( !phPC ){
    return E_INVALIDARG;
  }

  if( !phPipeIn ){
    return E_INVALIDARG;
  }

  if( !phPipeOut ){
    return E_INVALIDARG;
  }

  HRESULT hr{ E_UNEXPECTED };
  HANDLE hPipePTYIn{ INVALID_HANDLE_VALUE };
  HANDLE hPipePTYOut{ INVALID_HANDLE_VALUE };

  if ( CreatePipe(&hPipePTYIn, phPipeOut, NULL, 0)) {
    if ( CreatePipe(phPipeIn, &hPipePTYOut, NULL, 0)) {

      // Determine required size of Pseudo Console
      COORD consoleSize{};
      HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };
      //HANDLE hConsole{ CreateFile(TEXT("CONOUT$"),GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0) };

      assert( INVALID_HANDLE_VALUE != hConsole );
      
      {
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        /* GetConsoleScreenBufferInfo sometimes fails. For example, in Emacs conproxy.exe */
        if (GetConsoleScreenBufferInfo(hConsole, &csbi)) 
          {
            static_assert( std::is_signed< decltype( consoleSize.X )>::value , "" );
            static_assert( std::is_signed< decltype( consoleSize.Y )>::value , "" );
            
            consoleSize.X = (std::max<decltype( consoleSize.X )>)( 1, csbi.srWindow.Right - csbi.srWindow.Left + 1 );
            consoleSize.Y = (std::max<decltype( consoleSize.Y )>)( 1, csbi.srWindow.Bottom - csbi.srWindow.Top + 1 );
          }
        else
          {
            DWORD lastError = GetLastError();
            switch( lastError ){
            case ERROR_ACCESS_DENIED:
              std::cout << "ERROR_ACCESS_DENIED" << std::endl;
              break;
            case ERROR_INVALID_HANDLE:
              std::cout << "ERROR_INVALID_HANDLE" << std::endl;
              break;
            default:
              std::cout << lastError << std::endl; 
              break;
            }
            
            consoleSize.X = typename decltype(consoleSize.X) {120};
            consoleSize.Y = typename decltype(consoleSize.Y) {30};
          }
      }
      
      std::cout << "consoleSize = { " << consoleSize.X << " , " << consoleSize.Y << " }" << std::endl;
      
      // Create the Pseudo Console of the required size, attached to the PTY-end of the pipes
      // Note: consoleSize require 0 < X and 0 < Y
      assert( (0 < consoleSize.X && 0 < consoleSize.Y) );
      hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, 0, phPC);

      // Note: We can close the handles to the PTY-end of the pipes here
      // because the handles are dup'ed into the ConHost and will be released
      // when the ConPTY is destroyed.
      if (INVALID_HANDLE_VALUE != hPipePTYOut){
        VERIFY( CloseHandle(hPipePTYOut) );
      }
    }
    
    if (INVALID_HANDLE_VALUE != hPipePTYIn) {
      VERIFY( CloseHandle(hPipePTYIn) );
    }
  }
  return hr;
}

int entry()
{

  HRESULT hr = E_UNEXPECTED;
  HANDLE hPipeIn  = INVALID_HANDLE_VALUE ;
  HANDLE hPipeOut = INVALID_HANDLE_VALUE ;

  HPCON hPC = {};
  
  hr = CreatePseudoConsoleAndPipes( &hPC , &hPipeIn, &hPipeOut );
  if( S_OK != hr ){
    std::cout << hr << std::endl;
  }
  if( S_OK == hr ){
    // Close ConPTY - this will terminate client process if running
    STARTUPINFOEX siEx = {0};
    siEx.StartupInfo.cb = sizeof( siEx );


    size_t list_size = 0;
    InitializeProcThreadAttributeList( nullptr , 1 , 0,  &list_size );

    std::cout << "ProcThreadAttributeList size = " << list_size << std::endl;
    

    (void)ClosePseudoConsole(hPC);
    // Clean-up the pipes
    if (INVALID_HANDLE_VALUE != hPipeOut) {
      VERIFY( CloseHandle(hPipeOut) );
    }
    if (INVALID_HANDLE_VALUE != hPipeIn) {
      VERIFY( CloseHandle(hPipeIn) );
    }
  }
  return 0;
}

int main(int,char*[])
{
  std::locale::global( std::locale(""));
  DWORD console_mode;
  GetConsoleMode( GetStdHandle( STD_OUTPUT_HANDLE ), &console_mode );

  if( 1 ){
    if( ENABLE_ECHO_INPUT & console_mode ){
      std::cout << " enable ENABLE_ECHO_INPUT" << std::endl;
    }else{
      std::cout << "disable ENABLE_ECHO_INPUT" << std::endl;
    }

    if( ENABLE_INSERT_MODE & console_mode ){
      std::cout << " enable ENABLE_INSERT_MODE" << std::endl;
    }else{
      std::cout << "disalbe ENABLE_INSERT_MODE" << std::endl;
    }
    if( ENABLE_LINE_INPUT & console_mode ){
      std::cout << " enable ENABLE_LINE_INPUT" << std::endl;
    }else{
      std::cout << "disable ENABLE_LINE_INPUT" << std::endl;
    }
    if( ENABLE_VIRTUAL_TERMINAL_INPUT & console_mode ){
      std::cout << " enable ENABLE_VIRTUAL_TERMINAL_INPUT" << std::endl;
    }else{
      std::cout << "disable ENABLE_VIRTUAL_TERMINAL_INPUT" << std::endl;
    }

    {
      DWORD handle_information{};
      if( GetHandleInformation( GetStdHandle( STD_OUTPUT_HANDLE ), &handle_information ) ){
        std::cout << "STD_OUTPUT_HANDLE "
                  << (( handle_information & HANDLE_FLAG_INHERIT ) ? "HANDLE_FLAG_INHERIT" : "!HANDLE_FLAG_INHERIT" )
                  << std::endl;
        //std::cout << (handle_information&(~HANDLE_FLAG_INHERIT)) << std::endl;
      }
    }
  }

  entry();
  
  return 0;
}
