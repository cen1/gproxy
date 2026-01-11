#pragma once

#include "commandpacket.h"
#include "config.h"
#include "gproxy.h"
#include "incomingfriendlist.h"
#include "incominggamehost.h"
#include "log.h"
#include "socket.h"
#include "timer.h"
#include <queue>
#include <string>
#include <vector>

typedef vector<unsigned char> BYTEARRAY;

using namespace std;

// forward declarations
class CGProxy;

class CWC3 {
  private:
	enum Protocol {
		SID_NULL = 0,					 // 0x0
		SID_STOPADV = 2,				 // 0x2
		SID_GETADVLISTEX = 9,			 // 0x9
		SID_ENTERCHAT = 10,				 // 0xA
		SID_JOINCHANNEL = 12,			 // 0xC
		SID_CHATCOMMAND = 14,			 // 0xE
		SID_CHATEVENT = 15,				 // 0xF
		SID_CHECKAD = 21,				 // 0x15
		SID_STARTADVEX3 = 28,			 // 0x1C
		SID_DISPLAYAD = 33,				 // 0x21
		SID_NOTIFYJOIN = 34,			 // 0x22
		SID_PING = 37,					 // 0x25
		SID_LOGONRESPONSE = 41,			 // 0x29
		SID_PROFILE = 53,				 // 0x35
		SID_NETGAMEPORT = 69,			 // 0x45
		SID_AUTH_INFO = 80,				 // 0x50
		SID_AUTH_CHECK = 81,			 // 0x51
		SID_AUTH_ACCOUNTLOGON = 83,		 // 0x53
		SID_AUTH_ACCOUNTLOGONPROOF = 84, // 0x54
		SID_WARDEN = 94,				 // 0x5E
		SID_FRIENDSLIST = 101,			 // 0x65
		SID_FRIENDSUPDATE = 102,		 // 0x66
		SID_CLANMEMBERLIST = 125,		 // 0x7D
		SID_CLANMEMBERSTATUSCHANGE = 127 // 0x7F
	};
	enum IncomingChatEvent {
		EID_SHOWUSER = 1,			  // received when you join a channel (includes users in the channel and their information)
		EID_JOIN = 2,				  // received when someone joins the channel you're currently in
		EID_LEAVE = 3,				  // received when someone leaves the channel you're currently in
		EID_WHISPER = 4,			  // received a whisper message
		EID_TALK = 5,				  // received when someone talks in the channel you're currently in
		EID_BROADCAST = 6,			  // server broadcast
		EID_CHANNEL = 7,			  // received when you join a channel (includes the channel's name, flags)
		EID_USERFLAGS = 9,			  // user flags updates
		EID_WHISPERSENT = 10,		  // sent a whisper message
		EID_CHANNELFULL = 13,		  // channel is full
		EID_CHANNELDOESNOTEXIST = 14, // channel does not exist
		EID_CHANNELRESTRICTED = 15,	  // channel is restricted
		EID_INFO = 18,				  // broadcast/information message
		EID_ERROR = 19,				  // error message
		EID_EMOTE = 23				  // emote

	};

	Log logger;

	Timer nullTimer;
	Timer m_QueueMessageTimer;

	CGProxy* gGProxy;

	CTCPSocket* m_LocalSocket;
	CTCPClient* m_RemoteSocket;
	bool m_IsBNFTP;
	string m_GIndicator;
	bool m_FirstPacket;
	vector<CIncomingGameHost*> m_Games;
	queue<CCommandPacket*> m_LocalPackets;
	queue<CCommandPacket*> m_RemotePackets;
	vector<string> gMutedUsers;

	string gameToAutoJoin;
	string playerToFollow;

	void ExtractWC3Packets();
	void ProcessWC3Packets();

	void ExtractBNETPackets();
	void ProcessBNETPackets();

	bool AssignLength(BYTEARRAY& content);
	bool ValidateLength(BYTEARRAY& content);

	// packet handling
	void SEND_SID_FRIENDSLIST();
	void SEND_SID_FRIENDSLIST_UPDATE();
	void Handle_SID_GETADVLISTEX(BYTEARRAY data);
	bool Handle_SID_CHATEVENT(BYTEARRAY data);
	void Handle_SID_CHATCOMMAND(BYTEARRAY data);
	bool ProcessCommand(string Command);

  public:
	enum GameState {
		IN_CHANNEL = 1,
		IN_GAMELIST = 2,
		IN_LOBBY = 3,
		IN_GAME = 4,
		IN_OTHER = 5
	};

	CWC3(CTCPSocket* socket, string hostname, uint16_t port, string indicator, CGProxy* gGProxy);
	~CWC3();
	unsigned int SetFD(void* fd, void* send_fd, int* nfds);
	bool Update(void* fd, void* send_fd);
	void SendChatCommand(string message);
	void SendLocalChat(string message);
	void QueueChatCommand(string message);
	vector<CIncomingGameHost*> GetGames() { return m_Games; }
	vector<CIncomingFriendList*> RECEIVE_SID_FRIENDSLIST(BYTEARRAY data);

	GameState currentGameState;
};