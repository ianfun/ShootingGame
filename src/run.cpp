#define _CRT_SECURE_NO_WARNINGS // sprintf
#define N_THREADS 1
#include <winsock2.h>
#include <vector>
#include <Ws2tcpip.h>
#include <mstcpip.h>
#include <mswsock.h>
#include <iphlpapi.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <wchar.h>
#include <WinInet.h>
#include <map>
#include <string>
#include <shlwapi.h>
#include <llhttp.h>
#include <set>
#include "types.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock") 
#pragma comment(lib, "wininet")
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment (lib, "Shlwapi.lib")
static CHAR* computer_name;
static SIZE_T computer_name_len;
static LPFN_ACCEPTEX pAcceptEx;
static LPFN_DISCONNECTEX pDisconnectEx;
static HANDLE heap, iocp;
static HMODULE ntdll;
static OVERLAPPED dumyOL = { 0 };
struct IOCP;

std::set<IOCP*> players{};

#if N_THREADS > 1
static HANDLE hThs[N_THREADS-1];
#endif

#ifdef _DEBUG
#define log_info(s, ...) fprintf(stderr, "\u001b[32m[info]: " s "\u001B[0m\n", __VA_ARGS__)
#define log_warn(s, ...)  fprintf(stderr, "\u001b[33m[warn]: " s "\u001B[0m\nWSAError: ", __VA_ARGS__);printWSAError()
#define assert(x) {if (!(x)){fprintf(stderr, "\u001b[31m[assertion failed]: %s.%d: %s, err=%d\u001B[0m\nWSAError: ", __FILE__, __LINE__, #x, WSAGetLastError());printWSAError();}}
#else
#define assert(x) (x)
#define log_info(x, ...)
#define log_warn(x, ...)

#endif // _DEBUG
#define TRACE 0
#if TRACE
#define WSASend(a, b, c, d, e, f, g) {puts("WSASend");WSASend(a, b, c, d, e, f, g);}
#define WSARecv(a, b, c, d, e, f, g) {puts("WSARecv");WSARecv(a, b, c, d, e, f, g);}
#define CloseClient(a) {puts("CloseClient");closeClient(a);}
#else
#define CloseClient closeClient
#endif

void websocketWrite(IOCP* ctx, const char* msg, ULONG length, OVERLAPPED* ol, WSABUF wsaBuf[2], Websocket::Opcode op);
void printWSAError() {
	LPWSTR s = NULL;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&s, 0, NULL);
	DWORD written;
	WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), s, lstrlenW(s), &written, NULL);
	LocalFree(s);
}

__declspec(noreturn) void fatal(const char* msg) {
	log_warn("%s", msg);
	ExitProcess(1);
}
void closeClient(IOCP* ctx);

#include "player.cpp"
#include "Mine.h"
void accept_next();
#include "handshake.cpp"
#include "Game.cpp"
#include "frame.cpp"
#include "server.cpp"
#include "accept.cpp"

#pragma warning(push)
#pragma warning(disable: 6308)
#pragma warning(disable: 28182)

DWORD __stdcall run(LPVOID param)
{
	heap = GetProcessHeap();
	if (heap == NULL) {
		fatal("GetProcessHeap");
	}
	{
		DWORD size = 0;
		WCHAR* tmp = NULL;
		if (GetComputerNameW(tmp, &size) == FALSE) {
			if (GetLastError() != ERROR_BUFFER_OVERFLOW) {
				fatal("GetComputerNameW");
			}
			tmp = (WCHAR*)HeapAlloc(heap, 0, (SIZE_T)size * 2);
			if (tmp == NULL) {
				fatal("HeapAlloc");
			}
			if (GetComputerNameW(tmp, &size) == FALSE) {
				fatal("GetComputerNameW");
			}
			int len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, tmp, -1, NULL, 0, NULL, NULL);
			if (len <= 0) {
				fatal("WideCharToMultiByte");
			}
			computer_name = (CHAR*)HeapAlloc(heap, 0, len);
			if (computer_name == NULL) {
				fatal("HeapAlloc");
			}
			len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, tmp, -1, computer_name, len, NULL, NULL);
			if (len <= 0) {
				fatal("WideCharToMultiByte");
			}
			computer_name_len = len-1; // no '\0'
			HeapFree(heap, 0, tmp);
		}
	}
	//system("chcp 65001");
	SetConsoleTitleW(L"Web Server");
	{
		WSADATA wsaData{};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			fatal("WSAStartup");
		}
	}
	iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, N_THREADS);
	if (iocp == NULL) {
		fatal("CreateIoCompletionPort");
	}
	if (!initHash()) {
		fatal("initHash");
	}
#ifndef PORT
#define PORT 80
#endif
	sockaddr_in ip4{ .sin_family = AF_INET, .sin_port = htons(PORT) };
#ifdef  CONSOLE_APP
	{
		PIP_ADAPTER_INFO pAdapter = (IP_ADAPTER_INFO*)HeapAlloc(heap, 0, sizeof(IP_ADAPTER_INFO));
		ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
		if (pAdapter == NULL) {
			fatal("HeapAlloc");
		}
		if (GetAdaptersInfo(pAdapter, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
			pAdapter = (IP_ADAPTER_INFO*)HeapReAlloc(heap, 0, pAdapter, ulOutBufLen);

			if (pAdapter == NULL) {
				fatal("HeapReAlloc");
				return 1;
			}
		}
		if (GetAdaptersInfo(pAdapter, &ulOutBufLen) == NO_ERROR) {
			PIP_ADAPTER_INFO head = pAdapter;
			DWORD i = 0;
			puts("select your ip address");
			while (pAdapter) {
				printf("[%d] %s\n", i, pAdapter->IpAddressList.IpAddress.String);
				pAdapter = pAdapter->Next;
				i++;
			}
			DWORD num;
			puts("enter number:");
			while (scanf_s("%u", &num) != 1 || num > i) { puts("invalid number, try again"); }
			while (num != 0) {
				num--;
				head = head->Next;
			}
			printf("selected address: %s\n", head->IpAddressList.IpAddress.String);
			if (inet_pton(AF_INET, head->IpAddressList.IpAddress.String, (SOCKADDR*)&ip4) != 1) {
				fatal("inet_pton");
			}
			USHORT port;
			puts("enter port");
			while (scanf_s("%hu", &port) != 1) { puts("invalid port, try again"); }
			printf("select port: %hu\n", port);
			ip4.sin_port = htons(port);
			ip4.sin_family = AF_INET;
		}
		else {
			fatal("GetAdaptersInfo");
		}
		HeapFree(heap, 0, pAdapter);
	}
#endif
	acceptIOCP.server = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (acceptIOCP.server == INVALID_SOCKET) {
		fatal("WSASocketW");
	}
	if (bind(acceptIOCP.server, (struct sockaddr*)&ip4, sizeof(ip4)) == SOCKET_ERROR) {
		fatal("bind");
	}
	if (listen(acceptIOCP.server, SOMAXCONN) == SOCKET_ERROR) {
		fatal("listen");
	}
	{
		DWORD dwBytes = 0;
		GUID ga = WSAID_ACCEPTEX, gd = WSAID_DISCONNECTEX;
		if (WSAIoctl(acceptIOCP.server, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&ga, sizeof(ga),
			&pAcceptEx, sizeof(pAcceptEx),
			&dwBytes, NULL, NULL) == SOCKET_ERROR || WSAIoctl(acceptIOCP.server, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&gd, sizeof(gd),
				&pDisconnectEx, sizeof(pDisconnectEx),
				&dwBytes, NULL, NULL)) {
			fatal("WSAIoctl: get AcceptEx & DisconnectEx function pointer");
		}
	}
	if (CreateIoCompletionPort((HANDLE)acceptIOCP.server, iocp, (ULONG_PTR)pAcceptEx, N_THREADS) == NULL) {
		fatal("CreateIoCompletionPort");
	}
	accept_next();
	
#if N_THREADS > 1
	for (int i = 0; i < N_THREADS-1; ++i) {
		DWORD id;
		hThs[i] = CreateThread(NULL, 0, RunIOCPLoop, NULL, 0, &id);
		log_info("spawn thrad: (id=%u)", id);
	}
#endif
	log_info("running main iocp thread: (id=%u)", GetCurrentThreadId());
	(void)RunIOCPLoop(NULL);
	log_info("exit main iocp loop (id=%u), wait for all threads exit\n", GetCurrentThreadId());
#if N_THREADS > 1
	WaitForMultipleObjects(N_THREADS - 1, hThs, TRUE, INFINITE);
#endif
	shutdown(acceptIOCP.server, SD_BOTH);
	closesocket(acceptIOCP.server);
	WSACleanup();
	
	closeHash();
	_ASSERT(_CrtCheckMemory());
	CloseHandle(iocp);
	puts("[info] exit process");
	return 0;
}
#pragma warning(pop)


