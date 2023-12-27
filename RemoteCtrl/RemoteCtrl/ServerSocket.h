#pragma once

#include "pch.h"
#include "framework.h"

#pragma pack(push)
#pragma pack(1)
class CPacket
{
public:
	CPacket() : sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 4;
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
	CPacket(const BYTE* pData, size_t& nSize) {
		size_t i = 0;
		for (; i < nSize; i++) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				i += 2;	// 防止只有包头后面没有内容的特殊情况，此情况下i加后会大于nSize被返回
				break;
			}
		}
		if (i + 8 > nSize) {	// 包未完全接受到或包数据不全，解析失败返回
			// 8 为 (4 + 2 + 2) 有 lenth + command + sum, data 另外算
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;
		if (nLength + i > nSize) {	// 包未完全接受到，解析失败返回
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i);	i += 2;
		if (nLength > 4) {
			strData.resize(nLength - 4);
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);	
			i += nLength - 4;	// 始终让i指向校验和
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

	int Size() {	//包数据的大小
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
	WORD sHead;	// 包头，固定为FEFF
	DWORD nLength;	// 包长度
	WORD sCmd;	// 命令字
	std::string strData;	// 包数据
	WORD sSum;	// 和校验
	std::string strOut;	// 整个包的数据
};
#pragma pack(pop)

typedef struct MouseEvent{
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;	// 点击、移动、双击
	WORD nButton;	// 左键、右键、中键
	POINT ptXY;		// 鼠标坐标
}MOUSEEV,*PMOUSEEV;

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
		if (m_socket == -1)
		{
			return false;
		}
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(8973);
		if (bind(m_socket, (SOCKADDR*)&serv_addr, sizeof(serv_addr)) == -1)
		{
			return false;
		}
		if (listen(m_socket, 1) == -1)
		{
			return false;
		}
		return true;
	}
	bool AcceptClient() {
		TRACE("Enter AcceptClient\r\n");
		sockaddr_in client_addr;
		int client_size = sizeof(client_addr);
		m_client = accept(m_socket, (SOCKADDR*)&client_addr, &client_size);
		TRACE("m_client = %d\r\n", m_client);
		if (m_client == -1) return false;
		return true;
	}
#define BUFFER_SIZE 4096
	int DealCommand() {
		if (m_client == -1) {
			return -1;
		}
		char* buffer = new char[BUFFER_SIZE];
		if (buffer == NULL)	//防止创建失败
		{
			TRACE("内存不足，buffer创建失败！");
			delete[] buffer;	//释放buffer
			return -1;
		}
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;
		while (true) {
			size_t len = recv(m_client, buffer + index, BUFFER_SIZE - index, 0);
			if (len <= 0) {
				delete[] buffer;
				return -1;
			}
			TRACE("recv %d\r\n", len);
			index += len;
			len = index;
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				memmove(buffer, buffer + len, BUFFER_SIZE - len);
				index -= len;
				delete[] buffer;
				return m_packet.sCmd;
			}
		}
		delete[] buffer;
		return -1;
	}

	bool Send(const char* pData, int nSize) {
		if (m_client == INVALID_SOCKET) {
			return false;
		}
		return send(m_client, pData, nSize, 0) > 0;
		
	}
	bool Send(CPacket& pack) {
		if (m_client == INVALID_SOCKET) {
			return false;
		}
		return send(m_client, pack.Data(), pack.Size(), 0) > 0;
	}

	bool GetFilePath(std::string & strPath) {
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

	void CloseClient() {
		closesocket(m_client);
		m_client = INVALID_SOCKET;
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
			MessageBox(NULL, _T("无法初始化套接字，请检查网络设置！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
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
