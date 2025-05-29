#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <Windows.h>
#include <iphlpapi.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <vector>
#include <codecvt>
#include <filesystem>
#include <tlhelp32.h>
#include <iostream>
#include <shlobj.h>
#include <cstdlib> // For system()
#include <setupapi.h>
#include <cfgmgr32.h>
#include <iomanip>
#include <random>
#include <sstream>
#include <psapi.h>
#include <powrprof.h> // For battery information
#include <timezoneapi.h> // For GetTimeZoneInformation
#include <oleauto.h> // For SafeArray functions
#include <rpc.h> // For RPC_S_ALREADY_INITIALIZED



#define _WIN32_WINNT 0x0600 // Ensure Windows Vista or later APIs



// Link necessary libraries
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "advapi32.lib") // For registry functions
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "powrprof.lib")



//-------------------------------------------------------------------------------------------------------------------------------------------
//HWID Checker Functions
//-------------------------------------------------------------------------------------------------------------------------------------------



// Helper function to initialize WMI
bool InitializeWMI(IWbemLocator** pLoc, IWbemServices** pSvc) {
	HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres) && hres != RPC_S_ALREADY_INITIALIZED) return false;

	hres = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
	if (FAILED(hres)) {
		if (hres != RPC_S_ALREADY_INITIALIZED) CoUninitialize();
		return false;
	}

	hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
		reinterpret_cast<LPVOID*>(pLoc));
	if (FAILED(hres)) {
		if (hres != RPC_S_ALREADY_INITIALIZED) CoUninitialize();
		return false;
	}

	hres = (*pLoc)->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, 0, 0, pSvc);
	if (FAILED(hres)) {
		(*pLoc)->Release();
		if (hres != RPC_S_ALREADY_INITIALIZED) CoUninitialize();
		return false;
	}

	return true;
}

// Helper function to clean up WMI
void CleanupWMI(IWbemLocator* pLoc, IWbemServices* pSvc) {
	if (pSvc) pSvc->Release();
	if (pLoc) pLoc->Release();
}

// Helper function to convert VARIANT to string
std::string VariantToString(const VARIANT& vtProp) {
	if (vtProp.vt == VT_NULL || vtProp.vt == VT_EMPTY) return "Unknown";
	if (vtProp.vt == VT_BSTR) return _bstr_t(vtProp.bstrVal);
	if (vtProp.vt == VT_I4) return std::to_string(vtProp.lVal);
	if (vtProp.vt == VT_UI4) return std::to_string(vtProp.ulVal);
	return "Unknown";
}

// Get System Name
std::string GetSystemName() {
	char computerName[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD size = sizeof(computerName);
	if (GetComputerNameA(computerName, &size)) {
		return std::string(computerName);
	}
	return "Unknown";
}

// Get System UUID
std::string GetSystemUUID() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT UUID FROM Win32_ComputerSystemProduct"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string uuid = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		hres = pclsObj->Get(L"UUID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) {
			uuid = VariantToString(vtProp);
		}
		VariantClear(&vtProp);
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return uuid;
}

// Get System Manufacturer and Model
std::string GetSystemManufacturerAndModel() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT Manufacturer, Model FROM Win32_ComputerSystem"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string info = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string manufacturer, model;
		hres = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) {
			manufacturer = VariantToString(vtProp);
		}
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) {
			model = VariantToString(vtProp);
		}
		VariantClear(&vtProp);

		info = "  Manufacturer: " + manufacturer + "\n  Model: " + model;
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return info;
}

// Get OS Information
std::string GetOSInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT Caption, Version, BuildNumber, SerialNumber FROM Win32_OperatingSystem"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string osInfo = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string caption, version, build, serial;
		hres = pclsObj->Get(L"Caption", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) caption = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Version", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) version = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"BuildNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) build = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) serial = VariantToString(vtProp);
		VariantClear(&vtProp);

		osInfo = "  Name: " + caption + "\n  Version: " + version + "\n  Build: " + build + "\n  Serial Number: " + serial;

		char sysDir[MAX_PATH], winDir[MAX_PATH];
		GetSystemDirectoryA(sysDir, MAX_PATH);
		GetWindowsDirectoryA(winDir, MAX_PATH);
		osInfo += "\n  System Directory: " + std::string(sysDir) + "\n  Windows Directory: " + std::string(winDir);

		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return osInfo;
}

// Get Locale and Time Zone
std::string GetLocaleAndTimeZone() {
	char locale[LOCALE_NAME_MAX_LENGTH];
	GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, locale, sizeof(locale));

	TIME_ZONE_INFORMATION tzInfo;
	GetTimeZoneInformation(&tzInfo);
	std::wstring wzTimeZone(tzInfo.StandardName);
	std::string timeZone(wzTimeZone.begin(), wzTimeZone.end());

	return "  Locale: " + std::string(locale) + "\n  Time Zone: " + timeZone;
}

// Enhanced GetNetworkInfo (Using IP_ADAPTER_ADDRESSES)
std::string GetNetworkInfo() {
	std::string result;
	ULONG bufferSize = 0;
	GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &bufferSize);
	std::vector<char> buffer(bufferSize);
	PIP_ADAPTER_ADDRESSES adapterInfo = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX, nullptr, adapterInfo, &bufferSize) == ERROR_SUCCESS) {
		int adapterCount = 0;
		while (adapterInfo) {
			char mac[18];
			sprintf_s(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
				adapterInfo->PhysicalAddress[0], adapterInfo->PhysicalAddress[1], adapterInfo->PhysicalAddress[2],
				adapterInfo->PhysicalAddress[3], adapterInfo->PhysicalAddress[4], adapterInfo->PhysicalAddress[5]);
			result += "  Adapter " + std::to_string(++adapterCount) + ": " + std::string(_bstr_t(adapterInfo->FriendlyName)) + "\n";
			result += "    MAC: " + std::string(mac) + "\n";

			// Get IP Address
			std::string ip = "Unknown";
			if (adapterInfo->FirstUnicastAddress) {
				char ipBuffer[INET_ADDRSTRLEN];
				PSOCKADDR_IN addr = reinterpret_cast<PSOCKADDR_IN>(adapterInfo->FirstUnicastAddress->Address.lpSockaddr);
				inet_ntop(AF_INET, &(addr->sin_addr), ipBuffer, sizeof(ipBuffer));
				ip = ipBuffer;
			}
			result += "    IP: " + ip + "\n";

			result += "    Adapter Name (GUID): " + std::string(_bstr_t(adapterInfo->AdapterName)) + "\n";
			result += "    Type: " + std::to_string(adapterInfo->IfType) + "\n";

			// Get DHCP Server
			std::string dhcp = "Unknown";
			if (adapterInfo->Dhcpv4Server.lpSockaddr) {
				char dhcpBuffer[INET_ADDRSTRLEN];
				PSOCKADDR_IN dhcpAddr = reinterpret_cast<PSOCKADDR_IN>(adapterInfo->Dhcpv4Server.lpSockaddr);
				inet_ntop(AF_INET, &(dhcpAddr->sin_addr), dhcpBuffer, sizeof(dhcpBuffer));
				dhcp = dhcpBuffer;
			}
			result += "    DHCP Server: " + dhcp + "\n";

			// Get Gateway
			std::string gateway = "Unknown";
			if (adapterInfo->FirstGatewayAddress) {
				char gatewayBuffer[INET_ADDRSTRLEN];
				PSOCKADDR_IN gwAddr = reinterpret_cast<PSOCKADDR_IN>(adapterInfo->FirstGatewayAddress->Address.lpSockaddr);
				inet_ntop(AF_INET, &(gwAddr->sin_addr), gatewayBuffer, sizeof(gatewayBuffer));
				gateway = gatewayBuffer;
			}
			result += "    Gateway: " + gateway + "\n";

			// Get DNS Servers
			result += "    DNS Servers: ";
			for (PIP_ADAPTER_DNS_SERVER_ADDRESS dns = adapterInfo->FirstDnsServerAddress; dns; dns = dns->Next) {
				char dnsBuffer[INET_ADDRSTRLEN];
				PSOCKADDR_IN dnsAddr = reinterpret_cast<PSOCKADDR_IN>(dns->Address.lpSockaddr);
				inet_ntop(AF_INET, &(dnsAddr->sin_addr), dnsBuffer, sizeof(dnsBuffer));
				result += std::string(dnsBuffer) + (dns->Next ? ", " : "\n");
			}
			if (!adapterInfo->FirstDnsServerAddress) result += "Unknown\n";

			adapterInfo = adapterInfo->Next;
		}
	}
	return result.empty() ? "Unknown" : result;
}

// Enhanced GetCPUInfo
std::string GetCPUInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT ProcessorId, Name, NumberOfCores, NumberOfLogicalProcessors, Architecture FROM Win32_Processor"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string cpuInfo = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string id, name, cores, threads, arch;
		hres = pclsObj->Get(L"ProcessorId", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) id = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) name = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"NumberOfCores", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) cores = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"NumberOfLogicalProcessors", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) threads = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Architecture", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) arch = VariantToString(vtProp);
		VariantClear(&vtProp);

		cpuInfo = "  Processor ID: " + id + "\n  Name: " + name + "\n  Cores: " + cores + "\n  Threads: " + threads + "\n  Architecture: " + arch;
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return cpuInfo;
}

// Enhanced GetGPUInfo
std::string GetGPUInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT PNPDeviceID, Name, DriverVersion, AdapterRAM FROM Win32_VideoController"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string gpuInfo;
	int gpuCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string pnpId, name, driver, ram;
		hres = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) pnpId = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) name = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"DriverVersion", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) driver = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"AdapterRAM", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres) && vtProp.vt == VT_UI4) {
			ram = std::to_string(vtProp.ulVal / (1024 * 1024)) + " MB";
		}
		else {
			ram = "Unknown";
		}
		VariantClear(&vtProp);

		gpuInfo += "  GPU " + std::to_string(++gpuCount) + ":\n";
		gpuInfo += "    PNP ID: " + pnpId + "\n    Name: " + name + "\n    Driver Version: " + driver + "\n    Memory: " + ram + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return gpuInfo.empty() ? "Unknown" : gpuInfo;
}

// Get Motherboard Info
std::string GetMotherboardInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT SerialNumber, Manufacturer, Product FROM Win32_BaseBoard"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string mbInfo = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string serial, manufacturer, product;
		hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) serial = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) manufacturer = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Product", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) product = VariantToString(vtProp);
		VariantClear(&vtProp);

		mbInfo = "  Serial Number: " + serial + "\n  Manufacturer: " + manufacturer + "\n  Product: " + product;
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return mbInfo;
}

// Enhanced GetDiskInfo (Fixed variable name and size issue)
std::string GetDiskInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT SerialNumber, Model, InterfaceType, Size FROM Win32_DiskDrive"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string diskInfo;
	int diskCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string serial, model, interfaceType, diskSize;
		hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) serial = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) model = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"InterfaceType", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) interfaceType = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Size", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR && vtProp.bstrVal != nullptr) {
			diskSize = std::to_string(_wtoi64(vtProp.bstrVal) / (1024 * 1024 * 1024)) + " GB";
		}
		else {
			diskSize = "Unknown";
		}
		VariantClear(&vtProp);

		diskInfo += "  Disk " + std::to_string(++diskCount) + ":\n";
		diskInfo += "    Serial Number: " + serial + "\n    Model: " + model + "\n    Interface: " + interfaceType + "\n    Size: " + diskSize + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return diskInfo.empty() ? "Unknown" : diskInfo;
}

// Get BIOS Info
std::string GetBIOSInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT SerialNumber, Manufacturer, SMBIOSBIOSVersion FROM Win32_BIOS"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string biosInfo = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string serial, manufacturer, version;
		hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) serial = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) manufacturer = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"SMBIOSBIOSVersion", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) version = VariantToString(vtProp);
		VariantClear(&vtProp);

		biosInfo = "  Serial Number: " + serial + "\n  Manufacturer: " + manufacturer + "\n  Version: " + version;
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return biosInfo;
}

// Enhanced GetRAMInfo
std::string GetRAMInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	MEMORYSTATUSEX memStatus;
	memStatus.dwLength = sizeof(memStatus);
	GlobalMemoryStatusEx(&memStatus);
	std::string ramInfo = "  Total Physical Memory: " + std::to_string(memStatus.ullTotalPhys / (1024 * 1024)) + " MB\n";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT DeviceLocator, PartNumber, Speed, FormFactor, SerialNumber FROM Win32_PhysicalMemory"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return ramInfo + "  Module Details: WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	int moduleCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string moduleInfo = "  Module " + std::to_string(++moduleCount) + ":\n";
		hres = pclsObj->Get(L"DeviceLocator", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) moduleInfo += "    Location: " + VariantToString(vtProp) + "\n";
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"PartNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) moduleInfo += "    Part Number: " + VariantToString(vtProp) + "\n";
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Speed", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) moduleInfo += "    Speed: " + VariantToString(vtProp) + " MHz\n";
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"FormFactor", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) moduleInfo += "    Form Factor: " + VariantToString(vtProp) + "\n";
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) moduleInfo += "    Serial Number: " + VariantToString(vtProp) + "\n";
		VariantClear(&vtProp);

		ramInfo += moduleInfo;
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return ramInfo.empty() ? "Unknown" : ramInfo;
}

// Get Monitor Info
std::string GetMonitorInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT DeviceID, PNPDeviceID, Name FROM Win32_DesktopMonitor"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string monitorInfo;
	int monitorCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string id, pnpId, name;
		hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) id = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) pnpId = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) name = VariantToString(vtProp);
		VariantClear(&vtProp);

		monitorInfo += "  Monitor " + std::to_string(++monitorCount) + ":\n";
		monitorInfo += "    Device ID: " + id + "\n    PNP ID: " + pnpId + "\n    Name: " + name + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return monitorInfo.empty() ? "Unknown" : monitorInfo;
}

// Get TPM Info
std::string GetTPMInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT SpecVersion, ManufacturerId FROM Win32_TPM"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string tpmInfo = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string version, manufacturer;
		hres = pclsObj->Get(L"SpecVersion", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) version = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"ManufacturerId", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) manufacturer = VariantToString(vtProp);
		VariantClear(&vtProp);

		tpmInfo = "  Version: " + version + "\n  Manufacturer ID: " + manufacturer;
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return tpmInfo;
}

// Get Sound Device Info
std::string GetSoundDeviceInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT DeviceID, PNPDeviceID, Name FROM Win32_SoundDevice"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string soundInfo;
	int deviceCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string id, pnpId, name;
		hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) id = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) pnpId = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) name = VariantToString(vtProp);
		VariantClear(&vtProp);

		soundInfo += "  Device " + std::to_string(++deviceCount) + ":\n";
		soundInfo += "    Device ID: " + id + "\n    PNP ID: " + pnpId + "\n    Name: " + name + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return soundInfo.empty() ? "Unknown" : soundInfo;
}

// Get Battery Info
std::string GetBatteryInfo() {
	SYSTEM_POWER_STATUS batteryStatus;
	if (!GetSystemPowerStatus(&batteryStatus)) {
		return "Battery Info Unavailable";
	}

	std::string batteryInfo;
	batteryInfo += "  Battery Present: " + std::string(batteryStatus.BatteryFlag == 128 ? "No" : "Yes") + "\n";
	if (batteryStatus.BatteryFlag != 128) {
		batteryInfo += "  Battery Life Percent: " + std::to_string(batteryStatus.BatteryLifePercent) + "%\n";
		batteryInfo += "  Battery Life Time: " + (batteryStatus.BatteryLifeTime == -1 ? "Unknown" : std::to_string(batteryStatus.BatteryLifeTime / 60) + " minutes") + "\n";
	}

	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return batteryInfo + "  WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT Chemistry, DeviceID FROM Win32_Battery"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return batteryInfo + "  WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string chemistry, deviceId;
		hres = pclsObj->Get(L"Chemistry", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) chemistry = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) deviceId = VariantToString(vtProp);
		VariantClear(&vtProp);

		batteryInfo += "  Chemistry: " + chemistry + "\n  Device ID: " + deviceId + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return batteryInfo.empty() ? "Unknown" : batteryInfo;
}

// Get USB Devices Info
std::string GetUSBDevicesInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT DeviceID, PNPDeviceID, Description FROM Win32_PnPEntity WHERE PNPDeviceID LIKE 'USB%'"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string usbInfo;
	int deviceCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string id, pnpId, desc;
		hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) id = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"PNPDeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) pnpId = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Description", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) desc = VariantToString(vtProp);
		VariantClear(&vtProp);

		usbInfo += "  USB Device " + std::to_string(++deviceCount) + ":\n";
		usbInfo += "    Device ID: " + id + "\n    PNP ID: " + pnpId + "\n    Description: " + desc + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return usbInfo.empty() ? "Unknown" : usbInfo;
}

// Get Printer Info
std::string GetPrinterInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT Name, DeviceID, Status FROM Win32_Printer"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string printerInfo;
	int printerCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string name, id, status;
		hres = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) name = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) id = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"Status", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) status = VariantToString(vtProp);
		VariantClear(&vtProp);

		printerInfo += "  Printer " + std::to_string(++printerCount) + ":\n";
		printerInfo += "    Name: " + name + "\n    Device ID: " + id + "\n    Status: " + status + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return printerInfo.empty() ? "Unknown" : printerInfo;
}

// Get Chassis Info
std::string GetChassisInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	if (!InitializeWMI(&pLoc, &pSvc)) return "WMI Init Failed";

	IEnumWbemClassObject* pEnumerator = nullptr;
	HRESULT hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT SerialNumber, ChassisTypes FROM Win32_SystemEnclosure"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string chassisInfo = "Unknown";
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string serial, type = "Unknown";
		hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) serial = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"ChassisTypes", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres) && vtProp.vt == (VT_ARRAY | VT_I4)) {
			SAFEARRAY* sa = vtProp.parray;
			long* pVal = nullptr;
			HRESULT hr = SafeArrayAccessData(sa, (void**)&pVal);
			if (SUCCEEDED(hr)) {
				long lBound, uBound;
				SafeArrayGetLBound(sa, 1, &lBound);
				SafeArrayGetUBound(sa, 1, &uBound);
				if (lBound <= uBound && pVal) {
					type = std::to_string(pVal[0]);
				}
				SafeArrayUnaccessData(sa);
			}
		}
		VariantClear(&vtProp);

		chassisInfo = "  Serial Number: " + serial + "\n  Chassis Type: " + type;
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return chassisInfo;
}

// Get Antivirus Info
std::string GetAntivirusInfo() {
	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;
	HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres) && hres != RPC_S_ALREADY_INITIALIZED) return "WMI Init Failed";

	hres = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
	if (FAILED(hres)) {
		if (hres != RPC_S_ALREADY_INITIALIZED) CoUninitialize();
		return "WMI Security Init Failed";
	}

	hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
		reinterpret_cast<LPVOID*>(&pLoc));
	if (FAILED(hres)) {
		if (hres != RPC_S_ALREADY_INITIALIZED) CoUninitialize();
		return "WMI Instance Creation Failed";
	}

	hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\SecurityCenter2"), nullptr, nullptr, 0, 0, 0, 0, &pSvc);
	if (FAILED(hres)) {
		pLoc->Release();
		if (hres != RPC_S_ALREADY_INITIALIZED) CoUninitialize();
		return "WMI Connection Failed (SecurityCenter2 may not be available)";
	}

	IEnumWbemClassObject* pEnumerator = nullptr;
	hres = pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t("SELECT displayName, productState FROM AntiVirusProduct"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);
	if (FAILED(hres)) {
		CleanupWMI(pLoc, pSvc);
		return "WMI Query Failed";
	}

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;
	std::string avInfo;
	int avCount = 0;
	while (pEnumerator) {
		hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (uReturn == 0) break;

		VARIANT vtProp;
		VariantInit(&vtProp);
		std::string name, state;
		hres = pclsObj->Get(L"displayName", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) name = VariantToString(vtProp);
		VariantClear(&vtProp);

		hres = pclsObj->Get(L"productState", 0, &vtProp, 0, 0);
		if (SUCCEEDED(hres)) state = VariantToString(vtProp);
		VariantClear(&vtProp);

		avInfo += "  Antivirus " + std::to_string(++avCount) + ":\n";
		avInfo += "    Name: " + name + "\n    Product State: " + state + "\n";
		pclsObj->Release();
	}

	pEnumerator->Release();
	CleanupWMI(pLoc, pSvc);
	return avInfo.empty() ? "Unknown" : avInfo;
}

// Get Formatted System Info
std::string GetFormattedSystemInfo() {
	std::string info;
	info += "System Name: " + GetSystemName() + "\n";
	info += "System UUID: " + GetSystemUUID() + "\n";
	info += "System Manufacturer and Model:\n" + GetSystemManufacturerAndModel() + "\n";
	info += "Operating System Info:\n" + GetOSInfo() + "\n";
	info += "Locale and Time Zone:\n" + GetLocaleAndTimeZone() + "\n";
	info += "Network Info:\n" + GetNetworkInfo();
	info += "CPU Info:\n" + GetCPUInfo() + "\n";
	info += "GPU Info:\n" + GetGPUInfo();
	info += "Motherboard Info:\n" + GetMotherboardInfo() + "\n";
	info += "Disk Info:\n" + GetDiskInfo();
	info += "BIOS Info:\n" + GetBIOSInfo() + "\n";
	info += "RAM Info:\n" + GetRAMInfo();
	info += "Monitor Info:\n" + GetMonitorInfo();
	info += "TPM Info:\n" + GetTPMInfo() + "\n";
	info += "Sound Device Info:\n" + GetSoundDeviceInfo();
	info += "Battery Info:\n" + GetBatteryInfo() + "\n";
	info += "USB Devices Info:\n" + GetUSBDevicesInfo();
	info += "Printer Info:\n" + GetPrinterInfo();
	info += "Chassis Info:\n" + GetChassisInfo() + "\n";
	info += "Antivirus Info:\n" + GetAntivirusInfo();
	return info;
}


//-------------------------------------------------------------------------------------------------------------------------------------------
//RecRoom File Deleter
//-------------------------------------------------------------------------------------------------------------------------------------------




// Function to get the current username
std::wstring GetCurrentUsername() {
	wchar_t username[256];
	DWORD size = sizeof(username) / sizeof(username[0]);
	GetUserNameW(username, &size);
	return std::wstring(username);
}

// Function to expand environment variables (e.g., %LocalAppData%, %AppData%)
std::wstring ExpandEnvironmentVariables(const std::wstring& path) {
	wchar_t expanded[MAX_PATH];
	ExpandEnvironmentStringsW(path.c_str(), expanded, MAX_PATH);
	return std::wstring(expanded);
}

// Function to delete a directory and its contents
bool DeleteDirectory(const std::wstring& path) {
	std::error_code ec;
	std::filesystem::remove_all(path, ec);
	return !ec;
}

// Function to delete a file
bool DeleteFile(const std::wstring& path) {
	return DeleteFileW(path.c_str()) != 0;
}

// Function to delete a registry key and its subkeys
bool DeleteRegistryKey(HKEY hKeyRoot, const std::wstring& subKey) {
	LONG result = RegDeleteTreeW(hKeyRoot, subKey.c_str());
	return result == ERROR_SUCCESS;
}

// Main function to perform all deletions
void PerformCleanup() {
	// Get username and environment variables for path expansion
	std::wstring username = GetCurrentUsername();
	std::wstring localAppData = ExpandEnvironmentVariables(L"%LocalAppData%");
	std::wstring appData = ExpandEnvironmentVariables(L"%AppData%");

	// List of files and directories to delete
	std::vector<std::wstring> pathsToDelete = {
		L"C:\\Users\\" + username + L"\\AppData\\LocalLow\\Against Gravity",
		L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\RecRoom",
		L"C:\\Users\\" + username + L"\\AppData\\Roaming\\Microsoft\\Windows\\Recent\\RecRoom.lnk",
		L"C:\\Users\\" + username + L"\\Desktop\\Rec Room.url",
		L"C:\\Windows\\Prefetch\\RECROOM.EXE-BEC42EED.pf",
		L"C:\\Windows\\Prefetch\\RECROOM_RELEASE.EXE-35556F3D.pf",
		localAppData + L"\\Temp\\RecRoom",
		localAppData + L"\\Temp\\Against Gravity",
		appData + L"\\Microsoft\\Windows\\Recent\\RecRoom.lnk",
		appData + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Steam\\Rec Room.url",
		L"C:\\Users\\" + username + L"\\AppData\\Local\\Programs\\recroom-launcher\\Apps\\Rec Room\\RecRoom.exe.log",
		L"C:\\Users\\" + username + L"\\AppData\\LocalLow\\Against Gravity",
		L"C:\\Users\\" + username + L"\\AppData\\Roaming\\recroom-launcher"
	};

	// Delete files and directories
	for (const auto& path : pathsToDelete) {
		if (std::filesystem::is_directory(path)) {
			if (DeleteDirectory(path)) {
				printf("Deleted directory: %ws\n", path.c_str());
			}
			else {
				printf("Failed to delete directory: %ws\n", path.c_str());
			}
		}
		else if (std::filesystem::exists(path)) {
			if (DeleteFile(path)) {
				printf("Deleted file: %ws\n", path.c_str());
			}
			else {
				printf("Failed to delete file: %ws\n", path.c_str());
			}
		}
	}

	// List of registry keys to delete
	std::vector<std::pair<HKEY, std::wstring>> registryKeys = {
		{ HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam\\Apps\\471710" },
		{ HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\FirewallRules" },
		{ HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 471710" },
		{ HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam\\Apps\\471710" },
		{ HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\recroom" },
		{ HKEY_CURRENT_USER, L"Software\\Against Gravity" }
	};

	// Delete registry keys
	for (const auto& [hKeyRoot, subKey] : registryKeys) {
		if (DeleteRegistryKey(hKeyRoot, subKey)) {
			printf("Deleted registry key: %ws\n", subKey.c_str());
		}
		else {
			printf("Failed to delete registry key: %ws\n", subKey.c_str());
		}
	}
}



//-------------------------------------------------------------------------------------------------------------------------------------------
//IP Config And DNS Fixer
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to execute network reset commands
void RunNetworkReset() {
	// Array of commands (Windows-specific)
	const char* commands[] = {
		"netsh winsock reset",
		"netsh int ip reset",
		"netsh advfirewall reset",
		"ipconfig /flushdns",
		"ipconfig /release",
		"ipconfig /renew"
	};

	// Execute each command
	for (const auto& cmd : commands) {
		// system() returns 0 on success, non-zero on failure
		int result = system(cmd);
		if (result != 0) {
			// Handle error (e.g., log to console or show in UI)
			// For simplicity, we continue with other commands
		}
	}
}



//-------------------------------------------------------------------------------------------------------------------------------------------
//MachineGUID Changer
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to generate a random GUID
std::wstring GenerateRandomGUID() {
	char guid[40];
	sprintf_s(guid, "{%08X-%04X-%04X-%04X-%012X}",
		rand() % 0xFFFFFFFF, rand() % 0xFFFF, rand() % 0xFFFF,
		rand() % 0xFFFF, rand() % 0xFFFFFFFFFFF);
	return std::wstring(guid, guid + strlen(guid));
}

// Function to get the current MachineGuid
std::wstring GetMachineGuid() {
	HKEY hKey;
	wchar_t buffer[256];
	DWORD size = sizeof(buffer);
	std::wstring result = L"Unknown";

	LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hKey);
	if (res == ERROR_SUCCESS) {
		res = RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr, (LPBYTE)buffer, &size);
		if (res == ERROR_SUCCESS) {
			result = buffer;
		}
		RegCloseKey(hKey);
	}
	return result;
}

// Function to set a new MachineGuid
std::wstring SetMachineGuid(const std::wstring& newGuid) {
	HKEY hKey;
	LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey);
	if (result != ERROR_SUCCESS) {
		return L"Failed to open registry key. Error: " + std::to_wstring(result) + L". Run as Administrator.";
	}

	result = RegSetValueExW(hKey, L"MachineGuid", 0, REG_SZ, (const BYTE*)newGuid.c_str(), (newGuid.size() + 1) * sizeof(wchar_t));
	RegCloseKey(hKey);
	if (result != ERROR_SUCCESS) {
		return L"Failed to set MachineGuid. Error: " + std::to_wstring(result);
	}
	return L"Successfully set MachineGuid to: " + newGuid;
}

std::wstring currentGuid = GetMachineGuid();
std::wstring statusMessage = L"Ready";



//------------------------------------------------------------------------------------------------------------------------------------------
//GPU HWID Changer
//------------------------------------------------------------------------------------------------------------------------------------------



// Function to generate a random HWID in PCI format
std::string GenerateRandomHWID() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> hexDist(0, 15); // For hexadecimal digits (0-F)

	auto hexChar = [](int n) -> char {
		return n < 10 ? '0' + n : 'A' + (n - 10);
		};

	std::stringstream ss;
	ss << "PCI\\VEN_";

	// Generate 4-digit Vendor ID
	for (int i = 0; i < 4; ++i) {
		ss << hexChar(hexDist(gen));
	}
	ss << "&DEV_";

	// Generate 4-digit Device ID
	for (int i = 0; i < 4; ++i) {
		ss << hexChar(hexDist(gen));
	}
	ss << "&SUBSYS_";

	// Generate 8-digit Subsystem ID
	for (int i = 0; i < 8; ++i) {
		ss << hexChar(hexDist(gen));
	}
	ss << "&REV_";

	// Generate 2-digit Revision ID
	for (int i = 0; i < 2; ++i) {
		ss << hexChar(hexDist(gen));
	}

	std::string hwid = ss.str();
	// Cap length at 50 characters to prevent registry issues
	if (hwid.length() > 50) {
		hwid = hwid.substr(0, 50);
	}
	return hwid;
}

// Function to find the registry path for the first display adapter
bool FindGPURegistryPath(std::wstring& registryPath, std::string& deviceID) {
	// GUID for display adapters
	const GUID displayGUID = { 0x4d36e968, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };

	// Get device information set for display adapters
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&displayGUID, NULL, NULL, DIGCF_PRESENT);
	if (deviceInfoSet == INVALID_HANDLE_VALUE) {
		return false;
	}

	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	// Enumerate devices
	for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
		// Get device ID
		wchar_t deviceIDBuffer[256];
		if (CM_Get_Device_IDW(deviceInfoData.DevInst, deviceIDBuffer, sizeof(deviceIDBuffer) / sizeof(wchar_t), 0) == CR_SUCCESS) {
			// Convert device ID to string
			deviceID = std::string(deviceIDBuffer, deviceIDBuffer + wcslen(deviceIDBuffer));

			// Construct registry path
			registryPath = L"SYSTEM\\CurrentControlSet\\Enum\\" + std::wstring(deviceIDBuffer);
			SetupDiDestroyDeviceInfoList(deviceInfoSet);
			return true; // Return first valid GPU
		}
	}

	SetupDiDestroyDeviceInfoList(deviceInfoSet);
	return false; // No GPU found
}

// Function to spoof GPU HWID in the registry
bool SpoofGPU(const std::wstring& registryPath, const std::string& newHardwareID) {
	HKEY hKey;

	// Open registry key with write access
	LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, registryPath.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
	if (result != ERROR_SUCCESS) {
		return false; // Failed to open key
	}

	// Prepare values
	const wchar_t* classGUID = L"{4d36e968-e325-11ce-bfc1-08002be10318}";
	const wchar_t* className = L"Display";
	const wchar_t* driver = L"pci.sys";
	DWORD configFlags = 0x00000000;
	std::wstring wideNewHardwareID(newHardwareID.begin(), newHardwareID.end());

	// Set HardwareID
	result = RegSetValueExW(hKey, L"HardwareID", 0, REG_SZ,
		(const BYTE*)wideNewHardwareID.c_str(),
		(wideNewHardwareID.size() + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set CompatibleIDs (as multi-string)
	result = RegSetValueExW(hKey, L"CompatibleIDs", 0, REG_MULTI_SZ,
		(const BYTE*)wideNewHardwareID.c_str(),
		(wideNewHardwareID.size() + 2) * sizeof(wchar_t)); // +2 for double null-termination
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set Driver
	result = RegSetValueExW(hKey, L"Driver", 0, REG_SZ,
		(const BYTE*)driver,
		(wcslen(driver) + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set ConfigFlags
	result = RegSetValueExW(hKey, L"ConfigFlags", 0, REG_DWORD,
		(const BYTE*)&configFlags, sizeof(DWORD));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set ClassGUID
	result = RegSetValueExW(hKey, L"ClassGUID", 0, REG_SZ,
		(const BYTE*)classGUID,
		(wcslen(classGUID) + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set Class
	result = RegSetValueExW(hKey, L"Class", 0, REG_SZ,
		(const BYTE*)className,
		(wcslen(className) + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	RegCloseKey(hKey);
	return true;
}

// Function to get current HWID from registry
std::string GetCurrentHWID(const std::wstring& registryPath) {
	HKEY hKey;
	std::string hwid = "Unknown";

	LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, registryPath.c_str(), 0, KEY_QUERY_VALUE, &hKey);
	if (result == ERROR_SUCCESS) {
		wchar_t buffer[256];
		DWORD bufferSize = sizeof(buffer);
		DWORD type;
		result = RegQueryValueExW(hKey, L"HardwareID", 0, &type, (BYTE*)buffer, &bufferSize);
		if (result == ERROR_SUCCESS && type == REG_SZ) {
			hwid = std::string(buffer, buffer + bufferSize / sizeof(wchar_t) - 1);
		}
		RegCloseKey(hKey);
	}
	return hwid;
}

// Variables for ImGui
std::wstring registryPath;
std::string deviceID;
bool gpuFound = FindGPURegistryPath(registryPath, deviceID);
std::string currentHWID = gpuFound ? GetCurrentHWID(registryPath) : "No GPU found";
std::string newHWID = GenerateRandomHWID(); // Auto-generate HWID on startup
std::string appliedHWID = ""; // Store HWID after applying
bool showMessage = false;
std::string message;



//-------------------------------------------------------------------------------------------------------------------------------------------
//RecRoom Checker
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to get the executable path of a process by its PID
std::wstring GetProcessPath(DWORD processId) {
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (!process) {
		return L"";
	}
	wchar_t path[MAX_PATH];
	DWORD size = MAX_PATH;
	if (GetModuleFileNameExW(process, nullptr, path, size)) {
		CloseHandle(process);
		return std::wstring(path);
	}
	CloseHandle(process);
	return L"";
}

// Function to check if Rec Room is installed
bool IsRecRoomInstalled(std::string& installPath) {
	char username[256];
	DWORD size = sizeof(username);
	if (!GetEnvironmentVariableA("USERNAME", username, size)) {
		std::cerr << "Failed to get USERNAME environment variable" << std::endl;
		return false;
	}

	installPath = std::string("C:\\Users\\") + username + "\\AppData\\Local\\Programs\\recroom-launcher";
	bool exists = std::filesystem::exists(installPath);
	std::cout << "Checking install path: " << installPath << " -> " << (exists ? "Exists" : "Does not exist") << std::endl;
	return exists;
}

// Function to check if RecRoom_WindowsPlatforms.exe is running
bool IsRecRoomRunning() {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		std::cerr << "Failed to create process snapshot. Error: " << GetLastError() << std::endl;
		return false;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	if (!Process32FirstW(snapshot, &pe32)) {
		std::cerr << "Failed to get first process. Error: " << GetLastError() << std::endl;
		CloseHandle(snapshot);
		return false;
	}

	bool found = false;
	const std::wstring targetExe = L"RecRoom_WindowsPlatforms.exe";
	std::string installPath;
	IsRecRoomInstalled(installPath); // Get the expected install path
	std::wstring wInstallPath(installPath.begin(), installPath.end());

	do {
		if (pe32.szExeFile[0] == L'\0') {
			continue; // Skip empty process names
		}

		std::wcout << L"Checking process: " << pe32.szExeFile << L" (PID: " << pe32.th32ProcessID << L")" << std::endl;

		if (_wcsicmp(pe32.szExeFile, targetExe.c_str()) == 0) {
			// Verify the process path to ensure it's the correct instance
			std::wstring processPath = GetProcessPath(pe32.th32ProcessID);
			if (!processPath.empty() && processPath.find(wInstallPath) != std::wstring::npos) {
				found = true;
				std::wcout << L"MATCH FOUND: Process '" << pe32.szExeFile << L"' (PID: " << pe32.th32ProcessID << L") matches target '" << targetExe << L"'" << std::endl;
				break; // Exit loop on first valid match
			}
			else {
				std::wcout << L"Ignoring process '" << pe32.szExeFile << L"' from different path: " << processPath << std::endl;
			}
		}
	} while (Process32NextW(snapshot, &pe32));

	CloseHandle(snapshot);

	std::cout << "Rec Room process check complete: " << (found ? "Running" : "Not Running") << std::endl;
	return found;
}

// Check Rec Room status
std::string installPath;
bool isInstalled = IsRecRoomInstalled(installPath);
bool isRunning = IsRecRoomRunning();



//-------------------------------------------------------------------------------------------------------------------------------------------
//ImGui Handling And Functions
//-------------------------------------------------------------------------------------------------------------------------------------------



extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU)
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter);
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGui::Begin(
		"Insanity Cheat https://discord.gg/projectunban",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);



	//-------------------------------------------------------------------------------------------------------------------------------------------
	//ImGui Popup Function On Start Up
	//-------------------------------------------------------------------------------------------------------------------------------------------



	// ImGui pop-up logic
	static bool show_popuprizz = true; // Show on startup
	if (show_popuprizz) {
		ImGui::OpenPopup("WARNING");
	}

	if (ImGui::BeginPopupModal("WARNING", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("This is still in heavy beta and only works for AMD CPU's. Intel support may be a future thing.");
		ImGui::Text("Not every feature works for everyone. Some features have not even been coded yet.");
		ImGui::Text("GitHub Page: https://github.com/AlwaysAError/Insanity");
		ImGui::Text("Discord Server: https://discord.gg/projectunban");
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) {
			show_popuprizz = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}



	//-------------------------------------------------------------------------------------------------------------------------------------------
	//Tab 1 ImGui Elemnts
	//-------------------------------------------------------------------------------------------------------------------------------------------



	// Rest of your tabbed interface (unchanged)
	if (ImGui::BeginTabBar("MainTabBar")) {
		// ... (your existing tab code remains unchanged)
	}

	//TOP TAB HIDER
	if (ImGui::BeginTabItem("HIDE TOP TAB")) {
		ImGui::Text("The top tab is now hidden. Press another top tab to open it.");
		ImGui::EndTabItem();
	}
	//Data Tab
	if (ImGui::BeginTabItem("Data Tab")) {
		ImGui::Text("You Are Now In The Data Tab");
		ImGui::Text("   ");
		ImGui::Text("Version");
		ImGui::Text("Your are using version [BETA]");
		ImGui::Text("   ");
		ImGui::Text("Secuirty");
		ImGui::Text("Insanity is up to date for RecRoom version [Standing Slide]");
		ImGui::Text("Insanity EAC bypass is [working]");
		ImGui::Text("Insanity Referee bypass is [ERROR Offset 1fef523ac8d00b24f994244693d751c9]");
		ImGui::Text("   ");
		ImGui::Text("Injection");
		ImGui::Text("Did the injector activate [ERROR]");
		ImGui::Text("Injection success API event received [ERROR]");
		ImGui::Text("   ");
		ImGui::Text("RecRoom Data");
		ImGui::Text("Installation Path: %s", installPath.c_str());
		ImGui::Text("Installed: %s", isInstalled ? "Yes" : "No");
		ImGui::Text("Running: %s", isRunning ? "Yes" : "No");
		ImGui::Text("If it's saying yes to running while you don't have RecRoom open then it's currently spying on you");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}
	//System Info Tab
	if (ImGui::BeginTabItem("System Info")) {
		// Buffer to hold the system info
		static std::string systemInfo = GetFormattedSystemInfo();
		static char buffer[8192]; // Increased size for more data
		strncpy_s(buffer, systemInfo.c_str(), sizeof(buffer) - 1);
		buffer[sizeof(buffer) - 1] = '\0'; // Ensure null-termination

		// Create a read-only multiline text box
		ImGui::Text("System Information:");
		ImGui::InputTextMultiline("##SystemInfo", buffer, sizeof(buffer),
			ImVec2(-1.0f, ImGui::GetTextLineHeight() * 20), // Increased height for more data
			ImGuiInputTextFlags_ReadOnly); // Read-only flag

		// Add a refresh button to update the info
		if (ImGui::Button("Refresh")) {
			systemInfo = GetFormattedSystemInfo(); // Update the info
			strncpy_s(buffer, systemInfo.c_str(), sizeof(buffer) - 1);
			buffer[sizeof(buffer) - 1] = '\0';
		}

		// Add a copy button
		ImGui::SameLine();
		if (ImGui::Button("Copy to Clipboard")) {
			ImGui::SetClipboardText(buffer);
		}
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");

		ImGui::EndTabItem();
	}
	//Spoofing Tab
	if (ImGui::BeginTabItem("Spoofing")) {
		ImGui::Text("You Are Now In The Tab For Spoofing And Cleaning");
		ImGui::Text("   ");
		

		if (ImGui::Button("Spoof MachineGuid")) {
			std::wstring newGuid = GenerateRandomGUID();
			statusMessage = SetMachineGuid(newGuid);
			currentGuid = GetMachineGuid(); // Refresh displayed GUID
		}
		ImGui::Text("Current MachineGuid: %ls", currentGuid.c_str());

		// ImGui button callback
		void RenderImGui(); {
			if (ImGui::Button("Clean RecRoom File's")) {
				PerformCleanup();
			}
		}
		ImGui::Text("After pressing the Clean RecRoom File's button wait 5-10 seconds");

		// Button to trigger network reset
		if (ImGui::Button("Clean IP and DNS")) {
			RunNetworkReset();
		}
		ImGui::Text("After pressing the Clean IP and DNS button please wait for 10 seconds and ignore any CMD pop ups");
		ImGui::Text("   ");






		ImGui::Text("GPU Device ID: %s", deviceID.c_str());
		ImGui::Text("Current GPU HWID: %s", currentHWID.c_str());

		// Button to generate new HWID
		if (ImGui::Button("Generate New HWID")) {
			newHWID = GenerateRandomHWID(); // Generate new random HWID
		}
		ImGui::Text("Generated HWID: %s", newHWID.c_str());

		// Button to spoof HWID
		if (ImGui::Button("Spoof HWID")) {
			if (!gpuFound) {
				message = "No GPU found. Cannot spoof HWID.";
				showMessage = true;
				appliedHWID = "";
			}
			else if (SpoofGPU(registryPath, newHWID)) {
				message = "HWID spoofed successfully!";
				currentHWID = GetCurrentHWID(registryPath); // Refresh current HWID
				appliedHWID = currentHWID; // Store applied HWID
			}
			else {
				message = "Failed to spoof HWID. Ensure admin privileges.";
				appliedHWID = "";
			}
			showMessage = true;
		}


		// Display applied HWID below button
		if (!appliedHWID.empty()) {
			ImGui::Text("New HWID Applied: %s", appliedHWID.c_str());
		}
		if (showMessage) {
			ImGui::Text("%s", message.c_str());
		}

		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}

	

	//-------------------------------------------------------------------------------------------------------------------------------------------
	//Tab 2 ImGui Elements
	//-------------------------------------------------------------------------------------------------------------------------------------------



	// Normals Tab
	if (ImGui::BeginTabBar("MainTabBar")) {
		if (ImGui::BeginTabItem("Normals Tab")) {
			ImGui::Text("This tab offers simple cheats.");
			ImGui::Text("   ");
			static bool godmode = false;
			ImGui::Checkbox("GodMode", &godmode);
			static bool agodmode = false;
			ImGui::Checkbox("Advanced GodMode", &agodmode);
			ImGui::Text("   ");
			static bool makerpend = false;
			ImGui::Checkbox("Unlock Makerpen", &makerpend);
			ImGui::Text("Unlock full makerpen permissions in any room");
			ImGui::Text("   ");
			static bool aimbot = false;
			ImGui::Checkbox("AimBot", &aimbot);
			static bool psilent = false;
			ImGui::Checkbox("Make AimBot P Silent", &psilent);
			static bool triggerbot = false;
			ImGui::Checkbox("TriggerBot", &triggerbot);
			static float value1 = 0.0f;
			ImGui::SliderFloat("TriggerBot Delay", &value1, 0.0f, 10.0f, "%.1f");
			static bool nospread = false;
			ImGui::Checkbox("NoSpread", &nospread);
			static bool NoRecoil = false;
			ImGui::Checkbox("NoRecoil", &NoRecoil);
			ImGui::Text("   ");
			static bool InfiniteAmmo = false;
			ImGui::Checkbox("Infinite Ammo", &InfiniteAmmo);
			static bool rapidfire = false;
			ImGui::Checkbox("Rapid Fire", &rapidfire);
			ImGui::Text("   ");
			ImGui::Button("Teleport To Crosshair");
			ImGui::Text("   ");
			static bool massivemic = false;
			ImGui::Checkbox("Massive Mic", &massivemic);
			ImGui::Text("Makes every player hear what you are saying no matter were you are");
			ImGui::EndTabItem();
		}
		// Tab Exploits Tab
		if (ImGui::BeginTabItem("Exploits Tab")) {
			ImGui::Text("This tab offers game exploit. Exploits may cause your account to be banned!");
			ImGui::Text("   ");
			static bool AntiAim = false;
			ImGui::Checkbox("AntiAim", &AntiAim);
			ImGui::Text("Spin Speed");
			static float value2 = 0.0f;
			ImGui::SliderFloat("Spin Speed", &value2, 0.0f, 100.0f, "%.1f");
			ImGui::Text("Pitch");
			static float value3 = 0.0f;
			ImGui::SliderFloat("Pitch", &value3, 0.0f, 90.0f, "%.1f");
			ImGui::Text("   ");
			static bool Doubletap = false;
			ImGui::Checkbox("Double Tap", &Doubletap);
			ImGui::Text("   ");
			ImGui::Text("Select A Bind For TimeStop");

			static const char* items[] = { "M1+M2", "P", "CONTROL", "Side Button" };
			static int current_item1 = 0;

			ImGui::Combo("TimeStop", &current_item1, items, IM_ARRAYSIZE(items));

			ImGui::Text("   ");
			ImGui::Text("Perm Kicker");
			ImGui::Button("Kick All Hosts");
			ImGui::Button("Kick All Mods");
			ImGui::Button("Kick All Co Onwers");
			ImGui::Button("Kick Room Owner");
			ImGui::Button("Kick All With Perms");
			ImGui::Button("Kick All With No Perms");
			ImGui::Button("Kick Everyone But Room Owner");
			ImGui::Text("   ");
			ImGui::Text("Flying");
			static bool NoClip = false;
			ImGui::Checkbox("NoClip", &NoClip);
			static float value4 = 0.0f;
			ImGui::SliderFloat("NoClip Speed", &value4, 0.0f, 10000.0f, "%.1f");
			static bool CreatorFly = false;
			ImGui::Checkbox("CreatorFly", &CreatorFly);
			static float value5 = 0.0f;
			ImGui::SliderFloat("CreatorFly Speed", &value5, 0.0f, 10000.0f, "%.1f");
			ImGui::Text("   ");
			ImGui::Text("Message To Server.");
			ImGui::Text("Sends the input message to the whole server");
			static char buffer[256] = "";
			ImGui::InputText(" ", buffer, sizeof(buffer));

			ImGui::Text("You typed: %s", buffer);
			ImGui::Button("Send Message");
			ImGui::EndTabItem();
		}
		// Tab Exetreme Exploits
		if (ImGui::BeginTabItem("Exetreme Exploits")) {
			ImGui::Text("WARNING THESE EXPLOITS MAY CAUSE A INSTANT BAN!");
			ImGui::Text("   ");
			ImGui::Text("Lobby Stuff");
			ImGui::Button("Soft Lock Lobby");
			ImGui::Button("Crash Lobby");
			ImGui::Button("DDoS Lobby . MAY TAKE YOUR INTERNET OFFLINE FOR 1 MINUTE");
			ImGui::Text("   ");
			ImGui::Text("Head Display");

			static const char* items[] = { "Vmod Text", "Mod Text", "Dev Text", "BroadCasting Text" };
			static int current_item = 0;

			ImGui::Combo("Select Option", &current_item, items, IM_ARRAYSIZE(items));

			ImGui::Text("Selected: %s", items[current_item]);
			ImGui::Text("   ");
			ImGui::Text("Lobby Host Exploits");
			ImGui::Button("Become lobby host");
			ImGui::Text("Forces RecRoom to make you the lobby host as the game is P2P");
			ImGui::Button("Become room owner");
			ImGui::Text("Gives you ownership of room for the lobby you are in. THE GREAT RETURN OF 2022 CHEATS");
			ImGui::Button("Makerpen all");
			ImGui::Text("Gives everyone full makerpen permissions");
			ImGui::Button("Swastika-fi lobby");
			ImGui::Text("Displays a swastika on everyone's screen");
			ImGui::EndTabItem();
		}
		// Tab Visuals
		if (ImGui::BeginTabItem("Visuals")) {
			ImGui::Text("This tab is used for visual elements.");
			ImGui::Text("   ");
			static bool pickupesp = false;
			ImGui::Checkbox("Pick Up ESP", &pickupesp);
			static bool Playeresp = false;
			ImGui::Checkbox("Player ESP", &Playeresp);
			ImGui::EndTabItem();
		}
		// Tab Players
		if (ImGui::BeginTabItem("Players")) {
			ImGui::Text("This tab is for targetting select players.");
			ImGui::Text("   ");
			ImGui::Text("We are not detecting any players in your lobby besides you!");
			ImGui::EndTabItem();
		}
		// Tab Lua Executor
		if (ImGui::BeginTabItem("Lua Executor")) {
			ImGui::Text("Our cheat not good enough or just want more features? This tab will show all your");
			ImGui::Text("Lua scripts you have added to the configs folder. Use the configs folder button");
			ImGui::Text("below to access the configs folder were you can add your Lua scripts.");
			ImGui::Text("   ");
			ImGui::Button("Open Lua Scripts Folder");
			ImGui::Text("   ");
			ImGui::Text("You currently have no Lua scripts.");
			ImGui::EndTabItem();
		}
		// Tab Prefab Swapper
		if (ImGui::BeginTabItem("Prefab Swapper")) {
			ImGui::Text("Use this tab to swap game prefabs around.");
			ImGui::Text("A example would be swapping the camera around for a Laser Shotgun");
			ImGui::Text("   ");
			ImGui::Text("Recommended Prefab List");
			ImGui::Text("https://github.com/AlwaysAError/RecRoom-Prefabs-2025");
			ImGui::Text("Why do we recommend this prefab list? It's the biggest prefab list to ever exist and have some prefabs never seen before");
			ImGui::Text("   ");
			static char buffertext3[256] = "";
			ImGui::InputText(" ", buffertext3, sizeof(buffertext3));
			ImGui::Text("Prefab to swap");
			ImGui::Text("   ");
			static char buffertext5[256] = "";
			ImGui::InputText("  ", buffertext5, sizeof(buffertext5));
			ImGui::Text("Swap first input prefab to");
			ImGui::EndTabItem();
		}
		// Tab Misc Options
		if (ImGui::BeginTabItem("Misc Options")) {
			ImGui::Text("This tab offers options which did not fit into other tabs.");
			ImGui::Text("   ");
			ImGui::Text("Protections");
			static bool antikick = false;
			ImGui::Checkbox("Anti Kick", &antikick);
			static bool antiban = false;
			ImGui::Checkbox("Anti Ban", &antiban);
			static bool antivoiceban = false;
			ImGui::Checkbox("Anti Voice Ban", &antivoiceban);
			static bool antisoftlock = false;
			ImGui::Checkbox("Anti Soft Lock", &antisoftlock);
			static bool anticrash = false;
			ImGui::Checkbox("Anti Crash", &anticrash);
			static bool antilag = false;
			ImGui::Checkbox("Anti Lag", &antilag);
			static bool hideip = false;
			ImGui::Checkbox("Hide IP", &hideip);
			ImGui::Text("   ");
			ImGui::Text("Slash Commands");
			ImGui::Text("Slash commands allow you to execute cheats through the game chat and in the future");
			ImGui::Text("will allow you to let other players use these slash commands");

			static char buffer[256] = "";
			ImGui::InputText(" ", buffer, sizeof(buffer));

			ImGui::Text("What you enter into the above text box will trigger what you choose below");

			{
				static bool check1 = false;
				static bool check2 = false;
				static bool check3 = false;

				if (ImGui::Checkbox("Request Kick Events (kicks as many people as possible)", &check1))
				{
					check2 = false;
					check3 = false;
				}

				if (ImGui::Checkbox("Makerpen lobby (gives everyone makerpen)", &check2))
				{
					check1 = false;
					check3 = false;
				}

				if (ImGui::Checkbox("Coming Soon...", &check3))
				{
					check1 = false;
					check2 = false;
				}
			}

			ImGui::Text("   ");
			ImGui::Text("   ");
			ImGui::Text("   ");
			ImGui::Text("Test Boxes");

			static bool isToggleOn = false;

			if (ImGui::Checkbox("Show Options", &isToggleOn)) {

			}

			if (isToggleOn) {

				static int currentItembox = 0;
				const char* itemsbox[] = { "Option 1", "Option 2", "Option 3", "Option 4" };
				ImGui::Combo("Select Option", &currentItembox, itemsbox, IM_ARRAYSIZE(itemsbox));
			}

			ImGui::EndTabItem();
		}
		// Tab Info/Credits
		if (ImGui::BeginTabItem("Info And Credits")) {
			ImGui::Text("A tab for the credits and any information you may need to know.");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Credits");
			ImGui::Text("   ");
			ImGui::Text("extremeblitz_ on Discord. Main coder");
			ImGui::Text("Cazz on YouTube. Helped with some ImGui elements");
			ImGui::Text("FatVirginCheater on Youtube. Helped with some features");
			ImGui::Text("spinmaster on Discord. Helped with some features");
			ImGui::Text("cheatzking on Discord. Helped EAC and Referee bypass");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Update Logs");
			ImGui::Text("   ");
			ImGui::Text("Updates? There are no updates your using the beta version");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Help/Support");
			ImGui::Text("   ");
			ImGui::Text("If the bypass is out of date will I get banned?");
			ImGui::Text("No, our cheat is designed so that if injection fails it will be untracable meaning you won't get");
			ImGui::Text("detected and banned.");
			ImGui::Text("   ");
			ImGui::Text("What are the detection rates?");
			ImGui::Text("There are no official detection rates as there are many different features but anything in the");
			ImGui::Text("normals tab will never get detected");
			ImGui::Text("   ");
			ImGui::Text("Is this a virus?");
			ImGui::Text("Put it simply no");
			ImGui::Text("   ");
			ImGui::Text("Could I get unbanned if I get banned using this cheat?");
			ImGui::Text("The best answer is no but if you want a more detailed answer then allow me to give you just that.");
			ImGui::Text("RecRoom support for ban appeals is automated to give you the response of no. You won't get unbanned but");
			ImGui::Text("if your a creator or have made RecRoom Inc a lot of money they will make a human respond to your");
			ImGui::Text("appeal meaning then there is a chance of being unbanned so if your a creator making RecRoom Inc money");
			ImGui::Text("then yes you could be unbanned but if your just the average Joe then no you won't be unbanned.");
			ImGui::Text("   ");
			ImGui::Text("Can I contribute code to the cheat?");
			ImGui::Text("YES! We allow contributions to the cheat to improve the user expierance. If you want to contribute");
			ImGui::Text("to our project please contact the main coder listed in the Credits section. You will also be added");
			ImGui::Text("to the credits list if we decide to use your code. MAKE SURE YOUR CODE IS IN C++ OR LUA!!");
			ImGui::Text("   ");
			ImGui::Text("How long will support for this cheat last?");
			ImGui::Text("How ever long the main coder listed in the credits will continue it for.");
			ImGui::Text("   ");
			ImGui::Text("What is NameLock?");
			ImGui::Text("NameLock is a special type of ban RecRoom gives to people which have been caught ban evading or cheating.");
			ImGui::Text("NameLock only applies to PC users and is not enforced on any other devices besides PlayStation.");
			ImGui::Text("   ");
			ImGui::Text("Does Insanity offer a RecRoom spoofer to get around bans like NameLock?");
			ImGui::Text("Were working on one!!!");
			ImGui::Text("   ");
			ImGui::Text("What should I do if features are not working?");
			ImGui::Text("Please inform us of the feature which is not working in our Discord server.");
			ImGui::Text("   ");
			ImGui::Text("The cheat is launching but it's not working with RecRoom?");
			ImGui::Text("This can be because of a couple different factors. See the list below for help.");
			ImGui::Text("1. The exe has not been run as admin.");
			ImGui::Text("2. The bypass is out of date or is broken for some reason.");
			ImGui::Text("3. The cheat is not detecting the running process RecRoom.");
			ImGui::Text("4. Windows or a installed anti virus may be blocking the cheat from doing what is loves to do.");
			ImGui::Text("5. Your system does not meet the requirements to run Insanity.");
			ImGui::Text("   ");
			ImGui::Text("Lua support");
			ImGui::Text("We support Lua scripts that have been coded to work with Cheat Engine.");
			ImGui::Text("   ");
			ImGui::EndTabItem();

		}
		
	}

	ImGui::End();
}

//-------------------------------------------------------------------------------------------------------------------------------------------