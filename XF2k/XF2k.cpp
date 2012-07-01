#include <cctype>
#include <cstring>

#include <utility>

#include <vector>
#include <string>
#include <set>
#include <map>

#include <algorithm>
#include <functional>

using namespace std;

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include "xchat-plugin.h"

const wchar_t *className = L"XF2k Message Receiver Window Class";
const wchar_t *windowName = L"XF2k Message Receiver Window";

char pluginName[] = "XF2k";
char pluginVersion[] = "1.1.0";
char pluginDescription[] = "Fetches music information from foobar2000.";

const char *usageXF = "Usage: XF | \xE3\x85\x8C\xE3\x84\xB9, says the current song information.";
const char *usageXFCFG = "Usage: XFCFG <name> [<value> | default], sets a XF2k option. <name> can be: FORMAT, WAITDELAY, METHOD. If <value> is empty, shows the current value. If <value> is \"default\", restores default value. FORMAT option can be any foobar2000 format string. WAITDELAY option must be a positive integer, and its unit must be milliseconds. METHOD must be: SAY, ME, ALLCHAN SAY, ALLCHAN ME, ALLCHANL SAY, or ALLCHANL ME.";

const char *stoppedGetInfo = "XF2k - foobar2000 is currently stopped.";
const char *failedGetInfo = "XF2k - Failed to fetch song information.";
const char *failedGetInfoFindWindow = "XF2k - Cannot find XF2kProvider window. Is foobar2000 running or XF2kProvider installed correctly?";
const char *failedGetInfoSendMessage = "XF2k - XF2kProvider does not respond. Is foobar2000 frozen?";

const char *printConfigGet = "XF2k - Current value of option \"%s\" is \"%s\".";
const char *succeededConfigSet = "XF2k - Option \"%s\" is set successfully.";
const char *failedConfigSet = "XF2k - Failed to set option \"%s\".";

wchar_t pluginConfigPath[512];

map<string, string> defaultOptionValue;
set<string> allowedMethod;

HINSTANCE instance;
HWND window;

xchat_plugin *ph;

LRESULT __stdcall WndProc(HWND, unsigned, WPARAM, LPARAM);
void initModuleInfo();
void uninitModuleInfo();
wstring readConfig(const wstring &, const wstring & = wstring());
bool writeConfig(const wstring &, const wstring &);
string UTFConv(const wstring &);
wstring UTFConv(const string &);
void windowCreate();
void windowDestroy();

#ifdef __cplusplus
extern "C"
{
#endif

void xchat_plugin_get_info(char **, char **, char **, void **);
int xchat_plugin_init(xchat_plugin *, char **, char **, char **, char *);
int xchat_plugin_deinit();

#ifdef __cplusplus
};
#endif

int xf2kGetInfo(char *[], char *[], void *);
int xf2kConfig(char *[], char *[], void *);

BOOL __stdcall DllMain(HANDLE module, DWORD reason, LPVOID)
{
	switch(reason)
	{
	case DLL_PROCESS_ATTACH:
		{
			instance = reinterpret_cast<HINSTANCE>(module);
			initModuleInfo();
			windowCreate();
		}
		break;

	case DLL_PROCESS_DETACH:
		{
			windowDestroy();
			uninitModuleInfo();
		}
		break;
	}

	return TRUE;
}

LRESULT __stdcall WndProc(HWND window, unsigned message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_COPYDATA:
		{
			COPYDATASTRUCT *cds = reinterpret_cast<COPYDATASTRUCT *>(lParam);
			string str;

			switch(cds->dwData)
			{
			case 0:
				str = static_cast<char *>(cds->lpData);
				break;

			case 1:
				str = stoppedGetInfo;
				break;

			default:
				str = failedGetInfo;
				break;
			}	

			if(cds->dwData == 0)
			{
				string method(UTFConv(readConfig(L"METHOD", UTFConv(defaultOptionValue["METHOD"]))));
				if(allowedMethod.find(method) == allowedMethod.end())
					method = defaultOptionValue["METHOD"];
				xchat_commandf(ph, "%s %s", method.c_str(), str.c_str());
			}
			else
				xchat_print(ph, str.c_str());
		}
		return 1;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(window, message, wParam, lParam);
}

void initModuleInfo()
{
	wchar_t *p;
	
	GetModuleFileNameW(reinterpret_cast<HMODULE>(instance), pluginConfigPath, 512);
	p = wcsrchr(pluginConfigPath, L'\\');
	if(p != NULL)
	{
		*(++ p) = L'\0';
		wcscat(p, L"XF2k.ini");
	}
	else
		wcscpy(pluginConfigPath, L".\\XF2k.ini");

	defaultOptionValue.insert(make_pair(string("FORMAT"), string("'['$if(%ispaused%,Paused,Playing)']' ['['%album artist% - ['['%date%'] ']%album%'] '][[CD%discnumber% ][#%tracknumber%. ]][%artist% - ]%title% '('%playback_time%[ / %length%]')'")));
	defaultOptionValue.insert(make_pair(string("WAITDELAY"), string("5000")));
	defaultOptionValue.insert(make_pair(string("METHOD"), string("SAY")));

	allowedMethod.insert("SAY");
	allowedMethod.insert("ME");
	allowedMethod.insert("ALLCHAN SAY");
	allowedMethod.insert("ALLCHAN ME");
	allowedMethod.insert("ALLCHANL SAY");
	allowedMethod.insert("ALLCHANL ME");
}

void uninitModuleInfo()
{
}

wstring readConfig(const wstring &name, const wstring &def)
{
	vector<wchar_t> buf(0x500);
	static wstring appName(UTFConv(pluginName));
	GetPrivateProfileStringW(appName.c_str(), name.c_str(), def.c_str(), &*buf.begin(), 0x500, pluginConfigPath);
	return wstring(&*buf.begin());
}

bool writeConfig(const wstring &name, const wstring &value)
{
	static wstring appName(UTFConv(pluginName));
	return WritePrivateProfileStringW(appName.c_str(), name.c_str(), (L"\"" + value + L"\"").c_str(), pluginConfigPath) != 0;
}

string UTFConv(const wstring &str)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.size(), NULL, 0, NULL, NULL);
	if(size <= 0)
		return string();

	vector<char> buf(static_cast<size_t>(size));

	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.size(), &*buf.begin(), size, NULL, NULL);
	return string(buf.begin(), buf.end());
}

wstring UTFConv(const string &str)
{
	int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), NULL, 0);
	if(size <= 0)
		return wstring();

	vector<wchar_t> buf(static_cast<size_t>(size));

	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), &*buf.begin(), size);
	return wstring(buf.begin(), buf.end());
}

void windowCreate()
{
	WNDCLASSW wc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = NULL;
	wc.hCursor = NULL;
	wc.hIcon = NULL;
	wc.hInstance = instance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = className;
	wc.lpszMenuName = NULL;
	wc.style = 0;
	RegisterClassW(&wc);

	window = CreateWindowExW(0, className, windowName, 0, 0, 0, 0, 0, NULL, NULL, instance, NULL);
	ShowWindow(window, SW_HIDE);
}

void windowDestroy()
{
	DestroyWindow(window);
	UnregisterClassW(className, instance);
}

void xchat_plugin_get_info(char **name, char **desc, char **version, void **reserved)
{
	*name = pluginName;
	*desc = pluginDescription;
	*version = pluginVersion;
}

int xchat_plugin_init(xchat_plugin *pluginHandle, char **name, char **desc, char **version, char *arg)
{
	ph = pluginHandle;

	*name = pluginName;
	*desc = pluginDescription;
	*version = pluginVersion;

	xchat_hook_command(ph, "XF", XCHAT_PRI_NORM, xf2kGetInfo, usageXF, NULL);
	xchat_hook_command(ph, "\xE3\x85\x8C\xE3\x84\xB9", XCHAT_PRI_NORM, xf2kGetInfo, usageXF, NULL);
	xchat_hook_command(ph, "XFCFG", XCHAT_PRI_NORM, xf2kConfig, usageXFCFG, NULL);
	xchat_printf(ph, "%s %s loaded.", pluginName, pluginVersion);

	return 1;
}

int xchat_plugin_deinit()
{
	return 1;
}

int xf2kGetInfo(char *word[], char *eol[], void *userdata)
{
	HWND target = FindWindowW(L"XF2kProvider Message Receiver Window Class", NULL);
	if(window == NULL)
	{
		xchat_print(ph, failedGetInfoFindWindow);
		return XCHAT_EAT_ALL;
	}

	COPYDATASTRUCT cds;
	memset(&cds, 0, sizeof(cds));

	string format(UTFConv(readConfig(L"FORMAT", UTFConv(defaultOptionValue["FORMAT"]))));
	vector<char> buf(format.begin(), format.end());
	buf.push_back('\0');
	cds.cbData = buf.size() * sizeof(char);
	cds.lpData = &*buf.begin();

	if(!SendMessageTimeoutW(target, WM_COPYDATA, reinterpret_cast<WPARAM>(window), reinterpret_cast<LPARAM>(&cds), SMTO_NORMAL, _wtoi(readConfig(L"WAITDELAY", UTFConv(defaultOptionValue["WAITDELAY"])).c_str()), NULL))
	{
		xchat_print(ph, failedGetInfoSendMessage);
		return XCHAT_EAT_ALL;
	}

	return XCHAT_EAT_ALL;
}

int xf2kConfig(char *word[], char *eol[], void *userdata)
{
	if(eol[2][0] == '\0') // XFCFG <name> [<value>]
	{
		xchat_print(ph, usageXFCFG);
		return XCHAT_EAT_ALL;
	}

	string name(word[2]);
	string value(eol[3]);

	transform(name.begin(), name.end(), name.begin(), toupper);

	if(name == "FORMAT")
	{
		if(value == "default")
			value = defaultOptionValue[name];
	}
	else if(name == "WAITDELAY")
	{
		if(!value.empty() && (value == "default" || atoi(value.c_str()) <= 0))
			value = defaultOptionValue[name];
		value.erase(remove_if(value.begin(), value.end(), not1(ptr_fun(isdigit))), value.end());
	}
	else if(name == "METHOD")
	{
		transform(value.begin(), value.end(), value.begin(), toupper);
		if(!value.empty() && (value == "DEFAULT" || allowedMethod.find(value) == allowedMethod.end()))
			value = defaultOptionValue[name];
	}
	else
	{
		xchat_print(ph, usageXFCFG);
		return XCHAT_EAT_ALL;
	}

	if(value.empty())
		xchat_printf(ph, printConfigGet, name.c_str(), UTFConv(readConfig(UTFConv(name), UTFConv(defaultOptionValue[name]))).c_str());
	else if(writeConfig(UTFConv(name), UTFConv(value)))
		xchat_printf(ph, succeededConfigSet, name.c_str());
	else
		xchat_printf(ph, failedConfigSet, name.c_str());

	return XCHAT_EAT_ALL;
}
