
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
#include <codecvt> // Для конвертации кодировок

#define _CRT_SECURE_NO_WARNINGS

// Grouping symbols by ranges

// Mutexes and shared variables


// Function declarations
std::string toUtf8(char32_t symbol);
template<typename T>
void merge(std::vector<T>& arr, std::vector<T>& temp, int left, int mid, int right);
template<typename T>
void mergeSortIterative(std::vector<T>& arr);
template<typename T>
void customSort(std::vector<T>& arr);
void SaveStatistics(std::ofstream& statsFile, const std::unordered_map<char32_t, uint64_t>& globalFrequency, uint64_t totalSymbolCount);
