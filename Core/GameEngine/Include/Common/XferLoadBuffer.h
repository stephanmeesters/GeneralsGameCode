#pragma once

#include "Common/Xfer.h"
#include <vector>

class Snapshot;

typedef size_t XferFilePos;

class XferLoadBuffer : public Xfer
{

public:

	XferLoadBuffer( void );
	virtual ~XferLoadBuffer( void );

	virtual void open( AsciiString identifier );
	void open( AsciiString identifier, const std::vector<Byte> &buffer );
	void open( AsciiString identifier, std::vector<Byte> &&buffer );
	virtual void close( void );
	virtual Int beginBlock( void );
	virtual void endBlock( void );
	virtual void skip( Int dataSize );

	virtual void xferSnapshot( Snapshot *snapshot );

	virtual void xferAsciiString( AsciiString *asciiStringData );
	virtual void xferUnicodeString( UnicodeString *unicodeStringData );

	void setBuffer( const std::vector<Byte> &buffer );
	void setBuffer( std::vector<Byte> &&buffer );

	XferFilePos tell( void ) const { return m_filePos; }

protected:

	virtual void xferImplementation( void *data, Int dataSize );

	Bool m_isOpen;
	XferFilePos m_filePos;
	std::vector<Byte> m_buffer;
};
