#pragma once

#include "pch.h"
#include "framework.h"


class CServerSocket
{
public:
	static CServerSocket* getInstance() {
		// 静态函数没有this指针，所以无法访问非静态成员变量
		if (m_instance == NULL) {
			m_instance = new CServerSocket();
		}
		return m_instance;
	}
	bool InitSocket() {
		if (m_socket == INVALID_SOCKET)
		{
			wprintf(L"套接字创建失败\n");
			return false;
		}
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		serv_addr.sin_port = htons(1234);
		if (bind(m_socket, (SOCKADDR*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
		{
			wprintf(L"绑定失败\n");
			return false;
		}
		if (listen(m_socket, 1) == SOCKET_ERROR)
		{
			wprintf(L"监听失败\n");
			return false;
		}
		return true;
	}
	bool AcceptClient() {
		sockaddr_in client_addr;
		int client_size = sizeof(client_addr);
		m_client = accept(m_socket, (SOCKADDR*)&client_addr, &client_size);
		if (m_client == INVALID_SOCKET)
		{
			wprintf(L"客户端连接失败\n");
			return false;
		}
		return true;
	}

	int DealCommand() {
		if (m_client == INVALID_SOCKET) {
			return false;
		}
		char buffer[1024];
		while (true) {
			int len = recv(m_client, buffer, sizeof(buffer), 0);
			if (len <= 0) {
				return -1;
			}
			//TODO: deal command
		}
	}

	bool Send(const char* pData, int nSize) {
		if (m_client == INVALID_SOCKET) {
			return false;
		}
		return send(m_client, pData, nSize, 0) > 0;
		
	}
private:
	SOCKET m_client;
	SOCKET m_socket;
	CServerSocket& operator=(const CServerSocket& server_socket) {}
	CServerSocket(const CServerSocket& server_socket) {
		m_socket = server_socket.m_socket;
		m_client = server_socket.m_client;
	}
	CServerSocket() {
		m_client = INVALID_SOCKET;
		if (InitSockEnv() == FALSE) {
			MessageBox(NULL, _T("InitSockEnv() failed! Please check your network settings!"), _T("Init failed!"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_socket = socket(PF_INET, SOCK_STREAM, 0);
	}
	~CServerSocket() {
		closesocket(m_socket);
		WSACleanup();
	}
	BOOL InitSockEnv() {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
			return FALSE;
		}
		return TRUE;
	}
	static void releaseInstance() {
		if (m_instance != NULL) {
			CServerSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}
	static CServerSocket* m_instance;
	class CHelper {
	public:
		CHelper() {
			CServerSocket::getInstance();
		}
		~CHelper() {
			CServerSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
};

extern CServerSocket server;
