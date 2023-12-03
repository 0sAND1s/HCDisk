#pragma once

#include <string>

class GUIUtil
{
public:
	static bool GUIUtil::Confirm(char* msg);
	static void GUIUtil::PrintIntense(char* str);
	static void GUIUtil::TextViewer(std::string str);
};

