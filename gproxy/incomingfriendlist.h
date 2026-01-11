#pragma once

#include <string>

using namespace std;

class CIncomingFriendList {
  private:
	string m_Account;
	unsigned char m_Status;
	unsigned char m_Area;
	string m_Location;

  public:
	CIncomingFriendList(string nAccount, unsigned char nStatus, unsigned char nArea, string nLocation);
	~CIncomingFriendList();

	string GetAccount() { return m_Account; }
	unsigned char GetStatus() { return m_Status; }
	unsigned char GetArea() { return m_Area; }
	string GetLocation() { return m_Location; }
	string GetDescription();

  private:
	string ExtractStatus(unsigned char status);
	string ExtractArea(unsigned char area);
	string ExtractLocation(string location);
};