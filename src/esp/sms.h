#pragma once

bool smsInit();
bool smsSend(const char* number, const char* message);
void smsModemSleep();
void smsModemWake();
