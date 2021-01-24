#pragma comment (lib, "Ws2_32.lib")
#include <opencv2/opencv.hpp>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <ctime>

#define PASIZE 1400

class Monitori {
	class Monitor {
	public:
		int x;
		int y;
		int w;
		int h;
		Monitor(int x, int y, int w, int h) {
			this->x = x;
			this->y = y;
			this->w = w;
			this->h = h;
		}
	};
	std::vector<Monitor> monitori;
	static BOOL CALLBACK MonitorEnum(HMONITOR, HDC, LPRECT lprcMonitor, LPARAM pData) {
		((Monitori *)pData)->monitori.push_back(Monitor((*lprcMonitor).left, (*lprcMonitor).top, abs((*lprcMonitor).right - (*lprcMonitor).left), abs((*lprcMonitor).bottom - (*lprcMonitor).top)));
		return TRUE;
	}
public:
	Monitori() {
		monitori.push_back(Monitor(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)));
		EnumDisplayMonitors(0, 0, MonitorEnum, (LPARAM)this);
	}
	Monitor operator[](int index) {
		return monitori[index];
	}
	int count() {
		return monitori.size();
	}
};

HDC hwindowDC = GetDC(NULL), hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);
ICONINFOEXW info = { sizeof(info) };
BITMAP bmpCursor = { 0 };
BITMAPINFOHEADER bi;
int monitorid = -1;
CURSORINFO cursor;
Monitori monitori;
HBITMAP hbwindow;
cv::Mat screenshot;
int w, h, x, y, sw, sh;
int mid;

void setup_ss() {
	w = monitori[mid].w;
	h = monitori[mid].h;
	x = monitori[mid].x;
	y = monitori[mid].y;
	sw = 0.75 * w;
	sh = 0.75 * h;
	hbwindow = CreateCompatibleBitmap(hwindowDC, w, h);
	SelectObject(hwindowCompatibleDC, hbwindow);
	bi.biSize = sizeof(BITMAPINFOHEADER);
	screenshot.create(h, w, CV_8UC3);
	bi.biYPelsPerMeter = 0;
	bi.biXPelsPerMeter = 0;
	bi.biClrImportant = 0;
	bi.biCompression = 0L;
	bi.biSizeImage = 0;
	bi.biBitCount = 24;
	bi.biClrUsed = 0;
	bi.biHeight = -h;
	bi.biPlanes = 1;
	bi.biWidth = w;
}

void add_cursor() {
	cursor = { sizeof(cursor) };
	GetCursorInfo(&cursor);
	if (cursor.flags == CURSOR_SHOWING) {
		GetIconInfoExW(cursor.hCursor, &info);
		GetObjectA(info.hbmColor, sizeof(bmpCursor), &bmpCursor);
		DrawIconEx(hwindowCompatibleDC, cursor.ptScreenPos.x - monitori[0].x - x - info.xHotspot, cursor.ptScreenPos.y - monitori[0].y - y - info.yHotspot, cursor.hCursor, bmpCursor.bmWidth, bmpCursor.bmHeight, 0, NULL, DI_NORMAL);
	}
}

void take_screenshot() {
	BitBlt(hwindowCompatibleDC, 0, 0, w, h, hwindowDC, x, y, SRCCOPY);
	add_cursor();
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, h, screenshot.data, (BITMAPINFO *)&bi, 0);
}

class TCPserver {
	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL;
	struct addrinfo hints;
	int recvbuflen = 32;
	struct fd_set fds;
	char recvbuf[32];
	WSADATA wsaData;
	u_long mode = 1;
	int iResult;
	int port;

public:
	void connect() {
		if (getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &result)) {
			WSACleanup();
			throw "getaddrinfo error!";
		}
		ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			freeaddrinfo(result);
			WSACleanup();
			throw "socket failed with error: " + std::to_string(WSAGetLastError());
		}
		if (bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
			freeaddrinfo(result);
			closesocket(ListenSocket);
			WSACleanup();
			throw "bind failed with error: " + std::to_string(WSAGetLastError());
		}
		freeaddrinfo(result);
		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
			closesocket(ListenSocket);
			WSACleanup();
			throw "listen failed with error: " + std::to_string(WSAGetLastError());
		}
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			closesocket(ListenSocket);
			WSACleanup();
			throw "accept failed with error: " + std::to_string(WSAGetLastError());
		}
		closesocket(ListenSocket);
		int dims[] = { sh, sw , 3 };
		send((char *)dims, sizeof(dims));
	}

	TCPserver(int port) {
		if (WSAStartup(MAKEWORD(2, 2), &wsaData))
			throw "WSAStartup error!";
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;
		this->port = port;
		connect();
	}

	void send(const char *data, int length) {
		if (::send(ClientSocket, (const char *)data, length, 0) == SOCKET_ERROR) {
			if (shutdown(ClientSocket, SD_SEND) == SOCKET_ERROR) {
				closesocket(ClientSocket);
				WSACleanup();
				throw "shutdown failed with error: " + std::to_string(WSAGetLastError());
			}
			closesocket(ClientSocket);
			connect();
			//throw "send failed with error: " + std::to_string(WSAGetLastError());
		}
	}

	~TCPserver() {
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			closesocket(ClientSocket);
			WSACleanup();
			throw "shutdown failed with error: " + std::to_string(WSAGetLastError());
		}
	};
};


int main() {
	mid = 2;
	setup_ss();
	TCPserver server(69);

	cv::Mat pom;
	cv::UMat upom1, upom2;
	std::chrono::high_resolution_clock::time_point pt = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; 1; i++) {
		if (i % 10 == 0) {
			SetConsoleTitleA(std::to_string(10 / (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - pt)).count()).c_str());
			pt = std::chrono::high_resolution_clock::now();
		}

		take_screenshot();

		resize(screenshot.getUMat(cv::ACCESS_FAST), upom1, cv::Size(), 0.75, 0.75);
		//cvtColor(upom1, upom2, cv::COLOR_BGR2GRAY);
		upom1.copyTo(pom);

		//int dims[] = { pom.rows , pom.cols , pom.channels() };
		int size = sw * sh * 3;
		server.send("START", 5);
		for (int i = 0; i < size; i += PASIZE)
			server.send((char *)pom.data + i, min(size - i, PASIZE));

	}
}