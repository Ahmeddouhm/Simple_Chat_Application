#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <thread>
#include <string>
#include <vector>

// =============================================================
//                  1. Basic Configuration
// =============================================================
#pragma comment(lib, "ws2_32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// =============================================================
//                2. Global Handles & Variables
// =============================================================
HWND hLog;
HWND hMessage;
HWND hName;
HWND hSendButton;
HFONT hGuiFont = NULL;
HBRUSH hEditBgBrush = NULL;
HBRUSH hButtonBrush = NULL;
HBRUSH hButtonPressBrush = NULL;

// network globalVars
SOCKET clientSocket = INVALID_SOCKET;
bool connected = false;
std::string username;

// =============================================================
//                    3. Utility Functions
// =============================================================

// func AddLog 
void AddLog(const std::string& text) {
    if (!hLog) return;
    int len = GetWindowTextLengthA(hLog);
    SendMessageA(hLog, EM_SETSEL, len, len);
    SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)(text + "\r\n").c_str());
}

// =============================================================
//                    4. Networking Logic
// =============================================================

// func to init Socket
// connects to the server ip and port
bool InitializeSocket(const std::string& serverIP, int port) {
    WSADATA data;
    // start winsock
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        AddLog("WSAStartup failed.");
        return false;
    }

    // create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        AddLog("socket() failed.");
        WSACleanup();
        return false;
    }

    // setup server address
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, serverIP.c_str(), &server.sin_addr);

    // connect to server
    if (connect(clientSocket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        AddLog("Connection failed. Error: " + std::to_string(WSAGetLastError()));
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }

    connected = true;
    AddLog("Connected to server successfully.");
    return true;
}

// func to listen for incmoing msgs
void ReceiveThread() {
    char buffer[4096];
    while (connected) {
        // receive blocking 
        int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (bytes <= 0) {
            AddLog("Disconnected from server.");
            connected = false;
            break;
        }

        std::string msg(buffer, bytes);
        AddLog(msg);
    }
}

// =============================================================
//                      5. GUI Logic
// =============================================================

// func to send msg via socket & clear inputBox
void SendMessageFromGUI() {
    if (!connected) {
        AddLog("Error: Not connected to server.");
        return;
    }

    char msgText[1024];
    GetWindowTextA(hMessage, msgText, sizeof(msgText));
    std::string msg(msgText);

    if (msg.empty()) return;

    // msg formatting
    std::string fullMsg = "[" + username + "]" + " => " + msg;
    send(clientSocket, fullMsg.c_str(), (int)fullMsg.size(), 0);

    // clear inputBox
    SetWindowTextA(hMessage, "");
}

// main window creation
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        hGuiFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        // dark theme colors
        hEditBgBrush = CreateSolidBrush(RGB(54, 69, 79));
        hButtonBrush = CreateSolidBrush(RGB(80, 100, 115));
        hButtonPressBrush = CreateSolidBrush(RGB(40, 50, 60));

        // create controls
        hName = CreateWindowW(L"EDIT", L"YourName",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, NULL, NULL, NULL);

        hMessage = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, NULL, NULL, NULL);

        hSendButton = CreateWindowW(L"BUTTON", L"Send",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, (HMENU)1, NULL, NULL);

        hLog = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 0, 0, hwnd, NULL, NULL, NULL);

        SendMessage(hName, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        SendMessage(hMessage, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        SendMessage(hLog, WM_SETFONT, (WPARAM)hGuiFont, TRUE);
        break;

    // responsive layout
    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        int padding = 10;
        int inputHeight = 30;
        int btnWidth = 100;

        // positioning textBoxes
        MoveWindow(hName, padding, padding, 150, 25, TRUE);

        int bottomY = height - inputHeight - padding;

        MoveWindow(hMessage, padding, bottomY, width - btnWidth - (padding * 3), inputHeight, TRUE);

        MoveWindow(hSendButton, width - btnWidth - padding, bottomY, btnWidth, inputHeight, TRUE);

        int logY = padding + 25 + padding;
        int logHeight = bottomY - logY - padding;
        MoveWindow(hLog, padding, logY, width - (padding * 2), logHeight, TRUE);
    }
                break;
    // btn draw
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
            DrawTextW(hdc, L"Send", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
    }
                    break;

    // dark mode colors
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(220, 220, 220));
        SetBkColor(hdc, RGB(54, 69, 79));
        return (LRESULT)hEditBgBrush;
    }

    // handling btn click
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            char nameText[100];
            GetWindowTextA(hName, nameText, sizeof(nameText));
            username = std::string(nameText);
            SendMessageFromGUI();
        }
        break;

        // halting program
    case WM_DESTROY:
        if (hGuiFont) DeleteObject(hGuiFont);
        if (hEditBgBrush) DeleteObject(hEditBgBrush);
        if (hButtonBrush) DeleteObject(hButtonBrush);
        if (hButtonPressBrush) DeleteObject(hButtonPressBrush);

        connected = false; // disable server
        if (clientSocket != INVALID_SOCKET) closesocket(clientSocket);
        WSACleanup();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// =============================================================
//                      6. Entry Point
// =============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(54, 69, 79));
    wc.lpszClassName = L"ClientGUI";

    if (!RegisterClassW(&wc)) return 1;

    HWND hwnd = CreateWindowW(
        L"ClientGUI",
        L"Chat Client",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 450,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // connecting to Localhost (127.0.0.1) on Port 12345
    if (InitializeSocket("127.0.0.1", 12345)) {
        // runs background thread for receiving messages
        std::thread recvThread(ReceiveThread);
        recvThread.detach();
    }

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}