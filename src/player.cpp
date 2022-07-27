void deletePlayer(IOCP* ctx) {
	auto num = players.erase(ctx);
	assert(num);
	log_info("player quit game! %s", ctx->player_name);
}

void newPlayer(IOCP* ctx) {
	static UINT16 gid = 0;
	ctx->id = ++gid;
	players.insert(ctx);
	log_info("new player join! %s", ctx->player_name);
}