/*
	Copyright (C) 2003-2005 Daniel Muller, dan at verliba dot cz
	Copyright (C) 2006-2017 Verlihub Team, info at verlihub dot net

	Verlihub is free software; You can redistribute it
	and modify it under the terms of the GNU General
	Public License as published by the Free Software
	Foundation, either version 3 of the license, or at
	your option any later version.

	Verlihub is distributed in the hope that it will be
	useful, but without any warranty, without even the
	implied warranty of merchantability or fitness for
	a particular purpose. See the GNU General Public
	License for more details.

	Please see http://www.gnu.org/licenses/ for a copy
	of the GNU General Public License.
*/

#include "stdafx.h"
#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#define BUFSIZE MAX_PATH
#include <local.h>
#endif
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "cserverdc.h"
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <signal.h>
#include <dirent.h>
#include "script_api.h"
#include "i18n.h"
#include "dirsettings.h"
#include <getopt.h>
#include <syslog.h>

using namespace std;
using namespace nVerliHub;
using namespace nVerliHub::nSocket;

cObj mainLogger("main");
#define MAIN_LOG_NOTICE if (mainLogger.Log(0)) mainLogger.LogStream()
#define MAIN_LOG_ERROR if (mainLogger.ErrLog(0)) mainLogger.LogStream()

#if ! defined _WIN32
void mySigPipeHandler(int i)
{
	signal(SIGPIPE, mySigPipeHandler);
	MAIN_LOG_NOTICE << "Received SIGPIPE, ignoring it, " << i << endl;
}

void mySigIOHandler(int i)
{
	signal(SIGIO, mySigIOHandler);
	MAIN_LOG_NOTICE << endl << "Received SIGIO, ignoring it, " << i << endl;
}

void mySigQuitHandler(int i)
{
	MAIN_LOG_NOTICE << "Received a " << i << " signal, quiting" << endl;
	cServerDC *serv = (cServerDC*)cServerDC::sCurrentServer;

	if (serv)
		serv->OnUnLoad(0);

	exit(0);
}

void mySigServHandler(int i)
{
	MAIN_LOG_ERROR << "Received a " << i << " signal, doing stacktrace and quiting" << endl;
	cServerDC *serv = (cServerDC*)cServerDC::sCurrentServer;

	if (serv) {
		serv->OnUnLoad(9);
		serv->DoStackTrace();
	}

	exit(128 + i); // proper exit code for this signal
}

void mySigHupHandler(int i)
{
	MAIN_LOG_NOTICE << "Received a " << i << " signal, reloading" << endl;
	cServerDC *serv = (cServerDC*)cServerDC::sCurrentServer;

	if (serv)
		serv->Reload();
}

#endif

bool DirExists(const char *dirname)
{
	DIR *dir ;
	dir = opendir(dirname);
	if( dir == NULL)
	{
		return false;
	}
	else
	{
		closedir(dir);
		return true;
	}
}

int main(int argc, char *argv[])
{
	int result = 0;
	string ConfigBase;
	int port = 0;
	int verbosity = 0;

	openlog("verlihub-debug", LOG_PID, LOG_USER);
	syslog(LOG_INFO,"Verlihub +1");
	const char* short_options = "Ss:d:v";

	const struct option long_options[] = {
		{"syslog",          no_argument,        0, 'S'},
		{"syslog-suffix",   required_argument,  0, 's'},
		{"config-dir",      required_argument,  0, 'd'},
		{"verbose",         no_argument,        0, 'v'},
		{NULL, 0, NULL, 0}
	};

	int long_index = -1;
	int opt = -1;
	char *OptDirName = NULL;

	while( (opt = getopt_long(argc, argv, short_options, long_options, &long_index)) != -1 ) {
		switch( opt ) {
			case 'S':
#ifdef ENABLE_SYSLOG
				cObj::msUseSyslog = 1;
#else
				MAIN_LOG_NOTICE << "Logging to syslog is disabled at compile time" << endl;
#endif
				break;
			case 's':
#ifdef ENABLE_SYSLOG
				cObj::msSyslogIdent += "-";
				cObj::msSyslogIdent += optarg;
#else
				MAIN_LOG_NOTICE << "Logging to syslog is disabled at compile time" << endl;
#endif
				break;
			case 'd':
				OptDirName = optarg;
				break;
			case 'v':
				verbosity++;
				break;

			default:
				/* unreachable */
				break;
		}
	}
	if (optind < argc) {
		stringstream arg(argv[optind]);
		arg >> port;
	}

	#ifdef _WIN32
		TCHAR Buffer[BUFSIZE];

		if (!GetCurrentDirectory(BUFSIZE, Buffer)) {
			MAIN_LOG_ERROR << "Unable to get current directory because: " << GetLastError() << endl;
			closelog();
			return 1;
		}

		ConfigBase = Buffer;
	#else
		const char *DirName = NULL;
		char *HomeDir = getenv("HOME");
		string tmp;

		if (HomeDir) {
			tmp = HomeDir;
			tmp += "/.config/verlihub";
			if (DirExists(tmp.c_str()))
				ConfigBase = tmp;
			else {
				tmp = HomeDir;
				tmp += "/.verlihub";
				if (DirExists(tmp.c_str()))
					ConfigBase = tmp;
			}
		}

		DirName = "./.verlihub";

		if (DirExists(DirName))
			ConfigBase = DirName;

		DirName = getenv("VERLIHUB_CFG");

		if ((OptDirName != NULL) && DirExists(OptDirName))
			ConfigBase = OptDirName;
		else if ((DirName != NULL) && DirExists(DirName))
			ConfigBase = DirName;

		if (!ConfigBase.size())
			ConfigBase = "/etc/verlihub";
	#endif

	MAIN_LOG_NOTICE << "Configuration directory: " << ConfigBase << endl;
	try
	{
		cServerDC server(ConfigBase, argv[0]); // create server
		cObj::msLogLevel += verbosity;

		#ifndef _WIN32
			signal(SIGPIPE, mySigPipeHandler);
			signal(SIGIO,   mySigIOHandler);
			signal(SIGQUIT, mySigQuitHandler);
			signal(SIGSEGV, mySigServHandler);
			signal(SIGHUP,  mySigHupHandler);
		#endif

		server.StartListening(port);
		result = server.run(); // run the main loop until it stops itself
		syslog(LOG_INFO,"verlihub -1");
		closelog();
		return result;
	}
	catch (const char *exception)
	{
		MAIN_LOG_ERROR << exception << endl;
		closelog();
		return 3;
	}
}
