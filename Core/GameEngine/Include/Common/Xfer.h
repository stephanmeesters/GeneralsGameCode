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

// FILE: Xfer.h ///////////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, February 2002
// Desc:   The Xfer system is capable of setting up operations to work with blocks of data
//				 from other subsystems.  It can work things such as file reading, file writing,
//				 CRC computations etc
//
// TheSuperHackers @info helmutbuhler 04/09/2025
//         The baseclass Xfer has 3 implementations:
//          - XferLoad: Load gamestate
//          - XferSave: Save gamestate
//          - XferCRC: Calculate gamestate CRC
//            - XferDeepCRC: This derives from XferCRC and also writes the gamestate data relevant
//              to crc calculation to a file (only used in developer builds)
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "Common/Science.h"
#include "Common/Upgrade.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////////////////////////
class Snapshot;
typedef Int Color;
enum ObjectID CPP_11(: Int);
enum DrawableID CPP_11(: Int);
enum KindOfType CPP_11(: Int);
enum ScienceType CPP_11(: Int);
class Matrix3D;

// ------------------------------------------------------------------------------------------------
typedef UnsignedByte XferVersion;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
enum XferMode CPP_11(: Int)
{
	XFER_INVALID = 0,

	XFER_SAVE,
	XFER_LOAD,
	XFER_CRC,

	NUM_XFER_TYPES
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
enum XferStatus CPP_11(: Int)
{
	XFER_STATUS_INVALID = 0,

	XFER_OK,														///< all is green and good
	XFER_EOF,														///< end of file encountered
	XFER_FILE_NOT_FOUND,								///< requested file does not exist
	XFER_FILE_NOT_OPEN,									///< file was not open
	XFER_FILE_ALREADY_OPEN,							///< this xfer is already open
	XFER_READ_ERROR,										///< error reading from file
	XFER_WRITE_ERROR,										///< error writing to file
	XFER_MODE_UNKNOWN,									///< unknown xfer mode
	XFER_SKIP_ERROR,										///< error skipping file
	XFER_BEGIN_END_MISMATCH,						///< mismatched pair calls of begin/end block
	XFER_OUT_OF_MEMORY,									///< out of memory
	XFER_STRING_ERROR,									///< error with strings
	XFER_INVALID_VERSION,								///< invalid version encountered
	XFER_INVALID_PARAMETERS,						///< invalid parameters
	XFER_LIST_NOT_EMPTY,								///< trying to xfer into a list that should be empty, but isn't
	XFER_UNKNOWN_STRING,								///< unrecognized string value

	XFER_ERROR_UNKNOWN,									///< unknown error (isn't that useful!)

	NUM_XFER_STATUS
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
enum XferOptions CPP_11(: UnsignedInt)
{
	XO_NONE										= 0x00000000,
	XO_NO_POST_PROCESSING			= 0x00000001,

	XO_ALL										= 0xFFFFFFFF
};

///////////////////////////////////////////////////////////////////////////////////////////////////
typedef Int XferBlockSize;

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
class Xfer
{

public:

	Xfer( void );
	virtual ~Xfer( void );

	virtual XferMode getXferMode( void ) { return m_xferMode; }
	AsciiString getIdentifier( void ) { return m_identifier; }

	// xfer management
	virtual void setOptions( UnsignedInt options ) { BitSet( m_options, options ); }
	virtual void clearOptions( UnsignedInt options ) { BitClear( m_options, options ); }
	virtual UnsignedInt getOptions( void ) { return m_options; }
	virtual void open( AsciiString identifier ) = 0;		///< xfer open event
	virtual void close( void ) = 0;											///< xfer close event
	virtual Int beginBlock( void ) = 0;									///< xfer begin block event
	virtual void endBlock( void ) = 0;									///< xfer end block event
	virtual void skip( Int dataSize ) = 0;							///< xfer skip data

	virtual void xferSnapshot( Snapshot *snapshot, const char *label = "" ) = 0;		///< entry point for xfering a snapshot

	//
	// default transfer methods, these call the implementation method with the data
	// parameters.  You may use the default, or derive and create new ways to xfer each
	// of these types of data
	//
	virtual void xferVersion( XferVersion *versionData, XferVersion currentVersion, const char *label = "" );
	virtual void xferByte( Byte *byteData, const char *label = "" );
	virtual void xferUnsignedByte( UnsignedByte *unsignedByteData, const char *label = "" );
	virtual void xferBool( Bool *boolData, const char *label = "" );
	virtual void xferInt( Int *intData, const char *label = "" );
	virtual void xferInt64( Int64 *int64Data, const char *label = "" );
	virtual void xferUnsignedInt( UnsignedInt *unsignedIntData, const char *label = "" );
	virtual void xferShort( Short *shortData, const char *label = "" );
	virtual void xferUnsignedShort( UnsignedShort *unsignedShortData, const char *label = "" );
	virtual void xferReal( Real *realData, const char *label = "" );
	virtual void xferMarkerLabel( AsciiString asciiStringData, const char *label = "" ); // This is purely for readability purposes - it is explicitly discarded on load.
	virtual void xferAsciiString( AsciiString *asciiStringData, const char *label = "" );
	virtual void xferUnicodeString( UnicodeString *unicodeStringData, const char *label = "" );
	virtual void xferCoord3D( Coord3D *coord3D, const char *label = "" );
	virtual void xferICoord3D( ICoord3D *iCoord3D, const char *label = "" );
	virtual void xferRegion3D( Region3D *region3D, const char *label = "" );
	virtual void xferIRegion3D( IRegion3D *iRegion3D, const char *label = "" );
	virtual void xferCoord2D( Coord2D *coord2D, const char *label = "" );
	virtual void xferICoord2D( ICoord2D *iCoord2D, const char *label = "" );
	virtual void xferRegion2D( Region2D *region2D, const char *label = "" );
	virtual void xferIRegion2D( IRegion2D *iRegion2D, const char *label = "" );
	virtual void xferRealRange( RealRange *realRange, const char *label = "" );
	virtual void xferColor( Color *color, const char *label = "" );
	virtual void xferRGBColor( RGBColor *rgbColor, const char *label = "" );
	virtual void xferRGBAColorReal( RGBAColorReal *rgbaColorReal, const char *label = "" );
	virtual void xferRGBAColorInt( RGBAColorInt *rgbaColorInt, const char *label = "" );
	virtual void xferObjectID( ObjectID *objectID, const char *label = "" );
	virtual void xferDrawableID( DrawableID *drawableID, const char *label = "" );
	virtual void xferSTLObjectIDVector( std::vector<ObjectID> *objectIDVectorData, const char *label = "" );
	virtual void xferSTLObjectIDList( std::list< ObjectID > *objectIDListData, const char *label = "" );
	virtual void xferSTLIntList( std::list< Int > *intListData, const char *label = "" );
	virtual void xferScienceType( ScienceType *science, const char *label = "" );
	virtual void xferScienceVec( ScienceVec *scienceVec, const char *label = "" );
	virtual void xferKindOf( KindOfType *kindOfData, const char *label = "" );
	virtual void xferUpgradeMask( UpgradeMaskType *upgradeMaskData, const char *label = "" );
	virtual void xferUser( void *data, Int dataSize, const char *label = "" );
	virtual void xferMatrix3D( Matrix3D* mtx, const char *label = "" );
	virtual void xferMapName( AsciiString *mapNameData, const char *label = "" );

	static void buildCRCLabel( const char *className, const char *memberName, const char *typeName, char *out, size_t outSize );

protected:

	// this is the actual xfer impelmentation that each derived class should implement
	virtual void xferImplementation( void *data, Int dataSize ) = 0;
	virtual void logCRCValue( const char *label, const char *valueText );
	virtual void logCRCBytes( const char *label, const void *data, Int dataSize );

	UnsignedInt m_options;					///< xfer options
	XferMode m_xferMode;						///< the current xfer mode
	AsciiString m_identifier;				///< the string identifier

};

#define CRC_XFER(xfer, className, method, member, typeName) \
	do { \
		char _crcLabel[128]; \
		Xfer::buildCRCLabel( className, #member, typeName, _crcLabel, sizeof( _crcLabel ) ); \
		(xfer)->method( &(member), _crcLabel ); \
	} while(0)

#define CRC_XFER_PTR(xfer, className, method, ptr, memberName, typeName) \
	do { \
		char _crcLabel[128]; \
		Xfer::buildCRCLabel( className, memberName, typeName, _crcLabel, sizeof( _crcLabel ) ); \
		(xfer)->method( (ptr), _crcLabel ); \
	} while(0)

#define CRC_XFER_WITH_ARG(xfer, className, method, member, arg, typeName) \
	do { \
		char _crcLabel[128]; \
		Xfer::buildCRCLabel( className, #member, typeName, _crcLabel, sizeof( _crcLabel ) ); \
		(xfer)->method( &(member), (arg), _crcLabel ); \
	} while(0)

#define CRC_XFER_USER(xfer, className, ptr, size, memberName, typeName) \
	do { \
		char _crcLabel[128]; \
		Xfer::buildCRCLabel( className, memberName, typeName, _crcLabel, sizeof( _crcLabel ) ); \
		(xfer)->xferUser( (ptr), (size), _crcLabel ); \
	} while(0)

#define CRC_XFER_SNAPSHOT(xfer, className, snapshot, memberName) \
	do { \
		char _crcLabel[128]; \
		Xfer::buildCRCLabel( className, memberName, "Snapshot", _crcLabel, sizeof( _crcLabel ) ); \
		(xfer)->xferSnapshot( (snapshot), _crcLabel ); \
	} while(0)
