#include "cmdbuffer.h"
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include "config.h"
#include "util.h"

CmdBuffer::CmdBuffer()
  : logger("gproxy.log", Logging::Subsystem::CMDBUFFER, CConfig::GetBool("debug", false), CConfig::GetBool("telemetry", false))
{
	cmdBufferEnable = true;
}

CmdBuffer::~CmdBuffer() {}

// press up and down arrows to cycle through command buffer
void CmdBuffer::start(DWORD w3pid)
{
	logger.debug("Chat command thread starting");

	HWND w_handle;
	DWORD pid;

	int counter = -1;

	bool statePressedUp = false;
	bool statePressedDown = false;
	bool ctrlYPressed = false;

	while (true) {
		// sleep interruption point
		boost::this_thread::sleep(boost::posix_time::milliseconds(55));

		// check if we are not in game
		bool isEnabled = false;
		mtxCmdBufferEnable.lock();
		if (cmdBufferEnable) {
			isEnabled = true;
		}
		mtxCmdBufferEnable.unlock();

		if (!isEnabled)
			continue; // skip this loop

		// check if w3 window is active
		w_handle = GetForegroundWindow();
		if (w_handle == NULL) {
			logger.debug("Window returned NULL");
			continue;
		}
		GetWindowThreadProcessId(w_handle, &pid);
		// logger.debug("Active window wpid is "+UTIL_ToString(w3pid)+" proc identifier is "+UTIL_ToString((int)pid));

		if (pid == w3pid) {
			// active window is w3
			// check for key UP or key DOWN
			// we need to detect if it changed from pressed to not pressed
			SHORT stateUp = GetKeyState(VK_UP);
			SHORT stateDown = GetKeyState(VK_DOWN);
			SHORT stateBack = GetKeyState(VK_BACK);
			SHORT stateReturn = GetKeyState(VK_RETURN);
			bool pressedUp = stateUp < 0;
			bool pressedDown = stateDown < 0;
			bool pressedBack = stateBack < 0;
			bool pressedReturn = stateReturn < 0;

			if (pressedUp) {
				// UP key is pressed
				logger.debug("UP key pressed");
				statePressedUp = true;
			}
			else if (pressedDown) {
				// DOWN key is pressed
				logger.debug("DOWN key pressed");
				statePressedDown = true;
			}
			else if (pressedBack) {
				counter = -1;
			}
			else if (pressedReturn) {
				counter = -1;
			}

			bool shouldDel = false;
			if (GetAsyncKeyState(VK_CONTROL) < 0 && GetAsyncKeyState(0x59) < 0) {
				ctrlYPressed = true;
			}
			else {
				if (ctrlYPressed) {
					// it was pressed but now not anymore
					ctrlYPressed = false;
					shouldDel = true;
				}
			}

			// del text buffer on ctrl+y release
			if (shouldDel) {
				for (int i = 0; i < 255; i++) {
					GenerateKey(VK_BACK);
				}
				counter = -1;
			}

			mtxChatBuffer.lock();
			if (!pressedUp && statePressedUp && chatBuffer.size() != 0) {
				if (counter < (int)(chatBuffer.size() - 1))
					counter++;

				logger.debug("3 UP key released, cnt " + UTIL_ToString(counter) + "/" + UTIL_ToString(chatBuffer.size()));
				// UP is not pressed but it was last cycle
				// key was released, reset state
				statePressedUp = false;

				// delete the previous text
				for (int i = 0; i < 255; i++) {
					GenerateKey(VK_BACK);
				}
				// output text at the current counter location
				for (unsigned int i = 0; i < chatBuffer[counter].length(); i++) {
					SHORT vkey_info = VkKeyScanEx(chatBuffer[counter][i], GetKeyboardLayout(0));
					GenerateKeyFromInfo(vkey_info);
				}
			}
			else if (!pressedDown && statePressedDown && chatBuffer.size() != 0) {
				if (counter != -1) {
					if (counter > 0)
						counter--;

					logger.debug(("DOWN key released, cnt " + UTIL_ToString(counter) + "/" + UTIL_ToString(chatBuffer.size())));
					// DOWN is not pressed but it was last cycle
					// key was released, reset state
					statePressedDown = false;

					// delete the previous text
					for (int i = 0; i < 255; i++) {
						GenerateKey(VK_BACK);
					}
					// output text at the current counter location
					writeText(chatBuffer[counter]);
				}
			}
			mtxChatBuffer.unlock();
		}
	}
	logger.debug("Main thread exited");
}

void CmdBuffer::addChatBuffer(string text)
{
	if (chatBuffer.size() > LIMIT) {
		chatBuffer.pop_back();
	}
	chatBuffer.push_front(text);
}

void CmdBuffer::GenerateKeyFromInfo(SHORT vkey_info)
{
	int vk = vkey_info & 0x00ff;
	int high_byte = (vkey_info & 0xff00) >> 8;

	INPUT ip1;
	ip1.type = INPUT_KEYBOARD;
	ip1.ki.wScan = 0;
	ip1.ki.time = 0;
	ip1.ki.dwExtraInfo = 0;

	INPUT ip2;
	ip2.type = INPUT_KEYBOARD;
	ip2.ki.wScan = 0;
	ip2.ki.time = 0;
	ip2.ki.dwExtraInfo = 0;

	INPUT ip3;
	ip3.type = INPUT_KEYBOARD;
	ip3.ki.wScan = 0;
	ip3.ki.time = 0;
	ip3.ki.dwExtraInfo = 0;

	INPUT ip4;
	ip4.type = INPUT_KEYBOARD;
	ip4.ki.wScan = 0;
	ip4.ki.time = 0;
	ip4.ki.dwExtraInfo = 0;

	if (high_byte == 1) {
		// shift must be pressed
		ip2.ki.wVk = VK_SHIFT;
		ip2.ki.dwFlags = 0; // 0 for key press
		SendInput(1, &ip2, sizeof(INPUT));
	}

	if (high_byte == 2) {
		// ctrl must be pressed
		ip3.ki.wVk = VK_CONTROL;
		ip3.ki.dwFlags = 0; // 0 for key press
		SendInput(1, &ip3, sizeof(INPUT));
	}

	if (high_byte == 4) {
		// alt must be pressed
		// logger.debug("ALT key detected");
		ip4.ki.wVk = VK_MENU;
		ip4.ki.dwFlags = 0; // 0 for key press
		SendInput(1, &ip4, sizeof(INPUT));
	}

	// press key 1
	ip1.ki.wVk = vk;
	ip1.ki.dwFlags = 0; // 0 for key press
	SendInput(1, &ip1, sizeof(INPUT));

	// release key 1
	ip1.ki.wVk = vk;
	ip1.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &ip1, sizeof(INPUT));

	if (high_byte == 1) {
		// release shift
		ip2.ki.wVk = VK_SHIFT;
		ip2.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &ip2, sizeof(INPUT));
	}

	if (high_byte == 2) {
		// release ctrl
		ip3.ki.wVk = VK_CONTROL;
		ip3.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &ip3, sizeof(INPUT));
	}

	if (high_byte == 4) {
		// release alt
		ip4.ki.wVk = VK_RMENU;
		ip4.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &ip4, sizeof(INPUT));
	}
}

void CmdBuffer::GenerateKey(int vk)
{
	KeyDown(vk);
	KeyUp(vk);
}

void CmdBuffer::GenerateKeyCombo(int vk1, int vk2)
{

	Log::console("Sending key combo " + UTIL_ToString(vk1) + " " + UTIL_ToString(vk2));

	INPUT ip1;
	ip1.type = INPUT_KEYBOARD;
	ip1.ki.wScan = 0;
	ip1.ki.time = 0;
	ip1.ki.dwExtraInfo = 0;
	ip1.ki.wVk = vk1;
	ip1.ki.dwFlags = 0;

	INPUT ip2;
	ip2.type = INPUT_KEYBOARD;
	ip2.ki.wScan = 0;
	ip2.ki.time = 0;
	ip2.ki.dwExtraInfo = 0;
	ip2.ki.wVk = vk2;
	ip2.ki.dwFlags = 0;

	INPUT ip3;
	ip3.type = INPUT_KEYBOARD;
	ip3.ki.wScan = 0;
	ip3.ki.time = 0;
	ip3.ki.dwExtraInfo = 0;
	ip3.ki.wVk = vk2;
	ip3.ki.dwFlags = KEYEVENTF_KEYUP;

	INPUT ip4;
	ip4.type = INPUT_KEYBOARD;
	ip4.ki.wScan = 0;
	ip4.ki.time = 0;
	ip4.ki.dwExtraInfo = 0;
	ip4.ki.wVk = vk1;
	ip4.ki.dwFlags = KEYEVENTF_KEYUP;

	INPUT inarr[4] = { ip1, ip2, ip3, ip4 };

	SendInput(4, inarr, sizeof(INPUT));

	// KeyDown(vk1);
	// KeyDown(vk2);
	// KeyUp(vk2);
	// KeyUp(vk1);
}

void CmdBuffer::writeText(string text)
{
	for (unsigned int i = 0; i < text.length(); i++) {
		SHORT vkey_info = VkKeyScanEx(text[i], GetKeyboardLayout(0));
		GenerateKeyFromInfo(vkey_info);
	}
}

void CmdBuffer::writeText2(string text)
{
	std::vector<INPUT> inputs;
	for (unsigned int i = 0; i < text.length(); i++) {

		INPUT ip1;
		ip1.type = INPUT_KEYBOARD;
		ip1.ki.wScan = text[i];
		ip1.ki.time = 0;
		ip1.ki.dwExtraInfo = 0;
		ip1.ki.wVk = 0;
		ip1.ki.dwFlags = KEYEVENTF_UNICODE;

		INPUT ip2;
		ip2.type = INPUT_KEYBOARD;
		ip2.ki.wScan = text[i];
		ip2.ki.time = 0;
		ip2.ki.dwExtraInfo = 0;
		ip2.ki.wVk = 0;
		ip2.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

		inputs.push_back(ip1);
		inputs.push_back(ip2);
	}

	SendInput(inputs.size(), inputs.data(), sizeof(INPUT));
}

void CmdBuffer::performGameJoin1(DWORD w3pid, int delayInSec)
{

	if (!isW3ActiveWindow(w3pid)) {
		return;
	}

	// Enter CG list
	Sleep(delayInSec * 1000);

	CmdBuffer::GenerateKeyCombo(VK_LMENU, 'G');
}

void CmdBuffer::performGameJoin2(DWORD w3pid, string gn, int delayInSec)
{

	if (!isW3ActiveWindow(w3pid)) {
		return;
	}

	Sleep(delayInSec * 1000);
	// Write game name
	CmdBuffer::writeText(gn);

	// Enter game
	CmdBuffer::GenerateKey(VK_RETURN);
}

void CmdBuffer ::KeyDown(int vk)
{
	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;
	ip.ki.wVk = vk;
	ip.ki.dwFlags = 0; // 0 for key press
	SendInput(1, &ip, sizeof(INPUT));
}

void CmdBuffer::KeyUp(int vk)
{
	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;
	ip.ki.wVk = vk;
	ip.ki.dwFlags = KEYEVENTF_KEYUP;
	SendInput(1, &ip, sizeof(INPUT));
}

bool CmdBuffer::isW3ActiveWindow(DWORD w3pid)
{
	HWND w_handle = GetForegroundWindow();
	if (w_handle == NULL) {
		return false;
	}

	DWORD windowPid;
	GetWindowThreadProcessId(w_handle, &windowPid);

	return windowPid == w3pid;
}