#include "AntiVm.h"

#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include "ProcessInfo.h"
#include "Util.h"
#include "TraceLog.h"
#include "Settings.h"
#include "PinLocker.h"
#include "TinyTracer.h"

#include "EvasionWatch.h"

#define ANTIVM_LABEL "[ANTIVM] --> "

using namespace LEVEL_PINCLIENT;

/* ================================================================== */
// Global variables used by AntiVm
/* ================================================================== */
#define PATH_BUFSIZE	 512
#define WMI_NUMBER_CORES "NUMBEROFCORES"
#define WMI_PROCESSOR    "PROCESSORID"
#define WMI_SIZE         "SIZE"
#define WMI_DEVICE_ID    "DEVICEID"
#define WMI_MAC_ADDRESS  "MACADDRESS"
#define WMI_TEMPERATURE  "CURRENTTEMPERATURE"
#define WMI_SERIAL       "SERIALNUMBER"
#define WMI_MODEL        "MODEL"
#define WMI_MANUFACTURER "MANUFACTURER"
#define WMI_GPU_ADAPTER  "ADAPTERCOMPATIBILITY"	
#define WMI_PRODUCT      "PRODUCT"
#define WMI_NAME         "NAME"

typedef VOID AntiVmCallBack(const ADDRINT addr, const CHAR* name, uint32_t argCount, VOID* arg1, VOID* arg2, VOID* arg3, VOID* arg4, VOID* arg5, VOID* arg6);


/* ================================================================== */
// Global variables used by AntiVm
/* ================================================================== */

class AntiVmWatch : public EvasionWatch
{
public:
    AntiVmWatch() { Init(); }
    virtual BOOL Init();
};

AntiVmWatch m_AntiVm;

namespace AntiVm {
    std::map<THREADID, FuncData> m_funcData;
}; // namespace AntiVm 
/* ==================================================================== */
// Log info with AntiVm label
/* ==================================================================== */

VOID LogAntiVm(const WatchedType wType, const ADDRINT Address, const char* msg, const char* link = nullptr)
{
    LogMsgAtAddress(wType, Address, ANTIVM_LABEL, msg, link);
}

/* ==================================================================== */
// Process API calls (related to AntiVm techniques)
/* ==================================================================== */

VOID AntiVm_WmiQueries(const ADDRINT addr, const THREADID tid, const CHAR* name, uint32_t argCount, VOID* arg1, VOID* arg2, VOID* arg3, VOID* arg4, VOID* arg5)
{   
    if (!argCount) return;

    PinLocker locker;
    const WatchedType wType = isWatchedAddress(addr);
    if (wType == WatchedType::NOT_WATCHED) return;

    const wchar_t* wmi_query = reinterpret_cast<const wchar_t*>(arg2);
    if (wmi_query == NULL) return;

    char wmi_query_field[PATH_BUFSIZE];
    GET_STR_TO_UPPER(wmi_query, wmi_query_field, PATH_BUFSIZE);

    if (util::iequals(wmi_query_field, WMI_NUMBER_CORES) || util::iequals(wmi_query_field, WMI_PROCESSOR)) {
        return LogAntiVm(wType, addr, "^ WMI query - number of CPU cores",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_SIZE)) {
        return LogAntiVm(wType, addr, "^ WMI query - hard disk size",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_DEVICE_ID)) {
        return LogAntiVm(wType, addr, "^ WMI query - device ID",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_MAC_ADDRESS)) {
        return LogAntiVm(wType, addr, "^ WMI query - MAC address",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_TEMPERATURE)) {
        return LogAntiVm(wType, addr, "^ WMI query - system temperatures",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_SERIAL)) {
        return LogAntiVm(wType, addr, "^ WMI query - BIOS serial number",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_MODEL) || util::iequals(wmi_query_field, WMI_MANUFACTURER)) {
        return LogAntiVm(wType, addr, "^ WMI query - system model and/or manufacturer",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_GPU_ADAPTER)) {
        return LogAntiVm(wType, addr, "^ WMI query - video controller adapter",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
    else if (util::iequals(wmi_query_field, WMI_PRODUCT) || util::iequals(wmi_query_field, WMI_NAME)) {
        return LogAntiVm(wType, addr, "^ WMI query - system device names",
            "https://evasions.checkpoint.com/techniques/wmi.html#generic-wmi-queries");
    }
}

/* ==================================================================== */
// Add to monitored functions all the API calls needed for AntiVM.
/* ==================================================================== */

//Functions handles:

VOID AntiVm_NtQuerySystemInformation(const ADDRINT Address, const THREADID tid, const CHAR* name, uint32_t argCount, VOID* arg1, VOID* arg2, VOID* arg3, VOID* arg4, VOID* arg5, BOOL isAfter = FALSE)
{
    if (!argCount) return;

    PinLocker locker;
    const WatchedType wType = isWatchedAddress(Address);
    if (wType == WatchedType::NOT_WATCHED) return;

    enum SystemInformationClass {
        SystemFirmwareTableInformation = 0x4C
    };
    if (int((size_t)arg1) == SystemInformationClass::SystemFirmwareTableInformation) {
        if (!isAfter) {
            return LogAntiVm(wType, Address, "^ ntdll!NtQuerySystemInformation (SystemFirmwareTableInformation)",
                "https://revers.engineering/evading-trivial-acpi-checks/");
        }
        else {
            std::stringstream ss;
            ss << "^ ntdll!NtQuerySystemInformation (SystemFirmwareTableInformation). Bypass: ";
            size_t buf_size = (size_t)arg3;
            if (PIN_CheckWriteAccess(arg2) && PIN_CheckWriteAccess((VOID*)((ADDRINT)arg2 + (buf_size - 1)))) {
                ::memset(arg2, 0, buf_size);
                ss << "OK";
            }
            else {
                ss << "Failed";
            }
            return LogAntiVm(wType, Address, ss.str().c_str());
        }
    }
}

VOID AntiVm_NtQuerySystemInformation_before(const ADDRINT Address, const THREADID tid, const CHAR* name, uint32_t argCount, VOID* arg1, VOID* arg2, VOID* arg3, VOID* arg4, VOID* arg5)
{
    storeData(AntiVm::m_funcData, tid, name, argCount, arg1, arg2, arg3, arg4, arg5);
    return AntiVm_NtQuerySystemInformation(Address, tid, name, argCount, arg1, arg2, arg3, arg4, arg5, FALSE);
}

VOID AntiVm_NtQuerySystemInformation_after(const ADDRINT Address, const THREADID tid, const CHAR* name, ADDRINT status)
{
    if (status != 0) {
        return; // failed
    }
    FuncData data;
    if (!retrieveData(AntiVm::m_funcData, tid, name, data)) {
        return;
    }
    return AntiVm_NtQuerySystemInformation(Address, tid, name, data.argsNum, data.args[0], data.args[1], data.args[2], data.args[3], data.args[4], TRUE);
}

VOID AntiVm::MonitorSyscallEntry(THREADID tid, const CHAR* name, const CONTEXT* ctxt, SYSCALL_STANDARD std, const ADDRINT Address)
{
    EvasionFuncInfo* wfunc = m_AntiVm.fetchSyscallFuncInfo(name, m_Settings.antivm);
    if (!wfunc) return;

    EvasionWatchBeforeCallBack* callback = wfunc->callbackBefore;
    if (!callback) {
        return;
    }

    const size_t argCount = wfunc->paramCount;
    const size_t args_max = 5;
    VOID* syscall_args[args_max] = { 0 };

    for (size_t i = 0; i < args_max; i++) {
        if (i == argCount) break;
        syscall_args[i] = reinterpret_cast<VOID*>(PIN_GetSyscallArgument(ctxt, std, i));
    }
    callback(Address,
        tid,
        name, argCount,
        syscall_args[0],
        syscall_args[1],
        syscall_args[2],
        syscall_args[3],
        syscall_args[4]);
}

VOID AntiVm::MonitorSyscallExit(THREADID tid, const CHAR* name, const CONTEXT* ctxt, SYSCALL_STANDARD std, const ADDRINT Address)
{
    EvasionFuncInfo* wfunc = m_AntiVm.fetchSyscallFuncInfo(name, m_Settings.antivm);
    if (!wfunc) return;

    EvasionWatchAfterCallBack* callback = wfunc->callbackAfter;
    if (!callback) {
        return;
    }
    
    callback(Address,
        tid,
        name, 
        PIN_GetContextReg(ctxt, REG_GAX)
    );
}

//---
namespace AntiVm {
    std::map<THREADID, ADDRINT> cpuidThreads;
}; //namespace AntiVm


namespace AntiVm
{
    VOID CpuidCheck(CONTEXT* ctxt, THREADID tid)
    {
        PinLocker locker;

        const ADDRINT Address = (ADDRINT)PIN_GetContextReg(ctxt, REG_INST_PTR);

        const WatchedType wType = isWatchedAddress(Address);
        if (wType == WatchedType::NOT_WATCHED) return;

        ADDRINT opId = (ADDRINT)PIN_GetContextReg(ctxt, REG_GAX);
        cpuidThreads[tid] = opId;
        if (opId == 0x0) {
            return LogAntiVm(wType, Address, "CPUID - vendor check",
                "https://unprotect.it/technique/cpuid/");
        }
        if (opId == 0x1) {
            return LogAntiVm(wType, Address, "CPUID - HyperVisor bit check",
                "https://unprotect.it/technique/cpuid/");
        }
        if (opId == 0x80000002 || opId == 0x80000003 || opId == 0x80000004) {
            return LogAntiVm(wType, Address, "CPUID - brand check",
                "https://unprotect.it/technique/cpuid/");
        }
        if (opId == 0x40000000) {
            return LogAntiVm(wType, Address, "CPUID - HyperVisor vendor check",
                "https://unprotect.it/technique/cpuid/");
        }
        if (opId == 0x40000002) {
            return LogAntiVm(wType, Address, "CPUID - HyperVisor system identity");
        }
        if (opId == 0x40000003) {
            return LogAntiVm(wType, Address, "CPUID - HyperVisor feature identification");
        }
    }

    BOOL _AlterCpuidValue(CONTEXT* ctxt, THREADID tid, const REG reg, ADDRINT& regVal)
    {
        const BOOL isHyperVisorSet = m_Settings.isHyperVSet;
        BOOL isSet = FALSE;
        const ADDRINT Address = (ADDRINT)PIN_GetContextReg(ctxt, REG_INST_PTR);

        const WatchedType wType = isWatchedAddress(Address);
        if (wType == WatchedType::NOT_WATCHED) return FALSE;

        auto itr = cpuidThreads.find(tid);
        if (itr == cpuidThreads.end()) return FALSE;

        const ADDRINT opId = itr->second;
        std::stringstream ss;
        ss << "CPUID - HyperVisor res:" << std::hex;

        const ADDRINT prev = regVal;

        if (opId == 0x1) {
            if (reg == REG_GCX) {
                ss << " ECX: " << regVal;
                const ADDRINT hv_bit = (ADDRINT)0x1 << 31;
                if (isHyperVisorSet) {
                    regVal |= hv_bit; //set bit
                }
                else {
                    regVal &= ~hv_bit; //clear bit
                }
                isSet = TRUE;
            }  
        }

        if (opId == 0x40000000) {
            if (reg == REG_GAX) {
                ss << " EAX: " << regVal;
                if (isHyperVisorSet) {
                    regVal = 0x40000006;
                } else {
                    regVal = 0xc1c;
                } 
            } else if (reg == REG_GBX) {
                ss << " EBX: " << regVal;
                if (isHyperVisorSet) {
                    regVal = 0x7263694d;
                }
                else {
                    regVal = 0x14b4;
                }
            } else if (reg == REG_GCX) {
                ss << " ECX: " << regVal;
                if (isHyperVisorSet) {
                    regVal = 0x666f736f;
                }
                else {
                    regVal = 0x64;
                }
            } else if (reg == REG_GDX) {
                ss << " EDX: " << regVal;
                if (isHyperVisorSet) {
                    regVal = 0x76482074;
                }
                else {
                    regVal = 0;
                }
            }
            isSet = TRUE;
        }
        else if (opId == 0x40000003) {
            if (reg == REG_GAX) {
                ss << " EAX: " << regVal;
                if (isHyperVisorSet) {
                    regVal = 0x3fff;
                }
                else {
                    regVal = 0x14b4;
                }
            } else if (reg == REG_GBX) {
                ss << " EBX: " << regVal;
                if (isHyperVisorSet) {
                    regVal = 0x2bb9ff;
                }
                else {
                    regVal = 0x64;
                }
            } else if (reg == REG_GCX) {
                ss << " ECX: " << regVal;
                regVal = 0;
            } else if (reg == REG_GDX) {
                ss << " EDX: " << regVal;
                regVal = 0;
            }
            isSet = TRUE;
        }
        if (isSet && ss.str().length()) {
            if (prev != regVal) {
                ss << " -> " << regVal;
            }
            LogAntiVm(wType, Address, ss.str().c_str());
        }
        return isSet;
    }

    ADDRINT AlterCpuidValue(CONTEXT* ctxt, THREADID tid, const REG reg)
    {
        PinLocker locker;
        ADDRINT regVal = PIN_GetContextReg(ctxt, reg);
        _AlterCpuidValue(ctxt, tid, reg, regVal);
        return regVal;
    }

}; //namespace AntiVm


VOID AntiVm::InstrumentCPUIDCheck(INS ins)
{
    INS_InsertCall(
        ins,
        IPOINT_BEFORE, (AFUNPTR)AntiVm::CpuidCheck,
        IARG_CONTEXT,
        IARG_THREAD_ID,
        IARG_END
    );

    INS_InsertCall(
        ins,
        IPOINT_AFTER, (AFUNPTR)AntiVm::AlterCpuidValue,
        IARG_CONTEXT,
        IARG_THREAD_ID,
        IARG_UINT32, REG_GAX,
        IARG_RETURN_REGS,
        REG_GAX,
        IARG_END
    );

    INS_InsertCall(
        ins,
        IPOINT_AFTER, (AFUNPTR)AntiVm::AlterCpuidValue,
        IARG_CONTEXT,
        IARG_THREAD_ID,
        IARG_UINT32, REG_GBX,
        IARG_RETURN_REGS,
        REG_GBX,
        IARG_END
    );

    INS_InsertCall(
        ins,
        IPOINT_AFTER, (AFUNPTR)AntiVm::AlterCpuidValue,
        IARG_CONTEXT,
        IARG_THREAD_ID,
        IARG_UINT32, REG_GCX,
        IARG_RETURN_REGS,
        REG_GCX,
        IARG_END
    );

    INS_InsertCall(
        ins,
        IPOINT_AFTER, (AFUNPTR)AntiVm::AlterCpuidValue,
        IARG_CONTEXT,
        IARG_THREAD_ID,
        IARG_UINT32, REG_GDX,
        IARG_RETURN_REGS,
        REG_GDX,
        IARG_END
    );
}

//---

BOOL AntiVmWatch::Init()
{
    watchedFuncs.appendFunc(EvasionFuncInfo("ntdll", "NtQuerySystemInformation", 4, AntiVm_NtQuerySystemInformation_before, AntiVm_NtQuerySystemInformation_after));
    // API needed to trace WMI queries:
#ifdef _WIN64
    watchedFuncs.appendFunc(EvasionFuncInfo("fastprox", "?Get@CWbemObject@@UEAAJPEBGJPEAUtagVARIANT@@PEAJ2@Z", 5, AntiVm_WmiQueries));
#else
    watchedFuncs.appendFunc(EvasionFuncInfo("fastprox", "?Get@CWbemObject@@UAGJPBGJPAUtagVARIANT@@PAJ2@Z", 5, AntiVm_WmiQueries));
#endif
    isInit = TRUE;
    return isInit;
}

VOID AntiVm::MonitorAntiVmFunctions(IMG Image)
{
    m_AntiVm.installCallbacks(Image, nullptr, m_Settings.antivm);
}
