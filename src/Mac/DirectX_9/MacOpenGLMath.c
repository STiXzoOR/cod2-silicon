#include "common_types.h"
#include "imports.h"
#if defined(COD2_X64)
#include <math.h>
#endif

static float *COpenGLMatrix_Data(COpenGLMatrix *matrix)
{
    return (float *)matrix;
}

static const float *COpenGLMatrix_ConstData(const COpenGLMatrix *matrix)
{
    return (const float *)matrix;
}

static float Matrix4_Abs(float value)
{
    return value < 0.0f ? -value : value;
}

static void Matrix4_Copy(float *dst, const float *src)
{
    int i;

    for (i = 0; i < 16; ++i) {
        dst[i] = src[i];
    }
}

static void Matrix4_SetIdentity(float *matrix)
{
    int i;

    for (i = 0; i < 16; ++i) {
        matrix[i] = 0.0f;
    }

    matrix[0] = 1.0f;
    matrix[5] = 1.0f;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;
}

static void Matrix4_SwapRows(float matrix[4][8], int rowA, int rowB)
{
    int column;

    for (column = 0; column < 8; ++column) {
        float temp;

        temp = matrix[rowA][column];
        matrix[rowA][column] = matrix[rowB][column];
        matrix[rowB][column] = temp;
    }
}

void COpenGLMatrix_SetIdentity(const COpenGLMatrix *_this)
{
    Matrix4_SetIdentity(COpenGLMatrix_Data((COpenGLMatrix *)_this));
}

void COpenGLMatrix_Transpose(const COpenGLMatrix *_this)
{
    float *matrix;
    int row;

    matrix = COpenGLMatrix_Data((COpenGLMatrix *)_this);

    for (row = 0; row < 4; ++row) {
        int column;

        for (column = row + 1; column < 4; ++column) {
            float temp;

            temp = matrix[row * 4 + column];
            matrix[row * 4 + column] = matrix[column * 4 + row];
            matrix[column * 4 + row] = temp;
        }
    }
}

#if defined(COD2_X64)
static float Matrix4_MinorDeterminant(const float *matrix, int excludedRow, int excludedColumn)
{
    float minor[9];
    int index = 0;

    for (int row = 0; row < 4; ++row) {
        if (row == excludedRow) {
            continue;
        }
        for (int column = 0; column < 4; ++column) {
            if (column != excludedColumn) {
                minor[index++] = matrix[row * 4 + column];
            }
        }
    }

    float first = (minor[4] * minor[8] - minor[7] * minor[5]) * minor[0];
    float second = (minor[8] * minor[3] - minor[5] * minor[6]) * minor[1];
    float third = (minor[7] * minor[3] - minor[4] * minor[6]) * minor[2];
    return (first - second) + third;
}

bool COpenGLMatrix_Inverse(const COpenGLMatrix *_this, COpenGLMatrix *mResult)
{
    const float *matrix = COpenGLMatrix_ConstData(_this);
    float determinant = 0.0f;
    float sign = 1.0f;

    for (int column = 0; column < 4; ++column) {
        float term = Matrix4_MinorDeterminant(matrix, 0, column) * matrix[column];
        determinant = determinant + term * sign;
        sign = -sign;
    }

    if (fabsf(determinant) < 0.0005f) {
        return 0;
    }

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float cofactor = Matrix4_MinorDeterminant(matrix, row, column);
            cofactor = cofactor * (float)(1 - 2 * ((row + column) % 2));
            COpenGLMatrix_Data(mResult)[column * 4 + row] = cofactor / determinant;
        }
    }
    return 1;
}
#else
bool COpenGLMatrix_Inverse(const COpenGLMatrix *_this, COpenGLMatrix *mResult)
{
    float augmented[4][8];
    int column;
    int row;

    for (row = 0; row < 4; ++row) {
        int columnIndex;

        for (columnIndex = 0; columnIndex < 4; ++columnIndex) {
            augmented[row][columnIndex] = COpenGLMatrix_ConstData(_this)[row * 4 + columnIndex];
            augmented[row][columnIndex + 4] = row == columnIndex ? 1.0f : 0.0f;
        }
    }

    for (column = 0; column < 4; ++column) {
        int pivotRow;
        float pivotAbs;
        float pivotValue;

        pivotRow = column;
        pivotAbs = Matrix4_Abs(augmented[column][column]);

        for (row = column + 1; row < 4; ++row) {
            float candidateAbs;

            candidateAbs = Matrix4_Abs(augmented[row][column]);
            if (candidateAbs > pivotAbs) {
                pivotAbs = candidateAbs;
                pivotRow = row;
            }
        }

        if (pivotAbs < 0.0005f) {
            return 0;
        }

        if (pivotRow != column) {
            Matrix4_SwapRows(augmented, column, pivotRow);
        }

        pivotValue = augmented[column][column];
        for (row = 0; row < 8; ++row) {
            augmented[column][row] /= pivotValue;
        }

        for (row = 0; row < 4; ++row) {
            int rowColumn;
            float factor;

            if (row == column) {
                continue;
            }

            factor = augmented[row][column];
            if (factor == 0.0f) {
                continue;
            }

            for (rowColumn = 0; rowColumn < 8; ++rowColumn) {
                augmented[row][rowColumn] -= factor * augmented[column][rowColumn];
            }
        }
    }

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 4; ++column) {
            COpenGLMatrix_Data(mResult)[row * 4 + column] = augmented[row][column + 4];
        }
    }

    return 1;
}
#endif
