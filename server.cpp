#include <iostream>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <dirent.h> 
#include <cstdio>
#include <string>
#include <sys/inotify.h>
#include <fcntl.h>
#include <thread>
#include <mutex>

std::mutex mtx; // Mutex for thread safety

const std::string path_1 = "/home/longvu/Desktop/project/project_server/";
const std::string path_2 = "/home/longvu/Desktop/project/project_client/";


void getFileList(const std::string& directoryPath, std::vector<std::string>& fileList) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(directoryPath.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            fileList.push_back(ent->d_name);
        }
        closedir(dir);
    }
}

void fileList(int clientSocket, const std::vector<std::string>& fileList) {
    std::string fileNames;
    for (const auto& file : fileList) {
        fileNames += file + "\n"; // Tên các tệp được nối với ký tự xuống dòng
    }

    // Gửi độ dài của chuỗi tên tệp trước
    int fileSize = fileNames.size();
    send(clientSocket, &fileSize, sizeof(fileSize), 0);

    // Gửi chuỗi tên tệp
    send(clientSocket, fileNames.c_str(), fileSize, 0);
}


void pushFile(int clientSocket, const char* fileName) {
  
    // Nhận kích thước tệp từ client
    std :: streamsize fileSize;
    recv(clientSocket, &fileSize, sizeof(fileSize), 0)  ;
    if (fileSize <= 0) {
        std::cerr << "\nFile không tồn tại hoặc rỗng." << std::endl;
        return;
    }

    // Tạo file mới để ghi dữ liệu từ client
    std::ofstream outputFile(fileName, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "\nKhông thể tạo file: " << strerror(errno) << std::endl;
        return;
    }

    const int bufferSize = 4096;
    char buffer[bufferSize];

    // Nhận và ghi dữ liệu từ client vào file
    while (fileSize > 0) {
        int bytesReceived = recv(clientSocket, buffer, std::min(fileSize, static_cast<std::streamsize>(bufferSize)), 0);

        if (bytesReceived <= 0) {
            std::cerr << "\nLỗi khi nhận dữ liệu từ server." << std::endl;
            break;
        }

        outputFile.write(buffer, bytesReceived);
        fileSize -= bytesReceived;
    }
    std :: cout << "\nĐã push file thành công ";

    std:: string reply = "\nPush file thành công!";
    send(clientSocket, reply.c_str(), reply.length(), 0);
    outputFile.close();
}


void popFile(int clientSocket, const char* fileName) {
    std::string filePath = path_1 + fileName;
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "\nKhông thể mở file: " << strerror(errno) << std::endl;
        int error = -1;
        send(clientSocket, &error, sizeof(error), 0); // Gửi thông báo lỗi đến client
        return;
    }

    // Lấy kích thước file
    file.seekg(0, std::ios::end);
    int fileSize = file.tellg();
    file.seekg(0, std::ios::beg);


    // Gửi kích thước file trước
    if (send(clientSocket, &fileSize, sizeof(fileSize), 0) <= 0) {
        std::cerr << "\nLỗi khi gửi kích thước file." << std::endl;
        file.close();
        return;
    }

    const int bufferSize = 4096;
    char buffer[bufferSize];

    // Gửi dữ liệu từ file
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

    
    std :: cout << "\nĐã pop file thành công ";

    std:: string reply = "\nPop file thành công!";
    send(clientSocket, reply.c_str(), reply.length(), 0);
    file.close();
    // close(clientSocket); // Đóng kết nối sau khi gửi xong dữ liệu
}
void deleteFile(int clientSocket ,char* fileName) {
    std :: string filePath = path_1 + fileName;  
    if (remove(filePath.c_str()) != 0) {
    std::cout << "\nLỗi khi xóa file.\n";
        // Gửi thông báo lỗi đến client thông qua socket
    std :: string reply = "\nXóa file thất bại!";
    send(clientSocket, reply.c_str(), reply.length(), 0);
        } else {
    std::cout << "\nFile đã được xóa thành công.\n";
        // Gửi thông báo thành công đến client thông qua socket
    std:: string reply = "\nXóa file thành công!";
    send(clientSocket, reply.c_str(), reply.length(), 0);
    }
   
}

void monitorDirectory(const std::string& directoryPath, int clientSocket) {
    int inotifyFd = inotify_init();
    if (inotifyFd == -1) {
        std::cerr << "\nError initializing inotify: " << strerror(errno) << std::endl;
        return;
    }

    int watchFd = inotify_add_watch(inotifyFd, directoryPath.c_str(), IN_CREATE | IN_DELETE);
    if (watchFd == -1) {
        std::cerr << "\nError adding watch to directory: " << strerror(errno) << std::endl;
        close(inotifyFd);
        return;
    }

    const int bufferSize = 4096;
    char buffer[bufferSize];

    while (true) {
        int bytesRead = read(inotifyFd, buffer, bufferSize);
        if (bytesRead == -1) {
            std::cerr << "\nError reading inotify events: " << strerror(errno) << std::endl;
            break;
        }

        struct inotify_event* event = reinterpret_cast<struct inotify_event*>(buffer);

        if (event->mask & IN_CREATE) {
            std::cout << "\nFile created: " << event->name << std::endl;
            std::string message = "\nFile created: " + std::string(event->name);
            // send(clientSocket, message.c_str(), message.length(), 0);
        }

        if (event->mask & IN_DELETE) {
            std::cout << "\nFile deleted: " << event->name << std::endl;
            std::string message = "\nFile deleted: " + std::string(event->name);
            // send(clientSocket, message.c_str(), message.length(), 0);
        }
    }

    close(watchFd);
    close(inotifyFd);
}

void startMonitoring(int clientSocket) {
    std::thread([&clientSocket](){
        monitorDirectory(path_1, clientSocket);
    }).detach();
}

void handleClient(int clientSocket) {
    while (true) {
        std::cout << "\nServer is available...\n";
        int requestSize;

        // Nhận kích thước của request
        recv(clientSocket, &requestSize, sizeof(requestSize), 0);

        // Cấp phát bộ nhớ cho request dựa trên kích thước đã nhận
        char* request = new char[requestSize];

        // Nhận dữ liệu request thực tế
        recv(clientSocket, request, requestSize, 0);

        if (strcmp(request, "FILE_LIST") == 0) {
            std :: cout << "\nGửi danh sách file";
            std::vector<std::string> serverFileList;
            getFileList(path_1, serverFileList);
            fileList(clientSocket, serverFileList);
         
        } else if (strcmp(request, "PUSH_FILE") == 0) {
            std :: cout << "\nĐang nhận một file ";
            int fileNameLength;
            recv(clientSocket, &fileNameLength, sizeof(fileNameLength), 0);

           // Nhận tên file từ client
            char fileName[256]; // Đảm bảo đủ cho tên file
            recv(clientSocket, fileName, fileNameLength, 0);

            // Acquire lock before calling pushFile to ensure thread safety
            std::lock_guard<std::mutex> lock(mtx);
            pushFile(clientSocket, fileName);
          
        } else if (strcmp(request, "POP_FILE") == 0) {
            std :: cout << "\n\nĐang gửi một file ";
            int fileNameLength;
            recv(clientSocket, &fileNameLength, sizeof(fileNameLength), 0);

            // Nhận tên file từ client
            char fileName[256]; // Đảm bảo đủ cho tên file
            recv(clientSocket, fileName, fileNameLength, 0);

            // Acquire lock before calling popFile to ensure thread safety
            std::lock_guard<std::mutex> lock(mtx);
            popFile(clientSocket, fileName);
        } else if (strcmp(request, "DELETE_FILE") == 0) {
              // Nhận độ dài của tên file
            int fileNameLength;
            recv(clientSocket, &fileNameLength, sizeof(fileNameLength), 0);

           // Nhận tên file từ client
            char fileName[256]; // Đảm bảo đủ cho tên file
            recv(clientSocket, fileName, fileNameLength, 0);

            // Acquire lock before calling deleteFile to ensure thread safety
            std::lock_guard<std::mutex> lock(mtx);
            deleteFile(clientSocket, fileName);
        } else {
            std::cerr << "\nYêu cầu không hợp lệ" << std::endl;
        }
        delete[] request; // Giải phóng bộ nhớ sau khi xử lý yêu cầu
        //Tạm dừng việc nhận yêu cầu và kết thúc kết nối nếu client đã đóng kết nối
        int receivedBytes = recv(clientSocket, request, sizeof(request), MSG_PEEK);
        if (receivedBytes <= 0) {
            std::cout << "\nĐã ngắt kết nối." << std::endl;
            break;
        }
    }

    close(clientSocket); // Đóng kết nối khi hoàn thành tất cả yêu cầu từ client
}

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;

    // Tạo socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(7891);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);

    bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

    if (listen(serverSocket, 5) == 0) {
        std::cout << "Server listening..." << std::endl;
    } else {
        std::cerr << "Error in listening" << std::endl;
        return -1;
    }
   
    addr_size = sizeof serverStorage;
    startMonitoring(clientSocket);

    
    while (true) {
        clientSocket = accept(serverSocket, (struct sockaddr *) &serverStorage, &addr_size);

        if (clientSocket >= 0) {
            std::cout << "Connected to a client" << std::endl;

            // Start a new thread to handle the client
            std::thread(handleClient, clientSocket).detach();
        }
    }

    close(serverSocket);
    return 0;
}
