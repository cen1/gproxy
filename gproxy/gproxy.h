/*

   Copyright 2010 Trevor Hogan

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/
#pragma once

#ifndef GPROXY_H
#define GPROXY_H

// standard integer sizes for 64 bit compatibility

#ifdef WIN32
#include <cstdint>
#else
#include <stdint.h>
#endif

#include "cmdbuffer.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "incominggamehost.h"
#include "log.h"
#include "socket.h"
#include "timer.h"
#include "w3version.h"
#include "wc3.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef XPAM_EDITION
#include "gproxy_addons.h"
#else
#include "systemid.h"
#endif

using namespace std;

typedef vector<unsigned char> BYTEARRAY;

#ifdef WIN32
#define MILLISLEEP(x) Sleep(x)
#else
#define MILLISLEEP(x) usleep((x) * 1000)
#endif

// network

#undef FD_SETSIZE
#define FD_SETSIZE 512

//
// CGProxy
//

class CWC3;

class CGProxy {
  public:
	Log logger;

	// config
	string m_Version;
	string m_Server;
	uint16_t m_Port;

	// commands
	bool m_DND;

	/**
	 * ==========
	 * TIMERS
	 * ==========
	 */
	/**
	 * Used to limit the sending rate of chat to bnet to avoid flooding.
	 */
	Timer m_QueueMessageTimer;

	/**
	 * Used to wait for Warcraft process to start.
	 */
	Timer m_W3Timer;

	/**
	 * Used to send SID_NULL every minute.
	 */
	Timer m_Null_Timer;

	/**
	 * Measures how long the Update loop takes to execute as a whole.
	 */
	Timer m_Start_Update;

#ifdef XPAM_EDITION
	gpa::Systemid* syd;
#else
	Systemid* syd;
#endif

	// other
	string edition = "Standard";
	bool m_Logon;
	bool m_LogonEvent;
	deque<string> m_chatqueue;
	bool m_Exiting;
	bool m_ExitAfterQ;
	string m_HostName;
	string m_JoinedName;
	string m_GIndicator;
	bool m_Sounds;
	bool m_SoundsOptions[11];
	bool m_SpoofCheckSent;
	CmdBuffer* chatbuffer;
	bool chatBufferEnabled;
	bool autoJoinEnabled;
	int autoJoinDelayInSec;	  // seconds
	int autoJoinGnDelayInSec; // seconds

	// GPS
	bool m_gpsInitReceived = false;

	// network and proto
	CTCPServer* m_LocalServer;
	CTCPSocket* m_LocalSocket;
	CTCPClient* m_RemoteSocket;
	CUDPSocket* m_UDPSocket;
	vector<CIncomingGameHost*> m_Games;
	CTCPClient* m_Socket;
	CGameProtocol* m_GameProtocol;
	CGPSProtocol* m_GPSProtocol;
	queue<CCommandPacket*> m_LocalPackets;
	queue<CCommandPacket*> m_RemotePackets;
	queue<CCommandPacket*> m_PacketBuffer;
	vector<unsigned char> m_Laggers;
	uint32_t m_TotalPacketsReceivedFromLocal;
	uint32_t m_TotalPacketsReceivedFromRemote;
	uint32_t m_LastConnectionAttemptTime;
	uint32_t m_LastRefreshTime;
	string m_RemoteServerIP;
	uint16_t m_RemoteServerPort;
	bool m_GameIsReliable;
	bool m_GameStarted;
	bool m_LeaveGameSent;
	bool m_ActionReceived;
	bool m_Synchronized;
	uint16_t m_ReconnectPort;
	unsigned char m_PID;
	unsigned char m_ChatPID;
	uint32_t m_ReconnectKey;
	unsigned char m_NumEmptyActions;
	unsigned char m_NumEmptyActionsUsed;
	uint32_t m_LastAckTime;
	uint32_t m_LastActionTime;
	CTCPServer* m_WC3Server;
	vector<CWC3*> m_Connections;

	uint32_t m_FMcounter;
	string m_RegKey;
	uint32_t m_HostCounter;

	bool m_restrictedMode;
	string m_w3Dir;
	W3Version* m_w3Version;
	DWORD m_w3pid;
	// string m_restrictedMapName;
	// string m_restrictedMapMd5;

	// Port forwarding addon
#ifdef XPAM_EDITION
	boost::thread* m_pfThread;
	gpa::Pf* m_pf;
	bool m_pfFullTunnel = false;
	string m_pfUsername;
	string m_pfSecret;
	string m_pfPlink;

	void stopPfThread();
#endif

	// processing functions
	bool Update(long usecBlock, bool unproxyMode);
	void ExtractLocalPackets();
	void ProcessLocalPackets();
	void ExtractRemotePackets();
	void ProcessRemotePackets();
	void SendEmptyAction();
	void SendLocalChat(string message);
	void UDPSend(string id, string message);
	void pushOnQueue(string command);

	CGProxy(string nServer, uint16_t nPort, bool unproxyMode);
	~CGProxy();
};

#endif