#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <thread>
#include <vector>
#include <string>
#include <algorithm>

// =============================================================
//                  1. Basic Configurations
// =============================================================
#pragma comment(lib, "ws2_32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// =============================================================
//                     2. Global Variables 
// =============================================================
HWND hLog;
HWND hPort;
SOCKET listenSocket = INVALID_SOCKET;
bool serverRunning = false;
std::vector<SOCKET> clients;

// =============================================================
//              3. Styling Globals (Fonts & Colors)
// =============================================================
HFONT hGuiFont = NULL;
HBRUSH hEditBgBrush = NULL;
HBRUSH hButtonBrush = NULL;
HBRUSH hButtonPressBrush = NULL;

// =============================================================
//                     4. Utility Functions
// =============================================================

// func adds msg to log
void AddLog(const std::string& text) {
    if (!hLog) return;
    int len = GetWindowTextLengthA(hLog);
    SendMessageA(hLog, EM_SETSEL, len, len);
    SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)(text + "\r\n").c_str());
}

// =============================================================
//                   5. Server Network Logic
// =============================================================

// func to deal with interaction msgs 
// runs in a separate thread for EACH connected client .
// handles receiving messages and broadcasting them .
void InteractWithClient(SOCKET clientSocket) {
    AddLog("Client connected.");
    char buffer[4096];

    while (true) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            AddLog("Client disconnected.");
            break;
        }

        std::string msg(buffer, bytes);
        AddLog("" + msg);

        for (auto c : clients) {
            if (c != clientSocket) {
                send(c, msg.c_str(), (int)msg.size(), 0);
            }
        }
    }

    // func halting the program
    clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    closesocket(clientSocket);
}

// func to the main server Bind -> Listen 
void ServerThread(int port) {
    // init WINSOCK
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        AddLog("WSAStartup failed.");
        return;
    }

    // init listening socket
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        AddLog("socket() failed.");
        WSACleanup();
        return;
    }

    // setting address structure
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    InetPton(AF_INET, L"0.0.0.0", &server.sin_addr);

    // binding [socket + port]
    if (bind(listenSocket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        AddLog("Bind Failed! Error: " + std::to_string(WSAGetLastError()));
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    // start listening
    listen(listenSocket, SOMAXCONN);
    AddLog("Server started on port: " + std::to_string(port));

    serverRunning = true;

    // main accept loop
    while (serverRunning) {
        // waiting new connection
        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            AddLog("accept() failed. Error: " + std::to_string(WSAGetLastError()));
            continue;
        }

        // adds curr client and runs new thread for upcoming one
        clients.push_back(clientSocket);
        std::thread(InteractWithClient, clientSocket).detach();
    }

    closesocket(listenSocket);
    WSACleanup();
}

// =============================================================
//                      6. GUI Window 
// =============================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        // creation
    case WM_CREATE:
        // fonts
        hGuiFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        // dark mode
        hEditBgBrush = CreateSolidBrush(RGB(54, 69, 79));
        hButtonBrush = CreateSolidBrush(RGB(80, 100, 115));
        hButtonPressBrush = CreateSolidBrush(RGB(40, 50, 60));

        // port inputBox
        hPort = CreateWindowW(L"EDIT", L"12345", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 100, 25, hwnd, NULL, NULL, NULL);

        // start btn
        CreateWindowW(L"BUTTON", L"Start Server",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            120, 10, 120, 25, hwnd, (HMENU)1, NULL, NULL);

        // log textBox
        hLog = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 10, 50, 480, 320, hwnd, NULL, NULL, NULL);

        // apply fonts
        SendMessage(hPort, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        SendMessage(hLog, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        break;

        // custom btns creation
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
        if (pDIS->CtlID == 1) {
            HDC hdc = pDIS->hDC;
            RECT rc = pDIS->rcItem;

            HBRUSH hCurrentBrush = (pDIS->itemState & ODS_SELECTED) ? hButtonPressBrush : hButtonBrush;

            FillRect(hdc, &rc, hCurrentBrush);
            FrameRect(hdc, &rc, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, hGuiFont);

            DrawTextW(hdc, L"Start Server", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
    }
                    break;

    // dark mode for textBoxes
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(220, 220, 220)); // light text
        SetBkColor(hdc, RGB(54, 69, 79));      // dark background
        return (LRESULT)hEditBgBrush;
    }
                          
    case WM_COMMAND:
        if (LOWORD(wParam) == 1 && !serverRunning) { 
            wchar_t portStr[10];
            GetWindowTextW(hPort, portStr, 10);
            int port = _wtoi(portStr);
            // running server logic
            std::thread(ServerThread, port).detach();
        }
        break;

    // halt & exit
    case WM_DESTROY:
        if (hGuiFont) DeleteObject(hGuiFont);
        if (hEditBgBrush) DeleteObject(hEditBgBrush);
        if (hButtonBrush) DeleteObject(hButtonBrush);
        if (hButtonPressBrush) DeleteObject(hButtonPressBrush);

        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// =============================================================
//                  7. WinMain (Entry Point)
// =============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    // main dark window
    wc.hbrBackground = CreateSolidBrush(RGB(54, 69, 79));

    wc.lpszClassName = L"ServerGUI";

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        L"ServerGUI",
        L"Chat Server",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 420,
        NULL, NULL, hInstance, NULL
    );

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}