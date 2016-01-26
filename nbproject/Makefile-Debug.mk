#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux-x86
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/src/common/wbt_config.o \
	${OBJECTDIR}/src/common/wbt_connection.o \
	${OBJECTDIR}/src/common/wbt_event.o \
	${OBJECTDIR}/src/common/wbt_file.o \
	${OBJECTDIR}/src/common/wbt_heap.o \
	${OBJECTDIR}/src/common/wbt_list.o \
	${OBJECTDIR}/src/common/wbt_log.o \
	${OBJECTDIR}/src/common/wbt_memory.o \
	${OBJECTDIR}/src/common/wbt_module.o \
	${OBJECTDIR}/src/common/wbt_rbtree.o \
	${OBJECTDIR}/src/common/wbt_ssl.o \
	${OBJECTDIR}/src/common/wbt_string.o \
	${OBJECTDIR}/src/common/wbt_time.o \
	${OBJECTDIR}/src/http/wbt_http.o \
	${OBJECTDIR}/src/http/wbt_http_consts.o \
	${OBJECTDIR}/src/json/wbt_json.o \
	${OBJECTDIR}/src/module/wbt_module_helloworld.o \
	${OBJECTDIR}/src/mq/wbt_mq.o \
	${OBJECTDIR}/src/mq/wbt_mq_channel.o \
	${OBJECTDIR}/src/mq/wbt_mq_msg.o \
	${OBJECTDIR}/src/mq/wbt_mq_subscriber.o \
	${OBJECTDIR}/src/os/linux/wbt_os_util.o \
	${OBJECTDIR}/src/os/linux/wbt_process.o \
	${OBJECTDIR}/src/os/linux/wbt_setproctitle.o \
	${OBJECTDIR}/src/os/linux/wbt_sigsegv.o \
	${OBJECTDIR}/src/os/linux/wbt_terminal.o \
	${OBJECTDIR}/src/webit.o


# C Compiler Flags
CFLAGS=-pg

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/webit

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/webit: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/webit ${OBJECTFILES} ${LDLIBSOPTIONS} -lssl -lcrypto -pg -ldl

${OBJECTDIR}/src/common/wbt_config.o: src/common/wbt_config.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_config.o src/common/wbt_config.c

${OBJECTDIR}/src/common/wbt_connection.o: src/common/wbt_connection.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_connection.o src/common/wbt_connection.c

${OBJECTDIR}/src/common/wbt_event.o: src/common/wbt_event.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_event.o src/common/wbt_event.c

${OBJECTDIR}/src/common/wbt_file.o: src/common/wbt_file.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_file.o src/common/wbt_file.c

${OBJECTDIR}/src/common/wbt_heap.o: src/common/wbt_heap.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_heap.o src/common/wbt_heap.c

${OBJECTDIR}/src/common/wbt_list.o: src/common/wbt_list.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_list.o src/common/wbt_list.c

${OBJECTDIR}/src/common/wbt_log.o: src/common/wbt_log.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_log.o src/common/wbt_log.c

${OBJECTDIR}/src/common/wbt_memory.o: src/common/wbt_memory.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_memory.o src/common/wbt_memory.c

${OBJECTDIR}/src/common/wbt_module.o: src/common/wbt_module.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_module.o src/common/wbt_module.c

${OBJECTDIR}/src/common/wbt_rbtree.o: src/common/wbt_rbtree.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_rbtree.o src/common/wbt_rbtree.c

${OBJECTDIR}/src/common/wbt_ssl.o: src/common/wbt_ssl.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_ssl.o src/common/wbt_ssl.c

${OBJECTDIR}/src/common/wbt_string.o: src/common/wbt_string.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_string.o src/common/wbt_string.c

${OBJECTDIR}/src/common/wbt_time.o: src/common/wbt_time.c 
	${MKDIR} -p ${OBJECTDIR}/src/common
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/common/wbt_time.o src/common/wbt_time.c

${OBJECTDIR}/src/http/wbt_http.o: src/http/wbt_http.c 
	${MKDIR} -p ${OBJECTDIR}/src/http
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/http/wbt_http.o src/http/wbt_http.c

${OBJECTDIR}/src/http/wbt_http_consts.o: src/http/wbt_http_consts.c 
	${MKDIR} -p ${OBJECTDIR}/src/http
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/http/wbt_http_consts.o src/http/wbt_http_consts.c

${OBJECTDIR}/src/json/wbt_json.o: src/json/wbt_json.c 
	${MKDIR} -p ${OBJECTDIR}/src/json
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/json/wbt_json.o src/json/wbt_json.c

${OBJECTDIR}/src/module/wbt_module_helloworld.o: src/module/wbt_module_helloworld.c 
	${MKDIR} -p ${OBJECTDIR}/src/module
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/module/wbt_module_helloworld.o src/module/wbt_module_helloworld.c

${OBJECTDIR}/src/mq/wbt_mq.o: src/mq/wbt_mq.c 
	${MKDIR} -p ${OBJECTDIR}/src/mq
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/mq/wbt_mq.o src/mq/wbt_mq.c

${OBJECTDIR}/src/mq/wbt_mq_channel.o: src/mq/wbt_mq_channel.c 
	${MKDIR} -p ${OBJECTDIR}/src/mq
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/mq/wbt_mq_channel.o src/mq/wbt_mq_channel.c

${OBJECTDIR}/src/mq/wbt_mq_msg.o: src/mq/wbt_mq_msg.c 
	${MKDIR} -p ${OBJECTDIR}/src/mq
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/mq/wbt_mq_msg.o src/mq/wbt_mq_msg.c

${OBJECTDIR}/src/mq/wbt_mq_subscriber.o: src/mq/wbt_mq_subscriber.c 
	${MKDIR} -p ${OBJECTDIR}/src/mq
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/mq/wbt_mq_subscriber.o src/mq/wbt_mq_subscriber.c

${OBJECTDIR}/src/os/linux/wbt_os_util.o: src/os/linux/wbt_os_util.c 
	${MKDIR} -p ${OBJECTDIR}/src/os/linux
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/os/linux/wbt_os_util.o src/os/linux/wbt_os_util.c

${OBJECTDIR}/src/os/linux/wbt_process.o: src/os/linux/wbt_process.c 
	${MKDIR} -p ${OBJECTDIR}/src/os/linux
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/os/linux/wbt_process.o src/os/linux/wbt_process.c

${OBJECTDIR}/src/os/linux/wbt_setproctitle.o: src/os/linux/wbt_setproctitle.c 
	${MKDIR} -p ${OBJECTDIR}/src/os/linux
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/os/linux/wbt_setproctitle.o src/os/linux/wbt_setproctitle.c

${OBJECTDIR}/src/os/linux/wbt_sigsegv.o: src/os/linux/wbt_sigsegv.c 
	${MKDIR} -p ${OBJECTDIR}/src/os/linux
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/os/linux/wbt_sigsegv.o src/os/linux/wbt_sigsegv.c

${OBJECTDIR}/src/os/linux/wbt_terminal.o: src/os/linux/wbt_terminal.c 
	${MKDIR} -p ${OBJECTDIR}/src/os/linux
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/os/linux/wbt_terminal.o src/os/linux/wbt_terminal.c

${OBJECTDIR}/src/webit.o: src/webit.c 
	${MKDIR} -p ${OBJECTDIR}/src
	${RM} "$@.d"
	$(COMPILE.c) -g -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/src/webit.o src/webit.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/webit

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
