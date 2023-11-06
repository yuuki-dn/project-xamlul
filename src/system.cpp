#include <Arduino.h>
#include "base/memory.h"
#include "base/rtc.h"

class System {
    public:
    System() {}


};

class Main {
    private:
        System _system;
    public:
        Main(System system) {
            _system = system;
        }
};