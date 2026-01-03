#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WWMath/matrix3d.h"
#include "WWMath/vector3.h"

struct Result
{
	Matrix3D matrix;
	Vector3 vector;
	unsigned int checksum;
};

static unsigned int float_bits(float value)
{
	union
	{
		float f;
		unsigned int u;
	} bits;
	bits.f = value;
	return bits.u;
}

static unsigned int mix_hash(unsigned int hash, unsigned int value)
{
	hash = (hash << 5) | (hash >> 27);
	hash ^= value + 0x9e3779b9u;
	return hash;
}

static unsigned int hash_result(const Matrix3D &m, const Vector3 &v)
{
	unsigned int hash = 0;
	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			hash = mix_hash(hash, float_bits(m[row][col]));
		}
	}
	hash = mix_hash(hash, float_bits(v.X));
	hash = mix_hash(hash, float_bits(v.Y));
	hash = mix_hash(hash, float_bits(v.Z));
	return hash;
}

static void set_fp_precision(int bits)
{
#if defined(_MSC_VER)
	_fpreset();
	unsigned int new_val = _controlfp(0, 0);
	new_val = (new_val & ~_MCW_RC) | (_RC_NEAR & _MCW_RC);
	unsigned int pc = _PC_24;
	if (bits == 53)
	{
		pc = _PC_53;
	}
	else if (bits == 64)
	{
		pc = _PC_64;
	}
	new_val = (new_val & ~_MCW_PC) | (pc & _MCW_PC);
	_controlfp(new_val, _MCW_PC | _MCW_RC);
#else
	(void)bits;
#endif
}

static unsigned int get_control_word(void)
{
#if defined(_MSC_VER)
	return _controlfp(0, 0);
#else
	return 0;
#endif
}

static Result run_test(int iterations)
{
	Matrix3D acc(true);
	Matrix3D step(true);
	Vector3 vec(1.0f, 2.0f, 3.0f);

	float t = 0.001f;
	const float deg_to_rad = 0.017453292519943295f;

	for (int i = 0; i < iterations; ++i)
	{
		float angle = (float)((i % 360) * deg_to_rad);
		step.Make_Identity();
		step.Rotate_X(angle);
		step.Rotate_Y(angle * 0.5f);
		step.Rotate_Z(angle * 0.25f);
		step.Translate(t, t * 2.0f, -t * 0.5f);

		acc = acc * step;
		vec = acc * vec;
		acc.Translate(vec);

		t += 0.0001f;
	}

	Result result;
	result.matrix = acc;
	result.vector = vec;
	result.checksum = hash_result(acc, vec);
	return result;
}

static void print_result(const Result &result)
{
	printf("checksum: 0x%08X\n", result.checksum);
	for (int row = 0; row < 3; ++row)
	{
		printf("row%d:", row);
		for (int col = 0; col < 4; ++col)
		{
			float value = result.matrix[row][col];
			printf(" % .9g/%08X", value, float_bits(value));
		}
		printf("\n");
	}
	printf("vec: % .9g/%08X % .9g/%08X % .9g/%08X\n",
		result.vector.X, float_bits(result.vector.X),
		result.vector.Y, float_bits(result.vector.Y),
		result.vector.Z, float_bits(result.vector.Z));
}

static float absf(float value)
{
	return (value < 0.0f) ? -value : value;
}

static void compare_results(const Result &a, const Result &b)
{
	float max_matrix_diff = 0.0f;
	float max_vector_diff = 0.0f;
	int matrix_bit_diffs = 0;
	int vector_bit_diffs = 0;

	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			float av = a.matrix[row][col];
			float bv = b.matrix[row][col];
			float diff = absf(av - bv);
			if (diff > max_matrix_diff)
			{
				max_matrix_diff = diff;
			}
			if (float_bits(av) != float_bits(bv))
			{
				++matrix_bit_diffs;
			}
		}
	}

	float av = a.vector.X;
	float bv = b.vector.X;
	float diff = absf(av - bv);
	if (diff > max_vector_diff)
	{
		max_vector_diff = diff;
	}
	if (float_bits(av) != float_bits(bv))
	{
		++vector_bit_diffs;
	}

	av = a.vector.Y;
	bv = b.vector.Y;
	diff = absf(av - bv);
	if (diff > max_vector_diff)
	{
		max_vector_diff = diff;
	}
	if (float_bits(av) != float_bits(bv))
	{
		++vector_bit_diffs;
	}

	av = a.vector.Z;
	bv = b.vector.Z;
	diff = absf(av - bv);
	if (diff > max_vector_diff)
	{
		max_vector_diff = diff;
	}
	if (float_bits(av) != float_bits(bv))
	{
		++vector_bit_diffs;
	}

	printf("compare: checksumA=0x%08X checksumB=0x%08X\n", a.checksum, b.checksum);
	printf("compare: max_matrix_diff=% .9g matrix_bit_diffs=%d\n", max_matrix_diff, matrix_bit_diffs);
	printf("compare: max_vector_diff=% .9g vector_bit_diffs=%d\n", max_vector_diff, vector_bit_diffs);
}

static void print_usage(const char *exe)
{
	printf("Usage: %s [--iterations N] [--precision 24|53|64] [--compare-precision 24|53|64]\n", exe);
}

int main(int argc, char **argv)
{
	int iterations = 20000;
	int precision = 24;
	int compare_precision = 0;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc)
		{
			iterations = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--precision") == 0 && i + 1 < argc)
		{
			precision = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--compare-precision") == 0 && i + 1 < argc)
		{
			compare_precision = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "--help") == 0)
		{
			print_usage(argv[0]);
			return 0;
		}
		else
		{
			print_usage(argv[0]);
			return 1;
		}
	}

	printf("iterations: %d\n", iterations);
	printf("precision: %d\n", precision);
	set_fp_precision(precision);
	printf("control_word: 0x%08X\n", get_control_word());

	Result primary = run_test(iterations);
	print_result(primary);

	if (compare_precision != 0 && compare_precision != precision)
	{
		printf("\ncompare_precision: %d\n", compare_precision);
		set_fp_precision(compare_precision);
		printf("control_word: 0x%08X\n", get_control_word());
		Result secondary = run_test(iterations);
		print_result(secondary);
		compare_results(primary, secondary);
	}

	return 0;
}
