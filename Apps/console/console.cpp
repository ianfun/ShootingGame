#define PORT 80
#define CONSOLE_APP
#include "../../src/run.cpp"

BOOL WINAPI ConsoleHandler(DWORD event)
{
	switch (event) {
	case CTRL_C_EVENT:
		for (int i = 0; i < N_THREADS; ++i) {
			PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)RunIOCPLoop, 0);
		}
		return TRUE;
	case CTRL_BREAK_EVENT:
		{
		printf("* * * * player infomation * * * *\n%zu players\n", players.size());
		for (auto ctx : players) {
			printf("\tname: %s, x: %d, y: %d, rad: %d\n", ctx->player_name, (int)ctx->x, (int)ctx->y, (int)ctx->rad);
		}
		}
		return TRUE;
	}
	return FALSE;
}

int main() {
	SetCurrentDirectoryW(L"../../static");
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	run(NULL);
}