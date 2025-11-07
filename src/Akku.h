//
// Created by Nick on 25.02.2024.
//

#ifndef MEASURECAPACITYV2_AKKU_H
#define MEASURECAPACITYV2_AKKU_H


class Akku {
private:
    int mosfetAddress, readingAddress;

public:
    Akku(int mosfet, int reading);

    double internalRes, volt, capacity;
    unsigned long lastMeasure, elapsedTime;
    bool isRunning = false;

    [[nodiscard]] int getMosfetAddress() const {
        return mosfetAddress;
    }

    [[nodiscard]] int getReadingAddress() const {
        return readingAddress;
    }

    void reset() {
        internalRes = 0.0;
        volt = 0.0;
        capacity = 0.0;
        lastMeasure = 0;
        elapsedTime = 0;
    }
};
#endif //MEASURECAPACITYV2_AKKU_H
