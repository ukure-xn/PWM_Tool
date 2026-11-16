#ifndef _IVESDRIVE_H	
#define _IVESDRIVE_H			    
#include "main.h" 
#include "ff.h"
#include "ffconf.h"
#include "diskio.h"
#include "integer.h"  
#include "SPIflash.h"  
#include "string.h" 

#include "swd_host.h"
#include "flash_intf.h"
#include "flash_hal.h"

void Offline_manger(void);

typedef struct {
		uint32_t  init;
		uint32_t  uninit;
		uint32_t  erase_chip;
		uint32_t  erase_sector;
		uint32_t  program_page;
		uint32_t  verify;
		program_syscall_t sys_call_s;
		uint32_t  program_buffer;
		uint32_t  algo_start;
		uint32_t  program_buffer_size;

	   
		uint32_t  SectorSize1;
		uint32_t  SectorAdres1;
		uint32_t  SectorSize2;
		uint32_t  SectorAdres2;
		uint32_t  SectorSize3;
		uint32_t  SectorAdres3;
		uint32_t  SectorSize4;
		uint32_t  SectorAdres4;
		uint32_t  SectorSize5;
		uint32_t  SectorAdres5;
		uint32_t  SectorSize6;
		uint32_t  SectorAdres6;	
	  
		uint32_t  algo_size;
				
		uint32_t  algo_blob[144]; //最大设置512 144*4=576 
} flash_blob_buf;


//有1K的空间来保存这些参数
//暂时只保存这些参数了  
typedef struct {
		uint8_t   mode_page;  //记录用户停留的界面   用于掉电保存 1 主界面  1  恢复写入界面  用于区分是联机模式还是脱机模式
	  uint8_t   decive[33];   //选择的目标类型
		uint8_t   file[33]; //记录上次选择的文件名
  	uint8_t   hexorbin; //记录上次选择的文件类型
	  uint8_t   auto_mode;
	  uint8_t   erase_mode;  //记录上次的烧录擦写模式  0 只擦除需要写入的部分  1 全片擦除  
	  uint8_t   reset_mode;  //写入完成后是否需要复位
		uint32_t  write_adress; //记录设置的扇区起始地址 hex不需要记录 根据文件确认起始地址
		uint32_t  size; //固件大小
		uint8_t   sel_file; //上次选择文件位置
		uint8_t   sel_decive; //选择类型位置
		uint8_t   sel_switch; //上次停留的开关
	  uint32_t  crc_add; //和校验
} offline_save;  //脱机保存的固定参数

typedef struct {
		char   name[33]; //固件名
		uint32_t  length; //固件大小
	  uint8_t   hexorbin; //固件大小 0是hex  1是bin
} file;  //脱机保存的固定参数

void Target_errors_check(uint8_t num);
void offline_data(uint8_t mod);
uint8_t char2hexchar(char *str);
uint8_t hex2bin(char *str); //一次传入一行  累积够4K后 写入
#endif





