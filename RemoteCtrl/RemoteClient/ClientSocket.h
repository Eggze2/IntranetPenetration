#pragma once

#include "pch.h"
#include "framework.h"
#include <string>
#include <vector>

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
	CPacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nLength + 4;
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else {
			strData.clear();
		}
		sSum = 0;
		for (size_t j = 0; j < strData.size(); j++) {
			sSum += BYTE(strData[j]) & 0xFF;
		}
	}
	CPacket(const CPacket& pack) {
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
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
			sum += BYTE(strData[j]) & 0xFF;
		}
		if (sum == sSum) {
			nSize = i;	// head2 length4 data...
			return;
		}
		nSize = 0;
	}
	~CPacket() {}
	CPacket& operator=(const CPacket& pack) {
		if (this != &pack) {
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
		}
		return *this;
	}
	int Size() {	//�����ݵĴ�С
		return nLength + 6;
	}
	const char* Data() {
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)(pData) = nLength; pData += 4;
		*(WORD*)pData = sCmd;	 pData += 2;
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;
		return strOut.c_str();
	}

public:
	WORD sHead;	// ��ͷ���̶�ΪFEFF
	DWORD nLength;	// ������
	WORD sCmd;	// ������
	std::string strData;	// ������
	WORD sSum;	// ��У��
	std::string strOut;	// ������������
};
#pragma pack(pop)

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;	// ������ƶ���˫��
	WORD nButton;	// ������Ҽ����м�
	POINT ptXY;		// �������
}MOUSEEV, * PMOUSEEV;

std::string GetErrorInfo(int wsaErrCode);

class CClientSocket
{
public:
	static CClientSocket* getInstance() {
		// ��̬����û��thisָ�룬�����޷����ʷǾ�̬��Ա����
		if (m_instance == NULL) {
			m_instance = new CClientSocket();
		}
		return m_instance;
	}
	bool InitSocket(const std::string& strIPAdress) {
		if (m_socket != INVALID_SOCKET) CloseSocket();
		m_socket = socket(PF_INET, SOCK_STREAM, 0);
		if (m_socket == INVALID_SOCKET)
		{
			wprintf(L"�׽��ִ���ʧ��\n");
			return false;
		}
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(strIPAdress.c_str());
		serv_addr.sin_port = htons(9527);
		if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
			AfxMessageBox("ָ����IP��ַ������!");
			return false;
		}
		int ret = connect(m_socket, (SOCKADDR*)&serv_addr, sizeof(serv_addr));
		if (ret == -1) {
			AfxMessageBox("����ʧ��!");
			TRACE(_T("����ʧ�ܣ�%d %s\r\n"), WSAGetLastError(), GetErrorInfo(WSAGetLastError()).c_str());
			return false;
		}
		return true;
	}
#define BUFFER_SIZE 4096
	int DealCommand() {
		if (m_socket == INVALID_SOCKET) return -1;
		char* buffer = m_buffer.data();
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;
		while (true) {
			size_t len = recv(m_socket, buffer + index, BUFFER_SIZE - index, 0);
			if (len <= 0) {
				return -1;
			}
			TRACE("recv %d\r\n", len);
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
		if (m_socket == INVALID_SOCKET) {
			return false;
		}
		return send(m_socket, pData, nSize, 0) > 0;

	}
	bool Send(CPacket& pack) {
		TRACE("m_socket = %d\r\n", m_socket);
		if (m_socket == INVALID_SOCKET) {
			return false;
		}
		return send(m_socket, pack.Data(), pack.Size(), 0) > 0;
	}

	bool GetFilePath(std::string& strPath) {
		if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}

	bool GetMouseEvent(MOUSEEV& mouse) {
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEV));
			return true;
		}
		return false;
	}

	CPacket& GetPacket() {
		return m_packet;
	}

	void CloseSocket() {
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
private:
	std::vector<char> m_buffer;
	SOCKET m_socket;
	CPacket m_packet;
	CClientSocket& operator=(const CClientSocket& server_socket) {}
	CClientSocket(const CClientSocket& server_socket) {
		m_socket = server_socket.m_socket;
	}
	CClientSocket() {
		if (InitSockEnv() == FALSE) {
			MessageBox(NULL, _T("InitSockEnv() failed! Please check your network settings!"), _T("Init failed!"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_buffer.resize(BUFFER_SIZE);
	}
	~CClientSocket() {
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
			CClientSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}
	static CClientSocket* m_instance;
	class CHelper {
	public:
		CHelper() {
			CClientSocket::getInstance();
		}
		~CHelper() {
			CClientSocket::releaseInstance();
		}
	};
	static CHelper m_helper;
};

extern CClientSocket server;