#pragma once
#define SYMBOL_ANALYZER_H

#include <windows.h>
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
#include <codecvt> 

#define _CRT_SECURE_NO_WARNINGS

std::string toUtf8(char32_t symbol);
template<typename T>
void merge(std::vector<T>& arr, std::vector<T>& temp, int left, int mid, int right) {

    int i = left;      
    int j = mid + 1;   
    int k = left;      

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

    while (i <= mid) {
        temp[k] = arr[i];
        ++i;
        ++k;
    }

    while (j <= right) {
        temp[k] = arr[j];
        ++j;
        ++k;
    }
    for (int l = left; l <= right; ++l) {
        arr[l] = temp[l];
    }
}
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

template<typename T>
void customSort(std::vector<T>& arr) {
    if (!arr.empty()) {
        mergeSortIterative(arr);
    }
}

void SaveStatistics(std::ofstream& statsFile, std::vector<std::pair<char32_t, uint64_t>>& sortedFrequency, uint64_t totalSymbolCount, bool& isSaving);

std::wstring toUtf16(const std::string& utf8Str);
std::wstring FormatFileSize(std::streamsize fileSize);