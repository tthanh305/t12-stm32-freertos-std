/**
 * @file    curve_fitting.c
 * @brief   Hồi quy đa thức bậc 3 (polyfit) & Khử Gauss tính 4 hệ số nhiệt độ sz_p[0..3]
 * @note    Phụ trách: Dev 1 (Leader) trên nhánh feature/task-sensor
 *          Chuyển đổi chuẩn xác 100% từ CurveFitting.ino (Arduino v1.20) sang C tĩnh (không dùng Heap).
 */

#include "task_sensor_pid.h"
#include "app_cfg.h"
#include <math.h>
#include <stddef.h>

/* Số điểm hiệu chuẩn cố định (4 điểm) và bậc đa thức (bậc 3 -> 4 hệ số P0..P3) */
#define POLY_NUM_POINTS     ( 4 )
#define POLY_DIMENSION      ( 3 )
#define POLY_NUM_COEFFS     ( POLY_DIMENSION + 1 )     /* 4 hệ số: P0, P1, P2, P3 */
#define POLY_NUM_SUMXX      ( POLY_DIMENSION * 2 + 1 ) /* 7 tổng lũy thừa: x^0 .. x^6 */

/**
 * @brief Giải hệ phương trình tuyến tính A * x = b bằng phương pháp Khử Gauss có chọn phần tử trụ
 *        (Chuyển đổi từ hàm gauss_solve() trong CurveFitting.ino)
 * @param n Kích thước ma trận vuông (4)
 * @param A Ma trận hệ số 4x4 (ata)
 * @param x Vector nghiệm đầu ra (4 hệ số đa thức P[0..3])
 * @param b Vector vế phải (sumxy[0..3])
 */
static void prv_gauss_solve(int n, float A[POLY_NUM_COEFFS][POLY_NUM_COEFFS], float x[POLY_NUM_COEFFS], float b[POLY_NUM_COEFFS])
{
    int i, j, k, r;
    float max_val;

    for (k = 0; k < n - 1; k++)
    {
        /* 1. Tìm hàng có phần tử trụ (pivot) lớn nhất */
        max_val = fabsf(A[k][k]);
        r = k;
        for (i = k + 1; i < n - 1; i++)
        {
            if (max_val < fabsf(A[i][i]))
            {
                max_val = fabsf(A[i][i]);
                r = i;
            }
        }

        /* 2. Hoán đổi hàng r và hàng k (nếu cần) */
        if (r != k)
        {
            for (i = 0; i < n; i++)
            {
                max_val = A[k][i];
                A[k][i] = A[r][i];
                A[r][i] = max_val;
            }
            max_val = b[k];
            b[k] = b[r];
            b[r] = max_val;
        }

        /* Bảo vệ tránh chia cho 0 nếu ma trận suy biến */
        if (fabsf(A[k][k]) < 1e-12f)
        {
            continue;
        }

        /* 3. Khử Gauss đưa ma trận A về dạng tam giác trên */
        for (i = k + 1; i < n; i++)
        {
            for (j = k + 1; j < n; j++)
            {
                A[i][j] -= A[i][k] * A[k][j] / A[k][k];
            }
            b[i] -= A[i][k] * b[k] / A[k][k];
        }
    }

    /* 4. Thế ngược (Back substitution) để tìm nghiệm x[0..n-1] */
    for (i = n - 1; i >= 0; i--)
    {
        x[i] = b[i];
        for (j = i + 1; j < n; j++)
        {
            x[i] -= A[i][j] * x[j];
        }
        if (fabsf(A[i][i]) > 1e-12f)
        {
            x[i] /= A[i][i];
        }
        else
        {
            x[i] = 0.0f;
        }
    }
}

/**
 * @brief Hồi quy đa thức bậc 3 (Least-Squares Polynomial Fit) sử dụng mảng tĩnh trên Stack
 *        (Chuyển đổi từ hàm polyfit() trong CurveFitting.ino)
 * @param n      Số điểm dữ liệu (4)
 * @param x      Mảng 4 điểm chuẩn ADC {10, 350, 650, 950}
 * @param y      Mảng 4 điểm nhiệt độ chuẩn sz_temp[0..3] (°C)
 * @param poly_n Bậc đa thức (3)
 * @param p      Mảng xuất 4 hệ số P[0..3]
 */
static void prv_polyfit(int n, const int x[POLY_NUM_POINTS], const int y[POLY_NUM_POINTS], int poly_n, float p[POLY_NUM_COEFFS])
{
    int i, j;
    const int n_he_so = poly_n + 1;
    const int n_sumxx = poly_n * 2 + 1;

    float tempx[POLY_NUM_POINTS];
    float tempy[POLY_NUM_POINTS];
    float sumxx[POLY_NUM_SUMXX];
    float sumxy[POLY_NUM_COEFFS];
    float ata[POLY_NUM_COEFFS][POLY_NUM_COEFFS];

    /* 1. Khởi tạo mảng tạm tempx = 1.0, tempy = y[i] */
    for (i = 0; i < n; i++)
    {
        tempx[i] = 1.0f;
        tempy[i] = (float)y[i];
    }

    /* 2. Tính mảng sumxx: tổng lũy thừa x^0, x^1, ..., x^6 */
    for (i = 0; i < n_sumxx; i++)
    {
        sumxx[i] = 0.0f;
        for (j = 0; j < n; j++)
        {
            sumxx[i] += tempx[j];
            tempx[j] *= (float)x[j];
        }
    }

    /* 3. Tính mảng sumxy: tổng x^i * y (i = 0..3) */
    for (i = 0; i < n_he_so; i++)
    {
        sumxy[i] = 0.0f;
        for (j = 0; j < n; j++)
        {
            sumxy[i] += tempy[j];
            tempy[j] *= (float)x[j];
        }
    }

    /* 4. Xây dựng ma trận chuẩn A^T * A (4x4) từ sumxx */
    for (i = 0; i < n_he_so; i++)
    {
        for (j = 0; j < n_he_so; j++)
        {
            ata[i][j] = sumxx[i + j];
        }
    }

    /* 5. Giải hệ phương trình ata * p = sumxy */
    prv_gauss_solve(n_he_so, ata, p, sumxy);
}

/**
 * @brief Tính toán lại 4 hệ số đa thức bậc 3 (sz_p[0..3]) từ 4 điểm nhiệt độ chuẩn
 *        Công thức nội suy nhiệt độ mũi hàn:
 *        t12_temp = sz_p[3]*x^3 + sz_p[2]*x^2 + sz_p[1]*x + sz_p[0] + (tc1047_temp - tc1047_refer)
 */
void app_curve_recalculate(const uint16_t sz_temp[4], float sz_p_out[4])
{
    int i;
    float P[POLY_NUM_COEFFS];
    const int xx[POLY_NUM_POINTS] = {
        (int)T12_CALIB_ADC0,
        (int)T12_CALIB_ADC1,
        (int)T12_CALIB_ADC2,
        (int)T12_CALIB_ADC3
    };
    int yy[POLY_NUM_POINTS];

    if ((sz_temp == NULL) || (sz_p_out == NULL))
    {
        return;
    }

    for (i = 0; i < POLY_NUM_POINTS; i++)
    {
        yy[i] = (int)sz_temp[i];
    }

    prv_polyfit(POLY_NUM_POINTS, xx, yy, POLY_DIMENSION, P);

    sz_p_out[0] = P[0];
    sz_p_out[1] = P[1];
    sz_p_out[2] = P[2];
    sz_p_out[3] = P[3];
}
