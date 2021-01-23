#pragma comment (lib, "Ws2_32.lib")
#include <opencv2/opencv.hpp>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <iostream>
#include <sstream>

#define CK 10.0

std::vector<cv::UMat> uslojevi1(3);
std::vector<cv::UMat> uslojevi2(3);
std::vector<cv::Mat> slojevi(3);
cv::UMat umat1, umat2;
int h, w, sh, sw, PASIZE;

class TCPclient {
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	SOCKET ConnectSocket = INVALID_SOCKET;
	int recvbuflen = 400000;
	std::string address;
	WSADATA wsaData;
	int iResult;
	int port;

public:
	char data[400000];
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

		int dims[3];
		memcpy(dims, recv(), sizeof(dims));
		h = dims[0];
		w = dims[1];
		sh = h * 0.75;
		sw = w * 0.75;
		PASIZE = dims[2];
		slojevi[0] = cv::Mat::zeros(cv::Size(sw, sh), CV_8UC1);
		slojevi[1] = cv::Mat::zeros(cv::Size(sw / CK, sh / CK), CV_8UC1);
		slojevi[2] = cv::Mat::zeros(cv::Size(sw / CK, sh / CK), CV_8UC1);
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

	bool send(const char *data, int length) {
		if (::send(ConnectSocket, data, length, 0) == SOCKET_ERROR) {
			if (shutdown(ConnectSocket, SD_SEND) == SOCKET_ERROR) {
				closesocket(ConnectSocket);
				WSACleanup();
				throw "shutdown failed with error: " + std::to_string(WSAGetLastError());
			}
			closesocket(ConnectSocket);
			connect();
			return true;
			//throw "send failed with error: " + std::to_string(WSAGetLastError());
		}
		return false;
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

int getPaket(char *data) {
	int dims[4];
	memcpy(dims, data, sizeof(dims));
	int xx = dims[0];
	int yy = dims[1];
	int ssize = dims[2];
	int flag = dims[3];
	if (flag != 'K' && flag != 'P')
		return flag;
	int size[3], x[3], y[3], sizex[3], sizey[3], size1[3];
	for (int i = 0; i < 3; i++) {
		size[i] = i ? ssize / 2 : ssize;
		x[i] = i ? xx / 2 : xx;
		y[i] = i ? yy / 2 : yy;
		sizex[i] = min(size[i], slojevi[i].cols - x[i]);
		sizey[i] = min(size[i], slojevi[i].rows - y[i]);
		size1[i] = y[i] + sizey[i];
	}

	int last = sizeof(dims);
	for (int i = 0; i < 3; i++)
		for (int j = y[i]; j < size1[i]; j++) {
			memcpy(slojevi[i].data + (j * slojevi[i].cols + x[i]), data + last, sizex[i]);
			last += sizex[i];
		}

	return flag;
}

int main() {
	TCPclient klijent("localhost", 69);
	//cv::namedWindow("img", cv::WINDOW_KEEPRATIO);

	std::chrono::high_resolution_clock::time_point pt = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; 1; i++) {
		klijent.recv();
		if (klijent.data != std::string("START"))
			continue;
		klijent.send("OK", 2);

		int size[3] = { sw * sh, sw * sh / CK / CK, sw * sh / CK / CK };
		for (int i = 0; i < 3; i++) {
			for (int z = 0; z < size[i]; z += PASIZE) {
				klijent.recv();
				klijent.send("OK", 2);
				memcpy((char *)slojevi[i].data + z, klijent.data, min(size[i] - z, PASIZE));
			}
		}

		slojevi[0].copyTo(uslojevi2[0]);
		slojevi[1].copyTo(uslojevi1[1]);
		slojevi[2].copyTo(uslojevi1[2]);

		resize(uslojevi1[1], uslojevi2[1], cv::Size(), CK, CK);
		resize(uslojevi1[2], uslojevi2[2], cv::Size(), CK, CK);
		merge(uslojevi2, umat1);
		cvtColor(umat1, umat2, cv::COLOR_YCrCb2BGR, 3);

		imshow("img", umat2);
		cv::waitKey(1);
		if (i % 5 == 0) {
			SetConsoleTitleA(std::to_string(5 / (std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - pt)).count()).c_str());
			pt = std::chrono::high_resolution_clock::now();
		}
	}

	return 0;
}