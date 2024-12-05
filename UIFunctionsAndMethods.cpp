#define _CRT_SECURE_NO_WARNINGS
#include "UIFunctionsAndMethods.h"
void AddItemToListView(HWND hListView, const std::wstring& filePath, const std::wstring& fileSizeStr,
    const std::wstring& endTimeStr, const std::wstring& durationStr, std::mutex& mtx)
 {
     std::lock_guard<std::mutex> lock(mtx);

     LVITEM lvItem = {};
     lvItem.mask = LVIF_TEXT;


     lvItem.pszText = const_cast<LPWSTR>(filePath.c_str());
     int itemIndex = ListView_InsertItem(hListView, &lvItem);

     ListView_SetItemText(hListView, itemIndex, 1, const_cast<LPWSTR>(fileSizeStr.c_str()));

     ListView_SetItemText(hListView, itemIndex, 2, const_cast<LPWSTR>(endTimeStr.c_str()));

     ListView_SetItemText(hListView, itemIndex, 3, const_cast<LPWSTR>(durationStr.c_str()));
}

void AddColumns(HWND hListView) {
    LVCOLUMN lvCol = { };
    lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvCol.cx = 75;
    lvCol.pszText = (LPWSTR)L"Символ";
    ListView_InsertColumn(hListView, 0, &lvCol);

    lvCol.cx = 100;
    lvCol.pszText = (LPWSTR)L"Число встреч";
    ListView_InsertColumn(hListView, 1, &lvCol);

    lvCol.cx = 100;
    lvCol.pszText = (LPWSTR)L"Процент";
    ListView_InsertColumn(hListView, 2, &lvCol);

    lvCol.cx = 100;
    lvCol.pszText = (LPWSTR)L"Юникод";
    ListView_InsertColumn(hListView, 3, &lvCol);

    lvCol.cx = 100;
    lvCol.pszText = (LPWSTR)L"Hex";
    ListView_InsertColumn(hListView, 4, &lvCol);

    lvCol.cx = 175;
    lvCol.pszText = (LPWSTR)L"Группа выборки";
    ListView_InsertColumn(hListView, 5, &lvCol);
}

HWND CreateListView(HWND hwndParent) {
    HWND hwndListView = CreateWindowEx(0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER | LVS_OWNERDATA,
        20, 130, 675, 300, hwndParent, NULL, GetModuleHandle(NULL), NULL);
    ShowWindow(hwndListView, TRUE);
    AddColumns(hwndListView);
    return hwndListView;
}

void ClearChartArea(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    RECT chartRect = { 0, 0, 1200,1220 };

    HDC hdc = GetDC(hwnd);

    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &chartRect, brush);
    DeleteObject(brush);

    ReleaseDC(hwnd, hdc);
}

HWND CreateComboBox(HWND hwndParent, std::vector<std::pair<std::wstring, std::vector<std::pair<char32_t, char32_t>>>> symbolGroups) {
    HWND hwndComboBox = CreateWindow(WC_COMBOBOX, L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL,
        20, 50, 200, 200, hwndParent, NULL, GetModuleHandle(NULL), NULL);

    for (const auto& [name, _] : symbolGroups) {
        SendMessage(hwndComboBox, CB_ADDSTRING, 0, (LPARAM)name.c_str());
    }
    SendMessage(hwndComboBox, CB_SETCURSEL, (WPARAM)9, 0);
    return hwndComboBox;
}

void AddColumnsForLog(HWND hListView) {
    LVCOLUMN lvCol = { };
    lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvCol.cx = 350;
    lvCol.pszText = (LPWSTR)L"Файл";
    ListView_InsertColumn(hListView, 0, &lvCol);


    lvCol.cx = 75;
    lvCol.pszText = (LPWSTR)L"Размер";
    ListView_InsertColumn(hListView, 1, &lvCol);

    lvCol.cx = 120;
    lvCol.pszText = (LPWSTR)L"Конец анализа";
    ListView_InsertColumn(hListView, 2, &lvCol);

    lvCol.cx = 120;
    lvCol.pszText = (LPWSTR)L"Время анализа";
    ListView_InsertColumn(hListView, 3, &lvCol);
}

HWND CreateListViewForLog(HWND hwndParent) {
    HWND hwndListViewForLog = CreateWindowEx(0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER,
        20, 460, 675, 300, hwndParent, NULL, GetModuleHandle(NULL), NULL);
    ShowWindow(hwndListViewForLog, TRUE);
    AddColumnsForLog(hwndListViewForLog);
    return hwndListViewForLog;
}

void DrawPieChart(HWND hwnd, const std::map<std::wstring, uint64_t>& groupFrequency) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    HDC hdc = GetDC(hwnd);
    HBRUSH hBrushes[] = {
        CreateSolidBrush(RGB(255, 0, 0)),
        CreateSolidBrush(RGB(0, 255, 0)),
        CreateSolidBrush(RGB(0, 0, 255)),
        CreateSolidBrush(RGB(255, 255, 0)),
        CreateSolidBrush(RGB(255, 165, 0)),
        CreateSolidBrush(RGB(128, 0, 128)),
        CreateSolidBrush(RGB(0, 255, 255)),
        CreateSolidBrush(RGB(128, 128, 128)),
        CreateSolidBrush(RGB(0, 128, 0)),
        CreateSolidBrush(RGB(75, 0, 130)),
        CreateSolidBrush(RGB(255, 192, 203))
    };

    int total = 0;
    for (const auto& group : groupFrequency) {
        total += group.second;
    }

    int startAngle = 0;
    int index = 0;
    int centerX = (rect.left + rect.right) / 2;
    int centerY = (rect.top + rect.bottom) / 2;
    int radius = (rect.right - rect.left) / 2;

    const int minAngle = 2;
    int totalUsedAngles = 0;
    std::vector<int> angles;

    for (const auto& group : groupFrequency) {
        int sweepAngle = static_cast<int>(360.0 * group.second / total);
        if (sweepAngle < minAngle && group.second > 0) {
            sweepAngle = minAngle;
        }
        angles.push_back(sweepAngle);
        totalUsedAngles += sweepAngle;
    }
    if (totalUsedAngles > 360) {
        int excess = totalUsedAngles - 360;
        for (auto& angle : angles) {
            if (angle > minAngle) {
                int reduction = min(angle - minAngle, excess);
                angle -= reduction;
                excess -= reduction;
                if (excess <= 0) break;
            }
        }
    }

    for (size_t i = 0; i < angles.size(); ++i) {
        int sweepAngle = angles[i];
        double startRadians = startAngle * 3.14159 / 180.0;
        double endRadians = (startAngle + sweepAngle) * 3.14159 / 180.0;

        int startX = centerX + static_cast<int>(radius * cos(startRadians));
        int startY = centerY - static_cast<int>(radius * sin(startRadians));
        int endX = centerX + static_cast<int>(radius * cos(endRadians));
        int endY = centerY - static_cast<int>(radius * sin(endRadians));

        SelectObject(hdc, hBrushes[index % 11]);

        BeginPath(hdc);
        MoveToEx(hdc, centerX, centerY, NULL);
        LineTo(hdc, startX, startY);
        ArcTo(hdc, rect.left, rect.top, rect.right, rect.bottom, startX, startY, endX, endY);
        LineTo(hdc, centerX, centerY);
        EndPath(hdc);

        FillPath(hdc);

        startAngle += sweepAngle;
        ++index;
    }

    int legendX = rect.right + 20;
    int legendY = rect.top + 20;
    int legendBoxSize = 15;
    int lineHeight = 20;

    SetTextColor(hdc, RGB(0, 0, 0));
    SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    index = 0;
    for (const auto& group : groupFrequency) {

        SelectObject(hdc, hBrushes[index % 11]);
        Rectangle(hdc, legendX, legendY, legendX + legendBoxSize, legendY + legendBoxSize);
        std::wstring strInfo = group.first + L" " + std::to_wstring((static_cast<double>(group.second) / total) * 100) + L" %";

        TextOut(hdc, legendX + legendBoxSize + 5, legendY + 2, strInfo.c_str(), strInfo.length());

        legendY += lineHeight;
        ++index;
    }

    for (HBRUSH hBrush : hBrushes) {
        DeleteObject(hBrush);
    }

    ReleaseDC(hwnd, hdc);
}

void AddMenu(HWND hwnd) {
    HMENU hMenu = CreateMenu();
    HMENU hSubMenu = CreatePopupMenu();

    AppendMenu(hSubMenu, MF_STRING, 11, L"Сохранить статистику");
    AppendMenu(hSubMenu, MF_STRING, 12, L"Сохранить информацию о файлах");

    AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, L"Выбрать пути для сохранения данных");

    SetMenu(hwnd, hMenu);
}