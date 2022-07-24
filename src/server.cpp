#define MAX_COPY (1024*1024) // max copy: 1 MB

static struct {
	SOCKET server;
	IOCP* currentCtx;
} acceptIOCP{};

void processRequest(IOCP* ctx, DWORD dwbytes);
void processIOCP(IOCP* ctx, OVERLAPPED* ol, DWORD dwbytes);
void parse_keepalive(IOCP* ctx) {
	if (ctx->firstCon) {
		ctx->firstCon = false;
	}
}
void CloseClient(IOCP* ctx) {
	if (ctx->state == State::WebSocketClosing || ctx->state == State::WebSocketConnecting || ctx->state==State::AfterHandShake) {
		deletePlayer(ctx);
	}
	ctx->state = State::AfterDisconnect;
	CancelIoEx((HANDLE)ctx->client, NULL);
	if (pDisconnectEx(ctx->client, &ctx->sendOL, 0, NULL)) {

	}
	else {
		if (WSAGetLastError() != ERROR_IO_PENDING) {
			assert(0);
		}
	}
}
int http_on_header_complete(llhttp_t* parser) {
	IOCP* ctx = (IOCP*)parser;
	parse_keepalive(ctx);
	return 0;
}
int http_on_header_field(llhttp_t* parser, const char* at, size_t length) {
	Parse_Data* p = (Parse_Data*)parser;
	p->length = length;
	p->at = at;
	return 0;
}
int http_on_header_value(llhttp_t* parser, const char* at, size_t length) {
	Parse_Data* p = (Parse_Data*)parser;
	p->headers[std::string(p->at, p->length)] = std::string(at, length);
	return 0;
}
WCHAR* URIComponentToWideChar(char* s) {
	if (UrlUnescapeA(s, NULL, NULL, URL_UNESCAPE_INPLACE | URL_UNESCAPE_AS_UTF8) != S_OK) {
		assert(0);
		return NULL;
	}
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
	if (len <= 0) {
		log_warn("client error: the url is not UTF-8 encoded");
		return NULL;
	}
	WCHAR* res = (WCHAR*)HeapAlloc(heap, 0, (SIZE_T)len * 2);
	if (res == NULL) {
		log_warn("HeapAlloc failed!");
		return NULL;
	}
	len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, res, len);
	assert(len > 0);
	return res;
}
int http_on_url(llhttp_t* parser, const char* at, size_t length) {
	if (length == 1) {
		return 0;
	}
	IOCP* ctx = reinterpret_cast<IOCP*>(parser);
	std::string tmp{ at + 1, length - 1 };
	ctx->url = URIComponentToWideChar(&tmp[0]);
	if (ctx->url == NULL) {
		return -1;
	}
	log_info("[request](method=%s, url=%ws)", llhttp_method_name((llhttp_method)ctx->p.parser.method), ctx->url);
	return 0;
}
void processIOCP(IOCP* ctx, OVERLAPPED* ol, DWORD dwbytes) {
	switch (ctx->state) {
	case State::AfterRecv:
	{
		processRequest(ctx, dwbytes);
	}break;
	case State::AfterHandShake:
	{
		ctx->state = State::WebSocketConnecting;
		ctx->recvBuf[0].buf = ctx->buf;
		ctx->recvBuf[0].len = 6;
		ctx->dwFlags = MSG_WAITALL;
		ctx->Reading6Bytes = true;
		WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
	}break;
	case State::ReadStaticFile:
	{
		(void)ReadFile(ctx->hProcess, ctx->buf, sizeof(ctx->buf), NULL, &ctx->recvOL);
		ctx->state = State::SendPartFile;
	}break;
	case State::SendPartFile:
	{
		*(reinterpret_cast<UINT64*>(&ctx->recvOL.Offset)) += dwbytes;
		ctx->sendBuf->len = dwbytes;
		WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
		if (*(reinterpret_cast<UINT64*>(&ctx->recvOL.Offset)) == ctx->filesize) {
			if (ctx->keepalive) {
				ctx->state = State::RecvNextRequest;
			}
			else {
				ctx->state = State::AfterSendHTML;
			}
		}
		else {
			ctx->state = State::ReadStaticFile;
		}
	}break;
	case State::RecvNextRequest:
	{
		CloseHandle(ctx->hProcess);
		ctx->hProcess = NULL;
		ctx->dwFlags = 0;
		assert(ctx->recvBuf[0].buf == ctx->buf);
		assert(ctx->recvBuf[0].len == sizeof(ctx->buf));
		assert(ctx->recvOL.hEvent == NULL);
		ctx->recvOL.Offset = ctx->recvOL.OffsetHigh = 0;
		WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
		ctx->state = State::AfterRecv;
	}break;
	case State::WebSocketConnecting: {
		if (ol == &ctx->recvOL) {
			if (ctx->Reading6Bytes) {
				onRead6Complete(ctx);
			}
			else {
				onRecvData(ctx);
			}
		}
		else if (ol == &ctx->sendOL) {
		}
		else {
			assert(0);
		}
	}break;
	case State::WebSocketClosing: {
		CloseClient(ctx);
	}break;
	case State::AfterSendHTML: {
		CloseClient(ctx);
	}break;
	default:
	{
		log_warn("invalid state: %u", ctx->state);
		assert(0);
	}
	}
}
void processRequest(IOCP* ctx, DWORD dwbytes) {
	ctx->buf[dwbytes] = '\0';
	ctx->hasp = true;
	if (ctx->firstCon) {
		new(&ctx->p)Parse_Data{};
	}
	enum llhttp_errno err = llhttp_execute(&ctx->p.parser, ctx->buf, dwbytes);
	if (err != HPE_OK && err != HPE_PAUSED_UPGRADE) {
		log_warn("llhttp_execute error: %s", llhttp_errno_name(err));
		CloseClient(ctx);
		return;
	}
	WSABUF* errBuf = &HTTP_ERR_RESPONCE::internal_server_error;
	switch (ctx->p.parser.method) {
	case llhttp_method::HTTP_GET: {
		switch (err) {
		case HPE_OK:
		{
			LPCWSTR file = ctx->url==NULL?L"index.html":ctx->url;
			{
				HANDLE hFile = CreateFileW(file,
					GENERIC_READ,
					FILE_SHARE_READ,
					NULL,
					OPEN_EXISTING,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
					NULL);
				if (hFile == INVALID_HANDLE_VALUE) {
					errBuf = &HTTP_ERR_RESPONCE::not_found;
					goto BAD_REQUEST_AND_RELEASE;
				}
				HANDLE r = CreateIoCompletionPort(hFile, iocp, (ULONG_PTR)ctx, N_THREADS);
				assert(r);
				const char* mine = getType(file);
				ctx->hProcess = hFile;
				LARGE_INTEGER fsize{};
				BOOL bSuccess = GetFileSizeEx(hFile, &fsize);
				ctx->filesize = (UINT64)fsize.QuadPart;
				assert(bSuccess);
				int res = sprintf(ctx->buf,
					"HTTP/1.1 200 OK\r\n"
					"Content-Type: %s\r\n"
					"Content-Length: %lld\r\n"
					"Connection: %s\r\n\r\n", mine, fsize.QuadPart, ctx->keepalive ? "keep-alive" : "close");
				assert(res > 0);
				ctx->sendBuf->buf = ctx->buf;
				ctx->sendBuf->len = (ULONG)res;
				WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
				ctx->state = State::ReadStaticFile;
			}
		}break;
		case HPE_PAUSED_UPGRADE:
		{
			auto upgrade = ctx->p.headers.find("Upgrade");
			if (upgrade != ctx->p.headers.end()) {
				if (upgrade->second == "websocket") {
					auto ws_key = ctx->p.headers.find("Sec-WebSocket-Key");
					if (ws_key != ctx->p.headers.end()) {
						{
							auto pro = ctx->p.headers.find("Sec-WebSocket-Protocol");
							if (pro == ctx->p.headers.end()) {
								ctx->state = State::AfterHandShake;
								ws_key->second += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
								char buf[29];
								BOOL ret = HashHanshake(ws_key->second.data(), (ULONG)ws_key->second.length(), buf);
								assert(ret);
								int len;
								len = sprintf(ctx->buf,
									"HTTP/1.1 101 Switching Protocols\r\n"
									"Upgrade: WebSocket\r\n"
									"Connection: Upgrade\r\n"
									"Sec-WebSocket-Accept: %s\r\n\r\n",  buf);
								ctx->sendBuf[0].buf = ctx->buf;
								ctx->sendBuf[0].len = (ULONG)len;
								WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
							}
							else {
								ctx->player_name = StrDupA(pro->second.data());
								if (ctx->player_name != NULL) {
									newPlayer(ctx);
									ctx->state = State::AfterHandShake;
									ws_key->second += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
									char buf[29];
									BOOL ret = HashHanshake(ws_key->second.data(), (ULONG)ws_key->second.length(), buf);
									assert(ret);
									int len;
									len = sprintf(ctx->buf,
										"HTTP/1.1 101 Switching Protocols\r\n"
										"Upgrade: WebSocket\r\n"
										"Connection: Upgrade\r\n"
										"Sec-WebSocket-Protocol: %s\r\n"
										"Sec-WebSocket-Accept: %s\r\n\r\n", pro->second.data(), buf);
									ctx->sendBuf[0].buf = ctx->buf;
									ctx->sendBuf[0].len = (ULONG)len;
									WSASend(ctx->client, ctx->sendBuf, 1, NULL, 0, &ctx->sendOL, NULL);
								}
								else {
									errBuf = &HTTP_ERR_RESPONCE::internal_server_error;
									goto BAD_REQUEST_AND_RELEASE;
								}
							}
						}
					}
					else {
						errBuf = &HTTP_ERR_RESPONCE::bad_request;
						goto BAD_REQUEST_AND_RELEASE;
					}
				}
				else {
					errBuf = &HTTP_ERR_RESPONCE::bad_request;
					goto BAD_REQUEST_AND_RELEASE;
				}
			}
			else {
				errBuf = &HTTP_ERR_RESPONCE::bad_request;
				goto BAD_REQUEST_AND_RELEASE;
			}
		}break;
		default: {
			errBuf = &HTTP_ERR_RESPONCE::bad_request;
			goto BAD_REQUEST_AND_RELEASE;
		}
		}
	}break;
	default:
	{
		errBuf = &HTTP_ERR_RESPONCE::method_not_allowed;
	BAD_REQUEST_AND_RELEASE:
		ctx->state = State::AfterSendHTML;
		WSASend(ctx->client, errBuf, 1, NULL, 0, &ctx->sendOL, NULL);
	}
	}
}