#include "ivsdrive.h" 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdio.h> 


uint8_t read_num=0;// 读取到的字节数（密钥长度）
uint8_t key=0;// 密钥校验通过的字节计数

void delay_us(uint32_t num)
{
  while(num--);
}

#define TIME_OUT_NUM 50
offline_save offline_saves=
{
// .decive=0,
// .file="NUll",
// .erase_mode=0,
// .reset_mode=0,
// .write_adress=0x8000000
};

#define   CAL_READ_ID  0x1ffff7e8

#define RID1  0x3e   //1f<<1
#define RID2  0xff   //
#define RID3  0xfa   //f7+3
#define RID4  0xe0   //e8-8

// DAP 密钥常量
#define key1  0x5a12c568   //e8-8
#define key2  0x78529823   //e8-8
#define key3  0x8056231a   //e8-8

#define key4  0x863597AB  //e8-8
#define key5  0xEAB5632A   //e8-8
#define key6  0x55AA321A   //e8-8



FATFS fs;                      // 文件系统对象
FIL fsrc, fdst;                // 文件对象
FRESULT res;                   // FatFs 函数返回值
FILINFO finfo;
DIR dirs;
file files[20]; // 文件列表，最多 20 个
extern flash_intf_t *intf;
uint8_t READ_BUF[4096];// 读写缓冲区，一次 4096 字节
error_t status;


uint32_t lock_id_1;
uint32_t lock_id_2;
uint32_t lock_id_3;

void nToHexstr(uint32_t n, unsigned char * hexstr, unsigned char strlen);

#define ESC_LONG_MS 800		// ESC 长按判定时间，单位毫秒

// ESC 长按检测：state 0=未按下 1=计时中 2=已触发
// 返回 1 表示本次按下刚达到长按时间，每次按下只返回一次
uint8_t ESC_LongPress(uint8_t *state, uint32_t *t0)
{
	if(KEY_ESC==0)		// ESC 低电平有效
	{
		uint32_t now=HAL_GetTick();
		if(*state==0)
		{
			*state=1;
			*t0=now;
		}else if(*state==1)
		{
			if((now-*t0)>=ESC_LONG_MS)
			{
				*state=2;
				return 1;
			}
		}
	}else
	{
		*state=0;
	}
	return 0;
}

uint8_t show_id(void)
{
	uint8_t show_id[8];
	uint8_t esc_st=0;
	uint32_t esc_t0=0;
  OLED_CLS(); // 清屏
	nToHexstr(lock_id_1,show_id,8);
	OLED_ShowString(0,0,"Key1:",12,0); // 默认显示
	OLED_ShowString(50,0,show_id,12,0); // 默认显示
	nToHexstr(lock_id_2,show_id,8);
	OLED_ShowString(0,1,"Key2:",12,0); // 默认显示
	OLED_ShowString(50,1,show_id,12,0); // 默认显示
	nToHexstr(lock_id_3,show_id,8);
	OLED_ShowString(0,2,"Key3:",12,0); // 默认显示
	OLED_ShowString(50,2,show_id,12,0); // 默认显示

	while(1)
	{
		if(ESC_LongPress(&esc_st,&esc_t0)) // 长按 ESC 放弃本次下载
		{
			while(KEY_ESC==0); // 等待按键释放
			return 1;
		}
		OLED_ShowString(0,3,"Please use the key!",12,1); // 默认显示
	  OLED_Refresh_Gram();
    delay_us(0x2fffff);
		OLED_ShowString(0,3,"Please use the key!",12,0); // 默认显示
	  OLED_Refresh_Gram();
    delay_us(0x2fffff);
	}
	return 0;
	
}

const char key_str[13]=  // 密钥文件路径（定长，中间不能有空格）
	{"0:/KEY/key.k"};

uint32_t read_id_1;
uint32_t read_id_2;
uint32_t read_id_3;
	
uint32_t  key_12[12];
uint8_t  key_read[50]; // 存放 key.k 读到的原始数据
uint8_t  key_cal[50]; // 由芯片 ID 计算出来的字符串
const  uint8_t  key_num[10]={7,3,6,1,5,3,2,1,1,0};
void key_uid(void)
{
	lock_id_1 = (*(__IO uint32_t*)(((RID1>>1)<<24)|(RID2<<16)|((RID3-3)<<8)|(RID4+8)))^key1^key4;    // 芯片唯一 ID（96 位）
	lock_id_2 = (*(__IO uint32_t*)(((RID1>>1)<<24)|(RID2<<16)|((RID3-3)<<8)|(RID4+8)+4))^key2^key5;  
	lock_id_3 = (*(__IO uint32_t*)(((RID1>>1)<<24)|(RID2<<16)|((RID3-3)<<8)|(RID4+8)+8))^key3^key6;  
}

/* 调试开关：1 = 跳过密钥校验；0 = 正常校验 key.k */
#define OFFLINE_SKIP_KEY_CHECK 1

uint8_t read_key(void)
{
#if (OFFLINE_SKIP_KEY_CHECK)
	/* 直接返回成功。read_num 必须等于 key，否则 bin_set_page() 会显示 "Please use the key!" */
	read_num = 0x55;
	key      = 0x55;
	return 1;
#else

	
		read_num=0x55;
	  key=0xaa;

    key_uid(); // 读取芯片 UID
		res = f_open(&fsrc,key_str, FA_READ);						
		if(res == FR_OK)		
		{
			uint32_t ByteOfRead=0; // 本次实际读取的字节数
			res=f_read(&fsrc,READ_BUF,sizeof(READ_BUF),&ByteOfRead); // 读取密钥文件内容
			read_num=ByteOfRead;
			if(read_num<10)read_num=0x55;
			res=f_close(&fsrc);					
      // 逐字节比较计算值与文件内容

      uint32_t  key_12[12];
			
			// 用芯片 ID 的各个字节算出 12 个数字
			key_12[0]=(lock_id_1>>24&0xff)*3;
			key_12[1]=(lock_id_2>>16&0xff)*5;
			key_12[2]=(lock_id_3>>8&0xff)*7;
			key_12[3]=(lock_id_1&0xff)*9;
				
			key_12[4]=(lock_id_1>>24&0xff)*11;
			key_12[5]=(lock_id_2>>16&0xff)*13;
			key_12[6]=(lock_id_3>>8&0xff)*15;
			key_12[7]=(lock_id_2&0xff)*17;

			key_12[8]=(lock_id_1>>24&0xff)*19;
			key_12[9]=(lock_id_2>>16&0xff)*21;
			key_12[10]=(lock_id_3>>8&0xff)*23;
			key_12[11]=(lock_id_3&0xff)*25;		     
			sprintf((char *)key_cal,"%d%d%d%d%d%d%d%d%d%d%d%d",key_12[0],key_12[1],key_12[2],key_12[3],key_12[4],key_12[5],key_12[6],key_12[7],key_12[8],key_12[9],key_12[10],key_12[11]);
			key=0;		
			for(uint8_t i=0;i<read_num;i++)
			{
				   switch(key_cal[i])
					 {
					   case '0':key_cal[i]+=key_num[0];break;
						 case '1':key_cal[i]+=key_num[1];break;
						 case '2':key_cal[i]+=key_num[2];break;
						 case '3':key_cal[i]+=key_num[3];break;
						 case '4':key_cal[i]+=key_num[4];break;
						 case '5':key_cal[i]+=key_num[5];break;
						 case '6':key_cal[i]+=key_num[6];break;
						 case '7':key_cal[i]+=key_num[7];break;
						 case '8':key_cal[i]+=key_num[8];break;	 
						 case '9':key_cal[i]+=key_num[9];break; 					 
					 }		
				   switch(READ_BUF[i])
					 {
					   case '0':READ_BUF[i]+=key_num[0];break;
						 case '1':READ_BUF[i]+=key_num[1]; break;
						 case '2':READ_BUF[i]+=key_num[2]; break;
						 case '3':READ_BUF[i]+=key_num[3]; break;
						 case '4':READ_BUF[i]+=key_num[4]; break;
						 case '5':READ_BUF[i]+=key_num[5];break;
						 case '6':READ_BUF[i]+=key_num[6];break;
						 case '7':READ_BUF[i]+=key_num[7]; break;
						 case '8':READ_BUF[i]+=key_num[8]; break;	 
						 case '9':READ_BUF[i]+=key_num[9]; break; 					 
					 }	
					 
					  if(key_cal[i]==READ_BUF[i])						
						{
							key++;
						}
						else
						{
							key=0;
							return 0; // 只要有一个字节不对就返回 0
						}					 
	
			}
			// 全部比较完成
		}					 
		else
		{
			return 0;
		}		
#endif
}

/*
 说明：read_key() 读取 key.k 并与芯片 UID 算出的值比较，不匹配就显示 "Please use the key!"。

*/

flash_blob_buf flash_blob_buf_t;

#define MAX_FILE_PAGENUM  4  // 每页最多显示的数量
#define MAX_DCEIVE_PAGENUM  50  // 支持的最大芯片数量
uint16_t hex2num=0;
// 定长字符串，中间不能有空格
//{
//	{"erase_sect"},
//	{"erase_full"},
//}; 
const char mask_reset_str[4][9]=  // 定长字符串，中间不能有空格
{
	{"   Reset"},
	{"No_Reset"}
}; 

const char mask_auto_str[4][7]=  // 定长字符串，中间不能有空格
{
	{"Manual"},
	{"  Auto"}
}; 


char adress_str[11]=  // 定长字符串，中间不能有空格
{"0x00000000"};


uint8_t device_name[MAX_DCEIVE_PAGENUM][10];


const char hex_bin_str[2][5]=  // 定长字符串，中间不能有空格
{
	{".BIN"},
	{".HEX"}
}; 

char decive_path[64];
char lfn_buf[33];        // 长文件名缓冲


const char fatpath[3]={"0:"}; 	  						 // 根目录，必须带盘符
const char decivepath[7]={"0:/IVS"}; 	  						 // 芯片固件所在目录
// 注意：目录名必须与实际存在的目录一致


// 取名字长度（最多 32 字符）
static uint8_t name_len(const char *s)
{
	uint8_t n=0;
	while(n<32&&s[n])n++;
	return n;
}

// 定宽窗口显示名字，off 为滚动偏移，窗口外补空格，避免超长名字换行溢出
static void show_name_win(uint8_t x,uint8_t y,const char *name,uint8_t off,uint8_t cols,uint8_t mode)
{
	char buf[22];
	uint8_t i,len;
	if(cols>21)cols=21;
	len=name_len(name);
	if(off>len)off=len;
	for(i=0;i<cols;i++)
	{
		if(off+i<len)buf[i]=name[off+i];
		else buf[i]=' ';
	}
	buf[cols]=0;
	OLED_ShowString(x,y,(uint8_t *)buf,12,mode);
}

// 文件列表的一行：名字定宽显示（仅选中行按 off 滚动），大小统一显示为 k
static void show_file_row(uint8_t y,uint8_t idx,uint8_t sel,uint8_t off)
{
	uint8_t len=name_len(files[idx].name);
	uint32_t kb;
	if(sel==0||len<=14)off=0;      // 未选中或名字放得下就不滚
	else if(off>len-10)off=0;      // 滚到末尾留空后回到开头
	show_name_win(0,y,files[idx].name,off,14,sel);
	kb=(files[idx].length+1023)/1024;
	if(kb==0)kb=1;
	if(kb>99999)kb=99999;
	OLED_ShowNum(88,y,kb,5,12);
	OLED_ShowString(121,y,"k",12,0);
}

// 清掉一整行（末页不足 4 行时用）
static void clear_row(uint8_t y)
{
	show_name_win(0,y,"",0,14,0);
	OLED_ShowString(88,y,"     ",12,0);
	OLED_ShowString(121,y," ",12,0);
}

// 芯片列表的一页：top 为窗口首行，row 为光标行
static void show_decive_page(uint8_t top,uint8_t row,uint8_t um)
{
	uint8_t i,rows=um-top;
	if(rows>MAX_FILE_PAGENUM)rows=MAX_FILE_PAGENUM;
	for(i=0;i<MAX_FILE_PAGENUM;i++)
	{
		if(i<rows)show_name_win(0,i,files[top+i].name,0,21,(i==row));
		else show_name_win(0,i,"",0,21,0);
	}
}

// 固件列表的一页：top 为窗口首行，row 为光标行
static void show_file_page(uint8_t top,uint8_t row,uint8_t um,uint8_t off)
{
	uint8_t i,rows=um-top;
	if(rows>MAX_FILE_PAGENUM)rows=MAX_FILE_PAGENUM;
	for(i=0;i<MAX_FILE_PAGENUM;i++)
	{
		if(i<rows)show_file_row(i,top+i,(i==row),off);
		else clear_row(i);
	}
}

// 右对齐显示：逐字符定位，绕开 OLED_ShowString 在 x>120 处的自动换行
static void show_str_right(uint8_t y,const char *s,uint8_t mode)
{
	uint8_t n=0,x;
	while(s[n]&&n<21)n++;
	x=(uint8_t)(128-n*6);
	while(*s)
	{
		if(*s==' ')OLED_ShowChar(x,y,' ',12,0);   // space only clears, never a solid block
		else OLED_ShowChar(x,y,(uint8_t)*s,12,mode);
		x=(uint8_t)(x+6);
		s++;
	}
}

void decive_list(void) // 选择芯片（固件）文件
{
    uint8_t file_unm=0;
	  uint8_t key_bit=0;
	  uint8_t esc_st=0;
	  uint32_t esc_t0=0;
	  uint8_t list_top=0;   // 列表窗口首行
	  uint8_t list_row=0;   // 光标所在行
		//read_key();
		res=f_opendir(&dirs, decivepath);
		 while (res == FR_OK)  	 // 循环读取目录项
		 {
				finfo.lfname=lfn_buf;
				finfo.lfsize=sizeof(lfn_buf);
				res=f_readdir(&dirs, &finfo) ;					      		   			
			 if (finfo.fattrib & (AM_ARC|AM_DIR)) 	
				{
					if(finfo.fattrib & AM_DIR) 
					{
						if(!finfo.fname[0])break;    
						
					}else
					{
					if(!finfo.fname[0])break;    		     // 目录读完就退出
					char *result;
					result=strstr(finfo.fname,".V");
					if(result)
						{
							if(finfo.lfname&&finfo.lfname[0])
								strcpy(files[file_unm].name,finfo.lfname);
							else
								strcpy(files[file_unm].name,finfo.fname);
							files[file_unm].name[32]=0;
							file_unm++;
						}
					}
				}
			}		
		  if(offline_saves.sel_decive>=file_unm)offline_saves.sel_decive=0;
		  list_row=offline_saves.sel_decive%MAX_FILE_PAGENUM;
		  list_top=offline_saves.sel_decive-list_row;
		  MX_OLED_Init();
		  OLED_CLS(); // 清屏
			if(file_unm==0)
			{
			  OLED_ShowString(0,0,"No Decive...",12,0);
			}else
			{
				// 显示文件列表（光标在窗口内上下移动，末页不足 4 行时清空）
				show_decive_page(list_top,list_row,file_unm);
		 }
		  OLED_Refresh_Gram();
			while(KEY_ENT==0);
		  delay_us(0x2ffff);
			while(1)
			{
				if(ESC_LongPress(&esc_st,&esc_t0)) // 长按 ESC 返回上一级
				{
					while(KEY_ESC==0); // 等待按键释放
					MX_OLED_Init();
					OLED_CLS();
					return;
				}
				if(KEY_ENT==0)
				{
					while(KEY_ENT==0){};	
						strcpy((char *)offline_saves.decive,files[offline_saves.sel_decive].name);
						strcpy(decive_path,decivepath);
						strcat(decive_path,"/");
						strcat(decive_path,(char *)offline_saves.decive);
  					res = f_open(&fsrc,decive_path, FA_READ);						
						if(res == FR_OK)		
						{
								uint32_t ByteOfRead=0; // 本次实际读取的字节数
								res=f_read(&fsrc,READ_BUF,sizeof(READ_BUF),&ByteOfRead); // 读取 flash 算法文件
								memset(&flash_blob_buf_t,0,sizeof(flash_blob_buf));
								memcpy(&flash_blob_buf_t, READ_BUF, ByteOfRead);	
								res=f_close(&fsrc);							
						}
					MX_OLED_Init();
					OLED_CLS(); // 清屏
					return;
				}
				if(key_bit==0)
				{
					if(KEY_ESC==0)
					{
						key_bit=1;
						offline_saves.sel_decive++;
						if(offline_saves.sel_decive>=file_unm)
						{
							offline_saves.sel_decive=0;
							list_top=0;
							list_row=0;
						}
						else if(list_row<MAX_FILE_PAGENUM-1)
						{
							list_row++;
						}
						else
						{
							list_top++;
						}
					}
				}
				if(KEY_ESC)
				{
					key_bit=0;
				}
				// 显示文件列表（光标在窗口内上下移动，末页不足 4 行时清空）
				show_decive_page(list_top,list_row,file_unm);
				OLED_Refresh_Gram();
				delay_us(0x2ffff);
			}
}
void file_list(void) //1hex 0bin
{
    uint8_t file_unm=0;
	  uint8_t key_bit=0;
	  uint8_t esc_st=0;
	  uint32_t esc_t0=0;
	  uint8_t name_off=0;   // 选中行名字滚动偏移
	  uint8_t name_tick=0;  // 滚动计时
	  uint8_t name_sel=0xFF; // 上一帧选中的文件
	  uint8_t list_top=0;   // 列表窗口首行
	  uint8_t list_row=0;   // 光标所在行
		res=f_opendir(&dirs, fatpath);
		 while (res == FR_OK)  	 // 循环读取目录项
		 {
				finfo.lfname=lfn_buf;
				finfo.lfsize=sizeof(lfn_buf);
				res=f_readdir(&dirs, &finfo) ;					      		   			
			 if (finfo.fattrib & (AM_ARC|AM_DIR)) 	
				{
					if(finfo.fattrib & AM_DIR) 
					{
						if(!finfo.fname[0])break;    
					}else
					{
					if(!finfo.fname[0])break;    		     // 目录读完就退出
					char *result;
					result=strstr(finfo.fname,hex_bin_str[0]);
					if(result)
						{
							if(finfo.lfname&&finfo.lfname[0])
								strcpy(files[file_unm].name,finfo.lfname);
							else
								strcpy(files[file_unm].name,finfo.fname);
							files[file_unm].name[32]=0;
						  files[file_unm].length=finfo.fsize;
							files[file_unm].hexorbin=0;
							file_unm++;
						}
						result=strstr(finfo.fname,hex_bin_str[1]);
						if(result)
						{
							if(finfo.lfname&&finfo.lfname[0])
								strcpy(files[file_unm].name,finfo.lfname);
							else
								strcpy(files[file_unm].name,finfo.fname);
							files[file_unm].name[32]=0;
						  files[file_unm].length=finfo.fsize;
							files[file_unm].hexorbin=1;
							file_unm++;
						}
					}
				}
			}		
		  MX_OLED_Init();
		  OLED_CLS(); // 清屏
		  if(offline_saves.sel_file>=file_unm)offline_saves.sel_file=0;
		  list_row=offline_saves.sel_file%MAX_FILE_PAGENUM;
		  list_top=offline_saves.sel_file-list_row;
			if(file_unm==0)
			{
			  OLED_ShowString(0,0,"no file...",12,0);
			}else
			{
				// 显示文件列表（光标在窗口内上下移动，末页不足 4 行时清空）
				show_file_page(list_top,list_row,file_unm,name_off);
		 }
			//OLED_ShowString(0,0,(uint8_t *)files[0].name,12,1);
		  OLED_Refresh_Gram();
			while(KEY_ENT==0);
		  delay_us(0x2ffff);
			while(1)
			{
				if(ESC_LongPress(&esc_st,&esc_t0)) // 长按 ESC 返回上一级
				{
					while(KEY_ESC==0); // 等待按键释放
					MX_OLED_Init();
					OLED_CLS();
					return;
				}
				if(KEY_ENT==0)
				{
					while(KEY_ENT==0){};	
						
					offline_saves.size=files[offline_saves.sel_file].length;
					strcpy((char *)offline_saves.file,files[offline_saves.sel_file].name);
						
					// 判断是 hex 还是 bin 文件
					if(files[offline_saves.sel_file].hexorbin)
					{
						offline_saves.hexorbin=1;
						// 打开文件并读出起始地址
						res = f_open(&fsrc,(char *)offline_saves.file, FA_READ); 
						if(res == FR_OK)		
						{
							  uint32_t adress_offet;
							  uint16_t adress;
							  //offline_saves.write_adress=0x8000000;
								f_gets((char *)READ_BUF,100,&fsrc); // 读取地址行
							  adress_offet=char2hexchar(READ_BUF+9);
							  adress_offet=adress_offet<<8+char2hexchar(READ_BUF+11);
								f_gets((char *)READ_BUF,100,&fsrc); // 读取数据长度行
								adress=char2hexchar(READ_BUF+3);
								adress=(adress<<8)+char2hexchar(READ_BUF+5);
								offline_saves.write_adress=adress+(adress_offet<<16);
						  	nToHexstr(offline_saves.write_adress,(uint8_t*)adress_str,8);
							  f_close(&fsrc); 
						}
						
					}else // bin 文件不需要解析地址
					{
					  offline_saves.hexorbin=0;
						offline_saves.write_adress=0x8000000; // bin 文件默认从 0x08000000 开始
						nToHexstr(offline_saves.write_adress,(uint8_t*)adress_str,8);
					}
					MX_OLED_Init();
					OLED_CLS(); // 清屏
					return;
				}
				if(key_bit==0)
				{
					if(KEY_ESC==0)
					{
						key_bit=1;
						offline_saves.sel_file++;
						if(offline_saves.sel_file>=file_unm)
						{
							offline_saves.sel_file=0;
							list_top=0;
							list_row=0;
						}
						else if(list_row<MAX_FILE_PAGENUM-1)
						{
							list_row++;
						}
						else
						{
							list_top++;
						}
					}
				}
				if(KEY_ESC)
				{
					key_bit=0;
				}
				// 显示文件列表（光标在窗口内上下移动，末页不足 4 行时清空）
				show_file_page(list_top,list_row,file_unm,name_off);
				OLED_Refresh_Gram();
				delay_us(0x2ffff);
				// 选中行文件名滚动：每 20 帧左移一个字符，滚到末尾留空后回到开头
				if(name_sel!=offline_saves.sel_file)
				{
					name_sel=offline_saves.sel_file;
					name_off=0;
					name_tick=0;
				}
				name_tick++;
				if(name_tick>=20)
				{
					uint8_t max_off=name_len(files[name_sel].name);
					if(max_off>14)max_off=max_off-10;
					else max_off=0;
					name_tick=0;
					name_off++;
					if(name_off>max_off)name_off=0;
				}
			}
}

void nToHexstr(uint32_t n, unsigned char * hexstr, unsigned char strlen)
{
	unsigned char hexChar[16] = {'0', '1','2','3', '4','5','6', '7','8','9', 'A','B','C', 'D','E','F'}; 
	unsigned char i;
	unsigned char dis;
	unsigned long nTemp = (long) n;
	for(i=1; i<=strlen; i++)
	{
		dis = nTemp & 0x0f;
		hexstr[strlen-i] = hexChar[dis]; 
		nTemp = nTemp >>4;
	}
		hexstr[strlen]='\0';
}
void adress_set(void)
{
  // 按 0x 格式显示地址
  OLED_CLS(); // 清屏
	// 把地址转换成 16 进制字符串
	//adress_str=
	nToHexstr(offline_saves.write_adress,(uint8_t*)adress_str,8);
	OLED_ShowString(20,0,"0x",16,0); // 默认显示
	OLED_ShowString(36,0,(uint8_t*)adress_str,16,0); // 默认显示
	OLED_ShowString(70,3,"Return",12,0);
	OLED_Refresh_Gram();
	while(KEY_ENT==0);
	while(KEY_ESC==0); // 等待按键，防止连击
	uint8_t sel_num=0;
	uint8_t key_bit_esc=0;
	uint8_t key_bit_ent=0;
	uint8_t esc_st=0;
	uint32_t esc_t0=0;
	uint32_t cr_adress=0;
	int32_t ls_adress=0;
	
	
	//OLED_ShowNum(88,0,files[i].length/1024,5,12);	
	
	while(1)
	{
		if(ESC_LongPress(&esc_st,&esc_t0)) // 长按 ESC 返回上一级
		{
			while(KEY_ESC==0); // 等待按键释放
			OLED_CLS();
			return;
		}
		for(uint8_t i=0;i<9;i++)
		{


				if(i==sel_num)
				{
					if(i<8)
					OLED_ShowChar(36+i*8,0,adress_str[i],16,1);
					OLED_ShowString(70,3,"Return",12,1);
				}
				else
				{
					if(i<8)
					OLED_ShowChar(36+i*8,0,adress_str[i],16,0);
					OLED_ShowString(70,3,"Return",12,0);
				}

		}
		OLED_Refresh_Gram();
		if(key_bit_esc==0)
		{
			if(KEY_ESC==0)
			{
				sel_num++;
				if(sel_num>8)sel_num=0;	
				key_bit_esc=1;
				cr_adress=offline_saves.write_adress;
				ls_adress=0;
			}

		}
		if(KEY_ESC)
		{
		 key_bit_esc=0;
		}
		
		if(key_bit_ent==0)
		{
		if(KEY_ENT==0)
			{
				if(sel_num==8)
				{
					OLED_CLS();
					return;
				}
				key_bit_ent=1;
				ls_adress++;
				if(ls_adress+((cr_adress>>((7-sel_num)<<2))&0xf)>15)ls_adress=0-(int32_t)((cr_adress>>((7-sel_num)<<2))&0xf);
				offline_saves.write_adress=	cr_adress+(ls_adress<<((7-sel_num)<<2));
				nToHexstr(offline_saves.write_adress,(uint8_t*)adress_str,8);
			}
		}
		if(KEY_ENT)
		{
			 key_bit_ent=0;
		}
		
		delay_us(0x2ffff);
	}
}
void bin_set_page(void)
{
	

  static uint8_t ss_bit=0;
	static uint8_t time_oput=0;
	static uint8_t key_bit_ent=0;
	static uint8_t key_bit_esc=0;
	static error_t states;
	uint8_t write_bit;
	MX_OLED_Init(); // 初始化 OLED
//	delay_us(0x2fffff); // 上电后稍等，让 OLED 稳定
//	
	OLED_CLS(); // 清屏
	// 显示状态信息
	nToHexstr(offline_saves.write_adress,(uint8_t*)adress_str,8);
  show_name_win(0,0,(char *)offline_saves.decive,0,12,0); // 默认显示芯片型号
	show_name_win(0,1,(char *)offline_saves.file,0,12,0); // 默认显示
	OLED_ShowString(0,2,"0x",12,0); // 默认显示
	OLED_ShowString(12,2,(uint8_t*)adress_str,12,0); // 默认显示
	
	show_str_right(0,mask_auto_str[offline_saves.auto_mode],0); //erase_full &  erase_sect
	show_str_right(1,mask_reset_str[offline_saves.reset_mode],0);   // reset&run  &   no_reset
	show_str_right(2,"Start",0); //
	OLED_ShowString(0,3,"Not Connected",12,1); // 显示当前连接状态
  OLED_Refresh_Gram();
	


	// 读取芯片对应的 flash 算法文件
	strcpy(decive_path,decivepath);
	strcat(decive_path,"/");
	strcat(decive_path,(char *)offline_saves.decive);
	res = f_open(&fsrc,decive_path, FA_READ);						
	if(res == FR_OK)		
	{
			uint32_t ByteOfRead=0; // 本次实际读取的字节数
			res=f_read(&fsrc,READ_BUF,sizeof(READ_BUF),&ByteOfRead); // 读取 flash 算法文件
			memset(&flash_blob_buf_t,0,sizeof(flash_blob_buf));
			memcpy(&flash_blob_buf_t, READ_BUF, ByteOfRead);	
			res=f_close(&fsrc);							
	}
	
	MX_OLED_Init(); // 初始化 OLED
	
	while(KEY_ENT==0);
	while(KEY_ESC==0); // 等待按键，防止连击
	write_bit=0;
	uint8_t AUTO_BIT=0;
	while(1)
	{
		time_oput++;
		if(time_oput>10)
		{
			time_oput=0;
			
			if(swd_init_debug()) //jtag to swd
			{
				if(write_bit==0)
				{
				ss_bit=0;
					AUTO_BIT=1;
				OLED_ShowString(0,3,"Connected    ",12,ss_bit);	
				}
			}else
			{
				write_bit=0;
				ss_bit=!ss_bit;
				AUTO_BIT=0;
				OLED_ShowString(0,3,"Not Connected",12,ss_bit);
			}			
		}
		
		if(offline_saves.decive[0]==0)
		{
		offline_saves.decive[0]='N';
		offline_saves.decive[1]='o';
		offline_saves.decive[2]=' ';
		offline_saves.decive[3]='D';
		offline_saves.decive[4]='e';
		offline_saves.decive[5]='v';
		offline_saves.decive[6]='i';
		offline_saves.decive[7]='c';
		offline_saves.decive[8]='e';
		}
		if(offline_saves.file[0]==0)
		{
			offline_saves.file[0]='N';
			offline_saves.file[1]='o';
			offline_saves.file[2]=' ';
			offline_saves.file[3]='b';
			offline_saves.file[4]='i';
			offline_saves.file[5]='n';
		}
	
		show_name_win(0,0,(char *)offline_saves.decive,0,12,0); // 默认显示芯片型号
		show_name_win(0,1,(char *)offline_saves.file,0,12,0); // 默认显示
		OLED_ShowString(0,2,"0x",12,0); // 默认显示
		OLED_ShowString(12,2,(uint8_t*)adress_str,12,0); // 默认显示
		
		show_str_right(0,mask_auto_str[offline_saves.auto_mode],0); //erase_full &  erase_sect
		show_str_right(1,mask_reset_str[offline_saves.reset_mode],0);   // reset&run  &   no_reset
		show_str_right(2,"Start",0); //
	
		switch(offline_saves.sel_switch)
		{
			case 0:show_name_win(0,0,(char *)offline_saves.decive,0,12,1);   break;
			case 1:show_name_win(0,1,(char *)offline_saves.file,0,12,1);  break; // 默认显示
			case 2:	OLED_ShowString(0,2,"0x",12,1); 	OLED_ShowString(12,2,(uint8_t*)adress_str,12,1);break; // 默认显示
			case 3:show_str_right(0,mask_auto_str[offline_saves.auto_mode],1);   break;//erase_full &  erase_sect  
			case 4:show_str_right(1,mask_reset_str[offline_saves.reset_mode],1);  break; // reset&run  &   no_reset
			case 5:show_str_right(2,"Start",1);   break;//  		
		}
			
		OLED_Refresh_Gram();
		if(key_bit_esc==0)
		{
			if(KEY_ESC==0)
			{
				offline_saves.sel_switch++;
				if(offline_saves.sel_switch>5)offline_saves.sel_switch=0;	
				key_bit_esc=1;
			}

		}
		if(KEY_ESC)
		{
		 key_bit_esc=0;
		}
		if(key_bit_ent==0)
		{
		if(KEY_ENT==0||(AUTO_BIT&&offline_saves.auto_mode&&( offline_saves.sel_switch==5))) // AUTO_BIT=1 表示可以自动开始
			{
				show_str_right(2,"Start",1);  
				AUTO_BIT=0;
				key_bit_ent=1;
				switch(offline_saves.sel_switch)
				{
					case 0: decive_list(); write_bit=0; break; // 选择芯片
					case 1: file_list(); write_bit=0; break; // 选择文件：hex 或 bin，hex 需要解析地址
					case 2: adress_set(); write_bit=0;  break; // 选择下载地址
					case 3: if(offline_saves.auto_mode==0)offline_saves.auto_mode=1;else offline_saves.auto_mode=0;break;
					case 4: if(offline_saves.reset_mode==0)offline_saves.reset_mode=1;else offline_saves.reset_mode=0;break;
					case 5:    
					{
						uint8_t dl_abort=0;
						offline_data(1);// 保存本次设置
					  // 连接目标芯片，开始下载
						 if(swd_init_debug())
						 {
								res = f_open(&fsrc,(char *)offline_saves.file, FA_READ); // 打开要下载的文件
								if(res == FR_OK)		
								{
									uint32_t addr_num=offline_saves.write_adress;
									uint32_t read_size=0;
									MX_OLED_Init();
									OLED_ShowString(0,3,"Erasing sectors...",12,ss_bit);// 正在擦除扇区
									OLED_Refresh_Gram();
									//
									
									error_t erase_chip_param(void);

									
									states=init_chip_param();//
                  Target_errors_check(states);// 检查错误状态
									
									states=erase_chip_param();// 全片擦除，方式由 auto_mode 决定
                  Target_errors_check(erase_chip_param());// 检查错误状态

//									if(offline_saves.hexorbin) // hex 需要解析地址
//									{
//										
//									}else
//									{
//										//offline_saves.size;
//										//offline_saves.write_adress; 
//										
//										// 根据起始地址算出要擦除的扇区
//										//flash_blob_buf_t.SectorSize1;  // 第一个扇区的地址
//										// 起始地址、文件大小、要擦除的范围
//										
//										// 计算要擦除的扇区范围
//										// 防止越界
//									  // 起始地址与扇区地址比较
//										if((offline_saves.write_adress>=flash_blob_buf_t.SectorAdres1))	
//										{
//										}
//										else if(offline_saves.write_adress<=flash_blob_buf_t.SectorAdres1)	
//										{
//										
//										}
//											erase_adress=0;											
//										uint32_t erase_adress=0;// 要擦除的扇区地址
//										while(1)
//										{
//											 // 判断当前擦除地址是否越界
//											 Target_errors_check(target_erase_sector(erase_adress)); // 擦除指定扇区
//											 
//                       erase_adress+=
//  											 
//										}
//									}
									OLED_ShowString(0,3,"                  ",12,ss_bit);// 清空提示
									OLED_Refresh_Gram();
									DAP_Data.fast_clock=1; // 切换到高速时钟
									hex2num=0;
									while(1)
									{
										if(offline_saves.hexorbin)
										{
											uint8_t hexsta=0;
											char hexstr[50];
											f_gets(hexstr,100,&fsrc); // 读取一行
											read_size+=strlen(hexstr)+1;
											hexsta=hex2bin(hexstr);
											if(hexsta==1)
											{
											  Target_errors_check(16);
											}else if(hexsta==2)
											{
											  break;
											}
											OLED_BFB(read_size,offline_saves.size);
										}else
										{
											uint32_t ByteOfRead=0; //
											res=f_read(&fsrc,(char *)READ_BUF,sizeof(READ_BUF),&ByteOfRead); // 一次读取 4096 字节
											if(ByteOfRead==0)break;	// 读完就退出
											states=param_write(addr_num,READ_BUF,ByteOfRead); // 写入 bin 数据
											Target_errors_check(states);// 检查错误状态
											addr_num+=ByteOfRead;				
											// 更新进度条
											// 每 5K 刷新一次
											OLED_BFB(addr_num-offline_saves.write_adress,offline_saves.size);											
										}
										MX_OLED_Init();
										OLED_Refresh_Gram();
										if(read_num!=key)
										{
											if(show_id()) // 长按 ESC 放弃本次下载
											{
												f_close(&fsrc);
												dl_abort=1;
												break;
											}
										}
									}
								}
								else
								{
									// 显示打开文件失败
								}
								if(dl_abort==0) // 正常下载完成才提示并复位
								{
									MX_OLED_Init();
									OLED_CLE(3);
									OLED_ShowString(0,3,"Done.",12,ss_bit);// 下载完成
									OLED_Refresh_Gram();
									res=f_close(&fsrc); // 关闭文件
									if(offline_saves.reset_mode==0)
									target_reset(); // 复位目标芯片
									write_bit=1;// 标记已下载
								}
						 }
					}
					break;	 // 开始下载
				}	
			}
		}
		if(KEY_ENT)
		{
		 key_bit_ent=0;
		}
	  delay_us(0x2ffff);
	}
}
uint8_t  sel_mod; // 当前模式
void Offline_manger(void)
{
		 if(offline_saves.mode_page==0)return; // 离线模式未开启就直接返回

			swd_init(); // 初始化 SWD
			disk_initialize(0);	// 初始化 flash 存储
			//res=
	    f_mount(0, &fs);	  // 挂载文件系统到 0:
     	read_key();
	    bin_set_page();
}

#define OFFDATA_ADRESS  0x801F800
// 离线参数保存地址

uint16_t FLASH_ReadHalfWord(uint32_t address)
{
return *(__IO uint16_t*)address; 
}

void FLASH_ReadMoreData(uint32_t startAddress,uint16_t *readData,uint16_t countToRead)
{
 uint16_t dataIndex;
 for(dataIndex=0;dataIndex<countToRead;dataIndex++)
 {
	 readData[dataIndex]=FLASH_ReadHalfWord(startAddress+dataIndex*2);
 }
}

void offline_data(uint8_t mod) // mod=0 读取参数，mod=1 写入参数
{
	uint32_t add_num=0;
  if(mod)
	{
		// 所有参数累加，用来判断是否发生变化
		add_num=offline_saves.hexorbin+offline_saves.auto_mode+offline_saves.mode_page+offline_saves.reset_mode+offline_saves.size+offline_saves.write_adress+offline_saves.sel_file+offline_saves.sel_decive+offline_saves.sel_switch;
		if(offline_saves.crc_add==add_num)return;
		flash_erase_sector(OFFDATA_ADRESS);// 先擦除
		offline_saves.crc_add=offline_saves.hexorbin+offline_saves.auto_mode+offline_saves.mode_page+offline_saves.reset_mode+offline_saves.size+offline_saves.write_adress+offline_saves.sel_file+offline_saves.sel_decive+offline_saves.sel_switch;
		flash_program_page(OFFDATA_ADRESS,sizeof(offline_save),(uint8_t *)&offline_saves);
		offline_data(0);
	}else
	{
	   FLASH_ReadMoreData(OFFDATA_ADRESS,(uint16_t *)&offline_saves,sizeof(offline_save)/2);
		 add_num=offline_saves.hexorbin+offline_saves.auto_mode+offline_saves.mode_page+offline_saves.reset_mode+offline_saves.size+offline_saves.write_adress+offline_saves.sel_file+offline_saves.sel_decive+offline_saves.sel_switch;
		 if(offline_saves.crc_add!=add_num)
		 {
			 offline_saves.auto_mode=0;
			 offline_saves.mode_page=0;
			 offline_saves.reset_mode=0;
			 offline_saves.sel_file=0;
			 offline_saves.sel_switch=0;
			 offline_saves.sel_decive=0;
			 for(uint8_t i=0;i<sizeof(offline_saves.file);i++)
			 {
			  offline_saves.file[i]=0;
				offline_saves.decive[i]=0; 
			 }
			 offline_saves.file[0]='N';
			 offline_saves.file[1]='u';
			 offline_saves.file[2]='l';
			 offline_saves.file[3]='l';
			 offline_saves.decive[0]='D';
			 offline_saves.decive[1]='e';
			 offline_saves.decive[2]='v';
			 offline_saves.decive[3]='i';
			 offline_saves.decive[4]='c';
			 offline_saves.decive[5]='e';
			 offline_saves.size=0;
			 offline_saves.write_adress=0x8000000;
			 offline_saves.crc_add=offline_saves.hexorbin+offline_saves.auto_mode+offline_saves.mode_page+offline_saves.reset_mode+offline_saves.size+offline_saves.write_adress+offline_saves.sel_file+offline_saves.sel_decive+offline_saves.sel_switch;
		 }
	}

		
}
void Target_errors_check(uint8_t num)
{
  if(num==0)return;
	uint8_t ss_bit=0;
	uint8_t esc_st=0;
	uint32_t esc_t0=0;
	MX_OLED_Init();
	OLED_CLE(3);
	while(1) // 一直显示错误，直到按键返回
	{
		if(ESC_LongPress(&esc_st,&esc_t0)) // 长按 ESC 返回主界面
		{
			while(KEY_ESC==0); // 等待按键释放
			bin_set_page();
		}
		ss_bit=!ss_bit;
		switch(num)
		{
			case 7:	OLED_ShowString(0,3,"ERROR RESET",12,ss_bit);// 错误码
			case 8: OLED_ShowString(0,3,"ERROR ALGO_DL",12,ss_bit); break;
			case 9:  OLED_ShowString(0,3,"ERROR ALGO DATA SEQ",12,ss_bit);break;
			case 10: OLED_ShowString(0,3,"ERROR_INIT",12,ss_bit); break;
			case 11: OLED_ShowString(0,3,"ERROR_SECURITY_BITS",12,ss_bit); break;
			case 12: OLED_ShowString(0,3,"ERROR_UNLOCK",12,ss_bit); break;
			case 13: OLED_ShowString(0,3,"ERROR_ERASE_SECTOR",12,ss_bit); break;
			case 14: OLED_ShowString(0,3,"ERROR_ERASE_ALL",12,ss_bit); break;
			case 15: OLED_ShowString(0,3,"ERROR_WRITE",12,ss_bit); break;
			case 16: OLED_ShowString(0,3,"ERROR_HEXDECODE",12,ss_bit); break;
			//defaut
		}	
		OLED_Refresh_Gram();		
		delay_us(0xfffff);
    if(KEY_ENT==0)
		{
		  bin_set_page();
		}
	}
}

uint8_t char2hexchar(char *str)
{
	 uint8_t num=0;
	
	 if(str[0]>0x39)
	 {
			num=(str[0]-0x37);
	 }else
	 {
		 num=(str[0]-0x30);
	 }
	 if(str[1]>0x39)
	 {
			 num=num<<4|(str[1]-0x37);
	 }else
	 {
		  num=num<<4|(str[1]-0x30);
	 }
	 return num;
}


uint8_t hex2bin(char *str) // 解析一行 hex 记录，攒够 4K 再写入
{
	static uint8_t  len_num;//=strlen(str);
	
	static uint8_t  RowDataByte ;// 本行数据的字节数
	
	
	static uint32_t  OffsetAddress = 0; // 扩展地址
	static uint16_t  Address = 0; // 当前地址
	static uint8_t str_type=0;  // 当前记录类型
	
	static uint32_t  write_adres = 0; // 写入地址
	if(str[0]==':')
	{
		len_num = char2hexchar(str+1); 
		str_type= char2hexchar(str+7); // 第 7 位是记录类型
		Address=char2hexchar(str+3); 
		switch(str_type)
		{
		  case 0x00: 
			{
				for(uint8_t i=0;i<len_num*2;i+=2)
				{
					if(hex2num==0) // 为 0 时是新数据块，记录起始地址
					{
						Address=char2hexchar(str+3);
						Address=(Address<<8)+char2hexchar(str+5);			
						write_adres=Address|(OffsetAddress<<16);
					}
					READ_BUF[hex2num]=char2hexchar(str+9+i);
					hex2num++;
					if(hex2num==4096) // 攒满 4096 字节
					{
					  Target_errors_check(param_write(write_adres,READ_BUF,hex2num));// 检查错误状态
						hex2num=0; // 清零
					}
				}		
				return 0;
			}break;  // 数据记录
			case 0x01: 
			{
				// 文件结束，写入剩余数据
				Target_errors_check(param_write(write_adres,READ_BUF,hex2num));// 检查错误状态
				hex2num=0; // 清零
				return 2;
			}break;  // 文件结束
			case 0x04: 
			{
				OffsetAddress=char2hexchar(str+9);
				OffsetAddress=(OffsetAddress<<8)+char2hexchar(str+11);
			}
			break; 
		}
	}else
	{
	  return 1;
	}
	return 0;
}