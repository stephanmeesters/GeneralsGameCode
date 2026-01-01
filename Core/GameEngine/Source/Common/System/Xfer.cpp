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

// FILE: Xfer.cpp /////////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, February 2002
// Desc:   The Xfer system is capable of setting up operations to work with blocks of data
//				 from other subsystems.  It can work things such as file reading, file writing,
//				 CRC computations etc
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include <Utility/stdio_adapter.h>
#include <string.h>
#include "Common/Upgrade.h"
#include "Common/GameState.h"
#include "Common/Xfer.h"
#include "Common/BitFlagsIO.h"


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Xfer::Xfer( void )
{

	m_options = XO_NONE;
	m_xferMode = XFER_INVALID;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Xfer::~Xfer( void )
{

}

// ------------------------------------------------------------------------------------------------
/** Open */
// ------------------------------------------------------------------------------------------------
void Xfer::open( AsciiString identifier )
{

	// save identifier
	m_identifier = identifier;

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferByte( Byte *byteData, const char *label )
{

	xferImplementation( byteData, sizeof( Byte ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(*byteData) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferVersion( XferVersion *versionData, XferVersion currentVersion, const char *label )
{

	xferImplementation( versionData, sizeof( XferVersion ) );

	// sanity, after the xfer, version data is never allowed to be higher than the current version
	if( *versionData > currentVersion )
	{

		DEBUG_CRASH(( "XferVersion - Unknown version '%d' should be no higher than '%d'",
									*versionData, currentVersion ));
		throw XFER_INVALID_VERSION;

	}
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%u", static_cast<UnsignedInt>(*versionData) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferUnsignedByte( UnsignedByte *unsignedByteData, const char *label )
{

	xferImplementation( unsignedByteData, sizeof( UnsignedByte ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%u", static_cast<UnsignedInt>(*unsignedByteData) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferBool( Bool *boolData, const char *label )
{

	xferImplementation( boolData, sizeof( Bool ) );
	if( getXferMode() == XFER_CRC )
	{
		logCRCValue( label, (*boolData) ? "1" : "0" );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferInt( Int *intData, const char *label )
{

	xferImplementation( intData, sizeof( Int ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%d", *intData );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferInt64( Int64 *int64Data, const char *label )
{

	xferImplementation( int64Data, sizeof( Int64 ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[64];
		snprintf( buffer, sizeof( buffer ), "%I64d", static_cast<Int64>(*int64Data) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferUnsignedInt( UnsignedInt *unsignedIntData, const char *label )
{

	xferImplementation( unsignedIntData, sizeof( UnsignedInt ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%u", *unsignedIntData );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferShort( Short *shortData, const char *label )
{

	xferImplementation( shortData, sizeof( Short ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(*shortData) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferUnsignedShort( UnsignedShort *unsignedShortData, const char *label )
{

	xferImplementation( unsignedShortData, sizeof( UnsignedShort ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%u", static_cast<UnsignedInt>(*unsignedShortData) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferReal( Real *realData, const char *label )
{

	xferImplementation( realData, sizeof( Real ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[64];
		snprintf( buffer, sizeof( buffer ), "%.9g", static_cast<double>(*realData) );
		logCRCValue( label, buffer );
		logCRCBytes(label, buffer, sizeof( buffer ) );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferMapName( AsciiString *mapNameData, const char *label )
{
	if (getXferMode() == XFER_SAVE)
	{
		AsciiString tmp = TheGameState->realMapPathToPortableMapPath(*mapNameData);
		xferAsciiString(&tmp, label);
	}
	else if (getXferMode() == XFER_LOAD)
	{
		xferAsciiString(mapNameData, label);
		*mapNameData = TheGameState->portableMapPathToRealMapPath(*mapNameData);
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferAsciiString( AsciiString *asciiStringData, const char *label )
{

	xferImplementation( (void *)asciiStringData->str(), sizeof( Byte ) * asciiStringData->getLength() );
	if( getXferMode() == XFER_CRC )
	{
		logCRCValue( label, asciiStringData->str() );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferMarkerLabel( AsciiString asciiStringData, const char *label )
{
	if( getXferMode() == XFER_CRC )
	{
		logCRCValue( label, asciiStringData.str() );
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferUnicodeString( UnicodeString *unicodeStringData, const char *label )
{

	xferImplementation( (void *)unicodeStringData->str(), sizeof( WideChar ) * unicodeStringData->getLength() );
	if( getXferMode() == XFER_CRC )
	{
		logCRCBytes( label, unicodeStringData->str(), sizeof( WideChar ) * unicodeStringData->getLength() );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferCoord3D( Coord3D *coord3D, const char *label )
{

	xferReal( &coord3D->x, label );
	xferReal( &coord3D->y, label );
	xferReal( &coord3D->z, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferICoord3D( ICoord3D *iCoord3D, const char *label )
{

	xferInt( &iCoord3D->x, label );
	xferInt( &iCoord3D->y, label );
	xferInt( &iCoord3D->z, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferRegion3D( Region3D *region3D, const char *label )
{

	xferCoord3D( &region3D->lo, label );
	xferCoord3D( &region3D->hi, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferIRegion3D( IRegion3D *iRegion3D, const char *label )
{

	xferICoord3D( &iRegion3D->lo, label );
	xferICoord3D( &iRegion3D->hi, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferCoord2D( Coord2D *coord2D, const char *label )
{

	xferReal( &coord2D->x, label );
	xferReal( &coord2D->y, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferICoord2D( ICoord2D *iCoord2D, const char *label )
{

	xferInt( &iCoord2D->x, label );
	xferInt( &iCoord2D->y, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferRegion2D( Region2D *region2D, const char *label )
{

	xferCoord2D( &region2D->lo, label );
	xferCoord2D( &region2D->hi, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferIRegion2D( IRegion2D *iRegion2D, const char *label )
{

	xferICoord2D( &iRegion2D->lo, label );
	xferICoord2D( &iRegion2D->hi, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferRealRange( RealRange *realRange, const char *label )
{

	xferReal( &realRange->lo, label );
	xferReal( &realRange->hi, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferColor( Color *color, const char *label )
{

	xferImplementation( color, sizeof( Color ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(*color) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferRGBColor( RGBColor *rgbColor, const char *label )
{

	xferReal( &rgbColor->red, label );
	xferReal( &rgbColor->green, label );
	xferReal( &rgbColor->blue, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferRGBAColorReal( RGBAColorReal *rgbaColorReal, const char *label )
{

	xferReal( &rgbaColorReal->red, label );
	xferReal( &rgbaColorReal->green, label );
	xferReal( &rgbaColorReal->blue, label );
	xferReal( &rgbaColorReal->alpha, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferRGBAColorInt( RGBAColorInt *rgbaColorInt, const char *label )
{

	xferUnsignedInt( &rgbaColorInt->red, label );
	xferUnsignedInt( &rgbaColorInt->green, label );
	xferUnsignedInt( &rgbaColorInt->blue, label );
	xferUnsignedInt( &rgbaColorInt->alpha, label );

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferObjectID( ObjectID *objectID, const char *label )
{

	xferImplementation( objectID, sizeof( ObjectID ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(*objectID) );
		logCRCValue( label, buffer );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferDrawableID( DrawableID *drawableID, const char *label )
{

	xferImplementation( drawableID, sizeof( DrawableID ) );
	if( getXferMode() == XFER_CRC )
	{
		char buffer[32];
		snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(*drawableID) );
		logCRCValue( label, buffer );
	}

}


// ------------------------------------------------------------------------------------------------
void Xfer::xferSTLObjectIDVector( std::vector<ObjectID> *objectIDVectorData, const char *label )
{
	//
	// the fact that this is a list and a little higher level than a simple data type
	// is reason enough to have every one of these versioned
	//
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xferVersion( &version, currentVersion, label );

	// xfer the count of the vector
	UnsignedShort listCount = objectIDVectorData->size();
	xferUnsignedShort( &listCount, label );

	// xfer vector data
	ObjectID objectID;
	if( getXferMode() == XFER_SAVE || getXferMode() == XFER_CRC )
	{

		// save all ids
		std::vector< ObjectID >::const_iterator it;
		for( it = objectIDVectorData->begin(); it != objectIDVectorData->end(); ++it )
		{

			objectID = *it;
			xferObjectID( &objectID, label );

		}

	}
	else if( getXferMode() == XFER_LOAD )
	{

		// sanity, the list should be empty before we transfer more data into it
		if( !objectIDVectorData->empty() )
		{

			DEBUG_CRASH(( "Xfer::xferSTLObjectIDList - object vector should be empty before loading" ));
			throw XFER_LIST_NOT_EMPTY;

		}

		// read all ids
		for( UnsignedShort i = 0; i < listCount; ++i )
		{

			xferObjectID( &objectID, label );
			objectIDVectorData->push_back( objectID );

		}

	}
	else
	{

		DEBUG_CRASH(( "xferSTLObjectIDList - Unknown xfer mode '%d'", getXferMode() ));
		throw XFER_MODE_UNKNOWN;

	}
}

// ------------------------------------------------------------------------------------------------
/** STL Object ID list (cause it's a common data structure we use a lot)
	* Version Info;
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void Xfer::xferSTLObjectIDList( std::list< ObjectID > *objectIDListData, const char *label )
{

	//
	// the fact that this is a list and a little higher level than a simple data type
	// is reason enough to have every one of these versioned
	//
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xferVersion( &version, currentVersion, label );

	// xfer the count of the list
	UnsignedShort listCount = objectIDListData->size();
	xferUnsignedShort( &listCount, label );

	// xfer list data
	ObjectID objectID;
	if( getXferMode() == XFER_SAVE || getXferMode() == XFER_CRC )
	{

		// save all ids
		std::list< ObjectID >::const_iterator it;
		for( it = objectIDListData->begin(); it != objectIDListData->end(); ++it )
		{

			objectID = *it;
			xferObjectID( &objectID, label );

		}

	}
	else if( getXferMode() == XFER_LOAD )
	{

		// sanity, the list should be empty before we transfer more data into it
		if( !objectIDListData->empty() )
		{

			DEBUG_CRASH(( "Xfer::xferSTLObjectIDList - object list should be empty before loading" ));
			throw XFER_LIST_NOT_EMPTY;

		}

		// read all ids
		for( UnsignedShort i = 0; i < listCount; ++i )
		{

			xferObjectID( &objectID, label );
			objectIDListData->push_back( objectID );

		}

	}
	else
	{

		DEBUG_CRASH(( "xferSTLObjectIDList - Unknown xfer mode '%d'", getXferMode() ));
		throw XFER_MODE_UNKNOWN;

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferSTLIntList( std::list< Int > *intListData, const char *label )
{

	// sanity
	if( intListData == NULL )
		return;

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xferVersion( &version, currentVersion, label );

	// xfer the count of the list
	UnsignedShort listCount = intListData->size();
	xferUnsignedShort( &listCount, label );

	// xfer list data
	Int intData;
	if( getXferMode() == XFER_SAVE || getXferMode() == XFER_CRC )
	{

		// save all ids
		std::list< Int >::const_iterator it;
		for( it = intListData->begin(); it != intListData->end(); ++it )
		{

			intData = *it;
			xferInt( &intData, label );

		}

	}
	else if( getXferMode() == XFER_LOAD )
	{

		// sanity, the list should be empty before we transfer more data into it
		if( !intListData->empty() )
		{

			DEBUG_CRASH(( "Xfer::xferSTLIntList - int list should be empty before loading" ));
			throw XFER_LIST_NOT_EMPTY;

		}

		// read all ids
		for( UnsignedShort i = 0; i < listCount; ++i )
		{

			xferInt( &intData, label );
			intListData->push_back( intData );

		}

	}
	else
	{

		DEBUG_CRASH(( "xferSTLIntList - Unknown xfer mode '%d'", getXferMode() ));
		throw XFER_MODE_UNKNOWN;

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferScienceType( ScienceType *science, const char *label )
{

	// sanity
	DEBUG_ASSERTCRASH( science != NULL, ("xferScienceType - Invalid parameters") );

	AsciiString scienceName;

	if( getXferMode() == XFER_SAVE )
	{
		// translate to string
		scienceName = TheScienceStore->getInternalNameForScience( *science );

		// write the string
		xferAsciiString( &scienceName, label );

	}
	else if( getXferMode() == XFER_LOAD )
	{
		xferAsciiString( &scienceName, label );

		// translate to science
		*science = TheScienceStore->getScienceFromInternalName( scienceName );
		if( *science == SCIENCE_INVALID )
		{

			DEBUG_CRASH(( "xferScienceType - Unknown science '%s'", scienceName.str() ));
			throw XFER_UNKNOWN_STRING;

		}

	}
	else if( getXferMode() == XFER_CRC )
	{
			xferImplementation( science, sizeof( *science ) );
			if( getXferMode() == XFER_CRC )
			{
				char buffer[32];
				snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(*science) );
				logCRCValue( label, buffer );
			}

	}
	else
	{

		DEBUG_CRASH(( "xferScienceType - Unknown xfer mode '%d'", getXferMode() ));
		throw XFER_MODE_UNKNOWN;

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferScienceVec( ScienceVec *scienceVec, const char *label )
{

	// sanity
	DEBUG_ASSERTCRASH( scienceVec != NULL, ("xferScienceVec - Invalid parameters") );

	// this deserves a version number
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xferVersion( &version, currentVersion, label );

	// count of vector
	UnsignedShort count = scienceVec->size();
	xferUnsignedShort( &count, label );

	if( getXferMode() == XFER_SAVE )
	{
		for( ScienceVec::const_iterator it = scienceVec->begin(); it != scienceVec->end(); ++it )
		{
			ScienceType science = *it;
			xferScienceType(&science, label);
		}
	}
	else if( getXferMode() == XFER_LOAD )
	{
		// vector should be empty at this point
		if( scienceVec->empty() == FALSE )
		{
			// Not worth an assert, since things can give you Sciences on creation.  Just handle it and load.
			scienceVec->clear();

			// Homework for today.  Write 2000 words reconciling "Your code must never crash" with "Intentionally putting crashes in the code".  Fucktard.
//			DEBUG_CRASH(( "xferScienceVec - vector is not empty, but should be" ));
//			throw XFER_LIST_NOT_EMPTY;
		}

		for( UnsignedShort i = 0; i < count; ++i )
		{
			ScienceType science;
			xferScienceType(&science, label);
			scienceVec->push_back( science );
		}

	}
	else if( getXferMode() == XFER_CRC )
	{
		for( ScienceVec::const_iterator it = scienceVec->begin(); it != scienceVec->end(); ++it )
		{
			ScienceType science = *it;
			xferImplementation( &science, sizeof( ScienceType ) );
			if( getXferMode() == XFER_CRC )
			{
				char buffer[32];
				snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(science) );
				logCRCValue( label, buffer );
			}
		}
	}
	else
	{

		DEBUG_CRASH(( "xferScienceVec - Unknown xfer mode '%d'", getXferMode() ));
		throw XFER_MODE_UNKNOWN;

	}

}

// ------------------------------------------------------------------------------------------------
/** kind of type, for load/save it is xfered as a string so we can reorder the
	* kindofs if we like
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void Xfer::xferKindOf( KindOfType *kindOfData, const char *label )
{

	// this deserves a version number
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xferVersion( &version, currentVersion, label );

	// check which type of xfer we're doing
	if( getXferMode() == XFER_SAVE )
	{

		// save as an ascii string
		AsciiString kindOfName = KindOfMaskType::getNameFromSingleBit(*kindOfData);
		xferAsciiString( &kindOfName, label );

	}
	else if( getXferMode() == XFER_LOAD )
	{

		// read ascii string from file
		AsciiString kindOfName;
		xferAsciiString( &kindOfName, label );

		// turn kind of name into an enum value
		Int bit = KindOfMaskType::getSingleBitFromName(kindOfName.str());
		if (bit != -1)
			*kindOfData = (KindOfType)bit;

	}
	else if( getXferMode() == XFER_CRC )
	{

		// just call the xfer implementation on the data values
		xferImplementation( kindOfData, sizeof( KindOfType ) );
		if( getXferMode() == XFER_CRC )
		{
			char buffer[32];
			snprintf( buffer, sizeof( buffer ), "%d", static_cast<Int>(*kindOfData) );
			logCRCValue( label, buffer );
		}

	}
	else
	{

		DEBUG_CRASH(( "xferKindOf - Unknown xfer mode '%d'", getXferMode() ));
		throw XFER_MODE_UNKNOWN;

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferUpgradeMask( UpgradeMaskType *upgradeMaskData, const char *label )
{

	// this deserves a version number
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xferVersion( &version, currentVersion, label );

	//Kris: The Upgrade system has been converted from Int64 to BitFlags. However because the
	//names of upgrades are saved to preserve order reassignments (inserting a new upgrade in
	//the INI file will skew the bit values), we must continue saving the names of the upgrades
	//in order to recalculate the actual bit value of said upgrade.
	//---------------------------------------------------------------------------------------------
	//NOTE: The xfer code didn't have to change with the bitset upgrades, because either way, we're
	//converting data <-> Ascii, so the minor syntax works with the before and after code!

	// check which type of xfer we're doing
	if( getXferMode() == XFER_SAVE )
	{
		AsciiString upgradeName;

		// count how many bits are set in the mask
		UnsignedShort count = 0;
		UpgradeTemplate *upgradeTemplate;
		for( upgradeTemplate = TheUpgradeCenter->firstUpgradeTemplate(); upgradeTemplate; upgradeTemplate = upgradeTemplate->friend_getNext() )
		{
			// if the mask of this upgrade is set, it counts
			if( upgradeMaskData->testForAll( upgradeTemplate->getUpgradeMask() ) )
			{
				count++;
			}
		}

		// write the count
		xferUnsignedShort( &count, label );

		// write out the upgrades as strings
		for( upgradeTemplate = TheUpgradeCenter->firstUpgradeTemplate(); upgradeTemplate; upgradeTemplate = upgradeTemplate->friend_getNext() )
		{
			// if the mask of this upgrade is set, it counts
			if( upgradeMaskData->testForAll( upgradeTemplate->getUpgradeMask() ) )
			{
				upgradeName = upgradeTemplate->getUpgradeName();
				xferAsciiString( &upgradeName, label );
			}
		}
	}
	else if( getXferMode() == XFER_LOAD )
	{
		AsciiString upgradeName;
		const UpgradeTemplate *upgradeTemplate;

		// how many strings are we going to read from the file
		UnsignedShort count;
		xferUnsignedShort( &count, label );

		// zero the mask data
		upgradeMaskData->clear();

		// read all the strings and set the mask vaules
		for( UnsignedShort i = 0; i < count; ++i )
		{

			// read the string
			xferAsciiString( &upgradeName, label );

			// find this upgrade template
			upgradeTemplate = TheUpgradeCenter->findUpgrade( upgradeName );
			if( upgradeTemplate == NULL )
			{

				DEBUG_CRASH(( "Xfer::xferUpgradeMask - Unknown upgrade '%s'", upgradeName.str() ));
				throw XFER_UNKNOWN_STRING;

			}

			// set the mask data
			upgradeMaskData->set( upgradeTemplate->getUpgradeMask() );

		}

	}
	else if( getXferMode() == XFER_CRC )
	{

		// just xfer implementation the data itself
		xferImplementation( upgradeMaskData, sizeof( UpgradeMaskType ) );
		if( getXferMode() == XFER_CRC )
		{
			logCRCBytes( label, upgradeMaskData, sizeof( UpgradeMaskType ) );
		}

	}
	else
	{

		DEBUG_CRASH(( "xferUpgradeMask - Unknown xfer mode '%d'", getXferMode() ));
		throw XFER_MODE_UNKNOWN;

	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferUser( void *data, Int dataSize, const char *label )
{

	xferImplementation( data, dataSize );
	if( getXferMode() == XFER_CRC )
	{
		logCRCBytes( label, data, dataSize );
	}

}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::xferMatrix3D( Matrix3D* mtx, const char *label )
{
	// this deserves a version number
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xferVersion( &version, currentVersion, label );

 	Vector4& tmp0 = (*mtx)[0];
 	Vector4& tmp1 = (*mtx)[1];
 	Vector4& tmp2 = (*mtx)[2];

	xferReal(&tmp0.X, label);
	xferReal(&tmp0.Y, label);
	xferReal(&tmp0.Z, label);
	xferReal(&tmp0.W, label);

	xferReal(&tmp1.X, label);
	xferReal(&tmp1.Y, label);
	xferReal(&tmp1.Z, label);
	xferReal(&tmp1.W, label);

	xferReal(&tmp2.X, label);
	xferReal(&tmp2.Y, label);
	xferReal(&tmp2.Z, label);
	xferReal(&tmp2.W, label);
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::logCRCValue( const char *label, const char *valueText )
{
	(void)label;
	(void)valueText;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::logCRCBytes( const char *label, const void *data, Int dataSize )
{
	(void)label;
	(void)data;
	(void)dataSize;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void Xfer::buildCRCLabel( const char *className, const char *memberName, const char *typeName, char *out, size_t outSize )
{
	if( out == NULL || outSize == 0 )
	{
		return;
	}

	if( className == NULL || className[0] == '\0' )
	{
		className = "Unknown";
	}
	if( memberName == NULL )
	{
		memberName = "";
	}
	if( typeName == NULL )
	{
		typeName = "";
	}

	snprintf( out, outSize, "%s::%s::%s", className, memberName, typeName );
}


