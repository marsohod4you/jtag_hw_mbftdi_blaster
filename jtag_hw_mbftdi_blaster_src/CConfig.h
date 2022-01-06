#pragma once

#include <fstream>
#include <string>
#include <list>
#include <map>

using namespace std;

#include "../common/debug.h"

class CConfig
{
public:
	CConfig();
	~CConfig();
	list<string> getIpServers();
private:
	const int MAX_PARAM_NUM = 128;
	map<string, string> par_val_;
};

extern CConfig g_cfg;
