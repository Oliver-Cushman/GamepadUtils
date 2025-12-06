#include "../include/gamepad/Gamepad.h"

#include <iostream>

int main()
{
    Gamepad gamepad = Gamepad("/dev/input/js0");
    std::array<std::string, 6> buttonNames{"A", "B", "X", "Y", "LB", "RB"};
    while (!(gamepad.getButton(0) && gamepad.getButton(3)))
    {
        gamepad.refresh();
        std::cout << "\r";
        for (int i = 0; i < buttonNames.size(); i++) {
            std::cout << buttonNames[i] + ": " << gamepad.getButton(i) << " | ";
        }
    }
}