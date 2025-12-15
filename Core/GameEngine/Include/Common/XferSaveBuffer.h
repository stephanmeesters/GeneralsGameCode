#pragma once

#include "Common/Xfer.h"
#include <vector>

class XferSaveBufferBlockData;
class Snapshot;

typedef size_t XferFilePos;

class XferSaveBuffer : public Xfer
{

public:

	XferSaveBuffer( void );
	virtual ~XferSaveBuffer( void );

	virtual void open( AsciiString identifier );
	virtual void close( void );
	virtual Int beginBlock( void );
	virtual void endBlock( void );
	virtual void skip( Int dataSize );

	virtual void xferSnapshot( Snapshot *snapshot );

	virtual void xferAsciiString( AsciiString *asciiStringData );
	virtual void xferUnicodeString( UnicodeString *unicodeStringData );

	std::vector<Byte> takeBuffer();

protected:

	virtual void xferImplementation( void *data, Int dataSize );

	Bool m_isOpen;
	std::vector<Byte> m_buffer;
	XferSaveBufferBlockData *m_blockStack;
};
