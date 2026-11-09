#ifndef _ZXSVACDECLIB_H
#define _ZXSVACDECLIB_H
#if (defined _WIN32) || (defined _WIN64)
//#include "StdAfx.h"
#endif

#if (defined _WIN32) || (defined _WIN64)

#define ZXSVACDECLIB_API  __declspec(dllexport)

#define CALLMETHOD __stdcall
#define CALLBACK __stdcall
#define CDECCALL __cdecl

#elif defined linux
#include <stdint.h>
#define ZXSVACDECLIB_API extern "C"

#define CALLMETHOD
#define CALLBACK
#define CDECCALL
#define LONG int
#define BOOL int
#define FALSE 0
#define TRUE 1
#define DWORD uint32_t
#endif

#if (defined linux)
typedef void *HANDLE;
#endif

#include "base_defs.h"

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if(p){delete(p);  (p)=NULL;} }
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if(p){delete[] (p);  (p)=NULL;} }
#endif


#define MAX_CUSTOM_EXT_SIZE          262144 //65536 //1024 // 256

#define MAX_CUSTOM_EXT_COUNT             16

#define DATA_LEN				128
#define DATA_S_LEN              64

#define SVAC_PIN_INCORRECT              0xF0000024      /* PIN 不正确 */
#define SVAC_PIN_LOCKED                 0xF0000025      /* PIN 被锁死 */
#define SVAC_UKEY_OPEN_ERR              0xF0000031      /* ukey 打开失败 */
#define SVAC_UKEY_UNKOWN                0xF0000032      /* ukey 未识别 */
#define SVAC_NO_HUKEY                   0xF0000033      /* ukey 未初始化 */
#define SVAC_UKEY_TYPEERR               0xF0000034      /* ukey 厂家错误，pc上没有插入正确厂家的ukey */
#define SVAC_UKEY_SIGN1_ERR             0xF0000035      /* 前端签名信息错误 */
#define SVAC_UKEY_VKEK_ERR              0xF0000036      /* 前端vkek错误 */

#define AUTHORIZEOK 0
#define VIRTUALMACHINE (-5)					//虚拟机
#define AUTHORIZEFAIL (-6)					//授权失败
#define NOLICENCEFILE (-7)					//未找到授权
#define LICENCEFILEERROR (-8)				//授权文件错误
#define LICENCEFILEEXPIRED (-9)				//授权过期
#define PERMISSION_DENIED (-10)				//权限不足
#define SVAC_PLUS_ERROR (-11)				//SVAC_PLUS未启动
#define SVAC_LOADDLL_FAILD (-12)			//SVAC库加载失败
#define VSEC_LOADDLL_FAILD (-13)			//VSEC库加载失败
#define SVAC_UNKNOWN_ERR (-14)				//未知错误
#define SVAC_NO_SUPPORT_FUNCTION (-15)		//不支持此功能
#define LIC_FAIL_TYPE_ERR (-16)				//授权类型错误

typedef struct _EXT_INFO_COMMON
{
	unsigned char  ext_info_custom_num;
	EXT_INFO ext_info[MAX_CUSTOM_EXT_COUNT];
}EXT_INFO_COMMON;

#ifndef DEFINE_SVAC_DEC_STATUS
#define DEFINE_SVAC_DEC_STATUS
typedef enum /*dec_status*/
{
	SVAC_DEC_PIC_PASSWORD_ERROR = -65535,   // 密码错，图像数据密码错
	SVAC_DEC_EXT_PASSWORD_ERROR = -65534,  // 密码错，扩展信息密码错
	SVAC_DEC_PIC_EXT_PASSWORD_ERROR = -65533,  // 密码错，图像数据及扩展信息均密码错
	SVAC_DEC_POC_ERROR = -65532,            // 解码错，帧序号不连续
	SVAC_DEC_EXT_ERROR = -65528,            // 解码错，扩展信息解码出错
	SVAC_DEC_POC_EXT_ERROR = -65524,       // 解码错，帧序号不连续及扩展信息出错
	SVAC_DEC_PIC_ERROR = -65520,             // 解码错，图像数据解码出错
	SVAC_DEC_PIC_EXT_ERROR = -65512,        // 解码错，图像数据解码出错同时扩展信息解码出错
	SVAC_DEC_DATA_LOSS = -65504,            // 解码错，图像数据不完整
	SVAC_DEC_NEED_MORE_BITS = -65472,
	SVAC_DEC_VER_ERROR = -65408,
	SVAC_DEC_SUCCESS = 0,

}SVAC_DEC_STATUS;
#endif


#ifndef DEFINE_SVAC_ROI
#define DEFINE_SVAC_ROI
typedef struct
{
	int		roi_info;		//ROI 区域信息,保留
	int 	top;			//ROI 顶边坐标 [y_min]
	int 	left;			//ROI 左边坐标 [x_min]
	int 	bot;			//ROI 底边坐标 [y_max]
	int 	right;			//ROI 右边坐标 [x_max]
}SVAC_ROI;
#endif

#ifndef DEFINE_SVAC_EXT_RESERVED_DATA
#define DEFINE_SVAC_EXT_RESERVED_DATA
typedef struct
{
	unsigned int	extension_id;		//扩展类型ID
	unsigned int	extension_length;	//在extension_length之后的字节长度 
	unsigned char*	reserved_data;		//该扩展单元的内容，在extension_length之后的所有字节 
}SVAC_EXT_RESERVED_DATA;
#endif


#ifndef DEFINE_SVAC_OSD
#define DEFINE_SVAC_OSD
typedef struct
{
	int  osd_type;  //信息类型，32－时间；33－摄像机名称，34－地点标注
	int  code_type; //编码格式，0－UTF-8编码
	int  align_type; //对齐格式，0－左对齐；1－右对齐
	int  char_size; //字体大小
	int  char_type; //字体格式，0－白底黑边；1－黑底白边；2－白色；3－黑色；4－自动反色
	int  osd_top; //字符上边界在画面中的坐标
	int  osd_left; //字符左边界在画面中的坐标
	int  osd_data_len; //OSD数据字节长度
	unsigned char *osd_data; //OSD数据

}SVAC_OSD;
#endif

#ifndef DEFINE_SVAC_AI_DATA
#define DEFINE_SVAC_AI_DATA
typedef struct
{
	int   analysis_id; // 分析结果的id
	int   description_type; //描述形式
	int   analysis_data_len; //分析结果的字节长度
	unsigned char  *analysis_data; //分析结果数据
}SVAC_AI_DATA;
#endif


#ifndef DEFINE_SVAC_EXT_DATA
#define DEFINE_SVAC_EXT_DATA
typedef struct
{
	int  TimeEnable;			//是否包含时间信息
	int  SurveilEnable;			//是否包含监控信息
	int  Surveil_alert_flag;		//是否包含报警信息
	int  RoiEnable;			//图像是否包含ROI信息

							//时间信息
	int  time_year;			//年
	int  time_month;			//月
	int  time_day;			//日
	int  time_hour;			//时	
	int  time_minute;			//分	
	int  time_second;			//秒
	int  time_fractional;		//毫秒，以1/16384秒为单位
	int  time_ref_date_flag;		//为1表示包括年月日的信息
	int  FrameNum;			//时间对应图像帧的frame_num，I帧为0，其他帧逐帧加1

							//扩展事件信息    
	int  Surveil_event_num[16];	//扩展事件数
	int  Surveil_event_ID[16];	//扩展事件ID

								//报警事件信息    
	int  Surveil_alert_num;			//报警事件数目
	int  Surveil_alert_ID[16];		//报警事件信息		
	int  Surveil_alert_region[16];		//位置编号信息
	int  Surveil_alert_frame_num[16];	//摄像头编号信息

										//ROI信息		
	int  		RoiNum;				//图像包含ROI数目
	int  		RoiSkipMode;			//ROI背景跳过模式
	int  		Roi_SVC_SkipMode;	//ROI_SVC背景跳过模式		
	SVAC_ROI	RoiRect[15];			//基本层ROI矩形信息		
	SVAC_ROI	Roi_SVC_ElRect[15];	//SVC增强层ROI矩形信息，SVC下有效

									//用户自定义扩展单元内容，解码库返回该扩展单元的全部内容
	unsigned int   unit_num; //自定义扩展单元数量
	unsigned int   ExtEnableFlag[8]; //每一个标志位表示自定义扩展信息ID是否存在返回值中。如第7个比特位为0表示extension_id为7的自定义扩展数据内容不存在，值为1则表示存在
	SVAC_EXT_RESERVED_DATA   ext_reserved_data[32];

	int  AIinfoEnable;		//是否包含智能分析信息
	int	OSDEnable;		//是否包含OSD扩展信息
	int  GInfoEnable; 		//是否包含地理信息

							//智能分析信息
	int   analysis_num; //分析结果的数目
	SVAC_AI_DATA analysis_info[64];

	//OSD信息
	int	 osd_num; //OSD信息数目
	SVAC_OSD osd_info[8];

	//地理信息
	int  longitude_type; //0-东经；1-西经
	int  longitude_degree; //经度度数
	int  longitude_fraction; //经度度数的分数部分
	int  latitude_type; //0-北纬；1-南纬
	int  latitude_degree; //纬度度数
	int  latitude_fraction; // 纬度度数的分数部分
	int  gis_height; //高度，以米为单位,
	int  gis_speed; //速度，以米/秒为单位
	int  yaw_degree; //方向角，0－正北；其他－从正北按顺时针递增

					 //保留数据区
	int 			Reserved[128];

}SVAC_EXT_DATA;
#endif


#ifndef DEFINE_CHECK_AUTHENTICATION_RESULT
#define DEFINE_CHECK_AUTHENTICATION_RESULT
typedef struct
{
	int		authentication_result_bl;	//空域SVC基本层验签结果：0-失败，1-成功，2-没有签名数据，3-没有验签结果
	int		authentication_result_el;	//空域SVC增强层验签结果：0-失败，1-成功，2-没有签名数据，3-没有验签结果
	int		FrameNum;					//验签结果对应帧的frame_num，I帧为0，其他帧逐帧加1
	char	camera_id[20];				//视频来源摄像机ID
} CHECK_AUTHENTICATION_RESULT;
#endif


#ifndef DEFINE_DEC_INPUT_PARAM
#define DEFINE_DEC_INPUT_PARAM
typedef struct
{
	void* pBitstream;	//码流首地址
	int	nLen;		//码流长度
	int chroma_format_idc;	//解码后输出的YUV格式。0- 4:0:0  1- 4:2:0  2- 4:2:2
	int  bSvcdec;		//当码流中有增强层数据时，是否解码增强层，0-否，1-是
	int  bExtdecOnly; 	//是否只解码码流中的扩展信息，0-否，1-是
	int  check_authentication_flag;//该帧数据是否需验签，0-不验签，1-只对I帧验签，2-逐帧验签
	int  bTsvcdec;  // 当码流存在时域svc增强层数据时，是否解码增强层，0-否；1-是，
	int  dec_type; 	// 解码类型，1-指定为SVAC1.0解码; 2-指定为SVAC2.0解码
					//0-(暂不支持)解码器内部自适应使用SVAC1.0,或者SVAC2.0解码;
					//注：dec_type可根据SVACdec_PrefetchParam获得的svac_version进行设置。
	int  Reserved[24]; //保留扩展参数区
} DEC_INPUT_PARAM;
#endif

#ifndef DEFINE_DEC_OUTPUT_PARAM
#define DEFINE_DEC_OUTPUT_PARAM
typedef struct
{
	int	nIsEffect;		//bit0（最低位）：本解码输出结构体中图像数据是否有效，0-无效，1-有效；bit1：本解码输出结构体中扩展信息数据是否有效，0-无效，1-有效；bit2：本解码输出结构体中验签数据是否有效，0-无效，1-有效；
						/* 图像相关数据*/
	void* pY;			//解码后y分量输出地址，spatial_svc_flag为1时，则为基本层y分量地址
	void* pU;			//解码后u分量输出地址，spatial_svc_flag为1时，则为基本层u分量地址
	void* pV;			//解码后v分量输出地址，spatial_svc_flag为1时，则为基本层v分量地址
	void* pY_SVC;		// 空域SVC增强层解码后y分量输出地址
	void*pU_SVC;		// 空域SVC增强层解码后u分量输出地址
	void*pV_SVC;		// 空域SVC增强层解码后v分量输出地址
	int  frameType;  	// 0:I帧  1:P帧  2:B帧
	int 	nWidth;		//帧宽
	int 	nHeight;		//帧高
	int 	nWidth_EL;		//空域SVC增强层帧宽
	int 	nHeight_EL;		//空域SVC增强层帧高
	int  chroma_format_idc; //解码输出的YUV格式，0- 4:0:0  1- 4:2:0  2- 4:2:2

	int spatial_svc_flag;	// 码流是否为空域SVC模式，0-否，1-是

	int	SVAC_STATE;			// 最低位(0)代表空域SVC增强层图像是否输出，0-否，1-是
	int	luma_bitdepth;		// 表示亮度的比特深度，以比特为单位
	int	chroma_bitdepth;	// 表示色度的比特深度，以比特为单位
	int  FrameNum;	    //图像数据对应帧的frame_num，I帧为0，其他帧逐帧加1
						/*扩展信息数据*/
	SVAC_EXT_DATA   DecExtData;  // 扩展信息
								 /*验签结果*/
	CHECK_AUTHENTICATION_RESULT  CheckAuthData;//验签结果
	int	 Retains[4];
	unsigned char pic_authen_idc; // 图像数据是否进行认证，0-否，1-是
	unsigned char pic_encryp_idc; // 图像数据是否进行加密，0-否，1-是
	unsigned char ext_authen_idc; // 扩展信息是否进行认证，0-否，1-是
	unsigned char ext_encryp_idc; // 扩展信息是否进行加密，0-否，1-是

	/* 安全参数集信息 */
	int encrptFlag; // 是否开启加密，0-否，1-是
	int authFlag;  // 是否开启认证，0-否，1-是
	unsigned char  certID_data[20]; //19 // 证书ID
	unsigned char  camID_data[20];   // 设备ID

	unsigned int cert_state;  // 证书状态
	int frame_authen_type; // 0-有验签结果的帧；1-无验签结果的帧；2-不参与验签的帧，
	int isDecWrong; 		// 是否解密错;
	int sec_level;        // 1 - 4密级参数；0 无密级参数 
	int Reserved[179];     //解码输出保留数据扩展区
} DEC_OUTPUT_PARAM;
#endif

#ifndef DEFINE_SVAC_PREFETCH_PARAM
#define DEFINE_SVAC_PREFETCH_PARAM
typedef struct
{
	int	width;	//当spatial_svc_flag=0 时,图像宽度；spatial_svc_flag=1时,图像增强层宽度。 
	int	height;	//当spatial_svc_flag=0 时,图像高度；spatial_svc_flag=1时,图像增强层高度。 
	int	roi_flag;	//是否有ROI，0-否，1-是
	int	spatial_svc_flag;	//是否支持空域SVC，0-否，1-是
	int	chroma_format_idc; 	//YUV图像格式
	int	bit_depth_luma;		//Y分量比特精度
	int	bit_depth_chroma;		//UV分量比特精度
	int svac_version;     //码流对应的SVAC标准版本，1:SVAC1.0，2: SVAC2.0
	int temporal_svc_flag;	//是否支持时域SVC，0-否，1-是
	int spatial_svc_ratio; //增强层与基本层的宽、高缩放比例，0: 比例为4:3; 1 :  比例为2:1; 2 :  比例为4:1; 3 :  比例为6:1; 4 :  比例为8:1
	int frame_rate; //帧率：0: 25fps；1: 30fps；2: 50fps；3: 60fps；4: 由VUI 参数决定；5,6 :  保留；7 :  无效帧率。对于SVAC1.0：固定取值为7。
	int reserved[5];

} SVAC_PREFETCH_PARAM;
#endif

typedef enum
{
	SVAC_ENCRPT_SM1 = 0,
	SVAC_ENCRPT_AES128 = 2,
	SVAC_ENCRPT_SM4 = 3
} SVAC_ENCRYPT_TYPE;

typedef enum
{
	SVAC_ENCRPT_ECB = 0,
	SVAC_ENCRPT_CBC = 1,
    SVAC_ENCRPT_CFB = 2,
	SVAC_ENCRPT_OFB = 3
} SVAC_ENCRYPT_MODE;

typedef enum
{
    SVAC_ASYMALG_SM2 = 0,
    SVAC_ASYMALG_SM9 = 1,
}SVAC_ASYM_ALG;

typedef enum
{
	SVAC_HASH_MD5 = 0,
	SVAC_HASH_SM3 = 7
} SVAC_HASH_TYPE;

typedef enum
{
	SVAC_SIGN_RSA = 1,
	SVAC_SIGN_SM2 = 3,
	SVAC_SIGN_SM2_BASE64 = 4
} SVAC_SIGN_TYPE;

typedef enum
{
	SVAC_AUTH_Result_NODATA = 0, // 没有验签数据。本帧应该要进行验签操作,但是还没有收到签名数据 
	SVAC_AUTH_Result_FAIL = 1,  // 验签失败 1
	SVAC_AUTH_Result_OK = 2,  // 验签成功 2
	SVAC_AUTH_Result_WAIT = 3, // 有 HASH 值,但是没有收到最后一帧 或者 没有收到下一个 I 帧 , 还在收集数据的过程中
	SVAC_AUTH_Result_LOST = 4, //
} SVAC_AUTH_RESULT;

typedef enum
{
	SVAC_STRM_NO_ENC = 0,   // 码流没有加密
	SVAC_STRM_USER_ENC = 1, // 码流使用用户配置的密码加密
	SVAC_STRM_UKEY_ENC = 2,  // 码流使用vkek和vek加密
}SVAC_STRM_ENC_FLAG;// 码流的加密状态

typedef enum
{
	SVAC_DEVCERT_FROM_IE = 0,
	SVAC_DEVCERT_FROM_VISS = 1,
}SVAC_DEVCERT_PUBLISHTYPE;

typedef struct
{
	unsigned int encrptFlag;		//码流是否被加密		取值参考: SVAC_STRM_ENC_FLAG
	unsigned int authFlag;			//码流是否有签名数据	0:未签名码流           1:签名码流
	unsigned int strmEncrptType;	// 码流加密算法			取值参考: SVAC_ENCRYPT_TYPE
	unsigned int strmEncrptMode;	// 码流加密模式			取值参考: SVAC_ENCRYPT_MODE
	unsigned int vekEncrptType;		// vek加密算法			取值参考: SVAC_ENCRYPT_TYPE
	unsigned int vekEncrptMode;		// vek加密模式			取值参考: SVAC_ENCRYPT_MODE
	unsigned int hashType;			// 摘要算法				取值参考: SVAC_HASH_TYPE
	unsigned int signType;			// 签名算法				取值参考: SVAC_SIGN_TYPE
	unsigned int isDecWrong;		// 是否密码错			0:解密正确；1:解密错误
	unsigned int authResult;		// 认证结果				取值参考: SVAC_AUTH_RESULT
} SVAC_SEC_INFO;



/** X509证书信息 */
typedef struct {
	int CertificateType;			///< 证书格式，备用，目前固定为0
	char* CertificateData;			///< 证书原始内容（二进制）
	int CertificateSize;			///< 证书CertificateData的字节数
	int CrlType;					///< 吊销列表格式：0-无（如果码流证书中有CRL分发点，则根据证书的分发点获取CRL，否则不检查CRL）；1-CrlData本身为CRL文件内容；2-CrlData为指向CRL文件的URL（如HTTP链接）。
	char* CrlData;					///< 吊销列表格式，内容视CrlType属性取值而定
	int CrlSize;					///< CrlData的字节数
}SVAC_X509CertInfo;


typedef struct _tagSVAC_SingnatureCompareParams
{
	BOOL bEnable;
	SVAC_X509CertInfo *pX509CertInfo;
	int nCertCnt;
}SVAC_SCP;

typedef enum
{
	SVAC_S_SM1_OFB_PKCS5 = 0x00000001,
	SVAC_S_SM4_OFB_PKCS5 = 0x00000002,
	SVAC_H_SM3 = 0x00000001,
	SVAC_A_SM2 = 0x00000001,
	SVAC_SI_SM3_SM2 = 0x00000001
} SVAC_BD_CAPS;

typedef struct
{
 	SVAC_BD_CAPS ACaps;  //非对称算法描述
 	SVAC_BD_CAPS HCaps;  //杂凑算法描述
 	SVAC_BD_CAPS SCaps;  //对称算法描述
 	SVAC_BD_CAPS SICaps; //签名算法描述
} SVAC_CAPS;

typedef enum
{
    SVAC_CERT_CHAINNOMATCH = 0x00,  // 根证书无效或不匹配
    SVAC_CERT_CHANGED   =   0x01,   // 码流中的certID有变化
    SVAC_CERT_MISSING   =   0x02,   // 导入的证书中缺少 certid 中指定的证书
    SVAC_CERT_INVALID   =   0x04,   // 证书id不匹配
    SVAC_CERT_NOMATCH   =   0x08,   // 弃用
    SVAC_CERT_CERTVERIFY =  0x10,   // 码流中的certid对应的证书证书链校验通过
    SVAC_CERT_ROOTCERT_MISS =   0x20,// 未导入证书链
    SVAC_CERT_CERTVERIFY_ERR    =   0x40,   // 证书链和证书都导入,但是验证失败,可能是因为证书链或待验证证书内容被修改
    SVAC_CERT_CERTDATE_ERR      =   0x80,   /* 证书有效期错误 */
    SVAC_CERT_CERTFORMAT_ERR    =   0x0100, /* 证书格式错误解析失败,无法转化成X509 */
    SVAC_CERT_CLEAR         =  0xFFFF000F,

    SVAC_VISS_AUTH_CLEAR      =   0xFFFF0000,
    SVAC_VISS_AUTH_OK   =   0x80000000, // 针对 VISS 平台客户端增加
    SVAC_VISS_AUTH_FAIL   =   0x40000000,
    SVAC_VISS_AUTH_NODATA   =   0x20000000
}SVAC_CERT_STAT;

typedef struct
{
	unsigned char *data;
	int				len;
} SVAC_ELEM;

typedef struct
{
	SVAC_ELEM svrID;
	SVAC_ELEM r1;
} SVAC_SIGN_INPARAM1;

typedef struct
{
	SVAC_ELEM r2;
	SVAC_ELEM sign1;
} SVAC_SIGN_OUTPARAM1;

typedef struct
{
	// verify sign1
	SVAC_ELEM r1;
	SVAC_ELEM r2;
	SVAC_ELEM svrID;
	SVAC_ELEM sign;		// contain sign1, after thar svac the sign2

						// sign sign2
	SVAC_ELEM devID;
	SVAC_ELEM cVkek; 	// contain input cryptkey enc by sdk pbkey, 
						// if len=0, creat by sdk, return cryptkey enc by sdk pbkey
	SVAC_ELEM keyVer;   // cVkek 对应的 vkek version
} SVAC_VEFY_INPARAM1;

typedef struct
{
	SVAC_ELEM cryptKey; //SM2加密后的数据，并经过base64处理
	SVAC_ELEM sign2;
} SVAC_VEFY_OUTPARAM1;

typedef struct
{
	// verify sign2
	SVAC_ELEM r1;
	SVAC_ELEM r2;
	SVAC_ELEM devID;
	SVAC_ELEM sign2;
	SVAC_ELEM cryptKey;
	SVAC_ELEM keyVer;
	SVAC_ELEM srvID;	// search server pub key
} SVAC_VEFY_INPARAM2;

typedef struct
{
	SVAC_ELEM date;
	SVAC_ELEM method;
	SVAC_ELEM from;
	SVAC_ELEM to;
	SVAC_ELEM callID;
	SVAC_ELEM msg;
	SVAC_ELEM nonce;
} SVAC_SIPPARAM;

typedef struct
{
	SVAC_ELEM vkekVer;	/* 标识 vkek  号, 唯一 , 与 vkek 一一对应  */
	SVAC_ELEM cVkek;    /* 密文形式的 vkek , 用于解密 vek          */
} SVAC_VKEK_PARAM;

typedef struct
{
	SVAC_ELEM certID;
	SVAC_ELEM pbKey;
} SVAC_PUBKEY_PARAM;

typedef struct
{
	SVAC_ELEM bindID;	// 说明该证书和哪个ID绑定，serID or devID
	SVAC_ELEM certID;
	SVAC_ELEM cert;
} SVAC_CERT_PARAM;

typedef struct
{
	SVAC_ELEM camID;
	SVAC_ELEM svrID;
   int vekInterval;    //vek 更新周期单位秒
   unsigned int res[32];
}SVAC_SIPSEC_PARAM;

typedef struct
{
	unsigned int   type;							//viss or IE 证书 SVAC_DEVCERT_PUBLISHTYPE		 
	unsigned int   certType;						//ukey证书类型 0: ECC 
	unsigned int   keyUsage;						/* 0:加密证书; 1:签名证书 */
	unsigned char  certID[DATA_LEN];
	unsigned char  certFrom[DATA_S_LEN];			// ukey内证书颁发者
	unsigned char  certUser[DATA_S_LEN];			// ukey内证书使用者
	unsigned char  certDateFrom[DATA_S_LEN];		//ukey证书颁发日期
	unsigned char  certDateTo[DATA_S_LEN];			//ukey证书截至日期
	SVAC_ELEM	   certData;						//证书信息
#if  defined (__x86_64__) || defined (_WIN64)
	unsigned char res[500];
#else
	unsigned char res[504];
#endif
}SVAC_DEVCERT_INFO;

typedef struct
{
	unsigned char		ctnName[DATA_LEN];
	unsigned int		num;						//cert个数
	SVAC_DEVCERT_INFO	*devCertInfo;
	unsigned char		res[512];
}SVAC_DEVCTN_INFO;

typedef struct
{
	unsigned char		appName[DATA_LEN];
	unsigned int		num;						//ctn个数
	SVAC_DEVCTN_INFO	*devCtnInfo;
	
	unsigned char		res[512];
}SVAC_DEVAPP_INFO;

typedef struct
{
	unsigned char		mfrsID[4];					// ukey厂家或者型号ID  01型 02型 05型
	unsigned char		devName[DATA_LEN];			//ukey的枚举名,也是open设备时候使用的 devName
	unsigned char		devSN[DATA_S_LEN];			//ukey 序列号
	unsigned int		slotIndex;					//插槽ID（PCIe卡）
	unsigned int		num;						//app个数
	SVAC_DEVAPP_INFO	*devAppInfo;
	
	unsigned char		res[508];
}SVAC_SEARCHRESULT;

typedef struct
{
	unsigned int num;					// 结果个数
	SVAC_SEARCHRESULT *result;
	unsigned int res[4];
}SVAC_AUTOSEARCH_RESULT;

typedef struct
{
	SVAC_ELEM	   mfrsID;				//ukey厂家类型
	SVAC_ELEM	   devName;				//ukey的枚举名, 也是open设备时候使用的 devName
	SVAC_ELEM	   appName;				//应用名，输入空采用默认应用名
	SVAC_ELEM	   cntName;				//容器名，输入空采用默认容器名
	SVAC_ELEM	   keyPin;				//ukey的pin码，输入空采用默认pin码
	SVAC_ELEM	   devSN;				//Ukey序列号
	unsigned int   reserved[488];
} SVAC_UKEY_INFO;

typedef struct
{
	unsigned char		devId[4];
	int					stat;
	unsigned char		devName[128];   //ukey的枚举名,也是open设备时候使用的 devName
	unsigned char		devSN[64];      //ukey 序列号
	unsigned int		num;			//app个数
	SVAC_DEVAPP_INFO	*devAppInfo;
	unsigned char		res[512];
}SVAC_SECDEV_STAT;

typedef struct
{
	void*               pBitstream;	//码流首地址
	int	                nLen;		//码流长度
	int                 frameType;     //0：未知类型SVAC流	1：I帧    2：P帧    3：B帧	4:SVAC音频
	int                 modeType;        //0：密码转密    1：Ukey转密    2：脱密
	int                 decrypType;        //脱密模式下，此参数生效 1:图像数据是否脱密	2:扩展信息是否脱密	3:图像数据和扩展信息是否脱密
	int                 Reserved[99];
}TRS_INPUT_PARAM;

typedef struct
{
	void*               pBitstream;	//码流首地址（转密或者脱密出来的码流）
	int	                nLen;		//码流长度    
	int                 Reserved[100];
}TRS_OUTPUT_PARAM;

typedef struct
{
	SVAC_ELEM			camID;
	SVAC_ELEM			startTime;
	SVAC_ELEM			endTime;
	unsigned int		res[16];
}SVAC_VKEKVERRANGE_INFO;

typedef struct
{
	unsigned int asymALG;    // 指定 cryptedKey 的非对称加密方式 SVAC_ASYM_ALG
	unsigned int symType;     // 加密或解密inData的算法 SVAC_ENCRYPT_TYPE
	unsigned int symMode;        // SVAC_ENCRYPT_MODE
	unsigned int mode;       // 0:解密; 1:加密
	SVAC_ELEM   cryptedKey; // 非对称算法加密后的密码
	SVAC_ELEM   iv; // 根据算法需要
	SVAC_ELEM   inData; // 待处理数据
	unsigned int res[16];
}SVAC_EXTERNAL_CIPHER;

typedef struct {
	DWORD    dwSdkVersion;  //本sdk版本，其中高16位是版本号，低16位是build号。例如:1.0.1.5
	DWORD    dwSVACDecoderVersion; //SVAC解码器版本,其中高16位是版本号，低16位是build号。例如:3.1.1.20
	DWORD	 dwSVAC1DecoderVersion; //SVAC解码器SVAC1版本,其中高16位是版本号，低16位是build号。例如:3.1.1.20
	DWORD	 dwSVAC2DecoderVersion; //SVAC解码器SVAC2版本,其中高16位是版本号，低16位是build号。例如:3.1.1.20
	DWORD	 dwSVACVSECVersion;		//安全库版本，其中高16位是版本号，低16位是build号。例如:3.1.1.20
	DWORD	 dwSVACAudioVersion;	//音频库版本，其中高16位是版本号，低16位是build号。例如:3.1.1.20
	DWORD    dwRes[4];   //预留字段
}SDK_VERSION;

typedef  int (CALLBACK* SVAC_GetSecDevStat)(void* devStat, void* userData);

ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_Init();
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SubscribeDevType(const char *pDevType, int nLen);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_GetSupportUKey(const char **p2Name, int nPointerCnt);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SearchUKEYResult(SVAC_AUTOSEARCH_RESULT *pSearchResult);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_Open(HANDLE& handle, int thread_num, int core_num, int svac_version);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetUKey(HANDLE handle, SVAC_UKEY_INFO *pUkeyInfo);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_Close(HANDLE handle);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_PrefetchParam(HANDLE handle, unsigned char *pBuf, int uBufLen, SVAC_PREFETCH_PARAM *pPreParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_Decode(HANDLE handle,
	DEC_INPUT_PARAM* decin,
	DEC_OUTPUT_PARAM* decout,
	EXT_INFO_COMMON* pExt_info);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_DecodeAudio(HANDLE handle, unsigned char *pSrcBuf, int uBufLen, unsigned char *pDestBuf);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetPassWord(HANDLE handle, char* password, int pswlength);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetTrustCerts(HANDLE handle, SVAC_SCP *pStSCP);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetRSAKey(HANDLE handle, unsigned int rsa_e, unsigned int rsa_n);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetVKEKBase64(HANDLE handle, char *pVKEKBase64,
	int s32Length, char *pVersion, int s32VersionLength);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetVKEKBase64Ex(HANDLE handle, char *pVKEKBase64,
	int s32Length, char *pVersion, int s32VersionLength, char *pGbInfo, int s32GbLength);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetCommonExtensionInfo(HANDLE handle, BOOL bCommonExtensionInfo, BOOL bDeduplication);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_GetSdkVersion(SDK_VERSION* pSdkVersion);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_GetSECInfo(HANDLE handle, SVAC_SEC_INFO *pSecInfo);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetSecurityLib(HANDLE handle,
	void *hVSECLib, void* hVSEC);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetSecDevStatCallBack(SVAC_GetSecDevStat devStatCallBack, void* userData);

ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_DecryptOpen(HANDLE &handle);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_Decrypt(HANDLE handle, int uStreamType, unsigned char *pStreamIn, int uStreamInLen, \
	unsigned char *pStreamOut, int uStreamOutLen, int uDecryptMode);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_GetCaps(HANDLE handle, SVAC_CAPS *pCaps);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_GetR1(HANDLE handle, unsigned char *pData, unsigned int *pLen);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_GetSign1(HANDLE handle, SVAC_SIGN_INPARAM1 *pInParam, SVAC_SIGN_OUTPARAM1 *pOutParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_VefySign1(HANDLE handle, SVAC_VEFY_INPARAM1 *pInParam, SVAC_VEFY_OUTPARAM1 *pOutParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_VefySign2(HANDLE handle, SVAC_VEFY_INPARAM2 *pInParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_SIPAuth(HANDLE handle, SVAC_SIPPARAM *pSipParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_SIPVefy(HANDLE handle, SVAC_SIPPARAM *pSipParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_VkekTransf(HANDLE handle, SVAC_VKEK_PARAM *pVkekParam, SVAC_PUBKEY_PARAM *pPubkParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_GetCryptKey(HANDLE handle, SVAC_VKEK_PARAM *pVkekParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_BD_Open(HANDLE& handle);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_DEC_SetCert(HANDLE handle, SVAC_CERT_PARAM *pCertParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_CERT_SetEncCert(HANDLE handle, SVAC_CERT_PARAM *pCertParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_CERT_GetDevCert(SVAC_UKEY_INFO *pUkeyInfo, unsigned char *certBuf, int *certLen);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_CERT_GetDevEncCert(SVAC_UKEY_INFO *pUkeyInfo, unsigned char *certBuf, int *certLen);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_TransferOpen(HANDLE& handle, int svac_version);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_Transfer(HANDLE handle, TRS_INPUT_PARAM* pInParam, TRS_OUTPUT_PARAM* pOutParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SimpleOpen(HANDLE &handle, int extinfo_flag, int authen_flag, int svac_version);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetSIPSecParam(HANDLE handle, SVAC_SIPSEC_PARAM* sipSecParam);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_GetHandle(HANDLE &handle);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_UnInit();
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_DEC_SetVkekVerRange(HANDLE handle, SVAC_VKEKVERRANGE_INFO *vkekVerRange);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_SetSupportUKey(unsigned char *pUkeyType, int nLen);
ZXSVACDECLIB_API int CALLMETHOD ZXSVACDec_ExternalDataCipher(HANDLE handle, SVAC_EXTERNAL_CIPHER *inData, SVAC_ELEM *outData);
#endif
