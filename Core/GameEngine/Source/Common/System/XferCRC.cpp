/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: XferCRC.cpp //////////////////////////////////////////////////////////////////////////////
// Author: Matt Campbell, February 2002
// Desc:   Xfer CRC implementation
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/XferCRC.h"
#include "Common/XferDeepCRC.h"
#include "Common/crc.h"
#include "Common/Snapshot.h"
#include "GameLogic/GameLogic.h"
#include "utility/endian_compat.h"
#include <Utility/stdio_adapter.h>
#include <time.h>
#include <string.h>
#include <windows.h>

static Bool g_crcSessionReady = FALSE;
static AsciiString g_crcSessionTimestamp;
static AsciiString g_crcSessionDir;

static float quantizeFloat(float x)
{
	double v = static_cast<double>(x) * 10e6;
	v = v >= 0.0 ? floor(v + 0.5) : ceil(v - 0.5);
	v = v == -0.0f ? 0.0f : v;
	return v / 10e6; // NOLINT(*-narrowing-conversions)
}

static void buildCRCSessionDir()
{
	if( g_crcSessionReady )
	{
		return;
	}

	char timeBuffer[32];
time_t now = time( NULL );
struct tm localTime;
#if defined(_WIN32)
	#if defined(_MSC_VER) && _MSC_VER >= 1400
		localtime_s( &localTime, &now );
	#else
		struct tm *tmp = localtime( &now );
		if( tmp != NULL )
		{
			localTime = *tmp;
		}
		else
		{
			memset( &localTime, 0, sizeof( localTime ) );
		}
	#endif
#else
	localTime = *localtime( &now );
#endif
	strftime( timeBuffer, sizeof( timeBuffer ), "%Y%m%d_%H%M%S", &localTime );
	g_crcSessionTimestamp = timeBuffer;

	char exePath[ _MAX_PATH ];
	exePath[0] = 0;
	::GetModuleFileName( NULL, exePath, ARRAY_SIZE( exePath ) );
	if( char *pEnd = strrchr( exePath, '\\' ) )
	{
		*(pEnd + 1) = 0;
	}

	g_crcSessionDir = exePath;
	g_crcSessionDir.concat( g_crcSessionTimestamp );
	CreateDirectory( g_crcSessionDir.str(), NULL );

	g_crcSessionReady = TRUE;
}

void InitCRCSessionTimestamp( void )
{
	buildCRCSessionDir();
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
XferCRC::XferCRC( void )
{

	m_xferMode = XFER_CRC;
	m_textLogEnabled = FALSE;
	m_crc = 0;
	m_textFP = NULL;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
XferCRC::~XferCRC( void )
{
	if( m_textFP != NULL )
	{
		fclose( m_textFP );
		m_textFP = NULL;
	}

}

//-------------------------------------------------------------------------------------------------
/** Open file 'identifier' for writing */
//-------------------------------------------------------------------------------------------------
void XferCRC::open( AsciiString identifier )
{
	if( m_textLogEnabled )
	{
		buildCRCSessionDir();
	}

	// call base class
	Xfer::open( identifier );

	// initialize CRC to brand new one at zero
	m_crc = 0;

	if( m_textFP != NULL )
	{
		fclose( m_textFP );
		m_textFP = NULL;
	}

	if( m_textLogEnabled )
	{
		UnsignedInt frame = 0;
		if( TheGameLogic != NULL )
		{
			frame = TheGameLogic->getFrame();
		}

		AsciiString logFileName;
		logFileName.format( "%s\\crc_frame_%04u.txt", g_crcSessionDir.str(), frame );
		m_textFP = fopen( logFileName.str(), "w" );
		if( m_textFP == NULL )
		{
			DEBUG_LOG(( "XferCRC - Unable to open CRC log file '%s'", logFileName.str() ));
		}
	}
}

//-------------------------------------------------------------------------------------------------
/** Close our current file */
//-------------------------------------------------------------------------------------------------
void XferCRC::close( void )
{

	if( m_textFP != NULL )
	{
		UnsignedInt finalCRC = getCRC();
		fprintf( m_textFP, "FinalCRC: 0x%8.8X\n", finalCRC );
	}
	if( m_textFP != NULL )
	{
		fclose( m_textFP );
		m_textFP = NULL;
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Int XferCRC::beginBlock( void )
{

	return 0;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void XferCRC::endBlock( void )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void XferCRC::addCRC( UnsignedInt val )
{

	m_crc = (m_crc << 1) + htobe(val) + ((m_crc >> 31) & 0x01);

}

// ------------------------------------------------------------------------------------------------
/** Entry point for xfering a snapshot */
// ------------------------------------------------------------------------------------------------
void XferCRC::xferSnapshot( Snapshot *snapshot, const char *label )
{

	if( snapshot == NULL )
	{

		return;

	}

	if( label != NULL && label[0] != '\0' )
	{
		logCRCValue( label, "Snapshot" );
	}

	// run the crc function of the snapshot
	snapshot->crc( this );

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void XferCRC::xferReal( Real *realData, const char *label )
{
	// const float value = static_cast<float>( *realData );
	// unsigned char quantized[8];
	// xferImplementation( quantized, static_cast<Int>( sizeof( quantized ) ) );

	// float quantized = quantizeFloat(value);

	xferImplementation( realData, sizeof( Real ) );
	// xferImplementation( &quantized, sizeof( Real ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[128];
		// snprintf( buffer, sizeof( buffer ), "%.15g", quantized );
		snprintf( buffer, sizeof( buffer ), "%.15g", static_cast<float>( *realData ) );
		logCRCValue( label, buffer );
		logCRCBytes( label, realData, sizeof(Real));
		// logCRCBytes( label, &quantized, sizeof(Real));
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void XferCRC::xferMatrix3D( Matrix3D* mtx, const char *label )
{
	Vector4& tmp0 = (*mtx)[0];
	Vector4& tmp1 = (*mtx)[1];
	Vector4& tmp2 = (*mtx)[2];

	xferReal( &tmp0.X, label );
	xferReal( &tmp0.Y, label );
	xferReal( &tmp0.Z, label );
	xferReal( &tmp0.W, label );

	xferReal( &tmp1.X, label );
	xferReal( &tmp1.Y, label );
	xferReal( &tmp1.Z, label );
	xferReal( &tmp1.W, label );

	xferReal( &tmp2.X, label );
	xferReal( &tmp2.Y, label );
	xferReal( &tmp2.Z, label );
	xferReal( &tmp2.W, label );
}

//-------------------------------------------------------------------------------------------------
/** Perform a single CRC operation on the data passed in */
//-------------------------------------------------------------------------------------------------
void XferCRC::xferImplementation( void *data, Int dataSize )
{
	const UnsignedInt *uintPtr = (const UnsignedInt *) (data);
	dataSize *= (data != NULL);

	int dataBytes = (dataSize / 4);

	for (Int i=0 ; i<dataBytes; ++i)
	{
		addCRC (*uintPtr++);
	}

	UnsignedInt val = 0;
	const unsigned char *c = (const unsigned char *)uintPtr;

	switch(dataSize & 3)
	{
	case 3:
		val += (c[2] << 16);
		FALLTHROUGH;
	case 2:
		val += (c[1] << 8);
		FALLTHROUGH;
	case 1:
		val += c[0];
		m_crc = (m_crc << 1) + val + ((m_crc >> 31) & 0x01);
		FALLTHROUGH;
	default:
		break;
	}

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void XferCRC::skip( Int dataSize )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UnsignedInt XferCRC::getCRC( void )
{

	return htobe(m_crc);

}

void XferCRC::setTextLogEnabled( Bool enable )
{
	m_textLogEnabled = enable;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void XferCRC::logCRCValue( const char *label, const char *valueText )
{
	if( m_textFP == NULL )
	{
		return;
	}

	if( label != NULL && label[0] != '\0' )
	{
		fprintf( m_textFP, "%s: %s\n", label, (valueText != NULL) ? valueText : "" );
	}
	else
	{
		fprintf( m_textFP, "%s\n", (valueText != NULL) ? valueText : "" );
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void XferCRC::logCRCBytes( const char *label, const void *data, Int dataSize )
{
	if( m_textFP == NULL )
	{
		return;
	}

	if( label != NULL && label[0] != '\0' )
	{
		fprintf( m_textFP, "%s: ", label );
	}

	if( data == NULL || dataSize <= 0 )
	{
		fprintf( m_textFP, "\n" );
		return;
	}

	const unsigned char *bytes = static_cast<const unsigned char *>( data );
	for( Int i = 0; i < dataSize; ++i )
	{
		fprintf( m_textFP, "%02X", bytes[i] );
	}
	fprintf( m_textFP, "\n" );
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
XferDeepCRC::XferDeepCRC( void )
{

	m_xferMode = XFER_SAVE;
	m_fileFP = NULL;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
XferDeepCRC::~XferDeepCRC( void )
{

	// warn the user if a file was left open
	if( m_fileFP != NULL )
	{

		DEBUG_CRASH(( "Warning: Xfer file '%s' was left open", m_identifier.str() ));
		close();

	}

}

//-------------------------------------------------------------------------------------------------
/** Open file 'identifier' for writing */
//-------------------------------------------------------------------------------------------------
void XferDeepCRC::open( AsciiString identifier )
{

	m_xferMode = XFER_SAVE;

	// sanity, check to see if we're already open
	if( m_fileFP != NULL )
	{

		DEBUG_CRASH(( "Cannot open file '%s' cause we've already got '%s' open",
									identifier.str(), m_identifier.str() ));
		throw XFER_FILE_ALREADY_OPEN;

	}

	// call base class
	XferCRC::open( identifier );

	// open the file
	m_fileFP = fopen( identifier.str(), "w+b" );
	if( m_fileFP == NULL )
	{

		DEBUG_CRASH(( "File '%s' not found", identifier.str() ));
		throw XFER_FILE_NOT_FOUND;

	}

}

//-------------------------------------------------------------------------------------------------
/** Close our current file */
//-------------------------------------------------------------------------------------------------
void XferDeepCRC::close( void )
{

	// sanity, if we don't have an open file we can do nothing
	if( m_fileFP == NULL )
	{

		DEBUG_CRASH(( "Xfer close called, but no file was open" ));
		throw XFER_FILE_NOT_OPEN;

	}

	// close the file
	fclose( m_fileFP );
	m_fileFP = NULL;

	// erase the filename
	m_identifier.clear();

	XferCRC::close();
}

//-------------------------------------------------------------------------------------------------
/** Perform a single CRC operation on the data passed in */
//-------------------------------------------------------------------------------------------------
void XferDeepCRC::xferImplementation( void *data, Int dataSize )
{

	if (!data || dataSize < 1)
	{
		return;
	}

	// sanity
	DEBUG_ASSERTCRASH( m_fileFP != NULL, ("XferSave - file pointer for '%s' is NULL",
										 m_identifier.str()) );

	// write data to file
	if( fwrite( data, dataSize, 1, m_fileFP ) != 1 )
	{

		DEBUG_CRASH(( "XferSave - Error writing to file '%s'", m_identifier.str() ));
		throw XFER_WRITE_ERROR;

	}

	XferCRC::xferImplementation( data, dataSize );

}

// ------------------------------------------------------------------------------------------------
/** Save ascii string */
// ------------------------------------------------------------------------------------------------
void XferDeepCRC::xferMarkerLabel( AsciiString asciiStringData, const char *label )
{
	(void)label;

}

// ------------------------------------------------------------------------------------------------
/** Save ascii string */
// ------------------------------------------------------------------------------------------------
void XferDeepCRC::xferAsciiString( AsciiString *asciiStringData, const char *label )
{
	(void)label;

	// sanity
	if( asciiStringData->getLength() > 16385 )
	{

		DEBUG_CRASH(( "XferSave cannot save this ascii string because it's too long.  Change the size of the length header (but be sure to preserve save file compatability" ));
		throw XFER_STRING_ERROR;

	}

	// save length of string to follow
	UnsignedShort len = asciiStringData->getLength();
	xferUnsignedShort( &len );

	// save string data
	if( len > 0 )
		xferUser( (void *)asciiStringData->str(), sizeof( Byte ) * len );

}

// ------------------------------------------------------------------------------------------------
/** Save unicodee string */
// ------------------------------------------------------------------------------------------------
void XferDeepCRC::xferUnicodeString( UnicodeString *unicodeStringData, const char *label )
{
	(void)label;

	// sanity
	if( unicodeStringData->getLength() > 255 )
	{

		DEBUG_CRASH(( "XferSave cannot save this unicode string because it's too long.  Change the size of the length header (but be sure to preserve save file compatability" ));
		throw XFER_STRING_ERROR;

	}

	// save length of string to follow
	Byte len = unicodeStringData->getLength();
	xferByte( &len );

	// save string data
	if( len > 0 )
		xferUser( (void *)unicodeStringData->str(), sizeof( WideChar ) * len );

}
