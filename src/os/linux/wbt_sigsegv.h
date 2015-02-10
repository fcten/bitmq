/*********************************************************************
*  sigsegv.h ---- Define debug interface fo segmentation fault for LINUX application
*  version 1.0 2008.01.09
*  jj.x
**********************************************************************/

#ifndef __sigsegv_h__
#define __sigsegv_h__
//#ifdef __cplusplus
// extern "C" 
// {
//#endif 
int    setup_sigsegv();
void  setSigsegvLogFilepath(const char *filepath);
// #ifdef __cplusplus
// }
// #endif
#endif /* __sigsegv_h__ */

