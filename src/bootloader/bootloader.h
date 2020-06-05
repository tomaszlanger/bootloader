#pragma once
#include "sigslot.h"
#include "serial/serial.h"
#include <mutex>
#include <atomic>

namespace bootloader {

using namespace std;
using namespace serial;

enum class STCmds {
	/* this is used by bootloader to determine the baudrate */
	INIT = 0x7F,
	/* Gets the version and the allowed commands supported
		* by the current version of the bootloader */
	GET = 0x00,
	/* Gets the bootloader version and the Read Protection
		* status of the Flash memory */
	GET_PROT = 0x01,
	/* Gets the chip ID */
	GET_ID = 0x02,
	/* Reads up to 256 bytes of memory starting from an
		* address specified by the application */
	READ = 0x11,
	/* Jumps to user application code located in the internal
		* Flash memory or in SRAM */
	GO = 0x21,
	/* Writes up to 256 bytes to the RAM or Flash memory starting
		* from an address specified by the application */
	WRITE = 0x31,
	/* Erases from one to all the Flash memory pages */
	ERASE = 0x43,
	/* Erases from one to all the Flash memory pages using
		* two byte addressing mode (available only for v3.0 usart
		* bootloader versions and above). */
	EXT_ERASE = 0x44,
	/* Enables the write protection for some sectors */
	WR_PROTECT = 0x63,
	/* Disables the write protection for all Flash memory sectors */
	WR_UNPROTECT = 0x73,
	/* Enables the read protection */
	RD_PROTECT = 0x82,
	/* Disables the read protection */
	RD_UNPROTECT = 0x92,	
	/* force bms to generate randmo value */
	RANDOMIZE = 0x93
};

/* response codes*/
enum class STResps {
	/* command accepted */
	ACK = 0x79,
	/* command discarded */
	NACK = 0x1F,
};

/* special erase mode for normal erase command */
enum class STEraseMode {
	/* erase all sectors */
	GLOBAL = 0xff,
};

/* special erase mode for normal erase command */
enum class STExtendedEraseMode {
	/* erase all sectors */
	GLOBAL = 0xffff,
	/* erase bank 1 */
	BANK1 = 0xfffe,
	/* erase bank 2 */
	BANK2 = 0xfffd
};

enum class DeviceCommunicationCommands {
	ENABLE_PRODUCTION_MODE = 0x0001,
	DISABLE_PRODUCTION_MODE = 0x0002,
	JUMP_TO_BOOTLOADER = 0x0003
};

class STBootException : public std::exception {
	// Disable copy constructors
	STBootException& operator=(const STBootException&);
	std::string e_what_;
public:
	STBootException(const char *description) {
		std::stringstream ss;
		ss << "STBootException " << description << " failed.";
		e_what_ = ss.str();
	}
	STBootException(const STBootException& other) : e_what_(other.e_what_) {}
	virtual ~STBootException() throw() {}
	virtual const char* what() const throw () {
		return e_what_.c_str();
	}
};

class STBootProgress {
public:
	typedef std::function<void(STBootProgress&)> callbackFnc;
	/* total number of bytes */
	int32_t bytesTotal;
	/* number of bytes processed */
	int32_t bytesProcessed;

	STBootProgress(int32_t bProcessed, int32_t bTotal, callbackFnc callback) :
		bytesProcessed(bProcessed), bytesTotal(bTotal), progressUpdateCallback(callback) {};
	~STBootProgress() {};

	void update(int32_t bProcessed, int32_t bTotal) {
		bytesProcessed = bProcessed;
		bytesTotal = bTotal;
		progressUpdateCallback(*this);
	}

private:
	callbackFnc progressUpdateCallback;
};

using namespace sigslot;

constexpr uint16_t SEED_OFFSET = 96U;

class bootloader : public has_slots<multi_threaded_global> {

public:
	bootloader();
	~bootloader();

	void cancelOperation();
	void findSerialPorts(std::list<std::string>& serialPortsList);
	void open(string portName);
	bool isOpen();
	void close();
	void Initialize();
	void Unprotect();
	void ReadMemory(uint32_t address, unsigned char* buf, int32_t offset, int32_t size, STBootProgress& p);
	void WriteMemory(uint32_t address, unsigned char* buf, int32_t offset, int32_t size, STBootProgress& p);
	void ErasePage(uint32_t pageNumber);	
	void GlobalErase();
	void Jump(uint32_t address);
	void sendCommunicationCommand(DeviceCommunicationCommands command);
	void Authorize();
	void Get();
	void GetID();
	void Read(uint32_t address, unsigned char* buf, int32_t offset, int32_t length);
	void Go(uint32_t address);
	void Write(uint32_t address, unsigned char* data, int32_t offset, int32_t length);
	void Erase(uint32_t pageNumber);
	void EraseSpecial(STEraseMode mode);
	void ExtendedErase(uint32_t pageNumber);
	void ExtendedEraseSpecial(STExtendedEraseMode mode);
	void WriteUnprotect();
	void ReadUnprotect();

	static uint16_t ComputeChecksum(const unsigned char* data, int32_t offset, int32_t count);
	static uint16_t ComputeChecksumWithPrecedingByte(const unsigned char* data, int32_t offset, int32_t count, uint8_t lastByte);
	static uint16_t ComputeChecksum(uint16_t crc, uint8_t byte);

	// Signals
	signal1<std::reference_wrapper<std::string>, multi_threaded_global> sendBootloaderMessage;

	string Version;
	USHORT ProductID;

private:
	size_t read(uint8_t *buffer, size_t size);
	size_t read(std::string &buffer, size_t size);
	static void encrypt(uint8_t* source, int16_t* destination, uint8_t length, int16_t n, int16_t e);
	static void decrypt(int16_t* source, uint8_t* destination, uint8_t length, int16_t n, int16_t d);

	const int16_t cryptoKeyBmsN = 2651;
	const int16_t cryptoKeyBmsE = 7;
	const int16_t cryptoKeyBmsD = 2743;

	const int16_t cryptoKeyHostN = 1243;
	const int16_t cryptoKeyHostE = 3;
	const int16_t cryptoKeyHostD = 1867;

	serial::Serial sp;
	list<STCmds> Commands;
	std::atomic<bool> cancel;
};

}  // namespace bootloader
