// Minimal CRegKey wrapper for MinGW (replaces ATL's CRegKey)
#pragma once

#ifdef __MINGW32__

// Add _T macro for text strings
#ifndef _T
#define _T(x) x
#endif

#include <windows.h>
#include <string>

class CRegKey {
private:
    HKEY m_hKey;

public:
    CRegKey() : m_hKey(NULL) {}

    ~CRegKey() {
        Close();
    }

    LONG Open(HKEY hKeyParent, LPCSTR lpszKeyName, REGSAM samDesired = KEY_READ) {
        Close();
        return RegOpenKeyExA(hKeyParent, lpszKeyName, 0, samDesired, &m_hKey);
    }

    LONG Create(HKEY hKeyParent, LPCSTR lpszKeyName, LPSTR lpszClass = NULL,
                DWORD dwOptions = REG_OPTION_NON_VOLATILE, REGSAM samDesired = KEY_WRITE,
                LPSECURITY_ATTRIBUTES lpSecAttr = NULL, LPDWORD lpdwDisposition = NULL) {
        Close();
        return RegCreateKeyExA(hKeyParent, lpszKeyName, 0, lpszClass, dwOptions,
                               samDesired, lpSecAttr, &m_hKey, lpdwDisposition);
    }

    LONG SetStringValue(LPCSTR pszValueName, LPCSTR pszValue) {
        if (!m_hKey) return ERROR_INVALID_HANDLE;
        return RegSetValueExA(m_hKey, pszValueName, 0, REG_SZ,
                              (const BYTE*)pszValue, (DWORD)(strlen(pszValue) + 1));
    }

    LONG SetDWORDValue(LPCSTR pszValueName, DWORD dwValue) {
        if (!m_hKey) return ERROR_INVALID_HANDLE;
        return RegSetValueExA(m_hKey, pszValueName, 0, REG_DWORD,
                              (const BYTE*)&dwValue, sizeof(DWORD));
    }

    LONG QueryStringValue(LPCSTR pszValueName, LPSTR pszValue, ULONG* pnChars) {
        if (!m_hKey) return ERROR_INVALID_HANDLE;
        DWORD dwType = 0;
        DWORD dwSize = *pnChars;
        LONG result = RegQueryValueExA(m_hKey, pszValueName, NULL, &dwType,
                                       (LPBYTE)pszValue, &dwSize);
        *pnChars = dwSize;
        return result;
    }

    LONG QueryDWORDValue(LPCSTR pszValueName, DWORD& dwValue) {
        if (!m_hKey) return ERROR_INVALID_HANDLE;
        DWORD dwType = 0;
        DWORD dwSize = sizeof(DWORD);
        return RegQueryValueExA(m_hKey, pszValueName, NULL, &dwType,
                                (LPBYTE)&dwValue, &dwSize);
    }

    LONG QueryMultiStringValue(LPCSTR pszValueName, LPSTR pszValue, ULONG* pnChars) {
        if (!m_hKey) return ERROR_INVALID_HANDLE;
        DWORD dwType = 0;
        DWORD dwSize = *pnChars;
        LONG result = RegQueryValueExA(m_hKey, pszValueName, NULL, &dwType,
                                       (LPBYTE)pszValue, &dwSize);
        *pnChars = dwSize;
        return result;
    }

    void Close() {
        if (m_hKey) {
            RegCloseKey(m_hKey);
            m_hKey = NULL;
        }
    }

    operator HKEY() const { return m_hKey; }
    HKEY m_hkey() const { return m_hKey; }
};

#endif // __MINGW32__
