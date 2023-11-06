#include <Arduino.h>
#include <RTClib.h>

int _init[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

class Schedule {
    public:
        int start[2] = {0, 0};
        int end[2] = {0, 0};
        bool active[8] = {false, false, false, false, false, false, false, false};
        Schedule(int* init = _init) {
            start[0] = init[0];
            start[1] = init[1];
            start[2] = init[2];
            end[0] = init[3];
            end[1] = init[4];
            end[2] = init[5];
            for (int i = 1; i < 8; i++) {
                if (init[i + 5]) {
                    active[i] = true;
                }
            }
        }
};

class Switch {
    private:
        int _pin;
        bool _state;
        bool upcoming = false;
        int upcoming_close[2] = {0, 0};
    public:
        Switch(int pin) {
            _pin = pin;
            pinMode(_pin, INPUT);
        }

        bool getState() {
            return _state;
        }

        void setClose(int hour, int minute) {
            upcoming_close[0] = hour;
            upcoming_close[1] = minute;
            upcoming = true;
        }

        void on() {
            _state = true;
            digitalWrite(_pin, LOW);
        }

        void off() {
            _state = false;
            digitalWrite(_pin, HIGH);
            upcoming = false;
        }

        void toggle() {
            if (_state) {
                off();
            } else {
                on();
            }
        }

        void run(DateTime time, Schedule schedule1, Schedule schedule2) {
            if (upcoming) {
                if (time.hour() == upcoming_close[0] && time.minute() == upcoming_close[1] && time.second() >= 30) {
                    off();
                }
                return;
            }
            if (time.hour() == schedule1.start[0] && time.minute() == schedule1.start[1] && time.second() < 30 && schedule1.active[time.dayOfTheWeek()]) {
                on();
                setClose(schedule1.end[0], schedule1.end[1]);
            }
            else if (time.hour() == schedule2.start[0] && time.minute() == schedule2.start[1] && time.second() < 30 && schedule2.active[time.dayOfTheWeek()]) {
                on();
                setClose(schedule2.end[0], schedule2.end[1]);
            }
            
        }
};