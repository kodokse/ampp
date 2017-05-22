// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

#define _NO_CVCONST_H

#include <SDKDDKVer.h>
//#undef WIN32_NO_STATUS
//#include <WinBase.h>
#include <ntstatus.h>

#define WIN32_NO_STATUS
//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <Windows.h>

#include <vector>
#include <map>
#include <set>
#include <list>
#include <string>
#include <cstdint>
#include <codecvt>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <functional>
#include <thread>
#include <mutex>

#ifndef AMPP_INTERNAL_BUILD
#include <ampp/config/autolink.h>
#endif

namespace fs = std::tr2::sys;
