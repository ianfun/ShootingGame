void websocketWrite(IOCP* ctx, const char* msg, ULONG length, OVERLAPPED* ol, WSABUF wsaBuf[2], Websocket::Opcode op) {
	ULONG hsize = 2;
	ctx->header[0] = 0b10000000 | BYTE(op);
	if (length < 126) {
		ctx->header[1] = (BYTE)length;
	}
	else if (length < 0b10000000000000000) {
		hsize += 2;
		ctx->header[1] = 126;
		ctx->header[2] = (BYTE)(length >> 8);
		ctx->header[3] = (BYTE)length;
	}
	else {
		log_warn("client error: websocket: data too long");
		return;
	}
	wsaBuf[0].buf = (char*)ctx->header;
	wsaBuf[0].len = hsize;
	wsaBuf[1].buf = (char*)msg;
	wsaBuf[1].len = length;
	WSASend(ctx->client, wsaBuf, 2, NULL, 0, ol, NULL);
	_ASSERT(_CrtCheckMemory());
}
#pragma warning(push)
#pragma warning(disable: 28193)
void onRecvData(IOCP* ctx) {
	_ASSERT(_CrtCheckMemory());
	PBYTE mask = (PBYTE)ctx->buf;
	PBYTE payload = mask + 4;
	for (unsigned __int64 i = 0; i < ctx->payload_len; ++i) {
		payload[i] = payload[i] ^ mask[i % 4];
	}
	{
		switch (ctx->op) {
		case Websocket::Text:
		case Websocket::Binary:
		{
			onWebSocketMesssage(ctx, payload);
		}break;
		case Websocket::Close:
		{
			constexpr USHORT close_msg = 1000;
			((PBYTE)ctx->buf)[0] = (BYTE)(close_msg >> 8);
			((PBYTE)ctx->buf)[1] = (BYTE)close_msg;
			websocketWrite(ctx, ctx->buf, 2, &ctx->sendOL, ctx->sendBuf, Websocket::Opcode::Close);
			ctx->state = State::WebSocketClosing;
		}break;
		default: {
			CloseClient(ctx);
		}return;
		}
	}
	ctx->recvBuf[0].len = 6;
	ctx->dwFlags = MSG_WAITALL;
	ctx->recvBuf[0].buf = ctx->buf;
	WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
	ctx->Reading6Bytes = true;
	_ASSERT(_CrtCheckMemory());
}
#pragma warning(pop)
void onRead6Complete(IOCP* ctx) {
	using namespace Websocket;
	PBYTE data = (PBYTE)ctx->buf;
	BIT FIN = data[0] & 0b10000000;
	if (!FIN) {
		log_warn("client error: websocket: FIN MUST be 1");
		CloseClient(ctx);
		return;
	}
	ctx->op = Websocket::Opcode(data[0] & 0b00001111);
	if (data[0] & 0b01110000) {
		log_warn("client error: websocket: RSV is not zero");
		CloseClient(ctx);
		return;
	}
	BIT hasmask = data[1] & 0b10000000;
	if (!hasmask) {
		log_warn("client error: websocket: client MUST mask data");
		CloseClient(ctx);
		return;
	}
	ctx->payload_len = data[1] & 0b01111111;
	PBYTE precv;
	ULONG offset;
	switch (ctx->payload_len) {
	default:
		offset = 0;
		data[0] = data[2];
		data[1] = data[3];
		data[2] = data[4];
		data[3] = data[5];
		precv = data + 4;
		break;
	case 126:
		ctx->payload_len = ((unsigned long long)data[2] << 8) | (unsigned long long)data[3];
		offset = 2;
		data[0] = data[4];
		data[1] = data[5];
		precv = data + 2;
		break;
	case 127:
		offset = 8;
		ctx->payload_len = ntohll(*(unsigned __int64*)&data[2]);
		precv = data + 0;
		break;
	}
	if (ctx->payload_len == 0) {
		ctx->Reading6Bytes = true;
		return;
	}
	if (ctx->payload_len + 6 > sizeof(ctx->buf)) {
		log_warn("client error: websocket: data too large!");
		CloseClient(ctx);
		return;
	}
	ctx->Reading6Bytes = false;
	ctx->recvBuf[0].len = (ULONG)ctx->payload_len + offset;
	ctx->recvBuf[0].buf = (char*)precv;
	ctx->dwFlags = MSG_WAITALL;
	WSARecv(ctx->client, ctx->recvBuf, 1, NULL, &ctx->dwFlags, &ctx->recvOL, NULL);
	_ASSERT(_CrtCheckMemory());
}