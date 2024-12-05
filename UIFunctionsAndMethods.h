#pragma once
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <mutex>
#include <queue>
#pragma comment(lib, "comctl32.lib")
void AddItemToListView(HWND hListView, const std::wstring& filePath, const std::wstring& fileSizeStr,
    const std::wstring& endTimeStr, const std::wstring& durationStr, std::mutex& mtx);

void AddColumns(HWND hListView);

HWND CreateListView(HWND hwndParent);

void ClearChartArea(HWND hwnd);

HWND CreateComboBox(HWND hwndParent, std::vector<std::pair<std::wstring, std::vector<std::pair<char32_t, char32_t>>>> symbolGroups);

void AddColumnsForLog(HWND hListView);

HWND CreateListViewForLog(HWND hwndParent);

void DrawPieChart(HWND hwnd, const std::map<std::wstring, uint64_t>& groupFrequency);

void AddMenu(HWND hwnd);
