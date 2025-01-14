/**
   これまでの実験で一番期待した動作に近いもの

   msys2 の /usr/bin/* コマンドが終了する問題に対応するために、
   cmdproxy に winpty を加えて、標準入力をコンソールにつなげて標準出力と標準エラー出力をパイプにつなげる

   
   startupinfo.StartupInfo.hStdInput = NULL; で、pseudo console の入力をそのまま使う
   hStdOutput と hStdError は、 パイプ経由で出力されるものを得る

 */
#include <tchar.h>
#include <windows.h>
#include <process.h>

#include <iostream>
#include <locale>
#include <string>
#include <cassert>
#pragma comment(lib,"rpcrt4")

#ifndef VERIFY
# ifdef NDEBUG 
#  define VERIFY(expression) do{ ( expression ); }while( 0 )
# else /* NDEBUG */
#  define VERIFY(expression) assert( expression )
# endif /* NDEBUG */
#endif  /* VERIFY*/

unsigned __stdcall outputPipeListener(void* pipe) noexcept
{
    HANDLE hPipe{ pipe };
    HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };

    const DWORD BUFF_SIZE{ 512 };
    char szBuffer[BUFF_SIZE]{};

    DWORD dwBytesWritten{};
    DWORD dwBytesRead{};
    BOOL fRead{ FALSE };
    do {
      // Read from the pipe
      fRead = ReadFile(hPipe, szBuffer, BUFF_SIZE, &dwBytesRead, NULL);
      
      // Write received text to the Console
      // Note: Write to the Console using WriteFile(hConsole...), not printf()/puts() to
      // prevent partially-read VT sequences from corrupting output
      WriteFile(hConsole, szBuffer, dwBytesRead, &dwBytesWritten, NULL);
    } while (fRead && dwBytesRead >= 0);
    return 0;
}

unsigned __stdcall pseudoConsoleOutputPipeLister( void *pipe ) noexcept
{
    HANDLE hPipe{ pipe };
    //HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };

    const DWORD BUFF_SIZE{ 512 };
    char szBuffer[BUFF_SIZE]{};

    // DWORD dwBytesWritten{};
    DWORD dwBytesRead{};
    BOOL fRead{ FALSE };
    do {
      // Read from the pipe
      fRead = ReadFile(hPipe, szBuffer, BUFF_SIZE, &dwBytesRead, NULL);
      
      // Write received text to the Console
      // Note: Write to the Console using WriteFile(hConsole...), not printf()/puts() to
      // prevent partially-read VT sequences from corrupting output
      // WriteFile(hConsole, szBuffer, dwBytesRead, &dwBytesWritten, NULL);
    } while (fRead && dwBytesRead >= 0);
    return 0;
}

unsigned __stdcall inputPipeListener( void* pipe ) noexcept
{
  HANDLE hPipe{ pipe };
  HANDLE hConsole{ GetStdHandle( STD_INPUT_HANDLE ) };
  const DWORD BUFF_SIZE{ 512 };
  char szBuffer[BUFF_SIZE]{};

  DWORD dwBytesWritten{};
  DWORD dwBytesRead{};
  BOOL fRead{ FALSE };
  do{
    // Read from the pipe
    fRead = ReadFile(hConsole, szBuffer, BUFF_SIZE, &dwBytesRead, NULL);
    
    // Write received text to the Console
    // Note: Write to the Console using WriteFile(hConsole...), not printf()/puts() to
    // prevent partially-read VT sequences from corrupting output
    WriteFile(hPipe, szBuffer, dwBytesRead, &dwBytesWritten, NULL);
  }while( fRead && dwBytesRead >= 0 );
  return 0;
}

void 
createNamedPipe( HANDLE& readSide , HANDLE &writeSide ,SECURITY_ATTRIBUTES& security_attributes ) noexcept
{
  {
    UUID uuid{};
    if( SUCCEEDED( UuidCreateSequential( &uuid ) )){

      struct rpcwstrdeletor{
        inline 
        void operator()( void * ptr ) const noexcept
        {
          RPC_WSTR wstr{ reinterpret_cast<RPC_WSTR>(ptr) };
          VERIFY( RPC_S_OK == RpcStringFree( &wstr ) );
          assert( wstr == nullptr );
        }
      };
      
      std::unique_ptr< wchar_t[] , rpcwstrdeletor > stringUUID {
        [&]()->wchar_t*{
          RPC_WSTR stringUUID{nullptr};
          VERIFY( RPC_S_OK == UuidToStringW( &uuid , &stringUUID ) );
          return reinterpret_cast<wchar_t*>(stringUUID);
        }()
      };
      
      // このパイプは ローカルでしか使わないので 名前には \\.\pipe\LOCAL を使うべきである。
      wchar_t path[MAX_PATH] = L"\\\\.\\pipe\\LOCAL\\" L"application-";
      wcscat_s( path , _countof( path ) , stringUUID.get() );

      std::wcout << path << std::endl;

      writeSide = ::CreateNamedPipeW( path ,
                                      PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE ,
                                      PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                      1 ,
                                      0,
                                      0,
                                      0,
                                      nullptr );
      assert( INVALID_HANDLE_VALUE != writeSide );

      if( INVALID_HANDLE_VALUE != writeSide ){
        CREATEFILE2_EXTENDED_PARAMETERS createfile2_extended_parameters{};
        createfile2_extended_parameters.dwSize = sizeof( CREATEFILE2_EXTENDED_PARAMETERS );
        createfile2_extended_parameters.lpSecurityAttributes = &security_attributes;
        
        readSide = ::CreateFile2( path ,
                                  GENERIC_READ | GENERIC_WRITE ,
                                  0 ,
                                  OPEN_EXISTING ,
                                  &createfile2_extended_parameters );
        assert( INVALID_HANDLE_VALUE != readSide );
      }
    }
  }
  assert( INVALID_HANDLE_VALUE != readSide );
  assert( INVALID_HANDLE_VALUE != writeSide );
  return;
}

HRESULT
PrepareStartupInformation(const HPCON& hpc, STARTUPINFOEX& si_ref) noexcept
{
  // Prepare Startup Information structure
  STARTUPINFOEX si = {0};
  si.StartupInfo.cb = sizeof(STARTUPINFOEX);

  // Discover the size required for the list
  {
    size_t bytesRequired = 0;
    ::InitializeProcThreadAttributeList( nullptr , 1, 0, &bytesRequired);

    assert( 0 < bytesRequired );
    
    // Allocate memory to represent the list
    si.lpAttributeList =
      reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>( ::HeapAlloc(GetProcessHeap(), 0, bytesRequired) );

    if (!si.lpAttributeList) {
      return E_OUTOFMEMORY;
    }
  
    // Initialize the list memory location
    if ( ! ::InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired) ){
      VERIFY( ::HeapFree(GetProcessHeap(), 0, si.lpAttributeList) );
      return HRESULT_FROM_WIN32(GetLastError());
    }
  }
  
  // Set the pseudoconsole information into the list
  if (!UpdateProcThreadAttribute(si.lpAttributeList,
                                 0,
                                 PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                 hpc,
                                 sizeof(hpc),
                                 NULL,
                                 NULL)) {
    VERIFY( ::HeapFree(GetProcessHeap(), 0, si.lpAttributeList) );
    return HRESULT_FROM_WIN32(GetLastError());
  }
  
  si_ref = si;
  
  return S_OK;
}

int main(int,char*[])
{
  std::locale::global( std::locale{""} );

  SECURITY_ATTRIBUTES security_attributes{};
  {
    security_attributes.nLength = sizeof( SECURITY_ATTRIBUTES );
    security_attributes.lpSecurityDescriptor = nullptr;
    security_attributes.bInheritHandle = TRUE;
  }

  HANDLE console_in_readSide  = INVALID_HANDLE_VALUE;
  HANDLE console_in_writeSide = INVALID_HANDLE_VALUE;
  createNamedPipe( console_in_readSide , console_in_writeSide , security_attributes );
  
  HANDLE console_out_readSide = INVALID_HANDLE_VALUE;
  HANDLE console_out_writeSide = INVALID_HANDLE_VALUE;
  createNamedPipe( console_out_readSide , console_out_writeSide , security_attributes );

  
  HPCON hPC = {0};
  HRESULT hr = ::CreatePseudoConsole(COORD{ 80 , 25 } , console_in_readSide, console_out_writeSide , 0, &hPC);
  std::ignore = hr;

  HANDLE stdout_readSide = INVALID_HANDLE_VALUE;
  HANDLE stdout_writeSide = INVALID_HANDLE_VALUE;

  CreatePipe( &stdout_readSide , &stdout_writeSide , &security_attributes , 0 );
  SetHandleInformation( stdout_readSide , HANDLE_FLAG_INHERIT , 0 );
  
  PROCESS_INFORMATION pi{};
  
  STARTUPINFOEXW startupinfo{};
  startupinfo.StartupInfo.cb = sizeof( STARTUPINFOEXW );
  PrepareStartupInformation( hPC , startupinfo );

  if( true ){
    startupinfo.StartupInfo.hStdInput = NULL; // これはpseudo console の入力を使うことを指示するため
    startupinfo.StartupInfo.hStdOutput = stdout_writeSide;
    startupinfo.StartupInfo.hStdError = stdout_writeSide;
    startupinfo.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
  }
  
  wchar_t cmdline[] = L"cmd.exe";
  if( CreateProcessW( nullptr ,
                      cmdline ,
                      NULL,
                      NULL,
                      TRUE ,
                      EXTENDED_STARTUPINFO_PRESENT ,
                      nullptr,
                      nullptr,
                      &startupinfo.StartupInfo,
                      &pi ) ){
    VERIFY( ::CloseHandle( pi.hThread ) );
    VERIFY( ::CloseHandle( console_in_readSide ) );
    VERIFY( ::CloseHandle( stdout_writeSide ) );
    
    _beginthreadex( nullptr, 0,
                    outputPipeListener ,
                    stdout_readSide ,
                    0,
                    nullptr );

    _beginthreadex( nullptr, 0,
                    pseudoConsoleOutputPipeLister ,
                    console_out_readSide ,
                    0,
                    nullptr );

    _beginthreadex( nullptr , 0,
                    inputPipeListener ,
                    console_in_writeSide ,
                    0 ,
                    nullptr );
    
  }
  WaitForSingleObject( pi.hProcess , INFINITE );
  VERIFY( ::CloseHandle( pi.hProcess ) );
  
  return EXIT_SUCCESS;
}
