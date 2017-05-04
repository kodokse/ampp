
#if _MSC_VER == 1910
#define AMPP_TOOLSET "141"
#elif _MSC_VER == 1900
#define AMPP_TOOLSET "140"
#elif _MSC_VER == 1800
#define AMPP_TOOLSET "120"
#elif _MSC_VER == 1700
#define AMPP_TOOLSET "110"
#elif _MSC_VER == 1600
#define AMPP_TOOLSET "100"
#else
#error Unknown toolset
#endif


#ifdef _DEBUG
#define AMPP_BUILD_TYPE "Debug"
#else
#define AMPP_BUILD_TYPE "Release"
#endif

#ifdef _DLL
#define AMPP_CRT_TYPE
#else
#define AMPP_CRT_TYPE "-Static"
#endif

#ifdef _WIN64
#define AMPP_ARCHITECTURE "x64"
#else
#define AMPP_ARCHITECTURE "x86"
#endif

#define AMPP_CONFIGURATION AMPP_BUILD_TYPE AMPP_CRT_TYPE

#define AMPP_LIB AMPP_BASE_NAME "-" AMPP_TOOLSET "-" AMPP_CONFIGURATION "-" AMPP_ARCHITECTURE

#pragma message("Autolinking " AMPP_LIB ".lib")
#pragma comment(lib, AMPP_LIB)

#undef AMPP_LIB
#undef AMPP_CONFIGURATION
#undef AMPP_ARCHITECTURE
#undef AMPP_CRT_TYPE
#undef AMPP_BUILD_TYPE
#undef AMPP_TOOLSET
