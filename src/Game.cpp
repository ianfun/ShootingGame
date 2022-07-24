void onWebSocketMesssage(IOCP* ctx, PBYTE payload) {
	if (ctx->payload_len != 6) {
		log_warn("bad client: payload_len != 6");
		return;
	}
	PINT16 d = (PINT16)payload;
	ctx->x = *d++;
	ctx->y = *d++;
	ctx->rad = *d;
	d = (PINT16)ctx->buf;
	DWORD i = 0;
	for (auto player : players) {
		if (player == ctx) {
			continue;
		}
		*d++ = player->id;
		*d++ = player->x;
		*d++ = player->y;
		*d++ = player->rad;
		i += 8;
	}
	websocketWrite(ctx, ctx->buf, i, &ctx->sendOL, ctx->sendBuf, Websocket::Opcode::Binary);
}
