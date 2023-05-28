#ifndef __VECTORS_H__
#define __VECTORS_H__

#include <map>
#include "cpux86.h"

class Memory;
class XCallableVector;

class Vectors {
public:
	Vectors(Memory& oMemory);

	//! \brief Registers a handler for a given vector
	void Register(uint8_t no, XCallableVector& oCallback);

	/*! \brief Invoke a given vector
	 *  \param no Vector number to invoke
	 *
	 *  The vector must be hooked.
	 */
	void Invoke(CPUx86& oCPU, uint8_t no);

protected:
	//! \brief Interrupts base address in memory
	static const unsigned int m_VectorHandlerSegment = 0xf800;

	typedef std::map<unsigned int, XCallableVector&> TintVectorHandlerMap;
	TintVectorHandlerMap m_Map;

	//! \brief Memory used to store the vectors
	Memory& m_Memory;
};

#endif /* __VECTORS_H__ */
