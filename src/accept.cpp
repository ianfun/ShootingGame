DWORD __stdcall RunIOCPLoop(LPVOID) {
	DWORD dwbytes = 0;
	IOCP* ctx = NULL;
	OVERLAPPED* ol = NULL;
	for (;;) {
		BOOL ret = GetQueuedCompletionStatus(iocp, &dwbytes, (PULONG_PTR)&ctx, &ol, INFINITE);
		if (ret == FALSE) {
			if (ctx) {
				if ((void*)ctx == (void*)pAcceptEx) {
					int err = WSAGetLastError();
					switch (err) {
					case ERROR_CONNECTION_ABORTED:
					case ERROR_NETNAME_DELETED:
					{
						goto DELETE_CLIENT;
					}break;
					default:
					{
						assert(0);
					}
					}
					accept_next();
					continue;
				}
				int err = WSAGetLastError();
				switch (err) {
				case ERROR_OPERATION_ABORTED:
				{
					/* CancelIoEx cancel WSARecv operation */
					/* Or thread exit! */
					log_info("ERROR_OPERATION_ABORTED! the WSARecv operation is aborted!");
				}break;
				case ERROR_CONNECTION_ABORTED:
				{
					printf("[error] ERROR_OPERATION_ABORTED at %p, CloseClient\n", ctx);
					CloseClient(ctx);
				}break;
				case ERROR_NETNAME_DELETED:
				{
					log_info("ERROR_NETNAME_DELETED: close client");
					if ((void*)ctx == (void*)pAcceptEx) {
						HeapFree(heap, 0, ctx);
						continue;
					}
					CancelIoEx((HANDLE)ctx->client, NULL);
					goto DELETE_CLIENT;
				}break;
				default:
					assert(0);
				}
			}
			continue;
		}
		if ((void*)ctx == (void*)pAcceptEx) {
			if (dwbytes != 0) {
				processRequest(acceptIOCP.currentCtx, dwbytes);
			}
			else {
				log_info("client error: empty request");
				CloseClient(acceptIOCP.currentCtx);
			}
			accept_next();
			continue;
		}
		if (ctx == NULL || ol == NULL) {
			if (ctx == (IOCP*)RunIOCPLoop) {
				printf("[info] exit thread (id=%u)\n", GetCurrentThreadId());
				return 0;
			}
			assert(0);
			continue;
		}
		if (dwbytes == 0) {
			if (ctx->state == State::AfterDisconnect)
			{
				DELETE_CLIENT:
				if (closesocket(ctx->client) != 0) {
					ctx->client = INVALID_SOCKET;
					assert(0);
				}
				if (ctx->hasp) {
					(&ctx->p)->~Parse_Data();
				}
				if (ctx->url) {
					HeapFree(heap, 0, (LPVOID)ctx->url);
					ctx->url = NULL;
				}
				if (ctx->hProcess && ctx->hProcess != INVALID_HANDLE_VALUE) {
					CloseHandle(ctx->hProcess);
					ctx->hProcess = NULL;
				}
				if (ctx->player_name) {
					LocalFree(ctx->player_name);
				}
				HeapFree(heap, 0, ctx);
			}
			else {
				CloseClient(ctx);
			}
			continue;
		}
		if (ol == &dumyOL) {
			continue;
		}
		processIOCP(ctx, ol, dwbytes);
	}
}
void accept_next() {
	while (1) {
		SOCKET client = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		BOOL bSuccess = SetHandleInformation((HANDLE)client, HANDLE_FLAG_INHERIT, 0);
		if (bSuccess == FALSE) {
			fatal("SetHandleInformation");
		}
		IOCP* ctx = (IOCP*)HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(IOCP));
		if (ctx == NULL) {
			fatal("HeapAlloc");
		}
		HANDLE bRet = CreateIoCompletionPort((HANDLE)client, iocp, (ULONG_PTR)ctx, N_THREADS);
		if (bRet == NULL) {
			fatal("CreateIoCompletionPort");
		}
		ctx->client = client;
		ctx->recvBuf->buf = ctx->buf;
		ctx->recvBuf->len = sizeof(ctx->buf);
		ctx->state = State::AfterRecv;
		ctx->firstCon = true;
		acceptIOCP.currentCtx = ctx;
		DWORD dwbytes;
		if (pAcceptEx(acceptIOCP.server, client, ctx->buf,
			sizeof(ctx->buf) - 64,
			32, 32,
			&dwbytes, &ctx->recvOL)) {
			processIOCP(ctx, &ctx->recvOL, dwbytes);
		}
		else {
			if (WSAGetLastError() == ERROR_IO_PENDING) {
				break;
			}
			else {
				assert(0);
				ExitThread(1);
			}
		}
	}
}