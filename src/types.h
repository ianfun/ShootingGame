namespace Websocket {
	using BIT = BYTE;
	enum Opcode : BYTE {
		Continuation = 0,
		Text = 0x1,
		Binary = 0x2,
		Close = 0x8,
		Ping = 0x9,
		Pong = 0xA
	};
}

#ifdef __GNUC__
#define CONSTEVAL
#else
#define CONSTEVAL consteval
#endif
template <ULONG N>
CONSTEVAL ULONG cstrlen(const char(&)[N]) {
	return N - 1;
}
CONSTEVAL ULONG  cstrlen(const char* s) {
	ULONG res = 0;
	for (; *s; s++) {
		res++;
	}
	return res;
}
enum class State : unsigned __int8 {
	AfterRecv=0, 
	ReadStaticFile=1, 
	SendPartFile=2, 
	RecvNextRequest=4, 
	AfterSendHTML=8, 
	AfterHandShake=16, 
	WebSocketConnecting=32, 
	WebSocketClosing=64,
	AfterDisconnect=128
};
int http_on_header_field(llhttp_t* parser, const char* at, size_t length);
int http_on_header_value(llhttp_t* parser, const char* at, size_t length);
int http_on_url(llhttp_t* parser, const char* at, size_t length);
int http_on_header_complete(llhttp_t* parser);
struct Parse_Data {
	Parse_Data() : headers{}, at{}, length{} {
		llhttp_settings_init(&settings);
		settings.on_url = http_on_url;
		settings.on_header_field = http_on_header_field;
		settings.on_header_value = http_on_header_value;
		settings.on_headers_complete = http_on_header_complete;
		llhttp_init(&parser, HTTP_REQUEST, &settings);
	};
	llhttp_t parser; 
	std::map<std::string, std::string> headers;
	size_t length;
	const char* at;
	llhttp_settings_t settings;
};

struct IOCP {
	Parse_Data p;
	WCHAR* url;
	union {
		UINT64 filesize;
		UINT16 id;
	};
	SOCKET client;
	State state;
	OVERLAPPED recvOL, sendOL;
	char buf[4096+64];
	char padding;
	DWORD dwFlags;
	WSABUF sendBuf[2], recvBuf[1];
	unsigned __int64 payload_len;
	BYTE header[4];
	Websocket::Opcode op;
	HANDLE hProcess;
	CHAR* player_name;
	INT16 x, y, rad;
	bool keepalive: 1,  firstCon:1, Reading6Bytes: 1, hasp: 1;
};

namespace HTTP_ERR_RESPONCE {
	static char sinternal_server_error[] = "HTTP/1.1 500 Internal Server Error\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"\r\n"
		"<!DOCTYPE html><html><head><title>500 Internal Server Error</title></head><body><h1 align=\"center\">500 Internal Server Error</h1><hr><p style=\"text-align: center\">The server has some error...<p/></body></html>",
		snot_found[] =
		"HTTP/1.1 404 Not Found\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"\r\n"
		"<!DOCTYPE html><html><head><title>404 Not Found</title></head><body><h1 align=\"center\">404 Not Found</h1><hr><p style=\"text-align: center\">The Request File is not found in the server<p/></body></html>",

		smethod_not_allowed[] =
		"HTTP/1.1 405 Method Not Allowed\r\n"
		"Connection: close\r\n"
		"Allow: GET, HEAD\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"\r\n"
		"<!DOCTYPE html><html><head><title>405 Method Not Allowed</title></head><body><h1 align=\"center\">405 Method Not Allowed</h1><hr><p style=\"text-align: center\">You can only use GET, HEAD request<p/></body></html>",
		sbad_request[] =
		"HTTP/1.1 400 Bad Request\r\n"
		"Connection: close\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"\r\n"
		"<!DOCTYPE html><html><head><title>400 Bad Request</title></head><body><h1 align=\"center\">400 Bad Request</h1><hr><p style=\"text-align: center\">Thes server can't process the request<br>Maybe you miss some header(s)<p/></body></html>",
		sclient_percent_err[] = 
		"HTTP/1.1 400 Bad Request\r\n"
		"Connection: close\r\n"
		"\r\n"
		"The Server cannot handle the request because the one or more url or header-value is not percent-encoded",
		sclient_utf8_err[] =
		"HTTP/1.1 400 Bad Request\r\n"
		"Connection: close\r\n"
		"\r\n"
		"The Server cannot handle the request because the one or more url or header-value is not UTF8-encoded";
	static WSABUF
		internal_server_error = {
			cstrlen(sinternal_server_error), sinternal_server_error
	},
		not_found = {
			cstrlen(snot_found), snot_found
	},
		method_not_allowed = {
		cstrlen(smethod_not_allowed), smethod_not_allowed
	},
		bad_request = {
		cstrlen(sbad_request), sbad_request
	},
		client_percent_err = {
		cstrlen(sclient_percent_err), sclient_percent_err
	},
		client_utf8_err = {
		cstrlen(sclient_utf8_err), sclient_utf8_err
	};
}