#include <iostream>
#include <winsock2.h>
#include <vector>
#include <cmath>
#include <thread>

enum Status {
    UNKNOWN = 0x06,
    IN_PROGRESS = 0x07,
    COMPLETED = 0x08,
    ERR = 0x09
};

enum Command {
    START = 0x01,
    STATUS = 0x02,
    RESULT = 0x03,
};

class TCPServer {
private:
    WSADATA wsaData;
    SOCKET serverSocket;
    SOCKADDR_IN serverAddr;
    int port;

public:
    TCPServer(int port) : port(port) {}

    bool initialize() {
        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed.\n";
            return false;
        }

        // Create socket
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed.\n";
            WSACleanup();
            return false;
        }

        // Bind socket
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serverAddr.sin_port = htons(port);
        if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed.\n";
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }

        // Listen
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed.\n";
            closesocket(serverSocket);
            WSACleanup();
            return false;
        }

        return true;
    }

    void acceptConnections() {
        while (true) {
            // Accept connection
            SOCKET clientSocket = accept(serverSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed.\n";
                continue;
            }

            // Create a new thread or handle asynchronously
            std::thread clientThread(&TCPServer::handleClient, this, clientSocket);
            clientThread.detach(); // Detach the thread to allow it to run independently
        }
    }

    void sendMatrix(const std::vector<std::vector<int>>& matrix, SOCKET& clientSocket) {
        std::vector<char> dataToSend;
        for (const auto& row : matrix) {
            for (int value : row) {
                dataToSend.push_back(static_cast<char>(value));
            }
        }
        int dataSize = dataToSend.size();
        send(clientSocket, reinterpret_cast<const char*>(&dataSize), sizeof(int), 0);
        send(clientSocket, dataToSend.data(), dataSize, 0);
    }

    std::vector<std::vector<int>> receiveMatrix(SOCKET& clientSocket) {
        int dataSize;
        recv(clientSocket, reinterpret_cast<char*>(&dataSize), sizeof(int), 0);
        std::vector<char> receivedData(dataSize);
        int totalBytesReceived = 0;
        int attempt = 0;
        while (totalBytesReceived < dataSize) {
            int bytesReceived = recv(clientSocket, receivedData.data() + totalBytesReceived, dataSize - totalBytesReceived, 0);
            if (bytesReceived == SOCKET_ERROR) {
                if(attempt > 5){
                    std::cerr << "Error in recv! Quitting" << std::endl;
                    closesocket(clientSocket);
                    WSACleanup();
                    exit(-1);
                }
                std::cerr << "Error in recv! Retrying..." << std::endl;
                attempt++;
                continue;
            }
            attempt = 0;
            totalBytesReceived += bytesReceived;
        }

        int m = static_cast<int>(sqrt(dataSize));
        std::cout << "size of m " << m << "\n";
        // Переводим последовательность байтов обратно в матрицу
        std::vector<std::vector<int>> matrix;
        for (size_t i = 0; i < m; i++) {
            std::vector<int> row;
            for (size_t j = 0; j < m; j++) {
                int value = static_cast<int>(receivedData[i * m + j]);
                row.push_back(value);
            }
            matrix.push_back(row);
        }
        return matrix;
    }


    void sendStatus(SOCKET clientSocket, Status status) {
        char statusBuffer[1] = { static_cast<char>(status) };
        send(clientSocket, statusBuffer, 1, 0);
    }

    Command receiveCommand(SOCKET clientSocket) {
        char commandBuffer[1];
        recv(clientSocket, commandBuffer, 1, 0);
        return static_cast<Command>(commandBuffer[0]);
    }

    void handleClient(SOCKET clientSocket) {
        std::vector<std::vector<int>> receivedMatrix;
        Status status = UNKNOWN;
        receivedMatrix = receiveMatrix(clientSocket);
        int m = receivedMatrix.size();
        std::vector<std::vector<int>> newMatrix(m, std::vector<int>(m));
        bool flag = true;
        do {
            Command command = receiveCommand(clientSocket);
            switch (command){ {
                case START:
                    if(status == IN_PROGRESS){
                        sendStatus(clientSocket, ERR);
                        break;
                    }
                    std::cout << "Received START command.\n";
                    status = IN_PROGRESS;
                    sendStatus(clientSocket, status);
                    std::thread sideSwapThread(&TCPServer::sideSwap, this,
                                               std::cref(receivedMatrix), std::ref(newMatrix),
                                               m, m, 1, 0, std::ref(status));
                    sideSwapThread.detach();
                    break;
            }
            case STATUS: {
                std::cout << "Received STATUS command.\n";
                sendStatus(clientSocket, status);
                break;
            }
            case RESULT: {
                if(status != COMPLETED){
                    sendStatus(clientSocket, status);
                    break;
                }
                std::cout << "Received RESULT command.\n";
                sendStatus(clientSocket, status);
                sendMatrix(newMatrix,clientSocket);
                break;
            }
            default: {
                std::cerr << "Unknown command.\n";
                sendStatus(clientSocket, ERR);
                flag = false;
                break;
            }
        }

    } while (flag == true);

    closesocket(clientSocket);

    }
    void sideSwap(const std::vector<std::vector<int>>& arr, std::vector<std::vector<int>>& newArr, int rows, int cols, int threadsAmount, int thread_id, Status &status){
        if(cols == 0){
            return;
        }
        int i = thread_id / cols;// індекс рядка
        for(; i < rows; i++){
            int j = thread_id % cols; // індекс колонки
            for(; j < cols; j = j+threadsAmount){ // те саме зрушення
                newArr[j][i] = arr[rows - 1 - i][cols - 1 - j]; // переписування в іншу матрицю
            }
        }
        status = COMPLETED;
    }

    ~TCPServer() {
        // Clean up
        closesocket(serverSocket);
        WSACleanup();
    }
};

int main() {
    TCPServer server(5400); // Specify port number
    if (server.initialize()) {
        std::cout << "Server initialized.\n";
        server.acceptConnections();
    } else {
        std::cerr << "Server initialization failed.\n";
    }
    return 0;
}
