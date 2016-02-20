/* Copyright (C) 2016 Doubango Telecom <https://www.doubango.org>
*
* This file is part of Open Source ComputerVision (a.k.a CompV) project.
* Source code hosted at https://github.com/DoubangoTelecom/compv
* Website hosted at http://compv.org
*
* CompV is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* CompV is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with CompV.
*/
#include "compv/intrinsics/x86/features/fast/compv_feature_fast_dete_intrin_sse.h"

#if defined(COMPV_ARCH_X86) && defined(COMPV_INTRINSIC)
#include "compv/intrinsics/x86/compv_intrin_sse.h"
#include "compv/compv_simd_globals.h"
#include "compv/compv_mathutils.h"
#include "compv/compv_bits.h"
#include "compv/compv_cpu.h"

#include <algorithm>

extern "C" const COMPV_ALIGN_DEFAULT() uint8_t kFast9Arcs[16][16];
extern "C" const COMPV_ALIGN_DEFAULT() uint8_t kFast12Arcs[16][16];
extern "C" const COMPV_ALIGN_DEFAULT() uint16_t Fast9Flags[16];
extern "C" const COMPV_ALIGN_DEFAULT() uint16_t Fast12Flags[16];

extern "C" void FastStrengths16(compv::compv_scalar_t rbrighters, compv::compv_scalar_t rdarkers, COMPV_ALIGNED(DEFAULT) const uint8_t* dbrighters16x16, COMPV_ALIGNED(DEFAULT) const uint8_t* ddarkers16x16, const compv::compv_scalar_t(*fbrighters16)[16], const compv::compv_scalar_t(*fdarkers16)[16], uint8_t* strengths16, compv::compv_scalar_t N);

COMPV_NAMESPACE_BEGIN()

// FUNCTION NOT USED
compv_scalar_t FastData_Intrin_SSE2(const uint8_t* dataPtr, COMPV_ALIGNED(SSE) const compv_scalar_t(&pixels16)[16], compv_scalar_t N, compv_scalar_t threshold, compv_scalar_t *pfdarkers, compv_scalar_t* pfbrighters, COMPV_ALIGNED(SSE) uint8_t(&ddarkers16)[16], COMPV_ALIGNED(SSE) uint8_t(&dbrighters16)[16])
{
    compv_scalar_t r = 0;

    uint8_t brighter = CompVMathUtils::clampPixel8(dataPtr[0] + (int16_t)threshold);
    uint8_t darker = CompVMathUtils::clampPixel8(dataPtr[0] - (int16_t)threshold);

    bool popcntHard = CompVCpu::isSupported(kCpuFlagPOPCNT);

    COMPV_ALIGN_SSE() uint8_t temp16[16] = {
        dataPtr[pixels16[0]],
        dataPtr[pixels16[1]],
        dataPtr[pixels16[2]],
        dataPtr[pixels16[3]],
        dataPtr[pixels16[4]],
        dataPtr[pixels16[5]],
        dataPtr[pixels16[6]],
        dataPtr[pixels16[7]],
        dataPtr[pixels16[8]],
        dataPtr[pixels16[9]],
        dataPtr[pixels16[10]],
        dataPtr[pixels16[11]],
        dataPtr[pixels16[12]],
        dataPtr[pixels16[13]],
        dataPtr[pixels16[14]],
        dataPtr[pixels16[15]],
    };
    __m128i xmmTemp16, xmmDarker, xmmBrighter, xmmZeros, xmmFF;
    __m128i (&xmmDdarkers16) = (__m128i (&))ddarkers16;
    __m128i (&xmmDbrighters16) = (__m128i (&))dbrighters16;

    _mm_store_si128(&xmmZeros, _mm_setzero_si128());

    _mm_store_si128(&xmmTemp16, _mm_load_si128((__m128i*)temp16));
    _mm_store_si128(&xmmDarker, _mm_set1_epi8((int8_t)darker));
    _mm_store_si128(&xmmBrighter, _mm_set1_epi8((int8_t)brighter));
    _mm_store_si128(&xmmFF, _mm_cmpeq_epi8(xmmZeros, xmmZeros));

    _mm_store_si128(&xmmDdarkers16, _mm_subs_epu8(xmmDarker, xmmTemp16));
    _mm_store_si128(&xmmDbrighters16, _mm_subs_epu8(xmmTemp16, xmmBrighter));
    // _mm_cmpgt_epi8 uses signed integers while we're using unsigned values and there is no _mm_cmpneq_epi8.
    *pfdarkers = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16, xmmZeros), xmmFF));
    *pfbrighters = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16, xmmZeros), xmmFF));

    // The flags contain int values with the highest bits always set -> we must use popcnt16 or at least popcnt32(flag&0xFFFF)
    compv_scalar_t popcnt0 = *pfdarkers ? compv_popcnt16(popcntHard, (unsigned short)*pfdarkers) : 0; // FIXME: popcnt
    compv_scalar_t popcnt1 = *pfbrighters ? compv_popcnt16(popcntHard, (unsigned short)*pfbrighters) : 0; // FIXME: popcnt
    if (popcnt0 >= N) {
        r |= (1 << 0);
    }
    if (popcnt1 >= N) {
        r |= (1 << 16);
    }
    return r;
}

void FastData16Row_Intrin_SSE2(
    const uint8_t* IP,
    const uint8_t* IPprev,
    compv_scalar_t width,
    const compv_scalar_t(&pixels16)[16],
    compv_scalar_t N,
    compv_scalar_t threshold,
	uint8_t* strengths,
    compv_scalar_t* me)
{
    compv_scalar_t i, sum, s;

    int colDarkersFlags, colBrightersFlags; // Flags defining which column has more than N non-zero bits
    bool loadB, loadD;
	__m128i xmm0, xmm1, xmm2, xmm3, xmmThreshold, xmmBrighter, xmmDarker, xmmZeros, xmmFF, xmmDarkersFlags[16], xmmBrightersFlags[16], xmmDataPtr[16], xmmOnes, xmmNMinusOne, xmm254;

	compv_scalar_t fdarkers16[16];
	compv_scalar_t fbrighters16[16];
	__m128i xmmDdarkers16x16[16];
	__m128i xmmDbrighters16x16[16];
	__m128i *xmmStrengths = (__m128i *)strengths;

    _mm_store_si128(&xmmZeros, _mm_setzero_si128());
    _mm_store_si128(&xmmThreshold, _mm_set1_epi8((uint8_t)threshold));
    _mm_store_si128(&xmmFF, _mm_cmpeq_epi8(xmmZeros, xmmZeros)); // 0xFF=255
    _mm_store_si128(&xmmOnes, _mm_load_si128((__m128i*)k1_i8));
    _mm_store_si128(&xmmNMinusOne, _mm_set1_epi8((uint8_t)N - 1));
    _mm_store_si128(&xmm254, _mm_load_si128((__m128i*)k254_u8)); // not(254) = 00000001 -> used to select the lowest bit in each u8

    for (i = 0; i < width; i += 16) {		
		_mm_storeu_si128(xmmStrengths, xmmZeros); // cleanup strengths

		// build brighter and darker
        _mm_store_si128(&xmm0, _mm_loadu_si128((__m128i*)IP)); // load samples
        _mm_store_si128(&xmmBrighter, _mm_adds_epu8(xmm0, xmmThreshold));
        _mm_store_si128(&xmmDarker, _mm_subs_epu8(xmm0, xmmThreshold));

        /* Motion estimation */
        if (IPprev) {
            (*me) = 0;
        }

        /*  Speed-Test-1 */

        // compare I1 and I9 aka 0 and 8
        _mm_store_si128(&xmm0, _mm_loadu_si128((__m128i*)&IP[pixels16[0]]));
        _mm_store_si128(&xmm1, _mm_loadu_si128((__m128i*)&IP[pixels16[8]]));
        _mm_store_si128(&xmmDdarkers16x16[0], _mm_subs_epu8(xmmDarker, xmm0));
        _mm_store_si128(&xmmDdarkers16x16[8], _mm_subs_epu8(xmmDarker, xmm1));
        _mm_store_si128(&xmmDbrighters16x16[0], _mm_subs_epu8(xmm0, xmmBrighter));
        _mm_store_si128(&xmmDbrighters16x16[8], _mm_subs_epu8(xmm1, xmmBrighter));
        _mm_store_si128(&xmmDarkersFlags[0], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[0], xmmZeros), xmmFF));
        _mm_store_si128(&xmmDarkersFlags[8], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[8], xmmZeros), xmmFF));
        _mm_store_si128(&xmmBrightersFlags[0], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[0], xmmZeros), xmmFF));
        _mm_store_si128(&xmmBrightersFlags[8], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[8], xmmZeros), xmmFF));
        _mm_store_si128(&xmm0, _mm_or_si128(xmmDarkersFlags[0], xmmBrightersFlags[0]));
        _mm_store_si128(&xmm1, _mm_or_si128(xmmDarkersFlags[8], xmmBrightersFlags[8]));
        sum = (_mm_movemask_epi8(xmm0) ? 1 : 0) + (_mm_movemask_epi8(xmm1) ? 1 : 0);
        if (!sum) {
            goto next;
        }

        // compare I5 and I13 aka 4 and 12
        _mm_store_si128(&xmm0, _mm_loadu_si128((__m128i*)&IP[pixels16[4]]));
        _mm_store_si128(&xmm1, _mm_loadu_si128((__m128i*)&IP[pixels16[12]]));
        _mm_store_si128(&xmmDdarkers16x16[4], _mm_subs_epu8(xmmDarker, xmm0));
        _mm_store_si128(&xmmDdarkers16x16[12], _mm_subs_epu8(xmmDarker, xmm1));
        _mm_store_si128(&xmmDbrighters16x16[4], _mm_subs_epu8(xmm0, xmmBrighter));
        _mm_store_si128(&xmmDbrighters16x16[12], _mm_subs_epu8(xmm1, xmmBrighter));
        _mm_store_si128(&xmmDarkersFlags[4], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[4], xmmZeros), xmmFF));
        _mm_store_si128(&xmmDarkersFlags[12], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[12], xmmZeros), xmmFF));
        _mm_store_si128(&xmmBrightersFlags[4], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[4], xmmZeros), xmmFF));
        _mm_store_si128(&xmmBrightersFlags[12], _mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[12], xmmZeros), xmmFF));
        _mm_store_si128(&xmm2, _mm_or_si128(xmmDarkersFlags[4], xmmBrightersFlags[4]));
        _mm_store_si128(&xmm3, _mm_or_si128(xmmDarkersFlags[12], xmmBrightersFlags[12]));
        s = (_mm_movemask_epi8(xmm2) ? 1 : 0) + (_mm_movemask_epi8(xmm3) ? 1 : 0);
        if (!s) {
            goto next;
        }
        sum += s;

        /*  Speed-Test-2 */
        if (N == 12 ? sum >= 3 : sum >= 2) { // FIXME
            colDarkersFlags = 0, colBrightersFlags = 0;
            loadB = false, loadD = false;

            // Check whether to load Brighters
            _mm_store_si128(&xmm0, _mm_or_si128(xmmBrightersFlags[0], xmmBrightersFlags[8]));
            _mm_store_si128(&xmm1, _mm_or_si128(xmmBrightersFlags[4], xmmBrightersFlags[12]));
            sum = _mm_movemask_epi8(xmm0) ? 1 : 0;
            sum += _mm_movemask_epi8(xmm1) ? 1 : 0;
            loadB = (sum > 1); // sum cannot be > 2 -> dot not check it against 3 for N = 12

            // Check whether to load Darkers
            _mm_store_si128(&xmm0, _mm_or_si128(xmmDarkersFlags[0], xmmDarkersFlags[8]));
            _mm_store_si128(&xmm1, _mm_or_si128(xmmDarkersFlags[4], xmmDarkersFlags[12]));
            sum = _mm_movemask_epi8(xmm0) ? 1 : 0;
            sum += _mm_movemask_epi8(xmm1) ? 1 : 0;
            loadD = (sum > 1); // sum cannot be > 2 -> dot not check it against 3 for N = 12

            if (!(loadB || loadD)) {
                goto next;
            }

            _mm_store_si128(&xmmDataPtr[1], _mm_loadu_si128((__m128i*)&IP[pixels16[1]]));
            _mm_store_si128(&xmmDataPtr[2], _mm_loadu_si128((__m128i*)&IP[pixels16[2]]));
            _mm_store_si128(&xmmDataPtr[3], _mm_loadu_si128((__m128i*)&IP[pixels16[3]]));
            _mm_store_si128(&xmmDataPtr[5], _mm_loadu_si128((__m128i*)&IP[pixels16[5]]));
            _mm_store_si128(&xmmDataPtr[6], _mm_loadu_si128((__m128i*)&IP[pixels16[6]]));
            _mm_store_si128(&xmmDataPtr[7], _mm_loadu_si128((__m128i*)&IP[pixels16[7]]));
            _mm_store_si128(&xmmDataPtr[9], _mm_loadu_si128((__m128i*)&IP[pixels16[9]]));
            _mm_store_si128(&xmmDataPtr[10], _mm_loadu_si128((__m128i*)&IP[pixels16[10]]));
            _mm_store_si128(&xmmDataPtr[11], _mm_loadu_si128((__m128i*)&IP[pixels16[11]]));
            _mm_store_si128(&xmmDataPtr[13], _mm_loadu_si128((__m128i*)&IP[pixels16[13]]));
            _mm_store_si128(&xmmDataPtr[14], _mm_loadu_si128((__m128i*)&IP[pixels16[14]]));
            _mm_store_si128(&xmmDataPtr[15], _mm_loadu_si128((__m128i*)&IP[pixels16[15]]));

            // We could compute pixels at 1 and 9, check if at least one is darker or brighter than the candidate
            // Then, do the same for 2 and 10 etc etc ... but this is slower than whant we're doing below because
            // _mm_movemask_epi8 is cyclyvore

            if (loadD) {
                // Compute xmmDdarkers
                _mm_store_si128(&xmmDdarkers16x16[1], _mm_subs_epu8(xmmDarker, xmmDataPtr[1]));
                _mm_store_si128(&xmmDdarkers16x16[2], _mm_subs_epu8(xmmDarker, xmmDataPtr[2]));
                _mm_store_si128(&xmmDdarkers16x16[3], _mm_subs_epu8(xmmDarker, xmmDataPtr[3]));
                _mm_store_si128(&xmmDdarkers16x16[5], _mm_subs_epu8(xmmDarker, xmmDataPtr[5]));
                _mm_store_si128(&xmmDdarkers16x16[6], _mm_subs_epu8(xmmDarker, xmmDataPtr[6]));
                _mm_store_si128(&xmmDdarkers16x16[7], _mm_subs_epu8(xmmDarker, xmmDataPtr[7]));
                _mm_store_si128(&xmmDdarkers16x16[9], _mm_subs_epu8(xmmDarker, xmmDataPtr[9]));
                _mm_store_si128(&xmmDdarkers16x16[10], _mm_subs_epu8(xmmDarker, xmmDataPtr[10]));
                _mm_store_si128(&xmmDdarkers16x16[11], _mm_subs_epu8(xmmDarker, xmmDataPtr[11]));
                _mm_store_si128(&xmmDdarkers16x16[13], _mm_subs_epu8(xmmDarker, xmmDataPtr[13]));
                _mm_store_si128(&xmmDdarkers16x16[14], _mm_subs_epu8(xmmDarker, xmmDataPtr[14]));
                _mm_store_si128(&xmmDdarkers16x16[15], _mm_subs_epu8(xmmDarker, xmmDataPtr[15]));
                /* Compute flags (not really, we have the inverse: 0xFF when zero, the not will be applied later) */
                _mm_store_si128(&xmmDarkersFlags[1], _mm_cmpeq_epi8(xmmDdarkers16x16[1], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[2], _mm_cmpeq_epi8(xmmDdarkers16x16[2], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[3], _mm_cmpeq_epi8(xmmDdarkers16x16[3], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[5], _mm_cmpeq_epi8(xmmDdarkers16x16[5], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[6], _mm_cmpeq_epi8(xmmDdarkers16x16[6], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[7], _mm_cmpeq_epi8(xmmDdarkers16x16[7], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[9], _mm_cmpeq_epi8(xmmDdarkers16x16[9], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[10], _mm_cmpeq_epi8(xmmDdarkers16x16[10], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[11], _mm_cmpeq_epi8(xmmDdarkers16x16[11], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[13], _mm_cmpeq_epi8(xmmDdarkers16x16[13], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[14], _mm_cmpeq_epi8(xmmDdarkers16x16[14], xmmZeros));
                _mm_store_si128(&xmmDarkersFlags[15], _mm_cmpeq_epi8(xmmDdarkers16x16[15], xmmZeros));
                // Convert flags from 0xFF to 0x01
                // 0 4 8 12 already computed and contains the right values (not the inverse)
                _mm_store_si128(&xmmDarkersFlags[0], _mm_andnot_si128(xmm254, xmmDarkersFlags[0]));
                _mm_store_si128(&xmmDarkersFlags[4], _mm_andnot_si128(xmm254, xmmDarkersFlags[4]));
                _mm_store_si128(&xmmDarkersFlags[8], _mm_andnot_si128(xmm254, xmmDarkersFlags[8]));
                _mm_store_si128(&xmmDarkersFlags[12], _mm_andnot_si128(xmm254, xmmDarkersFlags[12]));
                // other values
                _mm_store_si128(&xmmDarkersFlags[1], _mm_andnot_si128(xmmDarkersFlags[1], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[2], _mm_andnot_si128(xmmDarkersFlags[2], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[3], _mm_andnot_si128(xmmDarkersFlags[3], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[5], _mm_andnot_si128(xmmDarkersFlags[5], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[6], _mm_andnot_si128(xmmDarkersFlags[6], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[7], _mm_andnot_si128(xmmDarkersFlags[7], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[9], _mm_andnot_si128(xmmDarkersFlags[9], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[10], _mm_andnot_si128(xmmDarkersFlags[10], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[11], _mm_andnot_si128(xmmDarkersFlags[11], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[13], _mm_andnot_si128(xmmDarkersFlags[13], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[14], _mm_andnot_si128(xmmDarkersFlags[14], xmmOnes));
                _mm_store_si128(&xmmDarkersFlags[15], _mm_andnot_si128(xmmDarkersFlags[15], xmmOnes));
                // add all flags
                _mm_store_si128(&xmmDarkersFlags[0], _mm_adds_epu8(xmmDarkersFlags[0], xmmDarkersFlags[1]));
                _mm_store_si128(&xmmDarkersFlags[2], _mm_adds_epu8(xmmDarkersFlags[2], xmmDarkersFlags[3]));
                _mm_store_si128(&xmmDarkersFlags[4], _mm_adds_epu8(xmmDarkersFlags[4], xmmDarkersFlags[5]));
                _mm_store_si128(&xmmDarkersFlags[6], _mm_adds_epu8(xmmDarkersFlags[6], xmmDarkersFlags[7]));
                _mm_store_si128(&xmmDarkersFlags[8], _mm_adds_epu8(xmmDarkersFlags[8], xmmDarkersFlags[9]));
                _mm_store_si128(&xmmDarkersFlags[10], _mm_adds_epu8(xmmDarkersFlags[10], xmmDarkersFlags[11]));
                _mm_store_si128(&xmmDarkersFlags[12], _mm_adds_epu8(xmmDarkersFlags[12], xmmDarkersFlags[13]));
                _mm_store_si128(&xmmDarkersFlags[14], _mm_adds_epu8(xmmDarkersFlags[14], xmmDarkersFlags[15]));
                _mm_store_si128(&xmmDarkersFlags[0], _mm_adds_epu8(xmmDarkersFlags[0], xmmDarkersFlags[2]));
                _mm_store_si128(&xmmDarkersFlags[4], _mm_adds_epu8(xmmDarkersFlags[4], xmmDarkersFlags[6]));
                _mm_store_si128(&xmmDarkersFlags[8], _mm_adds_epu8(xmmDarkersFlags[8], xmmDarkersFlags[10]));
                _mm_store_si128(&xmmDarkersFlags[12], _mm_adds_epu8(xmmDarkersFlags[12], xmmDarkersFlags[14]));
                _mm_store_si128(&xmmDarkersFlags[0], _mm_adds_epu8(xmmDarkersFlags[0], xmmDarkersFlags[4]));
                _mm_store_si128(&xmmDarkersFlags[8], _mm_adds_epu8(xmmDarkersFlags[8], xmmDarkersFlags[12]));
                _mm_store_si128(&xmmDarkersFlags[0], _mm_adds_epu8(xmmDarkersFlags[0], xmmDarkersFlags[8])); // sum is in xmmDarkersFlags[0]
                // Check the columns with at least N non-zero bits
                _mm_store_si128(&xmmDarkersFlags[0], _mm_cmpgt_epi8(xmmDarkersFlags[0], xmmNMinusOne));
                colDarkersFlags = _mm_movemask_epi8(xmmDarkersFlags[0]);
				loadD = (colDarkersFlags != 0);
            }

            if (loadB) {
                /* Compute Dbrighters */
                _mm_store_si128(&xmmDbrighters16x16[1], _mm_subs_epu8(xmmDataPtr[1], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[2], _mm_subs_epu8(xmmDataPtr[2], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[3], _mm_subs_epu8(xmmDataPtr[3], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[5], _mm_subs_epu8(xmmDataPtr[5], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[6], _mm_subs_epu8(xmmDataPtr[6], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[7], _mm_subs_epu8(xmmDataPtr[7], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[9], _mm_subs_epu8(xmmDataPtr[9], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[10], _mm_subs_epu8(xmmDataPtr[10], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[11], _mm_subs_epu8(xmmDataPtr[11], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[13], _mm_subs_epu8(xmmDataPtr[13], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[14], _mm_subs_epu8(xmmDataPtr[14], xmmBrighter));
                _mm_store_si128(&xmmDbrighters16x16[15], _mm_subs_epu8(xmmDataPtr[15], xmmBrighter));
                /* Compute flags (not really, we have the inverse: 0xFF when zero, the not will be applied later) */
                _mm_store_si128(&xmmBrightersFlags[1], _mm_cmpeq_epi8(xmmDbrighters16x16[1], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[2], _mm_cmpeq_epi8(xmmDbrighters16x16[2], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[3], _mm_cmpeq_epi8(xmmDbrighters16x16[3], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[5], _mm_cmpeq_epi8(xmmDbrighters16x16[5], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[6], _mm_cmpeq_epi8(xmmDbrighters16x16[6], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[7], _mm_cmpeq_epi8(xmmDbrighters16x16[7], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[9], _mm_cmpeq_epi8(xmmDbrighters16x16[9], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[10], _mm_cmpeq_epi8(xmmDbrighters16x16[10], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[11], _mm_cmpeq_epi8(xmmDbrighters16x16[11], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[13], _mm_cmpeq_epi8(xmmDbrighters16x16[13], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[14], _mm_cmpeq_epi8(xmmDbrighters16x16[14], xmmZeros));
                _mm_store_si128(&xmmBrightersFlags[15], _mm_cmpeq_epi8(xmmDbrighters16x16[15], xmmZeros));
                // Convert flags from 0xFF to 0x01
                // 0 4 8 12 already computed and contains the right values (not the inverse)
                _mm_store_si128(&xmmBrightersFlags[0], _mm_andnot_si128(xmm254, xmmBrightersFlags[0]));
                _mm_store_si128(&xmmBrightersFlags[4], _mm_andnot_si128(xmm254, xmmBrightersFlags[4]));
                _mm_store_si128(&xmmBrightersFlags[8], _mm_andnot_si128(xmm254, xmmBrightersFlags[8]));
                _mm_store_si128(&xmmBrightersFlags[12], _mm_andnot_si128(xmm254, xmmBrightersFlags[12]));
                // other values
                _mm_store_si128(&xmmBrightersFlags[1], _mm_andnot_si128(xmmBrightersFlags[1], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[2], _mm_andnot_si128(xmmBrightersFlags[2], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[3], _mm_andnot_si128(xmmBrightersFlags[3], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[5], _mm_andnot_si128(xmmBrightersFlags[5], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[6], _mm_andnot_si128(xmmBrightersFlags[6], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[7], _mm_andnot_si128(xmmBrightersFlags[7], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[9], _mm_andnot_si128(xmmBrightersFlags[9], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[10], _mm_andnot_si128(xmmBrightersFlags[10], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[11], _mm_andnot_si128(xmmBrightersFlags[11], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[13], _mm_andnot_si128(xmmBrightersFlags[13], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[14], _mm_andnot_si128(xmmBrightersFlags[14], xmmOnes));
                _mm_store_si128(&xmmBrightersFlags[15], _mm_andnot_si128(xmmBrightersFlags[15], xmmOnes));
                // add all flags
                _mm_store_si128(&xmmBrightersFlags[0], _mm_adds_epu8(xmmBrightersFlags[0], xmmBrightersFlags[1]));
                _mm_store_si128(&xmmBrightersFlags[2], _mm_adds_epu8(xmmBrightersFlags[2], xmmBrightersFlags[3]));
                _mm_store_si128(&xmmBrightersFlags[4], _mm_adds_epu8(xmmBrightersFlags[4], xmmBrightersFlags[5]));
                _mm_store_si128(&xmmBrightersFlags[6], _mm_adds_epu8(xmmBrightersFlags[6], xmmBrightersFlags[7]));
                _mm_store_si128(&xmmBrightersFlags[8], _mm_adds_epu8(xmmBrightersFlags[8], xmmBrightersFlags[9]));
                _mm_store_si128(&xmmBrightersFlags[10], _mm_adds_epu8(xmmBrightersFlags[10], xmmBrightersFlags[11]));
                _mm_store_si128(&xmmBrightersFlags[12], _mm_adds_epu8(xmmBrightersFlags[12], xmmBrightersFlags[13]));
                _mm_store_si128(&xmmBrightersFlags[14], _mm_adds_epu8(xmmBrightersFlags[14], xmmBrightersFlags[15]));
                _mm_store_si128(&xmmBrightersFlags[0], _mm_adds_epu8(xmmBrightersFlags[0], xmmBrightersFlags[2]));
                _mm_store_si128(&xmmBrightersFlags[4], _mm_adds_epu8(xmmBrightersFlags[4], xmmBrightersFlags[6]));
                _mm_store_si128(&xmmBrightersFlags[8], _mm_adds_epu8(xmmBrightersFlags[8], xmmBrightersFlags[10]));
                _mm_store_si128(&xmmBrightersFlags[12], _mm_adds_epu8(xmmBrightersFlags[12], xmmBrightersFlags[14]));
                _mm_store_si128(&xmmBrightersFlags[0], _mm_adds_epu8(xmmBrightersFlags[0], xmmBrightersFlags[4]));
                _mm_store_si128(&xmmBrightersFlags[8], _mm_adds_epu8(xmmBrightersFlags[8], xmmBrightersFlags[12]));
                _mm_store_si128(&xmmBrightersFlags[0], _mm_adds_epu8(xmmBrightersFlags[0], xmmBrightersFlags[8])); // sum is in xmmDarkersFlags[0]
                // Check the columns with at least N non-zero bits
                _mm_store_si128(&xmm0, _mm_cmpgt_epi8(xmmBrightersFlags[0], xmmNMinusOne));
                colBrightersFlags = _mm_movemask_epi8(xmm0);
				loadB = (colBrightersFlags != 0);
            }
			
            if (loadD) {
                // Transpose
               COMPV_TRANSPOSE_I8_16X16_SSE2(
                    xmmDdarkers16x16[0], xmmDdarkers16x16[1], xmmDdarkers16x16[2], xmmDdarkers16x16[3],
                    xmmDdarkers16x16[4], xmmDdarkers16x16[5], xmmDdarkers16x16[6], xmmDdarkers16x16[7],
                    xmmDdarkers16x16[8], xmmDdarkers16x16[9], xmmDdarkers16x16[10], xmmDdarkers16x16[11],
                    xmmDdarkers16x16[12], xmmDdarkers16x16[13], xmmDdarkers16x16[14], xmmDdarkers16x16[15],
                    xmm0);
                // Flags
                fdarkers16[0] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[0], xmmZeros), xmmFF));
                fdarkers16[1] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[1], xmmZeros), xmmFF));
                fdarkers16[2] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[2], xmmZeros), xmmFF));
                fdarkers16[3] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[3], xmmZeros), xmmFF));
                fdarkers16[4] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[4], xmmZeros), xmmFF));
                fdarkers16[5] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[5], xmmZeros), xmmFF));
                fdarkers16[6] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[6], xmmZeros), xmmFF));
                fdarkers16[7] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[7], xmmZeros), xmmFF));
                fdarkers16[8] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[8], xmmZeros), xmmFF));
                fdarkers16[9] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[9], xmmZeros), xmmFF));
                fdarkers16[10] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[10], xmmZeros), xmmFF));
                fdarkers16[11] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[11], xmmZeros), xmmFF));
                fdarkers16[12] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[12], xmmZeros), xmmFF));
                fdarkers16[13] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[13], xmmZeros), xmmFF));
                fdarkers16[14] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[14], xmmZeros), xmmFF));
                fdarkers16[15] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDdarkers16x16[15], xmmZeros), xmmFF));
            }

            if (loadB) {
                // Transpose
                COMPV_TRANSPOSE_I8_16X16_SSE2(
                    xmmDbrighters16x16[0], xmmDbrighters16x16[1], xmmDbrighters16x16[2], xmmDbrighters16x16[3],
                    xmmDbrighters16x16[4], xmmDbrighters16x16[5], xmmDbrighters16x16[6], xmmDbrighters16x16[7],
                    xmmDbrighters16x16[8], xmmDbrighters16x16[9], xmmDbrighters16x16[10], xmmDbrighters16x16[11],
                    xmmDbrighters16x16[12], xmmDbrighters16x16[13], xmmDbrighters16x16[14], xmmDbrighters16x16[15],
                    xmm1);
                // Flags
                fbrighters16[0] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[0], xmmZeros), xmmFF));
                fbrighters16[1] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[1], xmmZeros), xmmFF));
                fbrighters16[2] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[2], xmmZeros), xmmFF));
                fbrighters16[3] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[3], xmmZeros), xmmFF));
                fbrighters16[4] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[4], xmmZeros), xmmFF));
                fbrighters16[5] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[5], xmmZeros), xmmFF));
                fbrighters16[6] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[6], xmmZeros), xmmFF));
                fbrighters16[7] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[7], xmmZeros), xmmFF));
                fbrighters16[8] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[8], xmmZeros), xmmFF));
                fbrighters16[9] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[9], xmmZeros), xmmFF));
                fbrighters16[10] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[10], xmmZeros), xmmFF));
                fbrighters16[11] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[11], xmmZeros), xmmFF));
                fbrighters16[12] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[12], xmmZeros), xmmFF));
                fbrighters16[13] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[13], xmmZeros), xmmFF));
                fbrighters16[14] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[14], xmmZeros), xmmFF));
                fbrighters16[15] = _mm_movemask_epi8(_mm_andnot_si128(_mm_cmpeq_epi8(xmmDbrighters16x16[15], xmmZeros), xmmFF));
				
            }
			if (colBrightersFlags || colDarkersFlags) {
				FastStrengths16(colBrightersFlags, colDarkersFlags, (const uint8_t*)xmmDbrighters16x16, (const uint8_t*)xmmDdarkers16x16, &fbrighters16, &fdarkers16, (uint8_t*)xmmStrengths, N);
			}
        }
next:
        IP += 16;
        if (IPprev) {
            IPprev += 16;
        }
		xmmStrengths += 1;
    } // for i
}

// TODO(dmi): ASM version
// TODO(dmi): add AVX
void FastStrengths16_Intrin_SSE2(compv_scalar_t rbrighters, compv_scalar_t rdarkers, COMPV_ALIGNED(SSE) const uint8_t* dbrighters16x16, COMPV_ALIGNED(SSE) const uint8_t* ddarkers16x16, const compv_scalar_t(*fbrighters16)[16], const compv_scalar_t(*fdarkers16)[16], uint8_t* strengths16, compv_scalar_t N)
{
    COMPV_DEBUG_INFO_CODE_NOT_OPTIMIZED(); // FastStrengths_(ASM/INTRIN)_SSE41 is the best choice

    __m128i xmm0, xmm1, xmmFbrighters, xmmFdarkers, xmmZeros, xmmFastXFlagsLow, xmmFastXFlagsHigh;
    int r0 = 0, r1 = 0;
    unsigned i, j, k;
    int maxnbrighter, maxndarker;
	const uint16_t(&FastXFlags)[16] = N == 9 ? Fast9Flags : Fast12Flags;
	uint16_t rb = (uint16_t)rbrighters, rd = (uint16_t)rdarkers;

    _mm_store_si128(&xmmZeros, _mm_setzero_si128());

	// Set strengths to zero
	_mm_storeu_si128((__m128i*)strengths16, xmmZeros);

    // FAST hard-coded flags
    _mm_store_si128(&xmmFastXFlagsLow, _mm_load_si128((__m128i*)(FastXFlags + 0)));
    _mm_store_si128(&xmmFastXFlagsHigh, _mm_load_si128((__m128i*)(FastXFlags + 8)));

	for (unsigned p = 0; p < 16; ++p) {
		maxnbrighter = 0, maxndarker = 0;
		// Brighters
		if ((rb & (1 << p)) && (*fbrighters16)[p]) {
			// brighters flags
			_mm_store_si128(&xmmFbrighters, _mm_set1_epi16((short)(*fbrighters16)[p]));

			_mm_store_si128(&xmm0, _mm_and_si128(xmmFbrighters, xmmFastXFlagsLow));
			_mm_store_si128(&xmm0, _mm_cmpeq_epi16(xmm0, xmmFastXFlagsLow));
			_mm_store_si128(&xmm1, _mm_and_si128(xmmFbrighters, xmmFastXFlagsHigh));
			_mm_store_si128(&xmm1, _mm_cmpeq_epi16(xmm1, xmmFastXFlagsHigh));
			// xmm0 and xmm1 contain zeros and 0xFF values, if packed and satured we'll end with all-zeros -> perform a ">> 1" which keep sign bit
			_mm_store_si128(&xmm0, _mm_srli_epi16(xmm0, 1));
			_mm_store_si128(&xmm1, _mm_srli_epi16(xmm1, 1));
			r0 = _mm_movemask_epi8(_mm_packus_epi16(xmm0, xmm1)); // r0's popcnt is equal to N as FastXFlags contains values with popcnt==N
			if (r0) {
				uint8_t nbrighter;
				for (i = 0; i < 16; ++i) {
					if (r0 & (1 << i)) {
						// Compute Horizontal minimum (TODO: Find SSE2 method)
						nbrighter = 255;
						k = unsigned(i + N);
						for (j = i; j < k; ++j) {
							if (dbrighters16x16[j & 15] < nbrighter) {
								nbrighter = dbrighters16x16[j & 15];
							}
						}
						maxnbrighter = std::max((int)nbrighter, (int)maxnbrighter);
					}
				}
			}
		}

		// Darkers
		if ((rd & (1 << p)) && (*fdarkers16)[p]) {
			// darkers flags
			_mm_store_si128(&xmmFdarkers, _mm_set1_epi16((short)(*fdarkers16)[p]));

			_mm_store_si128(&xmm0, _mm_and_si128(xmmFdarkers, xmmFastXFlagsLow));
			_mm_store_si128(&xmm0, _mm_cmpeq_epi16(xmm0, xmmFastXFlagsLow));
			_mm_store_si128(&xmm1, _mm_and_si128(xmmFdarkers, xmmFastXFlagsHigh));
			_mm_store_si128(&xmm1, _mm_cmpeq_epi16(xmm1, xmmFastXFlagsHigh));
			// xmm0 and xmm1 contain zeros and 0xFF values, if packed and satured we'll end with all-zeros -> perform a ">> 1" which keep sign bit
			_mm_store_si128(&xmm0, _mm_srli_epi16(xmm0, 1));
			_mm_store_si128(&xmm1, _mm_srli_epi16(xmm1, 1));
			r1 = _mm_movemask_epi8(_mm_packus_epi16(xmm0, xmm1)); // r1's popcnt is equal to N as FastXFlags contains values with popcnt==N
			if (r1) {
				uint8_t ndarker;
				for (i = 0; i < 16; ++i) {
					if (r1 & (1 << i)) {
						// Compute Horizontal minimum
						ndarker = 255;
						k = unsigned(i + N);
						for (j = i; j < k; ++j) {
							if (ddarkers16x16[j & 15] < ndarker) {
								ndarker = ddarkers16x16[j & 15];
							}
						}
						maxndarker = std::max((int)ndarker, (int)maxndarker);
					}
				}
			}
		}

		strengths16[p] = (uint8_t)std::max(maxndarker, maxnbrighter);

		ddarkers16x16 += 16;
		dbrighters16x16 += 16;
	}
}

// TODO(dmi): add AVX
void FastStrengths16_Intrin_SSE41(compv_scalar_t rbrighters, compv_scalar_t rdarkers, COMPV_ALIGNED(SSE) const uint8_t* dbrightersx1616, COMPV_ALIGNED(SSE) const uint8_t* ddarkers16x16, const compv_scalar_t(*fbrighters16)[16], const compv_scalar_t(*fdarkers16)[16], uint8_t* strengths16, compv_scalar_t N)
{
	__m128i xmm0, xmm1, xmmFastXFlagsLow, xmmFastXFlagsHigh, xmmFdarkers, xmmFbrighters;
    __m128i xmmZeros;
    int r0 = 0, r1 = 0;
    int maxnbrighter, maxndarker;
    int lowMin, highMin;
	uint16_t rb = (uint16_t)rbrighters, rd = (uint16_t)rdarkers;
	const uint16_t(&FastXFlags)[16] = N == 9 ? Fast9Flags : Fast12Flags;
    const uint8_t(&kFastArcs)[16][16] = (N == 12 ? kFast12Arcs : kFast9Arcs);

    // FAST hard-coded flags
    _mm_store_si128(&xmmFastXFlagsLow, _mm_load_si128((__m128i*)(FastXFlags + 0)));
    _mm_store_si128(&xmmFastXFlagsHigh, _mm_load_si128((__m128i*)(FastXFlags + 8)));

    _mm_store_si128(&xmmZeros, _mm_setzero_si128());

	// Set strengths to zero
	_mm_storeu_si128((__m128i*)strengths16, xmmZeros);

    // xmm0 contains the u8 values
    // xmm1 is used as temp register and will be trashed
#define COMPV_HORIZ_MIN(r, i, maxn) \
	if (r & (1 << i)) { \
		_mm_store_si128(&xmm1, _mm_shuffle_epi8(xmm0, _mm_load_si128((__m128i*)kFastArcs[i]))); /* eliminate zeros and duplicate first matching non-zero */ \
		lowMin = _mm_cvtsi128_si32(_mm_minpos_epu16(_mm_unpacklo_epi8(xmm1, xmmZeros))); \
		highMin = _mm_cvtsi128_si32(_mm_minpos_epu16(_mm_unpackhi_epi8(xmm1, xmmZeros))); \
		/* _mm_minpos_epu16 must set to zero the remaining bits but this doesn't look to happen or I missed something */ \
		lowMin &= 0xFFFF; \
		highMin &= 0xFFFF; \
		maxn = std::max(std::min(lowMin, highMin), (int)maxn); \
	}

	for (unsigned p = 0; p < 16; ++p) {
		maxnbrighter = 0, maxndarker = 0;
		// Brighters
		if ((rb & (1 << p)) && (*fbrighters16)[p]) {
			// brighters flags
			_mm_store_si128(&xmmFbrighters, _mm_set1_epi16((short)(*fbrighters16)[p]));

			_mm_store_si128(&xmm0, _mm_and_si128(xmmFbrighters, xmmFastXFlagsLow));
			_mm_store_si128(&xmm0, _mm_cmpeq_epi16(xmm0, xmmFastXFlagsLow));
			_mm_store_si128(&xmm1, _mm_and_si128(xmmFbrighters, xmmFastXFlagsHigh));
			_mm_store_si128(&xmm1, _mm_cmpeq_epi16(xmm1, xmmFastXFlagsHigh));
			// clear the high bit in the epi16, otherwise will be considered as the sign bit when saturated to u8
			_mm_store_si128(&xmm0, _mm_srli_epi16(xmm0, 1));
			_mm_store_si128(&xmm1, _mm_srli_epi16(xmm1, 1));
			r0 = _mm_movemask_epi8(_mm_packus_epi16(xmm0, xmm1)); // r0's popcnt is equal to N as FastXFlags contains values with popcnt==N
			if (r0) {
				_mm_store_si128(&xmm0, _mm_load_si128((__m128i*)dbrightersx1616));
				// Compute minimum hz
				COMPV_HORIZ_MIN(r0, 0, maxnbrighter) COMPV_HORIZ_MIN(r0, 1, maxnbrighter) COMPV_HORIZ_MIN(r0, 2, maxnbrighter) COMPV_HORIZ_MIN(r0, 3, maxnbrighter)
					COMPV_HORIZ_MIN(r0, 4, maxnbrighter) COMPV_HORIZ_MIN(r0, 5, maxnbrighter) COMPV_HORIZ_MIN(r0, 6, maxnbrighter) COMPV_HORIZ_MIN(r0, 7, maxnbrighter)
					COMPV_HORIZ_MIN(r0, 8, maxnbrighter) COMPV_HORIZ_MIN(r0, 9, maxnbrighter) COMPV_HORIZ_MIN(r0, 10, maxnbrighter) COMPV_HORIZ_MIN(r0, 11, maxnbrighter)
					COMPV_HORIZ_MIN(r0, 12, maxnbrighter) COMPV_HORIZ_MIN(r0, 13, maxnbrighter) COMPV_HORIZ_MIN(r0, 14, maxnbrighter) COMPV_HORIZ_MIN(r0, 15, maxnbrighter)
			}
		}

		// Darkers
		if ((rd & (1 << p)) && (*fdarkers16)[p]) {
			// darkers flags
			_mm_store_si128(&xmmFdarkers, _mm_set1_epi16((short)(*fdarkers16)[p]));

			_mm_store_si128(&xmm0, _mm_and_si128(xmmFdarkers, xmmFastXFlagsLow));
			_mm_store_si128(&xmm0, _mm_cmpeq_epi16(xmm0, xmmFastXFlagsLow));
			_mm_store_si128(&xmm1, _mm_and_si128(xmmFdarkers, xmmFastXFlagsHigh));
			_mm_store_si128(&xmm1, _mm_cmpeq_epi16(xmm1, xmmFastXFlagsHigh));
			// clear the high bit in the epi16, otherwise will be considered as the sign bit when saturated to u8
			_mm_store_si128(&xmm0, _mm_srli_epi16(xmm0, 1));
			_mm_store_si128(&xmm1, _mm_srli_epi16(xmm1, 1));
			r1 = _mm_movemask_epi8(_mm_packus_epi16(xmm0, xmm1)); // r1's popcnt is equal to N as FastXFlags contains values with popcnt==N
			if (r1) {
				_mm_store_si128(&xmm0, _mm_load_si128((__m128i*)ddarkers16x16));

				// Compute minimum hz
				COMPV_HORIZ_MIN(r1, 0, maxndarker) COMPV_HORIZ_MIN(r1, 1, maxndarker) COMPV_HORIZ_MIN(r1, 2, maxndarker) COMPV_HORIZ_MIN(r1, 3, maxndarker)
					COMPV_HORIZ_MIN(r1, 4, maxndarker) COMPV_HORIZ_MIN(r1, 5, maxndarker) COMPV_HORIZ_MIN(r1, 6, maxndarker) COMPV_HORIZ_MIN(r1, 7, maxndarker)
					COMPV_HORIZ_MIN(r1, 8, maxndarker) COMPV_HORIZ_MIN(r1, 9, maxndarker) COMPV_HORIZ_MIN(r1, 10, maxndarker) COMPV_HORIZ_MIN(r1, 11, maxndarker)
					COMPV_HORIZ_MIN(r1, 12, maxndarker) COMPV_HORIZ_MIN(r1, 13, maxndarker) COMPV_HORIZ_MIN(r1, 14, maxndarker) COMPV_HORIZ_MIN(r1, 15, maxndarker)
			}
		}

		strengths16[p] = (uint8_t)std::max(maxndarker, maxnbrighter);

		dbrightersx1616 += 16;
		ddarkers16x16 += 16;
	}
}

COMPV_NAMESPACE_END()

#endif /* COMPV_ARCH_X86 && COMPV_INTRINSIC */
