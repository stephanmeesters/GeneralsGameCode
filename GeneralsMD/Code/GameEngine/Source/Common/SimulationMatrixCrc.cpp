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

UnsignedInt SimulationMatrixCrc::calculate()
{
	Matrix3D matrix;
	Matrix3D factors_matrix;

	matrix.Set(
		4.1f, 1.2f, 0.3f, 0.4f,
		0.5f, 3.6f, 0.7f, 0.8f,
		0.9f, 1.0f, 2.1f, 1.2f);

	factors_matrix.Set(
			WWMath::Cos(-0.0) * WWMath::Sin(0.7f) * (float)log10(2.3f),
		WWMath::Sin(-1.0) * WWMath::Cos(1.1f) * (float)pow(1.1f, 2.0f),
		(float)tan(0.3f),
		WWMath::Asin(0.5f),
		WWMath::Acos(-0.3f),
		WWMath::Atan(0.9f) * (float)pow(1.1f, 2.0f),
		WWMath::Atan2(0.4f, 1.3f),
		(float)sinh(0.2f),
		(float)cosh(0.4f),
		(float)tanh(0.5f),
		(float)exp(0.1f) * (float)log10(2.3f),
		(float)log(1.4f));

	Matrix3D::Multiply(matrix, factors_matrix, &matrix);
	matrix.Get_Inverse(matrix);

	XferCRC xfer;
	xfer.open("SimulationMatrixCrc");
	xfer.xferMatrix3D(&matrix);
	xfer.close();

	return xfer.getCRC();
}

void SimulationMatrixCrc::print()
{
	UnsignedInt crc = calculate();
	printf("Simulation CRC: %08X\n", crc);
}
