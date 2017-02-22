/* Copyright (C) 2016-2017 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#include "compv/core/features/orb/intrin/arm/compv_core_feature_orb_desc_intrin_neon.h"

#if COMPV_ARCH_ARM && COMPV_INTRINSIC
#include "compv/base/intrin/arm/compv_intrin_neon.h"
#include "compv/base/compv_debug.h"

COMPV_NAMESPACE_BEGIN()

void CompVOrbBrief256_31_32f_Intrin_NEON(
	const uint8_t* img_center, compv_uscalar_t img_stride,
	const compv_float32_t* cos1, const compv_float32_t* sin1,
	COMPV_ALIGNED(NEON) const compv_float32_t* kBrief256Pattern31AX, COMPV_ALIGNED(NEON) const compv_float32_t* kBrief256Pattern31AY,
	COMPV_ALIGNED(NEON) const compv_float32_t* kBrief256Pattern31BX, COMPV_ALIGNED(NEON) const compv_float32_t* kBrief256Pattern31BY,
	void* out
)
{
	COMPV_DEBUG_INFO_CHECK_NEON();
	COMPV_ALIGN_NEON() int32_t vecIndex[16];
	COMPV_ALIGN_NEON() uint8_t vecA[16];
	COMPV_ALIGN_NEON() uint8_t vecB[16];
	int32x4_t vecX[4], vecY[4];
	uint8x16_t vecUnpacked;
	uint8x8_t vecPacked;

	uint16_t* outPtr = reinterpret_cast<uint16_t*>(out); // uint32_t for AVX
#if COMPV_ARCH_ARM32
	static const float32x4_t vecHalf = vdupq_n_f32(0.5f);
	static const uint8x8_t vecMask = (uint32x2_t) { 134480385, 2151686160 };
#else
	static const uint8x8_t vecMask = vdup_n_u64(9241421688590303745ULL);
#endif
	const uint32x4_t vecStride = vdupq_n_s32(static_cast<int32_t>(img_stride));
	const float32x4_t vecCosT = vdupq_n_f32(static_cast<float32_t>(*cos1));
	const float32x4_t vecSinT = vdupq_n_f32(static_cast<float32_t>(*sin1));

	for (size_t i = 0; i < 256; i += 16) {
		// xf = (kBrief256Pattern31AX[i] * cosT - kBrief256Pattern31AY[i] * sinT);
		vecX[0] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 0]), vecCosT), vld1q_f32(&kBrief256Pattern31AY[i + 0]), vecSinT);
		vecX[1] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 4]), vecCosT), vld1q_f32(&kBrief256Pattern31AY[i + 4]), vecSinT);
		vecX[2] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 8]), vecCosT), vld1q_f32(&kBrief256Pattern31AY[i + 8]), vecSinT);
		vecX[3] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 12]), vecCosT), vld1q_f32(&kBrief256Pattern31AY[i + 12]), vecSinT);
		// yf = (kBrief256Pattern31AX[i] * sinT + kBrief256Pattern31AY[i] * cosT);
		vecY[0] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 0]), vecSinT), vld1q_f32(&kBrief256Pattern31AY[i + 0]), vecCosT);
		vecY[1] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 4]), vecSinT), vld1q_f32(&kBrief256Pattern31AY[i + 4]), vecCosT);
		vecY[2] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 8]), vecSinT), vld1q_f32(&kBrief256Pattern31AY[i + 8]), vecCosT);
		vecY[3] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31AX[i + 12]), vecSinT), vld1q_f32(&kBrief256Pattern31AY[i + 12]), vecCosT);
		// x = COMPV_MATH_ROUNDF_2_NEAREST_INT(xf, int);
		vecX[0] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[0]);
		vecX[1] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[1]);
		vecX[2] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[2]);
		vecX[3] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[3]);
		// y = COMPV_MATH_ROUNDF_2_NEAREST_INT(yf, int);
		vecY[0] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[0]);
		vecY[1] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[1]);
		vecY[2] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[2]);
		vecY[3] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[3]);
		// a = img_center[(y * img_stride) + x];
		vst1q_s32(&vecIndex[0], vmlaq_s32(vecX[0], vecY[0], vecStride));
		vst1q_s32(&vecIndex[4], vmlaq_s32(vecX[1], vecY[1], vecStride));
		vst1q_s32(&vecIndex[8], vmlaq_s32(vecX[2], vecY[2], vecStride));
		vst1q_s32(&vecIndex[12], vmlaq_s32(vecX[3], vecY[3], vecStride));
		// TODO(dmi): ASM code ->
		// mov r0, img_center[]
		// add r0, r0, img_center[], LSL #8
		// ...
		// mov vecA[i], r0
		vecA[0] = img_center[vecIndex[0]];
		vecA[1] = img_center[vecIndex[1]];
		vecA[2] = img_center[vecIndex[2]];
		vecA[3] = img_center[vecIndex[3]];
		vecA[4] = img_center[vecIndex[4]];
		vecA[5] = img_center[vecIndex[5]];
		vecA[6] = img_center[vecIndex[6]];
		vecA[7] = img_center[vecIndex[7]];
		vecA[8] = img_center[vecIndex[8]];
		vecA[9] = img_center[vecIndex[9]];
		vecA[10] = img_center[vecIndex[10]];
		vecA[11] = img_center[vecIndex[11]];
		vecA[12] = img_center[vecIndex[12]];
		vecA[13] = img_center[vecIndex[13]];
		vecA[14] = img_center[vecIndex[14]];
		vecA[15] = img_center[vecIndex[15]];

		// xf = (kBrief256Pattern31BX[i] * cosT - kBrief256Pattern31BY[i] * sinT);
		vecX[0] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 0]), vecCosT), vld1q_f32(&kBrief256Pattern31BY[i + 0]), vecSinT);
		vecX[1] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 4]), vecCosT), vld1q_f32(&kBrief256Pattern31BY[i + 4]), vecSinT);
		vecX[2] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 8]), vecCosT), vld1q_f32(&kBrief256Pattern31BY[i + 8]), vecSinT);
		vecX[3] = vmlsq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 12]), vecCosT), vld1q_f32(&kBrief256Pattern31BY[i + 12]), vecSinT);
		// yf = (kBrief256Pattern31BX[i] * sinT + kBrief256Pattern31BY[i] * cosT);
		vecY[0] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 0]), vecSinT), vld1q_f32(&kBrief256Pattern31BY[i + 0]), vecCosT);
		vecY[1] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 4]), vecSinT), vld1q_f32(&kBrief256Pattern31BY[i + 4]), vecCosT);
		vecY[2] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 8]), vecSinT), vld1q_f32(&kBrief256Pattern31BY[i + 8]), vecCosT);
		vecY[3] = vmlaq_f32(vmulq_f32(vld1q_f32(&kBrief256Pattern31BX[i + 12]), vecSinT), vld1q_f32(&kBrief256Pattern31BY[i + 12]), vecCosT);
		// x = COMPV_MATH_ROUNDF_2_NEAREST_INT(xf, int);
		vecX[0] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[0]);
		vecX[1] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[1]);
		vecX[2] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[2]);
		vecX[3] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecX[3]);
		// y = COMPV_MATH_ROUNDF_2_NEAREST_INT(yf, int);
		vecY[0] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[0]);
		vecY[1] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[1]);
		vecY[2] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[2]);
		vecY[3] = COMPV_ARM_NEON_MATH_ROUNDF_2_NEAREST_INT(vecY[3]);
		// b = img_center[(y * img_stride) + x];
		vst1q_s32(&vecIndex[0], vmlaq_s32(vecX[0], vecY[0], vecStride));
		vst1q_s32(&vecIndex[4], vmlaq_s32(vecX[1], vecY[1], vecStride));
		vst1q_s32(&vecIndex[8], vmlaq_s32(vecX[2], vecY[2], vecStride));
		vst1q_s32(&vecIndex[12], vmlaq_s32(vecX[3], vecY[3], vecStride));
		vecB[0] = img_center[vecIndex[0]];
		vecB[1] = img_center[vecIndex[1]];
		vecB[2] = img_center[vecIndex[2]];
		vecB[3] = img_center[vecIndex[3]];
		vecB[4] = img_center[vecIndex[4]];
		vecB[5] = img_center[vecIndex[5]];
		vecB[6] = img_center[vecIndex[6]];
		vecB[7] = img_center[vecIndex[7]];
		vecB[8] = img_center[vecIndex[8]];
		vecB[9] = img_center[vecIndex[9]];
		vecB[10] = img_center[vecIndex[10]];
		vecB[11] = img_center[vecIndex[11]];
		vecB[12] = img_center[vecIndex[12]];
		vecB[13] = img_center[vecIndex[13]];
		vecB[14] = img_center[vecIndex[14]];
		vecB[15] = img_center[vecIndex[15]];

		// _out[0] |= (a < b) ? (u64_1 << j) : 0;		
		vecUnpacked = vcltq_u8(vld1q_u8(vecA), vld1q_u8(vecB));
		// _mm_movemask_epi8 doesn't exist on ARM NEON
		vecPacked = vpadd_u8(vand_u8(vecMask, vget_low_u8(vecUnpacked)), vand_u8(vecMask, vget_high_u8(vecUnpacked)));
		vecPacked = vpadd_u8(vecPacked, vecPacked);
		vecPacked = vpadd_u8(vecPacked, vecPacked);
		*outPtr++ = vget_lane_u16(vecPacked, 0);
	}
}

COMPV_NAMESPACE_END()

#endif /* COMPV_ARCH_ARM && COMPV_INTRINSIC */
