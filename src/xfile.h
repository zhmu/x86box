#ifndef __XFILE_H__
#define __XFILE_H__

#include <stdint.h>

//! \brief Provides uniform access to a file
class XFile
{
public:
	typedef int32_t ssize_t;
	typedef uint64_t off_t;

	virtual ~XFile() { }

	//! \brief Open flags
	static const unsigned int FLAG_READ = 1;
	static const unsigned int FLAG_WRITE = 2;

	/*! \brief Opens a given file by name
	 *  \param fname File name to open
	 *  \param flags Open flags (any combination of FLAG_xxx)
	 *  \returns true on success
	 */
	virtual bool Open(const char* fname, int flags) = 0;

	//! \brief Closes the currently-opened file, if any
	virtual void Close() = 0;

	/*! \brief Retrieve the file length
	 *  \returns File length, or zero on failure
	 */
	virtual off_t GetLength() const = 0;

	/*! \brief Seeks to a given offset
	 *  \param offset Offset to use from the beginning
	 */
	virtual void Seek(off_t offset) = 0;

	/*! \brief Reads data from the file
	 *  \param buffer Buffer to read to
	 *  \param size Number of bytes to read
	 *  \returns Number of bytes read, or -1 for failure
	 */
	virtual ssize_t Read(void* buffer, ssize_t size) = 0;

	/*! \brief Writes data to the file
	 *  \param buffer Buffer to write from
	 *  \param size Number of bytes to write
	 *  \returns Number of bytes written, or -1 for failure
	 */
	virtual ssize_t Write(const void* buffer, ssize_t size) = 0;

	//! \brief Is the file opened?
	virtual bool IsOpen() const = 0;
};

#endif /* __XFILE_H__ */
