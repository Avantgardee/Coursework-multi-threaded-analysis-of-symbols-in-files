#define _CRT_SECURE_NO_WARNINGS
#include "CharAnalysisAlgorithms.h"

std::string toUtf8(char32_t symbol) {
    std::string utf8Char;
    if (symbol < 0x80) {
        utf8Char.push_back(static_cast<char>(symbol));
    }
    else if (symbol < 0x800) {
        utf8Char.push_back(0xC0 | ((symbol >> 6) & 0x1F));
        utf8Char.push_back(0x80 | (symbol & 0x3F));
    }
    else if (symbol < 0x10000) {
        utf8Char.push_back(0xE0 | ((symbol >> 12) & 0x0F));
        utf8Char.push_back(0x80 | ((symbol >> 6) & 0x3F));
        utf8Char.push_back(0x80 | (symbol & 0x3F));
    }
    else {
        utf8Char.push_back(0xF0 | ((symbol >> 18) & 0x07));
        utf8Char.push_back(0x80 | ((symbol >> 12) & 0x3F));
        utf8Char.push_back(0x80 | ((symbol >> 6) & 0x3F));
        utf8Char.push_back(0x80 | (symbol & 0x3F));
    }
    return utf8Char;
}

void SaveStatistics(std::ofstream& statsFile, std::vector<std::pair<char32_t, uint64_t>>& sortedFrequency, uint64_t totalSymbolCount, bool &isSaving) {
    statsFile << "\xEF\xBB\xBF";
    double sum = 0;
    
       
    if (isSaving) {
        for (const auto& [symbol, count] : sortedFrequency) {
            if (sortedFrequency.empty()) {
                sortedFrequency.clear();
                statsFile << "Error: sortedFrequency is empty.\n";
                statsFile.flush();
                return;
            }

            if (totalSymbolCount == 0) {
                statsFile << "Error: totalSymbolCount is zero.\n";
                statsFile.flush();
                return;
            }

            if (count == 0) {
                statsFile << "Error: count is zero for symbol " << toUtf8(symbol) << "\n";
                break;
            }
            double percentage = (static_cast<double>(count) / totalSymbolCount) * 100.0;
            sum += percentage;
            statsFile << toUtf8(symbol) << " | Count: " << std::dec << count
                << " (" << std::fixed << std::setprecision(8) << percentage << "%)"
                << " - Unicode: U+" << std::setw(4) << std::setfill('0') << std::hex << std::uppercase << static_cast<uint32_t>(symbol)
                << " - Hex: 0x" << std::setw(4) << std::setfill('0') << std::hex << std::uppercase << static_cast<uint32_t>(symbol)
                << "\n";
        }
    }
    
    statsFile << "  Sum: " << sum;
    statsFile.flush();
    
}

std::wstring FormatFileSize(std::streamsize fileSize) {
    double KB = 1024.0;
    double MB = KB * 1024.0;
    double GB = MB * 1024.0;

    std::wostringstream sizeStream;

    if (fileSize >= GB) {
        sizeStream << std::fixed << std::setprecision(2) << (fileSize / GB) << L" GB";
    }
    else if (fileSize >= MB) {
        sizeStream << std::fixed << std::setprecision(2) << (fileSize / MB) << L" MB";
    }
    else if (fileSize >= KB) {
        sizeStream << std::fixed << std::setprecision(2) << (fileSize / KB) << L" KB";
    }
    else {
        sizeStream << fileSize << L" bytes";
    }

    return sizeStream.str();
}

std::wstring toUtf16(const std::string& utf8Str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wstr[0], wlen);
    return wstr;
}



