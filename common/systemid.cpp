#include "systemid.h"
#include "config.h"
#include "util.h"
#include "timer.h"
#include <psapi.h>
#include "Windows.h"
#include "Shlobj.h"
#include <TlHelp32.h>
#include <algorithm>
#include <sstream>
#pragma comment(lib, "psapi.lib")
#pragma comment( lib, "rpcrt4.lib" )

Systemid::Systemid() : logger("gproxy.log", Logging::Subsystem::SYSTEMID, CConfig::GetBool("debug", false), CConfig::GetBool("telemetry", false)) {}

string Systemid :: getInstallPath() {
	CRegKey reg;
	string s;
	if (reg.Open(HKEY_CURRENT_USER, "Software\\Blizzard Entertainment\\Warcraft III", KEY_READ | KEY_WOW64_64KEY)==ERROR_SUCCESS) {
		s=this->getRegString(reg, "InstallPath");
		reg.Close();
		return s;
	}
	else return "";
}

string Systemid :: DWORD2STR(DWORD d) {
   ostringstream stream;
   stream << d;
   string str = stream.str();
   return str;
}

string Systemid :: getRegString(CRegKey reg, string name) {
	LPTSTR szBuffer = new TCHAR[MAX_PATH];
	ULONG bufsize=MAX_PATH;
	DWORD s = reg.QueryStringValue(name.c_str(), szBuffer, &bufsize);
	if (s == ERROR_SUCCESS) {
		reg.QueryStringValue(name.c_str(), szBuffer, &bufsize);
		string r( szBuffer );
		return r;
	}
	else if (s == ERROR_INVALID_DATA) {
		return "";
	}
	else if (s == ERROR_MORE_DATA) {
		return "";
	}
	else {
		return "";
	}
}

bool Systemid :: getRegMultiString(CRegKey reg, string name, char * buffer, ULONG * bufsize) {
    DWORD s = reg.QueryMultiStringValue(name.c_str(), buffer, bufsize);
    if (s == ERROR_SUCCESS) {
        return true;
    }
    else {
        return false;
    }
}

DWORD Systemid :: getRegDWORD(CRegKey reg, string name) {
	DWORD buf;
	DWORD s = reg.QueryDWORDValue(name.c_str(), buf);
	if (s == ERROR_SUCCESS) {
		reg.QueryDWORDValue(name.c_str(), buf);
		return buf;
	}
	else if (s == ERROR_INVALID_DATA) {
		return -1;
	}
	else {
		return -1;
	}
}

bool Systemid :: setRegString(CRegKey reg, string name, string value) {
	DWORD s = reg.SetStringValue(name.c_str(), value.c_str());
	if (s==ERROR_SUCCESS) {
		return true;
	}
	else {
		return false;
	}
}

bool Systemid::setRegDWORD(CRegKey reg, string name, DWORD value) {
	DWORD s = reg.SetDWORDValue(name.c_str(), value);
	if (s == ERROR_SUCCESS) {
		return true;
	}
	else {
		logger.error("Error in setRegDWORD, code " + UTIL_ToString(s));
		return false;
	}
}

bool Systemid :: setEuroString(string name, string value) {
	CRegKey reg;
	if (reg.Open(HKEY_CURRENT_USER, "Software\\Eurobattle.net", KEY_WRITE | KEY_WOW64_64KEY)==ERROR_SUCCESS) {
		if (!this->setRegString(reg, name, value)) return false;
		reg.Close();
		return true;
	}
	else {
		return false;
	}
}

bool Systemid::setW3HostPort(int value) {
	CRegKey reg;
	if (reg.Open(HKEY_CURRENT_USER, "Software\\Blizzard Entertainment\\Warcraft III\\Gameplay", KEY_WRITE | KEY_WOW64_64KEY) == ERROR_SUCCESS) {
		if (!this->setRegDWORD(reg, "netgameport", value)) {
			logger.info("Failed to set netgameport to " + UTIL_ToString(value));
			return false;
		}
		reg.Close();
		logger.info("Successfully set netgameport to " + UTIL_ToString(value));
		return true;
	}
	else {
		return false;
	}
}

bool Systemid::createLoaderKey() {
    CRegKey reg;
    if(reg.Create(HKEY_CURRENT_USER,  _T("Software\\Loader"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, NULL)==ERROR_SUCCESS) {
        reg.Close();
        return true;
    }
    else {
        reg.Close();
        return false;
    }
}

bool Systemid::createEuroKey() {
	CRegKey reg;
	if (reg.Create(HKEY_CURRENT_USER, _T("Software\\Eurobattle.net"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, NULL) == ERROR_SUCCESS) {
		logger.info("Created euro reg key");
		reg.Close();
		return true;
	}
	else {
		reg.Close();
		return false;
	}
}

bool Systemid::createBlizzKey() {
	CRegKey reg;
	if (reg.Create(HKEY_CURRENT_USER, _T("Software\\Blizzard Entertainment\\Warcraft III"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, NULL) == ERROR_SUCCESS) {
		logger.info("Created blizz reg key");
		reg.Close();
		return true;
	}
	else {
		reg.Close();
		return false;
	}
}

bool Systemid::createPlinkKey() {
	CRegKey reg;
	if (reg.Create(HKEY_CURRENT_USER, _T("Software\\SimonTatham\\PuTTY\\SshHostKeys"), REG_NONE, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, NULL) == ERROR_SUCCESS) {
		logger.info("Created plink reg key");
		reg.Close();
		return true;
	}
	else {
		reg.Close();
		return false;
	}
}

bool Systemid::go() {

	createEuroKey();
	createBlizzKey();

	return true;
}

// Is a process running by name?
bool Systemid::isRunning(string pName) {

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (_stricmp(entry.szExeFile, pName.c_str()) == 0)
			{
				CloseHandle(snapshot);
				return true;
			}
		}
	}

	CloseHandle(snapshot);
	return false;

}

// Is any of processes by name running?
string Systemid::isAnyRunning(vector<std::pair<string,string>> names) {
	unsigned long aProcesses[1024], cbNeeded, cProcesses;
	if(!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
		return "";

	cProcesses = cbNeeded / sizeof(unsigned long);
	for(unsigned int i = 0; i < cProcesses; i++)
	{
		if(aProcesses[i] == 0)
			continue;
		
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, aProcesses[i]);
		char buffer[MAX_PATH];
		GetModuleBaseName(hProcess, 0, buffer, sizeof(buffer)/sizeof(char));
		CloseHandle(hProcess);

		string tmp = string(buffer); //process base name
		transform( tmp.begin( ), tmp.end( ), tmp.begin( ), ::tolower );
		
		for(std::pair<string,string> s : names) {
			string hName = s.first;
			transform(hName.begin( ), hName.end( ), hName.begin( ), ::tolower );
			if(tmp==hName) {
				return tmp;
			}
		}		
	}
	return "";
}

// Get PID from process name
DWORD Systemid::getPidByName(string pName) {
	unsigned long aProcesses[1024], cbNeeded, cProcesses;
	if(!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
		return false;

	cProcesses = cbNeeded / sizeof(unsigned long);
	for(unsigned int i = 0; i < cProcesses; i++)
	{
		if(aProcesses[i] == 0)
			continue;
		
		HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, aProcesses[i]);
		char buffer[MAX_PATH];
		GetModuleBaseName(hProcess, 0, buffer, sizeof(buffer)/sizeof(char));

		string tmp = string(buffer); //process base name
		transform( tmp.begin( ), tmp.end( ), tmp.begin( ), (int(*)(int))tolower );
		transform( pName.begin( ), pName.end( ), pName.begin( ), (int(*)(int))tolower );
		//logger.info(tmp+" "+pName);
		if(tmp==pName) {
			DWORD ret = GetProcessId(hProcess);
			CloseHandle(hProcess);
			return ret;
		}
		CloseHandle(hProcess);
	}
	return -1;
}

string Systemid::getW3ExeVersion(string t)
{
	DWORD verHandle = NULL;
	UINT size = 0;
	LPBYTE lpBuffer = NULL;
	DWORD verSize = GetFileVersionInfoSize(t.c_str(), &verHandle);

	if (verSize != NULL)
	{
		LPSTR verData = new char[verSize];

		if (GetFileVersionInfo(t.c_str(), verHandle, verSize, verData))
		{
			if (VerQueryValue(verData, _T("\\"), (VOID FAR* FAR*)&lpBuffer, &size))
			{
				if (size)
				{
					VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
					if (verInfo->dwSignature == 0xfeef04bd)
					{
						int major = HIWORD(verInfo->dwFileVersionMS);
						int minor = LOWORD(verInfo->dwFileVersionMS);
						int revision = HIWORD(verInfo->dwFileVersionLS);
						int build = LOWORD(verInfo->dwFileVersionLS);
						TCHAR buffer[255];
						sprintf(buffer, _T("%d.%d.%d.%d"), major, minor, revision, build);
						delete[] verData;
						return buffer;
					}
				}
			}
		}
		else {
			delete[] verData;
		}
	}
	return "";
}

bool Systemid::setPfRegKey(string key, string value) {
	
	createPlinkKey();
	
	CRegKey reg;
	if (reg.Open(HKEY_CURRENT_USER, "Software\\SimonTatham\\PuTTY\\SshHostKeys", KEY_WRITE | KEY_WOW64_64KEY) == ERROR_SUCCESS) {
		if (!this->setRegString(reg, key, value)) {
			reg.Close();
			logger.error("Could not set pf registry entry");
			return false;
		}
		else {
			logger.debug("Pf configuration is set");
		}
	}
	else {
		logger.error("Could not access pf registry. Please contact tech support.");
		return false;
	}
	reg.Close();

	return true;
}