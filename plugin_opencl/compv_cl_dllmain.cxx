/* Copyright (C) 2016-2018 Doubango Telecom <https://www.doubango.org>
* File author: Mamadou DIOP (Doubango Telecom, France).
* License: GPLv3. For commercial license please contact us.
* Source code: https://github.com/DoubangoTelecom/compv
* WebSite: http://compv.org
*/
#include "compv/cl/compv_cl_config.h"
#include "compv/cl/compv_cl_utils.h"
#include "compv/cl/compv_cl.h"

#include "compv/cl/core/features/fast/compv_cl_feature_fast_dete.h"

#if !COMPV_OS_WINDOWS_CE && COMPV_OS_WINDOWS
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
#endif

COMPV_NAMESPACE_BEGIN()

COMPV_EXTERNC COMPV_CL_API COMPV_ERROR_CODE clInit()
{
	return CompVCL::init();
}

COMPV_EXTERNC COMPV_CL_API COMPV_ERROR_CODE clNewObjCornerDeteFAST(CompVGpuCornerDeteFASTPtrPtr fast)
{
	COMPV_CHECK_EXP_RETURN(!fast, COMPV_ERROR_CODE_E_INVALID_PARAMETER);
	CompVCLCornerDeteFASTPtr fast_;
	COMPV_CHECK_CODE_RETURN(CompVCLCornerDeteFAST::newObj(&fast_), "Failed to create OpenCL implementation for FAST feature detection");

	*fast = *fast_;
	return COMPV_ERROR_CODE_S_OK;
}

COMPV_NAMESPACE_END()
