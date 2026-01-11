/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#include "PreRTS.h"

#include "Common/SimulationMatrixCrc.h"
#include "Common/XferCRC.h"
#include "WWMath/matrix3d.h"
#include "WWMath/wwmath.h"

#include <math.h>
#include <stdio.h>

namespace
{
struct FpRoundingMode
{
	UnsignedInt mode;
};

struct FpPrecisionMode
{
	UnsignedInt mode;
	bool use_precision_control;
};

void append_matrix_crc(XferCRC &xfer)
{
	Matrix3D matrix;
	Matrix3D factors_matrix;

	matrix.Set(
		4.1f, 1.2f, 0.3f, 0.4f,
		0.5f, 3.6f, 0.7f, 0.8f,
		0.9f, 1.0f, 2.1f, 1.2f);

	factors_matrix.Set(
			WWMath::Cos(-0.0) * WWMath::Sin(0.7f) * log10(2.3f),
		WWMath::Sin(-1.0) * WWMath::Cos(1.1f) * pow(1.1f, 2.0f),
		tan(0.3f),
		WWMath::Asin(0.5f),
		WWMath::Acos(-0.3f),
		WWMath::Atan(0.9f) * pow(1.1f, 2.0f),
		WWMath::Atan2(0.4f, 1.3f),
		sinh(0.2f),
		cosh(0.4f),
		tanh(0.5f),
		exp(0.1f) * log10(2.3f),
		log(1.4f));

	Matrix3D::Multiply(matrix, factors_matrix, &matrix);
	matrix.Get_Inverse(matrix);

	factors_matrix.Set(
		sqrtf(55788.84375), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	);

	xfer.xferMatrix3D(&matrix);
	xfer.xferMatrix3D(&factors_matrix);
}

void run_with_modes(XferCRC &xfer, UnsignedInt rounding_mode, const FpPrecisionMode &precision_mode)
{
	_fpreset();
	UnsignedInt curVal = _statusfp();
	UnsignedInt newVal = curVal;
	newVal = (newVal & ~_MCW_RC) | (rounding_mode & _MCW_RC);
	if (precision_mode.use_precision_control) {
		newVal = (newVal & ~_MCW_PC) | (precision_mode.mode & _MCW_PC);
	}
	_controlfp(newVal, precision_mode.use_precision_control ? (_MCW_PC | _MCW_RC) : _MCW_RC);

	append_matrix_crc(xfer);

	_fpreset();
}
}

UnsignedInt SimulationMatrixCrc::calculate()
{
	static const FpRoundingMode kRoundingModes[] = {
		{ _RC_NEAR },
	};
	static const FpPrecisionMode kPrecisionModes[] = {
		{ _PC_24, true },
	};

	XferCRC xfer;
	xfer.open("SimulationMatrixCrc");

	for (size_t i = 0; i < sizeof(kRoundingModes) / sizeof(kRoundingModes[0]); ++i) {
		for (size_t j = 0; j < sizeof(kPrecisionModes) / sizeof(kPrecisionModes[0]); ++j) {
			run_with_modes(xfer, kRoundingModes[i].mode, kPrecisionModes[j]);
		}
	}

	xfer.close();

	return xfer.getCRC();
}

void SimulationMatrixCrc::print()
{
	UnsignedInt crc = calculate();
	printf("Simulation CRC: %08X %u\n", crc, crc);
}
