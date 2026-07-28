#pragma once

#include <Arduino.h>

struct SmsCommand {
	String from;
	String body;
	int index;
};

bool modemInit();
bool modemEnsureNetwork();
bool smsSend(const char* number, const String& message);
size_t smsPollUnread(SmsCommand* outCommands, size_t maxCommands);
void smsDeleteIndex(int index);
void modemEnterSleep();
void modemExitSleep();
String modemGetSignalAndPowerStatus();
