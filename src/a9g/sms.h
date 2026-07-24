#pragma once

bool smsInit();
void smsModemSleep();
void smsModemWake();
bool smsSend(const char* number, const char* message);
