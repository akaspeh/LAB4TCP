#include <iostream>
#include <winsock2.h>
#include <vector>
#include <cmath>

enum Command {
    START = 0x01,
    STATUS = 0x02,
    RESULT = 0x03,
};

enum Status {
    UNKNOWN = 0x06,
    IN_PROGRESS = 0x07,
    COMPLETED = 0x08,
    ERR = 0x09
};

std::string statusToString(Status status) {
    switch(status) {
        case UNKNOWN:
            return "UNKNOWN";
        case IN_PROGRESS:
            return "IN_PROGRESS";
        case COMPLETED:
            return "COMPLETED";
        case ERR:
            return "ERR";
        default:
            return "UNKNOWN"; // Невідомий статус, можна змінити за необхідності
    }
}

class TCPClient {
private:
    WSADATA wsaData;
    SOCKET clientSocket;
    SOCKADDR_IN serverAddr;
    std::string serverIP;
    int port;

public:
    TCPClient(const std::string& serverIP, int port) : serverIP(serverIP), port(port) {}

    bool initialize() {
        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed.\n";
            return false;
        }

        // Create socket
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed.\n";
            WSACleanup();
            return false;
        }

        // Set server address
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());
        serverAddr.sin_port = htons(port);

        return true;
    }

    bool connectToServer() {
        // Connect to server
        if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connection failed.\n";
            closesocket(clientSocket);
            WSACleanup();
            return false;
        }

        return true;
    }

    void sendMatrix(const std::vector<std::vector<int>>& matrix) {
        // Переводим матрицу в последовательность байтов
        std::vector<char> dataToSend;
        for (const auto& row : matrix) {
            for (int value : row) {
                // Преобразуем int в char и добавляем в массив
                dataToSend.push_back(static_cast<char>(value));
            }
        }
        // Отправляем размер данных (размер матрицы)
        int dataSize = dataToSend.size();
        send(clientSocket, reinterpret_cast<const char*>(&dataSize), sizeof(int), 0);
        // Отправляем сами данные (матрицу)
        send(clientSocket, dataToSend.data(), dataSize, 0);
    }

    Status receiveStatus() {
        char statusBuffer[1];
        recv(clientSocket, statusBuffer, 1, 0);
        return static_cast<Status>(statusBuffer[0]);
    }

    void sendCommand(Command command) {
        char commandBuffer[1] = { static_cast<char>(command) };
        send(clientSocket, commandBuffer, 1, 0);
    }

    std::vector<std::vector<int>> receiveMatrix() {
        // Принимаем размер данных (размер матрицы)
        int dataSize;
        recv(clientSocket, reinterpret_cast<char*>(&dataSize), sizeof(int), 0);
        // Принимаем сами данные (матрицу)
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
                continue; // Переходим к следующей попытке чтения
            }
            totalBytesReceived += bytesReceived;
        }

        // Переводим последовательность байтов обратно в матрицу
        int m = static_cast<int>(sqrt(dataSize));
        std::cout << "size of m " << m;
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

    ~TCPClient() {
        // Clean up
        closesocket(clientSocket);
        WSACleanup();
    }

};

int main() {
    TCPClient client("127.0.0.1", 5400); // Specify server IP and port number
    if (client.initialize() && client.connectToServer()) {
        std::cout << "Connected to server.\n";
        bool Close = false;
        int size = 10;
        std::vector<std::vector<int>> matrix(size, std::vector<int>(size));
        std::vector<std::vector<int>> receivedMatrix;
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                matrix[i][j] = rand() % 100;
            }
        }
        client.sendMatrix(matrix);


        while(Close == false){
            int Command;
            std::cout << "write command ";
            std::cin >> Command;
            std::cout << "\n";
            switch(Command) {
                case 0:{
                    client.sendCommand(START);
                    Status status = client.receiveStatus();
                    std::cout << "Received status: " << statusToString(status) << std::endl;
                    break;
                }
                case 1: {
                    client.sendCommand(STATUS);
                    Status status = client.receiveStatus();
                    std::cout << "Received status: " << statusToString(status) << std::endl;
                    break;
                }
                case 2: {
                    client.sendCommand(RESULT);
                    Status status = client.receiveStatus();
                    std::cout << "Received status: " << statusToString(status) << std::endl;
                    if(status != COMPLETED){
                        std::cout << "Matrix isnt ready" << std::endl;
                    }
                    else{
                        receivedMatrix = client.receiveMatrix();
                        std::cout << "Received matrix: " << receivedMatrix.size() << std::endl;
                    }
                    break;
                }
                default: {
                    std::cout << "Unknown status received.\n";
                    Close = true;
                    break;
                }
            }
        }
    } else {
        std::cerr << "Failed to connect to server.\n";
    }

    return 0;
}