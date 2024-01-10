// Copyright Valve Corporation, All rights reserved.

#include "pch_tier0.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#if (defined(__clang__) || defined(__GNUC__)) && (__x86_64__ || __i386__)
#include <cpuid.h>
#endif

#if defined(_WIN32)
#include "winlite.h"
#elif defined(_LINUX)
#include <cstdlib>
#elif defined(OSX)
#include <sys/sysctl.h>
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static bool cpuid(unsigned int function,
	unsigned int& out_eax,
	unsigned int& out_ebx,
	unsigned int& out_ecx,
	unsigned int& out_edx)
{
	int CPUInfo[4] = { -1 };
#if (defined(__clang__) || defined(__GNUC__)) && defined(__cpuid)
	__cpuid(function, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
#else
	__cpuid(CPUInfo, (int)function);
#endif

	out_eax = CPUInfo[0];
	out_ebx = CPUInfo[1];
	out_ecx = CPUInfo[2];
	out_edx = CPUInfo[3];
	
	return true;
}

// Return the Processor's vendor identification string, or "Generic_x86" if it doesn't exist on this CPU
static const char* GetProcessorVendorId()
{
#if defined( _X360 ) || defined( _PS3 )
	return "PPC";
#else
	unsigned int unused, vendorIdRegs[3];
	static char vendorId[13];

	if (vendorId[0] != '\0')
	{
		return vendorId;
	}
	
	memset( vendorId, 0, sizeof(vendorId) );
	if ( !cpuid(0,unused, vendorIdRegs[0], vendorIdRegs[2], vendorIdRegs[1] ) )
	{
		if ( IsPC() )
		{
			strcpy( vendorId, "Generic_x86" );
		}
		else if ( IsX360() )
		{
			strcpy( vendorId, "PowerPC" );
		}
	}
	else
	{
		memcpy( vendorId+0, &(vendorIdRegs[0]), sizeof( vendorIdRegs[0] ) );
		memcpy( vendorId+4, &(vendorIdRegs[1]), sizeof( vendorIdRegs[1] ) ); //-V112
		memcpy( vendorId+8, &(vendorIdRegs[2]), sizeof( vendorIdRegs[2] ) );
	}

	return vendorId;
#endif
}

static bool CheckMMXTechnology( unsigned edx )
{
#if defined( _X360 ) || defined( _PS3 ) 
	return true;
#else
	return ( edx & ( 1U << 23U ) ) != 0;
#endif
}


static bool CheckSSETechnology( unsigned edx )
{
#if defined( _X360 ) || defined( _PS3 )
	return true;
#else
	return ( edx & ( 1U << 25U ) ) != 0;
#endif
}

static bool CheckSSE2Technology( unsigned edx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( edx & ( 1U << 26U ) ) != 0;
#endif
}

static bool CheckSSE3Technology( unsigned ecx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( ecx & 1U ) != 0;
#endif
}

static bool CheckSSSE3Technology( unsigned ecx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSSE 3 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	return ( ecx & ( 1U << 9U ) ) != 0;	// bit 9 of ECX
#endif
}

static bool CheckSSE41Technology( unsigned ecx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4.1 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	return ( ecx & ( 1U << 19U ) ) != 0;	// bit 19 of ECX
#endif
}

static bool CheckSSE42Technology( unsigned ecx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4.2 is implemented by both Intel and AMD
	// detection is done the same way for both vendors
	return ( ecx & ( 1U << 20U ) ) != 0;	// bit 20 of ECX
#endif
}


static bool CheckSSE4aTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// SSE 4a is an AMD-only feature
	const char *vendorId = GetProcessorVendorId();
	if ( 0 != V_tier0_stricmp( vendorId, "AuthenticAMD" ) )
		return false;

	unsigned int eax,ebx,edx,ecx;
	if( !cpuid( 0x80000001U,eax,ebx,ecx,edx) )
		return false;

	return ( ecx & ( 1U << 6U ) ) != 0;	// bit 6 of ECX
#endif
}

static bool Check3DNowTechnology()
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	unsigned int eax, unused;
	if ( !cpuid(0x80000000U,eax,unused,unused,unused) ) //-V112
		return false;

	if ( eax > 0x80000000U ) //-V112
	{
		if ( !cpuid(0x80000001U,unused,unused,unused,eax) )
			return false;

		return ( eax & 1U << 31U ) != 0;
	}

	return false;
#endif
}

static bool CheckCMOVTechnology( unsigned edx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( edx & (1U << 15U) ) != 0;
#endif
}

static bool CheckFCMOVTechnology( unsigned edx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	// Has x87 FPU and CMOV => have FCMOV.
	return ( edx & 1U ) != 0 && CheckCMOVTechnology( edx );
#endif
}

static bool CheckRDTSCTechnology( unsigned edx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( edx & (1U << 4U) ) != 0;
#endif
}

static bool CheckPopcntTechnology( unsigned int ecx )
{
#if defined( _X360 ) || defined( _PS3 )
	return false;
#else
	return ( ecx & ( 1U << 23U ) ) != 0;	// bit 23 of ECX
#endif
}

// Returns the number of logical processors per physical processors.
static uint8 LogicalProcessorsPerPackage( unsigned ebx )
{
#if defined( _X360 )
	return 6;
#else
	// EBX[23:16] indicate number of logical processors per package
	constexpr unsigned NUM_LOGICAL_BITS = 0x00FF0000U;

	return (uint8) ((ebx & NUM_LOGICAL_BITS) >> 16U);
#endif
}

#ifdef _WIN32

struct CpuCoreInfo
{
	unsigned physical_cores;
	unsigned logical_cores;
};

// Helper function to count set bits in the processor mask.
static unsigned CountSetBits( ULONG_PTR mask, bool is_popcnt_supported )
{
#if !(defined(_M_ARM) || defined(_M_ARM64) || defined(_WIN64))
	if ( is_popcnt_supported )
	{
		return __popcnt(mask);
	}
#elif defined(_WIN64)
	if ( is_popcnt_supported )
	{
		// Requires CPU support.
		return static_cast<unsigned>(__popcnt64(mask));
	}
#endif

	constexpr size_t kLeftShift{sizeof(ULONG_PTR) * 8 - 1};
	size_t bit_test{static_cast<size_t>(1U) << kLeftShift};

	unsigned bits_num{0};
	for (size_t i = 0; i <= kLeftShift; ++i)
	{
		bits_num += ((mask & bit_test) ? 1U : 0U);

		bit_test /= 2;
	}

	return bits_num;
}

static CpuCoreInfo GetProcessorCoresInfo( bool is_popcnt_supported )
{
	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buffer{nullptr};
	DWORD size{0};

	while ( true )
	{
		const BOOL rc{ ::GetLogicalProcessorInformation( buffer, &size ) };
		if ( rc ) break;
 
		if ( ::GetLastError() == ERROR_INSUFFICIENT_BUFFER )
		{
			if ( buffer ) ::HeapFree( ::GetProcessHeap(), 0, buffer );

			buffer = static_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(
				::HeapAlloc( ::GetProcessHeap(), 0, static_cast<size_t>( size ) ) );
			if ( !buffer )
			{
				// Allocation failure.
				return { 1, 1 };
			}
		}
		else
		{
			// Error GetLastError()
			return { 1, 1 };
		}
	}

	SYSTEM_LOGICAL_PROCESSOR_INFORMATION *it{buffer};
	size_t offset{0};
	unsigned logical_cores_num{0}, physical_cores_num{0};

	while ( offset + sizeof(*it) <= static_cast<size_t>(size) )
	{
		switch (it->Relationship)
		{
			case RelationProcessorCore:
				physical_cores_num++;
				// A hyperthreaded core supplies more than one logical processor.
				logical_cores_num += CountSetBits( it->ProcessorMask, is_popcnt_supported );
				break;

			default:
				// Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.
				break;
		}

		offset += sizeof(*it);
		++it;
	}

	::HeapFree( GetProcessHeap(), 0, buffer );

	return { physical_cores_num, logical_cores_num };
}
#endif

#if defined(POSIX)
// Move this declaration out of the CalculateClockSpeed() function because
// otherwise clang warns that it is non-obvious whether it is a variable
// or a function declaration: [-Wvexing-parse]
uint64 CalculateCPUFreq(); // from cpu_linux.cpp
#endif

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64 CalculateClockSpeed()
{
#if defined( _WIN32 )
#if !defined( _X360 )
	LARGE_INTEGER waitTime, startCount, curCount;
	CCycleCount start, end;

	// Take 1/32 of a second for the measurement.
	QueryPerformanceFrequency( &waitTime );
	int scale = 5;
	waitTime.QuadPart >>= scale;

	QueryPerformanceCounter( &startCount );
	start.Sample();
	do
	{
		QueryPerformanceCounter( &curCount );
	}
	while ( curCount.QuadPart - startCount.QuadPart < waitTime.QuadPart );
	end.Sample();

	int64 freq = (end.m_Int64 - start.m_Int64) << scale;
	if ( freq == 0 )
	{
		// Steam was seeing Divide-by-zero crashes on some Windows machines due to
		// WIN64_AMD_DUALCORE_TIMER_WORKAROUND that can cause rdtsc to effectively
		// stop. Staging doesn't have the workaround but I'm checking in the fix
		// anyway. Return a plausible speed and get on with our day.
		freq = 2000000000;
	}
	return freq;

#else
	return 3200000000LL;
#endif
#elif defined(POSIX)
	int64 freq =(int64)CalculateCPUFreq();
	if ( freq == 0 ) // couldn't calculate clock speed
	{
		Error( "Unable to determine CPU Frequency\n" );
	}
	return freq;
#endif
}

const CPUInformation* GetCPUInformation()
{
	static CPUInformation pi;

	// Has the structure already been initialized and filled out?
	if ( pi.m_Size == sizeof(pi) )
		return &pi;

	// Redundant, but just in case the user somehow messes with the size.
	memset(&pi, 0x0, sizeof(pi));

	// Grab the processor frequency:
	pi.m_Speed = CalculateClockSpeed();

	unsigned int eax, ebx, edx, ecx;
	if (cpuid(1, eax, ebx, ecx, edx))
	{
		pi.m_nModel = eax; // full CPU model info
		pi.m_nFeatures[0] = edx; // x87+ features
		pi.m_nFeatures[1] = ecx; // sse3+ features
		pi.m_nFeatures[2] = ebx; // some additional features
	}
	
	// Get the logical and physical processor counts:
	pi.m_nLogicalProcessors = LogicalProcessorsPerPackage( ebx );
	pi.m_nPhysicalProcessors = 1U;

#if defined(_WIN32) && !defined( _X360 )
	// dimhotepus: Correctly compute CPU cores count.
	CpuCoreInfo cpu_core_info{ GetProcessorCoresInfo( CheckPopcntTechnology( ecx ) ) };

	// Ensure 256+ CPUs are at least 256.
	pi.m_nPhysicalProcessors =
		static_cast<uint8>( min( cpu_core_info.physical_cores, static_cast<unsigned>(UCHAR_MAX) ) );
	pi.m_nLogicalProcessors =
		static_cast<uint8>( min( cpu_core_info.logical_cores, static_cast<unsigned>(UCHAR_MAX) ) );

	// Make sure I always report at least one, when running WinXP with the /ONECPU switch, 
	// it likes to report 0 processors for some reason.
	if ( pi.m_nPhysicalProcessors == 0 && pi.m_nLogicalProcessors == 0 )
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
	}
#elif defined( _X360 )
	pi.m_nPhysicalProcessors = 3;
	pi.m_nLogicalProcessors  = 6;
#elif defined(_LINUX)
	// TODO: poll /dev/cpuinfo when we have some benefits from multithreading
	FILE *fpCpuInfo = fopen( "/proc/cpuinfo", "r" );
	if ( fpCpuInfo )
	{
		int nLogicalProcs = 0;
		int nProcId = -1, nCoreId = -1;
		const int kMaxPhysicalCores = 128;
		int anKnownIds[kMaxPhysicalCores];
		int nKnownIdCount = 0;
		char buf[255];
		while ( fgets( buf, ARRAYSIZE(buf), fpCpuInfo ) )
		{
			if ( char *value = strchr( buf, ':' ) )
			{
				for ( char *p = value - 1; p > buf && isspace((unsigned char)*p); --p )
				{
					*p = 0;
				}
				for ( char *p = buf; p < value && *p; ++p )
				{
					*p = tolower((unsigned char)*p);
				}
				if ( !strcmp( buf, "processor" ) )
				{
					++nLogicalProcs;
					nProcId = nCoreId = -1;
				}
				else if ( !strcmp( buf, "physical id" ) )
				{
					nProcId = atoi( value+1 );
				}
				else if ( !strcmp( buf, "core id" ) )
				{
					nCoreId = atoi( value+1 );
				}

				if (nProcId != -1 && nCoreId != -1) // as soon as we have a complete id, process it
				{
					int i = 0, nId = (nProcId << 16) + nCoreId;
					while ( i < nKnownIdCount && anKnownIds[i] != nId ) { ++i; }
					if ( i == nKnownIdCount && nKnownIdCount < kMaxPhysicalCores )
						anKnownIds[nKnownIdCount++] = nId;
					nProcId = nCoreId = -1;
				}
			}
		}
		fclose( fpCpuInfo );
		pi.m_nLogicalProcessors = MAX( 1, nLogicalProcs );
		pi.m_nPhysicalProcessors = MAX( 1, nKnownIdCount );
	}
	else
	{
		pi.m_nPhysicalProcessors = 1;
		pi.m_nLogicalProcessors  = 1;
		Assert( !"couldn't read cpu information from /proc/cpuinfo" );
	}
#elif defined(OSX)
	int mib[2] { CTL_HW, HW_NCPU }, num_cpu = 1;
	size_t len = sizeof(num_cpu);

	sysctl(mib, 2, &num_cpu, &len, nullptr, 0);

	pi.m_nPhysicalProcessors = num_cpu;
	pi.m_nLogicalProcessors  = num_cpu;
#endif

	// Determine Processor Features:
	pi.m_bRDTSC        = CheckRDTSCTechnology( edx );
	pi.m_bCMOV         = CheckCMOVTechnology( edx );
	pi.m_bFCMOV        = CheckFCMOVTechnology( edx );
	pi.m_bSSE          = CheckSSETechnology( edx );
	pi.m_bSSE2         = CheckSSE2Technology( edx );
	pi.m_b3DNow        = Check3DNowTechnology();
	pi.m_bMMX          = CheckMMXTechnology( edx );
	// dimhotepus: Correctly check HyperThreading support.
	pi.m_bHT		   = pi.m_nPhysicalProcessors != pi.m_nLogicalProcessors;
	pi.m_bSSE3         = CheckSSE3Technology( ecx );
	pi.m_bSSSE3		   = CheckSSSE3Technology( ecx );
	pi.m_bSSE4a        = CheckSSE4aTechnology();
	pi.m_bSSE41        = CheckSSE41Technology( ecx );
	pi.m_bSSE42        = CheckSSE42Technology( ecx );
	pi.m_szProcessorID = GetProcessorVendorId();

	// Mark struct as ready and filled, return it:
	pi.m_Size = sizeof(pi);

	return &pi;
}
