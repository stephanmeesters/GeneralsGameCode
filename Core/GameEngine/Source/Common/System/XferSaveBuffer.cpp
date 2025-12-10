#include "PreRTS.h"
#include "Common/XferSaveBuffer.h"
#include "Common/Snapshot.h"
#include "Common/GameMemory.h"
#include <cstring>

class XferSaveBufferBlockData : public MemoryPoolObject
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(XferSaveBufferBlockData, "XferSaveBufferBlockData")

public:
	XferFilePos filePos;
	XferSaveBufferBlockData *next;
};
EMPTY_DTOR(XferSaveBufferBlockData)

XferSaveBuffer::XferSaveBuffer( void )
{
	m_xferMode = XFER_SAVE;
	m_isOpen = FALSE;
	m_blockStack = NULL;
}

XferSaveBuffer::~XferSaveBuffer( void )
{
	if( m_isOpen )
	{
		DEBUG_CRASH(( "Warning: XferSaveBuffer buffer '%s' was left open", m_identifier.str() ));
		m_isOpen = FALSE;
	}

	if( m_blockStack != NULL )
	{
		DEBUG_CRASH(( "Warning: XferSaveBuffer::~XferSaveBuffer - m_blockStack was not NULL!" ));

		XferSaveBufferBlockData *next;
		while( m_blockStack )
		{
			next = m_blockStack->next;
			deleteInstance(m_blockStack);
			m_blockStack = next;
		}
	}
}

void XferSaveBuffer::open( AsciiString identifier )
{
	if( m_isOpen )
	{
		DEBUG_CRASH(( "Cannot open buffer '%s' cause we've already got '%s' open",
									identifier.str(), m_identifier.str() ));
		throw XFER_FILE_ALREADY_OPEN;
	}

	Xfer::open( identifier );

	m_buffer.clear();
	while( m_blockStack )
	{
		XferSaveBufferBlockData *next = m_blockStack->next;
		deleteInstance(m_blockStack);
		m_blockStack = next;
	}
	m_isOpen = TRUE;
}

void XferSaveBuffer::close( void )
{
	if( !m_isOpen )
	{
		DEBUG_CRASH(( "Xfer close called, but no buffer was open" ));
		throw XFER_FILE_NOT_OPEN;
	}

	m_isOpen = FALSE;
	m_identifier.clear();
}

Int XferSaveBuffer::beginBlock( void )
{
	DEBUG_ASSERTCRASH( m_isOpen, ("XferSaveBuffer begin block - buffer for '%s' is closed",
										 m_identifier.str()) );

	XferFilePos filePos = static_cast<XferFilePos>(m_buffer.size());

	XferBlockSize blockSize = 0;
	m_buffer.insert(m_buffer.end(),
		           reinterpret_cast<Byte *>(&blockSize),
		           reinterpret_cast<Byte *>(&blockSize) + sizeof(XferBlockSize));

	XferSaveBufferBlockData *top = newInstance(XferSaveBufferBlockData);
	top->filePos = filePos;
	top->next = m_blockStack;
	m_blockStack = top;

	return XFER_OK;
}

void XferSaveBuffer::endBlock( void )
{
	DEBUG_ASSERTCRASH( m_isOpen, ("XferSaveBuffer end block - buffer for '%s' is closed",
										 m_identifier.str()) );

	if( m_blockStack == NULL )
	{
		DEBUG_CRASH(( "Xfer end block called, but no matching begin block was found" ));
		throw XFER_BEGIN_END_MISMATCH;
	}

	XferFilePos currentFilePos = static_cast<XferFilePos>(m_buffer.size());

	XferSaveBufferBlockData *top = m_blockStack;
	m_blockStack = m_blockStack->next;

	XferBlockSize blockSize = currentFilePos - top->filePos - sizeof( XferBlockSize );
	if( (top->filePos + sizeof( XferBlockSize )) > m_buffer.size() )
	{
		DEBUG_CRASH(( "Error writing block size to buffer '%s'", m_identifier.str() ));
		throw XFER_WRITE_ERROR;
	}
	std::memcpy(&m_buffer[top->filePos], &blockSize, sizeof(XferBlockSize));

	deleteInstance(top);
}

void XferSaveBuffer::skip( Int dataSize )
{
	DEBUG_ASSERTCRASH( m_isOpen, ("XferSaveBuffer - buffer for '%s' is closed",
										 m_identifier.str()) );

	if( dataSize > 0 )
	{
		m_buffer.insert(m_buffer.end(), dataSize, 0);
	}
}

void XferSaveBuffer::xferSnapshot( Snapshot *snapshot )
{
	if( snapshot == NULL )
	{
		DEBUG_CRASH(( "XferSaveBuffer::xferSnapshot - Invalid parameters" ));
		throw XFER_INVALID_PARAMETERS;
	}

	snapshot->xfer( this );
}

void XferSaveBuffer::xferAsciiString( AsciiString *asciiStringData )
{
	if( asciiStringData->getLength() > 255 )
	{
		DEBUG_CRASH(( "XferSaveBuffer cannot save this ascii string because it's too long.  Change the size of the length header (but be sure to preserve save file compatability" ));
		throw XFER_STRING_ERROR;
	}

	UnsignedByte len = asciiStringData->getLength();
	xferUnsignedByte( &len );

	if( len > 0 )
		xferUser( (void *)asciiStringData->str(), sizeof( Byte ) * len );
}

void XferSaveBuffer::xferUnicodeString( UnicodeString *unicodeStringData )
{
	if( unicodeStringData->getLength() > 255 )
	{
		DEBUG_CRASH(( "XferSaveBuffer cannot save this unicode string because it's too long.  Change the size of the length header (but be sure to preserve save file compatability" ));
		throw XFER_STRING_ERROR;
	}

	UnsignedByte len = unicodeStringData->getLength();
	xferUnsignedByte( &len );

	if( len > 0 )
		xferUser( (void *)unicodeStringData->str(), sizeof( WideChar ) * len );
}

void XferSaveBuffer::xferImplementation( void *data, Int dataSize )
{
	DEBUG_ASSERTCRASH( m_isOpen, ("XferSaveBuffer - buffer for '%s' is closed",
										 m_identifier.str()) );

	const size_t oldSize = m_buffer.size();
	m_buffer.resize(oldSize + dataSize);
	std::memcpy(&m_buffer[oldSize], data, dataSize);
}

std::vector<Byte> XferSaveBuffer::takeBuffer()
{
	if( m_isOpen )
	{
		DEBUG_CRASH(( "Cannot take buffer '%s' while still open", m_identifier.str() ));
		throw XFER_FILE_ALREADY_OPEN;
	}

	std::vector<Byte> out;
	out.swap(m_buffer);
	return out;
}
