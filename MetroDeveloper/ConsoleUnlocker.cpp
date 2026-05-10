#include "ConsoleUnlocker.h"
#include "uconsole.h"
#include "Utils.h"

void ConsoleUnlocker::clevel_r_on_key_press(int action, int key, int state, int resending)
{
	if (action == 39 || key == 87) // bind console || F11
	{
#ifdef _WIN64
		if (Utils::isExodus()) {
			uconsole_server_exodus** console = (uconsole_server_exodus**)Utils::GetConsole();
			(*console)->show(console);
		} else {
			uconsole_server** console = (uconsole_server**)Utils::GetConsole();
			(*console)->show(console);
		}
#else
		uconsole_server** console = (uconsole_server**)Utils::GetConsole();
		(*console)->show(console);
#endif
	}
}
