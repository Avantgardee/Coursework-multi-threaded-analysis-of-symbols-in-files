#define WM_UPDATE_PROGRESS (WM_USER + 1)
#define WM_SET_LENGTH (WM_USER + 2)
#define UNICODE
#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
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
#include <codecvt> 
#include "UIFunctionsAndMethods.h"
#include "CharAnalysisAlgorithms.h"

#pragma comment(lib, "comctl32.lib")

#define _CRT_SECURE_NO_WARNINGS

struct ThreadData {
    std::wstring filePath;
    uint64_t startByte;
    uint64_t endByte;
    std::unordered_map<char32_t, uint64_t>& localFrequency;  

    ThreadData(std::wstring path, uint64_t start, uint64_t end, std::unordered_map<char32_t, uint64_t>& freq)
        : filePath(std::move(path)), startByte(start), endByte(end), localFrequency(freq) {}
};
struct WorkerData {
    HWND hwnd;
};

std::vector<std::pair<std::wstring, std::vector<std::pair<char32_t, char32_t>>>> symbolGroups = {
    {L"Latin", {{0x0000, 0x002F}, {0x003A, 0x007F}, {0x0080, 0x00FF}, {0x0100, 0x017F}, {0x0180, 0x024F}}},
    {L"Digits and math", {{0x0030, 0x0039}, {0x2200, 0x22FF}, {0x2A00, 0x2AFF}}},
    {L"Cyrillic", {{0x0400, 0x04FF}, {0x0500, 0x052F}}},
    {L"Greek Alphabet", {{0x0370, 0x03FF}, {0x1F00, 0x1FFF}}},
    {L"Arabic Alphabet", {{0x0600, 0x06FF}, {0x08A0, 0x08FF}}},
    {L"Hebrew", {{0x0590, 0x05FF}}},
    {L"Chinese and Japanese", {{0x4E00, 0x9FFF}, {0x3040, 0x30FF}, {0xAC00, 0xD7AF}}},
    {L"Emoji", {{0x1F600, 0x1F64F}, {0x1F300, 0x1F5FF}, {0x1F680, 0x1F6FF}}},
    {L"Space and Punctuation Symbols", {{0x2000, 0x206F}, {0x2190, 0x21FF}, {0x20A0, 0x20CF}}},
    {L"All groups symbols", {} }
};


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
uint64_t fileAnalized =0 ;
uint64_t totalFindFiles = 0;
std::vector<std::pair<char32_t, uint64_t>> sortedFrequencyInFiles;
std::map<std::wstring, uint64_t> groupFrequencyMap;
HWND hwndPathInput, hwndComboBox, hwndListView;
HWND hwndProgressBar;
HWND hwndStatusText;
HWND hwndStatusSaveText;
HWND hwndListViewForLog;
HWND hwndBtnPie;
HWND hwndChartWindow;
HWND hwndSlider;
HWND hwndCountOfSymbols;
HWND hwndCountOfFiles;
std::wstring saveStatsPath;
std::wstring saveFilesInfoPath;
int numThreads;
std::wstring baseText = L"Сохраняется в файл";
bool isDrawPieChart;
bool isAllSaved = true;
const UINT_PTR TIMER_ID = 1;
int dotCount = 0; 
bool isLoading = false;
bool isSaving = true;

bool IsInSelectedRanges(char32_t symbol) {
    if (allSymbols) {
        return true; 
    }
    for (const auto& range : selectedRanges) {
        if (symbol >= range.first && symbol <= range.second) {
            return true;
        }
    }
    return false;
}

std::wstring GetSymbolGroup(char32_t symbol) {
    for (const auto& group : symbolGroups) {
        for (const auto& range : group.second) {
            if (symbol >= range.first && symbol <= range.second) {
                return group.first;
            }
        }
    }
    return L"Other symbol"; 
}

void AnalyzeFileSingleThreaded(const std::wstring& filePath) {
    auto start = std::chrono::system_clock::now();

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::wcerr << L"Error opening file: " << filePath << std::endl;
        return;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

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

    std::wostringstream timeStream;
    timeStream << std::put_time(&timeInfo, L"%Y-%m-%d %H:%M:%S");
    std::wstring endTimeStr = timeStream.str();

    std::wostringstream durationStream;
    durationStream << duration.count() << L" seconds";
    std::wstring durationStr = durationStream.str();

    std::wstring fileSizeStr = FormatFileSize(fileSize);

    {
        std::lock_guard<std::mutex> lock(mtx);
        std::wostringstream logStream;
        logStream << L"File: " << filePath
            << L" | Size: " << fileSizeStr
            << L" | End Time: " << endTimeStr
            << L" | Duration: " << durationStr << L"\n";
        logFile << logStream.str();
        logFile.flush();
    }

    AddItemToListView(hwndListViewForLog, filePath, fileSizeStr, endTimeStr, durationStr,mtx);
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

        AnalyzeFileSingleThreaded(filePath);


        PostMessage(data->hwnd, WM_UPDATE_PROGRESS, NULL, NULL);

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
                ListFiles(fullPath, hwnd);
            }
        }
        else {
            std::lock_guard<std::mutex> lock(mtx);
            fileQueue.push(fullPath);
            totalFindFiles++;
        }
    } while (FindNextFileW(hFind, &findFileData) != 0);
    FindClose(hFind);
    SendMessage(hwnd, WM_SET_LENGTH, totalFindFiles, NULL);
}

void AnalyzeDirectory(const std::wstring& directoryPath, std::wofstream& logFile, std::ofstream& statsFile, HWND hwnd) {
    isAllSaved = false;
    auto startTime = std::chrono::system_clock::now();
    totalFindFiles = 0;
    fileAnalized = 0;
    unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    statsFile.write(reinterpret_cast<const char*>(bom), sizeof(bom));

    std::vector<HANDLE> threads(numThreads);

    for (int i = 0; i < numThreads; i++) {
        WorkerData* data = new WorkerData{ hwnd };
        threads[i] = CreateThread(
            NULL,
            0,
            WorkerThread,
            data,  
            0,
            NULL
        );
    }

    ListFiles(directoryPath, hwnd);
    {
        std::lock_guard<std::mutex> lock(mtx);
        doneAddingFiles = true;
    }
    cv.notify_all();

    WaitForMultipleObjects(numThreads, threads.data(), TRUE, INFINITE);

    for (HANDLE thread : threads) {
        CloseHandle(thread);
    }

    sortedFrequencyInFiles.assign(globalFrequency.begin(), globalFrequency.end());
    customSort(sortedFrequencyInFiles);
    auto endTime = std::chrono::system_clock::now();
    std::chrono::duration<double> duration = endTime - startTime;
    wchar_t durationStr[50];
    swprintf(durationStr, sizeof(durationStr) / sizeof(wchar_t), L"Анализ окончен. Длительность: %.2f секунд", duration.count());
    SetWindowText(hwndStatusText, durationStr);
    ListView_SetItemCount(hwndListView, sortedFrequencyInFiles.size());
    SendMessage(hwndListView, WM_SETREDRAW, FALSE, 0);

    RedrawWindow(hwndListView, NULL, NULL, RDW_INVALIDATE);
    SendMessage(hwndListView, WM_SETREDRAW, TRUE, 0);
    EnableWindow(hwndBtnPie, TRUE);
    isDrawPieChart = true;
    statsFile << "Character frequency statistics:\n";
    isLoading = true;
    SetWindowText(hwndStatusSaveText, (baseText).c_str());
    SaveStatistics(statsFile, sortedFrequencyInFiles, totalSymbolCount, isSaving);
    
    isLoading = false;
    SetWindowText(hwndStatusSaveText, L"Стастика сохранена в файл");
    std::wstring message = L"Всего просканировано файлов: " + std::to_wstring(totalFindFiles);
    SetWindowText(hwndCountOfFiles, message.c_str());
    message = L"Всего найдено символов:" + std::to_wstring(totalSymbolCount);
    SetWindowText(hwndCountOfSymbols, message.c_str());
   
    logFile.close();
    statsFile.close();
    isAllSaved = true;
}

void StartAnalysis(HWND hwnd) {
    isDrawPieChart = false;
    EnableWindow(hwndBtnPie, FALSE);
    SetWindowText(hwndStatusText, L" ");
    SetWindowText(hwndStatusSaveText, L" ");
    SetWindowText(hwndCountOfFiles, L" ");
    SetWindowText(hwndCountOfSymbols, L" ");
    ListView_DeleteAllItems(hwndListView);
    ListView_DeleteAllItems(hwndListViewForLog);
    doneAddingFiles = false;
    SendMessage(hwndProgressBar, PBM_SETPOS, 0, 0);
    if (hwndChartWindow) {
        ClearChartArea(hwndChartWindow);
    }
   
    wchar_t buffer[1024];
    GetWindowText(hwndPathInput, buffer, 1024);
    std::wstring directoryPath = buffer;

    if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath)) {
        MessageBox(hwnd, L"Не указана директория для анализа!", L"Ошибка", MB_OK | MB_ICONERROR);
        return;
    }

    logFile.open(saveFilesInfoPath, std::ios::out | std::ios::trunc);
    if (!logFile.is_open()) {
        std::wcerr << L"Error opening log file." << std::endl;
        return;
    }
    logFile.imbue(std::locale("ru_RU.UTF-8"));
    statsFile.open(saveStatsPath, std::ios::out | std::ios::binary | std::ios::trunc); 
    if (!statsFile.is_open()) {
        std::wcerr << L"Error opening stats file." << std::endl;
        return;
    }

    globalFrequency.clear();
    totalSymbolCount = 0;

    std::thread analysisThread([=] {
        AnalyzeDirectory(directoryPath, logFile, statsFile, hwnd);
        });
    analysisThread.detach(); 

}

LRESULT CALLBACK ChartWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdc, &ps.rcPaint, brush);
        DeleteObject(brush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE : {
        hwndListView = CreateListView(hwnd);
        hwndComboBox = CreateComboBox(hwnd, symbolGroups);
        hwndListViewForLog = CreateListViewForLog(hwnd);
        hwndStatusText = CreateWindow(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            240, 50, 220, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hwndStatusSaveText = CreateWindow(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            470, 50, 223, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hwndCountOfSymbols = CreateWindow(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 435, 300, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hwndCountOfFiles = CreateWindow(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            350, 435, 300, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hwndPathInput = CreateWindow(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT,
            20, 10, 670, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);
        hwndProgressBar = CreateWindowEx(0, PROGRESS_CLASS, L"",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            20, 90, 675, 30, hwnd, NULL, GetModuleHandle(NULL), NULL);

        CreateWindow(L"BUTTON", L"Начать анализ", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            695, 10, 400, 30, hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
        hwndBtnPie = CreateWindow(L"BUTTON", L"Диаграмма",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
           1110, 10, 400, 30, hwnd, (HMENU)6, NULL, NULL);
        allSymbols = true;
        EnableWindow(hwndBtnPie, FALSE);

        int maxThreads = std::thread::hardware_concurrency();
        numThreads = maxThreads;
        if (maxThreads == 0) {
            maxThreads = 1;  
            numThreads = 1;
        }

        hwndSlider = CreateWindowEx(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_NOTICKS | TBS_ENABLESELRANGE,
            710, 90, 790, 30,
            hwnd, (HMENU)10, GetModuleHandle(NULL), NULL
        );

        SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(1, maxThreads)); 
        SendMessage(hwndSlider, TBM_SETPOS, TRUE, numThreads);
        for (int i = 1; i <= maxThreads; ++i) {
            SendMessage(hwndSlider, TBM_SETTIC, 0, i);  
        }

        CreateWindow(L"STATIC", L"Количество потоков для анализа",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            715, 70, 400, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);

        SetTimer(hwnd, TIMER_ID, 300, NULL);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT windowRect;
        GetClientRect(hwnd, &windowRect);
        FillRect(hdc, &windowRect, (HBRUSH)(COLOR_WINDOW + 1));

        RECT sliderRect;
        GetClientRect(hwndSlider, &sliderRect);

        POINT pt = { sliderRect.left, sliderRect.top };
        MapWindowPoints(hwndSlider, hwnd, &pt, 1); 
        pt.x += 20;
        int sliderWidth = sliderRect.right - sliderRect.left - 28;

        int tickCount = std::thread::hardware_concurrency(); 

        int offset = 5; 

        for (int i = 1; i <= tickCount; ++i) {
            int x = pt.x + (sliderWidth * (i - 1)) / (tickCount - 1) ; 
            wchar_t label[10];
            swprintf(label, 10, L"%d", i); 

            TextOut(hdc, x - 10, pt.y + sliderRect.bottom + 5, label, wcslen(label));
        }
        if (isDrawPieChart) {
            DrawPieChart(hwndChartWindow, groupFrequencyMap);
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == TIMER_ID) {
            if (isLoading) {
             
                dotCount = (dotCount + 1) % 6; 
                std::wstring dots(dotCount, L'.');
                SetWindowText(hwndStatusSaveText, (baseText + dots).c_str());
            }
          
        }
        break;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        SetTextColor(hdcStatic, RGB(0, 0, 0)); 
        return (LRESULT)GetStockObject(WHITE_BRUSH); 
    }
    case WM_HSCROLL: {
        if ((HWND)lParam == hwndSlider) {
            numThreads = SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            if (!isAllSaved) {
                int result = MessageBox(
                    hwnd,
                    TEXT("Вы не можете начать новый анализ, пока сохраняются данные"),
                    TEXT("Ошибка"),
                    MB_ICONERROR | MB_OK
                );
                break;
            }
            else {
                StartAnalysis(hwnd);
            } 
        }
        else if (HIWORD(wParam) == CBN_SELCHANGE) { 
            int selectedIndex = SendMessage(hwndComboBox, CB_GETCURSEL, 0, 0);
            if (selectedIndex != CB_ERR) {
                std::wstring selectedGroup;
                int length = SendMessage(hwndComboBox, CB_GETLBTEXTLEN, selectedIndex, 0);
                if (length != CB_ERR) {
                    selectedGroup.resize(length);
                    SendMessage(hwndComboBox, CB_GETLBTEXT, selectedIndex, (LPARAM)selectedGroup.data());
                }

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
        else if (LOWORD(wParam) == 6) {

            hwndChartWindow = CreateWindowEx(0, L"STATIC", NULL,
                WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
               710, 200, 520, 520, hwnd, NULL, NULL, NULL);
            groupFrequencyMap.clear();
            for (const auto& [symbol, frequency] : sortedFrequencyInFiles) {
                std::wstring group = GetSymbolGroup(symbol);
                groupFrequencyMap[group] += frequency;
            }

            DrawPieChart(hwndChartWindow, groupFrequencyMap);
        }
        else if (LOWORD(wParam) == 11) {
            OPENFILENAME ofn = {};
            WCHAR filePath[MAX_PATH] = {};

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Текстовые файлы (*.txt)\0*.txt\0Все файлы (*.*)\0*.*\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"txt";

            if (GetSaveFileName(&ofn)) {
                saveStatsPath = filePath;
                MessageBox(hwnd, L"Путь для сохранения статистики установлен.", L"Информация", MB_OK);
            }
            
        }
        else if (LOWORD(wParam) == 12) {
            OPENFILENAME ofn = {};
            WCHAR filePath[MAX_PATH] = {};

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"Текстовые файлы (*.txt)\0*.txt\0Все файлы (*.*)\0*.*\0";
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"txt";

            if (GetSaveFileName(&ofn)) {
                saveFilesInfoPath = filePath;
                MessageBox(hwnd, L"Путь для сохранения информации о файлах установлен.", L"Информация", MB_OK);
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        KillTimer(hwnd, TIMER_ID);
        statsFile.flush();
        statsFile.close();
        break;

    case WM_CLOSE: {
        int result = MessageBox(
            hwnd,
            L"Вы действительно хотите выйти? Некоторые данные могут быть не сохранены",
            L"Предупреждение",
            MB_YESNO | MB_ICONQUESTION
        );
        if (result == IDYES) {
            isSaving = false;
            DestroyWindow(hwnd); 
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == 'S') { 
            SendMessage(hwnd, WM_COMMAND, 11, 0);
        }
        else if (wParam == 'F') { 
            SendMessage(hwnd, WM_COMMAND, 12, 0);
        }
        break;
    }
    case WM_NOTIFY: {
        if (((LPNMHDR)lParam)->code == LVN_GETDISPINFO) {
            NMLVDISPINFO* pDispInfo = (NMLVDISPINFO*)lParam;
            LVITEM* pItem = &pDispInfo->item;

            if (pItem->mask & LVIF_TEXT) {
                int index = pItem->iItem;

                if (index >= 0 && index < sortedFrequencyInFiles.size()) {
                    char32_t symbol = sortedFrequencyInFiles[index].first;
                    uint64_t count = sortedFrequencyInFiles[index].second;

                    std::wstring utf16Symbol = toUtf16(toUtf8(symbol));
                    double percentage = (static_cast<double>(count) / totalSymbolCount) * 100.0;
                    wchar_t buffer[256];
                    switch (pItem->iSubItem) {
                    case 0: 
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, utf16Symbol.c_str(), _TRUNCATE);
                        break;
                    case 1: 
                        swprintf(buffer, 256, L"%llu", count);
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, buffer, _TRUNCATE);
                        break;
                    case 2: 
                        swprintf(buffer, 256, L"%.8f", percentage);
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, buffer, _TRUNCATE);
                        break;
                    case 3:
                        swprintf(buffer, 256, L"U+%04X", symbol);
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, buffer, _TRUNCATE);
                        break;
                    case 4:
                        swprintf(buffer, 256, L"0x%04X", symbol);
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, buffer, _TRUNCATE);
                        break;
                    
                    case 5:
                        std::wstring group = GetSymbolGroup(symbol);
                        wcsncpy_s(pItem->pszText, pItem->cchTextMax, group.c_str(), _TRUNCATE);
                        break;
                    }
                }
            }
        }
        break;
    }

    case WM_UPDATE_PROGRESS:
    {
        fileAnalized++;
        if (totalFindFiles > 0) {
            int progress = static_cast<int>((static_cast<float>(fileAnalized) / totalFindFiles) * 100);
            SendMessage(hwndProgressBar, PBM_SETPOS, progress, 0); 

        }
    }
    break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}


int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
    InitCommonControls();
    isDrawPieChart = false;
    WCHAR modulePath[MAX_PATH];
    GetModuleFileName(NULL, modulePath, MAX_PATH);
    std::wstring baseDir = modulePath;
    size_t pos = baseDir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        baseDir = baseDir.substr(0, pos);
    }

    saveStatsPath = baseDir + L"\\stats.txt";
    saveFilesInfoPath = baseDir + L"\\files_info.txt";

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"MainWindowClass";
    RegisterClass(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"Symbol Frequency Analyzer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE,
        0, 0, screenWidth, screenHeight, 
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );
    if (hwnd == NULL) {
        return 0;
    }
    AddMenu(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
