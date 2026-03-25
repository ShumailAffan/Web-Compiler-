#define WIN32_LEAN_AND_MEAN
// --- 1. HEADERS & LIBRARIES ---
// We include these headers to give our program "abilities" like networking and file handling.
#include <windows.h>      // Core Windows functions
#include <winsock2.h>     // Networking (Sockets)
#include <ws2tcpip.h>    // TCP/IP protocol support
#include <iostream>       // Console input/output (cout)
#include <string>         // Handling text
#include <stdexcept>     // Error handling (try/catch)
#include <sstream>        // Working with string streams
#include <fstream>        // Reading and writing files to disk
#include <vector>         // Handy list-like containers
#include <algorithm>      // Useful functions for searching/sorting

// This line tells Windows to use the Networking library (Ws2_32.lib).
#pragma comment (lib, "Ws2_32.lib")

using namespace std;

// --- 2. THE COMPILER EXECUTION LOGIC ---
// This function takes the code you typed in the browser and turns it into a working .exe file.
string runGccCompiler(const string& sourceCode) {
    ostringstream out;
    out << "> Preparing compilation environment...\n";

    // Step A: Save the browser code into a real file on your hard drive.
    ofstream tempFile("temp_code.cpp", ios::out | ios::trunc | ios::binary);
    if (!tempFile.is_open()) {
        return out.str() + "[x] Critical Error: Unable to create temporary file for source code.\n";
    }
    
    // We clean up the text to make sure Windows doesn't get confused by hidden characters.
    string cleanSourceCode;
    for (char c : sourceCode) {
        if (c != '\r') cleanSourceCode += c;
    }
    
    tempFile.write(cleanSourceCode.c_str(), cleanSourceCode.length());
    tempFile.close();

    // Step B: Run the background "g++" command to build the program.
    out << "> Running backend compiler: g++ temp_code.cpp -o temp_code.exe\n";
    
    // Delete old logs so we don't see results from the last time you clicked "Run".
    remove("compile_log.txt");
    remove("execute_log.txt");
    remove("temp_code.exe");

    // "system" runs a command just like if you typed it in CMD yourself.
    int compileResult = system("g++ temp_code.cpp -o temp_code.exe > compile_log.txt 2>&1");
    
    // Read the compiler report (logs) to see if there were any errors or warnings.
    ifstream logFile("compile_log.txt", ios::binary);
    string logContent;
    if (logFile.is_open()) {
        logContent.assign((istreambuf_iterator<char>(logFile)), istreambuf_iterator<char>());
        logFile.close();
    }
    
    // If the compiler failed (compileResult != 0), we show the error in red on the screen.
    if (compileResult != 0) {
        out << "\n[!] COMPILATION ERROR:\n";
        out << "-------------------------------------------\n";
        out << (logContent.empty() ? "(No compiler output provided.)\n" : logContent);
        out << "-------------------------------------------\n";
        out << "Build process terminated with exit code " << compileResult << "\n";
        return out.str(); 
    }

    out << "> Compiled Successfully! Initializing execution...\n";
    if (logContent.length() > 0) { out << "[Warnings]: " << logContent << "\n"; }
    
    // Step C: Run the newly created program and capture its output (like "Hello World").
    out << "\n--- PROGRAM OUTPUT ---\n";
    
    int execResult = system("temp_code.exe > execute_log.txt 2>&1");
    
    ifstream execFile("execute_log.txt", ios::binary);
    if (execFile.is_open()) {
        string execContent((istreambuf_iterator<char>(execFile)), istreambuf_iterator<char>());
        out << execContent;
        execFile.close();
    } else {
        out << "[!] RUNTIME ERROR: Execution process failed to start.\n";
    }
    
    out << "\n----------------------\n";
    out << "Process finished (Exit Code: " << execResult << ")\n";

    return out.str();
}

// --- 3. THE WEB SERVER CLIENT HANDLER ---
// This function handles a single person visiting your website or clicking "Run Code".
void handle_client(SOCKET clientSocket) {
    char singleChar;
    string headerData = "";
    int status;
    
    // We read the "Request Header" byte-by-byte to know what the browser wants.
    while (true) {
        status = recv(clientSocket, &singleChar, 1, 0);
        if (status <= 0) { closesocket(clientSocket); return; }
        headerData += singleChar;
        // We look for the "Double Newline" which means the header is finished.
        if (headerData.length() >= 4 && headerData.substr(headerData.length() - 4) == "\r\n\r\n") {
            break;
        }
    }

    // Split the request into Method (GET/POST) and Path (URL).
    string method, path, httpVersion;
    istringstream requestStream(headerData);
    requestStream >> method >> path >> httpVersion;

    // Log the request to help debugging
    cout << "[REQUEST] " << method << " " << path << endl;

    string responsePrefix = "HTTP/1.1 200 OK\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n";
    string responseBody = "";

    // A: If the browser asks for a file (like index.html).
    if (method == "GET") {
        if (path == "/") path = "/index.html";
        string filePath = "ui/" + path.substr(1);
        
        ifstream file(filePath, ios::binary);
        if (file) {
            stringstream fstream;
            fstream << file.rdbuf();
            responseBody = fstream.str();
            
            // Set the "type" so the browser knows if it's reading HTML or CSS.
            if (path.find(".html") != string::npos) responsePrefix += "Content-Type: text/html\r\n";
            else if (path.find(".css") != string::npos) responsePrefix += "Content-Type: text/css\r\n";
            else if (path.find(".js") != string::npos) responsePrefix += "Content-Type: application/javascript\r\n";
        } else {
            responsePrefix = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Type: text/plain\r\n";
            responseBody = "404 Not Found";
        }
    } 
    // B: Handle CORS Preflight (OPTIONS)
    else if (method == "OPTIONS") {
        string optionsResponse = "HTTP/1.1 204 No Content\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: Content-Type\r\n"
                                 "Connection: close\r\n\r\n";
        send(clientSocket, optionsResponse.c_str(), (int)optionsResponse.length(), 0);
        closesocket(clientSocket);
        return;
    }
    // C: If the browser is sending us code to compile (The API).
    else if (method == "POST" && (path == "/api/compile" || path == "/api/compile/")) {
        size_t contentLength = 0;
        size_t clPos = headerData.find("Content-Length: ");
        if (clPos != string::npos) {
            size_t endOfLine = headerData.find("\r\n", clPos);
            string clStr = headerData.substr(clPos + 16, endOfLine - (clPos + 16));
            try { contentLength = stoi(clStr); } catch (...) { contentLength = 0; }
        }
        cout << "[DEBUG] Content-Length: " << contentLength << endl;
        // Read the actual C++ code from the request body.
        string body = "";
        char* bodyBuffer = new char[contentLength + 1];
        size_t totalReceived = 0;
        int waitCounter = 0;
        while (totalReceived < contentLength) {
            int received = recv(clientSocket, bodyBuffer + totalReceived, (int)(contentLength - totalReceived), 0);
            if (received <= 0) break;
            totalReceived += received;
        }
        bodyBuffer[totalReceived] = '\0';
        body = string(bodyBuffer, totalReceived);
        delete[] bodyBuffer;
        cout << "[DEBUG] Received Body (" << body.length() << " bytes)" << endl;

        // EXECUTE THE COMPILER LOGIC
        responseBody = runGccCompiler(body);
        cout << "[DEBUG] Compilation/Execution finished" << endl;
        responsePrefix += "Content-Type: text/plain; charset=utf-8\r\n";
    } 
    else {
        cout << "[WARNING] Rejected Request - 405: " << method << " " << path << endl;
        responsePrefix = "HTTP/1.1 405 Method Not Allowed\r\n"
                         "Access-Control-Allow-Origin: *\r\n"
                         "Connection: close\r\n";
    }

    // Prepare the final message for the browser.
    responsePrefix += "Content-Length: " + to_string(responseBody.length()) + "\r\n\r\n";
    string finalResponse = responsePrefix + responseBody;
    
    // Send the data back across the internet to the person's computer.
    const char* sendBuf = finalResponse.c_str();
    int dataLength = (int)finalResponse.length();
    int totalSent = 0;
    while(totalSent < dataLength) {
        int sent = send(clientSocket, sendBuf + totalSent, dataLength - totalSent, 0);
        if (sent == SOCKET_ERROR) break;
        totalSent += sent;
    }
    
    // Close the connection.
    closesocket(clientSocket);
}

// --- 4. THE STARTING POINT (main) ---
int main() {
    // Stage 1: Initialize Windows Network settings.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    // Stage 2: Create a "Socket"—think of it as a telephone line.
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) return 1;
    
    // Allow the server to restart quickly if it crashes.
    BOOL optval = TRUE;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

    // Stage 3: Give the server an address and a Port (8080).
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // "Bind" links the server to that port, and "Listen" starts waiting for calls.
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) return 1;
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) return 1;

    cout << "========================================" << endl;
    cout << "   Web Compiler Server Initialized     " << endl;
    cout << "  Hosting at http://localhost:8080/     " << endl;
    cout << "========================================" << endl;

    // Launch the website automatically in your browser!
    system("start http://localhost:8080");

    // Stage 4: Enter the "Infinite Wait".
    // The server stays here forever, waiting for you to use it.
    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            handle_client(clientSocket);
        }
    }

    // Cleanup (This code is almost never reached because the loop is infinite).
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
