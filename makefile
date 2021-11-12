# Variables
PROJECT  := ts_demuxer

ROOT_DIR := $(shell pwd)
OUT_DIR  := ${ROOT_DIR}/output

HEADERS  := $(wildcard *.h)
SOURCES  := $(wildcard *.c)
OBJECTS  := $(patsubst %.c,${OUT_DIR}/%.o,${SOURCES})
BINARY   := ${OUT_DIR}/${PROJECT}

CPPFLAGS += -Wall -I${ROOT_DIR}
CFLAGS   += -m32
LDFLAGS  += -m32

# Commands
CC    ?= gcc
LD    ?= ld
ECHO  ?= echo
MKDIR ?= mkdir

# Rules
all : ${OUT_DIR} ${BINARY}

${OUT_DIR} :
	@if [ ! -d $@ ]; then ${MKDIR} -p $@; fi

${BINARY} : ${OBJECTS}
	@${ECHO} "LD $(notdir $@)"
	@${CC} ${LDFLAGS} -o $@ $^
#	@${LD} ${LDFLAGS} -o $@ $^

${OUT_DIR}/%.o : ${ROOT_DIR}/%.c
	@${ECHO} "CC $(notdir $^)"
	@${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<
