#ifndef SVAC_H
#define SVAC_H

#include "bs.h"
#include <stdint.h>
#include <stdio.h>

#define SVAC_TYPE(v) ((uint8_t)(v >> 2) & 0x0F)

typedef enum {
    NAL_NONE_IDR                    = 1,
    NAL_IDR                         = 2,
    NAL_NONE_IDR_SVC                = 3,
    NAL_IDR_SVC                     = 4,
    NAL_SURVEILLANCE_EXTENSION_UNIT = 5,
    NAL_SEI                         = 6,
    NAL_SPS                         = 7,
    NAL_PPS                         = 8,
    NAL_SES                         = 9,
    NAL_AUTH                        = 10,
    NAL_E0STREAM                    = 11,
    NAL_RESERVED1                   = 12,
    NAL_CHAPTER6                    = 13,
    NAL_RESERVED2                   = 14,
    NAL_SVC_PPS                     = 15,
} svac_nal_unit_type_t;

typedef enum {
    EXTENSION_TIME     = 0x04,
    EXTENSION_GIS      = 0x10,
    EXTENSION_ANALYSIS = 0x11,
    EXTENSION_OSD      = 0x12,
    EXTENSION_RESERVED
} svac_extension_type_t;

typedef struct {
    int forbidden_zero_bit;
    int nal_ref_idc;
    int nal_unit_type;
    int encryption_idc;
    int authentication_idc;
    void *parsed; // buf
    int sizeof_parsed;
    uint8_t *buf;
    size_t buf_size;

    // uint8_t* rbsp_buf;
    // int rbsp_size;
} svac_nalu_t;

// 时间参数结构体规范化
typedef struct {
    int extension_id;
    int extension_length;
    int hour_bits;
    int minute_bits;
    int second_bits;
    int second_fraction_bits;
    int ref_data_flag;
    int year_minus2000_bits;
    int month_bits;
    int day_bits;
} svac_time_parameters_t;


typedef struct {
    int profile_id;
    int level_id;
    int ldp_mode_flag;
    int frame_width_minus_1;
    int frame_height_minus_1;
    int chroma_format_idc;
    int bit_depth;
    int refs_per_frame;
    int frame_rate;
    int extended_sb_size_flag;
    int tile_enable;
    int wpp_enable;
    int sao_enable;
    int alf_enable;
    int roi_flag;
    int temporal_svc_flag;
    int layer_num_minus_1;
    int spatial_svc_flag;
    int svc_ratio;
    int svc_mode;

    int vui_parameters_present_flag;

    struct {
        int timing_info_present_flag;
        int num_units_in_tick;
        int time_scale;
        int fixed_frame_rate_flag;

        int hrd_parameters_present_flag;

        struct {
            int cpb_cnt_minus1;
            int bit_rate_scale;
            int cpd_size_scale;
            int bit_rate_value_minus1[32]; // up to cpb_cnt_minus1, which is <= 31
            int cpb_size_value_minus1[32];
            int cbr_flag[32];
            int initial_cpb_removal_delay_length_minus1;
            int pb_removal_delay_length_minus1;
            int dpb_output_delay_length_minus1;
        } hrd;

        int low_delay_hrd_flag;

        int max_dec_frame_buffering;

    } vui;

} svac_sps_t;

typedef struct {

    int frame_num;
    int layer_id;
    int svc_roi_flag;

} svac_pps_t;

typedef struct {
    int encryption_flag; // u(1)
    int authentication_flag; // u(1)
    int encryption_type; // u(4)
    int vek_flag; // u(1)
    int iv_flag; // u(1)
    int vek_encryption_type; // u(4)
    int evek_length_minus1; // u(8)
    unsigned char evek[32];

    int vkek_version_length_minus1; // u(8)
    unsigned char vkek_version[32]; // f(n)

    int iv_length_minus1; // u(8)
    unsigned char iv[32]; // f(n)

    int hash_type; // u(2)
    int hash_discard_p_pictures; // u(1)
    int signature_type; // u(2)
    int successive_hash_pictures_minus1; // u(8)
    unsigned char camera_idc[19]; // f(152)
    unsigned char camera_id[20];
} svac_ses_t;

typedef struct {

    int frame_num;
    int spatial_el_flag;
    int auth_data_lenth_minus_1;
    unsigned char authentication_data[512];
} svac_auth_t;

typedef struct {

    struct {
        int extension_id;
        int extension_length;
        int hour_bits;
        int minute_bits;
        int second_bits;
        int second_fraction_bits;
        int ref_data_flag;
        int year_minus2000_bits;
        int month_bits;
        int day_bits;
    } time_extension_t;

    struct {
        int extension_id;
        int extension_length;
        unsigned char camera_id[20];
        int analysis_num;
        unsigned char analysis_id[128];
        unsigned char description_type[128];
        int data_length[128];
        int analysis_data[128][1024];

    } analysis_extension_t;

    struct {
        int extension_id;
        int extension_length;
        int sub_type;
        int code_type;
        int align_type;
        int char_size;
        int char_type;
        int top_low8;
        int top_high8;
        int left_low8;
        int left_high8;
        int len;
        int res;
        unsigned char osd_data[255];

    } osd_extension_t;

    struct {
        int extension_id;
        int extension_length;
        int longitude_type;
        int longitude_degree;
        int longitude_fraction_bit;
        int latitude_type;
        int latitude_degree;
        int latitude_fraction_bit;
        int height;
        int speed;
        int yaw_degree;
        int reserverd;
    } gis_extension_t;

} svac_extension_t;

typedef struct {
    int extension_id;
    int extension_length;
    int hour_bits;
    int minute_bits;
    int second_bits;
    int second_fraction_bits;
    int ref_data_flag;
    int year_minus2000_bits;
    int month_bits;
    int day_bits;
} svac_time_parameters;


// svac_stream_t 结构体
typedef struct {

    svac_nalu_t *nal;
    svac_sps_t *sps;
    svac_pps_t *pps;
    svac_ses_t *ses;
    svac_auth_t *auth;
    svac_extension_t *ext;

} svac_stream_t;

svac_stream_t *svac_new();
void svac_free(svac_stream_t *h);

int rbsp_to_nal(const uint8_t *rbsp_buf, const int *rbsp_size, uint8_t *nal_buf, int *nal_size);
int nal_to_rbsp(const int nal_header_size, const uint8_t *nal_buf, int *nal_size, uint8_t *rbsp_buf, int *rbsp_size);
int prevention(uint8_t* src, size_t srcLen, uint8_t* dst, size_t* dstLen);

int read_nal_unit(svac_stream_t *h, uint8_t *buf, int size);

void read_seq_parameter_set_rbsp(svac_stream_t *h, bs_t *b);

void read_pic_parameter_set_rbsp(svac_stream_t *h, bs_t *b);

void read_sec_parameter_set_rbsp(svac_stream_t *h, bs_t *b);

void read_auth_rbsp(svac_stream_t *h, bs_t *b);

void read_surveillance_extension_rbsp(svac_stream_t *h, bs_t *b);

#endif // SVAC_H
