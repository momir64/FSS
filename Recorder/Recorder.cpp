#pragma comment (lib, "Ws2_32.lib")
#include <opencv2/opencv.hpp>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <ctime>

#define CUBE 16

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
cv::Mat screenshot;
CURSORINFO cursor;
Monitori monitori;
HBITMAP hbwindow;
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
	SOCKET ListenSocket = INVALID_SOCKET, ClientSocket = INVALID_SOCKET;
	struct addrinfo *result = NULL, hints;
	int recvbuflen = 32, iResult, port;
	char recvbuf[32];
	WSADATA wsaData;
	u_long mode = 1;

public:
	char data[400000];
	int length = 0;
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
		int dims[] = { h, w };
		send((char *)dims, sizeof(dims));
	}

	TCPserver(int port) {
		if (WSAStartup(MAKEWORD(2, 2), &wsaData))
			throw "WSAStartup error!";
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = AF_INET;
		this->port = port;
		connect();
	}

	bool send(const char *data, int length) {
		if (::send(ClientSocket, data, length, 0) == SOCKET_ERROR) {
			if (shutdown(ClientSocket, SD_SEND) == SOCKET_ERROR) {
				closesocket(ClientSocket);
				WSACleanup();
				throw "shutdown failed with error: " + std::to_string(WSAGetLastError());
			}
			closesocket(ClientSocket);
			connect();
			return true;
			//throw "send failed with error: " + std::to_string(WSAGetLastError());
		}
		return false;
	}

	char *recv() {
		length = ::recv(ClientSocket, data, recvbuflen, 0);
		if (length == 0)
			throw "Connection closed!";
		else if (length < 0)
			throw "recv failed with error: " + std::to_string(WSAGetLastError());
		return data;
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

char paket[400000];
cv::UMat umat1, umat2;
std::vector<cv::Mat> slojevi(3);
std::vector<cv::Mat> oldslojevi(3);
std::vector<cv::UMat> uslojevi1(3);
std::vector<cv::UMat> uslojevi2(3);
int getPaket(int xx, int yy, int flag = 0, int ssize = CUBE) {
	bool same = true;
	int size[3], x[3], y[3], sizex[3], sizey[3], size1[3];
	for (int i = 0; i < 3; i++) {
		size[i] = i ? ssize / 2 : ssize;
		x[i] = i ? xx / 2 : xx;
		y[i] = i ? yy / 2 : yy;
		sizex[i] = min(size[i], slojevi[i].cols - x[i]);
		sizey[i] = min(size[i], slojevi[i].rows - y[i]);
		size1[i] = y[i] + sizey[i];
	}
	for (int i = 0; i < 3; i++) {
		for (int j = y[i]; j < size1[i]; j++) {
			int size2 = j * slojevi[i].cols + x[i] + sizex[i];
			for (int z = size2 - sizex[i]; z < size2; z++)
				if (slojevi[i].data[z] != oldslojevi[i].data[z]) {
					same = false;
					break;
				}
			if (!same)
				break;
		}
	}
	int dims[] = { xx, yy, ssize , flag };
	memcpy(paket, dims, sizeof(dims));
	int last = sizeof(dims);
	if (same && flag != 'P')
		return 0;
	for (int i = 0; i < 3; i++)
		for (int j = y[i]; j < size1[i]; j++) {
			memcpy(paket + last, slojevi[i].data + (j * slojevi[i].cols + x[i]), sizex[i]);
			last += sizex[i];
		}
	return last;
}

void obradi_ss() {
	screenshot.copyTo(umat2);
	resize(umat2, umat1, cv::Size(), 0.75, 0.75);
	cvtColor(umat1, umat2, cv::COLOR_BGR2YCrCb, 3);
	cv::split(umat2, uslojevi1);
	resize(uslojevi1[1], uslojevi2[1], cv::Size(), 0.5, 0.5);
	resize(uslojevi1[2], uslojevi2[2], cv::Size(), 0.5, 0.5);
	uslojevi1[0].copyTo(slojevi[0]);
	uslojevi2[1].copyTo(slojevi[1]);
	uslojevi2[2].copyTo(slojevi[2]);
}

int main() {
	mid = 2;
	setup_ss();
	TCPserver server(69);

	//cv::namedWindow("img", cv::WINDOW_FREERATIO);
	slojevi[0] = cv::Mat::zeros(cv::Size(w, h), CV_8UC1);
	slojevi[1] = cv::Mat::zeros(cv::Size(sw, sh), CV_8UC1);
	slojevi[2] = cv::Mat::zeros(cv::Size(sw, sh), CV_8UC1);
	std::chrono::high_resolution_clock::time_point pt = std::chrono::high_resolution_clock::now();

	for (size_t i = 0; 1; i++) {
		if (i % 30 == 0) {
			SetConsoleTitleA(std::to_string(30 / (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - pt)).count()).c_str());
			pt = std::chrono::high_resolution_clock::now();
		}

		for (int i = 0; i < 3; i++)
			slojevi[i].copyTo(oldslojevi[i]);
		take_screenshot();
		obradi_ss();

		bool skip = false;
		for (int i = 0; i < sh; i += CUBE) {
			for (int j = 0; j < sw; j += CUBE) {
				char flag = i + CUBE >= sh && j + CUBE >= sw ? 'P' : 'K';
				int size = getPaket(j, i, flag);
				if (!size) continue;
				//if(flag == 'P')
				//	std::cout << "P";
				if (server.send(paket, size)) {
					skip = true;
					break;
				}
				server.recv();
				//std::cout << flag << server.recv();
			}
			if (skip) break;
		}



		////resize(slojevi[1], c1, cv::Size(), 2, 2);
		////resize(slojevi[2], c2, cv::Size(), 2, 2);
		////slojevi[1] = c1;
		////slojevi[2] = c2;
		////merge(slojevi, pom);
		//cvtColor(upom, pom, cv::COLOR_YCrCb2BGR, 3);

		//cvtColor(screenshot, pom, cv::COLOR_BGR2GRAY);


		//int dims[] = { pom.rows , pom.cols , pom.channels() , 6942069 };
		//int size = dims[0] * dims[1] * dims[2];
		//uchar dimsc[sizeof(dims)];
		//memcpy(dimsc, dims, sizeof(dims));
		//if (server.send(dimsc, sizeof(dims)))
		//	continue;;
		//for (int i = 0; i < size; i += 1024) {
		//	if (server.send(pom.data + i, min(size - i, 1024)))
		//		break;
		//}

		//std::cout << pom.rows * pom.cols * 3 << std::endl;

		//imshow("img", umat1);
		//cv::waitKey(2);
	}
}