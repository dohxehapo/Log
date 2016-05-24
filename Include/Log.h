// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Module:		Logger
// Author:		Anton Egorov
// Description:	Log library is the one-header-only library that provides
//				the log of any text to the console and to the given file
//				simultaneously. Thread-safety is guaranteed (or at least
//				should be). Also the VersionRetriever class is provided
//				to acquire product version from file (used for the welcome
//				logger message).
//
//
// Usage example:
//
// void SomeFunc()
// {
//		LOG("SomeFunc()");
//		...
//		echo("Hello, world! Result is: ", someVariable, ".");
//		...
// }
//
// void main()
// {
//		LOGINIT("Log.log", "MyProduct", "ThisFile.exe");
//		...
//		SomeFunc();
//		...
//		LOGDISPOSE();
// }
//
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-



#pragma once

#ifndef LOG_H
#define LOG_H



// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// System dependencies
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-



#include <Windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <iostream>


#pragma comment(lib, "version.lib") // For version retriever



// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Logger global aliases (defines)
// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-



// Initializes logger. Should be called first of all. Writes the welcome message to the log
// Params:
// [in] std::string pLogFileName	- log file name (ex. "Logs\myLog.log"). Warning: Directory should exist
// [in] std::string pProductName	- product name used in the welcome message
// [in] std::string pFilePath		- name of the file to take version from. Can be empty
#define LOGINIT(pLogFileName, pProductName, pFilePath) Log::Log<void>::Init(pLogFileName, pProductName, pFilePath)

// Disposes logger. Writes the bye message to the log
#define LOGDISPOSE Log::Log<void>::Dispose()

// Macros that should be called before any echo-calls. Initializes the log object for the current function
// Params:
// [in] std::string pPrefix	- function name
#define LOG(pPrefix) ::Log::Log<void> log(pPrefix)

// Logs all given args
#define echo log.Echo



namespace Log
{

	// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	// Version Retriever
	// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-



	// Returns version of the product from the file with the given name
	// Params:
	// [in]  std::string pFilePath				- file to acquire version from
	// [out] std::string & strProductVersion	- string that contains version if any
	// Returns:
	// true		- version successfully acquired
	// false	- failed to get version
	static bool GetProductVersion(std::string pFilePath, std::string &strProductVersion)
	{
		// Allocate a block of memory for the version info
		unsigned long dummy;
		unsigned long dwSize = GetFileVersionInfoSize(pFilePath.c_str(), &dummy);
		if (dwSize == 0)
		{
			//TRACE("GetFileVersionInfoSize failed with error %d\n", GetLastError());
			DWORD err = GetLastError();
			return false;
		}
		std::vector<unsigned char> data(dwSize);

		// Load the version info
		if (!GetFileVersionInfo(pFilePath.c_str(), NULL, dwSize, &data[0]))
		{
			//TRACE("GetFileVersionInfo failed with error %d\n", GetLastError());
			return false;
		}

		// Get the name and version strings
		LPVOID ptrVersion = NULL;
		unsigned int iVersion = 0;

		VerQueryValue(&data[0], ("\\"), &ptrVersion, &iVersion);
		if (iVersion == 0)
			return false;

		VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)ptrVersion;
		if (verInfo->dwSignature != 0xfeef04bd)
			return false;

		// Doesn't matter if you are on 32 bit or 64 bit,
		// DWORD is always 32 bits, so first two revision numbers
		// come from dwFileVersionMS, last two come from dwFileVersionLS
		strProductVersion.clear();
		strProductVersion.
			append(std::to_string((verInfo->dwProductVersionMS >> 16) & 0xffff).append(".").
				append(std::to_string((verInfo->dwProductVersionMS >> 0) & 0xffff)).append(".").
				append(std::to_string((verInfo->dwProductVersionLS >> 16) & 0xffff)).append(".").
				append(std::to_string((verInfo->dwProductVersionLS >> 0) & 0xffff)));

		return true;
	}



	// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	// Logger itself
	// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-



	// Class that logs any text to the console and the given file
	template <typename T>
	class Log
	{
	public:

		// Logger constructor that should be called before any echo-calls. Initializes the log object for the current function
		// Params:
		// [in] std::string pPrefix	- function name
		Log(std::string pPrefix) { m_prefix = pPrefix; }

		// Logger destructor
		virtual ~Log() { }


		// Logs all given args
		// Params:
		// [in] Args pArgs	- bag of args to log
		template <typename ... Args>
		void Echo(Args ... pArgs)
		{
			if (!s_isInitialized)
				return;

			EchoDateTime();
			Echo_Internal(pArgs ...);
		}


		// Initializes logger. Should be called first of all. Writes the welcome message to the log
		// Params:
		// [in] std::string pLogFileName	- log file name (ex. "Logs\myLog.log"). Warning: Directory should exist
		// [in] std::string pProductName	- product name used in the welcome message
		// [in] std::string pFilePath		- name of the file to take version from. Can be empty
		static void Init(std::string pLogFileName, std::string pProductName, std::string pFilePath)
		{
			bool gotVersion = false;
			std::string version = "";

			s_logMutex.lock();
			{
				if (pProductName.empty())
					s_productName = "Logging"; // To write something like "Logging started"
				else
					s_productName = pProductName;
				s_logFileName = pLogFileName;

				if ((!pFilePath.empty()) && (!pProductName.empty())) // Don't acquire version if the product name is not provided
					gotVersion = GetProductVersion(pFilePath, version);

				s_isInitialized = true;
			}
			s_logMutex.unlock();

			// Print welcome message

			LOG("");

			echo("");
			echo("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
			if (gotVersion)
				echo(s_productName + std::string(" v.").append(version).append(" started."));
			else
				echo(s_productName + std::string(" started."));
			echo("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
			echo("");
		}

		// Disposes logger. Writes the bye message to the log
		static void Dispose()
		{
			// Print bye message

			LOG("");

			echo("");
			echo("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
			echo(s_productName + std::string(" stopped."));
			echo("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-");
			echo("");

			s_logMutex.lock();
			{
				// Clear static vars
				s_productName.clear();
				s_logFileName.clear();

				s_isInitialized = false;
			}
			s_logMutex.unlock();
		}

		// Returns the DateTime string like this: "YYYY.MM.DD HH.MM.SS"
		static std::string GetDateTimeString()
		{
			// Get local time and convert it to struct 'tm'
			time_t Clock;
			time(&Clock);

			tm newTime;
			std::string dateTimeStr;
			dateTimeStr.resize(20);

			if (!localtime_s(&newTime, &Clock))
			{
				sprintf_s(&dateTimeStr[0],
					dateTimeStr.size(),
					"%04d.%02d.%02d %02d:%02d:%02d",
					1900 + newTime.tm_year,
					1 + newTime.tm_mon,
					newTime.tm_mday,
					newTime.tm_hour,
					newTime.tm_min,
					newTime.tm_sec);
			}
			else
				sprintf_s(&dateTimeStr[0], dateTimeStr.size(), "[unknown]");

			// Delete trailing \0 symbol
			dateTimeStr.erase(dateTimeStr.end() - 1);

			return dateTimeStr;
		}

	protected:

		std::string m_prefix;					// Function name (or whatever was sent to the LOG macros)

		volatile static bool s_isInitialized;	// Indicates whether the Logger class was initialized and is ready to log
		static std::string s_productName;		// Product name. Used in the welcome and bye messages
		static std::string s_logFileName;		// Name of the file to log into
		static std::mutex s_logMutex;			// Locker to provide thread-safety of Logger operations


		// Logs current date and time
		void EchoDateTime()
		{
			std::string dateTimeStr = GetDateTimeString();

			s_logMutex.lock();

			std::fstream m_logFile(s_logFileName, std::ios::app);
			if (m_prefix.empty())
			{
				std::cout << dateTimeStr << " > ";
				m_logFile << dateTimeStr << " > ";
			}
			else
			{
				std::cout << dateTimeStr << " > " << m_prefix << ": ";
				m_logFile << dateTimeStr << " > " << m_prefix << ": ";
			}
			m_logFile.close();
		}

		// Echoes the bag of arguments
		template <typename First, typename ... Rest>
		void Echo_Internal(First pFirst, Rest ... pRest)
		{
			std::fstream m_logFile(s_logFileName, std::ios::app);
			std::cout << pFirst;
			m_logFile << pFirst;
			m_logFile.close();

			Echo_Internal(pRest ...);
		}

		// Echoes the last of arguments from the args bag
		template <typename T>
		void Echo_Internal(T pPar)
		{
			std::fstream m_logFile(s_logFileName, std::ios::app);
			std::cout << pPar << std::endl;
			m_logFile << pPar << std::endl;
			m_logFile.close();

			s_logMutex.unlock();
		}

	}; // Log

	template<typename T>
	volatile bool Log<T>::s_isInitialized = false;

	template<typename T>
	std::string Log<T>::s_logFileName;

	template<typename T>
	std::string Log<T>::s_productName;

	template<typename T>
	std::mutex Log<T>::s_logMutex;

} // ns Log


#endif // LOG_H
