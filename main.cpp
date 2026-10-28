#define _WIN32_WINNT 0x0600

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <sstream>
#include <ctime>
#include <iomanip>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

// Telegram Dark Palette Constants
const COLORREF TG_BG_CHAT       = RGB(14, 22, 33);    // #0e1621
const COLORREF TG_BG_SIDEBAR    = RGB(23, 33, 43);    // #17212b
const COLORREF TG_BG_ITEM_HOVER = RGB(32, 43, 54);    // #202b36
const COLORREF TG_BG_ITEM_SEL   = RGB(43, 82, 120);   // #2b5278
const COLORREF TG_BUBBLE_IN     = RGB(24, 37, 51);    // #182533
const COLORREF TG_BUBBLE_OUT    = RGB(43, 82, 120);   // #2b5278
const COLORREF TG_HEADER        = RGB(23, 33, 43);    // #17212b
const COLORREF TG_INPUT_BG      = RGB(24, 37, 51);    // #182533
const COLORREF TG_BORDER        = RGB(16, 24, 34);    // #101822
const COLORREF TG_TEXT          = RGB(245, 245, 245); // #f5f5f5
const COLORREF TG_TEXT_MUTED    = RGB(126, 146, 162); // #7e92a2
const COLORREF TG_BLUE          = RGB(82, 136, 193);  // #5288c1
const COLORREF TG_ONLINE        = RGB(78, 172, 237);  // #4eaced

#define IDC_INPUT_MSG   201
#define IDC_BTN_SEND    202
#define IDC_BTN_CONNECT 203
#define IDC_BTN_HOST    204
#define IDC_EDIT_IP     205
#define IDC_EDIT_PORT   206
#define IDC_EDIT_NICK   207

#define WM_USER_MESSAGE_ARRIVED (WM_USER + 10)
#define WM_USER_STATUS_CHANGED  (WM_USER + 11)

struct ChatMessage {
    std::string sender;
    std::string text;
    std::string time;
    bool isOutgoing;
};

struct ChatItem {
    std::string name;
    std::string lastMsg;
    std::string time;
    COLORREF avatarColor;
};

HWND g_hWnd = NULL;
HWND g_hInputMsg = NULL;
HWND g_hBtnSend = NULL;
HWND g_hEditIp = NULL;
HWND g_hEditPort = NULL;
HWND g_hEditNick = NULL;
HWND g_hBtnConnect = NULL;
HWND g_hBtnHost = NULL;

HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;
HFONT g_hFontSmall = NULL;
HFONT g_hFontAvatar = NULL;

SOCKET g_listenSock = INVALID_SOCKET;
SOCKET g_clientSock = INVALID_SOCKET;
volatile bool g_connected = false;
volatile bool g_running = true;
std::string g_peerStatus = "offline";
std::string g_myNick = "phax";

std::vector<ChatMessage> g_messages;
std::vector<ChatItem> g_chatList = {
    { "Direct Peer Chat", "Click Host or Connect above", "12:00", RGB(82, 136, 193) },
    { "Saved Messages", "Код, заметки и ссылки", "11:45", RGB(43, 114, 186) },
    { "Telegram News", "Welcome to Telegram Desktop Clone", "Yesterday", RGB(16, 185, 129) },
    { "Pavel Durov", "Freedom of speech is the key.", "25 Oct", RGB(147, 51, 234) }
};
int g_activeChatIndex = 0;
int g_scrollOffset = 0;

std::string GetCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* ltm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << ltm->tm_hour << ":"
        << std::setfill('0') << std::setw(2) << ltm->tm_min;
    return oss.str();
}

void AppendMessage(const std::string& sender, const std::string& text, bool isOut) {
    ChatMessage msg;
    msg.sender = sender;
    msg.text = text;
    msg.time = GetCurrentTimestamp();
    msg.isOutgoing = isOut;

    g_messages.push_back(msg);
    if (!g_chatList.empty()) {
        g_chatList[0].lastMsg = (isOut ? "You: " : "") + text;
        g_chatList[0].time = msg.time;
    }

    if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

DWORD WINAPI NetworkRecvThread(LPVOID lpParam) {
    SOCKET s = (SOCKET)lpParam;
    char buffer[4096];
    while (g_running && g_connected) {
        int bytes = recv(s, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::string raw(buffer);

            std::string nick = "Peer";
            std::string text = raw;
            size_t sep = raw.find(": ");
            if (sep != std::string::npos) {
                nick = raw.substr(0, sep);
                text = raw.substr(sep + 2);
            }

            PostMessage(g_hWnd, WM_USER_MESSAGE_ARRIVED, 0, (LPARAM)new ChatMessage{ nick, text, GetCurrentTimestamp(), false });
        } else {
            g_connected = false;
            break;
        }
    }
    closesocket(s);
    if (g_clientSock == s) g_clientSock = INVALID_SOCKET;
    PostMessage(g_hWnd, WM_USER_STATUS_CHANGED, 0, (LPARAM)new std::string("offline"));
    return 0;
}

DWORD WINAPI HostThread(LPVOID lpParam) {
    int port = (int)(intptr_t)lpParam;
    g_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listenSock == INVALID_SOCKET) return 1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(g_listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(g_listenSock, 1) == SOCKET_ERROR) {
        closesocket(g_listenSock);
        g_listenSock = INVALID_SOCKET;
        PostMessage(g_hWnd, WM_USER_STATUS_CHANGED, 0, (LPARAM)new std::string("Bind error"));
        return 1;
    }

    PostMessage(g_hWnd, WM_USER_STATUS_CHANGED, 0, (LPARAM)new std::string("waiting for peer (port " + std::to_string(port) + ")"));

    sockaddr_in clientAddr{};
    int clientLen = sizeof(clientAddr);
    SOCKET client = accept(g_listenSock, (sockaddr*)&clientAddr, &clientLen);
    if (client == INVALID_SOCKET) return 1;

    g_clientSock = client;
    g_connected = true;
    PostMessage(g_hWnd, WM_USER_STATUS_CHANGED, 0, (LPARAM)new std::string("online"));
    CreateThread(NULL, 0, NetworkRecvThread, (LPVOID)client, 0, NULL);
    return 0;
}

struct ConnectArg {
    std::string ip;
    int port;
};

DWORD WINAPI ConnectThread(LPVOID lpParam) {
    ConnectArg* arg = (ConnectArg*)lpParam;
    std::string ip = arg->ip;
    int port = arg->port;
    delete arg;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.c_str());

    PostMessage(g_hWnd, WM_USER_STATUS_CHANGED, 0, (LPARAM)new std::string("connecting..."));

    if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        PostMessage(g_hWnd, WM_USER_STATUS_CHANGED, 0, (LPARAM)new std::string("offline"));
        return 1;
    }

    g_clientSock = s;
    g_connected = true;
    PostMessage(g_hWnd, WM_USER_STATUS_CHANGED, 0, (LPARAM)new std::string("online"));
    CreateThread(NULL, 0, NetworkRecvThread, (LPVOID)s, 0, NULL);
    return 0;
}

void DoSendMessage() {
    if (!g_hInputMsg) return;
    char buf[2048];
    GetWindowTextA(g_hInputMsg, buf, sizeof(buf));
    std::string text(buf);
    if (text.empty()) return;

    char nickBuf[64];
    GetWindowTextA(g_hEditNick, nickBuf, sizeof(nickBuf));
    if (strlen(nickBuf) > 0) g_myNick = nickBuf;

    AppendMessage(g_myNick, text, true);

    if (g_connected && g_clientSock != INVALID_SOCKET) {
        std::string pkt = g_myNick + ": " + text;
        send(g_clientSock, pkt.c_str(), (int)pkt.length(), 0);
    }

    SetWindowTextA(g_hInputMsg, "");
    SetFocus(g_hInputMsg);
}

void DrawRoundedBox(HDC hdc, int x, int y, int w, int h, COLORREF color, int radius = 10) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    RoundRect(hdc, x, y, x + w, y + h, radius, radius);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawAvatar(HDC hdc, int x, int y, int size, const std::string& name, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_NULL, 0, RGB(0,0,0));
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    Ellipse(hdc, x, y, x + size, y + size);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    // Initial letter
    std::string initial = name.empty() ? "?" : name.substr(0, 1);
    SelectObject(hdc, g_hFontAvatar);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    RECT rc = { x, y, x + size, y + size };
    DrawTextA(hdc, initial.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void RenderTelegramInterface(HWND hWnd, HDC hdc) {
    RECT clientRc;
    GetClientRect(hWnd, &clientRc);
    int width = clientRc.right;
    int height = clientRc.bottom;

    int sidebarWidth = 280;
    int headerHeight = 60;
    int bottomBarHeight = 60;
    int chatWidth = width - sidebarWidth;

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

    // 1. Fill Chat Background
    HBRUSH bgBrush = CreateSolidBrush(TG_BG_CHAT);
    RECT chatArea = { sidebarWidth, 0, width, height };
    FillRect(memDC, &chatArea, bgBrush);
    DeleteObject(bgBrush);

    // 2. Draw Left Sidebar (Chats)
    HBRUSH sidebarBrush = CreateSolidBrush(TG_BG_SIDEBAR);
    RECT sidebarArea = { 0, 0, sidebarWidth, height };
    FillRect(memDC, &sidebarArea, sidebarBrush);
    DeleteObject(sidebarBrush);

    // Sidebar separator line
    HPEN sepPen = CreatePen(PS_SOLID, 1, TG_BORDER);
    HGDIOBJ oldPen = SelectObject(memDC, sepPen);
    MoveToEx(memDC, sidebarWidth, 0, NULL);
    LineTo(memDC, sidebarWidth, height);

    // Sidebar Header (Telegram logo & Search)
    DrawRoundedBox(memDC, 12, 12, sidebarWidth - 24, 36, TG_INPUT_BG, 8);
    SelectObject(memDC, g_hFontNormal);
    SetTextColor(memDC, TG_TEXT_MUTED);
    SetBkMode(memDC, TRANSPARENT);
    RECT searchRc = { 24, 12, sidebarWidth - 24, 48 };
    DrawTextA(memDC, "🔍  Search...", -1, &searchRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Chat List Items
    int itemY = 60;
    for (size_t i = 0; i < g_chatList.size(); ++i) {
        bool isSel = ((int)i == g_activeChatIndex);
        if (isSel) {
            DrawRoundedBox(memDC, 8, itemY, sidebarWidth - 16, 62, TG_BG_ITEM_SEL, 12);
        }

        // Avatar
        DrawAvatar(memDC, 18, itemY + 10, 42, g_chatList[i].name, g_chatList[i].avatarColor);

        // Name
        SelectObject(memDC, g_hFontBold);
        SetTextColor(memDC, TG_TEXT);
        RECT nameRc = { 68, itemY + 12, sidebarWidth - 65, itemY + 30 };
        DrawTextA(memDC, g_chatList[i].name.c_str(), -1, &nameRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Time
        SelectObject(memDC, g_hFontSmall);
        SetTextColor(memDC, isSel ? TG_TEXT : TG_TEXT_MUTED);
        RECT timeRc = { sidebarWidth - 65, itemY + 14, sidebarWidth - 16, itemY + 30 };
        DrawTextA(memDC, g_chatList[i].time.c_str(), -1, &timeRc, DT_RIGHT | DT_SINGLELINE);

        // Last Message
        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, isSel ? RGB(220, 235, 255) : TG_TEXT_MUTED);
        RECT msgRc = { 68, itemY + 33, sidebarWidth - 20, itemY + 54 };
        DrawTextA(memDC, g_chatList[i].lastMsg.c_str(), -1, &msgRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        itemY += 66;
    }

    // 3. Right Panel Header (Active Chat Details)
    HBRUSH headerBrush = CreateSolidBrush(TG_HEADER);
    RECT headerArea = { sidebarWidth + 1, 0, width, headerHeight };
    FillRect(memDC, &headerArea, headerBrush);
    DeleteObject(headerBrush);

    MoveToEx(memDC, sidebarWidth + 1, headerHeight, NULL);
    LineTo(memDC, width, headerHeight);

    // Active Chat Avatar in Header
    DrawAvatar(memDC, sidebarWidth + 20, 10, 40, g_chatList[g_activeChatIndex].name, g_chatList[g_activeChatIndex].avatarColor);

    // Active Chat Title
    SelectObject(memDC, g_hFontBold);
    SetTextColor(memDC, TG_TEXT);
    RECT headerTitleRc = { sidebarWidth + 70, 12, width - 150, 32 };
    DrawTextA(memDC, g_chatList[g_activeChatIndex].name.c_str(), -1, &headerTitleRc, DT_LEFT | DT_SINGLELINE);

    // Status subtitle ("online" / "offline")
    SelectObject(memDC, g_hFontSmall);
    SetTextColor(memDC, (g_peerStatus == "online") ? TG_ONLINE : TG_TEXT_MUTED);
    RECT headerStatusRc = { sidebarWidth + 70, 32, width - 150, 50 };
    std::string statusText = (g_activeChatIndex == 0) ? ("peer: " + g_peerStatus) : "service channel";
    DrawTextA(memDC, statusText.c_str(), -1, &headerStatusRc, DT_LEFT | DT_SINGLELINE);

    // 4. Message History Feed
    int msgY = height - bottomBarHeight - 20;

    // Render messages bottom-to-top
    for (int i = (int)g_messages.size() - 1; i >= 0; --i) {
        const auto& m = g_messages[i];
        bool isOut = m.isOutgoing;

        // Calculate text bounding box
        RECT calcRc = { 0, 0, 420, 0 };
        SelectObject(memDC, g_hFontNormal);
        DrawTextA(memDC, m.text.c_str(), -1, &calcRc, DT_CALCRECT | DT_WORDBREAK);

        int bubbleW = calcRc.right + 44;
        if (bubbleW < 120) bubbleW = 120;
        int bubbleH = calcRc.bottom + 24;

        int bubbleX = isOut ? (width - bubbleW - 24) : (sidebarWidth + 24);
        msgY -= bubbleH;

        if (msgY < headerHeight) break; // Don't draw over header

        // Draw Bubble
        COLORREF bubbleColor = isOut ? TG_BUBBLE_OUT : TG_BUBBLE_IN;
        DrawRoundedBox(memDC, bubbleX, msgY, bubbleW, bubbleH, bubbleColor, 12);

        // Sender Nickname on incoming
        int textOffsetY = 8;
        if (!isOut) {
            SelectObject(memDC, g_hFontBold);
            SetTextColor(memDC, TG_BLUE);
            RECT nickRc = { bubbleX + 12, msgY + 6, bubbleX + bubbleW - 12, msgY + 22 };
            DrawTextA(memDC, m.sender.c_str(), -1, &nickRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            textOffsetY = 24;
        }

        // Message text
        SelectObject(memDC, g_hFontNormal);
        SetTextColor(memDC, TG_TEXT);
        RECT textRc = { bubbleX + 12, msgY + textOffsetY, bubbleX + bubbleW - 36, msgY + bubbleH - 6 };
        DrawTextA(memDC, m.text.c_str(), -1, &textRc, DT_LEFT | DT_WORDBREAK);

        // Timestamp & Double checkmark
        SelectObject(memDC, g_hFontSmall);
        SetTextColor(memDC, isOut ? RGB(140, 185, 230) : TG_TEXT_MUTED);
        std::string metaStr = m.time + (isOut ? " ✓✓" : "");
        RECT metaRc = { bubbleX + bubbleW - 48, msgY + bubbleH - 18, bubbleX + bubbleW - 8, msgY + bubbleH - 2 };
        DrawTextA(memDC, metaStr.c_str(), -1, &metaRc, DT_RIGHT | DT_SINGLELINE);

        msgY -= 10; // Spacing between bubbles
    }

    // 5. Bottom Input Area Background
    HBRUSH bottomBrush = CreateSolidBrush(TG_HEADER);
    RECT bottomArea = { sidebarWidth + 1, height - bottomBarHeight, width, height };
    FillRect(memDC, &bottomArea, bottomBrush);
    DeleteObject(bottomBrush);

    MoveToEx(memDC, sidebarWidth + 1, height - bottomBarHeight, NULL);
    LineTo(memDC, width, height - bottomBarHeight);

    // Blit to window
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldPen);
    DeleteObject(sepPen);
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // High quality typography
        g_hFontNormal = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        g_hFontBold = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        g_hFontSmall = CreateFontA(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        g_hFontAvatar = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

        // Top Connect Bar inside Header
        g_hEditNick = CreateWindowA("EDIT", "phax", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            580, 16, 70, 26, hWnd, (HMENU)IDC_EDIT_NICK, NULL, NULL);

        g_hEditIp = CreateWindowA("EDIT", "127.0.0.1", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            655, 16, 95, 26, hWnd, (HMENU)IDC_EDIT_IP, NULL, NULL);

        g_hEditPort = CreateWindowA("EDIT", "7777", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
            755, 16, 45, 26, hWnd, (HMENU)IDC_EDIT_PORT, NULL, NULL);

        g_hBtnHost = CreateWindowA("BUTTON", "Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            805, 16, 55, 26, hWnd, (HMENU)IDC_BTN_HOST, NULL, NULL);

        g_hBtnConnect = CreateWindowA("BUTTON", "Join", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            865, 16, 50, 26, hWnd, (HMENU)IDC_BTN_CONNECT, NULL, NULL);

        // Bottom Message Input Box (Telegram style)
        g_hInputMsg = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            305, 515, 530, 36, hWnd, (HMENU)IDC_INPUT_MSG, NULL, NULL);

        // Telegram Round Send Button
        g_hBtnSend = CreateWindowA("BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            845, 515, 70, 36, hWnd, (HMENU)IDC_BTN_SEND, NULL, NULL);

        const HWND controls[] = { g_hEditNick, g_hEditIp, g_hEditPort, g_hBtnHost, g_hBtnConnect, g_hInputMsg, g_hBtnSend };
        for (HWND h : controls) {
            SendMessage(h, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
        }

        // Initial welcome messages
        AppendMessage("Telegram", "Добро пожаловать в PHAX Telegram Desktop Clone!", false);
        AppendMessage("Telegram", "Нажми 'Host' чтобы слушать порт 7777, или введи IP собеседника и нажми 'Join'.", false);
        break;
    }

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // Reposition inputs dynamically
        SetWindowPos(g_hEditNick, NULL, width - 360, 16, 65, 26, SWP_NOZORDER);
        SetWindowPos(g_hEditIp, NULL, width - 290, 16, 95, 26, SWP_NOZORDER);
        SetWindowPos(g_hEditPort, NULL, width - 190, 16, 45, 26, SWP_NOZORDER);
        SetWindowPos(g_hBtnHost, NULL, width - 140, 16, 60, 26, SWP_NOZORDER);
        SetWindowPos(g_hBtnConnect, NULL, width - 75, 16, 60, 26, SWP_NOZORDER);

        SetWindowPos(g_hInputMsg, NULL, 300, height - 50, width - 400, 38, SWP_NOZORDER);
        SetWindowPos(g_hBtnSend, NULL, width - 90, height - 50, 75, 38, SWP_NOZORDER);

        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_SEND) {
            DoSendMessage();
        } else if (id == IDC_BTN_HOST) {
            if (g_connected || g_listenSock != INVALID_SOCKET) {
                AppendMessage("System", "Сервер уже запущен или подключен.", false);
            } else {
                char portBuf[16];
                GetWindowTextA(g_hEditPort, portBuf, sizeof(portBuf));
                int port = atoi(portBuf);
                if (port <= 0) port = 7777;
                CreateThread(NULL, 0, HostThread, (LPVOID)(intptr_t)port, 0, NULL);
            }
        } else if (id == IDC_BTN_CONNECT) {
            if (g_connected) {
                AppendMessage("System", "Уже подключено к пиру.", false);
            } else {
                char ipBuf[128], portBuf[16];
                GetWindowTextA(g_hEditIp, ipBuf, sizeof(ipBuf));
                GetWindowTextA(g_hEditPort, portBuf, sizeof(portBuf));
                int port = atoi(portBuf);
                if (port <= 0) port = 7777;
                ConnectArg* arg = new ConnectArg{ std::string(ipBuf), port };
                CreateThread(NULL, 0, ConnectThread, arg, 0, NULL);
            }
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (x < 280 && y >= 60) {
            int clickedIdx = (y - 60) / 66;
            if (clickedIdx >= 0 && clickedIdx < (int)g_chatList.size()) {
                g_activeChatIndex = clickedIdx;
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        break;
    }

    case WM_USER_MESSAGE_ARRIVED: {
        ChatMessage* p = (ChatMessage*)lParam;
        if (p) {
            AppendMessage(p->sender, p->text, false);
            MessageBeep(MB_OK);
            delete p;
        }
        break;
    }

    case WM_USER_STATUS_CHANGED: {
        std::string* p = (std::string*)lParam;
        if (p) {
            g_peerStatus = *p;
            InvalidateRect(hWnd, NULL, FALSE);
            delete p;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RenderTelegramInterface(hWnd, hdc);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_ERASEBKGND:
        return 1; // Prevent flickering

    case WM_DESTROY: {
        g_running = false;
        g_connected = false;
        if (g_clientSock != INVALID_SOCKET) closesocket(g_clientSock);
        if (g_listenSock != INVALID_SOCKET) closesocket(g_listenSock);
        WSACleanup();

        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        if (g_hFontSmall) DeleteObject(g_hFontSmall);
        if (g_hFontAvatar) DeleteObject(g_hFontAvatar);

        PostQuitMessage(0);
        break;
    }

    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; // Handled by double-buffer
    wc.lpszClassName = "PhaxTelegramDesktopClass";

    if (!RegisterClassExA(&wc)) return 1;

    g_hWnd = CreateWindowExA(
        0,
        "PhaxTelegramDesktopClass",
        "Telegram",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 620,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) return 1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            if (GetFocus() == g_hInputMsg) {
                DoSendMessage();
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}
