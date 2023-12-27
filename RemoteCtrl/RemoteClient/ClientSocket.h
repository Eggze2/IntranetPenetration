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
				i += 2;
				break;
			}
		}
		if (i + 8 > nSize) {	// 包未完全接受到或包数据不全，解析失败返回
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); 
		i += 4;
		if (nLength + i > nSize) {	// 包未完全接受到，解析失败返回
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i); 
		i += 2;
		if (nLength > 4) {
			strData.resize(nLength - 4);
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			i += nLength - 4;	// 始终让i指向校验和
		}
		sSum = *(WORD*)(pData + i); 
		i += 2;
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
		*(WORD*)pData = sHead; 
		pData += 2;
		*(DWORD*)(pData) = nLength; 
		pData += 4;
		*(WORD*)pData = sCmd;	 
		pData += 2;
		memcpy(pData, strData.c_str(), strData.size());
		pData += strData.size();
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

typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction;	// 点击、移动、双击
	WORD nButton;	// 左键、右键、中键
	POINT ptXY;		// 鼠标坐标
}MOUSEEV, * PMOUSEEV;

class CClientSocket
{
public:
	static CClientSocket* getInstance() {
		// 静态函数没有this指针，所以无法访问非静态成员变量
		if (m_instance == NULL) {
			m_instance = new CClientSocket();
		}
		return m_instance;
	}

	std::string GetErrInfo(int wsaErrCode);

	bool InitSocket(int nIP, int nPort) {
		if (m_socket != INVALID_SOCKET) CloseSocket();
		m_socket = socket(PF_INET, SOCK_STREAM, 0);
		if (m_socket == -1) {
			return false;
		}
		sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), nIP);
		serv_addr.sin_addr.s_addr = htonl(nIP);
		serv_addr.sin_port = htons(nPort);
		if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
			AfxMessageBox("指定的IP地址不存在!");
			return false;
		}
		int ret = connect(m_socket, (SOCKADDR*)&serv_addr, sizeof(serv_addr));
		if (ret == -1) {
			AfxMessageBox("连接失败!");
			TRACE(_T("连接失败：%d %s\r\n"), WSAGetLastError(), GetErrInfo(WSAGetLastError()).c_str());
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
