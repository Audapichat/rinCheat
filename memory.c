/*
 * This File is Part Of : 
 *      ___                       ___           ___           ___           ___           ___                 
 *     /  /\        ___          /__/\         /  /\         /__/\         /  /\         /  /\          ___   
 *    /  /::\      /  /\         \  \:\       /  /:/         \  \:\       /  /:/_       /  /::\        /  /\  
 *   /  /:/\:\    /  /:/          \  \:\     /  /:/           \__\:\     /  /:/ /\     /  /:/\:\      /  /:/  
 *  /  /:/~/:/   /__/::\      _____\__\:\   /  /:/  ___   ___ /  /::\   /  /:/ /:/_   /  /:/~/::\    /  /:/   
 * /__/:/ /:/___ \__\/\:\__  /__/::::::::\ /__/:/  /  /\ /__/\  /:/\:\ /__/:/ /:/ /\ /__/:/ /:/\:\  /  /::\   
 * \  \:\/:::::/    \  \:\/\ \  \:\~~\~~\/ \  \:\ /  /:/ \  \:\/:/__\/ \  \:\/:/ /:/ \  \:\/:/__\/ /__/:/\:\  
 *  \  \::/~~~~      \__\::/  \  \:\  ~~~   \  \:\  /:/   \  \::/       \  \::/ /:/   \  \::/      \__\/  \:\ 
 *   \  \:\          /__/:/    \  \:\        \  \:\/:/     \  \:\        \  \:\/:/     \  \:\           \  \:\
 *    \  \:\         \__\/      \  \:\        \  \::/       \  \:\        \  \::/       \  \:\           \__\/
 *     \__\/                     \__\/         \__\/         \__\/         \__\/         \__\/                
 *
 * Copyright (c) Rinnegatamante <rinnegatamante@gmail.com>
 *
 */
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include "memory.h"
int stack_access_mode = -2;
int ram_mode = 0;
static int results_fd = 0;

// Generic memory scanner (MMC storage)
void scanMemory(uint32_t* cur_matches, void* mem, uint32_t mem_size, uint64_t val, int val_size){
	int results_fd  = sceIoOpen("ux0:/data/rinCheat/rinCheat_temp.bin", SCE_O_WRONLY | SCE_O_CREAT, 0777);
	sceIoLseek(results_fd, 0, SEEK_END);
	int i = 0;
	uint8_t* p = (uint8_t*)mem;
	uint32_t matches = cur_matches[0];
	while (i < mem_size){
		if (memcmp(&p[i], &val, val_size) == 0){
			uint32_t to_write = (uint32_t)&p[i];
			sceIoWrite(results_fd, &to_write, 4);
			matches++;
			i+=val_size;
		}else i++;
	}
	cur_matches[0] = matches;
	sceIoClose(results_fd);
}

// Absolute search on stack (MMC storage)
int scanStack(void* stack_ptr, uint32_t stack_size, uint64_t val, int val_size){
	uint32_t matches = 1;
	scanMemory(&matches, stack_ptr, stack_size, val, val_size);
	return matches-1;
}

// Absolute search on heap (MMC storage)
// FIXME: Random crashes, maybe heap it's not linear mapped?
int scanHeap(uint64_t val, int val_size){
	uint8_t* dummy = (uint8_t*)malloc(1);
	if (dummy != NULL){
		SceUID heap_memblock = sceKernelFindMemBlockByAddr(dummy, 0);
		void* heap_addr;
		if (sceKernelGetMemBlockBase(heap_memblock, &heap_addr) >= 0){
			SceKernelMemBlockInfo heap_info;
			heap_info.size = sizeof(SceKernelMemBlockInfo);
			if (sceKernelGetMemBlockInfoByAddr(heap_addr, &heap_info) >= 0){
				uint32_t matches = 1;
				scanMemory(&matches, heap_info.mappedBase, heap_info.mappedSize, val, val_size);
				free(dummy);
				return matches-1;
			}
		}
		free(dummy);
	}
	return 0;
}

// Relative search (MMC storage)
void scanResults(uint64_t val, int val_size){
	int fd = sceIoOpen("ux0:/data/rinCheat/rinCheat_temp2.bin", SCE_O_WRONLY | SCE_O_CREAT, 0777);
	int results_fd = sceIoOpen("ux0:/data/rinCheat/rinCheat_temp.bin", SCE_O_RDONLY | SCE_O_CREAT, 0777);
	uint32_t cur_val;
	sceIoLseek(results_fd, 0x0, SEEK_SET);
	int i = 0;
	while (sceIoRead(results_fd, &cur_val, 4) > 0 && i < stack_access_mode){
		if (memcmp((uint8_t*)cur_val, &val, val_size) == 0){
			sceIoWrite(fd, &cur_val, 4);
			i++;
		}else stack_access_mode--;
	}
	sceIoClose(fd);
	sceIoClose(results_fd);
	sceIoRemove("ux0:/data/rinCheat/rinCheat_temp.bin");
	sceIoRename("ux0:/data/rinCheat/rinCheat_temp2.bin","ux0:/data/rinCheat/rinCheat_temp.bin");
}

// Inject a value on memory
void injectValue(uint8_t* offset, uint64_t val, int val_size){
	memcpy(offset, &val, val_size);
}

// Inject a stack dump on game stack
void injectStackFile(void* stack_ptr, int stack_size, char* file){
	int fd = sceIoOpen(file, SCE_O_RDONLY, 0777);
	if (fd >= 0){
		sceIoRead(fd, stack_ptr, stack_size);
		sceIoClose(fd);
	}
}

// Inject multiple values on memory (MMC storage)
void injectMemory(uint64_t val, int val_size){
	sceIoLseek(results_fd, 0x0, SEEK_SET);
	uint32_t addr;
	int i = 0;
	while (sceIoRead(results_fd, &addr, 4) > 0 && i++ < stack_access_mode){
		injectValue((uint8_t*)addr,val,val_size);
	}
}

// Save offsets from results on MMC (MMC storage)
void saveOffsets(char* filename){
	sceIoLseek(results_fd, 0x0, SEEK_SET);
	uint32_t addr;
	int fd = sceIoOpen(filename, SCE_O_CREAT|SCE_O_WRONLY|SCE_O_APPEND, 0777);
	int i = 0;
	while (sceIoRead(results_fd, &addr, 4) > 0 && i++ < stack_access_mode){
		char data[16];
		sprintf(data, "0x%lX\n", addr);
		sceIoWrite(fd, data, strlen(data));
	}
	sceIoClose(fd);
}