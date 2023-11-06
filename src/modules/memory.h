#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "etc/defaultcfg.h"

const int EEPROM_SIZE = 4096;
const int EEPROM_CONFIG_ADDR = 0;

char* EEPROM_read(int addr, int length) {
    char* buffer = new char[length];
    for (int i = 0; i < length; i++) {
        buffer[i] = EEPROM.read(addr + i);
    }
    return buffer;
}

int EEPROM_write(int addr, char* txt) {
    int length = strlen(txt);
    for (int i = 0; i < length; i++) {
        EEPROM.write(addr + i, txt[i]);
    }
    EEPROM.commit();
    return length;
}

void EEPROM_clear() {
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}

class MemoryBase {
    private:
        DynamicJsonDocument config = DynamicJsonDocument(EEPROM_SIZE);
        DynamicJsonDocument defaultcfg = DynamicJsonDocument(EEPROM_SIZE);
    public:
        MemoryBase() {
            deserializeJson(defaultcfg, DEFAULT_CONFIG_JSON);
            EEPROM.begin(EEPROM_SIZE);
            load();
        }

        void load() {
            char* buffer = EEPROM_read(EEPROM_CONFIG_ADDR, EEPROM_SIZE);
            deserializeJson(config, buffer);
            delete[] buffer;
        }

        void save() {
            char* buffer = new char[EEPROM_SIZE];
            serializeJson(config, buffer);
            EEPROM_write(EEPROM_CONFIG_ADDR, buffer);
            delete[] buffer;
        }

        bool hasKey(char* key) {
            return config.containsKey(key);
        }

        int getInt(char* key) {
            if (!hasKey(key)) {
                return defaultcfg[key];
            }
            return config[key];
        }

        char* getString(char* key) {
            if (!hasKey(key)) {
                return defaultcfg[key];
            }
            return config[key];
        }

        int* getIntArray(char* key) {
            if (!hasKey(key)) {
                return defaultcfg[key];
            }
            return config[key];
        }

        bool getBool(char* key) {
            if (!hasKey(key)) {
                return defaultcfg[key];
            }
            return config[key];
        }

        void setInt(char* key, int value) {
            config[key] = value;
        }

        void setString(char* key, char* value) {
            config[key] = value;
        }

        void setIntArray(char* key, int* value) {
            config[key] = value;
        }

        void setBool(char* key, bool value) {
            config[key] = value;
        }
};