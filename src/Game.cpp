void onWebSocketMesssage(IOCP* ctx, PBYTE payload) {
	if (ctx->payload_len  < 8) {
		log_info("bad client: payload_len too small");
		return;
	}
	PINT16 d = (PINT16)payload;
	ctx->x = *d++;
	ctx->y = *d++;
	ctx->rad = *d++;
	ctx->blood = *d++;
	for (unsigned long long i = 8;i<ctx->payload_len;i += 6) {
		Bullet b = {.x=d[0], .y=d[1], .rad=d[2]};
		for (auto player : players) {
#define MAX_BULLETS 10
			if (player==ctx || player->buf[0] > MAX_BULLETS) {
				continue;
			}
			PINT16 p = (PINT16)(player->bullets + 1 + player->bullets[0]);
			if ((PBYTE)(p + 3) < (player->bullets + sizeof(player->bullets))) {
				*p++ = b.x;
				*p++ = b.y;
				*p++ = b.rad;
				player->bullets[0]++;
			}
		}
		d += 3;
	}
	DWORD s = (DWORD)ctx->bullets[0] * 6 + 1;
	d = (PINT16)(ctx->buf + s);
	memcpy((PVOID)ctx->buf, (PVOID)ctx->bullets, (SIZE_T)s);
	DWORD i = s;
	for (auto player : players) {
		if (player == ctx) {
			continue;
		}
		i += 10;
		if (i > sizeof(ctx->buf)) {
			break;
		}
		*d++ = player->id;
		*d++ = player->x;
		*d++ = player->y;
		*d++ = player->rad;
		*d++ = player->blood;
	}
	ctx->bullets[0] = 0;
	websocketWrite(ctx, ctx->buf, i, &ctx->sendOL, ctx->sendBuf, Websocket::Opcode::Binary);
}
