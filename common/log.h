/*

   Copyright 2015 cen

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

#include <fstream>
#include <iostream>
#include <string>

/**
 * Logging namespace
 */
namespace Logging {
/**
 * Each subsystem instantiates it's own logger. Subsystem enum tells which subsystem it is.
 */
enum Subsystem {
	SYSTEM,
	GPROXY,
	AMH,
	CMDBUFFER,
	WC3,
	SYSTEMID,
	GAMEPROTO,
	PF,
	BNET
};
/**
 * Type enum tells what the line content actually is.
 */
enum Type {
	LOG_ERROR,
	LOG_INFO,
	LOG_DEBUG,
	LOG_TELEMETRY,
	BNET_CHAT,
	BNET_WHISPER,
	BNET_ERROR,
	BNET_INFO,
	BNET_MUTED
};
}

using namespace std;

class Log {

  private:
	/**
	 * Each logger belongs to a subsystem. Each subsystem has it's own logger instance.
	 */
	Logging::Subsystem subsystem;

	/**
	 * Filename to write to.
	 */
	string logFileName;

	/**
	 * Log file output stream, opened in constructor.
	 */
	ofstream logFile;

	/**
	 * Enables LOG_DEBUG type logs.
	 */
	bool debugEnable;

	/**
	 * Enables LOG_TELEMETRY type logs.
	 */
	bool telemetryEnable;

  public:
	/**
	 * Constructor.
	 *
	 * @param logFileName name of the log file on the filesystem
	 * @param subsystem type of the subsystem that is instantiating the logger
	 * @param debugEnable enables LOG_DEBUG level of logging
	 * @param telemetryEnable enables LOG_TELEMETRY level of logging
	 */
	Log(string logFileName, Logging::Subsystem subsystem, bool debugEnable, bool telemetryEnable);

	/**
	 * Destructor.
	 */
	~Log();

	/**
	 * Writes a log with type LOG_ERROR.
	 * @param s string to write
	 */
	void error(string s);

	/**
	 * Writes a log with type LOG_INFO.
	 * @param s string to write
	 */
	void info(string s);

	/**
	 * Writes a log with type LOG_DEBUG.
	 * @param s string to write
	 */
	void debug(string s);

	/**
	 * Writes a log with type LOG_TELEMETRY.
	 * @param s string to write
	 */
	void telemetry(string s);

	/**
	 * Writes a log with type BNET_CHAT.
	 * @param s string to write
	 */
	void chat(string s);

	/**
	 * Writes a log with type BNET_WHISPER.
	 * @param s string to write
	 */
	void whisper(string s);

	/**
	 * Writes a log with type BNET_ERROR.
	 * @param s string to write
	 */
	void chatError(string s);

	/**
	 * Writes a log with type BNET_INFO.
	 * @param s string to write
	 */
	void chatInfo(string s);

	/**
	 * Writes a log with type BNET_MUTED.
	 * @param s string to write
	 */
	void chatMuted(string s);

	/**
	 * Puts the text into the final output format
	 * @param s text to write
	 * @param t Logging::Type enum
	 * @returns formatted final string
	 */
	string format(string s, Logging::Type t);

	/**
	 * Converts Logging::Subsystem enum to string.
	 * @param s Logging::Subsystem enum to convert
	 * @returns string representation of the enum
	 */
	static string subsystemToString(Logging::Subsystem s);

	/**
	 * Converts Logging::Type enum to string.
	 * @param s Logging::Type enum to convert
	 * @returns string representation of the enum
	 */
	static string typeToString(Logging::Type t);

	/**
	 * Writes a log line to the log file.
	 * @param s text to write
	 * @param t log type enum
	 */
	void logPrint(string s, Logging::Type t);

	/**
	 * Writes a log line to the console.
	 * @param s text to write
	 * @param t log type enum
	 */
	void consolePrint(string s, Logging::Type t);

	static void console(string s);

	/**
	 * subsystem getter
	 * @returns subsystem enum
	 */
	Logging::Subsystem getSubsystem();

	/**
	 * filename getter
	 * @returns log filename
	 */
	string getLogFileName();

	/**
	 * debugEnable getter
	 * @returns debugEnable
	 */
	bool getDebugEnable();

	/**
	 * telemetryEnable getter
	 * @returns telemetryEnable
	 */
	bool getTelemetryEnable();

	/**
	 * subsystem setter
	 * @param s Logging::Subsystem enum
	 */
	void setSubsystem(Logging::Subsystem s);

	/**
	 * log filename setter
	 * @param s name of the log file
	 */
	void setLogFileName(string s);

	/**
	 * debugEnable setter
	 * @param s bool
	 */
	void setDebugEnable(bool b);

	/**
	 * telemetryEnable setter
	 * @param s bool
	 */
	void setTelemetryEnable(bool b);
};