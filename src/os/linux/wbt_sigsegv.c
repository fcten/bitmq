
#define SIGSEGV_NO_AUTO_INIT
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "wbt_sigsegv.h"

#define NO_CPP_DEMANGLE
#ifndef NO_CPP_DEMANGLE
#include "/usr/include/c++/4.1.1/cxxabi.h"
#include <new> 
char * __cxa_demangle(const char * __mangled_name, char * __output_buffer, size_t * __length, int * __status);
#endif
#if defined(REG_RIP)
#define SIGSEGV_STACK_IA64
#define REGFORMAT "%016lld"
#elif defined(REG_EIP)
#define SIGSEGV_STACK_X86
#define REGFORMAT "%08x"
#else
#define SIGSEGV_STACK_GENERIC
#define REGFORMAT "%x"
#endif

static FILE *sigsegv_log = NULL;
static char sigsegv_log_file[128] = "./webit_sigsegv.log";


static int sigsegv_log_open(const char *filepath)
{
	sigsegv_log = fopen(filepath, "a+");
	if(sigsegv_log!=NULL)
	{
		time_t now;
		time(&now);
		fprintf(sigsegv_log, "%s", ctime(&now));
	}
	return sigsegv_log==NULL?-1:0;
}

static void sigsegv_log_close()
{
	if(sigsegv_log!=NULL)
		fclose(sigsegv_log);
}

void  setSigsegvLogFilepath(const char *filepath)
{
	snprintf(sigsegv_log_file, 128, "%s", filepath);
}

static void signal_segv(int signum, siginfo_t* info, void*ptr) 
{  
	int fg;
	fg = sigsegv_log_open(sigsegv_log_file);
	if(fg==-1)
		return;
	static const char *si_codes[3] = {"", "SEGV_MAPERR", "SEGV_ACCERR"}; 
	size_t i;  
	ucontext_t *ucontext = (ucontext_t*)ptr;
#if defined(SIGSEGV_STACK_X86) || defined(SIGSEGV_STACK_IA64)  
	int f = 0;  
	Dl_info dlinfo;  
	void **bp = 0;  
	void *ip = 0;
#else  
	void *bt[20]; 
	char **strings;  
	size_t sz;
#endif  
	
	fprintf(sigsegv_log, "Segmentation Fault!\n");  
	fprintf(sigsegv_log, "info.si_signo = %d\n", signum);  
	fprintf(sigsegv_log, "info.si_errno = %d\n", info->si_errno);  
	fprintf(sigsegv_log, "info.si_code  = %d (%s)\n", info->si_code, si_codes[info->si_code]); 
	fprintf(sigsegv_log, "info.si_addr  = %p\n", info->si_addr);  
	for(i = 0; i <NGREG; i++)    
		fprintf(sigsegv_log, "reg[%02zd]       = 0x" REGFORMAT "\n", i, ucontext->uc_mcontext.gregs[i]);
#if defined(SIGSEGV_STACK_X86) || defined(SIGSEGV_STACK_IA64)
#if defined(SIGSEGV_STACK_IA64)  
	ip = (void*)ucontext->uc_mcontext.gregs[REG_RIP];  
	bp = (void**)ucontext->uc_mcontext.gregs[REG_RBP];
#elif defined(SIGSEGV_STACK_X86)  
	ip = (void*)ucontext->uc_mcontext.gregs[REG_EIP];  
	bp = (void**)ucontext->uc_mcontext.gregs[REG_EBP];
#endif  
	fprintf(sigsegv_log, "Stack trace:\n");  
	while(bp && ip) 
	{    
		if(!dladdr(ip, &dlinfo)) 
			break;   
		const char *symname = dlinfo.dli_sname;
#ifndef NO_CPP_DEMANGLE    
		int status;    
		char *tmp = __cxa_demangle(symname, NULL, 0, &status);    
		if(status == 0 && tmp)  
			symname = tmp;
#endif    
		fprintf(sigsegv_log, "% 2d: %p <%s+%u> (%s)\n", 
			++f, 
			ip, 
			symname,
			(unsigned)((char*)ip - (char*)dlinfo.dli_saddr),
			dlinfo.dli_fname);
#ifndef NO_CPP_DEMANGLE    
		if(tmp)      
			free(tmp);
#endif    
		if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main")) 
			break;    ip = bp[1];   
		bp = (void**)bp[0];  
	}
#else  
	fprintf(sigsegv_log, "Stack trace (non-dedicated):\n");  
	sz = backtrace(bt, 20); 
	strings = backtrace_symbols(bt, sz);  
	for(i = 0; i < sz; ++i)    
		fprintf(sigsegv_log, "%s\n", strings[i]);
#endif  
	fprintf(sigsegv_log, "End of stack trace\n"); 
	sigsegv_log_close();
	exit (-1);
}
int setup_sigsegv() 
{  
	struct sigaction action; 
	memset(&action, 0, sizeof(action));  
	action.sa_sigaction = signal_segv;  
	action.sa_flags = SA_SIGINFO; 
	if(sigaction(SIGSEGV, &action, NULL) < 0)
	{//segmentation fault
		printf("sigaction failed. errno is %d (%s)\n", errno, strerror(errno));
		return 0;
	}
 	if(sigaction(SIGILL, &action, NULL) < 0)
 	{//ִ���˷Ƿ�ָ��. ͨ������Ϊ��ִ���ļ�������ִ���, ������ͼִ�����ݶ�. ��ջ���ʱҲ�п��ܲ�������ź�
 		printf("sigaction failed. errno is %d (%s)\n", errno, strerror(errno));
 		return 0;
 	}
 
 	if(sigaction(SIGABRT, &action, NULL) < 0)
 	{//abort
 		printf("sigaction failed. errno is %d (%s)\n", errno, strerror(errno));
 		return 0;
 	}
     if(sigaction(SIGFPE, &action, NULL) < 0) 
 	{//algorithm operation error
 		printf("sigaction failed. errno is %d (%s)\n", errno, strerror(errno));
 		return 0;
 	}
	return 1;
}
/*
static void __attribute((constructor)) init(void)
{ 
	setup_sigsegv();
}
*/


