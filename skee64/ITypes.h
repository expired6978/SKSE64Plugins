#pragma once

#pragma warning(disable: 4018 4200 4244 4267 4305 4288 4312 4311)

// need win8 for windows store APIs
#define _WIN32_WINNT	0x0602

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <stddef.h>
#include "common/ITypes.h"
#include "common/IErrors.h"
#include "common/IDynamicCreate.h"
#include "common/ISingleton.h"
#include <winsock2.h>
#include <Windows.h>

#include "ILogger.h"
