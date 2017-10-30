//
// Created by Tim on 30/10/2017.
//

#ifndef VULKANITE_KEYBOARDINPUT_H
#define VULKANITE_KEYBOARDINPUT_H

#include <map>

class KeyboardInput
{
public:
	std::map<int, int> keyList;

	bool isKeyPressed(int keyCode);
};

#endif //VULKANITE_KEYBOARDINPUT_H
