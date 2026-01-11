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

#ifndef CONFIG_H
#define CONFIG_H

#include <map>
#include <string>

//
// CConfig
//

class CConfig {
  public:
	static std::map<std::string, std::string> m_CFG;

	static void Read(std::string file);
	static bool Exists(std::string key);
	static int GetInt(std::string key, int x);
	static bool GetBool(std::string key, bool x);
	static std::string GetString(std::string key, std::string x);
	static void Set(std::string key, std::string x);
};

#endif
