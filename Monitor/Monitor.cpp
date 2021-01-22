#pragma comment (lib, "Ws2_32.lib")
#include <opencv2/opencv.hpp>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <sstream>

class TCPclient {
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	SOCKET ConnectSocket = INVALID_SOCKET;
	int recvbuflen = 2048;
	std::string address;
	WSADATA wsaData;
	int iResult;
	int port;

public:
	char data[2048];
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
	}

	TCPclient(std::string address, int port) {
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
	};
};

int main() {
	int dims[4];
	cv::Mat img;
	cv::namedWindow("img", cv::WINDOW_KEEPRATIO);
	TCPclient klijent("localhost", 69);

	std::chrono::high_resolution_clock::time_point pt = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; 1; i++) {
		if (i % 10 == 0) {
			SetConsoleTitleA(std::to_string(10 / (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - pt)).count()).c_str());
			pt = std::chrono::high_resolution_clock::now();
		}
		try {
			klijent.recv();
		}
		catch (...) {
			klijent.connect();
		}
		if (klijent.length != 16)
			continue;
		memcpy(dims, klijent.data, klijent.length);
		if (dims[3] != 6942069)
			continue;
		int size = dims[0] * dims[1] * dims[2];
		img.create(dims[0], dims[1], CV_8UC(dims[2]));
		for (int i = 0; i < size; i += klijent.length) {
			try {
				klijent.recv();
			}
			catch (std::exception) {
				klijent.connect();
			}
			memcpy(img.data + min(i, size - klijent.length), klijent.data, klijent.length);
		}

		imshow("img", img);
		cv::waitKey(2);
	}

	return 0;
}