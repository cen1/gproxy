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

#include "util.h"
#include "boost/filesystem.hpp"
#include <botan/base64.h>
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <windows.h>
#include <ctime>
#include <sstream>
#include <fstream>
#include <iomanip>

#include <sys/stat.h>
#ifdef __MINGW32__
#include "mingw_atlbase.h"
#else
#include <atlbase.h>
#endif

BYTEARRAY UTIL_CreateByteArray( unsigned char *a, int size )
{
	if( size < 1 )
		return BYTEARRAY( );

	return BYTEARRAY( a, a + size );
}

BYTEARRAY UTIL_CreateByteArray( unsigned char c )
{
	BYTEARRAY result;
	result.push_back( c );
	return result;
}

BYTEARRAY UTIL_CreateByteArray( uint16_t i, bool reverse )
{
	BYTEARRAY result;
	result.push_back( (unsigned char)i );
	result.push_back( (unsigned char)( i >> 8 ) );

	if( reverse )
		return BYTEARRAY( result.rbegin( ), result.rend( ) );
	else
		return result;
}

BYTEARRAY UTIL_CreateByteArray( uint32_t i, bool reverse )
{
	BYTEARRAY result;
	result.push_back( (unsigned char)i );
	result.push_back( (unsigned char)( i >> 8 ) );
	result.push_back( (unsigned char)( i >> 16 ) );
	result.push_back( (unsigned char)( i >> 24 ) );

	if( reverse )
		return BYTEARRAY( result.rbegin( ), result.rend( ) );
	else
		return result;
}

uint16_t UTIL_ByteArrayToUInt16( BYTEARRAY b, bool reverse, unsigned int start )
{
	if( b.size( ) < start + 2 )
		return 0;

	BYTEARRAY temp = BYTEARRAY( b.begin( ) + start, b.begin( ) + start + 2 );

	if( reverse )
		temp = BYTEARRAY( temp.rbegin( ), temp.rend( ) );

	return (uint16_t)( temp[1] << 8 | temp[0] );
}

uint32_t UTIL_ByteArrayToUInt32( BYTEARRAY b, bool reverse, unsigned int start )
{
	if( b.size( ) < start + 4 )
		return 0;

	BYTEARRAY temp = BYTEARRAY( b.begin( ) + start, b.begin( ) + start + 4 );

	if( reverse )
		temp = BYTEARRAY( temp.rbegin( ), temp.rend( ) );

	return (uint32_t)( temp[3] << 24 | temp[2] << 16 | temp[1] << 8 | temp[0] );
}

string UTIL_ByteArrayToDecString( BYTEARRAY b )
{
	if( b.empty( ) )
		return string( );

	string result = UTIL_ToString( b[0] );

	for( BYTEARRAY :: iterator i = b.begin( ) + 1; i != b.end( ); i++ )
		result += " " + UTIL_ToString( *i );

	return result;
}

string UTIL_ByteArrayToHexString( BYTEARRAY b )
{
	if( b.empty( ) )
		return string( );

	string result = UTIL_ToHexString( b[0] );

	for( BYTEARRAY :: iterator i = b.begin( ) + 1; i != b.end( ); i++ )
	{
		if( *i < 16 )
			result += " 0" + UTIL_ToHexString( *i );
		else
			result += " " + UTIL_ToHexString( *i );
	}

	return result;
}

void UTIL_AppendByteArray( BYTEARRAY &b, BYTEARRAY append )
{
	b.insert( b.end( ), append.begin( ), append.end( ) );
}

void UTIL_AppendByteArrayFast( BYTEARRAY &b, BYTEARRAY &append )
{
	b.insert( b.end( ), append.begin( ), append.end( ) );
}

void UTIL_AppendByteArray( BYTEARRAY &b, unsigned char *a, int size )
{
	UTIL_AppendByteArray( b, UTIL_CreateByteArray( a, size ) );
}

void UTIL_AppendByteArray( BYTEARRAY &b, string append, bool terminator )
{
	// append the string plus a null terminator

	b.insert( b.end( ), append.begin( ), append.end( ) );

	if( terminator )
		b.push_back( 0 );
}

void UTIL_AppendByteArrayFast( BYTEARRAY &b, string &append, bool terminator )
{
	// append the string plus a null terminator

	b.insert( b.end( ), append.begin( ), append.end( ) );

	if( terminator )
		b.push_back( 0 );
}

void UTIL_AppendByteArray( BYTEARRAY &b, uint16_t i, bool reverse )
{
	UTIL_AppendByteArray( b, UTIL_CreateByteArray( i, reverse ) );
}

void UTIL_AppendByteArray( BYTEARRAY &b, uint32_t i, bool reverse )
{
	UTIL_AppendByteArray( b, UTIL_CreateByteArray( i, reverse ) );
}

BYTEARRAY UTIL_ExtractCString( BYTEARRAY &b, unsigned int start )
{
	// start searching the byte array at position 'start' for the first null value
	// if found, return the subarray from 'start' to the null value but not including the null value

	if( start < b.size( ) )
	{
		for( unsigned int i = start; i < b.size( ); i++ )
		{
			if( b[i] == 0 )
				return BYTEARRAY( b.begin( ) + start, b.begin( ) + i );
		}

		// no null value found, return the rest of the byte array

		return BYTEARRAY( b.begin( ) + start, b.end( ) );
	}

	return BYTEARRAY( );
}

unsigned char UTIL_ExtractHex( BYTEARRAY &b, unsigned int start, bool reverse )
{
	// consider the byte array to contain a 2 character ASCII encoded hex value at b[start] and b[start + 1] e.g. "FF"
	// extract it as a single decoded byte

	if( start + 1 < b.size( ) )
	{
		unsigned int c;
		string temp = string( b.begin( ) + start, b.begin( ) + start + 2 );

		if( reverse )
			temp = string( temp.rend( ), temp.rbegin( ) );

		std::stringstream SS;
		SS << temp;
		SS >> std::hex >> c;
		return c;
	}

	return 0;
}

BYTEARRAY UTIL_ExtractNumbers( string s, unsigned int count )
{
	// consider the string to contain a bytearray in dec-text form, e.g. "52 99 128 1"

	BYTEARRAY result;
	unsigned int c;
	std::stringstream SS;
	SS << s;

	for( unsigned int i = 0; i < count; i++ )
	{
		if( SS.eof( ) )
			break;

		SS >> c;

		// todotodo: if c > 255 handle the error instead of truncating

		result.push_back( (unsigned char)c );
	}

	return result;
}

BYTEARRAY UTIL_ExtractHexNumbers( string s )
{
	// consider the string to contain a bytearray in hex-text form, e.g. "4e 17 b7 e6"

	BYTEARRAY result;
	unsigned int c;
	std::stringstream SS;
	SS << s;

	while( !SS.eof( ) )
	{
		SS >> std::hex >> c;

		// todotodo: if c > 255 handle the error instead of truncating

		result.push_back( (unsigned char)c );
	}

	return result;
}

string UTIL_ToString( float f, int digits )
{
	string result;
	std::stringstream SS;
	SS << std :: fixed << std :: setprecision( digits ) << f;
	SS >> result;
	return result;
}

string UTIL_ToString( double d, int digits )
{
	string result;
	std::stringstream SS;
	SS << std :: fixed << std::setprecision( digits ) << d;
	SS >> result;
	return result;
}

string UTIL_ToHexString( uint32_t i )
{
	string result;
	std::stringstream SS;
	SS << std :: hex << i;
	SS >> result;
	return result;
}

// todotodo: these UTIL_ToXXX functions don't fail gracefully, they just return garbage (in the uint case usually just -1 casted to an unsigned type it looks like)

uint16_t UTIL_ToUInt16( string &s )
{
	uint16_t result;
	std::stringstream SS;
	SS << s;
	SS >> result;
	return result;
}

uint32_t UTIL_ToUInt32( string &s )
{
	uint32_t result;
	std::stringstream SS;
	SS << s;
	SS >> result;
	return result;
}

int16_t UTIL_ToInt16( string &s )
{
	int16_t result;
	std::stringstream SS;
	SS << s;
	SS >> result;
	return result;
}

int32_t UTIL_ToInt32( string &s )
{
	int32_t result;
	std::stringstream SS;
	SS << s;
	SS >> result;
	return result;
}

double UTIL_ToDouble( string &s )
{
	double result;
	std::stringstream SS;
	SS << s;
	SS >> result;
	return result;
}

string UTIL_MSToString( uint32_t ms )
{
	string MinString = UTIL_ToString( ( ms / 1000 ) / 60 );
	string SecString = UTIL_ToString( ( ms / 1000 ) % 60 );

	if( MinString.size( ) == 1 )
		MinString.insert( 0, "0" );

	if( SecString.size( ) == 1 )
		SecString.insert( 0, "0" );

	return MinString + "m" + SecString + "s";
}

bool UTIL_FileExists( string file )
{
	struct stat fileinfo;

	if( stat( file.c_str( ), &fileinfo ) == 0 )
		return true;

	return false;
}

string UTIL_FileRead( string file, uint32_t start, uint32_t length )
{
	std::ifstream IS;
	IS.open( file.c_str( ), std::ios::binary );

	if( IS.fail( ) )
	{
		//logger.info( "[UTIL] warning - unable to read file part [" + file + "]" );
		return string( );
	}

	// get length of file

	IS.seekg( 0, std::ios::end );
	uint32_t FileLength = IS.tellg( );

	if( start > FileLength )
	{
		IS.close( );
		return string( );
	}

	IS.seekg( start, std::ios::beg );

	// read data

	char *Buffer = new char[length];
	IS.read( Buffer, length );
	string BufferString = string( Buffer, IS.gcount( ) );
	IS.close( );
	delete [] Buffer;
	return BufferString;
}

string UTIL_FileRead( string file )
{
	std::ifstream IS;
	IS.open( file.c_str( ), std::ios::binary );

	if( IS.fail( ) )
	{
		//logger.info( "[UTIL] warning - unable to read file [" + file + "]" );
		return string( );
	}

	// get length of file

	IS.seekg( 0, std::ios::end );
	uint32_t FileLength = IS.tellg( );
	IS.seekg( 0, std::ios::beg );

	// read data

	char *Buffer = new char[FileLength];
	IS.read( Buffer, FileLength );
	string BufferString = string( Buffer, IS.gcount( ) );
	IS.close( );
	delete [] Buffer;

	if( BufferString.size( ) == FileLength )
		return BufferString;
	else
		return string( );
}

bool UTIL_FileWrite( string file, unsigned char *data, uint32_t length )
{
	std::ofstream OS;
	OS.open( file.c_str( ), std::ios::binary );

	if( OS.fail( ) )
	{
		//logger.info( "[UTIL] warning - unable to write file [" + file + "]" );
		return false;
	}

	// write data

	OS.write( (const char *)data, length );
	OS.close( );
	return true;
}

string UTIL_FileSafeName( string fileName )
{
	string :: size_type BadStart = fileName.find_first_of( "\\/:*?<>|" );

	while( BadStart != string :: npos )
	{
		fileName.replace( BadStart, 1, 1, '_' );
		BadStart = fileName.find_first_of( "\\/:*?<>|" );
	}

	return fileName;
}

string UTIL_AddPathSeperator( string path )
{
	if( path.empty( ) )
		return string( );

#ifdef WIN32
	char Seperator = '\\';
#else
	char Seperator = '/';
#endif

	if( *(path.end( ) - 1) == Seperator )
		return path;
	else
		return path + string( 1, Seperator );
}

BYTEARRAY UTIL_EncodeStatString( BYTEARRAY &data )
{
	unsigned char Mask = 1;
	BYTEARRAY Result;

	for( unsigned int i = 0; i < data.size( ); i++ )
	{
		if( ( data[i] % 2 ) == 0 )
			Result.push_back( data[i] + 1 );
		else
		{
			Result.push_back( data[i] );
			Mask |= 1 << ( ( i % 7 ) + 1 );
		}

		if( i % 7 == 6 || i == data.size( ) - 1 )
		{
			Result.insert( Result.end( ) - 1 - ( i % 7 ), Mask );
			Mask = 1;
		}
	}

	return Result;
}

BYTEARRAY UTIL_DecodeStatString( BYTEARRAY &data )
{
	unsigned char Mask;
	BYTEARRAY Result;

	for( unsigned int i = 0; i < data.size( ); i++ )
	{
		if( ( i % 8 ) == 0 )
			Mask = data[i];
		else
		{
			if( ( Mask & ( 1 << ( i % 8 ) ) ) == 0 )
				Result.push_back( data[i] - 1 );
			else
				Result.push_back( data[i] );
		}
	}

	return Result;
}

string UTIL_filenameFromPath(string mappath) {
	boost::filesystem::path p(mappath);
	return p.filename().string();
}

bool UTIL_findFile(const boost::filesystem::path & dir_path, const std::string & file_name, boost::filesystem::path & path_found) {
	if (!exists(dir_path)) return false;
	boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
	for (boost::filesystem::directory_iterator itr(dir_path); itr != end_itr; ++itr) {
		if (is_directory(itr->status())) {
			if (UTIL_findFile(itr->path(), file_name, path_found)) return true;
		}
		else if (itr->path().filename().string() == file_name) {
			path_found = itr->path();
			return true;
		}
	}
	return false;
}

vector<int> UTIL_vectorFill(int a[], int b) {
	vector<int> result;
	for (int i = 0; i < b; i++) {
		result.push_back(a[i]);
	}
	return result;
}


uint32_t GetTime() {
	return timeGetTime()/1000;
}

void RunSound(string sound, bool enabled) {
	if (enabled)
	{
		sound = "sounds\\" + sound + ".wav";
		PlaySound(sound.c_str(), NULL, SND_ASYNC);
	}
}

string UTIL_Encode64(string in) {
	std::vector<uint8_t> data(in.begin(), in.end());
	return Botan::base64_encode(data);
}

string UTIL_Decode64(string in) {
	try {
		auto data_secure = Botan::base64_decode(in);
		std::vector<uint8_t> data(data_secure.begin(), data_secure.end());
		return string(data.begin(), data.end());
	}
	catch (...) {
		return "";
	}
}

vector<string> UTIL_explode(string const & s, char delim) {
	vector<string> result;
	std::stringstream iss(s);

	for (string token; getline(iss, token, delim); )
	{
		result.push_back(move(token));
	}

	return result;
}

W3Version * UTIL_toW3Version(string s) {
	vector<string> w3versionExploded = UTIL_explode(s, '.');

	if (w3versionExploded.size() != 4) {
		return nullptr;
	}

	uint16_t ma = UTIL_ToUInt16(w3versionExploded.at(0));
	uint16_t mi = UTIL_ToUInt16(w3versionExploded.at(1));
	uint16_t pa = UTIL_ToUInt16(w3versionExploded.at(2));
	uint16_t bu = UTIL_ToUInt16(w3versionExploded.at(3));

	W3Version * w3v = new W3Version(ma, mi, pa, bu);
	return w3v;
}

string UTIL_getLastErrorAsString()
{
	//Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string(); //No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	std::string message(messageBuffer, size);

	//Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

string UTIL_replace(string subject, const string& search, const string& replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != string::npos) {
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
}

bool UTIL_startsWith(string hay, string prefix) {
	if (hay.find(prefix) == 0)
		return true;
	else
		return false;
}

void UTIL_ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
	}));
}

void UTIL_rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), s.end());
}

void UTIL_trim(std::string &s) {
	UTIL_ltrim(s);
	UTIL_rtrim(s);
}