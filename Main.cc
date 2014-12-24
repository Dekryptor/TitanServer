// TitanServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "AuthServer.h"

#define XBLHammer_PORT 4230
#define xblatlas_PORT 6977


int _tmain(int argc, _TCHAR* argv[])
{
	if (AuthServerStartup(XBLHammer_PORT)) {
		AuthServerListen();
	}

	return 0;
}

