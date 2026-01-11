#include "incominggamehost.h"
#include "gproxy.h"
#include "util.h"

uint32_t CIncomingGameHost::NextUniqueGameID = 1;

CIncomingGameHost::CIncomingGameHost(uint16_t nGameType, uint16_t nParameter, uint32_t nLanguageID, uint16_t nPort, BYTEARRAY& nIP, uint32_t nStatus, uint32_t nElapsedTime, string nGameName, BYTEARRAY nGamePassword, unsigned char nSlotsTotal, unsigned char nSlotsTotalRAW, uint32_t nHostCounter, BYTEARRAY nHostCounterRAW, BYTEARRAY& nStatString, CGProxy* gGProxyPtr)
{
	m_GameType = nGameType;
	m_Parameter = nParameter;
	m_LanguageID = nLanguageID;
	m_Port = nPort;
	m_IP = nIP;
	m_Status = nStatus;
	m_ElapsedTime = nElapsedTime;
	m_GameName = nGameName;
	m_SlotsTotal = nSlotsTotal;
	m_HostCounter = nHostCounter;
	m_StatString = nStatString;
	m_UniqueGameID = NextUniqueGameID++;
	m_ReceivedTime = GetTime();
	m_GamePassword = nGamePassword;
	m_HostCounterRAW = nHostCounterRAW;
	m_SlotsTotalRAW = nSlotsTotalRAW;
	gGProxy = gGProxyPtr;

	// decode stat string

	BYTEARRAY StatString = UTIL_DecodeStatString(m_StatString);
	BYTEARRAY MapFlags;
	BYTEARRAY MapWidth;
	BYTEARRAY MapHeight;
	BYTEARRAY MapCRC;
	BYTEARRAY MapPath;
	BYTEARRAY HostName;

	if (StatString.size() >= 14) {
		unsigned int i = 13;
		MapFlags = BYTEARRAY(StatString.begin(), StatString.begin() + 4);
		MapWidth = BYTEARRAY(StatString.begin() + 5, StatString.begin() + 7);
		MapHeight = BYTEARRAY(StatString.begin() + 7, StatString.begin() + 9);
		MapCRC = BYTEARRAY(StatString.begin() + 9, StatString.begin() + 13);
		MapPath = UTIL_ExtractCString(StatString, 13);
		i += MapPath.size() + 1;

		m_MapFlags = UTIL_ByteArrayToUInt32(MapFlags, false);
		// logger.info("FLAGGGS-------"+UTIL_ByteArrayToDecString( MapFlags )  );
		m_MapWidth = UTIL_ByteArrayToUInt16(MapWidth, false);
		m_MapHeight = UTIL_ByteArrayToUInt16(MapHeight, false);
		gGProxy->logger.info("Decoded map dimensions from StatString: width=" + UTIL_ToString(m_MapWidth) + ", height=" + UTIL_ToString(m_MapHeight));
		m_MapCRC = MapCRC;
		m_MapPath = string(MapPath.begin(), MapPath.end());

		if (StatString.size() >= i + 1) {
			HostName = UTIL_ExtractCString(StatString, i);
			m_HostName = string(HostName.begin(), HostName.end());
		}
	}
}

CIncomingGameHost ::~CIncomingGameHost()
{
}

BYTEARRAY CIncomingGameHost::GetData(string indicator)
{
	BYTEARRAY packet;
	unsigned char ip[] = { 127, 0, 0, 1 };
	unsigned char Zero[] = { 0, 0, 0, 0 };
	UTIL_AppendByteArray(packet, m_GameType, false);
	UTIL_AppendByteArray(packet, m_Parameter, false);
	UTIL_AppendByteArray(packet, m_LanguageID, false);
	packet.push_back(2);
	packet.push_back(0);
	UTIL_AppendByteArray(packet, gGProxy->m_Port, true);
	UTIL_AppendByteArray(packet, ip, 4);
	UTIL_AppendByteArray(packet, Zero, 4);
	UTIL_AppendByteArray(packet, Zero, 4);
	UTIL_AppendByteArray(packet, m_Status, false);
	UTIL_AppendByteArray(packet, m_ElapsedTime, false);
	if (!(m_Status == 17)) {
		if (m_MapWidth == 1984 && m_MapHeight == 1984) {
			string GameName = indicator + m_GameName.substr(0, m_GameName.size() - indicator.size());
			UTIL_AppendByteArrayFast(packet, GameName, true);
		}
		else {
			UTIL_AppendByteArrayFast(packet, m_GameName, true);
		}
	}
	else {
		UTIL_AppendByteArrayFast(packet, m_GameName, true);
	}
	string pwd = string(m_GamePassword.begin(), m_GamePassword.end());
	UTIL_AppendByteArrayFast(packet, pwd);
	packet.push_back(m_SlotsTotalRAW);
	UTIL_AppendByteArray(packet, m_HostCounterRAW);
	string StatString = string(m_StatString.begin(), m_StatString.end());
	UTIL_AppendByteArrayFast(packet, StatString);
	return packet;
}
string CIncomingGameHost::GetIPString()
{
	string Result;

	if (m_IP.size() >= 4) {
		for (unsigned int i = 0; i < 4; i++) {
			Result += UTIL_ToString((unsigned int)m_IP[i]);

			if (i < 3)
				Result += ".";
		}
	}

	return Result;
}