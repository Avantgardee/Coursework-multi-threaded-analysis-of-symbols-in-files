
#define _CRT_SECURE_NO_WARNINGS
#include "CharAnalysisAlgorithms.h"






//bool SelectSymbolGroup() {
//    std::wcout << L"Available symbol groups:\n";
//    int i = 1;
//    for (const auto& [name, ranges] : symbolGroups) {
//        std::wcout << i++ << L". " << name << L"\n";
//    }
//    std::wcout << i << L". All UTF-8 symbols\n"; // Добавляем опцию для всех символов
//
//    std::wcout << L"Enter the number of the symbol group: ";
//    int choice;
//    std::wcin >> choice;
//
//    if (choice < 1 || choice > symbolGroups.size() + 1) {
//        std::wcerr << L"Invalid choice. Exiting.\n";
//        return false;
//    }
//
//    if (choice == symbolGroups.size() + 1) {
//        allSymbols = true; // Устанавливаем флаг для всех символов
//        return true;
//    }
//
//    auto it = symbolGroups.begin();
//    std::advance(it, choice - 1);
//    selectedRanges = it->second;
//    return true;
//}

// Фильтрует символы в выбранном диапазоне

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



void SaveStatistics(std::ofstream& statsFile, const std::vector<std::pair<char32_t, uint64_t>>& sortedFrequency, uint64_t totalSymbolCount) {
    statsFile << "\xEF\xBB\xBF"; // Добавление BOM для корректного отображения UTF-8


    double sum = 0;
    for (const auto& [symbol, count] : sortedFrequency) {
        double percentage = (static_cast<double>(count) / totalSymbolCount) * 100.0;
        sum += percentage;
        statsFile << toUtf8(symbol) << " | Count: " << std::dec << count
            << " (" << std::fixed << std::setprecision(2) << percentage << "%)"
            << " - Unicode: U+" << std::setw(4) << std::setfill('0') << std::hex << std::uppercase << static_cast<uint32_t>(symbol)
            << " - Hex: 0x" << std::setw(4) << std::setfill('0') << std::hex << std::uppercase << static_cast<uint32_t>(symbol)
            << "\n";
    }
    statsFile << "  Sum: " << sum;
    statsFile.flush();
}



