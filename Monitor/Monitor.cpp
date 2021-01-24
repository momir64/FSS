#pragma comment (lib, "Ws2_32.lib")
#include <opencv2/opencv.hpp>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <sstream>

int PASIZE = 1400;
float cx = 0.75;
float cy = 0.75;
int c = 3;

int h, w;

class TCPclient {
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	SOCKET ConnectSocket = INVALID_SOCKET;
	std::string address;
	WSADATA wsaData;
	int recvbuflen;
	int iResult;
	int port;

public:
	char *data;
	int length = 0;
	void connect() {
		if (getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &result)) {
			WSACleanup();
			throw "getaddrinfo failed with error: " + std::to_string(iResult);
		}

		for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
			ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
			if (ConnectSocket == INVALID_SOCKET) {
				WSACleanup();
				throw "getaddrinfo failed with error: " + std::to_string(WSAGetLastError());
			}
			if (::connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
				closesocket(ConnectSocket);
				ConnectSocket = INVALID_SOCKET;
				continue;
			}
			break;
		}

		freeaddrinfo(result);

		if (ConnectSocket == INVALID_SOCKET) {
			WSACleanup();
			throw "Unable to connect to server!";
		}

		float dimsf[] = { cx, cy };
		int dimsi[] = { PASIZE, c };
		memcpy(data, dimsi, sizeof(dimsi));
		memcpy(data + sizeof(dimsi), dimsf, sizeof(dimsf));
		send(data, sizeof(dimsi) + sizeof(dimsf));

		recv();
		memcpy(dimsi, data, sizeof(dimsi));
		h = dimsi[0];
		w = dimsi[1];
	}

	TCPclient(std::string address, int port) {
		recvbuflen = PASIZE;
		data = new char[PASIZE];
		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
			throw "WSAStartup failed with error: " + std::to_string(iResult);

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		this->address = address;
		this->port = port;
		connect();
	}

	void send(const char *data, int length) {
		if (::send(ConnectSocket, (const char *)data, length, 0) == SOCKET_ERROR) {
			if (shutdown(ConnectSocket, SD_SEND) == SOCKET_ERROR) {
				closesocket(ConnectSocket);
				WSACleanup();
				throw "shutdown failed with error: " + std::to_string(WSAGetLastError());
			}
			closesocket(ConnectSocket);
			connect();
			//throw "send failed with error: " + std::to_string(WSAGetLastError());
		}
	}

	char *recv() {
		length = ::recv(ConnectSocket, data, recvbuflen, 0);
		if (length == 0)
			throw "Connection closed!";
		else if (length < 0)
			throw "recv failed with error: " + std::to_string(WSAGetLastError());
		return data;
	}

	~TCPclient() {
		if (shutdown(ConnectSocket, SD_SEND) == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			WSACleanup();
			throw "shutdown failed with error: " + std::to_string(WSAGetLastError());
		}
		closesocket(ConnectSocket);
		WSACleanup();
		delete[] data;
	};
};

int main() {
	cv::namedWindow("img", cv::WINDOW_KEEPRATIO);
	TCPclient klijent("localhost", 69);
	cv::Mat img(h, w, CV_8UC(c));
	int size = h * w * c;

	std::chrono::high_resolution_clock::time_point pt = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; 1; i++) {
		if (i % 10 == 0) {
			SetConsoleTitleA(std::to_string(10 / (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - pt)).count()).c_str());
			pt = std::chrono::high_resolution_clock::now();
		}

		klijent.recv();
		if (std::string(klijent.data, klijent.length) != "START")
			continue;
		for (int i = 0; i < size; i += klijent.length) {
			klijent.recv();
			memcpy(img.data + min(i, size - klijent.length), klijent.data, klijent.length);
		}

		imshow("img", img);
		cv::waitKey(1);
	}

	return 0;
}