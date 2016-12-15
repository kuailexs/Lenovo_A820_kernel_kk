#include <linux/string.h>
#ifndef BUILD_UBOOT
#include <linux/kernel.h>
#endif
#include "lcm_drv.h"
#if defined(BUILD_UBOOT)
    #include <asm/arch/mt_gpio.h>
#else
    #include <mach/mt_gpio.h>
#endif

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (540)
#define FRAME_HEIGHT (960)
#define LCM_DSI_CMD_MODE

#define GPIO_LCM_ID_PIN                                     GPIO154

#define REGFLAG_DELAY                                       0xFE
#define REGFLAG_END_OF_TABLE                                0x00   // END OF REGISTERS MARKER

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))

#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)       lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                                  lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)              lcm_util.dsi_write_regs(addr, pdata, byte_nums)
//#define read_reg                                          lcm_util.dsi_read_reg()
#define read_reg_v2(cmd, buffer, buffer_size)                   lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};


static struct LCM_setting_table lcm_initialization_setting[] = {

    /*
    Note :

    Data ID will depends on the following rule.

        count of parameters > 1 => Data ID = 0x39
        count of parameters = 1 => Data ID = 0x15
        count of parameters = 0 => Data ID = 0x05

    Structure Format :

    {DCS command, count of parameters, {parameter list}}
    {REGFLAG_DELAY, milliseconds of time, {}},

    ...

    Setting ending by predefined flag

    {REGFLAG_END_OF_TABLE, 0x00, {}}
    */

    {0x11,0,{}},
    {REGFLAG_DELAY, 200, {}},
    {0x35,1,{0}},
    // CABC start
    {0x51,1,{0xFF}},
    {0x53,1,{0x2C}},
    {0x55,1,{0x1}},
    {0x5E,1,{0x3F}},
    //CABC  end
    {0x29,0,{}},
    {REGFLAG_DELAY, 100, {}},


    // Note
    // Strongly recommend not to set Sleep out / Display On here. That will cause messed frame to be shown as later the backlight is on.


    // Setting ending by predefined flag
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_sleep_out_setting[] = {
    // Sleep Out
    {0x11, 1, {0x00}},
    {REGFLAG_DELAY, 20, {}},

    // Display ON
    {0x29, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_mode_in_setting[] = {
    // Display off sequence
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 200, {}},

    // Sleep Mode On
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_compare_id_setting[] = {
    // Display off sequence
    {0xF0,  5,  {0x55, 0xaa, 0x52,0x08,0x00}},
    {REGFLAG_DELAY, 10, {}},

    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for(i = 0; i < count; i++) {

        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {

            case REGFLAG_DELAY :
                MDELAY(table[i].count);
                break;

            case REGFLAG_END_OF_TABLE :
                break;

            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
                MDELAY(2);
        }
    }

}


// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DSI;

    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

#if defined(TEST)
    // enable tearing-free
    params->dbi.te_mode                 = LCM_DBI_TE_MODE_DISABLED;
    params->dbi.te_edge_polarity        = LCM_POLARITY_RISING;
#endif

#if defined(LCM_DSI_CMD_MODE)
    params->dsi.mode   = CMD_MODE;
#else
    params->dsi.mode   = SYNC_PULSE_VDO_MODE;
#endif

    // DSI
    /* Command mode setting */
    params->dsi.LANE_NUM                = LCM_TWO_LANE;
    //The following defined the format for data coming from LCD engine.
    params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
    params->dsi.PS                      = LCM_PACKED_PS_24BIT_RGB888;

#if defined(TEST)
    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;

    //params->dsi.compatibility_for_nvk=1;
    params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
#endif

#if defined(TEST)
    params->dsi.word_count                  = 540*3;

    params->dsi.vertical_sync_active        = 0;  //---3
    params->dsi.vertical_backporch          = 10; //---14
    params->dsi.vertical_frontporch         = 44;  //----8
    params->dsi.vertical_active_line        = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active      = 0;  //----2
    params->dsi.horizontal_backporch        = 60; //----28
    params->dsi.horizontal_frontporch       = 20; //----50
    params->dsi.horizontal_active_pixel     = FRAME_WIDTH;
#endif

    // Bit rate calculation
    //1 Every lane speed
    //params->dsi.pll_select=1; //0: MIPI_PLL; 1: LVDS_PLL
    params->dsi.PLL_CLOCK = LCM_DSI_6589_PLL_CLOCK_227_5;

#if 0
    params->dsi.pll_div1    = 0x00;     // div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
    params->dsi.pll_div2    = 0x1;      // div2=0,1,2,3;div1_real=1,2,4,4
    params->dsi.fbk_div     = 0x12;     // fref=26MHz, fvco=fref*(fbk_div+1)*fbk_sel_real/(div1_real*div2_real)
    params->dsi.fbk_sel     = 0x1;      // fbk_sel=0,1,2,3;fbk_select_real=1,2,4,4
    params->dsi.rg_bir      = 0x5;
    params->dsi.rg_bic      = 0x2;
    params->dsi.rg_bp       = 0xC;
#else
    params->dsi.pll_div1    = 0x1;      // div1=0,1,2,3;div1_real=1,2,4,4 ----0: 546Mbps  1:273Mbps
    params->dsi.pll_div2    = 0x1;      // div2=0,1,2,3;div1_real=1,2,4,4
    params->dsi.fbk_div     = 0x11;      // fref=26MHz, fvco=fref*(fbk_div+1)*fbk_sel_real/(div1_real*div2_real)
    params->dsi.fbk_sel     = 0x1;      // fbk_sel=0,1,2,3;fbk_select_real=1,2,4,4
    params->dsi.rg_bir      = 0x5;
    params->dsi.rg_bic      = 0x2;
    params->dsi.rg_bp       = 0xC;
#endif
    /* ESD or noise interference recovery For video mode LCM only. */ // Send TE packet to LCM in a period of n frames and check the response.
    //params->dsi.lcm_int_te_monitor = 0;
    //params->dsi.lcm_int_te_period = 1; // Unit : frames

    // Need longer FP for more opportunity to do int. TE monitor applicably.
    //if(params->dsi.lcm_int_te_monitor)
    //  params->dsi.vertical_frontporch *= 2;

    // Monitor external TE (or named VSYNC) from LCM once per 2 sec. (LCM VSYNC must be wired to baseband TE pin.)
    //params->dsi.lcm_ext_te_monitor = 0;
    // Non-continuous clock
    //params->dsi.noncont_clock = 1;
    //params->dsi.noncont_clock_period = 2; // Unit : frames
}

static unsigned int lcm_compare_id(void)
{
    unsigned char lcd_id = 0;
    int   array[4];
    char  buffer[3];
    char  id0=0;
    char  id1=0;
    char  id2=0;


    SET_RESET_PIN(0);
    MDELAY(200);
    SET_RESET_PIN(1);
    MDELAY(200);

    array[0] = 0x00033700;// read id return two byte,version and id
    dsi_set_cmdq(array, 1, 1);

    read_reg_v2(0xDA,buffer, 1);


    array[0] = 0x00033700;// read id return two byte,version and id
    dsi_set_cmdq(array, 1, 1);
    read_reg_v2(0xDB,buffer+1, 1);


    array[0] = 0x00033700;// read id return two byte,version and id
    dsi_set_cmdq(array, 1, 1);
    read_reg_v2(0xDC,buffer+2, 1);

    lcd_id =  mt_get_gpio_in(GPIO_LCM_ID_PIN);

    printk("otm9608a GPIO154 id = 0x%08x\n", lcd_id);

    id0 = buffer[0]; //should be 0x00
    id1 = buffer[1];//should be 0xaa
    id2 = buffer[2];//should be 0x55
#ifdef BUILD_UBOOT
    printf("zhibin uboot %s\n", __func__);
    printf("%s, id0 = 0x%08x\n", __func__, id0);//should be 0x00
    printf("%s, id1 = 0x%08x\n", __func__, id1);//should be 0xaa
    printf("%s, id2 = 0x%08x\n", __func__, id2);//should be 0x55
#else
        //printk("zhibin kernel %s\n", __func__);
#endif

    return lcd_id;
}

static void lcm_init(void)
{

    SET_RESET_PIN(0);
    MDELAY(200);
    SET_RESET_PIN(1);
    MDELAY(200);
    lcm_compare_id();

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_suspend(void)
{
    //push_table(lcm_sleep_mode_in_setting, sizeof(lcm_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
    //SET_RESET_PIN(0);
    //MDELAY(1);
    //SET_RESET_PIN(1);
    unsigned int data_array[2];
#if 1
    //data_array[0] = 0x00000504; // Display Off
    //dsi_set_cmdq(&data_array, 1, 1);
    //MDELAY(100);
    data_array[0] = 0x00280500; // Display Off
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(10);
    data_array[0] = 0x00100500; // Sleep In
    dsi_set_cmdq(&data_array, 1, 1);
    MDELAY(100);
#endif
#ifdef BUILD_UBOOT
    printf("zhibin uboot %s\n", __func__);
#else
    printk("zhibin kernel %s\n", __func__);
#endif
}


static void lcm_resume(void)
{
    //lcm_init();
#ifdef BUILD_UBOOT
    printf("zhibin uboot %s\n", __func__);
#else
    printk("zhibin kernel %s\n", __func__);

#endif
    lcm_init();
    push_table(lcm_sleep_out_setting, sizeof(lcm_sleep_out_setting) / sizeof(struct LCM_setting_table), 1);

}


static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1;

    unsigned char x0_MSB = ((x0>>8)&0xFF);
    unsigned char x0_LSB = (x0&0xFF);
    unsigned char x1_MSB = ((x1>>8)&0xFF);
    unsigned char x1_LSB = (x1&0xFF);
    unsigned char y0_MSB = ((y0>>8)&0xFF);
    unsigned char y0_LSB = (y0&0xFF);
    unsigned char y1_MSB = ((y1>>8)&0xFF);
    unsigned char y1_LSB = (y1&0xFF);

    unsigned int data_array[16];

#ifdef BUILD_UBOOT
    printf("zhibin uboot %s\n", __func__);
#else
    printk("zhibin kernel %s\n", __func__);
#endif

    data_array[0]= 0x00053902;
    data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
    data_array[2]= (x1_LSB);
    data_array[3]= 0x00053902;
    data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
    data_array[5]= (y1_LSB);
    data_array[6]= 0x002c3909;

    dsi_set_cmdq(&data_array, 7, 0);
}

LCM_DRIVER otm9608a_qhd_cmd_lcm_drv =
{
    .name           = "otm9608a_qhd_cmd",
    .set_util_funcs = lcm_set_util_funcs,
    .compare_id     = lcm_compare_id,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
#if defined(LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
};
