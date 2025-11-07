#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SSD1306.h"
#include <avr/wdt.h>
#include "Akku.h"

using namespace std;

#define interruptPin PIN_PA3

Adafruit_SSD1306 display(128, 64, &Wire, -1);
boolean hasDisplay = true;
boolean shouldRun = false;
double teiler = 5000.0f / (220.0f + 5000.0f);
double resistor = 2.7f;

volatile boolean changedRunning = false;

long debouncing_time = 100;
volatile unsigned long last_millis;
float start = 0;

Akku *akkus[8] = {
        new Akku(PIN_PC5, PIN_PA4),
        new Akku(PIN_PC4, PIN_PA5),
        new Akku(PIN_PC3, PIN_PA6),
        new Akku(PIN_PC2, PIN_PA7),
        new Akku(PIN_PC1, PIN_PB7),
        new Akku(PIN_PC0, PIN_PB6),
        new Akku(PIN_PB0, PIN_PB5),
        new Akku(PIN_PB1, PIN_PB4)
};

volatile int selectedAkku = 0;
volatile int pressed = 0;

double getVolt(int pin) {
    volatile float analog_value;
    if (pin == PIN_PB7 || pin == PIN_PB6) {
        analog_value = analogRead1(pin);
    } else {
        analog_value = analogRead(pin);
    }
    return ((analog_value * 5.0f) / 1023.0f) / teiler;
}

void handleInterrupt() {
    if (millis() - last_millis >= debouncing_time) {
        if (digitalRead(interruptPin) == 0) {
            pressed += 1;
            last_millis = millis();
        }
    }
}

void resetAll() {
    for (Akku *single: akkus) {
        single->reset();
        digitalWrite(single->getMosfetAddress(), LOW);
        volatile double volt = getVolt(single->getReadingAddress());
        single->isRunning = volt > 2.8;
        single->internalRes = 0.0;
        if (single->isRunning) {
            digitalWrite(single->getMosfetAddress(), HIGH);
            volatile double voltLast = getVolt(single->getReadingAddress());
            single->internalRes = (volt - voltLast) / resistor;
        }
    }
    start = millis();
}

void buttonPressedFirstStage() {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("holding");
    display.display();

    volatile long start = millis();
    volatile long runner = millis();
    while (digitalRead(interruptPin) == 0 && runner - start < 1500) {
        runner = millis();
    }
    long end = runner - start;

    if (end > 1400) {
        pressed = 2;
    } else {
        if (selectedAkku < 9) {
            selectedAkku++;
        } else {
            selectedAkku = 0;
        }
        pressed = 0;
    }
}

void buttonPressedSecondStage() {
    if (pressed == 2) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("Knopf druecken:");
        display.println("Start/Stop: kurz");
        display.println("Reset: lange");
        display.display();
    }

    if (pressed == 3) {
        volatile long start = millis();
        volatile long runner = millis();
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("rein");
        display.display();

        while (digitalRead(interruptPin) == 0 && runner - start < 1500) {
            runner = millis();
            display.clearDisplay();
            display.setCursor(0, 0);
            display.println("holding");
            display.display();
        }
        long end = runner - start;

        if (end > 1400) {
            resetAll();
        } else {
            if (!shouldRun) {
                changedRunning = true;
            }
            shouldRun = !shouldRun;
        }
        pressed = 0;
    }
}

void setup() {
    //PORT switch SDA und SCL auf alternative PINS
    PORTMUX.CTRLB |= PORTMUX_TWI0_ALTERNATE_gc;

    init_ADC0();
    init_ADC1();

    //Interrupt Taster
    pinMode(interruptPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);

    for (Akku *single: akkus) {
        pinMode(single->getMosfetAddress(), OUTPUT);
        pinMode(single->getReadingAddress(), INPUT);
        digitalWrite(single->getMosfetAddress(), LOW);
    }

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        delay(500);
        if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
            Serial.println("Kein Display angeschlossen");
            hasDisplay = false;
        }
    }

    resetAll();

    if (hasDisplay) {
        display.setTextSize(0);
        display.setTextColor(WHITE);
        display.display();
        display.clearDisplay();
    }
    start = millis();
}

void loop() {
    if (pressed == 1) {
        buttonPressedFirstStage();
    }
    if (pressed == 2) {
        buttonPressedSecondStage();
    }
    if (shouldRun && changedRunning) {
        changedRunning = false;
        for (Akku *single: akkus) {
            single->lastMeasure = millis();
        }
    }
    if (pressed == 0) {
        for (Akku *single: akkus) {
            volatile double volt = getVolt(single->getReadingAddress());
            single->volt = volt;
            if (single->volt <= 2.8 && single->isRunning) {
                single->isRunning = false;
            } else if (single->isRunning && shouldRun) {
                volatile unsigned long ellapsedTime = millis() - single->lastMeasure;
                if (ellapsedTime >= 1000 && shouldRun) {
                    volatile float timeMulti = (ellapsedTime / 1000.0f);
                    single->elapsedTime += ellapsedTime;
                    single->capacity += (volt / resistor) * timeMulti;
                    single->lastMeasure = millis();
                }
            }

            if (!single->isRunning || !shouldRun) {
                digitalWrite(single->getMosfetAddress(), LOW);
            }
            if (single->isRunning && shouldRun) {
                digitalWrite(single->getMosfetAddress(), HIGH);
            }
        }

        display.clearDisplay();
        display.setCursor(0, 0);
        if (selectedAkku == 0) {
            for (Akku *akku: akkus) {
                display.print("Volt:");
                display.println(String(akku->volt, 3));
            }
        }
        if (selectedAkku == 9) {
            display.println("Resistor: " + String(resistor));
            display.println("Teiler: " + String(teiler));
        }
        if (selectedAkku != 9 && selectedAkku != 0) {
            display.println(selectedAkku);
            display.println("Volt: " + String(akkus[selectedAkku - 1]->volt, 3));
            display.println("internal: " + String(akkus[selectedAkku - 1]->internalRes, 5));
            display.print("Running: ");
            display.println(akkus[selectedAkku - 1]->isRunning ? "Ja" : "Nein");
            display.println("mAh: " + String(akkus[selectedAkku - 1]->capacity / 3.6));
            display.println("time: " + String(akkus[selectedAkku - 1]->elapsedTime / 60000.0f));
        }

        display.display();
    }
}