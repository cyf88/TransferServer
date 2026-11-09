
#include "svac.h"
#include "bs.h"
#include "logger.h"


svac_stream_t *svac_new() {
    svac_stream_t *h = (svac_stream_t *)calloc(1, sizeof(svac_stream_t));
    h->nal           = (svac_nalu_t *)calloc(1, sizeof(svac_nalu_t));
    h->sps           = (svac_sps_t *)calloc(1, sizeof(svac_sps_t));
    h->pps           = (svac_pps_t *)calloc(1, sizeof(svac_pps_t));
    h->ses           = (svac_ses_t *)calloc(1, sizeof(svac_ses_t));
    h->auth          = (svac_auth_t *)calloc(1, sizeof(svac_auth_t));
    h->ext           = (svac_extension_t *)calloc(1, sizeof(svac_extension_t));

    return h;
}

void svac_free(svac_stream_t *h) {
    free(h->nal);
    free(h->sps);
    free(h->pps);
    free(h->ses);
    free(h->auth);
    free(h->ext);
    free(h);
}

/**
   Convert NAL data (Annex B format) to RBSP data.
   The size of rbsp_buf must be the same as size of the nal_buf to guarantee the output will fit.
   If that is not true, output may be truncated and an error will be returned.
   Additionally, certain byte sequences in the input nal_buf are not allowed in the spec and also cause the conversion to fail and
   an error to be returned.
   @param[in] nal_header_size the nal header
   @param[in] nal_buf   the nal data
   @param[in,out] nal_size  as input, pointer to the size of the nal data; as output, filled in with the actual size of the nal
   data
   @param[in,out] rbsp_buf   allocated memory in which to put the rbsp data
   @param[in,out] rbsp_size  as input, pointer to the maximum size of the rbsp data; as output, filled in with the actual size of
   rbsp data
   @return  actual size of rbsp data, or -1 on error
 */
// 7.3.1 NAL unit syntax
// 7.4.1.1 Encapsulation of an SODB within an RBSP

int nal_to_rbsp(const int nal_header_size, const uint8_t *nal_buf, int *nal_size, uint8_t *rbsp_buf, int *rbsp_size) {
    int i;
    int j     = 0;
    int count = 0;

    for (i = nal_header_size; i < *nal_size; i++) {
        // in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any byte-aligned position
        if ((count == 2) && (nal_buf[i] < 0x03)) {
            BLOG(error) << "nal_to_rbsp: 0x000000, 0x000001 or 0x000002 shall not occur at any byte-aligned position";
            return -1;
        }

        if ((count == 2) && (nal_buf[i] == 0x03)) {
            // check the 4th byte after 0x000003, except when cabac_zero_word is used, in which case the last three bytes of this
            // NAL unit must be 0x000003

            if ((i < *nal_size - 1) && (nal_buf[i + 1] > 0x03)) {
                BLOG(error) << "nal_to_rbsp: 0x000003 shall not occur at any byte-aligned position";
                return -1;
            }

            // if cabac_zero_word is used, the final byte of this NAL unit(0x03) is discarded, and the last two bytes of RBSP must
            // be 0x0000
            if (i == *nal_size - 1) {
                break;
            }

            i++;
            count = 0;
        }

        if (j >= *rbsp_size) {
            // error, not enough space
            return -1;
        }

        rbsp_buf[j] = nal_buf[i];
        if (nal_buf[i] == 0x00) {
            count++;
        } else {
            count = 0;
        }
        j++;
    }

    *nal_size  = i;
    *rbsp_size = j;
    return j;
}

/**
   Convert RBSP data to NAL data (Annex B format).
   The size of nal_buf must be 4/3 * the size of the rbsp_buf (rounded up) to guarantee the output will fit.
   If that is not true, output may be truncated and an error will be returned.
   If that is true, there is no possible error during this conversion.
   @param[in] rbsp_buf   the rbsp data
   @param[in] rbsp_size  pointer to the size of the rbsp data
   @param[in,out] nal_buf   allocated memory in which to put the nal data
   @param[in,out] nal_size  as input, pointer to the maximum size of the nal data; as output, filled in with the actual size of
   the nal data
   @return  actual size of nal data, or -1 on error
 */
// 7.3.1 NAL unit syntax
// 7.4.1.1 Encapsulation of an SODB within an RBSP
int rbsp_to_nal(const uint8_t *rbsp_buf, const int *rbsp_size, uint8_t *nal_buf, int *nal_size) {
    int i;
    int j     = 1;
    int count = 0;

    if (*nal_size > 0) {
        nal_buf[0] = 0x00;
    } // zero out first byte since we start writing from second byte

    for (i = 0; i < *rbsp_size; i++) {
        if (j >= *nal_size) {
            // error, not enough space
            return -1;
        }

        // HACK 0xFC
        if ((count == 2) && !(rbsp_buf[i] & 0xFC)) {
            nal_buf[j] = 0x03;
            j++;
            count = 0;
        }
        nal_buf[j] = rbsp_buf[i];
        if (rbsp_buf[i] == 0x00) {
            count++;
        } else {
            count = 0;
        }
        j++;
    }

    *nal_size = j;
    return j;
}


//检测到 00 00 [00,01,02,03]添加03
int prevention(uint8_t* src, size_t srcLen, uint8_t* dst, size_t* dstLen) {
	if (src == NULL || srcLen < 3) {
		return -1;
	}
	int dstPtr = 0;
	for (int i = 0; i < srcLen; i++) {
		uint32_t windows_data = src[i] << 16 | src[i + 1] << 8 | src[i + 2];
		windows_data = windows_data & 0xffffff;
		if (windows_data <= uint32_t(0x03)) {
			dst[dstPtr++] = 0x00;
			dst[dstPtr++] = 0x00;
			dst[dstPtr++] = 0x03;
			i++;
			continue;
		}
		dst[dstPtr++] = src[i];
	}
    *dstLen = dstPtr;
	return dstPtr;
}


int read_nal_unit(svac_stream_t *h, uint8_t *buf, int size) {

    svac_nalu_t *nal = h->nal;

    bs_t *b = bs_new(buf, size);

    nal->forbidden_zero_bit = bs_read_u(b, 1);
    nal->nal_ref_idc        = bs_read_u(b, 1);
    nal->nal_unit_type      = bs_read_u(b, 4);
    nal->encryption_idc     = bs_read_f(b, 1);
    nal->authentication_idc = bs_read_f(b, 1);
    nal->parsed             = buf;
    nal->sizeof_parsed      = size;

    bs_free(b);

    int nal_size      = size;
    int rbsp_size     = size;
    uint8_t *rbsp_buf = (uint8_t *)malloc(rbsp_size);

    int rc = nal_to_rbsp(1, buf, &nal_size, rbsp_buf, &rbsp_size);

    if (rc < 0) {
        free(rbsp_buf);
        return -1;
    }

    b = bs_new(rbsp_buf, rbsp_size);

    switch (nal->nal_unit_type) {

        case NAL_SPS: read_seq_parameter_set_rbsp(h, b); break;
        case NAL_PPS: read_pic_parameter_set_rbsp(h, b); break;
        case NAL_SES: read_sec_parameter_set_rbsp(h, b); break;
        case NAL_AUTH: read_auth_rbsp(h, b); break;
        case NAL_SURVEILLANCE_EXTENSION_UNIT: read_surveillance_extension_rbsp(h, b); break;
        default:
            nal->parsed        = NULL;
            nal->sizeof_parsed = 0;
            return 0;
    }

    if (bs_overrun(b)) {
        bs_free(b);
        free(rbsp_buf);
        return -1;
    }

    bs_free(b);
    free(rbsp_buf);

    return nal_size;
}

void read_seq_parameter_set_rbsp(svac_stream_t *h, bs_t *b) {

    svac_sps_t *sps = h->sps;

    sps->profile_id            = bs_read_u8(b);
    sps->level_id              = bs_read_u8(b);
    sps->ldp_mode_flag         = bs_read_u1(b);
    sps->frame_width_minus_1   = bs_read_u(b, 16);
    sps->frame_height_minus_1  = bs_read_u(b, 16);
    sps->chroma_format_idc     = bs_read_u(b, 2);
    sps->bit_depth             = bs_read_u(b, 2);
    sps->refs_per_frame        = bs_read_u(b, 3);
    sps->frame_rate            = bs_read_u(b, 3);
    sps->extended_sb_size_flag = bs_read_u1(b);
    sps->tile_enable           = bs_read_u1(b);
    sps->wpp_enable            = bs_read_u1(b);
    sps->sao_enable            = bs_read_u1(b);
    sps->alf_enable            = bs_read_u1(b);
    sps->roi_flag              = bs_read_u1(b);
    sps->temporal_svc_flag     = bs_read_u1(b);
    if (sps->temporal_svc_flag) {
        sps->layer_num_minus_1 = bs_read_u(b, 2);
    }
    sps->spatial_svc_flag = bs_read_u(b, 2);
    if (sps->spatial_svc_flag) {
        sps->svc_ratio = bs_read_u(b, 3);
        sps->svc_mode  = bs_read_u1(b);
    }
    if (sps->frame_rate >= 4) {
        sps->vui_parameters_present_flag = bs_read_u1(b);
    }
    if (sps->vui_parameters_present_flag) {
        sps->vui.timing_info_present_flag = bs_read_u1(b);
        if (sps->vui.timing_info_present_flag) {
            sps->vui.num_units_in_tick     = bs_read_u(b, 32);
            sps->vui.time_scale            = bs_read_u(b, 32);
            sps->vui.fixed_frame_rate_flag = bs_read_u1(b);
        }
        sps->vui.hrd_parameters_present_flag = bs_read_u1(b);
        if (sps->vui.hrd_parameters_present_flag) {
            sps->vui.hrd.cpb_cnt_minus1 = bs_read_ue(b);
            sps->vui.hrd.bit_rate_scale = bs_read_u(b, 4);
            sps->vui.hrd.cpd_size_scale = bs_read_u(b, 4);
            for (int i = 0; i <= sps->vui.hrd.cpb_cnt_minus1; i++) {
                sps->vui.hrd.bit_rate_value_minus1[i] = bs_read_ue(b);
                sps->vui.hrd.cpb_size_value_minus1[i] = bs_read_ue(b);
                sps->vui.hrd.cbr_flag[i]              = bs_read_u1(b);
            }
            sps->vui.hrd.initial_cpb_removal_delay_length_minus1 = bs_read_u(b, 5);
            sps->vui.hrd.pb_removal_delay_length_minus1          = bs_read_u(b, 5);
            sps->vui.hrd.dpb_output_delay_length_minus1          = bs_read_u(b, 5);
        }
    }
}

void read_pic_parameter_set_rbsp(svac_stream_t *h, bs_t *b) {
    svac_pps_t *pps = h->pps;

    pps->frame_num = bs_read_u8(b);
}

void read_auth_rbsp(svac_stream_t *h, bs_t *b) {

    svac_auth_t *auth = h->auth;

    auth->frame_num = bs_read_u8(b);
    if (h->sps->spatial_svc_flag) {
        auth->spatial_el_flag = bs_read_u8(b);
    }
    auth->auth_data_lenth_minus_1 = bs_read_u8(b);

    for (int i = 0; i <= auth->auth_data_lenth_minus_1; i++) {
        auth->authentication_data[i] = bs_read_u8(b);
    }
}

void read_sec_parameter_set_rbsp(svac_stream_t *h, bs_t *b) {

    svac_ses_t *ses = h->ses;

    ses->encryption_flag     = bs_read_u1(b);
    ses->authentication_flag = bs_read_u1(b);

    if (ses->encryption_flag) {
        ses->encryption_type = bs_read_u(b, 4);
        ses->vek_flag        = bs_read_f(b, 1);
        ses->iv_flag         = bs_read_u1(b);
        if (ses->vek_flag) {
            ses->vek_encryption_type = bs_read_u(b, 4);
            ses->evek_length_minus1  = bs_read_u8(b);
            for (int i = 0; i <= ses->evek_length_minus1; i++) {
                ses->evek[i] = bs_read_u8(b);
            }
            // bs_read_bytes(b, ses->evek, ses->evek_length_minus1 + 1);
            ses->vkek_version_length_minus1 = bs_read_u8(b);
            for (int i = 0; i <= ses->vkek_version_length_minus1; i++) {
                ses->vkek_version[i] = bs_read_u8(b);
            }
            // bs_read_bytes(b, ses->vkek_version, ses->vkek_version_length_minus1+1);
        }
        if (ses->iv_flag) {
            ses->iv_length_minus1 = bs_read_u8(b);
            for (int i=0; i<= ses->iv_length_minus1; i++) {
                ses->iv[i] = bs_read_u8(b);
            }       
            //bs_read_bytes(b, ses->iv, ses->iv_length_minus1 + 1);
        }
    }

    if (ses->authentication_flag) {
        ses->hash_type                       = bs_read_u(b, 2);
        ses->hash_discard_p_pictures         = bs_read_u1(b);
        ses->signature_type                  = bs_read_u(b, 2);
        ses->successive_hash_pictures_minus1 = bs_read_u8(b);
        for (int i = 0; i < 152 / 8; i++) {
            ses->camera_idc[i] = bs_read_u8(b);
        }
        // bs_read_bytes(b, ses->camera_idc, 19);
    }

    if (ses->vek_flag || ses->authentication_flag) {
        for (int i = 0; i < 160 / 8; i++) {
            ses->camera_id[i] = bs_read_u8(b);
        }
        
        // bs_read_bytes(b, ses->camera_id, 20);
    }
}

// 第一个字节ext_id之前已经读取
void read_time_extension(svac_stream_t *h, bs_t *b) {
    svac_extension_t *ext                      = h->ext;
    ext->time_extension_t.extension_length     = bs_read_u8(b);
    ext->time_extension_t.hour_bits            = bs_read_u(b, 5);
    ext->time_extension_t.minute_bits          = bs_read_u(b, 6);
    ext->time_extension_t.second_bits          = bs_read_u(b, 6);
    ext->time_extension_t.second_fraction_bits = bs_read_u(b, 14);
    ext->time_extension_t.ref_data_flag        = bs_read_u1(b);
    if (ext->time_extension_t.ref_data_flag) {
        ext->time_extension_t.year_minus2000_bits = bs_read_u(b, 7);
        ext->time_extension_t.month_bits          = bs_read_u(b, 4);
        ext->time_extension_t.day_bits            = bs_read_u(b, 5);
    }
}

void read_osd_extension(svac_stream_t *h, bs_t *b) {
    svac_extension_t *ext                 = h->ext;
    ext->osd_extension_t.extension_length = bs_read_u8(b);
    ext->osd_extension_t.sub_type         = bs_read_u8(b);
    ext->osd_extension_t.code_type        = bs_read_u8(b);
    ext->osd_extension_t.align_type       = bs_read_u8(b);
    ext->osd_extension_t.char_size        = bs_read_u8(b);
    ext->osd_extension_t.char_type        = bs_read_u8(b);
    ext->osd_extension_t.top_low8         = bs_read_u8(b);
    ext->osd_extension_t.top_high8        = bs_read_u8(b);
    ext->osd_extension_t.left_low8        = bs_read_u8(b);
    ext->osd_extension_t.left_high8       = bs_read_u8(b);
    ext->osd_extension_t.len              = bs_read_u8(b);
    ext->osd_extension_t.res              = bs_read_u(b, 24);
    for (int i = 0; i < ext->osd_extension_t.len; i++) {
        ext->osd_extension_t.osd_data[i] = bs_read_u8(b);
    }
}

void read_surveillance_extension_rbsp(svac_stream_t *h, bs_t *b) {

    int next_u8;
    while ((next_u8 = bs_read_u8(b)) != (int)0x80) {
        switch (next_u8) {
            case EXTENSION_TIME: {
                h->ext->time_extension_t.extension_id = next_u8;
                read_time_extension(h, b);
                break;
            }
            case EXTENSION_OSD: {
                h->ext->osd_extension_t.extension_id = next_u8;
                read_osd_extension(h, b);
                break;
            }
            default: return;
        }
    }
}
