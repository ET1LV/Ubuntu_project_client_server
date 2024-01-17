#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include <arpa/inet.h>
#include <limits>  
#include<string>
#include<cstdio>
#include<vector>
#include<sstream>
#include <thread>
using namespace std ;
const std::string path_1 = "/home/longvu/Desktop/project/project_server/";
const std::string path_2 = "/home/longvu/Desktop/project/project_client/";

void displayMenu() {
    std::cout << "\nDanh sách yêu cầu:\n";
    std::cout << "1. List file\n";
    std::cout << "2. Push file\n";
    std::cout << "3. Pop file\n";
    std::cout << "4. Delete file\n";
    std::cout << "5. Exit\n";
}


void pushFile(int clientSocket, const char* fileName) {
    // Gửi yêu cầu push và tên file, size tới server
    const char* request = "PUSH_FILE";
    // Tính toán kích thước của request
    int requestSize = strlen(request) + 1; // Bao gồm ký tự null kết thúc chuỗi
    // Gửi kích thước của request
    send(clientSocket, &requestSize, sizeof(requestSize), 0);
    // Gửi dữ liệu request thực tế
    send(clientSocket, request, requestSize, 0);

    std::string filePath = path_2 + fileName;
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "\nKhông thể mở file: " << strerror(errno) << std::endl;
        return;
    }
  
 // Lấy kích thước file
    file.seekg(0, std::ios::end);
    std :: streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);



    // Gửi độ dài của chuỗi fileName
    int fileNameLength = strlen(fileName) + 1;
    send(clientSocket, &fileNameLength, sizeof(fileNameLength), 0);

    // Gửi dữ liệu tên file
    send(clientSocket, fileName, fileNameLength, 0);
    // send(clientSocket, &fileSize, sizeof(fileSize), 0);
    // Gửi kích thước file trước
    if (send(clientSocket, &fileSize, sizeof(fileSize), 0) <= 0) {
        std::cerr << "\nLỗi khi gửi kích thước file." << std::endl;
        file.close();
        return;
    }


    const int bufferSize = 4096;
    char buffer[bufferSize];

    // Đọc và gửi dữ liệu từ file
    while (!file.eof()) {
        file.read(buffer, bufferSize);
        int bytesRead = file.gcount();

        if (bytesRead > 0) {
            if (send(clientSocket, buffer, bytesRead, 0) <= 0) {
                std::cerr << "\nLỗi khi gửi dữ liệu file." << std::endl;
                break;
            }
        }
    }

    file.close();
    // Nhận phản hồi từ server (nếu cần)
    char reply[1024];
    recv(clientSocket,reply,sizeof(reply),0);
    std :: cout << reply;
}
void popFile(int clientSocket, const char* fileName) {
    // Gửi yêu cầu push và tên file, size tới server
    const char* request = "POP_FILE";
    // Tính toán kích thước của request
    int requestSize = strlen(request) + 1; // Bao gồm ký tự null kết thúc chuỗi
    // Gửi kích thước của request
    send(clientSocket, &requestSize, sizeof(requestSize), 0);
    // Gửi dữ liệu request thực tế
    send(clientSocket, request, requestSize, 0);
    
    // Gửi độ dài của chuỗi fileName
    int fileNameLength = strlen(fileName) + 1;
    send(clientSocket, &fileNameLength, sizeof(fileNameLength), 0);

    // Gửi dữ liệu tên file
    send(clientSocket, fileName, fileNameLength, 0);

    // Nhận kích thước tệp từ server
    std::streamsize fileSize;
    recv(clientSocket, &fileSize, sizeof(fileSize), 0)  ;

    if (fileSize <= 0) {
        std::cerr << "\nFile không tồn tại hoặc rỗng." << std::endl;
        return;
    }

    // Tạo file mới để ghi dữ liệu từ server
    std::ofstream outputFile(fileName, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "\nKhông thể tạo file: " << strerror(errno) << std::endl;
        return;
    }

    const int bufferSize = 4096;
    char buffer[bufferSize];

    // Nhận và ghi dữ liệu từ server vào file
    while (fileSize > 0) {
        int bytesReceived = recv(clientSocket, buffer, std::min(fileSize, static_cast<std::streamsize>(bufferSize)), 0);

        if (bytesReceived <= 0) {
            std::cerr << "\nLỗi khi nhận dữ liệu từ server." << std::endl;
            break;
        }
        else{

        outputFile.write(buffer, bytesReceived);
        fileSize -= bytesReceived;
    }}
  // Nhận phản hồi từ server (nếu cần)
    char reply[1024];
    recv(clientSocket, reply, sizeof(reply), 0);
    std::cout << reply;
    outputFile.close();
}
void deleteFile(int clientSocket, const char* fileName) {
    // Gửi yêu cầu delete và tên của tệp tin tới server
    const char* request = "DELETE_FILE";
    int requestSize = strlen(request) + 1;

    // Gửi kích thước của request
    send(clientSocket, &requestSize, sizeof(requestSize), 0);

    // Gửi dữ liệu request thực tế
    send(clientSocket, request, requestSize, 0);

    // Gửi độ dài của chuỗi fileName
    int fileNameLength = strlen(fileName) + 1;
    send(clientSocket, &fileNameLength, sizeof(fileNameLength), 0);

    // Gửi dữ liệu tên file
    send(clientSocket, fileName, fileNameLength, 0);

    // Nhận phản hồi từ server (nếu cần)
    char reply[1024];
    recv(clientSocket, reply, sizeof(reply), 0);
    std::cout << reply;
}
void disconnect(int clientSocket) {
    close(clientSocket); // Đóng socket kết nối
    std::cout << "Đã ngắt kết nối" << std::endl;
}

int main() {
    int clientSocket;
    // char buffer[4096];
    struct sockaddr_in serverAddr;
    socklen_t addr_size;

    bool connected = false;
    const int maxAttempts = 5;
    int attempts = 0;

    while (!connected && attempts < maxAttempts) {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(7891);
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  

        addr_size = sizeof serverAddr;

        if (connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size) == -1) {
            std::cerr << "Connection attempt " << attempts + 1 << " failed: " << strerror(errno) << std::endl;
            attempts++;
            sleep(1); // Chờ 1 giây trước khi thử kết nối lại
        } else {
            connected = true;
            std::cout << "Connected to server!" << std::endl;
        }
    }

    if (connected) {
        // std::thread(receiveNotifications, clientSocket).detach();
        int choice = 0;
        int key = 1;

        while (choice != 5) {
            displayMenu();
            std::cout << "Nhập yêu cầu: ";
            std::cin >> choice;

            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // Ignore remaining characters in the buffer

            switch (choice) {
                case 1:{
                    std::system("clear");
                  // Gửi yêu cầu push và tên file, size tới server
                    const char* request = "FILE_LIST";
                  // Tính toán kích thước của request
                    int requestSize = strlen(request) + 1; // Bao gồm ký tự null kết thúc chuỗi
                  // Gửi kích thước của request
                    send(clientSocket, &requestSize, sizeof(requestSize), 0);
                  // Gửi dữ liệu request thực tế
                    send(clientSocket, request, requestSize, 0);
                    std :: streamsize fileSize;
                    recv(clientSocket, &fileSize, sizeof(fileSize), 0);

                    std::vector<char> buffer(fileSize + 1); // Tạo bộ đệm để nhận chuỗi tên tệp
                    recv(clientSocket, buffer.data(), fileSize, 0);

                    buffer[fileSize] = '\0'; // Kết thúc chuỗi

                    std::vector<std::string> fileList;
                    std::istringstream iss(buffer.data());

                    std::string fileNamelist;
                    while (std::getline(iss, fileNamelist, '\n')) {
                    fileList.push_back(fileNamelist);
                    }
                   // Displaying the file names received from the server
                    for (const auto& file : fileList) {
                    std::cout << file << std::endl;
                    }
                    std::cout << "\nNhấn 0 để quay lại danh sách yêu cầu: ";
                    std::cin >> key;
                    if (key == 0) {
                        std::system("clear");
                        displayMenu();
                    }
                    break;}
                case 2: {
                    std::system("clear");
                    std::string fileName;
                    std::cout << "Nhập file muốn push: ";
                    std::getline(std::cin, fileName); // Đường dẫn đến tệp tin cần đẩy lên server
                    pushFile(clientSocket, fileName.c_str());
                    std::cout << "\nNhấn 0 để quay lại danh sách yêu cầu: ";
                    std::cin >> key;
                    if (key == 0) {
                        std::system("clear");
                        displayMenu();
                    }
                    break;
                }
                case 3: {
                    std::system("clear");
                    std::string fileName;
                    std::cout << "Nhập tên tệp tin muốn pop: ";
                    getline(cin, fileName);
                    popFile(clientSocket, fileName.c_str());
                    std::cout << "\nNhấn 0 để quay lại danh sách yêu cầu: ";
                    std::cin >> key;
                    if (key == 0) {
                        std::system("clear");
                        displayMenu();
                    }
                    break;
                }
                case 4: {
                    std::system("clear");
                    std :: string fileName ;
                    cout << "Nhập tên file muốn xoá: ";
                    getline(cin, fileName);
                    deleteFile(clientSocket,fileName.c_str());
                    std::cout << "\nNhấn 0 để quay lại danh sách yêu cầu: ";
                    std::cin >> key;
                    if (key == 0) {
                        std::system("clear");
                        displayMenu();
                    }
                    break;
                }
                case 5:
                    disconnect(clientSocket);
                    break;
                default:
                    std::cerr << "\nYêu cầu không hợp lệ. Hãy chọn lại" << std::endl;
                    break;
            }
        }

        // close(clientSocket); // Đóng socket khi hoàn thành
    } else {
        std::cerr << "Failed to connect after " << maxAttempts << " attempts." << std::endl;
    }

    return 0;
}

