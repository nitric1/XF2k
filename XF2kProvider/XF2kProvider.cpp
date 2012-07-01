/*
#define WINVER 0x0500
#define _WIN32_WINDOWS 0x0410
#define _WIN32_WINNT 0x0500
#define _WIN32_IE 0x0600
*/

#include <vector>
#include <string>

using namespace std;

#include <windows.h>
#include <shlwapi.h>

#include "foobar2000/sdk/foobar2000.h"
#include "foobar2000/helpers/helpers.h"

DECLARE_COMPONENT_VERSION("XF2kProvider", "1.1.0", "Music Information Provider for XF2k\nby HNO3 <wdlee91 at gmail dot com>");

class XF2kProviderInitquit : public initquit
{
	virtual void on_init();
	virtual void on_quit();
};

initquit_factory_t<XF2kProviderInitquit> foo_initquit;

void initWindow();
void quitWindow();

void XF2kProviderInitquit::on_init()
{
	initWindow();
}

void XF2kProviderInitquit::on_quit()
{
	quitWindow();
}

int __stdcall getStatus()
{
	static_api_ptr_t<playback_control> pb;
	if(pb->is_playing())
	{
		if(pb->is_paused())
			return MCI_MODE_PAUSE;
		return MCI_MODE_PLAY;
	}
	return MCI_MODE_STOP;
}

string formatTime(double secf)
{
	unsigned sec = static_cast<unsigned>(secf);
	char buf[100];

	if(sec < 3600)
		sprintf(buf, "%d:%02d", sec / 60, sec % 60);
	else
		sprintf(buf, "%d:%02d:%02d", sec / 3600, (sec / 60) % 60, sec % 60);

	return string(buf);
}

void stringReplace(string &str, const string &from, const string &to)
{
	size_t pos = 0;
	while(true)
	{
		pos = str.find(from, pos);
		if(pos == string::npos)
			break;
		str.replace(pos, from.size(), to);
	}
}

LRESULT __stdcall WndProc(HWND window, unsigned msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_COPYDATA:
		{
			HWND target = reinterpret_cast<HWND>(wParam);
			COPYDATASTRUCT *rcv = reinterpret_cast<COPYDATASTRUCT *>(lParam);
			COPYDATASTRUCT snd;
			const char *str = static_cast<char *>(rcv->lpData);
			char sndstr[0x1000];
			static_api_ptr_t<playback_control> pb;
			static_api_ptr_t<titleformat_compiler> tfc;
			service_ptr_t<titleformat_object> script;

			snd.cbData = 0;
			snd.lpData = NULL;
			snd.dwData = 2;

			if(!pb->is_playing())
			{
				snd.dwData = 1;
			}
			if(str != NULL)
			{
				tfc->compile_safe_ex(script, str);
				pfc::string_formatter stro;

				if(pb->playback_format_title(nullptr, stro, script, nullptr, playback_control::display_level_all))
				{
					strcpy(sndstr, stro.get_ptr());

					snd.cbData = (strlen(sndstr) + 1) * sizeof(char);
					snd.lpData = sndstr;
					snd.dwData = 0;
				}

				/*string fstr(str);
				stringReplace(fstr, "%playback_time%", formatTime(pb->playback_get_position()));
				stringReplace(fstr, "$if(%ispaused%,", pb->is_paused() ? "$if(%path%," : "$if(0,");

				metadb_handle_ptr p;
				if(pb->get_now_playing(p))
				{
					pfc::string8 stro;
					if(p->format_title_legacy(NULL, stro, fstr.c_str(), NULL))
					{
						strcpy(sndstr, stro.get_ptr());

						snd.cbData = (strlen(sndstr) + 1) * sizeof(char);
						snd.lpData = sndstr;
						snd.dwData = 0;
					}
				}*/
			}

			SendMessageW(target, WM_COPYDATA, reinterpret_cast<WPARAM>(window), reinterpret_cast<LPARAM>(&snd));
		}
		return 1;
	}

	return DefWindowProcW(window, msg, wParam, lParam);
}

HWND window;

void initWindow()
{
	WNDCLASSW wc;

	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = NULL;
	wc.hCursor = NULL;
	wc.hIcon = NULL;
	wc.hInstance = core_api::get_my_instance();
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"XF2kProvider Message Receiver Window Class";
	wc.lpszMenuName = NULL;
	wc.style = 0;

	if(!RegisterClassW(&wc))
		return;

	window = CreateWindowExW(0, L"XF2kProvider Message Receiver Window Class", L"XF2kProvider Message Receiver Window", 0, 0, 0, 0, 0, NULL, NULL, core_api::get_my_instance(), NULL);
	if(window == NULL)
		return;

	ShowWindow(window, SW_HIDE);
}

void quitWindow()
{
	DestroyWindow(window);
}
