#ifndef _NSFFT_H
#define _NSFFT_H

/*! \file nsfft.h
 * \brief Not-so-fast Fourier Transform.
 *  Generic, general purpose processor implementation
 *  as-per "Intro to Algorithms" by Cormen, Leiserson,
 *  Rivest, and Stein.  Still O(N logN) but not optimized
 *  or SIMD.
 *
 * Copyright 2017 Epiq Solutions, All Rights Reserved
 */

#include <math.h>
#include <stdbool.h>
#include <assert.h>

struct nsfft_obj
{
    int N;

    float *twiddles;

    unsigned int* bitReversedIndices;
    int stageCount; // log2(N)
};
typedef struct nsfft_obj Nsfft;

/*! \brief determine if FFT size is radix 2 */
static bool is_radix2( int N )
{
    // borrowed this check directly from ffts code
    return !(bool)(N & (N - 1));
}


static int local_log2( int N )
{
    int result = -1;
    while (N) {
        result += 1;
        N >>= 1;
    }
    return result;
}

static inline void complex_mult(float *x, float *y, float *z, bool conjugate_x)
{
    float xr = *x++;
    float xi = (conjugate_x) ? -(*x++) : *x++;
    float yr = *y++;
    float yi = *y++;
    *z++ = (xr * yr) - (xi * yi);
    *z++ = (xi * yr) + (xr * yi);
}

static inline void complex_mult_double(double *x, double *y, double *z, bool conjugate_x)
{
    double xr = *x++;
    double xi = (conjugate_x) ? -(*x++) : *x++;
    double yr = *y++;
    double yi = *y++;
    *z++ = (xr * yr) - (xi * yi);
    *z++ = (xi * yr) + (xr * yi);
}

static inline void complex_add(float *x, float *y, float *z)
{
    *z++ = *x++ + *y++;
    *z++ = *x++ + *y++;
}

static inline void complex_sub(float *x, float *y, float *z)
{
    *z++ = *x++ - *y++;
    *z++ = *x++ - *y++;
}

unsigned int reverseBits(unsigned int value, int nBits)
{
    unsigned int count = nBits;
    unsigned int reverseValue = 0;

    while (count--) {
       reverseValue <<= 1;
       reverseValue |= value & 1;
       value >>= 1;
    }
    return reverseValue;
}

// these are computed with double precision, but saved as float
static void precompute_twiddles( Nsfft *self, bool reverse )
{
    int twiddle_count = self->N * self->stageCount / 2;

    self->twiddles = (float*)malloc(2*twiddle_count*sizeof(float));
    assert(self->twiddles != NULL);

    float* twiddle_pointer = self->twiddles;
    int s = 0;
    const double twopi = 6.283185307179586;
    int m = 1;
    for (s = 1; s <= self->stageCount; ++s) {
        m = (2*m);  // m = 2**s
        double wm[2];
        wm[0] = cos(twopi/m);
        if (reverse) {
            wm[1] = sin(twopi/m);
        } else {
            wm[1] = -sin(twopi/m);
        }
        int k;
        for (k = 0; k < self->N; k += m) {
            double w[] = {1.0f, 0.0f};
            int j;
            for (j = 0; j < m/2; ++j) {
                // store
                *twiddle_pointer++ = w[0];
                *twiddle_pointer++ = w[1];
                // update
                complex_mult_double(w, wm, w, false);
            }
        }
    }
}

/*! \brief allocate memory for object */
Nsfft * new_Nsfft( int size, bool reverse )
{
    assert(is_radix2(size));
    Nsfft* self = (Nsfft*)malloc(sizeof(Nsfft));
    assert(self != NULL);
    self->N = size;

    self->bitReversedIndices = (unsigned int *)malloc(size*sizeof(unsigned int));
    assert(self->bitReversedIndices != NULL);

    self->stageCount = local_log2(size);
    unsigned int i;
    for (i = 0; i < size; ++i) {
        self->bitReversedIndices[i] = reverseBits(i, self->stageCount);
    }

    precompute_twiddles(self, reverse);

    return self;
}

/*! \brief delete memory for object */
void delete_Nsfft( Nsfft *self )
{
    free(self->bitReversedIndices);
    free(self->twiddles);
}

void load_bit_reversed( Nsfft* self, const float* input, float* destination)
{
    unsigned int i;
    unsigned int* index_pointer = self->bitReversedIndices;
    for (i = 0; i < self->N; ++i) {
        int index = *index_pointer++;
        destination[2*index] = *input++;
        destination[2*index+1] = *input++;
    }
}

void exec_Nsfft( Nsfft *self, const float* input, float* output )
{
    load_bit_reversed( self, input, output );

    float* A = output;
    int s = 0;
    int m = 1;
    float* w = self->twiddles;
    for (s = 1; s <= self->stageCount; ++s) {
        m = (2*m);  // m = 2**s
        int k;
        for (k = 0; k < self->N; k += m) {
            int j;
            for (j = 0; j < m/2; ++j) {
                float t[2];
                complex_mult(w, A + 2*(k+j+m/2), t, false);
                float u[2] = {A[2*(k+j)], A[2*(k+j)+1]};
                complex_add(u, t, A + 2*(k+j));
                complex_sub(u, t, A + 2*(k+j+m/2));
                w += 2;
            }
        }
    }
}

#endif /* _NSFFT_H */
