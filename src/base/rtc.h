#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

RTC_DS3231 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.pool.ntp.org", 7 * 3600);

class RTC_Base {
    public:
        RTC_Base() {
            Wire.begin();
            rtc.begin();
            timeClient.begin();
        }

        void set(DateTime dt) {
            rtc.adjust(dt);
        }

        DateTime get() {
            return rtc.now();
        }

        void sync() {
            timeClient.update();
            rtc.adjust(timeClient.getEpochTime());
        }

        bool OSF() {
            return rtc.lostPower();
        }
};

class RTC {
    private:
        RTC_Base _RTC = RTC_Base();
    public:
        RTC() {}

        DateTime get() {
            return _RTC.get();
        }

        int getTimeStamp() {
            return _RTC.get().unixtime();
        }

        void set(int year = -1, int month = -1, int day = -1, int hour = -1, int minute = -1, int second = -1) {
            DateTime _dt = _RTC.get();
            int dt[6] = {year, month, day, hour, minute, second};
            if (year == -1) dt[0] = _dt.year();
            if (month == -1) dt[1] = _dt.month();
            if (day == -1) dt[2] = _dt.day();
            if (hour == -1) dt[3] = _dt.hour();
            if (minute == -1) dt[4] = _dt.minute();
            if (second == -1) dt[5] = _dt.second();
            _RTC.set(DateTime(dt[0], dt[1], dt[2], dt[3], dt[4], dt[5]));
        }
};