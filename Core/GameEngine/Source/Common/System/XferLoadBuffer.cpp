#include "PreRTS.h"
#include "Common/Debug.h"
#include "Common/GameState.h"
#include "Common/Snapshot.h"
#include "Common/XferLoadBuffer.h"
#include <cstring>
#include <utility>

XferLoadBuffer::XferLoadBuffer( void )
{
	m_xferMode = XFER_LOAD;
	m_isOpen = FALSE;
	m_filePos = 0;
}

XferLoadBuffer::~XferLoadBuffer( void )
{
	if( m_isOpen )
	{
		DEBUG_CRASH(( "Warning: XferLoadBuffer buffer '%s' was left open", m_identifier.str() ));
		close();
	}
}

void XferLoadBuffer::setBuffer( const std::vector<Byte> &buffer )
{
	if( m_isOpen )
	{
		DEBUG_CRASH(( "Cannot set buffer because '%s' is already open", m_identifier.str() ));
		throw XFER_FILE_ALREADY_OPEN;
	}

	m_buffer = buffer;
	m_filePos = 0;
}

void XferLoadBuffer::setBuffer( std::vector<Byte> &&buffer )
{
	if( m_isOpen )
	{
		DEBUG_CRASH(( "Cannot set buffer because '%s' is already open", m_identifier.str() ));
		throw XFER_FILE_ALREADY_OPEN;
	}

	m_buffer.swap( buffer );
	m_filePos = 0;
}

void XferLoadBuffer::open( AsciiString identifier )
{
	if( m_isOpen )
	{
		DEBUG_CRASH(( "Cannot open buffer '%s' cause we've already got '%s' open",
									identifier.str(), m_identifier.str() ));
		throw XFER_FILE_ALREADY_OPEN;
	}

	Xfer::open( identifier );

	m_isOpen = TRUE;
	m_filePos = 0;
}

void XferLoadBuffer::open( AsciiString identifier, const std::vector<Byte> &buffer )
{
	setBuffer( buffer );
	open( identifier );
}

void XferLoadBuffer::open( AsciiString identifier, std::vector<Byte> &&buffer )
{
	setBuffer( std::move( buffer ) );
	open( identifier );
}

void XferLoadBuffer::close( void )
{
	if( !m_isOpen )
	{
		DEBUG_CRASH(( "Xfer close called, but no buffer was open" ));
		throw XFER_FILE_NOT_OPEN;
	}

	m_isOpen = FALSE;
	m_filePos = 0;
	m_identifier.clear();
}

Int XferLoadBuffer::beginBlock( void )
{
	DEBUG_ASSERTCRASH( m_isOpen, ("XferLoadBuffer begin block - buffer for '%s' is closed",
										 m_identifier.str()) );

	if( (m_filePos + sizeof( XferBlockSize )) > m_buffer.size() )
	{
		DEBUG_CRASH(( "XferLoadBuffer - Error reading block size for '%s'", m_identifier.str() ));
		return 0;
	}

	XferBlockSize blockSize;
	std::memcpy( &blockSize, &m_buffer[ m_filePos ], sizeof( XferBlockSize ) );
	m_filePos += sizeof( XferBlockSize );

	return blockSize;
}

void XferLoadBuffer::endBlock( void )
{

}

void XferLoadBuffer::skip( Int dataSize )
{
	DEBUG_ASSERTCRASH( m_isOpen, ("XferLoadBuffer::skip - buffer for '%s' is closed",
										 m_identifier.str()) );
	DEBUG_ASSERTCRASH( dataSize >=0, ("XferLoadBuffer::skip - dataSize '%d' must be greater than 0",
										 dataSize) );

	if( (m_filePos + dataSize) > m_buffer.size() )
	{
		DEBUG_CRASH(( "XferLoadBuffer::skip - Cannot skip past end of buffer '%s'", m_identifier.str() ));
		throw XFER_SKIP_ERROR;
	}

	m_filePos += dataSize;
}

void XferLoadBuffer::xferSnapshot( Snapshot *snapshot )
{
	if( snapshot == NULL )
	{
		DEBUG_CRASH(( "XferLoadBuffer::xferSnapshot - Invalid parameters" ));
		throw XFER_INVALID_PARAMETERS;
	}

	// snapshot->xfer( this );

	if( BitIsSet( getOptions(), XO_NO_POST_PROCESSING ) == FALSE )
		TheGameState->addPostProcessSnapshot( snapshot );
}

void XferLoadBuffer::xferAsciiString( AsciiString *asciiStringData )
{
	UnsignedByte len;
	xferUnsignedByte( &len );

	const Int MAX_XFER_LOAD_STRING_BUFFER = 1024;
	static Char buffer[ MAX_XFER_LOAD_STRING_BUFFER ];

	if( len > 0 )
		xferUser( buffer, sizeof( Byte ) * len );
	buffer[ len ] = 0;  // terminate

	asciiStringData->set( buffer );
}

void XferLoadBuffer::xferUnicodeString( UnicodeString *unicodeStringData )
{
	UnsignedByte len;
	xferUnsignedByte( &len );

	const Int MAX_XFER_LOAD_STRING_BUFFER = 1024;
	static WideChar buffer[ MAX_XFER_LOAD_STRING_BUFFER ];

	if( len > 0 )
		xferUser( buffer, sizeof( WideChar ) * len );
	buffer[ len ] = 0;  // terminate

	unicodeStringData->set( buffer );
}

void XferLoadBuffer::xferImplementation( void *data, Int dataSize )
{
	DEBUG_ASSERTCRASH( m_isOpen, ("XferLoadBuffer - buffer for '%s' is closed",
										 m_identifier.str()) );
	DEBUG_ASSERTCRASH( dataSize >=0, ("XferLoadBuffer - dataSize '%d' must be greater than 0",
										 dataSize) );

	if( (m_filePos + dataSize) > m_buffer.size() )
	{
		DEBUG_CRASH(( "XferLoadBuffer - Error reading from buffer '%s'", m_identifier.str() ));
		throw XFER_READ_ERROR;
	}

	std::memcpy( data, &m_buffer[ m_filePos ], dataSize );
	m_filePos += dataSize;
}
