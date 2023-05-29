#ifndef __XCALLABLEVECTOR_H__
#define __XCALLABLEVECTOR_H__

#include "cpux86.h"

class XCallableVector
{
  public:
    virtual void InvokeVector(uint8_t no, CPUx86& oCPU, cpu::State& oState) = 0;
};

#endif /* __XCALLABLEVECTOR_H__ */
