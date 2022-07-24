void deletePlayer(IOCP* ctx) {
	auto num = players.erase(ctx);
	assert(num);
	printf("Delete player: %s\n", ctx->player_name);
	LocalFree(ctx->player_name);
}

void newPlayer(IOCP* ctx) {
	players.insert(ctx);
	printf("new player: %s\n", ctx->player_name);
}