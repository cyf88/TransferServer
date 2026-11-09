#ifndef _VITECH_INVS
#define _VITECH_INVS

/*
* 视频结构化头文版本 V1.0.7
*/
#ifdef __cplusplus
extern "C"
{
#endif

#define INVS_MIN_NUM	4
#define INVS_MAX_NUM	32

#define INVS_MAX_TARGET_NUM	128
#define INVS_TLVS_MAX_NUM	1000

typedef unsigned char			VITECH_U8;
typedef unsigned short			VITECH_U16;
typedef unsigned int			VITECH_U32;
typedef unsigned long long      VITECH_U64;

typedef signed char 			VITECH_S8;
typedef short					VITECH_S16;
typedef int 					VITECH_S32;
typedef long long               VITECH_S64;

typedef float					VITECH_FLOAT;
typedef signed char 			VITECH_CHAR;
typedef void*					VITECH_HANDLE;
typedef int						VITECH_INT;

enum color
{
	black = 0,
	white,
	gray,
	red,
	blue,
	yellow,
	orange,
	brown,
	green,
	purple,
	indigo,
	pink,
	transparent,
	white_green,
	yellow_green,
	golden	=	51,
	slivery,
	other
};
//规则部分
typedef struct tag_point
{
	VITECH_U16 x;
	VITECH_U16 y;
} INVS_POINT;

typedef struct tag_zone
{
	VITECH_U16 	zone_type;		// 0x01, 原人脸类型 0x03检测区域  0x04 屏蔽区域
	VITECH_U16	det_type;		// 0x01, 人员检测  	    0x02 人脸检测 
	VITECH_U16	reseverd[8];	//保留
	VITECH_U16	points_num;		
	INVS_POINT	points[INVS_MAX_NUM];	//点坐标
} ZONE;

typedef struct tag_rule
{
	VITECH_U16		face_rect_expire_frame;		//人脸框超时帧数
	VITECH_U16		vehicle_rect_expire_frame;	//机动车框超时帧数
	VITECH_U16		people_rect_expire_frame;	//行人超时帧数
	VITECH_U16		min_pupil_dis;				//最小瞳距
	VITECH_U16		zone_num;
	ZONE			rec_zone[INVS_MAX_NUM];
} RULES;

//目标检测部分
typedef struct tag_rect
{
	VITECH_U16		width;			// 宽
	VITECH_U16		height;			// 高
	INVS_POINT		point;
} REC,*P_REC;

typedef struct tag_moving_obj
{
	VITECH_U16		id;				//  id
	VITECH_U16		color;
	VITECH_U8		obj_sort;		//	类型 01 人员 02 人脸 03 机动车 04 非机动车 05 物品 06 场景
	VITECH_U8		resverd[4];		
	REC				rec;
} MOVING_OBJ,*P_MOVING_OBJ;

typedef MOVING_OBJ  	FACE_INFO;			
typedef	P_MOVING_OBJ	P_FACE_INFO;

typedef struct tag_moving_target
{
	VITECH_U16			target_num;
	MOVING_OBJ			finfos[INVS_MAX_TARGET_NUM];

} MTAR,*P_MTAR;

//运动目标检测扩展部分
typedef struct tag_tlvdata
{
	VITECH_U16		type;			//类型下的子类型
	VITECH_U32		length;				//长度>64byte，要求使用申请空间
	char			value[64];			//数据
	char*			huge_value;			//超大数据部分
} TLV,*P_TLV;

typedef struct tag_tlvs_data
{
	VITECH_U16		id;					//识别id
	VITECH_U16		type;				//车辆为0x01,行人为0x11. 固定值。 其它为0
	VITECH_U16		tlv_num;			//tlv_data中要插入的个数。
	TLV				tlv_data[INVS_TLVS_MAX_NUM];

} TLVS,*P_TLVS;

typedef struct 
{
	VITECH_U64 tv_sec;
	VITECH_U64 tv_usec;
}INVS_TIME_VAL;

typedef struct tag_absolute_time
{
	VITECH_U16	enable;		  //enable 为1代表插入，为0 不插入
	VITECH_U16	time_format;  // 0 为 INVS_TIME_VAL, 1为 tim_abs
	union 
	{
		INVS_TIME_VAL 	tv;	
		struct 
		{
			char	year;	//0-127 //2000 开始偏移
			char	month;	// 1-12
			char 	day;	// 1-31
			char	hour;	// 0-23
			char	min;	// 0-59
			char 	second;	// 0 -59
			VITECH_S16	sec_fraction; // 0 -16394
		}	tim_abs;
	};
} INVS_ABS_TIME,*PINVS_ABS_TIME;

typedef struct tag_moving_target_ext
{
	INVS_ABS_TIME	abs_time;				// 未设定无效
	TLVS	jpg_tlv;
	TLVS	cmp_tlv;
} MTAR_EXT,*P_MTAR_EXT;

typedef struct tag_vehicle_property
{
	VITECH_U16		reseverd;				// 保留
	INVS_ABS_TIME	abs_time;				// 未设定无效
	TLVS			vehicle_properies_tlvs;		// 车辆属性信息

} VEHICLE_PROPERTY,*P_VEHICLE_PROPERTY;

typedef struct tag_people_property
{
	VITECH_U16		resevred; 				//保留
	INVS_ABS_TIME	abs_time;				//
	TLVS			people_properties_tlvs;

} PEOPLE_PROPERTY,*P_PEOPLE_PROPERTY;

typedef struct tag_accompanied_property
{

	TLVS		imsi_properies_tlvs;		// imsi融合属性信息
	TLVS		mac_properies_tlvs;			// mac融合属性信息

} ACCOMPANIED_PROPERTY,*P_ACCOMPANIED_PROPERTY;


typedef struct tag_direct_retval
{
	VITECH_U32		length;					// 长度
	char *			buffer;					// 返回的数据
} DIRECT_RETVAL,*P_DIRECT_RETVAL;




#ifdef __cplusplus
}
#endif

#endif
