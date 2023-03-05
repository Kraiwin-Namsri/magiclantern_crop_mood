#include <dryos.h>
#include <module.h>
#include <config.h>
#include <menu.h>
#include <beep.h>
#include <property.h>
#include <patch.h>
#include <bmp.h>
#include <lvinfo.h>
#include <powersave.h>
#include <raw.h>
#include <fps.h>
#include <shoot.h>
#include <lens.h>
#include "../mlv_lite/mlv_lite.h"

#undef CROP_DEBUG

#ifdef CROP_DEBUG
#define dbg_printf(fmt,...) { printf(fmt, ## __VA_ARGS__); }
#else
#define dbg_printf(fmt,...) {}
#endif

static int is_DIGIC_5 = 0;
static int is_5D3 = 0;
static int is_6D = 0;
static int is_700D = 0;
static int is_650D = 0;
static int is_100D = 0;
static int is_EOSM = 0;
static int is_basic = 0;

static CONFIG_INT("crop.preset", crop_preset_index, 0);
static CONFIG_INT("crop.shutter_range", shutter_range, 0);

static CONFIG_INT("crop.bit_depth", bit_depth_analog, 0);
#define OUTPUT_14BIT (bit_depth_analog == 0)
#define OUTPUT_12BIT (bit_depth_analog == 1)
#define OUTPUT_11BIT (bit_depth_analog == 2)
#define OUTPUT_10BIT (bit_depth_analog == 3)

static CONFIG_INT("crop.preset_aspect_ratio", crop_preset_ar, 0);
#define AR_16_9        (crop_preset_ar == 0)
#define AR_2_1         (crop_preset_ar == 1)
#define AR_2_20_1      (crop_preset_ar == 2)
#define AR_2_35_1      (crop_preset_ar == 3)
#define AR_2_39_1      (crop_preset_ar == 4)

static CONFIG_INT("crop.preset_1x1", crop_preset_1x1_res, 0);
#define CROP_2_5K      (crop_preset_1x1_res == 0)
#define CROP_3K        (crop_preset_1x1_res == 1)
#define CROP_1440p     (crop_preset_1x1_res == 2)
#define CROP_Full_Res  (crop_preset_1x1_res == 3)

static CONFIG_INT("crop.preset_1x3", crop_preset_1x3_res, 0);
#define Anam_Highest   (crop_preset_1x3_res == 0)
#define Anam_Higher    (crop_preset_1x3_res == 1)
#define Anam_Medium    (crop_preset_1x3_res == 2)

static CONFIG_INT("crop.preset_3x3", crop_preset_3x3_res, 0);
#define High_FPS       (crop_preset_3x3_res == 0)
#define mv1080         (crop_preset_3x3_res == 1)

static CONFIG_INT("crop.preset_fps", crop_preset_fps, 0);
#define Framerate_24   (crop_preset_fps == 0)
#define Framerate_25   (crop_preset_fps == 1)
#define Framerate_30   (crop_preset_fps == 2)

enum crop_preset {
    CROP_PRESET_OFF = 0,
    CROP_PRESET_3X,
    CROP_PRESET_3X_TALL,
    CROP_PRESET_3K,
    CROP_PRESET_4K_HFPS,
    CROP_PRESET_UHD,
    CROP_PRESET_FULLRES_LV,
    CROP_PRESET_3x3_1X,
    CROP_PRESET_3x3_1X_48p,
    CROP_PRESET_1x3,
    CROP_PRESET_3x1,
    CROP_PRESET_40_FPS,
    CROP_PRESET_CENTER_Z,
    
    /* these are for 650D / 700D / EOSM/M2 / 100D */
    CROP_PRESET_1X1,
    CROP_PRESET_1X3,
    CROP_PRESET_3X3,
    NUM_CROP_PRESETS
};

/* presets are not enabled right away (we need to go to play mode and back)
 * so we keep two variables: what's selected in menu and what's actually used.
 * note: the menu choices are camera-dependent */
static enum crop_preset crop_preset = 0;

/* must be assigned in crop_rec_init */
static enum crop_preset * crop_presets = 0;

/* current menu selection (*/
#define CROP_PRESET_MENU crop_presets[crop_preset_index]

/* menu choices for 5D3 */
static enum crop_preset crop_presets_5d3[] = {
    CROP_PRESET_OFF,
    CROP_PRESET_3X,
    CROP_PRESET_3X_TALL,
    CROP_PRESET_3x3_1X,
    CROP_PRESET_3x3_1X_48p,
    CROP_PRESET_3K,
    CROP_PRESET_UHD,
    CROP_PRESET_4K_HFPS,
    CROP_PRESET_CENTER_Z,
    CROP_PRESET_FULLRES_LV,
  //CROP_PRESET_1x3,
  //CROP_PRESET_3x1,
  //CROP_PRESET_40_FPS,
};

static const char * crop_choices_5d3[] = {
    "OFF",
    "1920 1:1",
    "1920 1:1 tall",
    "1920 50/60 3x3",
    "1080p45/1040p48 3x3",
    "3K 1:1",
    "UHD 1:1",
    "4K 1:1 half-fps",
    "3.5K 1:1 centered x5",
    "Full-res LiveView",
  //"1x3 binning",
  //"3x1 binning",      /* needs manual LV refresh (by getting outside LV) */
  //"40 fps",
};

static const char crop_choices_help_5d3[] =
    "Change 1080p and 720p movie modes into crop modes (select one)";

static const char crop_choices_help2_5d3[] =
    "\n"
    "1:1 sensor readout (square raw pixels, 3x crop, good preview in 1080p)\n"
    "1:1 crop, higher vertical resolution (1920x1920 @ 24p, cropped preview)\n"
    "1920x960 @ 50p, 1920x800 @ 60p (3x3 binning, cropped preview)\n"
    "1920x1080 @ 45p, 1920x1040 @ 48p, 3x3 binning (50/60 FPS in Canon menu)\n"
    "1:1 3K crop (3072x1920 @ 24p, square raw pixels, preview broken)\n"
    "1:1 4K UHD crop (3840x1600 @ 24p, square raw pixels, preview broken)\n"
    "1:1 4K crop (4096x3072 @ 12.5 fps, half frame rate, preview broken)\n"
    "1:1 readout in x5 zoom mode (centered raw, high res, cropped preview)\n"
    "Full resolution LiveView (5796x3870 @ 7.4 fps, 5784x3864, preview broken)\n"
    "1x3 binning: read all lines, bin every 3 columns (extreme anamorphic)\n"
    "3x1 binning: bin every 3 lines, read all columns (extreme anamorphic)\n"
    "FPS override test\n";

/* menu choices for entry level DIGIC 5 models, 650D / 700D / EOS M/M2 / 100D */
static enum crop_preset crop_presets_DIGIC_5[] = {
    CROP_PRESET_OFF,
    CROP_PRESET_1X1,
    CROP_PRESET_1X3,
    CROP_PRESET_3X3,
};

static const char * crop_choices_DIGIC_5[] = {
    "OFF",
    "1:1 crop",
    "1x3",
    "3x3",
};

static const char crop_choices_help_DIGIC_5[] =
    "Change 1080p and 720p movie modes into crop modes (select one)";
    
    
/* menu choices for cameras that only have the basic 3x3 crop_rec option */
static enum crop_preset crop_presets_basic[] = {
    CROP_PRESET_OFF,
    CROP_PRESET_3x3_1X,
};

static const char * crop_choices_basic[] = {
    "OFF",
    "3x3 720p",
};

static const char crop_choices_help_basic[] =
    "Change 1080p and 720p movie modes into crop modes (one choice)";

static const char crop_choices_help2_basic[] =
    "3x3 binning in 720p (square pixels in RAW, vertical crop)";


/* camera-specific parameters */
static uint32_t CMOS_WRITE               = 0;
static uint32_t MEM_CMOS_WRITE           = 0;
static uint32_t ADTG_WRITE               = 0;
static uint32_t MEM_ADTG_WRITE           = 0;
static uint32_t ENGIO_WRITE              = 0;
static uint32_t MEM_ENGIO_WRITE          = 0;
static uint32_t ENG_DRV_OUT              = 0;
static uint32_t ENG_DRV_OUTS             = 0;
static uint32_t PATH_SelectPathDriveMode = 0;

/* from SENSOR_TIMING_TABLE (fps-engio.c) or FPS override submenu */
static int fps_main_clock = 0;
static int default_timerA[11]; /* 1080p  1080p  1080p   720p   720p   zoom   crop   crop   crop   crop   crop */
static int default_timerB[11]; /*   24p    25p    30p    50p    60p     x5    24p    25p    30p    50p    60p */
static int default_fps_1k[11] = { 23976, 25000, 29970, 50000, 59940, 29970, 23976, 25000, 29970, 50000, 59940 };

/* video modes */

/* properties are fired AFTER the new video mode is fully up and running
 * to apply our presets, we need to know the video more DURING the switch
 * we'll peek into the PathDriveMode structure for that
 * 
 * Example: x10 -> x1 on 5D3
 * This sequence cannot be identified just by looking at C0F06804;
 * some ADTG registers that we need to overide are configured before that.
 * 
 * CtrlSrv: DlgLiveView.c PRESS_TELE_MAG_BUTTON KeyRepeat[0]
 *     Gmt: gmtModeChange
 *     Evf: evfModeChangeRequest(4)
 *     Evf: PATH_SelectPathDriveMode S:0 Z:10000 R:0 DZ:0 SM:1
 *      (lots of stuff going on)
 *     Evf: evfModeChangeComplete
 *      (some more stuff)
 *     Gmt: VisibleParam 720, 480, 0, 38, 720, 404.
 *     Gmt: gmtUpdateDispSize (10 -> 1)
 * PropMgr: *** mpu_send(06 05 09 11 01 00)     ; finally triggered PROP_LV_DISPSIZE...
 */

/* PATH_SelectPathDriveMode S:%d Z:%lx R:%lx DZ:%d SM:%d */
/* offsets verified on 5D3, 6D, 70D, EOSM, 100D, 60D, 80D, 200D */
const struct PathDriveMode
{
    uint32_t SM;            /* 5D3,700D: 0 during zoom, 1 in all other modes; 6D: 2 is flicker-related?! */
    uint32_t fps_mode;      /* 5D3,700D: 0=60p, 1=50p, 2=30p/zoom, 3=25p, 4=24p */
    uint32_t S;             /* 5D3,700D: 0=1080p, 1=720p, 8=zoom, 6=1080crop (700D) */
    uint32_t resolution_idx;/* 5D3,700D: 0=1080p/zoom, 1=720p, 2=640x480 (PathDriveMode->resolution_idx) */
    uint16_t zoom_lo;       /* 5D3,700D: lower word of zoom; unused? */
    uint16_t zoom;          /* 5D3,700D: 1, 5 or 10 */
    uint32_t unk_14;        /* 5D3: unused? */
    uint32_t DZ;            /* 5D3: unused? 700D: 2=1080crop */
    uint32_t unk_1c;
    uint32_t unk_20;
    uint32_t CF;            /* 100D, 80D: ? */
    uint32_t SV;            /* 100D, EOSM, 80D: ? */
    uint32_t unk_2c;
    uint32_t unk_30;
    uint32_t unk_34;
    uint32_t unk_38;
    uint32_t unk_3c;
    uint32_t unk_40;
    uint32_t DT;            /* 100D, EOSM: ? */
} * PathDriveMode = 0;

enum fps_mode {
    FPS_60 = 0,
    FPS_50 = 1,
    FPS_30 = 2,
    FPS_25 = 3,
    FPS_24 = 4,
};

static int is_1080p()
{
    /* properties triggered too late */
    if (PathDriveMode->zoom != 1)
    {
        return 0;
    }

    /* unsure whether fast enough or not; to be tested */
    if (PathDriveMode->DZ)
    {
        return 0;
    }

    /* this snippet seems OK with properties */
    /* note: on 5D2 and 5D3 (maybe also 6D, not sure),
     * sensor configuration in photo mode is identical to 1080p.
     * other cameras may be different */
    return !is_movie_mode() || PathDriveMode->resolution_idx == 0;
}

static int is_720p()
{
    /* properties triggered too late */
    if (PathDriveMode->zoom != 1)
    {
        return 0;
    }

    /* unsure whether fast enough or not; to be tested */
    if (PathDriveMode->DZ)
    {
        return 0;
    }

    if (is_EOSM && !RECORDING_H264)
    {
        /* EOS M stays in 720p30 during standby */
        return 1;
    }

    /* this snippet seems OK with properties */
    return is_movie_mode() && PathDriveMode->resolution_idx == 1;
}

static int is_supported_mode()
{
    if (!lv) return 0;

    if (0)
    {
        printf(
            "Path: SM=%d S=%d res=%d DZ=%d zoom=%d mode=%d\n", 
            PathDriveMode->SM, PathDriveMode->S, PathDriveMode->resolution_idx,
            PathDriveMode->DZ, PathDriveMode->zoom, PathDriveMode->fps_mode
        );
    }
    
    /* 650D / 700D / EOSM/M2 / 100D prests will only work in x5 mode, don't patch x1 */
    if (PathDriveMode->zoom == 1)
    {
        // FIXME: 100D will crash if added, probably something related to engio_hook?
        if (is_650D || is_700D || is_EOSM) 
        {
            return 0;
        }
    }

    if (PathDriveMode->zoom == 10)
    {
        /* leave the x10 zoom unaltered, for focusing */
        return 0; 
    }

    if (PathDriveMode->zoom == 5)
    {
        if (is_5D3)
        {
            if (crop_preset != CROP_PRESET_CENTER_Z)
            {
                return 0;
            }
        }
        
        if (is_basic)
        {
            return 0;
        }
    }

    return 1;
}

static int32_t  target_yres = 0;
static int32_t  delta_adtg0 = 0;
static int32_t  delta_adtg1 = 0;
static int32_t  delta_head3 = 0;
static int32_t  delta_head4 = 0;
static uint32_t cmos1_lo = 0, cmos1_hi = 0;
static uint32_t cmos2 = 0;

/* helper to allow indexing various properties of Canon's video modes */
static inline int get_video_mode_index()
{
    if (lv_dispsize > 1)
    {
        if (PathDriveMode->zoom == 5)
        {
            return 5;
        }
    }

    return
        (video_mode_fps == 24) ?  0 :
        (video_mode_fps == 25) ?  1 :
        (video_mode_fps == 30) ?  2 :
        (video_mode_fps == 50) ?  3 :
     /* (video_mode_fps == 60) */ 4 ;
}

/* optical black area sizes */
/* not sure how to adjust them from registers, so... hardcode them here */
static inline void FAST calc_skip_offsets(int * p_skip_left, int * p_skip_right, int * p_skip_top, int * p_skip_bottom)
{
    /* start from LiveView values */
    int skip_left       = 146;
    int skip_right      = 2;
    int skip_top        = 28;
    int skip_bottom     = 0;

    switch (crop_preset)
    {
        case CROP_PRESET_FULLRES_LV:
            /* photo mode values */
            skip_left       = 138;
            skip_right      = 2;
            skip_top        = 60;   /* fixme: this is different, why? */
            break;

        case CROP_PRESET_3K:
        case CROP_PRESET_UHD:
        case CROP_PRESET_4K_HFPS:
            skip_right      = 0;    /* required for 3840 - tight fit */
            /* fall-through */
        
        case CROP_PRESET_3X_TALL:
            skip_top        = 30;
            break;

        case CROP_PRESET_3X:
        case CROP_PRESET_1x3:
            skip_top        = 60;
            break;

        case CROP_PRESET_3x3_1X:
        case CROP_PRESET_3x3_1X_48p:
            if (is_720p()) skip_top = 0;
            break;
    }

    if (p_skip_left)   *p_skip_left    = skip_left;
    if (p_skip_right)  *p_skip_right   = skip_right;
    if (p_skip_top)    *p_skip_top     = skip_top;
    if (p_skip_bottom) *p_skip_bottom  = skip_bottom;
}

/* to be in sync with 0xC0F06800 */
static int get_top_bar_adjustment()
{
    switch (crop_preset)
    {
        case CROP_PRESET_FULLRES_LV:
            return 0;                   /* 0x10018: photo mode value, unchanged */
        case CROP_PRESET_3x3_1X:
        case CROP_PRESET_3x3_1X_48p:
            if (is_720p()) return 28;   /* 0x1D0017 from 0x10017 */
            /* fall through */
        default:
            return 30;                  /* 0x1F0017 from 0x10017 */
    }
}

/* Vertical resolution from current unmodified video mode */
/* (active area only, as seen by mlv_lite) */
static inline int get_default_yres()
{
    return 
        (video_mode_fps <= 30) ? 1290 : 672;
}

/* skip_top from unmodified video mode (raw.c, LiveView skip offsets) */
static inline int get_default_skip_top()
{
    return 
        (video_mode_fps <= 30) ? 28 : 20;
}

/* max resolution for each video mode (trial and error) */
/* it's usually possible to push the numbers a few pixels further,
 * at the risk of corrupted frames */
static int max_resolutions[NUM_CROP_PRESETS][6] = {
                                /*   24p   25p   30p   50p   60p   x5 */
    [CROP_PRESET_3X_TALL]       = { 1920, 1728, 1536,  960,  800, 1320 },
    [CROP_PRESET_3x3_1X]        = { 1290, 1290, 1290,  960,  800, 1320 },
    [CROP_PRESET_3x3_1X_48p]    = { 1290, 1290, 1290, 1080, 1040, 1320 }, /* 1080p45/48 */
    [CROP_PRESET_3K]            = { 1920, 1728, 1504,  760,  680, 1320 },
    [CROP_PRESET_UHD]           = { 1536, 1472, 1120,  640,  540, 1320 },
    [CROP_PRESET_4K_HFPS]       = { 3072, 3072, 2500, 1440, 1200, 1320 },
    [CROP_PRESET_FULLRES_LV]    = { 3870, 3870, 3870, 3870, 3870, 1320 },
};

/* 5D3 vertical resolution increments over default configuration */
/* note that first scanline may be moved down by 30 px (see reg_override_top_bar) */
static inline int FAST calc_yres_delta()
{
    int desired_yres = (target_yres) ? target_yres
        : max_resolutions[crop_preset][get_video_mode_index()];

    if (desired_yres)
    {
        /* user override */
        int skip_top;
        calc_skip_offsets(0, 0, &skip_top, 0);
        int default_yres = get_default_yres();
        int default_skip_top = get_default_skip_top();
        int top_adj = get_top_bar_adjustment();
        return desired_yres - default_yres + skip_top - default_skip_top + top_adj;
    }

    ASSERT(0);
    return 0;
}

#define YRES_DELTA calc_yres_delta()


static int cmos_vidmode_ok = 0;

/* return value:
 *  1: registers checked and appear OK (1080p/720p video mode)
 *  0: registers checked and they are not OK (other video mode)
 * -1: registers not checked
 */
static int FAST check_cmos_vidmode(uint16_t* data_buf)
{
    int ok = 1;
    int found = 1;
    while (*data_buf != 0xFFFF)
    {
        int reg = (*data_buf) >> 12;
        int value = (*data_buf) & 0xFFF;
        
        if (is_5D3)
        {
            if (reg == 1)
            {
                found = 1;

                switch (crop_preset)
                {
                    case CROP_PRESET_CENTER_Z:
                    {
                        /* detecting the zoom mode is tricky;
                         * let's just exclude 1080p and 720p for now ... */
                        if (value == 0x800 ||
                            value == 0xBC2)
                        {
                            ok = 0;
                        }
                        break;
                    }

                    default:
                    {
                        if (value != 0x800 &&   /* not 1080p? */
                            value != 0xBC2)     /* not 720p? */
                        {
                            ok = 0;
                        }
                        break;
                    }
                }
            }
        }
        
        if (is_basic && !is_6D)
        {
            if (reg == 7)
            {
                found = 1;
                /* prevent running in 600D hack crop mode */
                if (value != 0x800) 
                {
                    ok = 0;
                }
            }
        }
        
        data_buf++;
    }
    
    if (found) return ok;
    
    return -1;
}

/* pack two 6-bit values into a 12-bit one */
#define PACK12(lo,hi) ((((lo) & 0x3F) | ((hi) << 6)) & 0xFFF)

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

static void FAST cmos_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    /* make sure we are in 1080p/720p mode */
    if (!is_supported_mode())
    {
        /* looks like checking properties works fine for detecting
         * changes in video mode, but not for detecting the zoom change */
        return;
    }
    
    /* also check CMOS registers; in zoom mode, we get different values
     * and this check is instant (no delays).
     * 
     * on 5D3, the 640x480 acts like 1080p during standby,
     * so properties are our only option for that one.
     */
     
    uint16_t* data_buf = (uint16_t*) regs[0];
    int ret = check_cmos_vidmode(data_buf);
    
    if (ret >= 0)
    {
        cmos_vidmode_ok = ret;
    }
    
    if (ret != 1)
    {
        return;
    }

    int cmos_new[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    
    if (is_5D3)
    {
        switch (crop_preset)
        {
            /* 1:1 (3x) */
            case CROP_PRESET_3X:
                /* start/stop scanning line, very large increments */
                /* note: these are two values, 6 bit each, trial and error */
                cmos_new[1] = (is_720p())
                    ? PACK12(13,10)     /* 720p,  almost centered */
                    : PACK12(11,11);    /* 1080p, almost centered */
                
                cmos_new[2] = 0x10E;    /* read every column, centered crop */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;
            
            case CROP_PRESET_3X_TALL:
                cmos_new[1] =           /* vertical centering (trial and error) */
                    (video_mode_fps == 24) ? PACK12(8,13)  :
                    (video_mode_fps == 25) ? PACK12(8,12)  :
                    (video_mode_fps == 30) ? PACK12(9,11)  :
                    (video_mode_fps == 50) ? PACK12(12,10) :
                    (video_mode_fps == 60) ? PACK12(13,10) :
                                             (uint32_t) -1 ;
                cmos_new[2] = 0x10E;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;

            /* 3x3 binning in 720p */
            /* 1080p it's already 3x3, don't change it */
            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_48p:
                if (is_720p())
                {
                    /* start/stop scanning line, very large increments */
                    cmos_new[1] =
                        (crop_preset == CROP_PRESET_3x3_1X_48p) ? PACK12(3,15) :
                        (video_mode_fps == 50)                  ? PACK12(4,14) :
                        (video_mode_fps == 60)                  ? PACK12(6,14) :
                                                                 (uint32_t) -1 ;
                    cmos_new[6] = 0x370;    /* pink highlights without this */
                }
                break;

            case CROP_PRESET_3K:
                cmos_new[1] =           /* vertical centering (trial and error) */
                    (video_mode_fps == 24) ? PACK12(8,12)  :
                    (video_mode_fps == 25) ? PACK12(8,12)  :
                    (video_mode_fps == 30) ? PACK12(9,11)  :
                    (video_mode_fps == 50) ? PACK12(13,10) :
                    (video_mode_fps == 60) ? PACK12(14,10) :    /* 13,10 has better centering, but overflows */
                                             (uint32_t) -1 ;
                cmos_new[2] = 0x0BE;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;

            case CROP_PRESET_UHD:
                cmos_new[1] =
                    (video_mode_fps == 24) ? PACK12(4,9)  :
                    (video_mode_fps == 25) ? PACK12(4,9)  :
                    (video_mode_fps == 30) ? PACK12(5,8)  :
                    (video_mode_fps == 50) ? PACK12(12,9) :
                    (video_mode_fps == 60) ? PACK12(13,9) :
                                            (uint32_t) -1 ;
                cmos_new[2] = 0x08E;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;

            case CROP_PRESET_4K_HFPS:
                cmos_new[1] =
                    (video_mode_fps == 24) ? PACK12(4,15)  :
                    (video_mode_fps == 25) ? PACK12(4,15)  :
                    (video_mode_fps == 30) ? PACK12(6,14)  :
                    (video_mode_fps == 50) ? PACK12(10,11) :
                    (video_mode_fps == 60) ? PACK12(12,11) :
                                             (uint32_t) -1 ;
                cmos_new[2] = 0x07E;    /* horizontal centering (trial and error) */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;

            case CROP_PRESET_FULLRES_LV:
                cmos_new[1] = 0x800;    /* from photo mode */
                cmos_new[2] = 0x00E;    /* 8 in photo mode; E enables shutter speed control from ADTG 805E */
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;

            /* 1x3 binning (read every line, bin every 3 columns) */
            case CROP_PRESET_1x3:
                /* start/stop scanning line, very large increments */
                cmos_new[1] = (is_720p())
                    ? PACK12(14,10)     /* 720p,  almost centered */
                    : PACK12(11,11);    /* 1080p, almost centered */
                
                cmos_new[6] = 0x170;    /* pink highlights without this */
                break;

            /* 3x1 binning (bin every 3 lines, read every column) */
            case CROP_PRESET_3x1:
                cmos_new[2] = 0x10E;    /* read every column, centered crop */
                break;

            /* raw buffer centered in zoom mode */
            case CROP_PRESET_CENTER_Z:
                cmos_new[1] = PACK12(9+2,42+1); /* vertical (first|last) */
                cmos_new[2] = 0x09E;            /* horizontal offset (mask 0xFF0) */
                break;
        }
    }

    if (is_basic)
    {
        switch (crop_preset)
        {
            case CROP_PRESET_3x3_1X:
            if (is_720p())
            {
                /* start/stop scanning line, very large increments */
                cmos_new[7] = (is_6D) ? PACK12(37,10) : PACK12(6,29);
            }
            break; 
        }
    }
    
    
    // 650D / 700D / EOSM/M2 / 100D presets
    // cmos_new[5] used for vertical offset, cmos_new[7] for horizontal offset
    if (is_DIGIC_5)
    {
        switch (crop_preset)
        {
            case CROP_PRESET_1X1:
            if (!is_720p() || !is_1080p())
            {
                if (CROP_2_5K)
                {
                    if (is_650D || is_700D || is_100D)
                    {
                        cmos_new[5] = 0x2C0;
                        cmos_new[7] = 0xA6A;
                    }
                }
                
                if (CROP_1440p)
                {
                    if (is_650D || is_700D || is_100D)
                    {
                        cmos_new[5] = 0x2C0;
                        cmos_new[7] = 0xAA9;
                    }
                }
                
                if (CROP_3K)
                {
                    if (is_650D || is_700D || is_100D)
                    {
                        cmos_new[5] = 0x240;
                        cmos_new[7] = 0xAA9;
                    }
                }
                
                if (CROP_Full_Res)
                {
                    if (is_650D || is_700D || is_100D)
                    {
                        cmos_new[5] = 0x0;
                        cmos_new[7] = 0x3A0;
                    }
                }
            }
            break;
            
            case CROP_PRESET_1X3:
            if (AR_2_35_1)
            {
                if (Anam_Highest)
                {
                    cmos_new[5] = 0x20;
                    cmos_new[7] = 0x305;
                }
            }
            break; 
        }
    }

    /* menu overrides */
    if (cmos1_lo || cmos1_hi)
    {
        cmos_new[1] = PACK12(cmos1_lo,cmos1_hi);
    }

    if (cmos2)
    {
        cmos_new[2] = cmos2;
    }
    
    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks, values are back to normal */
    static uint16_t copy[512];
    uint16_t* copy_end = &copy[COUNT(copy)];
    uint16_t* copy_ptr = copy;

    while (*data_buf != 0xFFFF)
    {
        *copy_ptr = *data_buf;

        int reg = (*data_buf) >> 12;
        if (cmos_new[reg] != -1)
        {
            *copy_ptr = (reg << 12) | cmos_new[reg];
            dbg_printf("CMOS[%x] = %x\n", reg, cmos_new[reg]);
        }

        data_buf++;
        copy_ptr++;
        if (copy_ptr > copy_end) while(1);
    }
    *copy_ptr = 0xFFFF;

    /* pass our modified register list to cmos_write */
    regs[0] = (uint32_t) copy;
}

static uint32_t nrzi_encode( uint32_t in_val )
{
    uint32_t out_val = 0;
    uint32_t old_bit = 0;
    for (int num = 0; num < 31; num++)
    {
        uint32_t bit = in_val & 1<<(30-num) ? 1 : 0;
        if (bit != old_bit)
            out_val |= (1 << (30-num));
        old_bit = bit;
    }
    return out_val;
}

static uint32_t nrzi_decode( uint32_t in_val )
{
    uint32_t val = 0;
    if (in_val & 0x8000)
        val |= 0x8000;
    for (int num = 0; num < 31; num++)
    {
        uint32_t old_bit = (val & 1<<(30-num+1)) >> 1;
        val |= old_bit ^ (in_val & 1<<(30-num));
    }
    return val;
}

static int FAST adtg_lookup(uint32_t* data_buf, int reg_needle)
{
    while(*data_buf != 0xFFFFFFFF)
    {
        int reg = (*data_buf) >> 16;
        if (reg == reg_needle)
        {
            return *(uint16_t*)data_buf;
        }
    }
    return -1;
}

/* adapted from fps_override_shutter_blanking in fps-engio.c */
static int adjust_shutter_blanking(int old)
{
    /* sensor duty cycle: range 0 ... timer B */
    int current_blanking = nrzi_decode(old);
    
    static int previous_blanking = -1;
    
    if (ABS(current_blanking - previous_blanking) == 1) 
    {
        current_blanking = previous_blanking;
    } 
    else 
    {
       previous_blanking = current_blanking;
    }

    int video_mode = get_video_mode_index();

    /* what value Canon firmware assumes for timer B? */
    int fps_timer_b_orig = default_timerB[video_mode];

    int current_exposure = fps_timer_b_orig - current_blanking;
    
    /* wrong assumptions? */
    if (current_exposure < 0)
    {
        return old;
    }

    int default_fps = default_fps_1k[video_mode];
    int current_fps = fps_get_current_x1000();

    dbg_printf("FPS %d->%d\n", default_fps, current_fps);

    float frame_duration_orig = 1000.0 / default_fps;
    float frame_duration_current = 1000.0 / current_fps;

    float orig_shutter = frame_duration_orig * current_exposure / fps_timer_b_orig;

    float new_shutter =
        (shutter_range == 0) ?
        ({
            /* original shutter speed from the altered video mode */
            orig_shutter;
        }) :
        ({
            /* map the available range of 1/4000...1/30 (24-30p) or 1/4000...1/60 (50-60p)
             * from minimum allowed (1/15000 with full-res LV) to 1/fps */
            int max_fps_shutter = (video_mode_fps <= 30) ? 33333 : 64000;
            int default_fps_adj = 1e9 / (1e9 / max_fps_shutter - 250);
            (orig_shutter - 250e-6) * default_fps_adj / current_fps;
        });

    /* what value is actually used for timer B? (possibly after our overrides) */
    int fps_timer_b = (shamem_read(0xC0F06014) & 0xFFFF) + 1;

    dbg_printf("Timer B %d->%d\n", fps_timer_b_orig, fps_timer_b);

    int new_exposure = new_shutter * fps_timer_b / frame_duration_current;
    int new_blanking = COERCE(fps_timer_b - new_exposure, 10, fps_timer_b - 2);

    dbg_printf("Exposure %d->%d (timer B units)\n", current_exposure, new_exposure);

#ifdef CROP_DEBUG
    float chk_shutter = frame_duration_current * new_exposure / fps_timer_b;
    dbg_printf("Shutter %d->%d us\n", (int)(orig_shutter*1e6), (int)(chk_shutter*1e6));
#endif

    dbg_printf("Blanking %d->%d\n", current_blanking, new_blanking);

    return nrzi_encode(new_blanking);
}

extern void fps_override_shutter_blanking();

static void FAST adtg_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
        
       if (is_5D3 || is_basic)
       {
           if (!cmos_vidmode_ok)
           {
               return;
           }
       }
    }

    if (!is_720p())
    {
        if (crop_preset == CROP_PRESET_3x3_1X ||
            crop_preset == CROP_PRESET_3x3_1X_48p)
        {
            /* these presets only have effect in 720p mode */
            return;
        }
    }

    /* This hook is called from the DebugMsg's in adtg_write,
     * so if we change the register list address, it won't be able to override them.
     * Workaround: let's call it here. */
    fps_override_shutter_blanking();

    uint32_t cs = regs[0];
    uint32_t *data_buf = (uint32_t *) regs[1];
    int dst = cs & 0xF;
    
    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks, values are back to normal */
    static uint32_t copy[512];
    uint32_t* copy_end = &copy[COUNT(copy)];
    uint32_t* copy_ptr = copy;
    
    struct adtg_new
    {
        int dst;
        int reg;
        int val;
    };
    
    /* expand this as required */
    struct adtg_new adtg_new[24] = {{0}};

    /* scan for shutter blanking and make both zoom and non-zoom value equal */
    /* (the values are different when using FPS override with ADTG shutter override) */
    /* (fixme: might be better to handle this in ML core?) */
    /* also scan for one ADTG gain register and get its value to be used for lower bit-depth */
    /* this is similair method to lowering digital gain to get "fake" lossless compression 
       in lower bit-depths, digital gain method only works with native RAW resolutions       */
    /* we will use [ADTG2/4] 8882/8884/8886/8888 registers, they change values slightly with ISO changes */
    int shutter_blanking = 0;
    int analog_gain = 0;
    
    const int blanking_reg_zoom   = (is_5D3) ? 0x805E : 0x805F;
    const int blanking_reg_nozoom = (is_5D3) ? 0x8060 : 0x8061;
    const int blanking_reg        = (lv_dispsize == 1) ? blanking_reg_nozoom : blanking_reg_zoom;
    
    int adtg_analog_gain_reg = 0x8882;
    for (uint32_t * buf = data_buf; *buf != 0xFFFFFFFF; buf++)
    {
        int reg = (*buf) >> 16;
        if (reg == blanking_reg)
        {
            int val = (*buf) & 0xFFFF;
            shutter_blanking = val;
        }
        if (reg == adtg_analog_gain_reg)
        {
            int val = (*buf) & 0xFFFF;
            analog_gain = val;
        }
    }

    /* some modes may need adjustments to maintain exposure */
    if (shutter_blanking)
    {
        /* FIXME: remove this kind of hardcoded conditions */
        if ((crop_preset == CROP_PRESET_CENTER_Z && lv_dispsize != 1) ||
            (crop_preset != CROP_PRESET_CENTER_Z && lv_dispsize == 1) || is_DIGIC_5)
        {
            shutter_blanking = adjust_shutter_blanking(shutter_blanking);
        }
    }

    /* all modes may want to override shutter speed */
    /* ADTG[0x8060]: shutter blanking for 3x3 mode  */
    /* ADTG[0x805E]: shutter blanking for zoom mode  */
    adtg_new[0] = (struct adtg_new) {6, blanking_reg_nozoom, shutter_blanking};
    adtg_new[1] = (struct adtg_new) {6, blanking_reg_zoom, shutter_blanking};   

    /* hopefully generic; to be tested later */
    if (1)
    {
        switch (crop_preset)
        {
            /* all 1:1 modes (3x, 3K, 4K...) */
            case CROP_PRESET_3X:
            case CROP_PRESET_3X_TALL:
            case CROP_PRESET_3K:
            case CROP_PRESET_UHD:
            case CROP_PRESET_4K_HFPS:
            case CROP_PRESET_FULLRES_LV:
                /* ADTG2/4[0x8000] = 5 (set in one call) */
                /* ADTG2[0x8806] = 0x6088 on 5D3 (artifacts without it) */
                adtg_new[2] = (struct adtg_new) {6, 0x8000, 5};
                if (is_5D3) {
                    /* this register is model-specific */
                    adtg_new[3] = (struct adtg_new) {2, 0x8806, 0x6088};
                }
                break;

            /* 3x3 binning in 720p (in 1080p it's already 3x3) */
            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_48p:
                /* ADTG2/4[0x800C] = 2: vertical binning factor = 3 */
                adtg_new[2] = (struct adtg_new) {6, 0x800C, 2};
                break;

            /* 1x3 binning (read every line, bin every 3 columns) */
            case CROP_PRESET_1x3:
                /* ADTG2/4[0x800C] = 0: read every line */
                adtg_new[2] = (struct adtg_new) {6, 0x800C, 0};
                break;

            /* 3x1 binning (bin every 3 lines, read every column) */
            case CROP_PRESET_3x1:
                /* ADTG2/4[0x800C] = 2: vertical binning factor = 3 */
                /* ADTG2[0x8806] = 0x6088 on 5D3 (artifacts worse without it) */
                /* ADTG2[0x8183]/[0x8184] used for horizontal binning, artifacts without it */
                /* FIXME: ADTG2[0x8183]/[0x8184] won't be overridden until we go outside LV 
                          then get back, maybe because Canon only update them once?        */
                adtg_new[2] = (struct adtg_new) {6, 0x800C, 2};
                adtg_new[3] = (struct adtg_new) {2, 0x8183, 0};
                adtg_new[4] = (struct adtg_new) {2, 0x8184, 0};
                if (is_5D3) {
                    /* this register is model-specific */
                    adtg_new[5] = (struct adtg_new) {2, 0x8806, 0x6088};
                }
                break;
        }
        
        // 650D / 700D / EOSM/M2 / 100D presets
        // ADTG2[0x8183] and ADTG2[0x8184] enable horizontal pixel binning instead of skipping
        // in 1080p ADTG2[0x8183] = 0x21, ADTG2[0x8183] = 0x7B, in x5 both are = 0x0 
        // ADTG2[0x800C] = 2: vertical binning/skipping factor = 3, ADTG2[0x800C] = 0 read all vertical lines
        if (is_DIGIC_5)
        {
            switch (crop_preset)
            {
                case CROP_PRESET_1X3:
                if (is_650D || is_700D || is_100D || is_EOSM)
                {
                    adtg_new[2] = (struct adtg_new) {2, 0x800C, 0};
                    adtg_new[3] = (struct adtg_new) {2, 0x8000, 0x6};
                    adtg_new[4] = (struct adtg_new) {2, 0x8183, 0x21};
                    adtg_new[5] = (struct adtg_new) {2, 0x8184, 0x7B};
                    
                    if (is_100D)
                    {
                    /*  Artifacts without these on certain 100D bodies:
                        https://www.magiclantern.fm/forum/index.php?topic=26511.msg239495#msg239495
                        https://www.magiclantern.fm/forum/index.php?topic=26511.msg239557#msg239557  */

                        adtg_new[6]  = (struct adtg_new) {2, 0xc00d, 0x5000};
                        adtg_new[7]  = (struct adtg_new) {2, 0xc00e, 0x53};
                        adtg_new[8]  = (struct adtg_new) {2, 0xc00f, 0x52};
                        adtg_new[9]  = (struct adtg_new) {2, 0xc010, 0x52};
                        adtg_new[10] = (struct adtg_new) {2, 0xc011, 0x52};
                    }
                }
                break;
                
                case CROP_PRESET_3X3:
                if (is_650D || is_700D || is_100D || is_EOSM)
                {
                    adtg_new[2] = (struct adtg_new) {2, 0x800C, 0x2};
                    adtg_new[3] = (struct adtg_new) {2, 0x8000, 0x6};
                    adtg_new[4] = (struct adtg_new) {2, 0x8183, 0x21};
                    adtg_new[5] = (struct adtg_new) {2, 0x8184, 0x7B};
                }
                break; 
            }
        }

        /* PowerSaveTiming & ReadOutTiming registers */
        /* these need changing in all modes with higher vertical resolution */
        switch (crop_preset)
        {
            case CROP_PRESET_3X_TALL:
            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_48p:
            case CROP_PRESET_3K:
            case CROP_PRESET_UHD:
            case CROP_PRESET_4K_HFPS:
            case CROP_PRESET_FULLRES_LV:
            case CROP_PRESET_40_FPS:
            case CROP_PRESET_1X1:
            case CROP_PRESET_1X3:
            case CROP_PRESET_3X3:
            {
                /* assuming FPS timer B was overridden before this */
                int fps_timer_b = (shamem_read(0xC0F06014) & 0xFFFF) + 1;
                int readout_end = shamem_read(0xC0F06804) >> 16;    /* fixme: D5 only */

                /* PowerSaveTiming registers */
                /* after readout is finished, we can turn off the sensor until the next frame */
                /* we could also set these to 0; it will work, but the sensor will run a bit hotter */
                /* to be tested to find out exactly how much */
                adtg_new[11] = (struct adtg_new) {6, 0x8172, nrzi_encode(readout_end + 1) }; /* PowerSaveTiming ON (6D/700D) */
                adtg_new[12] = (struct adtg_new) {6, 0x8178, nrzi_encode(readout_end + 1) }; /* PowerSaveTiming ON (5D3/6D/700D) */
                adtg_new[13] = (struct adtg_new) {6, 0x8196, nrzi_encode(readout_end + 1) }; /* PowerSaveTiming ON (5D3) */

                adtg_new[14] = (struct adtg_new) {6, 0x8173, nrzi_encode(fps_timer_b - 1) }; /* PowerSaveTiming OFF (6D/700D) */
                adtg_new[15] = (struct adtg_new) {6, 0x8179, nrzi_encode(fps_timer_b - 1) }; /* PowerSaveTiming OFF (5D3/6D/700D) */
                adtg_new[16] = (struct adtg_new) {6, 0x8197, nrzi_encode(fps_timer_b - 1) }; /* PowerSaveTiming OFF (5D3) */

                adtg_new[17] = (struct adtg_new) {6, 0x82B6, nrzi_encode(readout_end - 1) }; /* PowerSaveTiming ON? (700D); 2 units below the "ON" timing from above */

                /* ReadOutTiming registers */
                /* these shouldn't be 0, as they affect the image */
                adtg_new[18] = (struct adtg_new) {6, 0x82F8, nrzi_encode(readout_end + 1) }; /* ReadOutTiming */
                adtg_new[19] = (struct adtg_new) {6, 0x82F9, nrzi_encode(fps_timer_b - 1) }; /* ReadOutTiming end? */
                break;
            }
        }
    }
    
    /* divid signal to achieve lower bit-depths using negative analog gain */
    if (which_output_format() >= 3) // don't patch if uncompressed RAW is selected
    {
        if (OUTPUT_12BIT)
        {
            adtg_new[20] = (struct adtg_new) {6, 0x8882, analog_gain / 4};
            adtg_new[21] = (struct adtg_new) {6, 0x8884, analog_gain / 4};
            adtg_new[22] = (struct adtg_new) {6, 0x8886, analog_gain / 4};
            adtg_new[23] = (struct adtg_new) {6, 0x8888, analog_gain / 4};
        }
    
        if (OUTPUT_11BIT)
        {
            adtg_new[20] = (struct adtg_new) {6, 0x8882, analog_gain / 8};
            adtg_new[21] = (struct adtg_new) {6, 0x8884, analog_gain / 8};
            adtg_new[22] = (struct adtg_new) {6, 0x8886, analog_gain / 8};
            adtg_new[23] = (struct adtg_new) {6, 0x8888, analog_gain / 8};
        }

        if (OUTPUT_10BIT)
        {
            adtg_new[20] = (struct adtg_new) {6, 0x8882, analog_gain / 16};
            adtg_new[21] = (struct adtg_new) {6, 0x8884, analog_gain / 16};
            adtg_new[22] = (struct adtg_new) {6, 0x8886, analog_gain / 16};
            adtg_new[23] = (struct adtg_new) {6, 0x8888, analog_gain / 16};
        } 
    }

    while(*data_buf != 0xFFFFFFFF)
    {
        *copy_ptr = *data_buf;
        int reg = (*data_buf) >> 16;
        for (int i = 0; i < COUNT(adtg_new); i++)
        {
            if ((reg == adtg_new[i].reg) && (dst & adtg_new[i].dst))
            {
                int new_value = adtg_new[i].val;
                dbg_printf("ADTG%x[%x] = %x\n", dst, reg, new_value);
                *(uint16_t*)copy_ptr = new_value;

                if (reg == blanking_reg_zoom || reg == blanking_reg_nozoom)
                {
                    /* also override in original data structure */
                    /* to be picked up on the screen indicators */
                    *(uint16_t*)data_buf = new_value;
                }
            }
        }
        data_buf++;
        copy_ptr++;
        if (copy_ptr >= copy_end) while(1);
    }
    *copy_ptr = 0xFFFFFFFF;
    
    /* pass our modified register list to adtg_write */
    regs[1] = (uint32_t) copy;
}

int analog_gain_is_acive()
{
    if (CROP_PRESET_MENU) // analog gain is only active when preset is selected
    {
        if (bit_depth_analog == 1)
        {
            return 1;
        }
    
        if (bit_depth_analog == 2)
        {
            return 2;
        }
    
        if (bit_depth_analog == 3)
        {
            return 3;
        }
    }
   
    return 0;
}

int crop_rec_is_enabled()
{
    if (CROP_PRESET_MENU)
    {
        return 1;
    }
    
    return 0;
}

/* this is used to cover the black bar at the top of the image in 1:1 modes */
/* (used in most other presets) */
static inline uint32_t reg_override_top_bar(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
        /* raw start line/column */
        /* move start line down by 30 pixels */
        /* not sure where this offset comes from */
        case 0xC0F06800:
            return 0x1F0017;
    }

    return 0;
}

/* these are required for increasing vertical resolution */
/* (used in most other presets) */
static inline uint32_t reg_override_HEAD34(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
        /* HEAD3 timer */
        case 0xC0F0713C:
            return old_val + YRES_DELTA + delta_head3;

        /* HEAD4 timer */
        case 0xC0F07150:
            return old_val + YRES_DELTA + delta_head4;
    }

    return 0;
}

static inline uint32_t reg_override_common(uint32_t reg, uint32_t old_val)
{
    uint32_t a = reg_override_top_bar(reg, old_val);
    if (a) return a;

    uint32_t b = reg_override_HEAD34(reg, old_val);
    if (b) return b;

    return 0;
}

static inline uint32_t reg_override_fps(uint32_t reg, uint32_t timerA, uint32_t timerB, uint32_t old_val)
{
    /* hardware register requires timer-1 */
    timerA--;
    timerB--;

    /* only override FPS registers if the old value is what we expect
     * otherwise we may be in some different video mode for a short time
     * this race condition is enough to lock up LiveView in some cases
     * e.g. 5D3 3x3 50/60p when going from photo mode to video mode
     */

    switch (reg)
    {
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        case 0xC0F06010:
        {
            uint32_t expected = default_timerA[get_video_mode_index()] - 1;

            if (old_val == expected || old_val == expected + 1)
            {
                return timerA;
            }

            break;
        }
        
        case 0xC0F06008:
        case 0xC0F0600C:
        {
            uint32_t expected = default_timerA[get_video_mode_index()] - 1;
            expected |= (expected << 16);

            if (old_val == expected || old_val == expected + 0x00010001)
            {
                return timerA | (timerA << 16);
            }

            break;
        }

        case 0xC0F06014:
        {
            uint32_t expected = default_timerB[get_video_mode_index()] - 1;

            if (old_val == expected || old_val == expected + 1)
            {
                return timerB;
            }

            break;
        }
    }

    return 0;
}

static inline uint32_t reg_override_3X_tall(uint32_t reg, uint32_t old_val)
{
    /* change FPS timers to increase vertical resolution */
    if (video_mode_fps >= 50)
    {
        int timerA = 400;

        int timerB =
            (video_mode_fps == 50) ? 1200 :
            (video_mode_fps == 60) ? 1001 :
                                       -1 ;

        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }

    /* fine-tuning head timers appears to help
     * pushing the resolution a tiny bit further */
    int head_adj =
        (video_mode_fps == 50) ? -30 :
        (video_mode_fps == 60) ? -20 :
                                   0 ;

    switch (reg)
    {
        /* raw resolution (end line/column) */
        case 0xC0F06804:
            return old_val + (YRES_DELTA << 16);

        /* HEAD3 timer */
        case 0xC0F0713C:
            return old_val + YRES_DELTA + delta_head3 + head_adj;

        /* HEAD4 timer */
        case 0xC0F07150:
            return old_val + YRES_DELTA + delta_head4 + head_adj;
    }

    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3x3_tall(uint32_t reg, uint32_t old_val)
{
    if (!is_720p() || !is_5D3)
    {
        /* 1080p not patched in 3x3 */
        return 0;
    }

    /* change FPS timers to increase vertical resolution */
    if (video_mode_fps >= 50)
    {
        int timerA = 400;

        int timerB =
            (video_mode_fps == 50) ? 1200 :
            (video_mode_fps == 60) ? 1001 :
                                       -1 ;

        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }

    /* fine-tuning head timers appears to help
     * pushing the resolution a tiny bit further */
    int head_adj =
        (video_mode_fps == 50) ? -10 :
        (video_mode_fps == 60) ? -20 :
                                   0 ;

    switch (reg)
    {
        /* for some reason, top bar disappears with the common overrides */
        /* very tight fit - every pixel counts here */
        case 0xC0F06800:
            return 0x1D0017;

        /* raw resolution (end line/column) */
        case 0xC0F06804:
            return old_val + (YRES_DELTA << 16);

        /* HEAD3 timer */
        case 0xC0F0713C:
            return old_val + YRES_DELTA + delta_head3 + head_adj;

        /* HEAD4 timer */
        case 0xC0F07150:
            return old_val + YRES_DELTA + delta_head4 + head_adj;
    }

    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3x3_48p(uint32_t reg, uint32_t old_val)
{
    if (!is_720p())
    {
        /* 1080p not patched in 3x3 */
        return 0;
    }

    /* change FPS timers to increase vertical resolution */
    if (video_mode_fps >= 50)
    {
        int timerA =
            (video_mode_fps == 50) ? 401 :
            (video_mode_fps == 60) ? 400 :
                                      -1 ;
        int timerB =
            (video_mode_fps == 50) ? 1330 : /* 45p */
            (video_mode_fps == 60) ? 1250 : /* 48p */
                                       -1 ;

        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }

    switch (reg)
    {
        /* for some reason, top bar disappears with the common overrides */
        /* very tight fit - every pixel counts here */
        case 0xC0F06800:
            return 0x1D0017;

        /* raw resolution (end line/column) */
        case 0xC0F06804:
            return old_val + (YRES_DELTA << 16);

        /* HEAD3 timer */
        /* 2E6 in 50p, 2B4 in 60p */
        case 0xC0F0713C:
            return 0x2B4 + YRES_DELTA + delta_head3;

        /* HEAD4 timer */
        /* 2B4 in 50p, 26D in 60p */
        case 0xC0F07150:
            return 0x26D + YRES_DELTA + delta_head4;
    }

    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_3K(uint32_t reg, uint32_t old_val)
{
    /* FPS timer A, for increasing horizontal resolution */
    /* 25p uses 480 (OK), 24p uses 440 (too small); */
    /* only override in 24p, 30p and 60p modes */
    if (video_mode_fps != 25 && video_mode_fps !=  50)
    {
        int timerA = 455;
        int timerB =
            (video_mode_fps == 24) ? 2200 :
            (video_mode_fps == 30) ? 1760 :
            (video_mode_fps == 60) ?  880 :
                                       -1 ;

        int a = reg_override_fps(reg, timerA, timerB, old_val);
        if (a) return a;
    }

    switch (reg)
    {
        /* raw resolution (end line/column) */
        /* X: (3072+140)/8 + 0x17, adjusted for 3072 in raw_rec */
        case 0xC0F06804:
            return (old_val & 0xFFFF0000) + 0x1AA + (YRES_DELTA << 16);

    }

    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_4K_hfps(uint32_t reg, uint32_t old_val)
{
    /* FPS timer A, for increasing horizontal resolution */
    /* trial and error to allow 4096; 572 is too low, 576 looks fine */
    /* pick some values with small roundoff error */
    int timerA =
        (video_mode_fps < 30)  ?  585 : /* for 23.976/2 and 25/2 fps */
                                  579 ; /* for all others */

    /* FPS timer B, tuned to get half of the frame rate from Canon menu */
    int timerB =
        (video_mode_fps == 24) ? 3422 :
        (video_mode_fps == 25) ? 3282 :
        (video_mode_fps == 30) ? 2766 :
        (video_mode_fps == 50) ? 1658 :
        (video_mode_fps == 60) ? 1383 :
                                   -1 ;

    int a = reg_override_fps(reg, timerA, timerB, old_val);
    if (a) return a;

    switch (reg)
    {
        /* raw resolution (end line/column) */
        /* X: (4096+140)/8 + 0x18, adjusted for 4096 in raw_rec */
        case 0xC0F06804:
            return (old_val & 0xFFFF0000) + 0x22A + (YRES_DELTA << 16);
    }

    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_UHD(uint32_t reg, uint32_t old_val)
{
    /* FPS timer A, for increasing horizontal resolution */
    /* trial and error to allow 3840; 536 is too low */
    int timerA = 
        (video_mode_fps == 25) ? 547 :
        (video_mode_fps == 50) ? 546 :
                                 550 ;
    int timerB =
        (video_mode_fps == 24) ? 1820 :
        (video_mode_fps == 25) ? 1755 :
        (video_mode_fps == 30) ? 1456 :
        (video_mode_fps == 50) ?  879 :
        (video_mode_fps == 60) ?  728 :
                                   -1 ;

    int a = reg_override_fps(reg, timerA, timerB, old_val);
    if (a) return a;

    switch (reg)
    {
        /* raw resolution (end line/column) */
        /* X: (3840+140)/8 + 0x18, adjusted for 3840 in raw_rec */
        case 0xC0F06804:
            return (old_val & 0xFFFF0000) + 0x20A + (YRES_DELTA << 16);
    }

    return reg_override_common(reg, old_val);
}

static inline uint32_t reg_override_fullres_lv(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
        case 0xC0F06800:
            return 0x10018;         /* raw start line/column, from photo mode */
        
        case 0xC0F06804:            /* 1080p 0x528011B, photo 0xF6E02FE */
            return (old_val & 0xFFFF0000) + 0x2FE + (YRES_DELTA << 16);
        
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
            return 0x312;           /* from photo mode */
        
        case 0xC0F06010:            /* FPS timer A, for increasing horizontal resolution */
            return 0x317;           /* from photo mode; lower values give black border on the right */
        
        case 0xC0F06008:
        case 0xC0F0600C:
            return 0x3170317;

        case 0xC0F06014:
            return (video_mode_fps > 30 ? 856 : 1482) + YRES_DELTA;   /* up to 7.4 fps */
    }

    /* no need to adjust the black bar */
    return reg_override_HEAD34(reg, old_val);
}

/* just for testing */
/* (might be useful for FPS override on e.g. 70D) */
static inline uint32_t reg_override_40_fps(uint32_t reg, uint32_t old_val)
{
    switch (reg)
    {
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        case 0xC0F06010:
            return 0x18F;
        
        case 0xC0F06008:
        case 0xC0F0600C:
            return 0x18F018F;

        case 0xC0F06014:
            return 0x5DB;
    }

    return 0;
}

static inline uint32_t reg_override_fps_nocheck(uint32_t reg, uint32_t timerA, uint32_t timerB, uint32_t old_val)
{
    /* hardware register requires timer-1 */
    timerA--;
    timerB--;

    switch (reg)
    {
        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        case 0xC0F06010:
        {
            return timerA;
        }
        
        case 0xC0F06008:
        case 0xC0F0600C:
        {
            return timerA | (timerA << 16);
        }

        case 0xC0F06014:
        {
            return timerB;
        }
    }

    return 0;
}

static inline uint32_t reg_override_zoom_fps(uint32_t reg, uint32_t old_val)
{
    /* attempt to reconfigure the x5 zoom at the FPS selected in Canon menu */
    int timerA = 
        (video_mode_fps == 24) ? 512 :
        (video_mode_fps == 25) ? 512 :
        (video_mode_fps == 30) ? 520 :
        (video_mode_fps == 50) ? 512 :  /* cannot get 50, use 25 */
        (video_mode_fps == 60) ? 520 :  /* cannot get 60, use 30 */
                                  -1 ;
    int timerB =
        (video_mode_fps == 24) ? 1955 :
        (video_mode_fps == 25) ? 1875 :
        (video_mode_fps == 30) ? 1540 :
        (video_mode_fps == 50) ? 1875 :
        (video_mode_fps == 60) ? 1540 :
                                   -1 ;

    return reg_override_fps_nocheck(reg, timerA, timerB, old_val);
}

/* 650D / 700D / EOSM/M2 / 100D reg_override presets */

int preview_debug = 0;

static unsigned TimerB = 0;
static unsigned TimerA = 0;

static unsigned RAW_H = 0;            // RAW width    resolution          0xC0F06804
static unsigned RAW_V = 0;            // RAW vertical resolution          0xC0F06804

static unsigned Preview_Control = 0;

/* used to increase processed RAW data in LiveView, also show new image on screen via stretch regs */
static unsigned Preview_H = 0;        // How much width to process        List of registers
static unsigned Preview_V = 0;        // How much height to process       List of registers
static unsigned Preview_R = 0;        // Preview related                  0xC0F383D4
static unsigned YUV_HD_S_H = 0;       // YUV (HD) horizontal stretch      0xC0F11B8C
static unsigned YUV_HD_S_V = 0;       // YUV (HD) vertical stretch        0xC0F11BCC
static unsigned YUV_HD_S_V_E = 0;     // YUV (HD) enable vertical stretch 0xC0F11BC8

/* used to correct aspect ratio on screen */
static unsigned YUV_LV_S_V = 0;       // YUV (LV) vertical stretch        0xC0F11ACC
static unsigned YUV_LV_Buf = 0;       // YUV (LV) buffer size             0xC0F04210

/* used to exceed preview limits */
//static unsigned EDMAC_24_s = 0;       // EDMAC#24 size                    0xC0F26810
//static unsigned EDMAC_24_address = 0; // EDMAC#24 buffer address          0xC0F26808
static unsigned EDMAC_24_Redirect = 0;  // EDMAC#24 re-driect buffer flag
static unsigned Black_Bar = 0;          // Exceed black bar width limit     0xC0F3B038, 0xC0F3B088

// addresses (part of EDMAC#9 configuration structure) which holds vertical HIV size in x5 mode
// on 700D the structure starts in 0x3e1b4 and ends in 0x3e234, it's being loaded in LVx5_StartPreproPath from ff4f2860
// overriding them are needed to exceed vertical preview limit, on 700D it's RAW V - 1 = 0x453 (in x5 mode)

// 0xC0F08184 = RAW V - 1 = 0x453 (in x5), which is also related somehow to EDMAC#9 vertical size and needs to be tweaked
// also EDMAC_24_Redirect is required before increasing 0xC0F08184 value, otherwise RAW data would be corrupted

// AFAIK EDMAC#9 is used for darkframe subtraction for LiveView, it also sets black and white level values (for LiveView)
static uint32_t EDMAC_9_Vertical_1 = 0;         // 0x453 , it's being set in 0xC0F04910 register which control EDMAC#9 Size B
static uint32_t EDMAC_9_Vertical_2 = 0;         // 0x453 , tweaking it has no effect? let's tweak just in case

static unsigned EDMAC_9_Vertical_Change = 0;    // flag to enable/disable EDMAC#9 tweaks

/* used to center preview and clear VRAM artifacts */
static unsigned preview_shift_value = 0;
static unsigned Shift_Preview = 0;
static unsigned Center_Preview_ON = 0;
static unsigned Clear_Artifacts = 0;
static unsigned Clear_Artifacts_ON = 0;

/* camera-specific ROM addresses */
/* Shift_x5 holds preview shifting value for an output for x5 mode */
static uint32_t Shift_x5_LCD = 0;
static uint32_t Shift_x5_HDMI_480p = 0; // Called VIDEO NTSC 
static uint32_t Shift_x5_HDMI_1080i_Full = 0;
static uint32_t Shift_x5_HDMI_1080i_Info = 0;

/* Clear_Vram_x5 holds preview clear Vram value for an output for x5 mode */
static uint32_t Clear_Vram_x5_LCD = 0;
static uint32_t Clear_Vram_x5_HDMI_480p = 0;
static uint32_t Clear_Vram_x5_HDMI_1080i_Full = 0;
static uint32_t Clear_Vram_x5_HDMI_1080i_Info = 0;

static inline uint32_t reg_override_1X1(uint32_t reg, uint32_t old_val)
{
    if (CROP_2_5K)
    {
        if (is_650D || is_700D || is_EOSM)
        {
            RAW_H         = 0x298;
            RAW_V         = 0x454;
            TimerB        = 0x5D3;
            TimerA        = 0x2CB;
        }

        if (is_100D)
        {
            RAW_H         = 0x2a1;
            RAW_V         = 0x458;
            TimerB        = 0x5B3;
            TimerA        = 0x2DB;
        }

        // Preview_H should be = active RAW width - 4? , 2520 - 4 = 2516 (active RAW width is 2520)
        // otherwise a black bar will appear in the left part of both YUV (HD) and (LV) dumps 
        // e.g. Preview_H = 2520 --> black bar on the left, also will loss some pixel on the right
        Preview_H     = 2516; 
        Preview_V     = 1080;
        Preview_R     = 0x19000D;
        YUV_HD_S_H    = 0x105027D;
        YUV_HD_S_V    = 0x1050195;
        YUV_HD_S_V_E  = 0;
        Black_Bar     = 2;

        YUV_LV_S_V    = 0x1050244;
        YUV_LV_Buf    = 0x13505A0;

        Preview_Control = 1;
        EDMAC_24_Redirect = 0;
    }

    if (CROP_3K)
    {
        if (is_650D || is_700D || is_EOSM)
        {
            RAW_H    = 0x322;
            RAW_V    = 0x538;
            TimerB   = 0x60F;
            TimerA   = 0x35B;
        }

        if (is_100D)
        {
            RAW_H    = 0x32B;
            RAW_V    = 0x53E;
            TimerB   = 0x60B;
            TimerA   = 0x35D;
        }

        Preview_Control = 0;
        EDMAC_24_Redirect = 0;
    }

    if (CROP_1440p)
    {
        if (is_650D || is_700D || is_EOSM)
        {
            RAW_H    = 0x2A2;
            RAW_V    = 0x5BC;
            TimerB   = 0x71E;
            TimerA   = 0x2DB;
        }

        if (is_100D)
        {
            RAW_H    = 0x2AB;
            RAW_V    = 0x5C2;
            TimerB   = 0x719;
            TimerA   = 0x2DD;
        }

        Preview_H     = 2552;  // 2556 causes preview artifacts
        Preview_V     = 1440;
        Preview_R     = 0x19000E;
        YUV_HD_S_H    = 0x1050286;
        YUV_HD_S_V    = 0x1050215;
        
        YUV_LV_S_V    = 0x10501BA;
        YUV_LV_Buf    = 0x19505A0;
        
        Black_Bar     = 2;
        Preview_Control = 1;
        EDMAC_24_Redirect = 1;
        EDMAC_9_Vertical_Change = 1;
    }

    if (CROP_Full_Res)
    {
        if (is_650D || is_700D || is_EOSM)
        {
            RAW_H    = 0x538;
            RAW_V    = 0xDB4;
            TimerB   = 0x2D06;
            TimerA   = 0x56B;
        }

        if (is_100D)
        {
            RAW_H    = 0x541;
            RAW_V    = 0xDB4;
            TimerB   = 0x2D06;
            TimerA   = 0x56B;
        }

        Preview_Control = 0;
        EDMAC_24_Redirect = 0;
    }

    if (Preview_Control)
    {
        if (EDMAC_9_Vertical_Change)
        {
            if (MEM(EDMAC_9_Vertical_1) != RAW_V - 1 || MEM(EDMAC_9_Vertical_2) != RAW_V - 1) // set our new value if not set yet
            {
                MEM(EDMAC_9_Vertical_1)  = RAW_V - 1;
                MEM(EDMAC_9_Vertical_2)  = RAW_V - 1;
            }

            switch (reg)
            {
                case 0xC0F08184: return RAW_V - 1; // used to exceed vertical preview limit
            }
        }
        
        if (shamem_read(0xC0F11BC8) != 0)
        {
            EngDrvOutLV(0xC0F11BC8, YUV_HD_S_V_E); // Enable vertical stretch on YUV (HD) path
        }

        switch (reg)
        {
            case 0xC0F1A00C: return (Preview_V << 16) + Preview_H - 0x1;   
            case 0xC0F11B9C: return (Preview_V << 16) + Preview_H - 0x1;

            case 0xC0F11B8C: return YUV_HD_S_H;
            case 0xC0F11BCC: return YUV_HD_S_V;
        //  case 0xC0F11BC8: return YUV_HD_S_V_E; // overriding it from here doesn't work
            case 0xC0F11ACC: return YUV_LV_S_V;
            case 0xC0F04210: return YUV_LV_Buf;
        }
    }

    switch (reg)
    {
        case 0xC0F06804: return (RAW_V << 16) + RAW_H;

        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        {
            return RAW_H + 0x32;
        }

        case 0xC0F0713c: return RAW_V + 0x1;
        case 0xC0F07150: return RAW_V - 0x3A;

        case 0xC0F06014: return TimerB;
        case 0xC0F06010: return TimerA;
        case 0xC0F06008: return TimerA + (TimerA << 16);
        case 0xC0F0600C: return TimerA + (TimerA << 16);
    }

    return 0;
}

static inline uint32_t reg_override_1X3(uint32_t reg, uint32_t old_val)
{
    if (AR_2_35_1)
    {
        if (Anam_Highest)
        {
            if (is_650D || is_700D || is_EOSM) // not sure about EOS M
            {
                RAW_H         = 0x1D4;  // from mv1080 mode
                RAW_V         = 0x8C2;
                TimerB        = 0xA05;
                TimerA        = 0x207;
            }

            if (is_100D)
            {
                RAW_H         = 0x1DD;
                RAW_V         = 0x8C8;
                TimerB        = 0x9CB;
                TimerA        = 0x213;  // can be lowered even more? need to be fine tuned
            }

            Preview_H     = 1728;      // from mv1080 mode
            Preview_V     = 2214;
            Preview_R     = 0x1D000E;  // from mv1080 mode
            YUV_HD_S_H    = 0x8500DF;
            YUV_HD_S_V    = 0x8501A8;
            YUV_HD_S_V_E  = 0;

            YUV_LV_S_V    = 0x8E013F;
            YUV_LV_Buf    = 0x13205A0;

            EDMAC_24_Redirect = 1;
            EDMAC_9_Vertical_Change = 1;
        }
        
        Preview_Control = 1;
    }

    Black_Bar = 0;

    if (Preview_Control)
    {
        if (EDMAC_9_Vertical_Change)
        {
            if (MEM(EDMAC_9_Vertical_1) != RAW_V - 1 || MEM(EDMAC_9_Vertical_2) != RAW_V - 1) // set our new value if not set yet
            {
                MEM(EDMAC_9_Vertical_1)  = RAW_V - 1;
                MEM(EDMAC_9_Vertical_2)  = RAW_V - 1;
            }

            switch (reg)
            {
                case 0xC0F08184: return RAW_V - 1; // used to exceed vertical preview limit
            }
        }

        if (shamem_read(0xC0F11BC8) != 0)
        {
            EngDrvOutLV(0xC0F11BC8, YUV_HD_S_V_E); // Enable vertical stretch on YUV (HD) path
        }

        switch (reg)
        {
            case 0xC0F1A00C: return (Preview_V << 16) + Preview_H - 0x1;   
            case 0xC0F11B9C: return (Preview_V << 16) + Preview_H - 0x1;

            case 0xC0F11B8C: return YUV_HD_S_H;
            case 0xC0F11BCC: return YUV_HD_S_V;
        //  case 0xC0F11BC8: return YUV_HD_S_V_E; // overriding it from here doesn't work
            case 0xC0F11ACC: return YUV_LV_S_V;
            case 0xC0F04210: return YUV_LV_Buf;
        }
    }

    switch (reg)
    {
        case 0xC0F06804: return (RAW_V << 16) + RAW_H;

        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        {
            return RAW_H + 0x32;
        }

        case 0xC0F0713c: return RAW_V + 0x1;
        case 0xC0F07150: return RAW_V - 0x3A;

        case 0xC0F06014: return TimerB;
        case 0xC0F06010: return TimerA;
        case 0xC0F06008: return TimerA + (TimerA << 16);
        case 0xC0F0600C: return TimerA + (TimerA << 16);
    }

    return 0;
}

static inline uint32_t reg_override_3X3(uint32_t reg, uint32_t old_val)
{
    return 0;
}

static int engio_vidmode_ok = 0;

static void * get_engio_reg_override_func()
{
    uint32_t (*reg_override_func)(uint32_t, uint32_t) = 
      //(crop_preset == CROP_PRESET_3X)         ? reg_override_top_bar     : /* fixme: corrupted image */
        (crop_preset == CROP_PRESET_3X_TALL)    ? reg_override_3X_tall    :
        (crop_preset == CROP_PRESET_3x3_1X)     ? reg_override_3x3_tall   :
        (crop_preset == CROP_PRESET_3x3_1X_48p) ? reg_override_3x3_48p    :
        (crop_preset == CROP_PRESET_3K)         ? reg_override_3K         :
        (crop_preset == CROP_PRESET_4K_HFPS)    ? reg_override_4K_hfps    :
        (crop_preset == CROP_PRESET_UHD)        ? reg_override_UHD        :
        (crop_preset == CROP_PRESET_40_FPS)     ? reg_override_40_fps     :
        (crop_preset == CROP_PRESET_FULLRES_LV) ? reg_override_fullres_lv :
        (crop_preset == CROP_PRESET_CENTER_Z)   ? reg_override_zoom_fps   :
        
        /* 650D / 700D / EOSM/M2 / 100D reg_override_func presets */
        (crop_preset == CROP_PRESET_1X1)        ? reg_override_1X1        :
        (crop_preset == CROP_PRESET_1X3)        ? reg_override_1X3        :
        (crop_preset == CROP_PRESET_3X3)        ? reg_override_3X3        :
                                                  0                       ;
    return reg_override_func;
}

static void FAST engio_write_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    uint32_t (*reg_override_func)(uint32_t, uint32_t) = 
        get_engio_reg_override_func();

    if (!reg_override_func)
    {
        return;
    }

    // is engio_vidmode_ok still needed? PathDriveMode might be enough to detect video modes
    
    if (is_basic || is_5D3)
    {
        /* cmos_vidmode_ok doesn't help;
        * we can identify the current video mode from 0xC0F06804 */
        for (uint32_t * buf = (uint32_t *) regs[0]; *buf != 0xFFFFFFFF; buf += 2)
        {
            uint32_t reg = *buf;
            uint32_t old = *(buf+1);
            if (reg == 0xC0F06804)
            {
                if (is_5D3)
                {
                    engio_vidmode_ok = (crop_preset == CROP_PRESET_CENTER_Z)
                    ? (old == 0x56601EB)                        /* x5 zoom */
                    : (old == 0x528011B || old == 0x2B6011B);   /* 1080p or 720p */
                }
            
                else
                {
                    if ((PathDriveMode->zoom > 1) && is_basic) // don't brighten up LiveView in x5/x10 modes for now for is_basic
                    {
                        engio_vidmode_ok = 0;
                    }
                
                    else
                    {
                        engio_vidmode_ok = 1;
                    }
                }
            }
        }
    }

    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
        
        if (is_5D3 || is_basic)
        {
            if (!engio_vidmode_ok)
            {
                return;
            }
        }
    }

    for (uint32_t * buf = (uint32_t *) regs[0]; *buf != 0xFFFFFFFF; buf += 2)
    {
        uint32_t reg = *buf;
        uint32_t old = *(buf+1);
        
        int new = reg_override_func(reg, old);
        if (new)
        {
            dbg_printf("[%x] %x: %x -> %x\n", regs[0], reg, old, new);
            *(buf+1) = new;
        }
        
        // brighten up LiveView when using negative analog gain in lower bit-depths
        if (reg == 0xC0F42744) 
        {
            if (which_output_format() >= 3) // don't patch if we are using uncompressed RAW 
            {
                if (OUTPUT_12BIT && old != 0x2020202)
                {
                    *(buf+1) = 0x2020202;
                }
            
                if (OUTPUT_11BIT && old != 0x3030303)
                {
                    *(buf+1) = 0x3030303;
                }
            
                if (OUTPUT_10BIT && old != 0x4040404)
                {
                *(buf+1) = 0x4040404;
                }
            }
        }
    }
}

static int change_buffer_now = 0;

static void FAST EngDrvOut_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
    }
    
    uint32_t data = (uint32_t) regs[0];
    uint16_t dst = (data & 0xFFFF0000) >> 16;
    uint16_t reg = data & 0x0000FFFF;
    uint32_t val = (uint32_t) regs[1];

    // adjust LiveView black level when using lower bit-depths with negative analog gain
    if (data == 0xC0F0819C)
    {
        // 100D doesn't need this
        if (is_650D || is_700D || is_EOSM)
        {
            if (lens_info.iso_analog_raw == ISO_400)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC39;
                if (OUTPUT_11BIT) regs[1] = 0xC39;
                if (OUTPUT_12BIT) regs[1] = 0xC39;
            }
            if (lens_info.iso_analog_raw == ISO_800)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC3C;
                if (OUTPUT_11BIT) regs[1] = 0xC3A;
                if (OUTPUT_12BIT) regs[1] = 0xC3A;
            }
            if (lens_info.iso_analog_raw == ISO_1600)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC41;
                if (OUTPUT_11BIT) regs[1] = 0xC40;
                if (OUTPUT_12BIT) regs[1] = 0xC40;
            }
            if (lens_info.iso_analog_raw == ISO_3200)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC4A;
                if (OUTPUT_11BIT) regs[1] = 0xC48;
                if (OUTPUT_12BIT) regs[1] = 0xC48;
            }
            if (lens_info.iso_analog_raw == ISO_6400)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC5C;
                if (OUTPUT_11BIT) regs[1] = 0xC5B;
                if (OUTPUT_12BIT) regs[1] = 0xC58;
            }
            if (lens_info.iso_analog_raw == ISO_12800)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC5C;
                if (OUTPUT_11BIT) regs[1] = 0xC5A;
                if (OUTPUT_12BIT) regs[1] = 0xC58;
            }
        }
    }

    if (dst == 0xC0F2)
    {
        // 0xC0F26808 register sets EDMAC#24 buffer address, change it to Photo mode buffer address
        if (Preview_Control && EDMAC_24_Redirect)
        {
            // we need to know when to override 0xC0F26808, detect it from 0xC0F26804, it's always
            // being set to 0x40000000 before setting 0xC0F26808 value
            if (reg == 0x6804 && val == 0x40000000) 
            {
                change_buffer_now = 1;
            }
    
            if (reg == 0x6808 && change_buffer_now == 1) // 0xC0F26808
            {
                if (is_650D || is_700D || is_EOSM)
                {
                    regs[1] = 0x1595b00; // Size 0xC0F26810  = 0x3237e  is being set in EngDrvOuts_hook
                }
            
                if (is_100D)
                {
                    regs[1] = 0x2000000; // Size 0xC0F26810  = 0x1f32da is being set in EngDrvOuts_hook
                }
            
                change_buffer_now = 0;
            }
        }
    }

    /* set our preview registers overrides */
    if (dst == 0xC0F3)
    {
        if (Preview_Control)
        {
            switch (reg)
            {
                case 0x8070: regs[1] = ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5;       break;
                case 0x8078: regs[1] = (((Preview_H / 4) + 6) << 16) + 1;                   break;
                case 0x807C: regs[1] = ((Preview_H / 4) + 5) << 16;                         break;
                case 0x8080: regs[1] = ((Preview_V + 0x7) << 16) + 2;                       break;
                case 0x8084: regs[1] = ((Preview_H / 4) + 7) << 16;                         break;
                case 0x8094: regs[1] = ( Preview_V + 0xa) << 16;                            break;
                case 0x80A0: regs[1] = ((Preview_H / 4) + 7) << 16;                         break;
                case 0x80A4: regs[1] = ((Preview_H / 4) + 7) << 16;                         break;
                case 0x8024: 
                if (is_700D || is_EOSM || is_650D)
                             regs[1] = ((RAW_V - 1) << 16)  + RAW_H - 0x11;                 
                if (is_100D) regs[1] = ((RAW_V - 5) << 16)  + RAW_H - 0x1A;                 break;
                case 0x83D4: regs[1] =   Preview_R;                                         break;
                case 0x83DC: regs[1] = ((Preview_V + 0x1c) << 16)  + Preview_H / 4 + 0x48;  break;
                case 0x8934: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break;
                case 0x8960: regs[1] = ( Preview_V + 0x6) << 16;                            break;
                case 0x89A4: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break;
                case 0x89B4: regs[1] = ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 6;     break;
                case 0x89D4: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break;
                case 0x89E4: regs[1] = ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 7;     break;
                case 0x89EC: regs[1] = ((Preview_H / 4 + 6) << 16) + 1;                     break;
                
            //  case 0xA04C: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break; // It's being set in EngDrvOuts_hook
            //  case 0xA0A0: regs[1] = ((Preview_V + 0xa) << 16)   + Preview_H + 0xb;       break; // It's being set in EngDrvOuts_hook
            //  case 0xA0B0: regs[1] = ((Preview_V + 0xa) << 16)   + Preview_H + 0x8;       break; // It's being set in EngDrvOuts_hook
                case 0xB038: regs[1] =  Black_Bar;                                          break;
                case 0xB088: regs[1] =  Black_Bar;                                          break;
                case 0xB054: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H + 0x7;       break;
                case 0xB070: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H + 0x57;      break;
                case 0xB074: regs[1] = ( Preview_V        << 16)   + Preview_H + 0x57;      break;
                case 0xB0DC: regs[1] = ( Preview_V        << 16)   + Preview_H + 0x4f;      break;
            }
        }
    }

    if (dst == 0xC0F4)
    {
        if (Preview_Control)
        {
            switch (reg)
            {
                case 0x2014: regs[1] = ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5;      break;
                case 0x204C: regs[1] = ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5;      break;
                case 0x2194: regs[1] = ( Preview_H / 4) + 5;                               break;
            }
        }
    }
}

static void FAST EngDrvOuts_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
    }

    uint32_t data = (uint32_t) regs[0];
//  uint16_t dst = (data & 0xFFFF0000) >> 16;
//  uint16_t reg = data & 0x0000FFFF;
//  uint32_t * val = (uint32_t*) regs[1];
//  uint32_t num = (uint32_t) regs[2];

    if (Preview_Control)
    {
        /* set our preview registers overrides */
        if (data == 0xC0F3A048)
        {
            *(uint32_t*) (regs[1] + 4)    = ((Preview_V + 0x6) << 16) + Preview_H / 4 + 5; // 0xC0F3A04C
        }
        
        if (data == 0xC0F3A098)
        {
            *(uint32_t*) (regs[1] + 8)    = ((Preview_V + 0xa) << 16) + Preview_H + 0xb;   // 0xC0F3A0A0
            *(uint32_t*) (regs[1] + 0x18) = ((Preview_V + 0xa) << 16) + Preview_H + 0x8;   // 0xC0F3A0B0
        }

        /* change EDMAC#24 buffer size 0xC0F26810 to photo mode buffer size */
        if (EDMAC_24_Redirect)
        {
            if (data == 0xC0F2680C)
            {
                // we need to know when to set buffer size because the channel does other things before
                // setting the final buffer size which we want to change, let's use 0xC0F35084 as flag because
                // it's always being set after "the other things" finish and before setting 0xC0F26810 final size
                if (shamem_read(0xC0F35084) == 0xA1F)
                {
                    if (is_650D || is_700D || is_EOSM)
                    {
                        *(uint32_t*) (regs[1] + 4) = 0x3237e;
                    }

                    if (is_100D)
                    {
                        *(uint32_t*) (regs[1] + 4) = 0x1f32da;
                    }
                }
            }  
        }
    }
}

static void FAST PATH_SelectPathDriveMode_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    /* we need to enable and set preview shifting and clearing artifacts values here especially for clear artifacts value */
    /* I don't know which function load shifting preview value, but it's being loaded and applied many times in LiveView, not just once. */
    /* clear artifacts value is being loaded very early before CMOS, ADTG, ENGIO, ENG_DRV_OUT, ENG_DRV_OUTS stuff */
    /* in [VRAM] VRAM_PTH_StartTripleRamClearInALump[ff962a90] (ff962a90 + 0x8 holds value for clearing artifacts for x5 mode for LCD output on 700D) */
    /* apparently PATH_SelectPathDriveMode sets its arguments before loading/applying any video configuration, e.g: */

    /*  700D DebugLog, x5 mode:
    
        Evf:ff19ce1c:ad:03: PATH_Select S:8 Z:50000 R:0 DZ:0 SM:0 SV:0 DT:0       <-- we are patching it from here before loading it
        Evf:ff19cfb0:ad:03: PathDriveMode Change: 10->2
        Evf:ff37ad64:ad:03: GetPathDriveInfo[2]
        Evf:ff4f2ce8:ad:03: LVx5_SelectPath LCD
        Evf:ff4f3980:ad:01: LVx5_GetVramParam(W:720 H:480)
        Evf:ff37cebc:ad:03: RamClear_SetPath
        Evf:ff37d4c8:ad:03: LV_ResLockTripleRamClearPass
        Evf:ff4ee18c:a9:03: [VRAM] VRAM_PTH_StartTripleRamClearInALump[ff962a90]  <-- clear artifacts value is being loaded here, ff962a90 is array holding six 32 bits values
        Evf:ff37cf1c:ad:03: RamClear_StartPath
        Evf:ff37d084:ad:03: RamClear_LV_RAMCLEAR_COLOR_BLACK
        Evf:ff37cf1c:ad:03: RamClear_StartPath
        Evf:ff37d084:ad:03: RamClear_LV_RAMCLEAR_COLOR_BLACK
        Evf:ff37cf1c:ad:03: RamClear_StartPath
        Evf:ff37d084:ad:03: RamClear_LV_RAMCLEAR_COLOR_BLACK
        Evf:ff36ce48:a9:03: [VRAM]====>> PathRamClearCompleteCBR   */ 

    /* FIXME: we might be able to implement clearing artifacts directly in VRAM_PTH_StartTripleRamClearInALump
              this way we don't to patch ROM addresses for clearing artifacts for x5 mode and for every output on every model */

    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        if (crop_preset_1x1_res == 0)       // CROP_2_5K
        {
            preview_shift_value = 0x1F4A0;
            Shift_Preview = 1;
            Clear_Artifacts = 1;
            EDMAC_9_Vertical_Change = 0;
        }

        else if (crop_preset_1x1_res == 2)  // CROP_1440p
        {
            preview_shift_value = 0xD5C0;
            Shift_Preview = 1;
            Clear_Artifacts = 1;
            EDMAC_9_Vertical_Change = 1;
        }

        /* not supported presets, turn these off */
        else
        {
            preview_shift_value = 0;
            Shift_Preview = 0;
            Clear_Artifacts = 0;
        }
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X3)
    {
        if (AR_2_35_1)
        {
            preview_shift_value = 0x1f4a0;
        }

        Shift_Preview = 1;
        Clear_Artifacts = 1;
        EDMAC_9_Vertical_Change = 1;
    }

    /* restore defualt EDMAC#9 vertical size */
    if (EDMAC_9_Vertical_Change == 0 || PathDriveMode->zoom != 5)
    {
        if (MEM(EDMAC_9_Vertical_1) != 0x453 || MEM(EDMAC_9_Vertical_2) != 0x453)
        {
            MEM(EDMAC_9_Vertical_1)  = 0x453;
            MEM(EDMAC_9_Vertical_2)  = 0x453;
        }
        
        EDMAC_9_Vertical_Change = 0;
    }

    /* FIXME: hardcoded addresses for x5 mode on LCD screen, implement HDMI support! */
    if (PathDriveMode->zoom == 5)
    {
        /* patch supported presets if patch not active */
        if (Shift_Preview && !Center_Preview_ON)
        {
            patch_memory(Shift_x5_LCD, 0x0, preview_shift_value, "Center");
            Center_Preview_ON = 1;
        }

        if (Clear_Artifacts && !Clear_Artifacts_ON)
        {
            patch_memory(Clear_Vram_x5_LCD, 0x0, 0x5a0, "Clear"); // 0x5a0 seems to clear all artifacts
            Clear_Artifacts_ON = 1;
        }

        /* unpatch not supported presets if patch already active */
        if (!Shift_Preview && Center_Preview_ON)
        {
            unpatch_memory(Shift_x5_LCD);
            Center_Preview_ON = 0;
        }

        if (!Clear_Artifacts && Clear_Artifacts_ON)
        {
            unpatch_memory(Clear_Vram_x5_LCD);
            Clear_Artifacts_ON = 0;
        }
    }

    /* unpatch in all other modes if patch already active  */
    if (PathDriveMode->zoom != 5)
    {
        if (Center_Preview_ON)
        {
            unpatch_memory(Shift_x5_LCD);
            Center_Preview_ON = 0;
        }

        if (Clear_Artifacts_ON)
        {
            unpatch_memory(Clear_Vram_x5_LCD);
            Clear_Artifacts_ON = 0;
        }
    }
}

static int patch_active = 0;

static void update_patch()
{
    if (CROP_PRESET_MENU)
    {
        /* update preset */
        crop_preset = CROP_PRESET_MENU;

        /* install our hooks, if we haven't already do so */
        if (!patch_active)
        {
            patch_hook_function(CMOS_WRITE, MEM_CMOS_WRITE, &cmos_hook, "crop_rec: CMOS[1,2,6] parameters hook");
            patch_hook_function(ADTG_WRITE, MEM_ADTG_WRITE, &adtg_hook, "crop_rec: ADTG[8000,8806] parameters hook");
            if (ENGIO_WRITE)
            {
                patch_hook_function(ENGIO_WRITE, MEM_ENGIO_WRITE, engio_write_hook, "crop_rec: video timers hook");
            }
            if (ENG_DRV_OUT)
            {
                patch_hook_function(ENG_DRV_OUT, MEM(ENG_DRV_OUT), EngDrvOut_hook, "crop_rec: preview stuff 1");
            }
            if (ENG_DRV_OUTS)
            {
                patch_hook_function(ENG_DRV_OUTS, MEM(ENG_DRV_OUTS), EngDrvOuts_hook, "crop_rec: preview stuff 2");
            }
            if (PATH_SelectPathDriveMode)
            {
                patch_hook_function(PATH_SelectPathDriveMode, MEM(PATH_SelectPathDriveMode), PATH_SelectPathDriveMode_hook, "crop_rec: preview stuff 3");
            }
            patch_active = 1;
        }
    }
    else
    {
        /* undo active patches, if any */
        if (patch_active)
        {
            unpatch_memory(CMOS_WRITE);
            unpatch_memory(ADTG_WRITE);
            if (ENGIO_WRITE)
            {
                unpatch_memory(ENGIO_WRITE);
            }
            if (ENG_DRV_OUT)
            {
                unpatch_memory(ENG_DRV_OUT);
            }
            if (ENG_DRV_OUTS)
            {
                unpatch_memory(ENG_DRV_OUTS);
            }
            if (PATH_SelectPathDriveMode)
            {
                unpatch_memory(PATH_SelectPathDriveMode);
            }

            /* FIXME: hardcoded addresses for x5 mode on LCD screen, implement HDMI support! */
            if (Clear_Artifacts_ON)
            {
                unpatch_memory(Clear_Vram_x5_LCD);
                Clear_Artifacts_ON = 0;
            }
            if (Center_Preview_ON)
            {
                unpatch_memory(Shift_x5_LCD);
                Center_Preview_ON = 0 ;
            }
            patch_active = 0;
            crop_preset = 0;
        }
    }
}

/* enable patch when switching LiveView (not in the middle of LiveView) */
/* otherwise you will end up with a halfway configured video mode that looks weird */
PROP_HANDLER(PROP_LV_ACTION)
{
    update_patch();
}

/* also try when switching zoom modes */
PROP_HANDLER(PROP_LV_DISPSIZE)
{
    update_patch();
}

/* forward reference */
static struct menu_entry crop_rec_menu[];

static MENU_UPDATE_FUNC(crop_update)
{
/*  if (is_DIGIC_5)
    {
        MENU_SET_VALUE("%s %s", CROP_2_5K ? ,
                                (crop_preset_index == 0) ? crop_choices_DIGIC_5[0] : (crop_preset_index == 1) ? crop_choices_DIGIC_5[1] :
                                (crop_preset_index == 2) ? crop_choices_DIGIC_5[2] : (crop_preset_index == 3) ? crop_choices_DIGIC_5[3] : 
                                 crop_choices_DIGIC_5[0]);
    }*/

    if (is_DIGIC_5)
    {
        /* reveal options for the current crop mode (1:1, 1x3 and 3x3) */
        crop_rec_menu[0].children[0].shidden = (crop_preset_index != 1);  // 1 CROP_PRESET_1X1
        crop_rec_menu[0].children[1].shidden = (crop_preset_index != 2);  // 2 CROP_PRESET_1X3
        crop_rec_menu[0].children[2].shidden = (crop_preset_index != 3);  // 3 CROP_PRESET_3X3
    }

    if (CROP_PRESET_MENU && lv)
    {
        if (CROP_PRESET_MENU == CROP_PRESET_CENTER_Z || is_DIGIC_5)
        {
            if (lv_dispsize == 1)
            {
                MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "To use this mode, exit ML menu & press the zoom button (set to x5).");
            }
        }
        else /* non-zoom modes */
        {
            if (is_basic || is_5D3)
            {
                if (!is_supported_mode())
                {
                    MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "This preset only works in 1080p and 720p video modes.");
                }
                else if (lv_dispsize != 1)
                {
                    MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "To use this mode, exit ML menu and press the zoom button (set to x1).");
                }
                else if (!is_720p())
                {
                    if (CROP_PRESET_MENU == CROP_PRESET_3x3_1X ||
                        CROP_PRESET_MENU == CROP_PRESET_3x3_1X_48p)
                    {
                        /* these presets only have effect in 720p mode */
                        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "This preset only works in the 720p 50/60 fps modes from Canon menu.");
                        return;
                    }
                }
            }
        }
    }
}

static MENU_UPDATE_FUNC(crop_preset_1x3_res_update)
{
    if (AR_16_9)
    {
        if (Anam_Highest) MENU_SET_VALUE("4.5K");
        if (Anam_Higher)  MENU_SET_VALUE("4.2K");
        if (Anam_Medium)  MENU_SET_VALUE("UHD");
    }

    if (AR_2_1)
    {
        if (Anam_Highest) MENU_SET_VALUE("4.8K");
        if (Anam_Higher)  MENU_SET_VALUE("4.4K");
        if (Anam_Medium)  MENU_SET_VALUE("4K");
    }

    if (AR_2_20_1)
    {
        if (Anam_Highest) MENU_SET_VALUE("5K");
        if (Anam_Higher)  MENU_SET_VALUE("4.6K");
        if (Anam_Medium)  MENU_SET_VALUE("4.1K");
    }

    if (AR_2_35_1 || AR_2_39_1)
    {
        if (Anam_Highest) MENU_SET_VALUE("5.2K");
        if (Anam_Higher)  MENU_SET_VALUE("4.8K");
        if (Anam_Medium)  MENU_SET_VALUE("4.3K");
    }
}

static MENU_UPDATE_FUNC(target_yres_update)
{
    MENU_SET_RINFO("from %d", max_resolutions[crop_preset][get_video_mode_index()]);
}

static MENU_UPDATE_FUNC(bit_depth_analog_update)
{
    if (which_output_format() < 3)
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "To use this option, change Data format to lossless in RAW video.");
    }
}

static struct menu_entry crop_rec_menu[] =
{
    // FIXME: how to handle menu in cleaner way for is_DIGIC_5 models?
    {
        .name       = "Crop mode",
        .priv       = &crop_preset_index,
        .update     = crop_update,
        .depends_on = DEP_LIVEVIEW,
        .children =  (struct menu_entry[]) {
            {
                .name       = "Preset:",   // CROP_PRESET_1X1
                .priv       = &crop_preset_1x1_res,
                .max        = 3,
                .choices    = CHOICES("2.5K", "3K","1440p", "Full-Res LV"),
                .help       = "Choose 1:1 preset.",
                .icon_type  = IT_ALWAYS_ON,
                .shidden    = 1,
            },
            {
                .name       = "Preset: ",  // CROP_PRESET_1X3
                .priv       = &crop_preset_1x3_res,
                .update     = crop_preset_1x3_res_update,
                .max        = 2,
                .choices    = CHOICES("Highest", "Higher", "Medium"),  // dummy choices, strings are being changed depending on aspect ratio and res
                .help       = "Choose 1x3 preset.",
                .icon_type  = IT_ALWAYS_ON,
                .shidden    = 1,
            },
            {
                .name       = "Preset:  ",  // CROP_PRESET_3X3
                .priv       = &crop_preset_3x3_res,
                .max        = 0,
                .choices    = CHOICES("1736"),
                .help       = "Choose 3x3 preset.",
                .icon_type  = IT_ALWAYS_ON,
                .shidden    = 1,
            },
            {
                .name       = "Aspect ratio:",
                .priv       = &crop_preset_ar,
                .max        = 4,
                .choices    = CHOICES("16:9", "2:1", "2.20:1", "2.35:1", "2.39:1"),
                .help       = "Select aspect ratio for current preset.",
                .icon_type  = IT_ALWAYS_ON,
            },
/*          {
                .name       = "Framerate:",
                .priv       = &crop_preset_fps,
                .max        = 2,
                .choices    = CHOICES("23.976", "25", "30"),
                .help       = "Select framerate for current preset.",
                .icon_type  = IT_ALWAYS_ON,
            },*/
            {
                .name       = "Bit-depth",
                .priv       = &bit_depth_analog,
                .update     = bit_depth_analog_update,
                .max        = 3,
                .choices    = CHOICES("14-bit", "12-bit","11-bit", "10-bit"),
                .help       = "Choose bit-depth for lossless RAW video compression.",
                .icon_type  = IT_ALWAYS_ON,
            },
            {
                .name       = "Shutter range",
                .priv       = &shutter_range,
                .max        = 1,
                .choices    = CHOICES("Original", "Full range"),
                .help       = "Choose the available shutter speed range:",
                .help2      = "Original: default range used by Canon in selected video mode.\n"
                              "Full range: from 1/FPS to minimum exposure time allowed by hardware."
            },
            {
                .name   = "Preview Debug",
                .priv   = &preview_debug,
                .max    = 0xFFFFFFF,
                .unit   = UNIT_HEX,
                .help   = "Preview Debug.",
                .advanced = 1,
            },
            {
                .name   = "Target YRES",
                .priv   = &target_yres,
                .update = target_yres_update,
                .max    = 3870,
                .unit   = UNIT_DEC,
                .help   = "Desired vertical resolution (only for presets with higher resolution).",
                .help2  = "Decrease if you get corrupted frames (dial the desired resolution here).",
                .advanced = 1,
            },
            {
                .name   = "Delta ADTG 0",
                .priv   = &delta_adtg0,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help   = "ADTG 0x8178, 0x8196, 0x82F8",
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "Delta ADTG 1",
                .priv   = &delta_adtg1,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help   = "ADTG 0x8179, 0x8197, 0x82F9",
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "Delta HEAD3",
                .priv   = &delta_head3,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "Delta HEAD4",
                .priv   = &delta_head4,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "CMOS[1] lo",
                .priv   = &cmos1_lo,
                .max    = 63,
                .unit   = UNIT_DEC,
                .help   = "Start scanline (very rough). Use for vertical positioning.",
                .advanced = 1,
            },
            {
                .name   = "CMOS[1] hi",
                .priv   = &cmos1_hi,
                .max    = 63,
                .unit   = UNIT_DEC,
                .help   = "End scanline (very rough). Increase if white bar at bottom.",
                .help2  = "Decrease if you get strange colors as you move the camera.",
                .advanced = 1,
            },
            {
                .name   = "CMOS[2]",
                .priv   = &cmos2,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Horizontal position / binning.",
                .help2  = "Use for horizontal centering.",
                .advanced = 1,
            },
            MENU_ADVANCED_TOGGLE,
            MENU_EOL,
        },
    },
};

static int settings_changed = 0;
static int crop_rec_needs_lv_refresh()
{
    if (!lv)
    {
        return 0;
    }

    if (CROP_PRESET_MENU)
    {
        if (is_supported_mode())
        {
            if (!patch_active || CROP_PRESET_MENU != crop_preset || settings_changed)
            {
                return 1;
            }
        }
    }
    else /* crop disabled */
    {
        if (patch_active)
        {
            return 1;
        }
    }

    return 0;
}

static void center_canon_preview()
{
    /* center the preview window on the raw buffer */
    /* overriding these registers once will do the trick...
     * ... until the focus box is moved by the user */
    int old = cli();

    uint32_t pos1 = shamem_read(0xc0f383d4);
    uint32_t pos2 = shamem_read(0xc0f383dc);

    if ((pos1 & 0x80008000) == 0x80008000 &&
        (pos2 & 0x80008000) == 0x80008000)
    {
        /* already centered */
        sei(old);
        return;
    }

    int x1 = pos1 & 0xFFFF;
    int x2 = pos2 & 0xFFFF;
    int y1 = pos1 >> 16;
    int y2 = pos2 >> 16;

    if (x2 - x1 != 299 && y2 - y1 != 792)
    {
        /* not x5/x10 (values hardcoded for 5D3) */
        sei(old);
        return;
    }

    int raw_xc = (146 + 3744) / 2 / 4;  /* hardcoded for 5D3 */
    int raw_yc = ( 60 + 1380) / 2;      /* values from old raw.c */

    if (1)
    {
        /* use the focus box position for moving the preview window around */
        /* don't do that while recording! */
        dbg_printf("[crop_rec] %d,%d ", raw_xc, raw_yc);
        raw_xc -= 146 / 2 / 4;  raw_yc -= 60 / 2;
        /* this won't change the position if the focus box is centered */
        get_afframe_pos(raw_xc * 2, raw_yc * 2, &raw_xc, &raw_yc);
        raw_xc += 146 / 2 / 4;  raw_yc += 60 / 2;
        raw_xc &= ~1;   /* just for consistency */
        raw_yc &= ~1;   /* this must be even, otherwise the image turns pink */
        raw_xc = COERCE(raw_xc, 176, 770);  /* trial and error; image pitch changes if we push to the right */
        raw_yc = COERCE(raw_yc, 444, 950);  /* trial and error; broken image at the edges, outside these limits */
        dbg_printf("-> %d,%d using focus box position\n", raw_xc, raw_yc);
    }
    int current_xc = (x1 + x2) / 2;
    int current_yc = (y1 + y2) / 2;
    int dx = raw_xc - current_xc;
    int dy = raw_yc - current_yc;
    
    if (dx || dy)
    {
        /* note: bits 0x80008000 appear to have no effect,
         * so we'll use them to flag the centered zoom mode,
         * e.g. for focus_box_get_raw_crop_offset */
        dbg_printf("[crop_rec] centering zoom preview: dx=%d, dy=%d\n", dx, dy);
        EngDrvOutLV(0xc0f383d4, PACK32(x1 + dx, y1 + dy) | 0x80008000);
        EngDrvOutLV(0xc0f383dc, PACK32(x2 + dx, y2 + dy) | 0x80008000);
    }

    sei(old);
}

/* faster version than the one from ML core */
static void set_zoom(int zoom)
{
    if (!lv) return;
    if (RECORDING) return;
    if (is_movie_mode() && video_mode_crop) return;
    zoom = COERCE(zoom, 1, 10);
    if (zoom > 1 && zoom < 10) zoom = 5;
    prop_request_change_wait(PROP_LV_DISPSIZE, &zoom, 4, 1000);
}

/* variables for 650D / 700D / EOSM/M2 / 100D help to detect if settings changed */
static int old_ar_preset;
static int old_fps_preset;
static int old_1x1_preset;
static int old_1x3_preset;
static int old_3x3_preset;

int check_if_settings_changed()
{
    if (old_ar_preset  != crop_preset_ar       ||
        old_fps_preset != crop_preset_fps      ||
        old_1x1_preset != crop_preset_1x1_res  ||
        old_1x3_preset != crop_preset_1x3_res  ||
        old_3x3_preset != crop_preset_3x3_res)
    {
        return 1;
    }

    return 0;
}

/* when closing ML menu, check whether we need to refresh the LiveView */
static unsigned int crop_rec_polling_cbr(unsigned int unused)
{
    /* also check at startup */
    static int lv_dirty = 1;

    int menu_shown = gui_menu_shown();
    if (lv && menu_shown)
    {
        lv_dirty = 1;
    }
    
    if (!lv || menu_shown || RECORDING_RAW)
    {
        /* outside LV: no need to do anything */
        /* don't change while browsing the menu, but shortly after closing it */
        /* don't change while recording raw, but after recording stops
         * (H.264 should tolerate this pretty well, except maybe 50D) */
        return CBR_RET_CONTINUE;
    }
    
    /* check if any of our settings are changed */
    /* for 650D / 700D / EOSM/M2 / 100D */
    if (check_if_settings_changed())
    {
        lv_dirty = 1;
        settings_changed = 1;
    }

    if (lv_dirty)
    {
        /* do we need to refresh LiveView? */
        if (crop_rec_needs_lv_refresh())
        {
            /* let's check this once again, just in case */
            /* (possible race condition that would result in unnecessary refresh) */
            msleep(500);
            if (crop_rec_needs_lv_refresh())
            {
                info_led_on();
                gui_uilock(UILOCK_EVERYTHING);
                int old_zoom = lv_dispsize;
                set_zoom(lv_dispsize == 1 ? 5 : 1);
                set_zoom(old_zoom);
                gui_uilock(UILOCK_NONE);
                info_led_off();
            }
        }
        lv_dirty = 0;
        settings_changed = 0;
    }

    if (crop_preset == CROP_PRESET_CENTER_Z &&
        (lv_dispsize == 5 || lv_dispsize == 10))
    {
        center_canon_preview();
    }
    
    /* 650D / 700D / EOSM/M2 / 100D preferences */
    if (is_DIGIC_5)
    {
        // all of our presets work in x5 mode because of preview, even none-cropped ones
        if (lv_dispsize == 1 && CROP_PRESET_MENU) 
        {
            set_zoom(5);
        }
        
        if (!menu_shown)
        {   
            // check crop_rec configurations while outside ML menu
            old_ar_preset  = crop_preset_ar;
            old_fps_preset = crop_preset_fps;
            old_1x1_preset = crop_preset_1x1_res;
            old_1x3_preset = crop_preset_1x3_res;
            old_3x3_preset = crop_preset_3x3_res;
        }
    }

    return CBR_RET_CONTINUE;
}

/* Display recording status in top info bar */
static LVINFO_UPDATE_FUNC(crop_info)
{
    LVINFO_BUFFER(16);
    
    if (patch_active)
    {
        if (lv_dispsize > 1)
        {
            switch (crop_preset)
            {
                case CROP_PRESET_CENTER_Z:
                    snprintf(buffer, sizeof(buffer), "3.5K");
                    break;
            }
        }
        else
        {
            switch (crop_preset)
            {
                case CROP_PRESET_3X:
                    /* In movie mode, we are interested in recording sensor pixels
                     * without any binning (that is, with 1:1 mapping);
                     * the actual crop factor varies with raw video resolution.
                     * So, printing 3x is not very accurate, but 1:1 is.
                     * 
                     * In photo mode (mild zoom), what changes is the magnification
                     * of the preview screen; the raw image is not affected.
                     * We aren't actually previewing at 1:1 at pixel level,
                     * so printing 1:1 is a little incorrect.
                     */
                    if (!is_movie_mode())
                    {
                        snprintf(buffer, sizeof(buffer), "3x");
                        goto warn;
                    }
                    break;

                case CROP_PRESET_3X_TALL:
                    snprintf(buffer, sizeof(buffer), "T");
                    break;

                case CROP_PRESET_3K:
                    snprintf(buffer, sizeof(buffer), "3K");
                    break;

                case CROP_PRESET_4K_HFPS:
                    snprintf(buffer, sizeof(buffer), "4K");
                    break;

                case CROP_PRESET_UHD:
                    snprintf(buffer, sizeof(buffer), "UHD");
                    break;

                case CROP_PRESET_FULLRES_LV:
                    snprintf(buffer, sizeof(buffer), "FLV");
                    break;
            }
        }
    }

    /* append info about current binning mode */

    if (raw_lv_is_enabled())
    {
        /* fixme: raw_capture_info is only updated when LV RAW is active */

        if (raw_capture_info.binning_x + raw_capture_info.skipping_x == 1 &&
            raw_capture_info.binning_y + raw_capture_info.skipping_y == 1)
        {
            STR_APPEND(buffer, "%s1:1", buffer[0] ? " " : "");
        }
        else
        {
            STR_APPEND(buffer, "%s%dx%d",
                buffer[0] ? " " : "",
                raw_capture_info.binning_x + raw_capture_info.skipping_x,
                raw_capture_info.binning_y + raw_capture_info.skipping_y
            );
        }
    }

warn:
    if (crop_rec_needs_lv_refresh())
    {
        if (!streq(buffer, SYM_WARNING))
        {
            STR_APPEND(buffer, " " SYM_WARNING);
        }
        item->color_fg = COLOR_YELLOW;
    }
}

static struct lvinfo_item info_items[] = {
    {
        .name = "Crop info",
        .which_bar = LV_BOTTOM_BAR_ONLY,
        .update = crop_info,
        .preferred_position = -50,  /* near the focal length display */
        .priority = 1,
    }
};

static unsigned int raw_info_update_cbr(unsigned int unused)
{
    if (patch_active)
    {
        /* not implemented yet */
        raw_capture_info.offset_x = raw_capture_info.offset_y   = SHRT_MIN;

        if (!is_DIGIC_5) // needed for 700D and similair models
        {
            if (lv_dispsize > 1)
            {
                /* raw backend gets it right */
                return 0;
            }
        }

        /* update horizontal pixel binning parameters */
        switch (crop_preset)
        {
            case CROP_PRESET_3X:
            case CROP_PRESET_3X_TALL:
            case CROP_PRESET_3K:
            case CROP_PRESET_4K_HFPS:
            case CROP_PRESET_UHD:
            case CROP_PRESET_FULLRES_LV:
            case CROP_PRESET_3x1:
            case CROP_PRESET_1X1:
                raw_capture_info.binning_x    = raw_capture_info.binning_y  = 1;
                raw_capture_info.skipping_x   = raw_capture_info.skipping_y = 0;
                break;

            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_48p:
            case CROP_PRESET_1x3:
            case CROP_PRESET_1X3:
                raw_capture_info.binning_x = 3; raw_capture_info.skipping_x = 0;
                break;
        }

        /* update vertical pixel binning / line skipping parameters */
        switch (crop_preset)
        {
            case CROP_PRESET_3X:
            case CROP_PRESET_3X_TALL:
            case CROP_PRESET_3K:
            case CROP_PRESET_4K_HFPS:
            case CROP_PRESET_UHD:
            case CROP_PRESET_FULLRES_LV:
            case CROP_PRESET_1x3:
            case CROP_PRESET_1X3:
            case CROP_PRESET_1X1:
                raw_capture_info.binning_y = 1; raw_capture_info.skipping_y = 0;
                break;

            case CROP_PRESET_3x3_1X:
            case CROP_PRESET_3x3_1X_48p:
            case CROP_PRESET_3x1:
            case CROP_PRESET_3X3:
            {
                int b = (is_5D3) ? 3 : 1;
                int s = (is_5D3) ? 0 : 2;
                raw_capture_info.binning_y = b; raw_capture_info.skipping_y = s;
                break;
            }
        }

        if (is_5D3)
        {
            /* update skip offsets */
            int skip_left, skip_right, skip_top, skip_bottom;
            calc_skip_offsets(&skip_left, &skip_right, &skip_top, &skip_bottom);
            raw_set_geometry(raw_info.width, raw_info.height, skip_left, skip_right, skip_top, skip_bottom);
        }
    }
    return 0;
}

static unsigned int crop_rec_init()
{
    if (is_camera("5D3",  "1.1.3") || is_camera("5D3", "1.2.3"))
    {
        /* same addresses on both 1.1.3 and 1.2.3 */
        CMOS_WRITE = 0x119CC;
        MEM_CMOS_WRITE = 0xE92D47F0;
        
        ADTG_WRITE = 0x11640;
        MEM_ADTG_WRITE = 0xE92D47F0;
        
        ENGIO_WRITE = is_camera("5D3", "1.2.3") ? 0xFF290F98 : 0xFF28CC3C;
        MEM_ENGIO_WRITE = 0xE51FC15C;
        
        PathDriveMode = (void *) (is_camera("5D3", "1.2.3") ? 0x56414 : 0x563BC);   /* argument of PATH_SelectPathDriveMode */
        
        is_5D3 = 1;
        crop_presets                = crop_presets_5d3;
        crop_rec_menu[0].choices    = crop_choices_5d3;
        crop_rec_menu[0].max        = COUNT(crop_choices_5d3) - 1;
        crop_rec_menu[0].help       = crop_choices_help_5d3;
        crop_rec_menu[0].help2      = crop_choices_help2_5d3;
        
        fps_main_clock = 24000000;
                                       /* 24p,  25p,  30p,  50p,  60p,   x5 */
        memcpy(default_timerA, (int[]) {  440,  480,  440,  480,  440,  518 }, 24);
        memcpy(default_timerB, (int[]) { 2275, 2000, 1820, 1000,  910, 1556 }, 24);
    }
    else if (is_camera("EOSM", "2.0.2"))
    {
        CMOS_WRITE = 0x2998C;
        MEM_CMOS_WRITE = 0xE92D41F0;
        
        ADTG_WRITE = 0x2986C;
        MEM_ADTG_WRITE = 0xE92D43F8;
        
        ENGIO_WRITE = 0xFF2C19AC;
        MEM_ENGIO_WRITE = 0xE51FC15C;
        
        ENG_DRV_OUT = 0xFF2C1694;
        ENG_DRV_OUTS = 0xFF2C17B8;
        
        PathDriveMode = (void *) 0x892E8;   /* argument of PATH_SelectPathDriveMode */
        PATH_SelectPathDriveMode = 0x14AC4; // it's being called from RAM
        
        EDMAC_9_Vertical_1 = 0x5976C;
        EDMAC_9_Vertical_2 = 0x5979C;
        
        Shift_x5_LCD = 0xFF96EA3C;
        Shift_x5_HDMI_480p = 0xFF96F3FC;
        Shift_x5_HDMI_1080i_Full = 0xFF96FF54;
        Shift_x5_HDMI_1080i_Info = 0xFF970558;

        Clear_Vram_x5_LCD = 0xFF96EA60;
        Clear_Vram_x5_HDMI_480p = 0xFF96F420;
        Clear_Vram_x5_HDMI_1080i_Full = 0xFF96FF78;
        Clear_Vram_x5_HDMI_1080i_Info = 0xFF97057C;
        
        is_EOSM = 1;
        is_DIGIC_5 = 1;
        crop_presets                = crop_presets_DIGIC_5;
        crop_rec_menu[0].choices    = crop_choices_DIGIC_5;
        crop_rec_menu[0].max        = COUNT(crop_choices_DIGIC_5) - 1;
        crop_rec_menu[0].help       = crop_choices_help_DIGIC_5;
    }
    else if (is_camera("700D", "1.1.5") || is_camera("650D", "1.0.4"))
    {
        CMOS_WRITE = 0x17A1C;
        MEM_CMOS_WRITE = 0xE92D41F0;
        
        ADTG_WRITE = 0x178FC;
        MEM_ADTG_WRITE = 0xE92D43F8;
        
        ENGIO_WRITE = is_camera("700D", "1.1.5") ? 0xFF2C2D00 : 0xFF2C0778;
        MEM_ENGIO_WRITE = 0xE51FC15C;
        
        ENG_DRV_OUT = is_camera("700D", "1.1.5") ? 0xFF2C29E8 : 0xFF2C0460;
        ENG_DRV_OUTS = is_camera("700D", "1.1.5") ? 0xFF2C2B0C : 0xFF2C0584;
        
        PathDriveMode = (void *) (is_camera("700D", "1.1.5") ? 0x6B7F4 : 0x6AEC0);   /* argument of PATH_SelectPathDriveMode */
        PATH_SelectPathDriveMode = is_camera("700D", "1.1.5") ? 0xFF19CDD4 : 0xFF19B230;
        
        EDMAC_9_Vertical_1 = is_camera("700D", "1.1.5") ? 0x3E200 : 0x3E120;
        EDMAC_9_Vertical_2 = is_camera("700D", "1.1.5") ? 0x3E230 : 0x3E150;
        
        /* I know these look ugly, but we want something works for now, right? , it's not that bad */
        Shift_x5_LCD = is_camera("700D", "1.1.5") ? 0xFF962A74 : 0xFF955894;
        Shift_x5_HDMI_480p = is_camera("700D", "1.1.5") ? 0xFF963434 : 0xFF956254;
        Shift_x5_HDMI_1080i_Full = is_camera("700D", "1.1.5") ? 0xFF963F8C : 0xFF956DAC;
        Shift_x5_HDMI_1080i_Info = is_camera("700D", "1.1.5") ? 0xFF964590 : 0xFF9573B0;

        Clear_Vram_x5_LCD = is_camera("700D", "1.1.5") ? 0xFF962A98 : 0xFF9558B8;
        Clear_Vram_x5_HDMI_480p = is_camera("700D", "1.1.5") ? 0xFF963458 : 0xFF956278;
        Clear_Vram_x5_HDMI_1080i_Full = is_camera("700D", "1.1.5") ? 0xFF963FB0 : 0xFF956DD0;
        Clear_Vram_x5_HDMI_1080i_Info = is_camera("700D", "1.1.5") ? 0xFF9645B4 : 0xFF9573D4;
        
        is_650D = 1;
        is_700D = 1;
        is_DIGIC_5 = 1;
        crop_presets                = crop_presets_DIGIC_5;
        crop_rec_menu[0].choices    = crop_choices_DIGIC_5;
        crop_rec_menu[0].max        = COUNT(crop_choices_DIGIC_5) - 1;
        crop_rec_menu[0].help       = crop_choices_help_DIGIC_5;
    }
    else if (is_camera("100D", "1.0.1"))
    {
        CMOS_WRITE = 0x475B8;
        MEM_CMOS_WRITE = 0xE92D41F0;
        
        ADTG_WRITE = 0x47144;
        MEM_ADTG_WRITE = 0xE92D43F8;
        
        ENGIO_WRITE = 0xFF2B2460;
        MEM_ENGIO_WRITE = 0xE51FC15C;
        
        ENG_DRV_OUT = 0xFF2B2148;
        ENG_DRV_OUTS = 0xFF2B226C;
        
        PathDriveMode = (void *) 0xAAEA4;   /* argument of PATH_SelectPathDriveMode */
        PATH_SelectPathDriveMode = 0x19E30; // it's being called from RAM
        
        EDMAC_9_Vertical_1 = 0x77170;
        EDMAC_9_Vertical_2 = 0x771A0;
        
        Shift_x5_LCD = 0xFF98F5EC;
        Shift_x5_HDMI_480p = 0xFF98FFAC;
        Shift_x5_HDMI_1080i_Full = 0xFF990B04;
        Shift_x5_HDMI_1080i_Info = 0xFF991108;

        Clear_Vram_x5_LCD = 0xFF98F610;
        Clear_Vram_x5_HDMI_480p = 0xFF98FFD0;
        Clear_Vram_x5_HDMI_1080i_Full = 0xFF990B28;
        Clear_Vram_x5_HDMI_1080i_Info = 0xFF99112C;
        
        is_100D = 1;
        is_DIGIC_5 = 1;
        crop_presets                = crop_presets_DIGIC_5;
        crop_rec_menu[0].choices    = crop_choices_DIGIC_5;
        crop_rec_menu[0].max        = COUNT(crop_choices_DIGIC_5) - 1;
        crop_rec_menu[0].help       = crop_choices_help_DIGIC_5;
    }       
    else if (is_camera("6D", "1.1.6"))
    {
        CMOS_WRITE = 0x2420C;
        MEM_CMOS_WRITE = 0xE92D41F0;        
        
        ADTG_WRITE = 0x24108;
        MEM_ADTG_WRITE = 0xE92D41F0;
        
        PathDriveMode = (void *) 0xB5D1C;   /* argument of PATH_SelectPathDriveMode */
        
        is_6D = 1;
        is_basic = 1;
        crop_presets                = crop_presets_basic;
        crop_rec_menu[0].choices    = crop_choices_basic;
        crop_rec_menu[0].max        = COUNT(crop_choices_basic) - 1;
        crop_rec_menu[0].help       = crop_choices_help_basic;
        crop_rec_menu[0].help2      = crop_choices_help2_basic;
        
        fps_main_clock = 25600000;
                                       /* 24p,  25p,  30p,  50p,  60p,   x5 */
        memcpy(default_timerA, (int[]) {  546,  640,  546,  640,  520,  730 }, 24);
        memcpy(default_timerB, (int[]) { 1955, 1600, 1564,  800,  821, 1172 }, 24);
                                   /* or 1956        1565         822        2445        1956 */
    }
    
    /* default FPS timers are the same on all these models */
    if (is_EOSM || is_700D || is_650D || is_100D)
    {
        fps_main_clock = 32000000;
                                       /* 24p,  25p,  30p,  50p,  60p,   x5, c24p, c25p, c30p */
        memcpy(default_timerA, (int[]) {  528,  640,  528,  640,  528,  716,  546,  640,  546 }, 36);
        memcpy(default_timerB, (int[]) { 2527, 2000, 2022, 1000, 1011, 1491, 2444, 2000, 1955 }, 36);
                                   /* or 2528        2023        1012        2445        1956 */
    }

    /* FPS in x5 zoom may be model-dependent; assume exact */
    default_fps_1k[5] = (uint64_t) fps_main_clock * 1000ULL / default_timerA[5] / default_timerB[5];

    printf("[crop_rec] checking FPS timer values...\n");
    for (int i = 0; i < COUNT(default_fps_1k); i++)
    {
        if (default_timerA[i])
        {
            int fps_i = (uint64_t) fps_main_clock * 1000ULL / default_timerA[i] / default_timerB[i];
            if (fps_i == default_fps_1k[i])
            {
                printf("%d) %s%d.%03d: A=%d B=%d (exact)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i]);

                if (i == 5 && default_fps_1k[i] != 29970)
                {
                    printf("-> unusual FPS in x5 zoom\n", i);
                }
            }
            else
            {
                int fps_p = (uint64_t) fps_main_clock * 1000ULL / default_timerA[i] / (default_timerB[i] + 1);
                if (fps_i > default_fps_1k[i] && fps_p < default_fps_1k[i])
                {
                    printf("%d) %s%d.%03d: A=%d B=%d/%d (averaged)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i], default_timerB[i] + 1);
                }
                else
                {
                    printf("%d) %s%d.%03d: A=%d B=%d (%s%d.%03d ?!?)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i], FMT_FIXEDPOINT3(fps_i));
                    return CBR_RET_ERROR;
                }

                /* assume 25p is exact on all models */
                if (i == 1)
                {
                    printf("-> 25p check error\n");
                    return CBR_RET_ERROR;
                }
            }
        }
    }
    
    menu_add("Movie", crop_rec_menu, COUNT(crop_rec_menu));
    lvinfo_add_items (info_items, COUNT(info_items));

    return 0;
}

static unsigned int crop_rec_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(crop_rec_init)
    MODULE_DEINIT(crop_rec_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(crop_preset_index)
    MODULE_CONFIG(shutter_range)
    MODULE_CONFIG(bit_depth_analog)
    MODULE_CONFIG(crop_preset_1x1_res)
    MODULE_CONFIG(crop_preset_1x3_res)
    MODULE_CONFIG(crop_preset_3x3_res)
    MODULE_CONFIG(crop_preset_ar)
    MODULE_CONFIG(crop_preset_fps)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, crop_rec_polling_cbr, 0)
    MODULE_CBR(CBR_RAW_INFO_UPDATE, raw_info_update_cbr, 0)
MODULE_CBRS_END()

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_LV_ACTION)
    MODULE_PROPHANDLER(PROP_LV_DISPSIZE)
MODULE_PROPHANDLERS_END()
