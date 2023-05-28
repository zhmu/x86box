#include "file.h"
#include <stdio.h>
#include <string.h>

File::File()
: m_File(NULL)
{
}
	
File::~File()
{
	Close();
}

bool
File::Open(const char* fname, int flags)
{
	char fopen_flags[16] = "rb";
	if (flags & FLAG_WRITE)
		strcpy(fopen_flags, "wb");

	Close();
	m_File = (void*)fopen(fname, fopen_flags);
	return m_File != NULL;
}

void
File::Close()
{
	if (m_File == NULL)
		return;
	fclose((FILE*)m_File);
	m_File = NULL;
}

File::off_t
File::GetLength() const
{
	FILE* f = (FILE*)m_File;
	if (f == NULL)
		return 0;
	fseek(f, 0, SEEK_END);
	off_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	return size;
}

void File::Seek(off_t offset)
{
	FILE* f = (FILE*)m_File;
	if (f != NULL)
		fseek(f, offset, SEEK_SET);
}

File::ssize_t
File::Read(void* buffer, ssize_t size)
{
	FILE* f = (FILE*)m_File;
	if (f == NULL)
		return -1;
	return fread(buffer, 1, size, f);
}

File::ssize_t
File::Write(const void* buffer, ssize_t size)
{
	FILE* f = (FILE*)m_File;
	if (f == NULL)
		return -1;
	return fwrite(buffer, 1, size, f);
}

bool
File::IsOpen() const
{
	return m_File != NULL;
}


/* vim:set ts=2 sw=2: */
