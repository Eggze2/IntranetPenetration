#pragma once

#include "pch.h"
#include "framework.h"

class CPacket
{
public:
	CPacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(const CPacket& packet) {
		sHead = packet.sHead;
		nLength = packet.nLength;
		sCmd = packet.sCmd;
		strData = packet.strData;
		sSum = packet.sSum;
	}
	CPacket(const BYTE* pData, size_t& nSize) {
		size_t i = 0;
		for (; i < nSize; i++) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				i += 2;
				break;
			}
		}
		if (i + 4 + 2 + 2 > nSize) {	// ��δ��ȫ���ܵ�������ݲ�ȫ������ʧ�ܷ���
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {	// ��δ��ȫ���ܵ�������ʧ�ܷ���
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i);	i += 2;
		if (nLength > 4) {
			strData.resize(nLength - 2 - 2);
			memcpy((void*)strData.c_str(), pData + i, nLength - 2 - 2);	
			i += nLength - 2 - 2;	// ʼ����iָ��У���
		}
		sSum = *(WORD*)(pData + i);	i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sum += BYTE(strData[i]) & 0xFF;
		}
		if (sum == sSum) {
			nSize = i;	// head2 length4 data...
			return;
		}
		nSize = 0;
	}
	~CPacket() {}
	CPacket& operator=(const CPacket& packet) {
		if (this != &packet) {
			sHead = packet.sHead;
			nLength = packet.nLength;
			sCmd = packet.sCmd;
			strData = packet.strData;
			sSum = packet.sSum;
		}
		return *this;
	}
public:
	WORD sHead;	// ��ͷ���̶�ΪFEFF
	DWORD nLength;	// ������
	WORD sCmd;	// ������
	std::string strData;	// ������
	WORD sSum;	// ��У��

};


class CServerSocket
{
public:
	static CServerSocket* getInstance() {
		// ��̬����û��thisָ�룬�����޷����ʷǾ�̬��Ա����
		if (m_instance == NULL) {
			m_instance = new CServerSocket();
		}
		return m_instance;
	}
	bool InitSocket() {
		if (m_socket == INVALID_SOCKET)
		{
			wprintf(L"�׽��ִ���ʧ��\n");
			return false;
		}
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		serv_addr.sin_port = htons(9527);
		if (bind(m_socket, (SOCKADDR*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
		{
			wprintf(L"��ʧ��\n");
			return false;
		}
		if (listen(m_socket, 1) == SOCKET_ERROR)
		{
			wprintf(L"����ʧ��\n");
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
			wprintf(L"�ͻ�������ʧ��\n");
			return false;
		}
		return true;
	}
#define BUFFER_SIZE 4096
	int DealCommand() {
		if (m_client == INVALID_SOCKET) {
			return -1;
		}
		char* buffer = new char[BUFFER_SIZE];
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;
		while (true) {
			size_t len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);
			if (len <= 0) {
				return -1;
			}
			index += len;
			len = index;
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				return m_packet.sCmd;
			}
		}
		return -1;
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
	CPacket m_packet;
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
