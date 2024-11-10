#define WM_UPDATE_PROGRESS (WM_USER + 1)
#define WM_SET_LENGTH (WM_USER + 2)
#define UNICODE
#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt> // Для конвертации кодировок
#include "CharAnalysisAlgorithms.h"
#pragma comment(lib, "comctl32.lib")

#define _CRT_SECURE_NO_WARNINGS


struct WorkerData {
    HWND hwnd;
};
// Данные для фильтров
std::vector<std::pair<std::wstring, std::vector<std::pair<char32_t, char32_t>>>> symbolGroups = {
    {L"Latin", {{0x0000, 0x007F}, {0x0080, 0x00FF}, {0x0100, 0x017F}, {0x0180, 0x024F}}},
    {L"Digits and math", {{0x0030, 0x0039}, {0x2200, 0x22FF}, {0x2A00, 0x2AFF}}},
    {L"Cyrillic", {{0x0400, 0x04FF}, {0x0500, 0x052F}}},
    {L"Greek Alphabet", {{0x0370, 0x03FF}, {0x1F00, 0x1FFF}}},
    {L"Arabic Alphabet", {{0x0600, 0x06FF}, {0x08A0, 0x08FF}}},
    {L"Hebrew", {{0x0590, 0x05FF}}},
    {L"Chinese and Japanese", {{0x4E00, 0x9FFF}, {0x3040, 0x30FF}, {0xAC00, 0xD7AF}}},
    {L"Emoji", {{0x1F600, 0x1F64F}, {0x1F300, 0x1F5FF}, {0x1F680, 0x1F6FF}}},
    {L"Space and Punctuation Symbols", {{0x2000, 0x206F}, {0x2190, 0x21FF}, {0x20A0, 0x20CF}}},
    {L"All UTF - 8 symbols", {} }
};

// Глобальные переменные
std::mutex mtx;
std::condition_variable cv;
std::unordered_map<char32_t, uint64_t> globalFrequency;
std::queue<std::wstring> fileQueue;
bool doneAddingFiles;
std::wofstream logFile;
std::ofstream statsFile;
std::vector<std::pair<char32_t, char32_t>> selectedRanges;
bool allSymbols;
uint64_t totalSymbolCount;
uint64_t fileAnalized;
uint64_t totalFindFiles = 0;
std::vector<std::pair<char32_t, uint64_t>> sortedFrequencyInFiles;
// UI элементы
HWND hwndPathInput, hwndComboBox, hwndListView;
HWND hwndProgressBar;
bool IsInSelectedRanges(char32_t symbol) {
    if (allSymbols) {
        return true; // Если выбран пункт для всех символов, пропускаем проверку
    }
    for (const auto& range : selectedRanges) {
        if (symbol >= range.first && symbol <= range.second) {
            return true;
        }
    }
    return false;
}
std::wstring toUtf16(const std::string& utf8Str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wstr[0], wlen);
    return wstr;
}

void AnalyzeFile(const std::wstring& filePath) {
    auto start = std::chrono::system_clock::now();

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::wcerr << L"Error opening file: " << filePath << std::endl;
        return;
    }

    std::unordered_map<char32_t, uint64_t> localFrequency;
    char ch;
    while (file.get(ch)) {
        unsigned char byte = static_cast<unsigned char>(ch);

        char32_t symbol = 0;
        int bytes = 0;
        if (byte < 0x80) {
            symbol = byte;
            bytes = 1;
        }
        else if ((byte & 0xE0) == 0xC0) {
            symbol = byte & 0x1F;
            bytes = 2;
        }
        else if ((byte & 0xF0) == 0xE0) {
            symbol = byte & 0x0F;
            bytes = 3;
        }
        else if ((byte & 0xF8) == 0xF0) {
            symbol = byte & 0x07;
            bytes = 4;
        }

        for (int i = 1; i < bytes; i++) {
            file.get(ch);
            byte = static_cast<unsigned char>(ch);
            symbol = (symbol << 6) | (byte & 0x3F);
        }
        // Учитываем только символы в выбранном диапазоне
        if (IsInSelectedRanges(symbol)) {
            localFrequency[symbol]++;
        }
    }

    file.close();

    {
        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& [sym, count] : localFrequency) {
            globalFrequency[sym] += count;
            totalSymbolCount += count; 
        }
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> duration = end - start;
    auto endTime = std::chrono::system_clock::to_time_t(end);
    tm timeInfo;
    localtime_s(&timeInfo, &endTime);

   
    {
        std::lock_guard<std::mutex> lock(mtx);
        std::wostringstream logStream;
        logStream << L"File: " << filePath
            << L" | End Time: " << std::put_time(&timeInfo, L"%Y-%m-%d %H:%M:%S")
            << L" | Duration: " << duration.count() << L" seconds\n";
        logFile << logStream.str();
        logFile.flush(); 
    }
}

DWORD WINAPI WorkerThread(LPVOID param) {
    WorkerData* data = static_cast<WorkerData*>(param);

    while (true) {
        std::wstring filePath;

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !fileQueue.empty() || doneAddingFiles; });

            if (fileQueue.empty() && doneAddingFiles) break;

            filePath = fileQueue.front();
            fileQueue.pop();
        }

        AnalyzeFile(filePath);

        
        PostMessage(data->hwnd, WM_UPDATE_PROGRESS, NULL, NULL);
        // Отправляем сообщение в главный поток для обновления прогресс-бара
       
    }
    
    return 0;
}

void ListFiles(const std::wstring& directoryPath, HWND hwnd) {
    WIN32_FIND_DATAW findFileData;
    std::wstring searchPath = directoryPath + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Error: Unable to access directory " << directoryPath << std::endl;
        return;
    }

    do {
        std::wstring fileName = findFileData.cFileName;
        std::wstring fullPath = directoryPath + L"\\" + fileName;

        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fileName != L"." && fileName != L"..") {
                ListFiles(fullPath,hwnd); // Рекурсивный вызов для обхода поддиректорий
            }
        }
        else {
            std::lock_guard<std::mutex> lock(mtx);
            fileQueue.push(fullPath);
            totalFindFiles++;
        }
    } while (FindNextFileW(hFind, &findFileData) != 0);
    FindClose(hFind);
    SendMessage(hwnd, WM_SET_LENGTH, totalFindFiles, NULL); // Обновляем прогресс-бар
}

void AddItemToListView(HWND hListView, int index, char32_t symbol, uint64_t count, uint64_t totalCount) {
    LVITEM lvItem = { };
    lvItem.mask = LVIF_TEXT;
    lvItem.iItem = index;

    // Символ (сначала отображаем сам символ)
    std::wstring utf16Symbol = toUtf16(toUtf8(symbol));
    lvItem.pszText = const_cast<LPWSTR>(utf16Symbol.c_str());
    int itemIndex = ListView_InsertItem(hListView, &lvItem);

    // Количество
    wchar_t buffer[100];
    swprintf(buffer, 100, L"%llu", count);
    ListView_SetItemText(hListView, itemIndex, 1, buffer);

    // Процент
    double percentage = (static_cast<double>(count) / totalCount) * 100.0;
    swprintf(buffer, 100, L"%.2f", percentage);
    ListView_SetItemText(hListView, itemIndex, 2, buffer);

    // Unicode код
    swprintf(buffer, 100, L"U+%04X", symbol);
    ListView_SetItemText(hListView, itemIndex, 3, buffer);

    // Hex код
    swprintf(buffer, 100, L"0x%04X", symbol);
    ListView_SetItemText(hListView, itemIndex, 4, buffer);
}

void AddColumns(HWND hListView) {
    LVCOLUMN lvCol = { };
    lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    // Столбец: Символ
    lvCol.cx = 150;
    lvCol.pszText = (LPWSTR)L"Symbol";
    ListView_InsertColumn(hListView, 0, &lvCol);

    // Столбец: Количество
    lvCol.cx = 100;
    lvCol.pszText = (LPWSTR)L"Count";
    ListView_InsertColumn(hListView, 1, &lvCol);

    // Столбец: Процент
    lvCol.cx = 100;
    lvCol.pszText = (LPWSTR)L"Percentage";
    ListView_InsertColumn(hListView, 2, &lvCol);

    // Столбец: Unicode код
    lvCol.cx = 150;
    lvCol.pszText = (LPWSTR)L"Unicode";
    ListView_InsertColumn(hListView, 3, &lvCol);

    // Столбец: Hex код
    lvCol.cx = 100;
    lvCol.pszText = (LPWSTR)L"Hex";
    ListView_InsertColumn(hListView, 4, &lvCol);
}

// Функция для создания ListView
void CreateListView(HWND hwndParent) {
    hwndListView = CreateWindowEx(0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER,
        70, 100, 600, 300, hwndParent, NULL, GetModuleHandle(NULL), NULL);
    ShowWindow(hwndListView, TRUE);
    AddColumns(hwndListView);
}



// Функция для обновления списка в ListView
void UpdateListView(const std::vector<std::pair<char32_t, uint64_t>>& sortedFrequency) {
    int index = 0;
    // Преобразование в строку
    for (const auto& [symbol, count] : sortedFrequency) {
        double percentage = (static_cast<double>(count) / totalSymbolCount) * 100.0;

        AddItemToListView(hwndListView, index, symbol, count, totalSymbolCount);
        index++;
    }
}


void AnalyzeDirectory(const std::wstring& directoryPath, std::wofstream& logFile, std::ofstream& statsFile, HWND hwnd) {
    

    auto startTime = std::chrono::system_clock::now();
    
    // Запись BOM для UTF-8
    unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    statsFile.write(reinterpret_cast<const char*>(bom), sizeof(bom));

    int numThreads = std::thread::hardware_concurrency();
    std::vector<HANDLE> threads(numThreads);

    for (int i = 0; i < numThreads; i++) {
    WorkerData* data = new WorkerData{ hwnd};
    threads[i] = CreateThread(
        NULL, 
        0, 
        WorkerThread, 
        data,  // передаем структуру данных
        0, 
        NULL
    );
}

    // Добавляем файлы в очередь
    ListFiles(directoryPath, hwnd);

    {
        std::lock_guard<std::mutex> lock(mtx);
        doneAddingFiles = true;
    }
    cv.notify_all();

    // Ожидаем завершения всех потоков
    WaitForMultipleObjects(numThreads, threads.data(), TRUE, INFINITE);

    for (HANDLE thread : threads) {
        CloseHandle(thread);
    }

    SendMessage(hwndListView, WM_SETREDRAW, FALSE, 0);
    sortedFrequencyInFiles.assign(globalFrequency.begin(), globalFrequency.end());
    customSort(sortedFrequencyInFiles);
    UpdateListView(sortedFrequencyInFiles);
    RedrawWindow(hwndListView, NULL, NULL, RDW_INVALIDATE);
    SendMessage(hwndListView, WM_SETREDRAW, TRUE, 0);
    statsFile << "Character frequency statistics:\n";
    SaveStatistics(statsFile, sortedFrequencyInFiles, totalSymbolCount);  // Передаем statsFile в SaveStatistics

    auto endTime = std::chrono::system_clock::now();
    std::chrono::duration<double> duration = endTime - startTime;

    logFile.close();
    statsFile.close();

}



// Функция для создания комбобокса
void CreateComboBox(HWND hwndParent) {
    hwndComboBox = CreateWindow(WC_COMBOBOX, L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL,
        10, 50, 200, 200, hwndParent, NULL, GetModuleHandle(NULL), NULL);

    for (const auto& [name, _] : symbolGroups) {
        SendMessage(hwndComboBox, CB_ADDSTRING, 0, (LPARAM)name.c_str());
    }
}

// Функция для обработки кнопки запуска анализа
void StartAnalysis(HWND hwnd) {
    // Чтение введенного пути
    wchar_t buffer[1024];
    GetWindowText(hwndPathInput, buffer, 1024);
    std::wstring directoryPath = buffer;

    // Проверка пути
    if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath)) {
        MessageBox(hwnd, L"Invalid directory path!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Получаем выбранный фильтр
   

    // Открытие лог-файла и файла статистики
    logFile.open("analysis_log.txt", std::ios::out | std::ios::trunc);
    if (!logFile.is_open()) {
        std::wcerr << L"Error opening log file." << std::endl;
        return;
    }
    logFile.imbue(std::locale("ru_RU.UTF-8"));
    statsFile.open("stats.txt", std::ios::out | std::ios::binary | std::ios::trunc); // Очистка файла для статистики
    if (!statsFile.is_open()) {
        std::wcerr << L"Error opening stats file." << std::endl;
        return;
    }

    globalFrequency.clear();
    totalSymbolCount = 0;

    // Запуск анализа
    std::thread analysisThread([=] {
        AnalyzeDirectory(directoryPath, logFile, statsFile, hwnd);
        });
    analysisThread.detach(); // Отсоединяем поток, чтобы избежать блокировки основного потока интерфейса

}

// Обработчик оконных сообщений
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateListView(hwnd);
        CreateComboBox(hwnd);

        // Поле ввода пути
        hwndPathInput = CreateWindow(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            10, 10, 600, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hwndProgressBar = CreateWindowEx(0, PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            10, 400, 600, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
        // Кнопка старта
        CreateWindow(L"BUTTON", L"Start Analysis", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            620, 10, 150, 30, hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            StartAnalysis(hwnd);
        }
        else if (HIWORD(wParam) == CBN_SELCHANGE) { // Обрабатываем изменение выбора в комбобоксе
            int selectedIndex = SendMessage(hwndComboBox, CB_GETCURSEL, 0, 0);
            if (selectedIndex != CB_ERR) {
                std::wstring selectedGroup;
                int length = SendMessage(hwndComboBox, CB_GETLBTEXTLEN, selectedIndex, 0);
                if (length != CB_ERR) {
                    selectedGroup.resize(length);
                    SendMessage(hwndComboBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedGroup.data());
                }

                // Обновляем выбранные диапазоны на основе выбора в комбобоксе
                selectedRanges.clear();
                if (selectedIndex == 9) {
                    allSymbols = true;
                    selectedRanges = symbolGroups[9].second;
                    break;
                }
                else {
                    allSymbols = false;
                    if (selectedIndex >= 0 && selectedIndex < symbolGroups.size()) {
                        selectedRanges = symbolGroups[selectedIndex].second;
                    }
                }
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_UPDATE_PROGRESS:
    {
        // Получаем размер глобальной очереди файлов

        // Инкрементируем глобальную переменную fileAnalized
        fileAnalized++;

        // Вычисляем прогресс
        if (totalFindFiles > 0) {
            int progress = static_cast<int>((static_cast<float>(fileAnalized) / totalFindFiles) * 100);
            SendMessage(hwndProgressBar, PBM_SETPOS, progress, 0); // Обновляем прогресс-бар
        }
    }
    break;
    //case WM_SET_LENGTH:
    //{
    //    // Получаем размер глобальной очереди файлов
    //    totalFindFiles = static_cast<int>(wParam); // Количество найденых файлов

    //}
    break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
    // Инициализация библиотеки common controls
    InitCommonControls();

    // Регистрация окна
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"MainWindowClass";
    RegisterClass(&wc);

    // Создание окна
    HWND hwnd = CreateWindow(wc.lpszClassName, L"Symbol Frequency Analyzer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 800, 500, NULL, NULL, wc.hInstance, NULL);

    // Главный цикл обработки сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
