#include "log.h"
#include "timer.h"

Log::Log(string logFileName, Logging::Subsystem subsystem, bool debugEnable, bool telemetryEnable) {
	setLogFileName(logFileName);
	setSubsystem(subsystem);
	setDebugEnable(debugEnable);
	setTelemetryEnable(telemetryEnable);

	this->logFile.open(getLogFileName().c_str(), ios::app);

	if (!this->logFile) {
		cout << "Failed to open log file!!!" << subsystem << endl;
	}
}

Log :: ~Log() {
	logFile.close();
}

//gproxy related
void Log::error(string s) {
	logPrint(s, Logging::Type::LOG_ERROR);
}

void Log::info(string s) {
	logPrint(s, Logging::Type::LOG_INFO);
}

void Log::debug(string s) {
	if (debugEnable)
		logPrint(s, Logging::Type::LOG_DEBUG);
}

void Log::telemetry(string s) {
	if (telemetryEnable)
		logPrint(s, Logging::Type::LOG_TELEMETRY);
}

//bnet chat related
void Log::chat(string s) {
	logPrint(s, Logging::Type::BNET_CHAT);
}

void Log::whisper(string s) {
	logPrint(s, Logging::Type::BNET_WHISPER);
}

void Log::chatError(string s) {
	logPrint(s, Logging::Type::BNET_ERROR);
}

void Log::chatInfo(string s) {
	logPrint(s, Logging::Type::BNET_INFO);
}

void Log::chatMuted(string s) {
	logPrint(s, Logging::Type::BNET_MUTED);
}

string Log::format(string s, Logging::Type t) {
	return "[" + Timer::getTimeString() + "]["+ subsystemToString(subsystem)+"]["+typeToString(t)+"] "+s;
}

string Log::subsystemToString(Logging::Subsystem s) {
	switch (s) {
		case Logging::Subsystem::GPROXY:
			return "GPROXY";
		case Logging::Subsystem::SYSTEM:
			return "SYSTEM";
		case Logging::Subsystem::AMH:
			return "AMH";
		case Logging::Subsystem::CMDBUFFER:
			return "CMDBUFFER";
		default:
			return "GPROXY";
	}
}

string Log::typeToString(Logging::Type t) {
	switch (t) {
	case Logging::Type::LOG_ERROR : 
				return "LOG_ERROR";
	case Logging::Type::LOG_INFO :
				return "LOG_INFO";
		case Logging::Type::LOG_DEBUG :
				return "LOG_DEBUG";
		case Logging::Type::LOG_TELEMETRY :
			return "LOG_TELEMETRY";
		case Logging::Type::BNET_CHAT:
			return "BNET_CHAT";
		case Logging::Type::BNET_WHISPER:
			return "BNET_WHISPER";
		case Logging::Type::BNET_ERROR:
			return "BNET_ERROR";
		case Logging::Type::BNET_INFO:
			return "BNET_INFO";
		case Logging::Type::BNET_MUTED:
			return "BNET_MUTED";
		default:
			return "ERROR";
	}
}

//getters
Logging::Subsystem Log::getSubsystem() {
	return subsystem;
}

string Log::getLogFileName() {
	return logFileName;
}

bool Log::getDebugEnable() {
	return debugEnable;
}

bool Log::getTelemetryEnable() {
	return telemetryEnable;
}

//setters
void Log::setSubsystem(Logging::Subsystem s) {
	subsystem = s;
}

void Log::setLogFileName(string s) {
	logFileName = s;
}

void Log::setDebugEnable(bool b) {
	debugEnable = b;
}

void Log::setTelemetryEnable(bool b) {
	telemetryEnable = b;
}

//logging
void Log::logPrint(string s, Logging::Type t) {
	if (logFile.is_open()) {
		logFile << format(s, t) << endl;
	}
	else {
		console("FATAL ERROR: LOG FILE IS CLOSED");
	}
	consolePrint(s, t);
}

void Log::consolePrint(string s, Logging::Type t) {
	cout << format(s, t) << endl;
}

void Log::console(string s) {
	cout << s << endl;
}