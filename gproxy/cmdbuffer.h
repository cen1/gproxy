#pragma once

#include "boost/thread.hpp"
#include <deque>
#include <iostream>
#include <string>
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include "Windows.h"
#include "Winuser.h"
#include "log.h"

using namespace std;

class CmdBuffer {

  private:
	const unsigned int LIMIT = 20;

	Log logger;

	/**
	 * Temporarily stop or start the cmdbuffer functionality.
	 */
	bool cmdBufferEnable;

	/**
	 * Chat buffer of entered commands
	 */
	deque<string> chatBuffer;

	static void KeyDown(int vk);
	static void KeyUp(int vk);

  public:
	/**
	 * For locking cmdBufferEnable. Enabling and disabling cmdbuffer functionality.
	 */
	boost::mutex mtxCmdBufferEnable;

	/**
	 * /Locks typing and deleting key presses. ALso isnerting into buffer.
	 */
	boost::mutex mtxChatBuffer;

	/**
	 * Main thread loop.
	 */
	void start(DWORD w3pid);

	/**
	 * Adds new entry to chat buffer.
	 */
	void addChatBuffer(string text);

	/**
	 * Key combination press.
	 */
	static void GenerateKeyFromInfo(SHORT vkey_info);

	/**
	 * Simple key press.
	 */
	static void GenerateKey(int vk);

	/**
	 * Key combination press by specifying both keys.
	 */
	static void GenerateKeyCombo(int vk1, int vk2);

	static void writeText(string text);
	static void writeText2(string text);

	static void performGameJoin1(DWORD w3pid, int delayInSec);
	static void performGameJoin2(DWORD w3pid, string gn, int delayInSec);
	static bool isW3ActiveWindow(DWORD w3pid);

	/**
	 * Enables or disabled chatbuffer functionality without exiting the main loop.
	 */
	void setCmdBufferEnable(bool b)
	{
		cmdBufferEnable = b;
	}

	CmdBuffer();
	~CmdBuffer();
};