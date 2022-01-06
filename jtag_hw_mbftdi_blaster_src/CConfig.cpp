#ifdef _WINDOWS
#include <windows.h>
#else
#endif
#include "CConfig.h"

CConfig g_cfg;

CConfig::CConfig()
{
	char curdir[1024*4];
	curdir[0]=0;
#ifdef _WINDOWS
	GetModuleFileNameA(
		GetModuleHandle(NULL),
		curdir,
		sizeof(curdir)
	);
	size_t len = strlen(curdir);
	while (len)
	{
		if(curdir[len-1]=='\\')
		{
			curdir[len] = 0;
			break;
		}
		len--;
	}
#endif
	printd("Current directory: %s",curdir);
	string path = string(curdir) + "jconfig.txt";
	try {
		ifstream t;
		t.open(path);
		if (!t.is_open()) {
			printd("No config file");
			return;
		}
		printd("Config file opened");
		string s;
		int n = 0;
		while (getline(t, s))
		{
			size_t pos = s.find("=");
			if (pos != string::npos)
			{
				//found assignment, split line into param/value
				string param = s.substr(0, pos);
				string val = s.substr(pos + 1);
				printd("Config param: %s %s",param.c_str(),val.c_str());
				if (param.length() && val.length())
				{
					par_val_[param] = val;
				}
			}
			n++;
			if (n == MAX_PARAM_NUM)
				break; //too many params
		}
	}
	catch (...) {
	}
	printd("Config has %d params",par_val_.size());
}

CConfig::~CConfig()
{
}

list<string> CConfig::getIpServers()
{
	list<string> L;
	for (int i = 0; i < MAX_PARAM_NUM; i++)
	{
		string param = string("ipaddr") + to_string(i);
		printd("IpAddr from config search %s", param.c_str());
		string val = par_val_[param];
		if (val.length())
		{
			L.push_back(val);
			printd("IpAddr from config added %s", val.c_str());
		}
		else
			break;
	}
	return L;
}
