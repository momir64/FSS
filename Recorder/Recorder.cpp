#pragma comment (lib, "Ws2_32.lib")
#include <opencv2/opencv.hpp>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <ctime>

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
int w, h, x, y;

void take_screenshot(int id) {
	if (monitorid != id) {
		monitorid = id;
		w = monitori[id].w;
		h = monitori[id].h;
		x = monitori[id].x;
		y = monitori[id].y;
		hbwindow = CreateCompatibleBitmap(hwindowDC, w, h);
		screenshot.create(h, w, CV_8UC3);
		bi.biSize = sizeof(BITMAPINFOHEADER);
		bi.biWidth = w;
		bi.biHeight = -h;
		bi.biPlanes = 1;
		bi.biBitCount = 24;
		bi.biCompression = 0L;
		bi.biSizeImage = 0;
		bi.biXPelsPerMeter = 0;
		bi.biYPelsPerMeter = 0;
		bi.biClrUsed = 0;
		bi.biClrImportant = 0;
		SelectObject(hwindowCompatibleDC, hbwindow);
	}
	BitBlt(hwindowCompatibleDC, 0, 0, w, h, hwindowDC, x, y, SRCCOPY);
	cursor = { sizeof(cursor) };
	GetCursorInfo(&cursor);
	if (cursor.flags == CURSOR_SHOWING) {
		GetIconInfoExW(cursor.hCursor, &info);
		GetObjectA(info.hbmColor, sizeof(bmpCursor), &bmpCursor);
		DrawIconEx(hwindowCompatibleDC, cursor.ptScreenPos.x - monitori[0].x - x - info.xHotspot, cursor.ptScreenPos.y - monitori[0].y - y - info.yHotspot, cursor.hCursor, bmpCursor.bmWidth, bmpCursor.bmHeight, 0, NULL, DI_NORMAL);
	}
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, h, screenshot.data, (BITMAPINFO *)&bi, 0);
}

char colormat[300] = {
36,28,237,36,28,237,76,177,34,76,177,34,232,162,0,232,162,0,21,0,136,21,0,136,21,0,136,21,0,136,
39,127,255,76,177,34,76,177,34,232,162,0,232,162,0,14,201,255,21,0,136,29,230,181,29,230,181,29,230,181,
39,127,255,76,177,34,36,28,237,232,162,0,21,0,136,14,201,255,14,201,255,14,201,255,234,217,153,21,0,136,
39,127,255,76,177,34,21,0,136,21,0,136,21,0,136,176,228,239,201,174,255,14,201,255,14,201,255,76,177,34,
39,127,255,76,177,34,21,0,136,201,174,255,201,174,255,201,174,255,201,174,255,232,162,0,14,201,255,76,177,34,
39,127,255,21,0,136,201,174,255,232,162,0,36,28,237,36,28,237,36,28,237,232,162,0,14,201,255,14,201,255,
39,127,255,21,0,136,232,162,0,29,230,181,36,28,237,36,28,237,36,28,237,232,162,0,234,217,153,14,201,255,
39,127,255,21,0,136,232,162,0,36,28,237,36,28,237,176,228,239,76,177,34,232,162,0,234,217,153,14,201,255,
39,127,255,36,28,237,36,28,237,36,28,237,176,228,239,76,177,34,76,177,34,36,28,237,76,177,34,14,201,255,
39,127,255,36,28,237,176,228,239,190,146,112,190,146,112,190,146,112,190,146,112,190,146,112,190,146,112,14,201,255 };

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
		//if (ioctlsocket(ClientSocket, FIONBIO, &mode) == INVALID_SOCKET) {
		//	WSACleanup();
		//	throw "ioctlsocket failed with error: " + std::to_string(WSAGetLastError());
		//}
		//FD_ZERO(&fds);
		//FD_SET(ClientSocket, &fds);
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

	void send(const uchar *data, int length) {
		/*iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult <= 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
			if (shutdown(ClientSocket, SD_SEND) == SOCKET_ERROR) {
				closesocket(ClientSocket);
				WSACleanup();
				throw "shutdown failed with error: " + std::to_string(WSAGetLastError());
			}
			closesocket(ClientSocket);
			connect();
		}*/
		//select(0, 0, &fds, 0, 0);
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
	cv::Mat img(10, 10, CV_8UC3, colormat);

	cv::Mat pom;
	TCPserver server(69);
	cv::UMat upom, c1, c2;
	std::vector<cv::UMat> slojevi(3);
	std::chrono::high_resolution_clock::time_point pt = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; 1; i++) {
		if (i % 10 == 0) {
			SetConsoleTitleA(std::to_string(10 / (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - pt)).count()).c_str());
			pt = std::chrono::high_resolution_clock::now();
		}

		take_screenshot(2);

		//cvtColor(screenshot, upom, cv::COLOR_BGR2YCrCb, 3);
		//split(upom, slojevi);
		//resize(slojevi[1], c1, cv::Size(), 0.5, 0.5);
		//resize(slojevi[2], c2, cv::Size(), 0.5, 0.5);

		//resize(c1, slojevi[1], cv::Size(), 2, 2);
		//resize(c2, slojevi[2], cv::Size(), 2, 2);
		//merge(slojevi, upom);
		//cvtColor(upom, pom, cv::COLOR_YCrCb2BGR, 3);

		resize(screenshot, pom, cv::Size(), 0.75, 0.75);
		//cvtColor(screenshot, pom, cv::COLOR_BGR2GRAY);

		
		int dims[] = { pom.rows , pom.cols , pom.channels() , 6942069};
		int size = dims[0] * dims[1] * dims[2];
		uchar dimsc[16];
		memcpy(dimsc, dims, 16);
		server.send(dimsc, 16);
		for (int i = 0; i < size; i += 1024) {
			server.send(pom.data + i, min(size - i, 1024));
		}

		//std::cout << pom.rows * pom.cols * 3 << std::endl;
		//imshow("img", pom);
		//cv::waitKey(2);
	}
}