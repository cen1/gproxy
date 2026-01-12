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

#include "config.h"
#include "util.h"

#include <algorithm>
#include <stdlib.h>
#include <fstream>
#include <iostream>

void CConfig :: Read( string file )
{
	std::ifstream in;
	in.open( file.c_str( ) );

	if (in.fail())
		std::cout << "[CONFIG] warning - unable to read file [" + file + "]" << std::endl;
	else
	{
		std::cout << "[CONFIG] loading file [" + file + "]" << std::endl;
		string Line;

		while( !in.eof( ) )
		{
			getline( in, Line );
			std::cout << Line << std::endl;

			// ignore blank lines and comments
			if( Line.empty() || Line[0] == '#' || Line[0] == '[' || Line[0] == '=' || Line.length()<3)
				continue;

			// remove newlines and partial newlines to help fix issues with Windows formatted config files on Linux systems

			Line.erase( std::remove( Line.begin( ), Line.end( ), '\r' ), Line.end( ) );
			Line.erase( std::remove( Line.begin( ), Line.end( ), '\n' ), Line.end( ) );

			string :: size_type Split = Line.find( "=" );

			if( Split == string :: npos )
				continue;

			string key = Line.substr(0, Split);
			string value = Line.substr(Split+1, Line.length());

			UTIL_trim(key);
			UTIL_ltrim(value);

			m_CFG[key] = value;
		}

		in.close( );
	}
}

bool CConfig :: Exists( string key )
{
	return m_CFG.find( key ) != m_CFG.end( );
}

int CConfig :: GetInt( string key, int x )
{
	if( m_CFG.find( key ) == m_CFG.end( ) )
		return x;
	else
		return atoi( m_CFG[key].c_str( ) );
}

bool CConfig::GetBool(string key, bool x) {
	int xx = (x) ? 1 : 0;
	if (GetInt(key, xx) == 1) {
		return true;
	}
	else {
		return false;
	}
}

string CConfig :: GetString( string key, string x )
{
	if( m_CFG.find( key ) == m_CFG.end( ) )
		return x;
	else
		return m_CFG[key];
}

void CConfig :: Set( string key, string x )
{
	m_CFG[key] = x;
}

//file scope define
std::map<std::string, std::string> CConfig::m_CFG;
