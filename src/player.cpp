void deletePlayer(IOCP* ctx) {
	auto num = players.erase(ctx);
	assert(num);
	printf("Delete player: %s\n", ctx->player_name);
}

void newPlayer(IOCP* ctx) {
	static UINT16 gid = 0;
	ctx->id = ++gid;
	players.insert(ctx);
	printf("new player: %s\n", ctx->player_name);
}