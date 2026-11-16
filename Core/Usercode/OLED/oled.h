#ifndef __OLED_H		
#define __OLED_H			  	 
#include "stdlib.h"	    
#include "main.h"

#define     OLED_DC_GPIO_Port   GPIOB
#define     OLED_RST_GPIO_Port   GPIOB
#define     OLED_CS_GPIO_Port   GPIOB



#define     OLED_DC_Pin 	GPIO_PIN_14
#define     OLED_RST_Pin  GPIO_PIN_7
#define     OLED_CS_Pin   GPIO_PIN_6

#define     OLED_DC_Set()       HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_SET);
#define     OLED_DC_Clr()       HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_RESET);

#define     OLED_RST_Set()      HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
#define     OLED_RST_Clr()      HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);

#define     OLED_DISABLE()      HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET);
#define     OLED_ENABLE()       HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_RESET);

#define     KEY_ENT_Pin   GPIO_PIN_1
#define     KEY_ESC_Pin   GPIO_PIN_0

#define     KEY_ENT       HAL_GPIO_ReadPin(GPIOB, KEY_ENT_Pin)
#define     KEY_ESC       HAL_GPIO_ReadPin(GPIOB, KEY_ESC_Pin)
//#define     FLASH_CS_GPIO_Port   GPIOB
//#define     FLASH_CS_Pin  GPIO_PIN_10

//#define     FLASH_DISABLE()      HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
//#define     FLASH_ENABLE()       HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);

//根据数据手册提供对应的宏定义
#define	    BRIGHTNESS	  0x10			//亮度
#define     Max_Column       128				//宽度
#define     Max_Row       32				//长度
#define		  PAGE			    8					//页数
#define			MAX_CHAR_POSX Max_Column-6 //字符宽度需要-6
#define			MAX_CHAR_POSY Max_Row-6 //字符长度需要-6

#define OLED_CMD  0	//写命令
#define OLED_DATA 1	//写数据



		
//OLED初始化
void OLED_Init(void);
//OLED清屏
void OLED_CLS(void);
//OLED刷屏
void OLED_Fill(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2,uint8_t dot);
void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *chr,uint8_t size,uint8_t mode);
void OLED_Refresh_Gram(void);
void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint8_t len,uint8_t size);
void ShowLogo(void) ;
void show_2412Num(uint8_t y,int num);
void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t size,uint8_t mode);
void OLED_BFB(uint32_t a,uint32_t b);
void OLED_CLE(uint8_t num);
//void OLED_Show32Num(u8 x,u8 y,u8 no); //显示32位数字
//void OLED_Power(u8 no); //显示电池电量
////OLED控制用函数
//void OLED_WR_Byte(u8 dat,u8 cmd);	    
//void OLED_Display_On(void);
//void OLED_Display_Off(void);	   							   		    
//void OLED_Init(void);
//void OLED_Clear(void);
//void OLED_DrawPoint(u8 x,u8 y,u8 t);
//void OLED_Fill(u8 x1,u8 y1,u8 x2,u8 y2,u8 dot);
//void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 size);
//void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size);
//void OLED_ShowString(u8 x,u8 y,u8 *chr,u8 size); 
//void OLED_Set_Pos(unsigned char x, unsigned char y);
//void OLED_Show24hz(u8 x,u8 y,u8 no);
//void OLED_DrawBMP(unsigned char x0, unsigned char y0,unsigned char x1, unsigned char y1,unsigned char BMP[]);
//void OLED_LOGO(void) ;
//void OLED_ICO(u8 mod); //
//void OLED_1616hz(u8 x,u8 y,u8 mod); //
//void OLED_Show2412(u8 x,u8 y,u8 no);
//void show_2412Num(u8 y,s16 num);
#endif  
	 



