#include "wc3.h"
#include "handshake.h"
#include "util.h"
#include <thread>

CWC3::CWC3(CTCPSocket* socket, string hostname, uint16_t port, string indicator, CGProxy* gGProxyPtr)
  : logger("gproxy.log", Logging::Subsystem::WC3, CConfig::GetBool("debug", false), CConfig::GetBool("telemetry", false))
{
	m_LocalSocket = socket;
	m_LocalSocket->SetNoDelay(true);
	m_RemoteSocket = new CTCPClient();
	m_RemoteSocket->SetNoDelay(true);
	m_RemoteSocket->Connect(string(), hostname, port);
	logger.info("Initiating the two way connection");
	m_GIndicator = indicator;
	m_FirstPacket = true;
	m_IsBNFTP = false;
	gGProxy = gGProxyPtr;
	gameToAutoJoin = "";
	playerToFollow = "";
	currentGameState = IN_OTHER;
}
CWC3 ::~CWC3()
{
	delete m_LocalSocket;
	delete m_RemoteSocket;
}
unsigned int CWC3::SetFD(void* fd, void* send_fd, int* nfds)
{
	unsigned int NumFDs = 0;
	if (!m_LocalSocket->HasError() && m_LocalSocket->GetConnected()) {
		m_LocalSocket->SetFD((fd_set*)fd, (fd_set*)send_fd, nfds);
		NumFDs++;
	}
	if (!m_RemoteSocket->HasError() && m_RemoteSocket->GetConnected()) {
		m_RemoteSocket->SetFD((fd_set*)fd, (fd_set*)send_fd, nfds);
		NumFDs++;
	}
	return NumFDs;
}

bool CWC3::Update(void* fd, void* send_fd)
{
	// local socket
	if (m_LocalSocket->HasError()) {
		logger.info("Local socket disconnected due to socket error");
		m_RemoteSocket->Disconnect();
		return true;
	}
	else if (!m_LocalSocket->GetConnected()) {
		logger.info("Local socket disconnected");
		m_RemoteSocket->Disconnect();
		return true;
	}
	else {
		m_LocalSocket->DoRecv((fd_set*)fd);
		ExtractWC3Packets();
		ProcessWC3Packets();
	}
	// remote socket
	if (m_RemoteSocket->GetConnected()) {
	}
	if (m_RemoteSocket->HasError()) {
		logger.info("Remote socket disconnected due to socket error");
		m_LocalSocket->Disconnect();
		return true;
	}
	else if (!m_RemoteSocket->GetConnected() && !m_RemoteSocket->GetConnecting()) {
		logger.info("Remote socket disconnected");
		m_RemoteSocket->Disconnect();
		return true;
	}
	else if (m_RemoteSocket->GetConnecting()) {
		if (m_RemoteSocket->CheckConnect()) {
			logger.info("Remote socket connected with [" + m_RemoteSocket->GetIPString() + "]");
		}
	}

	if (m_RemoteSocket->GetConnected()) {
		m_RemoteSocket->DoRecv((fd_set*)fd);
		ExtractBNETPackets();
		ProcessBNETPackets();

		// send SID_NULL every minute to keep the TCP to pvpgn alive
		// for some reason W3 does not do this all the time
		// or is router supposed to do this? Who the fuck knows..
		if (nullTimer.passed(60000) && gGProxy->m_Logon) {
			BYTEARRAY packet;
			packet.push_back(255);
			packet.push_back(SID_NULL);
			packet.push_back(0);
			packet.push_back(0);
			AssignLength(packet);
			m_RemoteSocket->PutBytes(packet);

			nullTimer.start();
			logger.debug("SID_NULL sent");
		}
	}
	m_RemoteSocket->DoSend((fd_set*)send_fd);
	m_LocalSocket->DoSend((fd_set*)send_fd);

	return false;
}
void CWC3::Handle_SID_GETADVLISTEX(BYTEARRAY data)
{
	logger.debug("RECEIVED SID_GETADVLISTEX");
	// DEBUG_Print( data );

	// 2 bytes					-> Header
	// 2 bytes					-> Length
	// 4 bytes					-> GamesFound
	// for( 1 .. GamesFound )
	//		2 bytes				-> GameType
	//		2 bytes				-> Parameter
	//		4 bytes				-> Language ID
	//		2 bytes				-> AF_INET
	//		2 bytes				-> Port
	//		4 bytes				-> IP
	//		4 bytes				-> zeros
	//		4 bytes				-> zeros
	//		4 bytes				-> Status
	//		4 bytes				-> ElapsedTime
	//		null term string	-> GameName
	//		1 byte				-> GamePassword
	//		1 byte				-> SlotsTotal
	//		8 bytes				-> HostCounter (ascii hex format)
	//		null term string	-> StatString

	vector<CIncomingGameHost*> Games;
	if (ValidateLength(data) && data.size() >= 8) {
		unsigned int i = 8;
		uint32_t GamesFound = UTIL_ByteArrayToUInt32(data, false, 4);
		if (GamesFound == 0) {
			BYTEARRAY packet;
			packet.push_back(255);
			packet.push_back(SID_GETADVLISTEX);
			packet.push_back(0);
			packet.push_back(0);
			UTIL_AppendByteArray(packet, GamesFound, false);
			UTIL_AppendByteArray(packet, UTIL_ByteArrayToUInt32(data, false, 8), false);
			AssignLength(packet);
			m_LocalSocket->PutBytes(packet);
			return;
		}
		while (GamesFound > 0) {
			unsigned char ip[] = { 127, 0, 0, 1 };
			GamesFound--;

			if (data.size() < i + 33)
				break;
			uint16_t GameType = UTIL_ByteArrayToUInt16(data, false, i);
			i += 2;
			uint16_t Parameter = UTIL_ByteArrayToUInt16(data, false, i);
			i += 2;
			uint32_t LanguageID = UTIL_ByteArrayToUInt32(data, false, i);
			i += 4;
			// AF_INET
			i += 2;
			uint16_t Port = UTIL_ByteArrayToUInt16(data, true, i);
			i += 2;
			BYTEARRAY IP = BYTEARRAY(data.begin() + i, data.begin() + i + 4);
			i += 4;
			// zeros
			i += 4;
			// zeros
			i += 4;
			uint32_t Status = UTIL_ByteArrayToUInt32(data, false, i);
			i += 4;
			uint32_t ElapsedTime = UTIL_ByteArrayToUInt32(data, false, i);
			i += 4;
			BYTEARRAY GameName = UTIL_ExtractCString(data, i);

			int t = i;
			i += GameName.size() + 1;

			if (data.size() < i + 1)
				break;

			BYTEARRAY GamePassword = UTIL_ExtractCString(data, i);
			i += GamePassword.size() + 1;

			if (data.size() < i + 10)
				break;

			// SlotsTotal is in ascii hex format

			unsigned char SlotsTotal = data[i];
			unsigned int c;
			stringstream SS;
			SS << string(1, SlotsTotal);
			SS >> hex >> c;
			i++;

			// HostCounter is in reverse ascii hex format
			// e.g. 1  is "10000000"
			// e.g. 10 is "a0000000"
			// extract it, reverse it, parse it, construct a single uint32_t

			BYTEARRAY HostCounterRaw = BYTEARRAY(data.begin() + i, data.begin() + i + 8);
			string HostCounterString = string(HostCounterRaw.rbegin(), HostCounterRaw.rend());
			uint32_t HostCounter = 0;

			for (int j = 0; j < 4; j++) {
				unsigned int c;
				stringstream SS;
				SS << HostCounterString.substr(j * 2, 2);
				SS >> hex >> c;
				HostCounter |= c << (24 - j * 8);
			}

			i += 8;
			BYTEARRAY StatString = UTIL_ExtractCString(data, i);
			i += StatString.size() + 1;

			CIncomingGameHost* Game = new CIncomingGameHost(GameType, Parameter, LanguageID, Port, IP, Status, ElapsedTime, string(GameName.begin(), GameName.end()), GamePassword, c, SlotsTotal, HostCounter, HostCounterRaw, StatString, gGProxy);
			Games.push_back(Game);
		}
	}
	m_Games = Games;
	gGProxy->m_Games = Games;

	BYTEARRAY packet;
	packet.push_back(255);
	packet.push_back(SID_GETADVLISTEX);
	packet.push_back(0);
	packet.push_back(0);
	UTIL_AppendByteArray(packet, (uint32_t)m_Games.size(), false);

	for (vector<CIncomingGameHost*>::iterator i = m_Games.begin(); i != m_Games.end(); i++) {
		UTIL_AppendByteArray(packet, (*i)->GetData(string()));
		// UTIL_AppendByteArray(packet,(*i)->GetData(m_GIndicator ));
	}
	AssignLength(packet);
	m_LocalSocket->PutBytes(packet);
}
bool CWC3::Handle_SID_CHATEVENT(BYTEARRAY data)
{
	logger.debug("RECEIVED SID_CHATEVENT");
	// DEBUG_Print( data );

	// 2 bytes					-> Header
	// 2 bytes					-> Length
	// 4 bytes					-> EventID
	// 4 bytes					-> ???
	// 4 bytes					-> Ping
	// 12 bytes					-> ???
	// null terminated string	-> User
	// null terminated string	-> Message

	if (ValidateLength(data) && data.size() >= 29) {
		BYTEARRAY EventID = BYTEARRAY(data.begin() + 4, data.begin() + 8);
		BYTEARRAY Ping = BYTEARRAY(data.begin() + 12, data.begin() + 16);
		BYTEARRAY Username = UTIL_ExtractCString(data, 28);
		BYTEARRAY message = UTIL_ExtractCString(data, Username.size() + 29);
		string User = string(Username.begin(), Username.end());
		string Message = string(message.begin(), message.end());

		// don't receive anything if user is in this list
		string TestName = User;
		transform(TestName.begin(), TestName.end(), TestName.begin(), (int (*)(int))tolower);
		switch (UTIL_ByteArrayToUInt32(EventID, false)) {
			case EID_TALK: {
				for (vector<string>::iterator i = gMutedUsers.begin(); i != gMutedUsers.end(); i++) {
					if ((*i).find(TestName) != string::npos)
						return false;
				}
				logger.chat("User [" + User + "] said " + Message);
				if (Message.find("(for random). To see users type") != string::npos) {
					RunSound("challenge-started", gGProxy->m_SoundsOptions[6]);
				}
				else if (Message.find("Voted mode is") != string::npos) {
					RunSound("challenge-completed", gGProxy->m_SoundsOptions[7]);
				}
				break;
			}
			case EID_WHISPERSENT: {
				string tmpusr = User;
				transform(tmpusr.begin(), tmpusr.end(), tmpusr.begin(), (int (*)(int))tolower);
				string greeterUsername = getGreeterUsername();
				transform(greeterUsername.begin(), greeterUsername.end(), greeterUsername.begin(), (int (*)(int))tolower);

				// ignore special greeter messages
				if (tmpusr == greeterUsername && (Message.substr(0, 2).find("!8") != string::npos || Message.substr(0, 2).find("!h") != string::npos)) {
					return false;
				}
				else {
					if (User == "your friends")
						logger.info("[FRIEND WHISPER]: " + Message);
					else
						logger.info("[WHISPER] [" + User + "]: " + Message);
				}
				break;
			}
			case EID_WHISPER: {
				logger.debug("Whisper by user " + User);

				gGProxy->UDPSend("4", "[BNET] " + Message);
				for (vector<string>::iterator i = gMutedUsers.begin(); i != gMutedUsers.end(); i++) {
					if ((*i).find(TestName) != string::npos)
						return false;
				}

				bool blockSound = false;

				// Autojoin
				if (User == "la-dota" && gGProxy->autoJoinEnabled && (UTIL_startsWith(Message, "Creating private game") || UTIL_startsWith(Message, "Creating public game")) && currentGameState == IN_CHANNEL) {

					logger.debug("la-dota guard created our game");
					int firstB = Message.find_first_of('[');
					int firstE = Message.find_first_of(']');
					if (firstB != string::npos && firstE != string::npos && firstE > firstB) {
						string gn = Message.substr(firstB + 1, firstE - firstB - 1);
						logger.debug("Bot created game with gn " + gn);

						this->gameToAutoJoin = gn;
						RunSound("autojoin", gGProxy->m_SoundsOptions[11]);
						CmdBuffer::performGameJoin1(gGProxy->m_w3pid, gGProxy->autoJoinDelayInSec);
						blockSound = true;
					}
				}

				//! follow
				if (User == "PvPGN Realm" && this->playerToFollow != "" && gGProxy->autoJoinEnabled && currentGameState == IN_CHANNEL) {

					if (UTIL_startsWith(Message, "Watched user " + this->playerToFollow + " has entered a Warcraft III Frozen Throne game named")) {

						logger.debug("Performing follow");

						int firstE = Message.find_last_of('"');
						int firstB = Message.find_first_of('"');
						if (firstB != string::npos && firstE != string::npos && firstE > firstB) {
							string gn = Message.substr(firstB + 1, firstE - firstB - 1);
							logger.debug("Following gn " + gn);

							this->gameToAutoJoin = gn;

							// We delay following so it doesn't mess up if you are just typing sth in channel etc
							// Use detached thread to not block main
							RunSound("autojoin", gGProxy->m_SoundsOptions[11]);
							std::thread{ &CmdBuffer::performGameJoin1, gGProxy->m_w3pid, this->gGProxy->autoJoinDelayInSec }.detach();
							blockSound = true;
						}
					}
				}

				if (gGProxy->m_DND)
					return false;

				if (Message.find("has entered a Warcraft III Frozen Throne game named") != string::npos) {
					if (!blockSound) {
						RunSound("friend-join-game", gGProxy->m_SoundsOptions[8]);
						logger.info("[BNET] " + Message);
					}
				}

				if (User.find("PvPGN Realm") != string::npos)
					break;

				if (Message.find("send-slap") != string::npos) {
					RunSound("slap", gGProxy->m_SoundsOptions[3]);
				}
				else if (Message.find("cause if you don t you ll be banned for") != string::npos) {
					RunSound("hosted", gGProxy->m_SoundsOptions[2]);
				}
				else {
					if (!blockSound) {
						if (!UTIL_startsWith(User, "lagabuse.com.") || !gGProxy->autoJoinEnabled) {
							RunSound("whisper", gGProxy->m_SoundsOptions[9]);
						}
					}
				}

				logger.whisper("[" + User + "] said: " + Message);

				break;
			}
			case EID_EMOTE: {
				logger.chatMuted("[EMOTE] user [" + User + "] said " + Message);
				break;
			}
			case EID_INFO: {
				logger.chatInfo("[INFO] " + Message);
				if (Message.find("Channel is now moderated") != string::npos)
					RunSound("moderate", gGProxy->m_SoundsOptions[4]);
				else if (Message.find("Channel is now unmoderated") != string::npos)
					RunSound("unmoderate", gGProxy->m_SoundsOptions[5]);
				else if (Message == "Connection closed by admin.") {
					logger.info("You were kicked from server by warden/admin");
				}

				//! follow
				if (this->playerToFollow != "" && gGProxy->autoJoinEnabled && currentGameState == IN_CHANNEL) {

					logger.console(this->playerToFollow);
					logger.console(Message);
					if (UTIL_startsWith(Message, this->playerToFollow + " is using Warcraft III Frozen Throne and is currently in private game") || UTIL_startsWith(Message, this->playerToFollow + " is using Warcraft III Frozen Throne and is currently in public game") || UTIL_startsWith(Message, this->playerToFollow + " is using Warcraft III Frozen Throne and is currently in game")) {

						logger.debug("Performing follow");

						int firstE = Message.find_last_of('"');
						int firstB = Message.find_first_of('"');
						if (firstB != string::npos && firstE != string::npos && firstE > firstB) {
							string gn = Message.substr(firstB + 1, firstE - firstB - 1);
							logger.debug("Following gn " + gn);

							this->gameToAutoJoin = gn;
							RunSound("autojoin", gGProxy->m_SoundsOptions[11]);
							CmdBuffer::performGameJoin1(gGProxy->m_w3pid, gGProxy->autoJoinDelayInSec);
						}
					}
				}

				break;
			}
			case EID_ERROR: {
				logger.chatError("[ERROR] " + Message);
				break;
			}
			case EID_CHANNEL: {
				logger.chatInfo("[BNET] joined channel [" + Message + "]");
				currentGameState = IN_CHANNEL;
				logger.debug("Game state changed to IN_CHANNEL");

				if (gGProxy->chatBufferEnabled) {
					gGProxy->chatbuffer->mtxCmdBufferEnable.lock();
					gGProxy->chatbuffer->setCmdBufferEnable(true);
					gGProxy->chatbuffer->mtxCmdBufferEnable.unlock();
				}
				break;
			}
			case EID_SHOWUSER: {
				break;
			}
			case EID_JOIN: {
				break;
			}
			case EID_LEAVE: {
				break;
			}
			case EID_BROADCAST: {
				RunSound("announcement", gGProxy->m_SoundsOptions[12]);
				break;
			}
		}
	}
	return true;
}
void CWC3::Handle_SID_CHATCOMMAND(BYTEARRAY data)
{
	if (ValidateLength(data)) {
		BYTEARRAY msg = UTIL_ExtractCString(data, 4);
		string Message = string(msg.begin(), msg.end());
		if (ProcessCommand(Message)) {
			m_RemoteSocket->PutBytes(data);
			if ((Message[0] == '/' && Message[1] == 'w') || (Message[0] == '/' && Message[1] == 'f')) {
				logger.whisper("[WHISPER] " + Message);
			}
			else if (Message[0] == '/')
				logger.chat("[BNET COMMAND] " + Message);
			else {
				gGProxy->UDPSend("1", "[CHAT] " + Message);
				logger.chat("[CHAT] " + Message);
			}
		}
	}
}
bool CWC3::ProcessCommand(string Message)
{

	string Command;
	string Payload;
	string::size_type PayloadStart = Message.find(" ");
	if (Message[0] == '!') {
		if (PayloadStart != string::npos) {
			Command = Message.substr(1, PayloadStart - 1);
			Payload = Message.substr(PayloadStart + 1);
		}
		else
			Command = Message.substr(1);
	}
	else
		Command = Message;

	transform(Command.begin(), Command.end(), Command.begin(), (int (*)(int))tolower);
	bool forward = true; // if forward == true the chatcommand will be sent to bnet
						 // bnet commands in channel
	if (Message[0] == '/') {
		if (Command.size() >= 5 && (Command.substr(0, 5) == "/f m " || Command.substr(0, 5) == "/f w ")) {
			Payload = Message.substr(5);
			for (vector<CWC3*>::iterator i = gGProxy->m_Connections.begin(); i != gGProxy->m_Connections.end(); i++)
				(*i)->SendChatCommand("/f m (fm) " + Payload);
			forward = false;
		}
		else if (Command.size() >= 7 && Command.substr(0, 7) == "/f msg ") {
			Payload = Message.substr(7);
			for (vector<CWC3*>::iterator i = gGProxy->m_Connections.begin(); i != gGProxy->m_Connections.end(); i++)
				(*i)->SendChatCommand("/f m (fm) " + Payload);
			forward = false;
		}
	}

	if (Message[0] == '!') {

		if (Command == "mu" && !Payload.empty()) {
			forward = false;
			SendLocalChat("You just squelch (mute) " + Payload);
			transform(Payload.begin(), Payload.end(), Payload.begin(), (int (*)(int))tolower);
			gMutedUsers.push_back(Payload);
		}
		else if (Command == "ml" && Payload.empty()) {
			forward = false;
			SendLocalChat("Squelch (mute) list");
			SendLocalChat("===================");
			for (vector<string>::iterator i = gMutedUsers.begin(); i != gMutedUsers.end(); i++)
				SendLocalChat(*i);
			SendLocalChat("===================");
		}
		else if (Command == "md" && Payload.empty()) {
			forward = false;
			SendLocalChat("Squelch (muted) list cleared.");
			gMutedUsers.clear();
			// gFlistWindowChanged = true;
		}
		else if (Command == "v" && Payload.empty()) {
			SendLocalChat("GProxy++ Version " + gGProxy->m_Version + " XPAM edition");
			forward = false;
		}
		else if (Command == "dnd" && Payload.empty()) {
			forward = false;
			gGProxy->m_DND = !gGProxy->m_DND;

			if (gGProxy->m_DND)
				SendLocalChat("Do Not Disturb mode engaged.");
			else
				SendLocalChat("Do Not Disturb mode cancelled.");
		}
		else if (Command == "sounds") {
			gGProxy->m_Sounds = !gGProxy->m_Sounds;
			if (gGProxy->m_Sounds)
				SendLocalChat("Sounds enabled.");
			else
				SendLocalChat("Sounds disabled.");
			forward = false;
		}
		else if (Command == "commands") {
			SendLocalChat("----------------------");
			SendLocalChat("Available commands are");
			SendLocalChat("----------------------");
			SendLocalChat("!sounds (enable/disable sounds)");
			SendLocalChat("!sid (sound id enable or disable)");
			SendLocalChat("!v (version)");
			SendLocalChat("!mu <user> (mute user)");
			SendLocalChat("!md (delete muted users list)");
			SendLocalChat("!ml (print list of muted users)");
			SendLocalChat("!dnd (Do Not Disturb mode)");
			SendLocalChat("!apub <gn> (Host public ladder DotA game)");
			SendLocalChat("!apriv <gn> (Host private ladder DotA game)");
			SendLocalChat("!follow <player> (If player is in game join it. Alternatively, join when player enters.)");
			SendLocalChat("----------------------");
			forward = false;
		}
		else if (Command == "sid") {
			forward = false;
			if (Payload.empty()) {
				SendLocalChat("--------------------------------------------------");
				SendLocalChat("Sound id and messages");
				SendLocalChat("--------------------------------------------------");
				SendLocalChat("(1) game started");
				SendLocalChat("(2) game hosted");
				SendLocalChat("(3) slap (/w user send-slap)");
				SendLocalChat("(4) channel moderated");
				SendLocalChat("(5) channel unmoderated");
				SendLocalChat("(6) challenge started");
				SendLocalChat("(7) challenge complete");
				SendLocalChat("(8) friend joined a game");
				SendLocalChat("(9) whisper");
				SendLocalChat("(10) kicked from lobby");
				SendLocalChat("(11) autojoin event");
				SendLocalChat("(12) server announcement");
				SendLocalChat("--------------------------------------------------");
				forward = false;
			}
			else {
				string Victim;
				string Reason;
				stringstream SS;
				SS << Payload;
				SS >> Victim;

				if (!SS.eof()) {
					getline(SS, Reason);
					string::size_type Start = Reason.find_first_not_of(" ");

					if (Start != string::npos)
						Reason = Reason.substr(Start);
				}
				uint32_t id = UTIL_ToUInt32(Victim);
				transform(Reason.begin(), Reason.end(), Reason.begin(), (int (*)(int))tolower);
				if (id > 9)
					return false;

				if (Reason == "off") {
					SendLocalChat("Sound with id " + Victim + " disabled.");
					gGProxy->m_SoundsOptions[id] = 0;
				}
				else if (Reason == "on") {
					SendLocalChat("Sound with id " + Victim + " enabled.");
					gGProxy->m_SoundsOptions[id] = 1;
				}
				else if (Reason == "print") {
					if (gGProxy->m_SoundsOptions[id] == 1)
						SendLocalChat("Sound with id " + Victim + " is enabled.");
					else
						SendLocalChat("Sound with id " + Victim + " is disabled.");
				}
				else {
					SendLocalChat("Wrong input. Type !sid + commandid + (off, on or print)");
					SendLocalChat("To see sound id just type !sid");
				}
			}
		}
		else if (Command == "apub" && !Payload.empty()) {
			string cmd = "/w la-dota !pub " + Payload;
			SendChatCommand(cmd);
			forward = false;
		}
		else if (Command == "apriv" && !Payload.empty()) {
			string cmd = "/w la-dota !priv " + Payload;
			SendChatCommand(cmd);
			forward = false;
		}
		else if (Command == "follow" && !Payload.empty() && this->gGProxy->autoJoinEnabled) {
			string cmd = "/whereis " + Payload;
			SendChatCommand(cmd);

			string cmd2 = "/watch " + Payload;
			SendChatCommand(cmd2);

			this->playerToFollow = Payload;

			SendLocalChat("Started following " + this->playerToFollow);

			forward = false;
		}
		else if (Command == "unfollow" && Payload.empty() && this->gGProxy->autoJoinEnabled) {
			if (this->playerToFollow != "") {
				string cmd = "/unwatch " + this->playerToFollow;
				SendChatCommand(cmd);

				SendLocalChat("Unfollowed " + this->playerToFollow);
				this->playerToFollow = "";
			}

			forward = false;
		}
	}

	// insert into global chat buffer
	if (gGProxy->chatBufferEnabled) {
		gGProxy->chatbuffer->mtxChatBuffer.lock();
		gGProxy->chatbuffer->addChatBuffer(Message);
		gGProxy->chatbuffer->mtxChatBuffer.unlock();
	}

	return forward;
}

void CWC3::ExtractWC3Packets()
{
	string* RecvBuffer = m_LocalSocket->GetBytes();
	BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char*)RecvBuffer->c_str(), RecvBuffer->size());
	if (m_FirstPacket) {
		if (Bytes.size() >= 1) {
			if (Bytes[0] == 1) {
				BYTEARRAY packet;
				packet.push_back(1);
				m_RemoteSocket->PutBytes(packet);
				*RecvBuffer = RecvBuffer->substr(1);
				Bytes = BYTEARRAY(Bytes.begin() + 1, Bytes.end());
				logger.info(" Connection marked as WC3");
				m_FirstPacket = false;
			}
			if (Bytes[0] == 2) {
				logger.info(" Connection marked as BNFTP");
				m_FirstPacket = false;
				m_IsBNFTP = true;
			}
		}
	}
	if (m_IsBNFTP) {
		m_RemoteSocket->PutBytes(Bytes);
		*RecvBuffer = RecvBuffer->substr(Bytes.size());
		return;
	}
	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes
	while (Bytes.size() >= 4) {
		// byte 0 is always 255

		if (Bytes[0] == 255) {
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16(Bytes, false, 2);

			if (Length >= 4) {
				if (Bytes.size() >= Length) {
					BYTEARRAY Data = BYTEARRAY(Bytes.begin(), Bytes.begin() + Length);
					m_LocalPackets.push(new CCommandPacket(255, Bytes[1], Data));
					*RecvBuffer = RecvBuffer->substr(Length);
					Bytes = BYTEARRAY(Bytes.begin() + Length, Bytes.end());
				}
				else
					return;
			}
			else {
				logger.info(" received invalid packet from wc3 (bad length)");
				return;
			}
		}
		else {
			logger.info(" received invalid packet from wc3 (bad header constant)");
			logger.info(UTIL_ByteArrayToDecString(Bytes));
			return;
		}
	}
}
void CWC3::ProcessWC3Packets()
{
	queue<CCommandPacket*> temp;
	bool forward = true;
	while (!m_LocalPackets.empty()) {
		forward = true;
		CCommandPacket* packet = m_LocalPackets.front();
		m_LocalPackets.pop();

		logger.telemetry(UTIL_ToString(packet->GetID()));

		switch (packet->GetID()) {

			case SID_CHATCOMMAND: {
				logger.debug("Processed SID_CHATCOMMAND");
				Handle_SID_CHATCOMMAND(packet->GetData());
				forward = false;
				delete packet;
				break;
			}
			case SID_AUTH_ACCOUNTLOGONPROOF: {
				break;
			}
			case SID_AUTH_ACCOUNTLOGON: {
				BYTEARRAY data = packet->GetData();
				BYTEARRAY msg = UTIL_ExtractCString(data, 36);
				string username = string(msg.begin(), msg.end());
				logger.info("Username used for login is " + username);
#ifdef XPAM_EDITION
				gGProxy->m_pfUsername = username;
#endif
				break;
			}
			default:
				break;
		}
		if (forward) {
			if (m_RemoteSocket->GetConnected()) {
				m_RemoteSocket->PutBytes(packet->GetData());
				delete packet;
			}
			else {
				temp.push(packet);
			}
		}
	}
	m_LocalPackets = temp;
}

void CWC3::ExtractBNETPackets()
{
	string* RecvBuffer = m_RemoteSocket->GetBytes();
	BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char*)RecvBuffer->c_str(), RecvBuffer->size());

	if (m_IsBNFTP) {
		m_LocalSocket->PutBytes(Bytes);
		*RecvBuffer = RecvBuffer->substr(Bytes.size());
		return;
	}
	// a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes
	while (Bytes.size() >= 4) {
		// byte 0 is always 255

		if (Bytes[0] == 255) {
			// bytes 2 and 3 contain the length of the packet

			uint16_t Length = UTIL_ByteArrayToUInt16(Bytes, false, 2);

			if (Length >= 4) {
				if (Bytes.size() >= Length) {
					BYTEARRAY Data = BYTEARRAY(Bytes.begin(), Bytes.begin() + Length);
					m_RemotePackets.push(new CCommandPacket(255, Bytes[1], Data));
					*RecvBuffer = RecvBuffer->substr(Length);
					Bytes = BYTEARRAY(Bytes.begin() + Length, Bytes.end());
				}
				else
					return;
			}
			else {
				logger.info(" received invalid packet from bnet (bad length)");
				return;
			}
		}
		else {
			logger.info(" received invalid packet from bnet (bad header constant)");
			return;
		}
	}
}

void CWC3::ProcessBNETPackets()
{
	bool forward = true;
	while (!m_RemotePackets.empty()) {
		forward = true;
		CCommandPacket* packet = m_RemotePackets.front();
		m_RemotePackets.pop();

		logger.telemetry("Processing BNET packet " + UTIL_ToString(packet->GetID()));

		switch (packet->GetID()) {
			case SID_STARTADVEX3: {
				logger.debug("Processing SID_STARTADVEX3");
				break;
			}
			case SID_GETADVLISTEX: {
				logger.debug("Processing SID_GETADVLISTEX");
				currentGameState = IN_GAMELIST;
				logger.debug("Game state changed to IN_GAMELIST");

				Handle_SID_GETADVLISTEX(packet->GetData());

				if (gGProxy->autoJoinEnabled && this->gameToAutoJoin != "") {
					CmdBuffer::performGameJoin2(gGProxy->m_w3pid, this->gameToAutoJoin, this->gGProxy->autoJoinGnDelayInSec);
					this->gameToAutoJoin = "";

					// Follow: Only do it one time to avoid confusion
					if (this->playerToFollow != "") {
						string cmd = "/unwatch " + this->playerToFollow;
						SendChatCommand(cmd);
						this->playerToFollow = "";
					}
				}

				forward = false;
				break;
			}
			case SID_FRIENDSLIST: {
				logger.debug("Processing SID_FRIENDSLIST");
				break;
			}
			case SID_CHATEVENT: {
				logger.debug("Processing SID_CHATEVENT");
				forward = Handle_SID_CHATEVENT(packet->GetData());
				break;
			}
			case SID_PROFILE: {
				logger.debug("Processing SID_PROFILE");
				break;
			}
			case SID_NOTIFYJOIN: {
				logger.debug("Processing SID_NOTIFYJOIN");
				currentGameState = IN_LOBBY;
				logger.debug("Game state changed to IN_LOBBY");
				break;
			}
			case SID_AUTH_ACCOUNTLOGONPROOF: {
				logger.debug("Processed SID_AUTH_ACCOUNTLOGONPROOF");
				gGProxy->UDPSend("1", "[GPROXY] logon bnet successful");
				logger.info("logon bnet successful");
				gGProxy->m_Logon = true;
				gGProxy->m_LogonEvent = true;
				break;
			}
			default:
				break;
		}

		if (forward) {
			m_LocalSocket->PutBytes(packet->GetData());
		}
		delete packet;
	}
}
void CWC3::SendLocalChat(string message)
{
	// send the message from user gproxy as a whisper using the sid_chatevent packet;
	string user = "GProxy";
	BYTEARRAY packet;
	packet.push_back(255);
	packet.push_back(SID_CHATEVENT);
	packet.push_back(0);
	packet.push_back(0);
	UTIL_AppendByteArray(packet, (uint32_t)EID_WHISPER, false); // event id
	UTIL_AppendByteArray(packet, (uint32_t)0, false);			// user flags
	UTIL_AppendByteArray(packet, (uint32_t)0, false);			// ping
	UTIL_AppendByteArray(packet, (uint32_t)0, false);			// ipaddress
	UTIL_AppendByteArray(packet, (uint32_t)0, false);			// account number
	UTIL_AppendByteArray(packet, (uint32_t)0, false);			// registration authority
	UTIL_AppendByteArrayFast(packet, user);						// username
	UTIL_AppendByteArrayFast(packet, message);					// message
	AssignLength(packet);
	m_LocalSocket->PutBytes(packet);
}

void CWC3::QueueChatCommand(string message)
{
	// SEND_SID_CHATCOMMAND
	if (m_QueueMessageTimer.passed(2000)) {
		m_QueueMessageTimer.start();
		BYTEARRAY packet;
		packet.push_back(255);
		packet.push_back(SID_CHATCOMMAND);
		packet.push_back(0);
		packet.push_back(0);
		UTIL_AppendByteArrayFast(packet, message);
		AssignLength(packet);
		m_RemoteSocket->PutBytes(packet);
		// add here in w3 so we know what we wrote via it
		gGProxy->SendLocalChat(message);
	}
}
void CWC3::SendChatCommand(string message)
{
	// SEND_SID_CHATCOMMAND
	BYTEARRAY packet;
	packet.push_back(255);
	packet.push_back(SID_CHATCOMMAND);
	packet.push_back(0);
	packet.push_back(0);
	UTIL_AppendByteArrayFast(packet, message);
	AssignLength(packet);
	m_RemoteSocket->PutBytes(packet);
}
bool CWC3::AssignLength(BYTEARRAY& content)
{
	// insert the actual length of the content array into bytes 3 and 4 (indices 2 and 3)

	BYTEARRAY LengthBytes;

	if (content.size() >= 4 && content.size() <= 65535) {
		LengthBytes = UTIL_CreateByteArray((uint16_t)content.size(), false);
		content[2] = LengthBytes[0];
		content[3] = LengthBytes[1];
		return true;
	}

	return false;
}

bool CWC3::ValidateLength(BYTEARRAY& content)
{
	// verify that bytes 3 and 4 (indices 2 and 3) of the content array describe the length
	uint16_t Length;
	BYTEARRAY LengthBytes;

	if (content.size() >= 4 && content.size() <= 65535) {
		LengthBytes.push_back(content[2]);
		LengthBytes.push_back(content[3]);
		Length = UTIL_ByteArrayToUInt16(LengthBytes, false);

		if (Length == content.size())
			return true;
	}

	return false;
}

void CWC3::SEND_SID_FRIENDSLIST()
{
	//	SID_FRIENDSLIST				= 101,	// 0x65
	//	SID_FRIENDSUPDATE			= 102,
	BYTEARRAY packet;
	packet.push_back(255); // BNET header constant
	packet.push_back(101); // SID_FRIENDSLIST
	packet.push_back(0);   // packet length will be assigned later
	packet.push_back(0);   // packet length will be assigned later
	AssignLength(packet);
	// DEBUG_Print( "SENT SID_FRIENDSLIST" );
	// DEBUG_Print( packet );
	m_RemoteSocket->PutBytes(packet);
}
void CWC3::SEND_SID_FRIENDSLIST_UPDATE()
{
	//	SID_FRIENDSLIST				= 101,	// 0x65
	//	SID_FRIENDSUPDATE			= 102,
	BYTEARRAY packet;
	packet.push_back(255); // BNET header constant
	packet.push_back(102); // SID_FRIENDSLIST
	packet.push_back(0);   // packet length will be assigned later
	packet.push_back(0);   // packet length will be assigned later
	AssignLength(packet);
	// DEBUG_Print( "SENT SID_FRIENDSLIST" );
	// DEBUG_Print( packet );
	m_RemoteSocket->PutBytes(packet);
}