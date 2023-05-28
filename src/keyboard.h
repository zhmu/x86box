#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "memory.h"
#include "xcallablevector.h"

class HostIO;
class Vectors;

class Keyboard : public XCallableVector
{
  public:
    Keyboard(Memory& memory, HostIO& hostio, Vectors& vectors);
    virtual ~Keyboard();

    virtual void Reset();

    void InvokeVector(uint8_t no, CPUx86& oCPU, CPUx86::State& oState);

  protected:
    //! \brief Host IO used
    HostIO& m_hostio;

    //! \brief Vectors object
    Vectors& m_vectors;

    //! \brief Host memory
    Memory& m_memory;
};

#endif /* __KEYBOARD_H__ */
