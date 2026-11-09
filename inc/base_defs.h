
#ifndef BASE_DEFS_H_FOR_STRUCTS
#define BASE_DEFS_H_FOR_STRUCTS
#ifdef __cplusplus
extern "C"
{
#endif
#ifdef linux
#ifndef LINUX
#define LINUX
#endif
#endif

#include "vitech_invs.h"

#if 0

#define TIME_INFO_LEN		19
#define MAX_NUM				18
#else
#define MAX_NUM				INVS_TLVS_MAX_NUM
#endif

#define SVAC_EXT_TYPE_DIVA_INFO					9      	///Diva智能分析结果扩展信息
#define SVAC_EXT_TYPE_IDCARD					129    	///身份证扩展信息  
#define SVAC_EXT_TYPE_OSD						130  	///osd扩展信息
#define SVAC_EXT_TYPE_CAR 						131  	///车辆信息
#define SVAC_EXT_TYPE_MAC   					135  	///MAC扩展信息
#define SVAC_EXT_TYPE_MAC_EX 					194		///MAC扩展信息	，不分组处理类型
#define SVAC_EXT_TYPE_IoT 						201
#define	SVAC_EXT_TYPE_ANALYSIS_EXTENSION		17
#define	SVAC_EXT_TYPE_ANALYSIS_EXTENSION_EX		225		///长度大于64K的智能信息,

#define	SVAC_EXT_TYPE_ANALYSIS_EXTENSION_PEOPLE		(SVAC_EXT_TYPE_ANALYSIS_EXTENSION_EX * 256 + 3)		///长度大于64K的智能信息,
#define	SVAC_EXT_TYPE_ANALYSIS_EXTENSION_VEHICLE	(SVAC_EXT_TYPE_ANALYSIS_EXTENSION_EX * 256 + 4)		///长度大于64K的智能信息,
	/**身份证内容**/
typedef struct 
{ 
	unsigned char id_number[18];   ///<身份证号
	unsigned char res1[1];			///<该保留字节是为IE控件显示用,值为0x00.
	unsigned char name[30];			///<姓名
	unsigned char sex[2];			///<性别
	unsigned char nation[24];		///<民族
	unsigned char bD[16];			///<出生日期
	unsigned char address[70];		///<家庭住址
	unsigned char time[19];			///<插入时间,占用19字节；例如"2014-07-24 15:07:30"
}SVAC_EXT_INFO_ID_CARD;

/**车牌识别相关内容**/
typedef struct 
{	
	char   	channelID;		 //车道号
	char  	plate[11];		 //车牌
	char   	plateColor;	 //车牌颜色		
	char   	plateType;		 //车辆类型
	char  	carColor;		 //车身颜色
	char	alarmType;		 //报警类型
	int     redBeginTime;	 //红灯开始时间，单位秒
	int     redEndTime;	 //红灯结束时间，单位秒
	char  	captureTime[8]	;//闯红灯录像的绝对时间 年-月-日-星期-时-分-秒-毫秒
}SVAC_EXT_INFO_CAR;

typedef struct
{
	unsigned char subtype;		//同一subtype的OSD扩展信息出来后，需要覆盖前一个同subtype的OSD扩展信息
	unsigned char codeType;		//字符编码格式，当前仅 0 在使用，表示使用 UTF-8 编码 
	unsigned char alignType;	//字符对齐格式，0 为左对齐，1 为右对齐 
	unsigned char charSize;		//字符字体大小，用像素表示 
	unsigned char charType;		//字符格式，0 为白底黑边，1 为黑底白边，2 为白色字体，3 为黑色字体 ，4为自适应（根据背景色自动变换字体颜色）
	unsigned char top_high;		//字符位置高8位，按像素表示
	unsigned char top_low;		//字符位置低8位，按像素表示
	unsigned char left_high;	//字符位置高8位，按像素表示 	
	unsigned char left_low;		//字符位置低8位，按像素表示 	
	unsigned char Len;			//字符在 osd_data 中占用的字节数 
	unsigned char res[3];		
	unsigned char OSD_DATA[243];//OSD 字符数据，其长度由 len 确定。支持换行，定义'\n'为换行，'\0'为结束符（同时也受到字符长度的约束）。osd_data 的最长长度为 243 字节。	
}SVAC_EXT_INFO_OSD;


typedef struct
{
	unsigned short Ts_time_year;						//年
	unsigned char  Ts_time_month;						//月
	unsigned char  Ts_time_day; 						//日
	unsigned char  Ts_time_hour;						//时	
	unsigned char  Ts_time_minute;						//分	
	unsigned char  Ts_time_second;						//秒
	unsigned short Ts_time_sec_fractional;        	/*是秒的分数信息，以1/16384秒为单位，取值范围为0～16383之间（包括0和16383）*/
}ABS_TIME;


typedef struct
{
	int type;
	union{
		SVAC_EXT_INFO_ID_CARD IdCard;
		SVAC_EXT_INFO_CAR Car_info;
		SVAC_EXT_INFO_OSD OSD_info;
		VEHICLE_PROPERTY Vechicle_Property;
		PEOPLE_PROPERTY People_Property;
		char Data[64 * 1024];
	}info;
}EXT_INFO;

#ifdef __cplusplus
}
#endif
#endif
