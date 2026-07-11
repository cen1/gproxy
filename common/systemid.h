#pragma once

#include <string>
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include "log.h"
#include <vector>
#include <windows.h>
#ifdef __MINGW32__
#include "mingw_atlbase.h"
#else
#include <atlbase.h>
#endif

using std::string;

class Systemid {

  private:
	Log logger;

  public:
	Systemid();

	string getInstallPath();
	string DWORD2STR(DWORD d);
	string getRegString(CRegKey reg, string name);
	bool getRegMultiString(CRegKey reg, string name, char* buffer, ULONG* bufsize);
	DWORD getRegDWORD(CRegKey reg, string name);
	bool setRegDWORD(CRegKey reg, string name, DWORD value);
	bool setRegString(CRegKey reg, string name, string value);
	bool setEuroString(string name, string value);
	bool createLoaderKey();
	bool createEuroKey();
	bool createBlizzKey();
	bool createPlinkKey();
	bool setW3HostPort(int value);
	string getW3ExeVersion(string t);
	bool setPfRegKey(string key, string value);

	// psapi
	static bool isRunning(string pName);
	static string isAnyRunning(vector<std::pair<string, string>> names);
	static DWORD getPidByName(string pName);

	// runner
	bool go();
};