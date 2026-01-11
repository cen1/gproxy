#include "incomingfriendlist.h"

CIncomingFriendList ::CIncomingFriendList(string nAccount, unsigned char nStatus, unsigned char nArea, string nLocation)
{
	m_Account = nAccount;
	m_Status = nStatus;
	m_Area = nArea;
	m_Location = nLocation;
}

CIncomingFriendList ::~CIncomingFriendList()
{
}

string CIncomingFriendList ::GetDescription()
{
	string Description;
	Description += GetAccount() + "\n";
	Description += ExtractStatus(GetStatus()) + "\n";
	Description += ExtractArea(GetArea()) + "\n";
	Description += ExtractLocation(GetLocation()) + "\n\n";
	return Description;
}

string CIncomingFriendList ::ExtractStatus(unsigned char status)
{
	string Result;

	if (status & 1)
		Result += "<Mutual>";

	if (status & 2)
		Result += "<DND>";

	if (status & 4)
		Result += "<Away>";

	if (Result.empty())
		Result = "<None>";

	return Result;
}

string CIncomingFriendList ::ExtractArea(unsigned char area)
{
	switch (area) {
		case 0:
			return "<Offline>";
		case 1:
			return "<No Channel>";
		case 2:
			return "<In Channel>";
		case 3:
			return "<Public Game>";
		case 4:
			return "<Private Game>";
		case 5:
			return "<Private Game>";
	}

	return "<Unknown>";
}

string CIncomingFriendList ::ExtractLocation(string location)
{
	string Result;

	if (location.substr(0, 4) == "PX3W")
		Result = location.substr(4);

	if (Result.empty())
		Result = ".";

	return Result;
}