/*
   Copyright 2010 Trevor Hogan
			 2010-2014: stefos007, cen
			 2015-2026: cen

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

/*
	TODO:
	-GHost++ may drop the player even after they reconnect if they run out of time and haven't caught up yet
*/
#define GPROXY_VERSION 14

// collision problem
#define NOMINMAX

//
// INCLUDE
//
#include <fstream>
#include <iostream>

#include "commandpacket.h"
#include "config.h"
#include "gameprotocol.h"
#include "gproxy.h"
#include "gpsprotocol.h"
#include "handshake.h"
#include "socket.h"
#include "util.h"

#include <signal.h>
#include <stdlib.h>
#include <typeinfo>

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#ifdef WIN32
#include <windows.h>
// #include <winsock.h>
#endif

#include "boost/filesystem.hpp"
#include "boost/thread.hpp"

#include "argagg.h"
#include "cmdbuffer.h"
#include "log.h"
#include "timer.h"

#ifdef XPAM_EDITION
#include "gproxy_addons.h"
#else
#include "systemid.h"
#endif

const int pvpgnRemotePort = 6112;
const int pvpgnLocalPort = 6112;

//
// main
//
int main(int argc, char** argv)
{
	// Command line arguments - parse early for --help
	argagg::parser argparser{ {
	  { "help", { "-h", "--help" }, "Show this help message", 0 },
	  { "w3exe", { "--w3exe" }, "Full path of w3 .exe", 1 },
	  { "mode", { "--mode" }, "Restricted or normal", 1 },
	  { "pvpgn_server", { "--pvpgn-server" }, "PvPGN server address", 1 },
	  { "plink", { "--plink" }, "Full path to plink.exe", 1 },
	  { "ft", { "--ft" }, "Use full tunnel for port forwarding.", 1 },
	} };

	argagg::parser_results args;
	if (argc > 1) {
		try {
			args = argparser.parse(argc, argv);
		}
		catch (const std::exception& e) {
			std::cerr << "Error parsing command line arguments: " << e.what() << std::endl;
			return 1;
		}

		// Help - handle immediately before any initialization
		if (args["help"]) {
			std::cout << "GProxy++ v" << GPROXY_VERSION << std::endl << std::endl;
			std::cout << "Available options:" << std::endl;
			std::cout << argparser;
			return 0;
		}
	}

	// init config
	CConfig::Read("gproxy.ini");

	// truncate log file
	ofstream of("gproxy.log", ios::trunc);
	of.close();

	// initialize main logger
	Log logger("gproxy.log", Logging::Subsystem::SYSTEM, CConfig::GetBool("debug", false), CConfig::GetBool("telemetry", false));

	// init system (OS)
#ifdef XPAM_EDITION
	gpa::Systemid systemid;
#else
	Systemid systemid;
#endif

	// hide console
	if (!CConfig::GetBool("console", true)) {
		logger.info("hiding console");
		HWND hWnd = GetConsoleWindow();
		ShowWindow(hWnd, SW_HIDE);
	}

	// initialize winsock
	logger.info("starting winsock");
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		logger.error("error starting winsock");
		logger.info("GPROXY EXITING");
		return 1;
	}

	// increase process priority
	logger.info("setting process priority to \"above normal\"");
	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

	// Default server information
	string TPort;
	string server = CConfig::GetString("pvpgn_server", "server.eurobattle.net");
	uint16_t port = CConfig::GetInt("game_port", 6125);
	string GIndicator = CConfig::GetString("game_indicator", "");

	bool restrictedMode = false;
	string w3dir = systemid.getInstallPath();
	string w3Exe = "Warcraft III.exe";
	W3Version* w3v = nullptr;

	bool pfEnabled = CConfig::GetInt("pf_enable", 0) == 1 ? true : false;
	bool pfFullTunnelEnabled = (CConfig::GetInt("pf_full_tunnel", 0)) == 1 ? true : false;
	string plinkExe = "plink.exe";

	// Process command line arguments
	if (argc > 1) {
		// Mode
		if (args["mode"]) {

			string mode = args["mode"].as<std::string>("");

			if (mode == "restricted") {
				restrictedMode = true;
				logger.info("RESTRICTED MODE ON");
			}
			else {
				restrictedMode = false;
				logger.info("RESTRICTED MODE OFF");
			}
		}
		// Exe
		if (args["w3exe"]) {

			string exeArg = args["w3exe"].as<std::string>("");

			logger.info("W3 exe: " + exeArg);

			string w3VerString = systemid.getW3ExeVersion(exeArg);
			if (w3VerString == "") {
				logger.info("COULD NOT READ PROPER W3 VERSION");
				logger.info("GPROXY EXITING");
				return 0;
			}
			w3v = UTIL_toW3Version(w3VerString);

			logger.info("GProxy will only allow joining bots with W3 enabled version.");
			logger.info("MAJOR: " + UTIL_ToString(w3v->m_major));
			logger.info("MINOR: " + UTIL_ToString(w3v->m_minor));
			logger.info("PATCH: " + UTIL_ToString(w3v->m_patch));
			logger.info("BUILD: " + UTIL_ToString(w3v->m_build));

			w3dir = exeArg.substr(0, exeArg.find_last_of("\\/"));
			w3Exe = UTIL_filenameFromPath(exeArg);
		}
		// Server
		if (args["pvpgn_server"]) {
			server = args["pvpgn_server"].as<std::string>("");
		}
		// Server
		if (args["plink"]) {
			if (args["ft"]) {
				int ft = args["ft"].as<int>(0);
				if (ft == 1 && pfEnabled) {
					pfFullTunnelEnabled = true;
				}
			}
			plinkExe = args["plink"].as<std::string>("plink.exe");
			plinkExe = "\"" + plinkExe + "\"";
			logger.info("Plink: " + plinkExe);
		}
		if (args["ft"]) {
			int ft = args["ft"].as<int>(0);
			if (ft == 0) {
				pfFullTunnelEnabled = false;
			}
		}
	}

	logger.info("Server: " + server);

	// Initialize gproxy
	CGProxy gGProxy(server, port, pfFullTunnelEnabled);

	// Cmd options
	gGProxy.m_w3Dir = w3dir;
	gGProxy.m_restrictedMode = restrictedMode;
	gGProxy.m_w3Version = w3v;

#ifdef XPAM_EDITION
	gGProxy.m_pfPlink = plinkExe;
	gGProxy.edition = "Xpam";
#endif

	logger.info("W3 path: " + gGProxy.m_w3Dir);

	// Empty cfg
	if (!CConfig::Exists("option_sounds")) {
		logger.info("It looks like this is the first time you run gproxy.");
		logger.info("Setting up default server: server.eurobattle.net");
		if (!TPort.empty())
			port = UTIL_ToUInt16(TPort);
		string Indicator;

		if (!Indicator.empty())
			GIndicator = Indicator;
		logger.info("Saving file as gproxy.");
		ofstream out;
		out.open("gproxy.ini");

		if (out.fail())
			logger.error("Error saving configuration file.");
		else {
			out << "######################################################################" << endl;
			out << "pvpgn_server = " << server << endl;
			out << "game_port = " << UTIL_ToString(port) << endl;
			out << "console = 1" << endl;
			out << "option_sounds = 1" << endl;
			for (int i = 1; i <= 12; i++) {
				out << "sound_" + UTIL_ToString(i) + " = 1" << endl;
			}
			out << "chatbuffer = 0" << endl;
			out << "debug = 0" << endl;
			out << "telemetry = 0" << endl;

			out << "autojoin = 0" << endl;
			out << "autojoin_gndelay = 2" << endl;
			out << "autojoin_delay = 2" << endl;

			out << "pf_enable = 0" << endl;
			out << "pf_api_base_url = https://pf.example.com" << endl;
			out << "pf_host = pf.example.com" << endl;
			out << "pf_full_tunnel = 0" << endl;
			out << "pf_login_port = 2248" << endl;
			out << "pf_username =" << endl;
			out << "pf_secret =" << endl;

			out.close();
		}
	}

	// Check if W3 is running
	if (systemid.isRunning(w3Exe)) {
		logger.error("You can't run gproxy while Warcraft is running...");
		logger.info("GPROXY EXITING");
		return 0;
	}

	// load sound settings, size 11
	gGProxy.m_SoundsOptions[0] = true;
	for (int i = 1; i <= 12; i++) {
		if (CConfig::GetInt("sound_" + UTIL_ToString(i), 1) == 0 ? false : true) {
			gGProxy.m_SoundsOptions[i] = true;
		}
	}
	gGProxy.m_Sounds = CConfig::GetInt("option_sounds", 1) == 0 ? false : true;

	// output some info
	logger.info("Listening for gproxy games on port [" + UTIL_ToString(gGProxy.m_Port) + "]");
	logger.info("Listening for warcraft 3 connections on port " + UTIL_ToString(pvpgnLocalPort));
	logger.info("GProxy++ " + gGProxy.edition + " Version " + gGProxy.m_Version);
	logger.info("List of commands: !commands");

	// init system registry
	bool systemOk = systemid.go();
	if (!systemOk) {
		logger.error("Systematic failure");
		logger.info("GPROXY EXITING");
		return 0;
	}

	gGProxy.syd = &systemid;

	// Pf config
#ifdef XPAM_EDITION
	string pfEnabledStr = pfEnabled == true ? "enabled" : "disabled";
	logger.info("pf subsystem is " + pfEnabledStr);

	string pfFullTunnelEnabledStr = pfFullTunnelEnabled == true ? "enabled" : "disabled";
	logger.info("pf full tunnel is " + pfFullTunnelEnabledStr);

	gGProxy.m_pfUsername = CConfig::GetString("pf_username", "");
	gGProxy.m_pfSecret = CConfig::GetString("pf_secret", "");

	// In full tunnel mode, start it up before w3 launch
	if (pfFullTunnelEnabled) {

		if (gGProxy.m_pfUsername == "" || gGProxy.m_pfSecret == "") {
			logger.error("Pf username or secret was empty, can't continue");
			return 0;
		}

		gGProxy.m_pfFullTunnel = true;
		gGProxy.m_pf = new gpa::Pf();

		uint16_t pfPort = gGProxy.m_pf->getPort(CConfig::GetString("pf_api_base_url", "https://pf.example.com"), gGProxy.m_pfUsername, gGProxy.m_pfSecret);
		if (pfPort == 0) {
			logger.error("Could not establish tunnel for port forwarding.");
			return 0;
		}

		systemid.setW3HostPort((int)pfPort);

		gGProxy.m_pfThread = new boost::thread(&gpa::Pf::startFullTunnel, gGProxy.m_pf, pfPort, gGProxy.m_pfUsername, gGProxy.m_pfSecret, plinkExe);
	}
	else {
		logger.info("Setting netgameport to 6113");
		systemid.setW3HostPort(6113);
	}
#else
	logger.info("Setting netgameport to 6113");
	systemid.setW3HostPort(6113);
#endif

	// we are ready
	logger.info("GPROXY READY");

	// wait here for 35 seconds for w3 to start up or exit
	logger.info("Waiting max 35 seconds for W3 to start");
	Timer w3StartupTimer;
	w3StartupTimer.start();
	bool w3started = false;
	while (!w3StartupTimer.passed(35000)) {
		if (systemid.isRunning(w3Exe)) {
			logger.info("Detected W3 as running");
			w3started = true;
			break;
		}
		logger.info("Waiting W3...");
		Sleep(1000);
	}

	if (!w3started) {
		logger.error("W3 did not start in time. Shutting down");
		logger.info("GPROXY EXITING");

#ifdef XPAM_EDITION
		gGProxy.stopPfThread();
#endif

		return 0;
	}

	// start amh thread
#ifdef XPAM_EDITION
	gpa::AMH antihack;
	antihack.argW3Path = gGProxy.m_w3Dir;
	antihack.argW3Exe = w3Exe;
	boost::thread amh_thread(&gpa::AMH::start, &antihack);
#endif

	// Get W3 pid for keyboard checks
	gGProxy.m_w3pid = systemid.getPidByName(w3Exe);
	logger.debug("W3 pid is " + UTIL_ToString(gGProxy.m_w3pid));

	// start command buffer thread
	bool chatBufferEnable = CConfig::GetInt("chatbuffer", 0) == 0 ? false : true;
	boost::thread* cmd_thread = NULL;
	CmdBuffer cmdbuffer;
	if (chatBufferEnable) {
		logger.info("Starting chatbuffer functionality");

		cmd_thread = new boost::thread(&CmdBuffer::start, &cmdbuffer, gGProxy.m_w3pid);
		gGProxy.chatbuffer = &cmdbuffer;
		gGProxy.chatBufferEnabled = true;
	}

	// autojoin
	gGProxy.autoJoinEnabled = CConfig::GetInt("autojoin", 0) == 0 ? false : true;
	if (gGProxy.autoJoinEnabled) {
		logger.info("Autojoin ENABLED");
	}

	gGProxy.autoJoinDelayInSec = CConfig::GetInt("autojoin_delay", 2);
	if (gGProxy.autoJoinDelayInSec < 1 || gGProxy.autoJoinDelayInSec > 20) {
		logger.debug("Resetting autojoin_delay to safe default");
		gGProxy.autoJoinDelayInSec = 2;
	}
	gGProxy.autoJoinGnDelayInSec = CConfig::GetInt("autojoin_gndelay", 2);
	if (gGProxy.autoJoinGnDelayInSec < 1 || gGProxy.autoJoinGnDelayInSec > 5) {
		logger.debug("Resetting autojoin_gndelay to safe default");
		gGProxy.autoJoinGnDelayInSec = 2;
	}

	long ublock = 10000;
	if (pfFullTunnelEnabled) {
		ublock = 1000000;
	}

	while (true) {
		if (gGProxy.Update(ublock, pfFullTunnelEnabled)) {
			break;
		}
		else {
#ifdef XPAM_EDITION
			auto [l_ExitAfterQ, l_Exiting, l_Whisper] = systemid.vectorCheck(amh_thread, antihack, pfFullTunnelEnabled);
			gGProxy.m_ExitAfterQ = l_ExitAfterQ;
			gGProxy.m_Exiting = l_Exiting;

			if (l_Whisper != "")
				gGProxy.pushOnQueue(l_Whisper);
#endif
		}
	}

	// stop and join amh, pf thread
#ifdef XPAM_EDITION
	amh_thread.interrupt();
	amh_thread.join();
	gGProxy.stopPfThread();
#endif

	// stop and join cmdbuffer thread
	if (chatBufferEnable && cmd_thread != NULL) {
		cmd_thread->interrupt();
		cmd_thread->join();
	}

	// shutdown gproxy
	logger.info("Shutting down");

	// shutdown winsock
	logger.info("Shutting down winsock");
	WSACleanup();

	logger.info("GPROXY EXITING");
	Sleep(2000);
	return 0;
}

//
// CGProxy
//
CGProxy::CGProxy(string nServer, uint16_t nPort, bool unproxyMode)
  : logger("gproxy.log", Logging::Subsystem::GPROXY, CConfig::GetBool("debug", false), CConfig::GetBool("telemetry", false))
{
	// config
	stringstream ss;
	ss << GPROXY_VERSION;
	ss >> m_Version;
	m_Server = nServer;
	m_Port = nPort;

	// commands
	m_DND = false;

	// other
	m_Logon = false;
	m_LogonEvent = false;
	m_GIndicator = "";
	m_HostCounter = 0;
	m_Sounds = 0;
	m_Exiting = false;
	m_ExitAfterQ = false;
	m_SpoofCheckSent = false;
	chatBufferEnabled = false;
	autoJoinEnabled = false;
	autoJoinDelayInSec = 2000;

	// Restricted mode
	m_restrictedMode = false;
	m_w3Version = nullptr;
	// m_restrictedMapName = "DotA_Allstars_6.90a8.w3x";
	// m_restrictedMapMd5 = "6e120bc0d8477a0bd5d915b7216f03a4";

	if (unproxyMode)
		return;

	// network and proto
	m_Socket = new CTCPClient();
	m_UDPSocket = new CUDPSocket();
	m_UDPSocket->SetBroadcastTarget("127.0.0.1");
	m_UDPSocket->SetDontRoute(false);
	m_LocalServer = new CTCPServer();
	m_LocalSocket = NULL;
	m_RemoteSocket = new CTCPClient();
	m_RemoteSocket->SetNoDelay(true);
	m_GameProtocol = new CGameProtocol();
	m_GPSProtocol = new CGPSProtocol();
	m_TotalPacketsReceivedFromLocal = 0;
	m_TotalPacketsReceivedFromRemote = 0;
	m_LastConnectionAttemptTime = 0;
	m_LastRefreshTime = 0;
	m_RemoteServerPort = 0;
	m_GameIsReliable = false;
	m_GameStarted = false;
	m_LeaveGameSent = false;
	m_ActionReceived = false;
	m_Synchronized = true;
	m_ReconnectPort = 0;
	m_PID = 255;
	m_ChatPID = 255;
	m_ReconnectKey = 0;
	m_NumEmptyActions = 0;
	m_NumEmptyActionsUsed = 0;
	m_LastAckTime = 0;
	m_LastActionTime = 0;
	m_WC3Server = new CTCPServer();
	m_LocalServer->Listen(string(), m_Port);
	m_WC3Server->Listen(string(), pvpgnLocalPort);

	// Restricted mode
	m_restrictedMode = false;
	m_w3Version = nullptr;
}

CGProxy ::~CGProxy()
{
	delete m_UDPSocket;
	delete m_LocalServer;
	delete m_LocalSocket;
	delete m_RemoteSocket;
	for (vector<CIncomingGameHost*>::iterator i = m_Games.begin(); i != m_Games.end(); i++)
		delete *i;
	for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++)
		delete *i;

	delete m_GameProtocol;
	delete m_GPSProtocol;

#ifdef XPAM_EDITION
	delete m_pf;
	delete m_pfThread;
#endif

	while (!m_LocalPackets.empty()) {
		delete m_LocalPackets.front();
		m_LocalPackets.pop();
	}

	while (!m_RemotePackets.empty()) {
		delete m_RemotePackets.front();
		m_RemotePackets.pop();
	}

	while (!m_PacketBuffer.empty()) {
		delete m_PacketBuffer.front();
		m_PacketBuffer.pop();
	}

	if (m_w3Version != nullptr) {
		delete m_w3Version;
	}
}

bool CGProxy::Update(long usecBlock, bool unproxyMode)
{
	if (unproxyMode) {
		MILLISLEEP(usecBlock / 1000);
		return m_Exiting;
	}

	m_Start_Update.start();

	unsigned int NumFDs = 0;

	// take every socket we own and throw it in one giant select statement so we can block on all sockets
	int nfds = 0;
	fd_set fd;
	fd_set send_fd;
	FD_ZERO(&fd);
	FD_ZERO(&send_fd);

	// 2. the local server
	m_LocalServer->SetFD(&fd, &send_fd, &nfds);
	NumFDs++;

	// 3. the local socket
	if (m_LocalSocket) {
		m_LocalSocket->SetFD(&fd, &send_fd, &nfds);
		NumFDs++;
	}

	// 4. the remote socket
	if (!m_RemoteSocket->HasError() && m_RemoteSocket->GetConnected()) {
		m_RemoteSocket->SetFD(&fd, &send_fd, &nfds);
		NumFDs++;
	}

	// 5. the wc3server
	m_WC3Server->SetFD(&fd, &send_fd, &nfds);
	NumFDs++;

	// 8. the bnftps
	for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++) {
		NumFDs += (*i)->SetFD(&fd, &send_fd, &nfds);
	}

	// try to send a queued message
	// to use safe command sending just push the message on top of m_chatqueue and gproxy will try to send it asap
	if (m_QueueMessageTimer.passed(2000) && m_Logon) {
		if (!m_chatqueue.empty()) {
			for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++) {
				(*i)->SendChatCommand(m_chatqueue.front());
			}
			m_chatqueue.pop_front();
			m_QueueMessageTimer.start();
			logger.debug("Sending queued msg");
		}
		else {
			if (m_ExitAfterQ) {
				logger.info("Chat queue is empty and m_ExitAfterQ is set - triggering exit");
				m_Exiting = true;
			}
		}
	}
	else if (!m_Logon) {
		if (m_ExitAfterQ) {
			logger.info("Not logged on and m_ExitAfterQ is set - triggering exit");
			m_Exiting = true;
		}
	}

	// send to warden after login
	if (m_LogonEvent) {

		// post login event
#ifdef XPAM_EDITION
		vector<string> postLoginMessage = syd->postLoginMessage();

		for (vector<string>::iterator msg = postLoginMessage.begin(); msg != postLoginMessage.end(); msg++) {
			pushOnQueue(*msg);
		}
#endif
		m_LogonEvent = false;
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = usecBlock;

	struct timeval send_tv;
	send_tv.tv_sec = 0;
	send_tv.tv_usec = 0;

	int fdreturn1 = 0;
	int fdreturn2 = 0;
#ifdef WIN32
	fdreturn1 = select(1, &fd, NULL, NULL, &tv);
	fdreturn2 = select(1, NULL, &send_fd, NULL, &send_tv);
#else
	select(nfds + 1, &fd, NULL, NULL, &tv);
	select(nfds + 1, NULL, &send_fd, NULL, &send_tv);
#endif

	if (NumFDs == 0)
		MILLISLEEP(10);

	//
	// accept new connections
	//

	CTCPSocket* NewSocket = m_LocalServer->Accept(&fd);

	if (NewSocket) {
		if (m_LocalSocket) {
			// someone's already connected, reject the new connection
			// we only allow one person to use the proxy at a time
			delete NewSocket;
		}
		else {
			logger.info("Local player connected");
			m_LocalSocket = NewSocket;
			m_LocalSocket->SetNoDelay(true);
			m_TotalPacketsReceivedFromLocal = 0;
			m_TotalPacketsReceivedFromRemote = 0;
			m_GameIsReliable = false;
			m_GameStarted = false;
			m_LeaveGameSent = false;
			m_ActionReceived = false;
			m_Synchronized = true;
			m_ReconnectPort = 0;
			m_PID = 255;
			m_ChatPID = 255;
			m_ReconnectKey = 0;
			m_NumEmptyActions = 0;
			m_NumEmptyActionsUsed = 0;
			m_LastAckTime = 0;
			m_LastActionTime = 0;
			m_JoinedName.clear();
			m_HostName.clear();
			m_SpoofCheckSent = false;

			while (!m_PacketBuffer.empty()) {
				delete m_PacketBuffer.front();
				m_PacketBuffer.pop();
			}
		}
	}

	if (m_LocalSocket) {
		//
		// handle proxying (reconnecting, etc...)
		//

		if (m_LocalSocket->HasError() || !m_LocalSocket->GetConnected()) {
			logger.info("Local player disconnected with error: " + m_LocalSocket->GetErrorString());

			delete m_LocalSocket;
			m_LocalSocket = NULL;

			// ensure a leavegame message was sent, otherwise the server may wait for our reconnection which will never happen
			// if one hasn't been sent it's because Warcraft III exited abnormally

			if (m_GameIsReliable && !m_LeaveGameSent) {
				// note: we're not actually 100% ensuring the leavegame message is sent, we'd need to check that DoSend worked, etc...

				BYTEARRAY LeaveGame;
				LeaveGame.push_back(0xF7);
				LeaveGame.push_back(0x21);
				LeaveGame.push_back(0x08);
				LeaveGame.push_back(0x00);
				UTIL_AppendByteArray(LeaveGame, (uint32_t)PLAYERLEAVE_GPROXY, false);
				m_RemoteSocket->PutBytes(LeaveGame);
				m_RemoteSocket->DoSend(&send_fd);
			}

			m_RemoteSocket->Reset();
			m_RemoteSocket->SetNoDelay(true);
			m_RemoteServerIP.clear();
			m_RemoteServerPort = 0;
		}
		else {
			m_LocalSocket->DoRecv(&fd);
			ExtractLocalPackets();
			ProcessLocalPackets();

			if (!m_RemoteServerIP.empty()) {
				if (m_GameIsReliable && m_ActionReceived && GetTime() - m_LastActionTime >= 60) {
					logger.debug("Reliable game detected, empty action not sent for 60 seconds");
					if (m_NumEmptyActionsUsed < m_NumEmptyActions) {
						SendEmptyAction();
						m_NumEmptyActionsUsed++;
						logger.debug("Empty action sent.. Used: " + UTIL_ToString(m_NumEmptyActionsUsed) + "/" + UTIL_ToString(m_NumEmptyActions));
					}
					else {
						SendLocalChat("GProxy++ ran out of time to reconnect, Warcraft III will disconnect soon.");
						logger.debug("Ran out of time to reconnect.");
					}

					m_LastActionTime = GetTime();
				}

				if (m_RemoteSocket->HasError()) {

					logger.info("Disconnected from remote server due to socket error.");

					if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0) {
						SendLocalChat("You have been disconnected from the server due to a socket error.");
						uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

						if (GetTime() - m_LastActionTime > (uint32_t)((m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60))
							TimeRemaining = 0;

						SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
						logger.info("[GPROXY] attempting to reconnect");
						m_RemoteSocket->Reset();
						m_RemoteSocket->SetNoDelay(true);
						m_RemoteSocket->Connect(string(), m_RemoteServerIP, m_ReconnectPort);
						m_LastConnectionAttemptTime = GetTime();
					}
					else {
						m_LocalSocket->Disconnect();
						delete m_LocalSocket;
						m_LocalSocket = NULL;
						m_RemoteSocket->Reset();
						m_RemoteSocket->SetNoDelay(true);
						m_RemoteServerIP.clear();
						m_RemoteServerPort = 0;
						return false;
					}
				}

				if (!m_RemoteSocket->GetConnecting() && !m_RemoteSocket->GetConnected()) {
					logger.info("[GPROXY] disconnected from remote server");
					if (!m_GameStarted && m_SpoofCheckSent) { // we were kicked from the lobby .. or can't join a full lobby
						RunSound("kicked", m_SoundsOptions[10]);
					}
					if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0) {
						SendLocalChat("You have been disconnected from the server.");
						uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

						if (GetTime() - m_LastActionTime > (uint32_t)((m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60))
							TimeRemaining = 0;

						SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
						logger.info("Attempting to reconnect");
						m_RemoteSocket->Reset();
						m_RemoteSocket->SetNoDelay(true);
						m_RemoteSocket->Connect(string(), m_RemoteServerIP, m_ReconnectPort);
						m_LastConnectionAttemptTime = GetTime();
					}
					else {
						m_LocalSocket->Disconnect();
						delete m_LocalSocket;
						m_LocalSocket = NULL;
						m_RemoteSocket->Reset();
						m_RemoteSocket->SetNoDelay(true);
						m_RemoteServerIP.clear();
						m_RemoteServerPort = 0;
						return false;
					}
				}

				if (m_RemoteSocket->GetConnected()) {
					if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0 && GetTime() - m_RemoteSocket->GetLastRecv() >= 20) {
						logger.info("Disconnected from remote server due to 20 second timeout");
						SendLocalChat("You have been timed out from the server.");
						uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

						if (GetTime() - m_LastActionTime > (uint32_t)((m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60))
							TimeRemaining = 0;

						SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
						logger.info("Attempting to reconnect");
						m_RemoteSocket->Reset();
						m_RemoteSocket->SetNoDelay(true);
						m_RemoteSocket->Connect(string(), m_RemoteServerIP, m_ReconnectPort);
						m_LastConnectionAttemptTime = GetTime();
					}
					else {
						m_RemoteSocket->DoRecv(&fd);
						ExtractRemotePackets();
						ProcessRemotePackets();

						if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0 && GetTime() - m_LastAckTime >= 10) {
							m_RemoteSocket->PutBytes(m_GPSProtocol->SEND_GPSC_ACK(m_TotalPacketsReceivedFromRemote));
							m_LastAckTime = GetTime();
						}

						m_RemoteSocket->DoSend(&send_fd);
					}
				}

				if (m_RemoteSocket->GetConnecting()) {
					// we are currently attempting to connect

					if (m_RemoteSocket->CheckConnect()) {
						// the connection attempt completed

						if (m_GameIsReliable && m_ActionReceived) {
							// this is a reconnection, not a new connection
							// if the server accepts the reconnect request it will send a GPS_RECONNECT back requesting a certain number of packets

							SendLocalChat("GProxy++ reconnected to the server!");
							SendLocalChat("==================================================");
							logger.info("Reconnected to remote server");

							// note: even though we reset the socket when we were disconnected, we haven't been careful to ensure we never queued any data in the meantime
							// therefore it's possible the socket could have data in the send buffer
							// this is bad because the server will expect us to send a GPS_RECONNECT message first
							// so we must clear the send buffer before we continue
							// note: we aren't losing data here, any important messages that need to be sent have been put in the packet buffer
							// they will be requested by the server if required

							m_RemoteSocket->ClearSendBuffer();
							m_RemoteSocket->PutBytes(m_GPSProtocol->SEND_GPSC_RECONNECT(m_PID, m_ReconnectKey, m_TotalPacketsReceivedFromRemote));

							// we cannot permit any forwarding of local packets until the game is synchronized again
							// this will disable forwarding and will be reset when the synchronization is complete

							m_Synchronized = false;
						}
						else
							logger.info("Connected to remote server");
					}
					else if (GetTime() - m_LastConnectionAttemptTime >= 10) {
						// the connection attempt timed out (10 seconds)

						logger.info("Connect to remote server timed out");

						if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0) {
							uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

							if (GetTime() - m_LastActionTime > (uint32_t)((m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60))
								TimeRemaining = 0;

							SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
							logger.info("Attempting to reconnect");
							m_RemoteSocket->Reset();
							m_RemoteSocket->SetNoDelay(true);
							m_RemoteSocket->Connect(string(), m_RemoteServerIP, m_ReconnectPort);
							m_LastConnectionAttemptTime = GetTime();
						}
						else {
							m_LocalSocket->Disconnect();
							delete m_LocalSocket;
							m_LocalSocket = NULL;
							m_RemoteSocket->Reset();
							m_RemoteSocket->SetNoDelay(true);
							m_RemoteServerIP.clear();
							m_RemoteServerPort = 0;
							return false;
						}
					}
				}
			}

			m_LocalSocket->DoSend(&send_fd);
		}
	}
	CTCPSocket* CNewSocket = m_WC3Server->Accept(&fd);
	if (CNewSocket) {
		logger.info("Creating new WC3 socket connection");
		m_Connections.push_back(new CWC3(CNewSocket, m_Server, pvpgnRemotePort, m_GIndicator, this));
	}
	for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end();) {
		if ((*i)->Update(&fd, &send_fd)) {
			delete *i;
			i = m_Connections.erase(i);
			logger.info("Deleting connection");
		}
		else {
			i++;
		}
	}

	// display warning about update being too long
	if (m_Start_Update.passed(50)) {
		logger.debug("Total update took a bit too long " + UTIL_ToString(m_Start_Update.diff()) + "ms");
	}

	return m_Exiting;
}

void CGProxy ::ExtractLocalPackets()
{
	if (!m_LocalSocket)
		return;

	string* RecvBuffer = m_LocalSocket->GetBytes();
	BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char*)RecvBuffer->c_str(), RecvBuffer->size());

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while (Bytes.size() >= 4) {
		// byte 0 is always 247

		if (Bytes[0] == W3GS_HEADER_CONSTANT) {
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16(Bytes, false, 2);

			if (Length >= 4) {
				if (Bytes.size() >= Length) {
					BYTEARRAY Data = BYTEARRAY(Bytes.begin(), Bytes.begin() + Length);

					m_LocalPackets.push(new CCommandPacket(W3GS_HEADER_CONSTANT, Bytes[1], Data));
					m_PacketBuffer.push(new CCommandPacket(W3GS_HEADER_CONSTANT, Bytes[1], Data));
					m_TotalPacketsReceivedFromLocal++;

					*RecvBuffer = RecvBuffer->substr(Length);
					Bytes = BYTEARRAY(Bytes.begin() + Length, Bytes.end());
				}
				else
					return;
			}
			else {
				logger.info("Received invalid packet from local player (bad length)");
				m_Exiting = true;
				return;
			}
		}
		else {
			logger.info("Received invalid packet from local player (bad header constant)");
			m_Exiting = true;
			return;
		}
	}
}

void CGProxy ::ProcessLocalPackets()
{
	if (!m_LocalSocket)
		return;

	while (!m_LocalPackets.empty()) {
		CCommandPacket* Packet = m_LocalPackets.front();
		m_LocalPackets.pop();
		BYTEARRAY Data = Packet->GetData();

		if (Packet->GetPacketType() == W3GS_HEADER_CONSTANT) {
			if (Packet->GetID() == CGameProtocol ::W3GS_REQJOIN) {

				if (Data.size() >= 20) {
					// parse
					uint32_t HostCounter = UTIL_ByteArrayToUInt32(Data, false, 4);
					uint32_t EntryKey = UTIL_ByteArrayToUInt32(Data, false, 8);
					unsigned char Unknown = Data[12];
					uint16_t ListenPort = UTIL_ByteArrayToUInt16(Data, false, 13);
					uint32_t PeerKey = UTIL_ByteArrayToUInt32(Data, false, 15);
					BYTEARRAY Name = UTIL_ExtractCString(Data, 19);
					string NameString = string(Name.begin(), Name.end());
					BYTEARRAY Remainder = BYTEARRAY(Data.begin() + Name.size() + 20, Data.end());

					if (Remainder.size() == 18) {
						// lookup the game in the main list

						bool GameFound = false;

						for (vector<CIncomingGameHost*>::iterator i = m_Games.begin(); i != m_Games.end(); i++) {
							logger.debug("Games loop");
							if ((*i)->GetHostCounter() == HostCounter) {
								m_HostCounter = HostCounter;
								logger.info("Local player requested game name [" + (*i)->GetGameName() + "]");

								// Check for sha1 validity of DotA map in restricted mode
								/*if (m_restrictedMode) {
									string mapname = UTIL_filenameFromPath((*i)->GetMapPath());
									logger.debug("Map name: " + mapname);
									if (mapname == m_restrictedMapName) {
										string mapPath = m_w3Dir + "\\" + (*i)->GetMapPath();
										logger.debug("Map path: " + mapPath);
										Crypto c;
										string mapmd5 = c.fileMD5(mapPath);
										if (mapmd5 == "") {
											logger.error("Fatal error verifying the map");
;											logger.error(UTIL_getLastErrorAsString());
										}
										if (mapmd5 != m_restrictedMapMd5) {
											//Invalid DotA map checksum, possible security exploit in 1.26!, disconnect
											logger.debug("Invalid DotA map checksum, disconnecting for security reasons: "+ mapmd5);
										}
									}
									else {
										//Joined invalid map, disconnect
										logger.debug("Joined invalid map, only specific DotA version allowed in restricted mode");
									}
								}*/

								logger.debug(" connecting to remote server [" + (*i)->GetIPString() + "] on port " + UTIL_ToString((*i)->GetPort()));
								m_RemoteServerIP = (*i)->GetIPString();
								m_RemoteServerPort = (*i)->GetPort();
								m_RemoteSocket->Reset();
								m_RemoteSocket->SetNoDelay(true);
								m_RemoteSocket->Connect(string(), m_RemoteServerIP, m_RemoteServerPort);
								m_LastConnectionAttemptTime = GetTime();
								logger.info("Map dimensions: width=" + UTIL_ToString((*i)->GetMapWidth()) + ", height=" + UTIL_ToString((*i)->GetMapHeight()));
								m_GameIsReliable = ((*i)->GetMapWidth() == 1984 && (*i)->GetMapHeight() == 1984);
								logger.info("Game reliability check: m_GameIsReliable=" + UTIL_ToString(m_GameIsReliable) + " (magic dimensions 1984x1984)");
								m_GameStarted = false;
								m_SpoofCheckSent = false;

								// rewrite packet
								// logger.info("NAME - "+NameString);
								// logger.info("GAME NAME: "+(*i)->GetGameName( ));
								/*
								BYTEARRAY DataRewritten;
								DataRewritten.push_back( W3GS_HEADER_CONSTANT );
								DataRewritten.push_back( Packet->GetID( ) );
								DataRewritten.push_back( 0 );
								DataRewritten.push_back( 0 );
								UTIL_AppendByteArray( DataRewritten, (*i)->GetHostCounter( ), false );
								UTIL_AppendByteArray( DataRewritten, (uint32_t)0, false );
								DataRewritten.push_back( Unknown );
								UTIL_AppendByteArray( DataRewritten, ListenPort, false );
								UTIL_AppendByteArray( DataRewritten, PeerKey, false );
								UTIL_AppendByteArray( DataRewritten, Name );
								UTIL_AppendByteArrayFast( DataRewritten, Remainder );
								BYTEARRAY LengthBytes;
								LengthBytes = UTIL_CreateByteArray( (uint16_t)DataRewritten.size( ), false );
								DataRewritten[2] = LengthBytes[0];
								DataRewritten[3] = LengthBytes[1];
								Data = DataRewritten;*/

								// save the hostname for later (for manual spoof checking)

								m_JoinedName = NameString;
								m_HostName = (*i)->GetHostName();

								GameFound = true;
								break;
							}
						}

						if (!GameFound) {
							logger.info("Local player requested unknown game (expired?)");
							m_LocalSocket->Disconnect();
						}
					}
					else
						logger.info("Received invalid join request from local player (invalid remainder)");
				}
				else
					logger.info("Received invalid join request from local player (too short)");
			}
			else if (Packet->GetID() == CGameProtocol ::W3GS_LEAVEGAME) {
				m_LeaveGameSent = true;
				m_LocalSocket->Disconnect();
			}
			else if (Packet->GetID() == CGameProtocol ::W3GS_CHAT_TO_HOST) { // id 40

				// handled in ExtractLocalPackets (yes, it's ugly)
				unsigned int i = 5;
				unsigned char Total = Data[4];
				if (Total > 0 && Data.size() >= i + Total) {
					BYTEARRAY ToPIDs = BYTEARRAY(Data.begin() + i, Data.begin() + i + Total);
					i += Total;
					unsigned char FromPID = Data[i];
					unsigned char Flag = Data[i + 1];
					i += 2;
					//

					if (Flag == 16 && Data.size() >= i + 1) {
						// chat message
						BYTEARRAY msg = UTIL_ExtractCString(Data, i);
						string tmp = string(msg.begin(), msg.end());
						string Command;
						string Payload;
						stringstream SS;
						SS << tmp;
						SS >> Command;

						if (!SS.eof()) {
							getline(SS, Payload);
							string ::size_type Start = Payload.find_first_not_of(" ");

							if (Start != string ::npos)
								Payload = Payload.substr(Start);
						}

						/***************/
						// lobby commands
						/***************/
						if (Command[0] == '!')
							if (Command == "!sc") {
								for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++)
									(*i)->SendChatCommand("/w " + m_HostName + " sc");
							}
							else if (Command == "!v") {
								for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++)
									(*i)->SendLocalChat("GProxy++ Version " + m_Version);
							}
							else if (Command == "!sounds") {
								m_Sounds = !m_Sounds;
								for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++) {
									if (m_Sounds)
										(*i)->SendLocalChat("Sounds enabled.");
									else
										(*i)->SendLocalChat("Sounds disabled.");
								}
							}

						// insert into global chat buffer
						if (this->chatBufferEnabled) {
							chatbuffer->mtxChatBuffer.lock();
							chatbuffer->addChatBuffer(tmp);
							chatbuffer->mtxChatBuffer.unlock();
						}

						/* END LOBBY */
					}
					else if (Flag == 32 && Data.size() >= i + 5) {
						// chat message with extra flags

						BYTEARRAY ExtraFlags = BYTEARRAY(Data.begin() + i, Data.begin() + i + 4);
						BYTEARRAY msg = UTIL_ExtractCString(Data, i + 4);
						string tmp = string(msg.begin(), msg.end());
						string Command;
						string Payload;
						stringstream SS;
						SS << tmp;
						SS >> Command;

						if (!SS.eof()) {
							getline(SS, Payload);
							string ::size_type Start = Payload.find_first_not_of(" ");

							if (Start != string ::npos)
								Payload = Payload.substr(Start);
						}
						/***************/
						// game commands
						/***************/
						// logger.info("2 "+tmp, false);
						if (Command[0] == '!')
							if (Command == "!v") {
								for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++)
									(*i)->SendLocalChat("GProxy++ Version " + m_Version);
							}
							else if (Command == "!sounds") {
								m_Sounds = !m_Sounds;
								for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++) {
									if (m_Sounds)
										(*i)->SendLocalChat("Sounds enabled.");
									else
										(*i)->SendLocalChat("Sounds disabled.");
								}
							}
						/* END GAME COMMANDS */
					}
				}
			}
		}

		// logger.info( "Packate id: "+UTIL_ToString( Packet->GetID( ) ), false);

		// warning: do not forward any data if we are not synchronized (e.g. we are reconnecting and resynchronizing)
		// any data not forwarded here will be cached in the packet buffer and sent later so all is well

		if (m_RemoteSocket && m_Synchronized)
			m_RemoteSocket->PutBytes(Data);

		delete Packet;
	}
}

void CGProxy ::ExtractRemotePackets()
{
	string* RecvBuffer = m_RemoteSocket->GetBytes();
	BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char*)RecvBuffer->c_str(), RecvBuffer->size());

	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

	while (Bytes.size() >= 4) {
		if (Bytes[0] == W3GS_HEADER_CONSTANT || Bytes[0] == GPS_HEADER_CONSTANT) {
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16(Bytes, false, 2);

			if (Length >= 4) {
				if (Bytes.size() >= Length) {
					m_RemotePackets.push(new CCommandPacket(Bytes[0], Bytes[1], BYTEARRAY(Bytes.begin(), Bytes.begin() + Length)));

					if (Bytes[0] == W3GS_HEADER_CONSTANT)
						m_TotalPacketsReceivedFromRemote++;

					*RecvBuffer = RecvBuffer->substr(Length);
					Bytes = BYTEARRAY(Bytes.begin() + Length, Bytes.end());
				}
				else
					return;
			}
			else {
				logger.info("Received invalid packet from remote server (bad length)");
				m_Exiting = true;
				return;
			}
		}
		else {
			logger.info("Received invalid packet from remote server (bad header constant) " + UTIL_ToString(Bytes[0]));
			m_Exiting = true;
			return;
		}
	}
}

void CGProxy ::ProcessRemotePackets()
{
	if (!m_LocalSocket || !m_RemoteSocket)
		return;

	while (!m_RemotePackets.empty()) {
		CCommandPacket* Packet = m_RemotePackets.front();
		m_RemotePackets.pop();

		if (Packet->GetPacketType() == W3GS_HEADER_CONSTANT) {
			if (Packet->GetID() == CGameProtocol ::W3GS_SLOTINFOJOIN) {
				BYTEARRAY Data = Packet->GetData();

				if (Data.size() >= 6) {
					uint16_t SlotInfoSize = UTIL_ByteArrayToUInt16(Data, false, 4);

					if ((uint16_t)Data.size() >= 7 + SlotInfoSize)
						m_ChatPID = Data[6 + SlotInfoSize];
				}

				// send a GPS_INIT packet
				// if the server doesn't recognize it (e.g. it isn't GHost++) we should be kicked

				logger.info("Join request accepted by remote server");

				if (this->chatBufferEnabled) {
					chatbuffer->mtxCmdBufferEnable.lock();
					chatbuffer->setCmdBufferEnable(true);
					chatbuffer->mtxCmdBufferEnable.unlock();
				}

				if (m_GameIsReliable) {
					logger.info("Detected reliable game, starting GPS handshake");
					// send spoofcheck
					for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++) {
						(*i)->SendChatCommand("/w " + m_HostName + " sc");
						m_SpoofCheckSent = true;
					}

					m_gpsInitReceived = false;
					uint32_t version = getVersion();
					logger.info("Sending GPS_INIT packet to server, version=" + to_string(version));
					m_RemoteSocket->PutBytes(m_GPSProtocol->SEND_GPSC_INIT(version));
					logger.info("GPS_INIT packet sent");
				}
				else
					logger.info("Detected standard game, disconnect protection disabled");
			}
			else if (Packet->GetID() == CGameProtocol ::W3GS_COUNTDOWN_END) {
				for (vector<CWC3*>::iterator i = m_Connections.begin(); i != m_Connections.end(); i++)
					(*i)->currentGameState = CWC3::IN_GAME;
				logger.debug("Game state changed to IN_GAME");

				RunSound("started", m_SoundsOptions[1]);
				if (m_GameIsReliable && m_ReconnectPort > 0) {
					logger.info("Game started, disconnect protection enabled");
				}
				else {
					if (m_GameIsReliable) {
						logger.info("Game started but GPS handshake not complete, disconnect protection disabled");
					}
					else {
						logger.info("Game started");
					}
				}
				if (this->chatBufferEnabled) {
					chatbuffer->mtxCmdBufferEnable.lock();
					chatbuffer->setCmdBufferEnable(false);
					chatbuffer->mtxCmdBufferEnable.unlock();
				}
				m_GameStarted = true;
			}
			else if (Packet->GetID() == CGameProtocol ::W3GS_INCOMING_ACTION) {
				if (m_GameIsReliable) {
					// we received a game update which means we can reset the number of empty actions we have to work with
					// we also must send any remaining empty actions now
					// note: the lag screen can't be up right now otherwise the server made a big mistake, so we don't need to check for it

					BYTEARRAY EmptyAction;
					EmptyAction.push_back(0xF7);
					EmptyAction.push_back(0x0C);
					EmptyAction.push_back(0x06);
					EmptyAction.push_back(0x00);
					EmptyAction.push_back(0x00);
					EmptyAction.push_back(0x00);

					for (unsigned char i = m_NumEmptyActionsUsed; i < m_NumEmptyActions; i++)
						m_LocalSocket->PutBytes(EmptyAction);

					m_NumEmptyActionsUsed = 0;
				}

				m_ActionReceived = true;
				m_LastActionTime = GetTime();
			}
			else if (Packet->GetID() == CGameProtocol ::W3GS_START_LAG) {
				if (m_GameIsReliable) {
					BYTEARRAY Data = Packet->GetData();

					if (Data.size() >= 5) {
						unsigned char NumLaggers = Data[4];

						if (Data.size() == 5 + NumLaggers * 5) {
							for (unsigned char i = 0; i < NumLaggers; i++) {
								bool LaggerFound = false;

								for (vector<unsigned char>::iterator j = m_Laggers.begin(); j != m_Laggers.end(); j++) {
									if (*j == Data[5 + i * 5])
										LaggerFound = true;
								}

								if (LaggerFound)
									logger.info("Warning - received start_lag on known lagger");
								else
									m_Laggers.push_back(Data[5 + i * 5]);
							}
						}
						else
							logger.info("Warning - unhandled start_lag (2)");
					}
					else
						logger.info("Warning - unhandled start_lag (1)");
				}
			}
			else if (Packet->GetID() == CGameProtocol ::W3GS_STOP_LAG) {
				if (m_GameIsReliable) {
					BYTEARRAY Data = Packet->GetData();

					if (Data.size() == 9) {
						bool LaggerFound = false;

						for (vector<unsigned char>::iterator i = m_Laggers.begin(); i != m_Laggers.end();) {
							if (*i == Data[4]) {
								i = m_Laggers.erase(i);
								LaggerFound = true;
							}
							else
								i++;
						}

						if (!LaggerFound)
							logger.info("Warning - received stop_lag on unknown lagger");
					}
					else
						logger.info("Warning - unhandled stop_lag");
				}
			}
			else if (Packet->GetID() == CGameProtocol ::W3GS_INCOMING_ACTION2) {
				if (m_GameIsReliable) {
					// we received a fractured game update which means we cannot use any empty actions until we receive the subsequent game update
					// we also must send any remaining empty actions now
					// note: this means if we get disconnected right now we can't use any of our buffer time, which would be very unlucky
					// it still gives us 60 seconds total to reconnect though
					// note: the lag screen can't be up right now otherwise the server made a big mistake, so we don't need to check for it

					BYTEARRAY EmptyAction;
					EmptyAction.push_back(0xF7);
					EmptyAction.push_back(0x0C);
					EmptyAction.push_back(0x06);
					EmptyAction.push_back(0x00);
					EmptyAction.push_back(0x00);
					EmptyAction.push_back(0x00);

					for (unsigned char i = m_NumEmptyActionsUsed; i < m_NumEmptyActions; i++)
						m_LocalSocket->PutBytes(EmptyAction);

					m_NumEmptyActionsUsed = m_NumEmptyActions;
				}
			}

			// forward the data

			m_LocalSocket->PutBytes(Packet->GetData());

			// we have to wait until now to send the status message since otherwise the slotinfojoin itself wouldn't have been forwarded

			if (Packet->GetID() == CGameProtocol ::W3GS_SLOTINFOJOIN) {
				if (m_GameIsReliable)
					SendLocalChat("This is a reliable game. Requesting GProxy++ disconnect protection from server...");
				else
					SendLocalChat("This is an unreliable game. GProxy++ disconnect protection is disabled.");
			}
		}
		else if (Packet->GetPacketType() == GPS_HEADER_CONSTANT) {
			if (m_GameIsReliable) {
				BYTEARRAY Data = Packet->GetData();

				logger.debug("Packet id " + UTIL_ToString(Packet->GetID()) + ", data len " + UTIL_ToString(Data.size()));

				if (Packet->GetID() == CGPSProtocol ::GPS_INIT && Data.size() == 12) {
					m_gpsInitReceived = true;
					m_ReconnectPort = UTIL_ByteArrayToUInt16(Data, false, 4);
					m_PID = Data[6];
					m_ReconnectKey = UTIL_ByteArrayToUInt32(Data, false, 7);
					m_NumEmptyActions = Data[11];
					SendLocalChat("GProxy++ disconnect protection is ready (" + UTIL_ToString((m_NumEmptyActions + 1) * 60) + " second buffer).");
					logger.info("Handshake complete, disconnect protection ready (" + UTIL_ToString((m_NumEmptyActions + 1) * 60) + " second buffer)");
				}
				else if (Packet->GetID() == CGPSProtocol::GPS_W3VERSION && Data.size() == 12) {
					// Must be received before GPS_INIT, if not, we are connecting to unprotected host
					if (m_gpsInitReceived && m_restrictedMode) {
						logger.info("Tried to connect to unprotected host, disconnecting");
						m_LocalSocket->Disconnect();
					}

					// Send w3 version to hostbot
					// Bot will then kick us out if version is incorrect
					if (m_restrictedMode) {
						logger.debug("Sending w3 version to host: " + m_w3Version->toString());
						m_RemoteSocket->PutBytes(m_GPSProtocol->SEND_GPSC_W3VERSION(m_w3Version));
					}
				}
				else if (Packet->GetID() == CGPSProtocol ::GPS_RECONNECT && Data.size() == 8) {
					uint32_t LastPacket = UTIL_ByteArrayToUInt32(Data, false, 4);
					uint32_t PacketsAlreadyUnqueued = m_TotalPacketsReceivedFromLocal - m_PacketBuffer.size();

					if (LastPacket > PacketsAlreadyUnqueued) {
						uint32_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

						if (PacketsToUnqueue > m_PacketBuffer.size()) {
							logger.info("Received GPS_RECONNECT with last packet > total packets sent");
							PacketsToUnqueue = m_PacketBuffer.size();
						}

						while (PacketsToUnqueue > 0) {
							delete m_PacketBuffer.front();
							m_PacketBuffer.pop();
							PacketsToUnqueue--;
						}
					}

					// send remaining packets from buffer, preserve buffer
					// note: any packets in m_LocalPackets are still sitting at the end of this buffer because they haven't been processed yet
					// therefore we must check for duplicates otherwise we might (will) cause a desync

					queue<CCommandPacket*> TempBuffer;

					while (!m_PacketBuffer.empty()) {
						if (m_PacketBuffer.size() > m_LocalPackets.size())
							m_RemoteSocket->PutBytes(m_PacketBuffer.front()->GetData());

						TempBuffer.push(m_PacketBuffer.front());
						m_PacketBuffer.pop();
					}

					m_PacketBuffer = TempBuffer;

					// we can resume forwarding local packets again
					// doing so prior to this point could result in an out-of-order stream which would probably cause a desync

					m_Synchronized = true;
				}
				else if (Packet->GetID() == CGPSProtocol ::GPS_ACK && Data.size() == 8) {
					uint32_t LastPacket = UTIL_ByteArrayToUInt32(Data, false, 4);
					uint32_t PacketsAlreadyUnqueued = m_TotalPacketsReceivedFromLocal - m_PacketBuffer.size();

					if (LastPacket > PacketsAlreadyUnqueued) {
						uint32_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

						if (PacketsToUnqueue > m_PacketBuffer.size()) {
							logger.info("Received GPS_ACK with last packet > total packets sent");
							PacketsToUnqueue = m_PacketBuffer.size();
						}

						while (PacketsToUnqueue > 0) {
							delete m_PacketBuffer.front();
							m_PacketBuffer.pop();
							PacketsToUnqueue--;
						}
					}
				}
				else if (Packet->GetID() == CGPSProtocol ::GPS_REJECT && Data.size() == 8) {
					uint32_t Reason = UTIL_ByteArrayToUInt32(Data, false, 4);

					if (Reason == REJECTGPS_INVALID)
						logger.info("Rejected by remote server: invalid data");
					else if (Reason == REJECTGPS_NOTFOUND)
						logger.info("Rejected by remote server: player not found in any running games");

					m_LocalSocket->Disconnect();
				}
			}
		}

		delete Packet;
	}
}

void CGProxy ::SendLocalChat(string message)
{
	if (m_LocalSocket) {
		if (m_GameStarted) {
			if (message.size() > 127)
				message = message.substr(0, 127);

			m_LocalSocket->PutBytes(m_GameProtocol->SEND_W3GS_CHAT_FROM_HOST(m_ChatPID, UTIL_CreateByteArray(m_ChatPID), 32, UTIL_CreateByteArray((uint32_t)0, false), message));
		}
		else {
			if (message.size() > 254)
				message = message.substr(0, 254);

			m_LocalSocket->PutBytes(m_GameProtocol->SEND_W3GS_CHAT_FROM_HOST(m_ChatPID, UTIL_CreateByteArray(m_ChatPID), 16, BYTEARRAY(), message));
		}
	}
}

void CGProxy ::SendEmptyAction()
{
	// we can't send any empty actions while the lag screen is up
	// so we keep track of who the lag screen is currently showing (if anyone) and we tear it down, send the empty action, and put it back up

	for (vector<unsigned char>::iterator i = m_Laggers.begin(); i != m_Laggers.end(); i++) {
		BYTEARRAY StopLag;
		StopLag.push_back(0xF7);
		StopLag.push_back(0x11);
		StopLag.push_back(0x09);
		StopLag.push_back(0);
		StopLag.push_back(*i);
		UTIL_AppendByteArray(StopLag, (uint32_t)60000, false);
		m_LocalSocket->PutBytes(StopLag);
	}

	BYTEARRAY EmptyAction;
	EmptyAction.push_back(0xF7);
	EmptyAction.push_back(0x0C);
	EmptyAction.push_back(0x06);
	EmptyAction.push_back(0x00);
	EmptyAction.push_back(0x00);
	EmptyAction.push_back(0x00);
	m_LocalSocket->PutBytes(EmptyAction);

	if (!m_Laggers.empty()) {
		BYTEARRAY StartLag;
		StartLag.push_back(0xF7);
		StartLag.push_back(0x10);
		StartLag.push_back(0);
		StartLag.push_back(0);
		StartLag.push_back((unsigned char)m_Laggers.size());

		for (vector<unsigned char>::iterator i = m_Laggers.begin(); i != m_Laggers.end(); i++) {
			// using a lag time of 60000 ms means the counter will start at zero
			// hopefully warcraft 3 doesn't care about wild variations in the lag time in subsequent packets

			StartLag.push_back(*i);
			UTIL_AppendByteArray(StartLag, (uint32_t)60000, false);
		}

		BYTEARRAY LengthBytes;
		LengthBytes = UTIL_CreateByteArray((uint16_t)StartLag.size(), false);
		StartLag[2] = LengthBytes[0];
		StartLag[3] = LengthBytes[1];
		m_LocalSocket->PutBytes(StartLag);
	}
}

void CGProxy::UDPSend(string id, string message)
{
	// talk 1 whisper 4 info 3 error 2 flist 5 channel 6
	logger.debug("UDPSent called");
	message = id + " " + message;
	string ip = "127.0.0.1";
	uint16_t port = 6220;
	m_UDPSocket->SendTo(ip, port, message);
}

void CGProxy::pushOnQueue(string command)
{
	// only push if we are not already exiting. Usually after reporting hacks
	if (!m_ExitAfterQ) {
		m_chatqueue.push_back(command);
	}
}

#ifdef XPAM_EDITION
void CGProxy::stopPfThread()
{
	if (this->m_pfThread != NULL && this->m_pf != NULL && !this->m_pf->m_terminated) {
		long maxSleepCnt = 10;
		this->m_pf->exit();
		while (true) {
			if (this->m_pf->m_terminated) {
				logger.info("Pf was terminated, continuing orderly shutdown");
				break;
			}
			else {
				maxSleepCnt--;
				if (maxSleepCnt <= 0) {
					logger.info("Pf did not shut down in time, continue forcibly");
					break;
				}
				Sleep(100);
			}
		}
		this->m_pfThread->interrupt();
		this->m_pfThread->join();
	}
}
#endif