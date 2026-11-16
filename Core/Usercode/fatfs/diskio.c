/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2007        */
/*-----------------------------------------------------------------------*/
/* by grqd_xp                                                            */
/* This is a stub disk I/O module that acts as front end of the existing */
/* disk I/O modules and attach it to FatFs module with common interface. */
/*-----------------------------------------------------------------------*/
#include <string.h>
#include "diskio.h"
#include "SPIflash.h"
//#include "flash.h"	 

/*-----------------------------------------------------------------------*/
/* Correspondence between physical drive number and physical drive.      */
/* Note that Tiny-FatFs supports only single drive and always            */
/* accesses drive number 0.                                              */

#define SECTOR_SIZE 4096	  

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                      
*/
#define FLASH_JB	 0  //SD卡,卷标为0

DSTATUS disk_initialize (
	BYTE drv				/* Physical drive nmuber (0..) */
)
{
			switch(drv)
				{
						case FLASH_JB://SD卡
               return 0;
					//	break;
							default:
				}
	    return 0;
//				if(res)return  STA_NOINIT;
	   // else return 0; //初始化成功
}

/*-----------------------------------------------------------------------*/
/* Return Disk Status                                                    */

DSTATUS disk_status (
	BYTE drv		/* Physical drive nmuber (0..) */
)
{	
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */

DRESULT disk_read (
	BYTE drv,		/* Physical drive nmuber (0..) */
 BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	BYTE count		/* Number of sectors to read (1..255) */
)
{	 
	uint8_t res=0; 
    if (!count)return RES_PARERR;//count不能等于0，否则返回参数错误		 	 
	switch(drv)
	{
		case FLASH_JB://SD卡
			//for(;count>0;count--)
		   {
				SPI_Flash_Read(sector*SECTOR_SIZE,SECTOR_SIZE,(uint8_t *)buff);
				//sector++;
				//buff+=SECTOR_SIZE;
			}
			res=0;
			break;
		default:
			res=1; 
	}
   //处理返回值，将SPI_SD_driver.c的返回值转成ff.c的返回值
    if(res==0x00)return RES_OK;	 
    else return RES_ERROR;	   
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */

#if _READONLY == 0
DRESULT disk_write (
	BYTE drv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	BYTE count			/* Number of sectors to write (1..255) */
)
{
	uint8_t res=0;  
    if (!count)return RES_PARERR;//count不能等于0，否则返回参数错误		 	 
	switch(drv)
	{
		case FLASH_JB://SD卡
			
//				W25QXX_Erase_Sector(sector); //擦除这个扇区
//				W25QXX_Write_NoCheck((uint8_t *)buff,sector*SECTOR_SIZE,SECTOR_SIZE);

//			for(;count>0;count--)
//			{										    
//			//W25QXX_Write((uint8_t *)buff,sector*SECTOR_SIZE,SECTOR_SIZE);
//				sector++;
//				buff+=SECTOR_SIZE;
//			}
			res=0;
			break;
		default:
			res=1; 
	}
    //处理返回值，将SPI_SD_driver.c的返回值转成ff.c的返回值
    if(res == 0x00)return RES_OK;	 
    else return RES_ERROR;		 
}
#endif /* _READONLY */



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */

DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0..) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	 if(drv==FLASH_JB)	//外部FLASH  
	{
	    switch(ctrl)
	    {
		    case CTRL_SYNC:
				res = RES_OK; 
		        break;	 
		    case GET_SECTOR_SIZE:
		        *(WORD*)buff = SECTOR_SIZE;
		        res = RES_OK;
		        break;	 
		    case GET_BLOCK_SIZE:
		        *(WORD*)buff = 8;
		        res = RES_OK;
		        break;	 
		    case GET_SECTOR_COUNT:
		        *(DWORD*)buff = 0;
		        res = RES_OK;
		        break;
		    default:
		        res = RES_PARERR;
		        break;
	    }
	}
	return res;
}

DWORD get_fattime(void){
	return 0;
}























