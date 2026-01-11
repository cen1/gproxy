#pragma once

#include <iostream>
#include <stdint.h>

using namespace std;

class Timer {

  private:
	uint32_t startTime;
	uint32_t diffTime;

  public:
	Timer();
	~Timer();

	static uint32_t getTimeUnix();
	static uint32_t getTimeS();
	static uint32_t getTimeMS();
	static string getTimeString();

	uint32_t start();
	uint32_t diff();
	bool passed(uint32_t d);

	// getters
	uint32_t getStartTime();
	uint32_t getDiffTime();

	// setters
	void setStartTime();
	void setDiffTime();
};