#include "timer.h"
#include "boost/date_time.hpp"
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
#include <windows.h>
#include <ctime>

Timer::Timer() {
	setStartTime();
	diffTime = 0;
}

Timer::~Timer() {

}

uint32_t Timer::getTimeUnix() {
	return (uint32_t)time(NULL);
}

uint32_t Timer::getTimeS() {
	return timeGetTime( )/1000;
}

uint32_t Timer::getTimeMS() {
	return timeGetTime( );
}

string Timer::getTimeString() {
	char format[255];
	time_t now = time(NULL);
	tm * ptm = std::localtime(&now);
	strftime(format, sizeof(format), "%d-%m-%Y %H:%M:%S", ptm);
	string r(format);
	return r;
}


uint32_t Timer::start() {
	setStartTime();
	return getStartTime();
}

uint32_t Timer::diff() {
	setDiffTime();
	return getDiffTime()-getStartTime();
}

//returns true if d ms already elapsed
bool Timer::passed(uint32_t d) {
	setDiffTime();
	uint32_t difference = getDiffTime()-getStartTime();
	if (difference > d) return true;
	else return false;
}

//getters
uint32_t Timer::getStartTime() {
	return startTime;
}

uint32_t Timer::getDiffTime() {
	return diffTime;
}

//setters
void Timer::setStartTime() {
	startTime = getTimeMS();
}
void Timer::setDiffTime() {
	diffTime = getTimeMS();
}

