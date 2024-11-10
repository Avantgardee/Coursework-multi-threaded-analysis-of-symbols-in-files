
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

template<typename T>
void merge(std::vector<T>& arr, std::vector<T>& temp, int left, int mid, int right) {
    int i = left;      // Начало первой половины
    int j = mid + 1;   // Начало второй половины
    int k = left;      // Начало для временного массива

    // Слияние двух половин
    while (i <= mid && j <= right) {
        if (arr[i].second > arr[j].second) {
            temp[k] = arr[i];
            ++i;
        }
        else {
            temp[k] = arr[j];
            ++j;
        }
        ++k;
    }

    // Копируем оставшиеся элементы из первой половины
    while (i <= mid) {
        temp[k] = arr[i];
        ++i;
        ++k;
    }

    // Копируем оставшиеся элементы из второй половины
    while (j <= right) {
        temp[k] = arr[j];
        ++j;
        ++k;
    }

    // Копируем временный массив обратно в основной
    for (int l = left; l <= right; ++l) {
        arr[l] = temp[l];
    }
}

// Итеративная функция сортировки слиянием
template<typename T>
void mergeSortIterative(std::vector<T>& arr) {
    int n = arr.size();
    std::vector<T> temp(n);

    for (int currSize = 1; currSize <= n - 1; currSize *= 2) {
        for (int leftStart = 0; leftStart < n - 1; leftStart += 2 * currSize) {
            int mid = min(leftStart + currSize - 1, n - 1);
            int rightEnd = min(leftStart + 2 * currSize - 1, n - 1);
            merge(arr, temp, leftStart, mid, rightEnd);
        }
    }
}

// Функция для сортировки с использованием итеративной сортировки слиянием
template<typename T>
void customSort(std::vector<T>& arr) {
    if (!arr.empty()) {
        mergeSortIterative(arr);
    }
}

void SaveStatistics(std::ofstream& statsFile, const std::unordered_map<char32_t, uint64_t>& globalFrequency, uint64_t totalSymbolCount) {
    statsFile << "\xEF\xBB\xBF"; // Добавление BOM для корректного отображения UTF-8

    // Создаем вектор для сортировки по частоте встречаемости
    std::vector<std::pair<char32_t, uint64_t>> sortedFrequency(globalFrequency.begin(), globalFrequency.end());

    // Используем свою функцию сортировки
    customSort(sortedFrequency);

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



