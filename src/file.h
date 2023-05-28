#ifndef __FILE_H__
#define __FILE_H__

#include "xfile.h"

//! \brief Provides access to a local file
class File : public XFile
{
  public:
    File();
    virtual ~File();
    virtual bool Open(const char* fname, int flags);
    virtual void Close();
    virtual off_t GetLength() const;
    virtual void Seek(off_t offset);
    virtual ssize_t Read(void* buffer, ssize_t size);
    virtual ssize_t Write(const void* buffer, ssize_t size);
    virtual bool IsOpen() const;

  protected:
    void* m_File;
};

#endif /* __FILE_H__ */
