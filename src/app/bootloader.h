#pragma once
#include "sigslot.h"
#include "serial/serial.h"
#include <mutex>
#include <atomic>

namespace bootloader {



using namespace std;
using namespace serial;

enum class STCmds
{
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
	RD_UNPROTECT = 0x92
};

/* response codes*/
enum class STResps
{
	/* command accepted */
	ACK = 0x79,
	/* command discarded */
	NACK = 0x1F,
};

/* special erase mode for normal erase command */
enum class STEraseMode
{
	/* erase all sectors */
	GLOBAL = 0xff,
};

/* special erase mode for normal erase command */
enum class STExtendedEraseMode
{
	/* erase all sectors */
	GLOBAL = 0xffff,
	/* erase bank 1 */
	BANK1 = 0xfffe,
	/* erase bank 2 */
	BANK2 = 0xfffd
};

class STBootException : public std::exception
{
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

//using ReaderFunction = std::function<uint32_t(uint32_t)>;

class STBootProgress
{
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

class bootloader : public has_slots<multi_threaded_global> {

public:
	bootloader();
	~bootloader();

	void cancelOperation() {
		cancel = true;
	}

	// Signals
	signal1<std::reference_wrapper<std::string>, multi_threaded_global> sendBootloaderMessage;

	void findSerialPorts(std::list<std::string>& serialPortsList) {
		string currentOpenedPort = "";
		if (sp.isOpen()) {
			sp.close();
			currentOpenedPort = sp.getPort();
		}
		serialPortsList.clear();
		for (uint8_t i = 0; i < 255; i++) {
			sp.setPort("COM" + std::to_string(i));
			try {
				sp.open();
				sp.close();
				serialPortsList.push_back(sp.getPort());
			} catch (const std::exception &e) {}
			
		}
		if (!currentOpenedPort.empty()) {
			sp.setPort(currentOpenedPort);
			sp.open();
		}
	}

	/* open serial port */
	void open(string portName) {
		/* initialize serial port */
		sp.setPort(portName);
		sp.setParity(parity_even);
		sp.setBytesize(eightbits);
		/* open serial port */
		sp.open();
		/* discard input and output buffers */
		sp.flush();
	}
	bool isOpen() {
		return sp.isOpen();

	}
	/* close */
	void close() {
		/* close permitted? */
		if (sp.isOpen())
			sp.close();
	}
	/* initialize communication */
	void Initialize()
	{
		/* perform autobauding */
		//Init();
		/* get version and command list */
		Get();

		/* no support for get id? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::GET_ID) == Commands.end()) {
			throw STBootException("Command not supported");
		}
		
		/* get product id */
		GetID();
	}

	/* unprotect memory */
	void Unprotect() {
		/* no support for unprotect? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::WR_UNPROTECT) == Commands.end()) {
			throw STBootException("Command not supported");
		}
		/* no support for unprotect? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::RD_UNPROTECT) == Commands.end()) {
			throw STBootException("Command not supported");
		}

		ReadUnprotect();
		WriteUnprotect();
	}

	/* read memory */
	void ReadMemory(uint32_t address, unsigned char* buf, int32_t offset,
		int32_t size, STBootProgress& p) {
		/* number of bytes read */
		int32_t bread = 0;
		/* no support for read? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::READ) == Commands.end()) {
			throw STBootException("Command not supported");
		}

		cancel = false;

		/* data is read in chunks */
		while (size > 0 && !cancel) {
			/* chunk size */
			int32_t csize = min(size, 256);
			/* read a single chunk */
			Read(address, buf, offset, csize);

			/* update iterators */
			size -= csize; offset += csize; address += (uint32_t)csize;
			/* update number of bytes read */
			bread += csize;

			/* report progress */
			p.update(bread, 0);
		}

		/* throw exception if operation was cancelled */
		if (cancel)
			throw STBootException("Read cancelled");
	}

	/* write memory */
	void WriteMemory(uint32_t address, unsigned char* buf, int32_t offset,
		int32_t size, STBootProgress& p) {
		/* number of bytes written */
		int32_t bwritten = 0, btotal = size;

		cancel = false;

		/* no support for write? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::WRITE) == Commands.end()) {
			throw STBootException("Command not supported");
		}

		/* data is read in chunks */
		while (size > 0 && !cancel) {
			/* chunk size */		
			int32_t csize = min(size, 256);
			/* read a single chunk */
			Write(address, buf, offset, csize);

			/* update iterators */
			size -= csize; offset += csize; address += (uint32_t)csize;
			/* update number of bytes read */
			bwritten += csize;

			/* report progress */
			p.update(bwritten, btotal);
		}

		/* throw exception if operation was cancelled */
		if (cancel)
			throw STBootException("Write cancelled");
	}

	/* erase page */
	void ErasePage(uint32_t pageNumber) {
		/* 'classic' erase operation supported? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::ERASE) != Commands.end()) {
			Erase(pageNumber);
		/* 'extended' erase operation supported? */
		} else if (std::find(Commands.begin(), Commands.end(), STCmds::EXT_ERASE) != Commands.end()) {
			 ExtendedErase(pageNumber);
		/* no operation supported */
		} else {
			throw new STBootException("Command not supported");
		}
	}

	/* perform global erase */
	void GlobalErase() {
		/* 'classic' erase operation supported? */

		/* 'classic' erase operation supported? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::ERASE) != Commands.end()) {
			EraseSpecial(STEraseMode::GLOBAL);
		/* 'extended' erase operation supported? */
		} else if (std::find(Commands.begin(), Commands.end(), STCmds::EXT_ERASE) != Commands.end()) {
			ExtendedEraseSpecial(STExtendedEraseMode::GLOBAL);	
		/* no operation supported */
		} else {
			throw STBootException("Command not supported");
		}
	}

	/* jump to user code */
	void Jump(uint32_t address) {
		/* no support for go? */
		if (std::find(Commands.begin(), Commands.end(), STCmds::GO) == Commands.end()) {
			throw STBootException("Command not supported");
		}
		/* go! */
		Go(address);
	}

	void wakeUp(uint8_t* bin, uint32_t* length)	{
		uint8_t tx[9] = {0x00,0xAA,0xAA,0xAA,0x08,0x01,0x00,0x98,0xA1};
		uint8_t rx[100];

		sp.write(tx, 1);
		std::this_thread::sleep_for(std::chrono::microseconds(50));		
		sp.write(tx+1, 8);

		if (sp.read(rx, 1) == 0) 
			throw STBootException("No BMS command response!");
		if ((rx[0] != 0xE2)) 
			throw STBootException(" Bad BMS command response! " + rx[0]);

		bin[0] = rx[0];
		*length = 1;
	}


	void status(uint8_t* bin, uint32_t* length) {
		uint8_t tx[8] = {0xAA,0xAA,0xAA,0x08,0x05,0x00,0x58,0xA3 };		
		uint8_t rx[100];

		sp.write(tx, 8);

		uint32_t bytesReceived = sp.read(rx, 200);
		if (bytesReceived == 0)
			throw STBootException("No BMS command response!");
		for (uint32_t i = 0; i < bytesReceived; i++) {
			bin[i] = rx[i];			
		}
		*length = bytesReceived;
	}

	//void Init()	{
	//	/* command word */
	//	uint8_t tx[1];
	//	/* response code */
	//	uint8_t ack[1];
	//	/* store code */
	//	tx[0] = (uint8_t)STCmds::INIT;

	//	/* send bytes */
	//	sp.write(tx, 1);
	//	/* wait for response code */
	//	sp.read(ack, 1);
	//	/* check response code */
	//	if (ack[0] != (uint8_t)STResps::ACK)
	//		/* error during send */
	//		throw STBootException("Command Rejected");
	//}

	void Get()	{
		/* command word */
		uint8_t tx[2];
		/* temporary storage for response bytes */
		uint8_t tmp[1];
		/* numbe or response bytes */
		int nbytes;
		/* rx buffer */
		string rx = "";

		/* store code */
		tx[0] = (uint8_t)STCmds::GET;
		/* set checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);

		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw STBootException("Command Rejected");

		/* wait for number of bytes */
		sp.read(tmp, 1);
		/* assign number of bytes that will follow (add for acks) */
		nbytes = tmp[0] + 2;
		/* nbytes must be equal to 13 for stm32 products */
		if (nbytes != 13)
			throw STBootException("Invalid length");

		/* receive response */
		sp.read(rx, nbytes);

		///* store version information */
		Version = (rx[0] >> 4) + "." + (rx[0] & 0xf);

		///* add all commands */
		for (uint16_t i = 1; i < nbytes - 1; i++)
			Commands.push_back((STCmds)rx[i]);
	}

	/* get id command */
	void GetID()
	{
		/* command word */
		uint8_t tx[2];
		/* temporary storage for response bytes */
		uint8_t tmp[1];
		/* numbe or response bytes */
		int nbytes;
		/* rx buffer */
		string rx = "";

		/* store code */
		tx[0] = (uint8_t)STCmds::GET_ID;
		/* set checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
			
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw STBootException("Command Rejected");

		/* wait for number of bytes */
		sp.read(tmp, 1);
		/* assign number of bytes that will follow (add for acks) */
		nbytes = tmp[0] + 2;
		/* nbytes must be equal to 3 for stm32 products */
		if (nbytes != 3)
			throw STBootException("Invalid length");

		/* receive response */
		sp.read(rx, nbytes);

		/* store product id */
		ProductID = (USHORT)((rx[0] << 8) | (rx[1]));
	}

	void Read(uint32_t address, unsigned char* buf, int32_t offset, int32_t length) {
		/* command word */
		uint8_t tx[9];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::READ;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* store address */
		tx[2] = (uint8_t)((address >> 24) & 0xff);
		tx[3] = (uint8_t)((address >> 16) & 0xff);
		tx[4] = (uint8_t)((address >> 8) & 0xff);
		tx[5] = (uint8_t)(address & 0xff);
		/* address checksum (needs to be not negated. why? because ST!
		 * that's why. */
		tx[6] = (uint8_t)~ComputeChecksum(tx, 2, 4);

		/* store number of bytes */
		tx[7] = (uint8_t)(length - 1);
		/* size checksum */
		tx[8] = ComputeChecksum(tx, 7, 1);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* send address */
		sp.write(tx + 2, 5);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Address Rejected");

		/* send address */
		sp.write(tx + 7, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Size Rejected");

		/* receive response */
		sp.read(buf + offset, length);
		/* oops, something baaad happened! */
	}

	/* go command */
	void Go(uint32_t address) {
		/* command word */
		uint8_t tx[7];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::GO;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* store address */
		tx[2] = (uint8_t)((address >> 24) & 0xff);
		tx[3] = (uint8_t)((address >> 16) & 0xff);
		tx[4] = (uint8_t)((address >> 8) & 0xff);
		tx[5] = (uint8_t)(address & 0xff);
		/* address checksum (needs to be not negated. why? because ST!
		 * that's why. */
		tx[6] = (uint8_t)~ComputeChecksum(tx, 2, 4);

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* send address */	
		sp.write(tx + 2, 5);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Address Rejected");		
	}

	/* write memory */
	void Write(uint32_t address, unsigned char* data, int32_t offset, int32_t length) {
		/* command word */
		uint8_t tx[9];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::WRITE;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* store address */
		tx[2] = (uint8_t)((address >> 24) & 0xff);
		tx[3] = (uint8_t)((address >> 16) & 0xff);
		tx[4] = (uint8_t)((address >> 8) & 0xff);
		tx[5] = (uint8_t)(address & 0xff);
		/* address checksum (needs to be not negated. why? because ST!
		 * that's why. */
		tx[6] = (uint8_t)~ComputeChecksum(tx, 2, 4);

		/* number of bytes */
		tx[7] = (uint8_t)(length - 1);
		/* data checksum */
		tx[8] = (uint8_t)(~(ComputeChecksum(data, offset, length) ^ tx[7]));

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* send address */
		sp.write(tx + 2, 5);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Address Rejected");

		/* send the number of bytes */
		sp.write(tx + 7, 1);
		/* send data */
		sp.write(data + offset, length);
		/* send checksum */
		sp.write(tx + 8, 1);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Data Rejected");
	}

	/* erase memory page */
	void Erase(uint32_t pageNumber) {
		/* command word */
		uint8_t tx[5];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::ERASE;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* erase single page */
		tx[2] = 0;
		/* set page number */
		tx[3] = (uint8_t)pageNumber;
		/* checksum */
		tx[4] = (uint8_t)~ComputeChecksum(tx, 2, 2);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* send address */
		sp.write(tx + 2, 3);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Page Rejected");
	}

	/* erase memory page */
	void EraseSpecial(STEraseMode mode) {
		/* command word */
		uint8_t tx[4];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::ERASE;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* erase single page */
		tx[2] = (uint8_t)((int)mode);
		/* checksum */
		tx[3] = (uint8_t)~ComputeChecksum(tx, 2, 2);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* send address */;
		sp.write(tx + 2, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Special Code Rejected");
	}

	/* extended erase memory page */
	void ExtendedErase(uint32_t pageNumber) {
		/* command word */
		uint8_t tx[7];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::EXT_ERASE;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* erase single page */
		tx[2] = 0;
		tx[3] = 0;
		/* set page number */
		tx[4] = (uint8_t)(pageNumber >> 8);
		tx[5] = (uint8_t)(pageNumber >> 0);
		/* checksum */
		tx[6] = (uint8_t)~ComputeChecksum(tx, 2, 5);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* send address */
		sp.write(tx + 2, 5);
		/* wait for response code. use longer timeout, erase might
			* take a while or two. */
		sp.read(tmp, 1 /*,3000*/);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Page Rejected");
	}

	/* extended erase memory page */
	void ExtendedEraseSpecial(STExtendedEraseMode mode) {
		/* command word */
		uint8_t tx[5];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::EXT_ERASE;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* erase single page */
		tx[2] = (uint8_t)((int)mode >> 8);
		tx[3] = (uint8_t)((int)mode >> 0);
		/* checksum */
		tx[4] = (uint8_t)~ComputeChecksum(tx, 2, 3);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* send address */
		sp.write(tx + 2, 3);
		/* wait for response code. use longer timeout, erase might
			* take a while or two. */
		sp.read(tmp, 1/*, 10000*/);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Special code Rejected");
	}

	/* unprotect flash before writing */
	void WriteUnprotect() {
		/* command word */
		uint8_t tx[2];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::WR_UNPROTECT;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* wait for response code. use longer timeout, erase might
			* take a while or two. */
		sp.read(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Write Unprotect Rejected");
	}

	/* unprotect flash before reading */
	void ReadUnprotect() {
		/* command word */
		uint8_t tx[2];
		/* temporary storage for response bytes */
		uint8_t tmp[1];

		/* command code */
		tx[0] = (uint8_t)STCmds::RD_UNPROTECT;
		/* checksum */
		tx[1] = ComputeChecksum(tx, 0, 1);

		/* try to send command and wait for response */

		/* send bytes */
		sp.write(tx, 2);
		/* wait for response code */
		sp.write(tmp, 1);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Command Rejected");

		/* wait for response code. use longer timeout, erase might
			* take a while or two. */
		sp.read(tmp, 1 /*,10000*/);
		/* check response code */
		if (tmp[0] != (uint8_t)STResps::ACK)
			throw new STBootException("Write Unprotect Rejected");
	}

	/* compute checksum */
	uint8_t ComputeChecksum(const unsigned char* data, int32_t offset, int32_t count)	{
		/* initial value */
		uint8_t xor = 0xff;
		/* compute */
		for (int32_t i = offset; i < count + offset; i++)
			xor ^= data[i];

		/* return value */
		return xor;
	}

	/* bootloader version */
	string Version;
	/* product id */
	USHORT ProductID;

private:
	/* serial port */

	serial::Serial sp;
	/* command mutex */
	//SemaphoreSlim ;
	/* list of supported commands */
	list<STCmds> Commands;

	mutex bootProcessMutex;
	condition_variable cv;

	std::atomic<bool> cancel;

	std::unique_ptr<std::thread> bootProcessThread;
};


}  // namespace bootloader
