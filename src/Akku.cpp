//
// Created by Nick on 25.02.2024.
//

#include "Akku.h"

Akku::Akku(int mosfet, int reading)  {
    mosfetAddress = mosfet;
    readingAddress = reading;
}
