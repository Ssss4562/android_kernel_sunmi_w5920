#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#include <string.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

/* Sunmi V1s-G (w5920) panel: xc_ili9881c_dsi_vdo_dijing_2, 720x1280.
 * Init/suspend tables extracted from stock LK (see lcm_tables.h).
 * Panel power (DSV_EN) is GPIO21, reset via standard set_reset_pin. */

#define FRAME_WIDTH  (720)
#define FRAME_HEIGHT (1280)

#define LCM_ID_ILI9881  0x9881

#define GPIO_LCM_EN  21

#define REGFLAG_DELAY  0xFE
#define REGFLAG_END_OF_TABLE  0xFD

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)  (lcm_util.set_reset_pin((v)))
#define MDELAY(n)  (lcm_util.mdelay(n))
#define UDELAY(n)  (lcm_util.udelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define LCM_DSI_CMD_MODE  0

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

#ifndef BUILD_LK
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
#endif

#include "lcm_tables.h"

static void push_table(struct LCM_setting_table *table,
		unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			dsi_set_cmdq_V2(cmd, table[i].count,
					table[i].para_list, force_update);
			break;
		}
	}
}

/* ---------------------------------------------------------------------------
 * LCM Driver Implementations
 * --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
	memset(params, 0, sizeof(LCM_PARAMS));

	params->type = LCM_TYPE_DSI;
	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	params->physical_width = 62.10;
	params->physical_height = 110.40;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode = CMD_MODE;
#else
	params->dsi.mode = SYNC_PULSE_VDO_MODE;
#endif

	params->dsi.LANE_NUM = LCM_THREE_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	params->dsi.packet_size = 256;
	params->dsi.intermediat_buffer_num = 2;
	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active = 5;
	params->dsi.vertical_backporch = 18;
	params->dsi.vertical_frontporch = 10;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 40;
	params->dsi.horizontal_backporch = 60;
	params->dsi.horizontal_frontporch = 60;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 260;
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
#if (LCM_DSI_CMD_MODE)
	params->dsi.fbk_div = 7;
#else
	params->dsi.fbk_div = 7;
#endif
	params->dsi.ssc_disable = 1;
	params->dsi.noncont_clock = 1;
	params->dsi.noncont_clock_period = 1;

	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}

static void lcm_set_gpio_output(unsigned int pin, unsigned int out)
{
#ifdef BUILD_LK
	(void)pin;
	(void)out;
#else
	mt_set_gpio_mode(pin, GPIO_MODE_00);
	mt_set_gpio_dir(pin, GPIO_DIR_OUT);
	mt_set_gpio_out(pin, out ? GPIO_OUT_ONE : GPIO_OUT_ZERO);
#endif
}

static void lcm_init(void)
{
	lcm_set_gpio_output(GPIO_LCM_EN, 1);

	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(50);
	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(lcm_initialization_setting,
		sizeof(lcm_initialization_setting) /
		sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	push_table(lcm_suspend_setting,
		sizeof(lcm_suspend_setting) /
		sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
}

static void lcm_resume(void)
{
	lcm_init();
}

static unsigned int lcm_compare_id(void)
{
	int array[4];
	unsigned char buffer[3] = {0, 0, 0};
	unsigned int id;
#ifndef BUILD_LK
	int data[4] = {0, 0, 0, 0};
	int rawdata = 0;
#endif

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(50);

	lcm_set_gpio_output(GPIO_LCM_EN, 1);

	array[0] = 0x00043902;
	array[1] = 0x018198FF;
	dsi_set_cmdq(array, 2, 1);

	MDELAY(10);
	array[0] = 0x00023700;
	dsi_set_cmdq(array, 1, 1);

	MDELAY(10);
	read_reg_v2(0, buffer, 1);
	read_reg_v2(1, buffer + 1, 1);
	read_reg_v2(2, buffer + 2, 1);
	id = buffer[0] | (buffer[1] << 8);

#ifdef BUILD_LK
	printf("dijing_2 %s: id = 0x%08x\n", __func__, id);
#else
	pr_debug("dijing_2 %s: id = 0x%08x\n", __func__, id);
#endif

	if (LCM_ID_ILI9881 == id)
		return 1;

#ifndef BUILD_LK
	/* Fallback: panel ID pin strapped to AUXADC channel 1,
	 * stock LK accepts raw 201..399. */
	if (IMM_GetOneChannelValue(1, data, &rawdata) >= 0 &&
			rawdata >= 201 && rawdata <= 399)
		return 1;
#endif

	return 0;
}

static unsigned int lcm_esd_check(void)
{
#ifndef BUILD_LK
	char buffer[3] = {0, 0, 0};
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0A, buffer, 1);
	if (buffer[0] == 0x9C)
		return FALSE;
	else
		return TRUE;
#else
	return FALSE;
#endif
}

static unsigned int lcm_esd_recover(void)
{
	lcm_init();
	return TRUE;
}

LCM_DRIVER xc_ili9881c_dsi_vdo_dijing_2_lcm_drv = {
	.name = "xc_ili9881c_dsi_vdo_dijing_2",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.esd_check = lcm_esd_check,
	.esd_recover = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
	.update = lcm_update,
#endif
};
