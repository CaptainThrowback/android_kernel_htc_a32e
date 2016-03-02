#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <linux/debug_display.h>
#include <linux/htc_flags.h>
#include "../../../../drivers/video/msm/mdss/mdss_dsi.h"

#define PANEL_ID_A32E_EVM_TIANMA_HX8394D  1
#define PANEL_ID_A32E_TIANMA_HX8394D      2
#define PANEL_ID_A32E_TRULY_HX8394D       3

#ifdef  CONFIG_HTC_PNPMGR
extern void set_screen_status(bool onoff);
#endif

struct dsi_power_data {
	uint32_t sysrev;          
	struct regulator *vddio;  
	int lcmio_en;             
	int lcmp5v;               
	int lcmn5v;               
	int lcm_bl_en;
};

#ifdef MODULE
extern struct module __this_module;
#define THIS_MODULE (&__this_module)
#else
#define THIS_MODULE ((struct module *)0)
#endif

static struct kobject *android_disp_kobj;
static uint8_t panel_id;
const char bootstr[] = "offmode_charging";

struct lcd_vendor_info {
	int idx;
	char vendor[64];
};

struct lcd_vendor_info lcd_name[] = {
	{0, "null panel"},
	{1, "EVM Tianma HX8394D"},
	{2, "Tianma HX8394D"},
	{3, "Truly HX8394D"},
};

static ssize_t disp_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", lcd_name[panel_id].vendor);

	return ret;
}
static DEVICE_ATTR(vendor, S_IRUGO, disp_vendor_show, NULL);

void htc_display_sysfs_init(bool status)
{
	if (status) {
		android_disp_kobj = kobject_create_and_add("android_display", NULL);
		if (!android_disp_kobj)
			PR_DISP_ERR("%s: subsystem register failed\n", __func__);

		if (sysfs_create_file(android_disp_kobj, &dev_attr_vendor.attr))
			PR_DISP_ERR("Fail to create sysfs file (vendor)\n");
	} else {
		sysfs_remove_file(android_disp_kobj, &dev_attr_vendor.attr);
		kobject_del(android_disp_kobj);
	}
}

static struct i2c_client *tps65132_client = NULL;

struct i2c_dev_info {
	uint8_t	dev_addr;
	struct i2c_client *client;
};

#define TPS65132_I2C_ADDRESS       0x3E
#define TPS65132_VPOS_ADDRESS      0x00
#define TPS65132_VNEG_ADDRESS      0x01
#define TPS65132_APPS_DIS_ADDRESS  0x03

#define I2C_DEV_INFO(addr) \
	{.dev_addr = addr >> 1, .client = NULL}

static uint8_t tps65132_cmd[3][2] = {
	
	{TPS65132_VPOS_ADDRESS, 0x0F},
	{TPS65132_VNEG_ADDRESS, 0x0F},
	
	{TPS65132_APPS_DIS_ADDRESS, 0x43},
};

static struct i2c_dev_info tps65132_addresses[] = {
	I2C_DEV_INFO(TPS65132_I2C_ADDRESS)
};

static int i2c_txdata(struct i2c_client *i2c, uint8_t *txData, int length)
{
	int ret = 0, retry = 10, i;
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.buf = txData,
			.len = length,
		},
	};

	for (i = 0; i < retry; i++) {
		ret = i2c_transfer(i2c->adapter, msg, ARRAY_SIZE(msg));
		if (ret == ARRAY_SIZE(msg))
			break;
		else if (ret < 0) {
			dev_err(&i2c->dev, "[DISP] %s: transfer failed. retry: %d", __func__, i);
			usleep_range(2000, 4000);
		}
	}
	if (i == retry) {
		PR_DISP_ERR("i2c_write_block retry over %d times\n", i);
		return -EIO;
	}

	dev_vdbg(&i2c->dev, "TxData: len = %02x, addr = %02x data = %02x",
		length, txData[0], txData[1]);

	return 0;
}

static void tps65132_boost_on(void)
{
	int ret = 0;

	ret = i2c_txdata(tps65132_client, tps65132_cmd[0] , 2);
	usleep_range(1500, 2000);
	ret = i2c_txdata(tps65132_client, tps65132_cmd[1] , 2);
	if (ret)
		PR_DISP_ERR("%s: Boost to 5.5V FAIL\n", __func__);
	ret = i2c_txdata(tps65132_client, tps65132_cmd[2] , 2);
	if (ret)
		PR_DISP_ERR("%s: Set Tablet mode FAIL\n", __func__);
}

static int tps65132_add_i2c(struct i2c_client *client)
{
	int idx = 0;

	tps65132_client = client;
	if (tps65132_client == NULL) {
		PR_DISP_ERR("%s() failed to get i2c adapter tps65132_client\n", __func__);
		return -ENODEV;
	}

	for (idx = 0; idx < ARRAY_SIZE(tps65132_addresses); idx++) {
		if (idx == 0)
			tps65132_addresses[idx].client = client;
		else {
			tps65132_addresses[idx].client =
				i2c_new_dummy(client->adapter, tps65132_addresses[idx].dev_addr);

			if (tps65132_addresses[idx].client == NULL)
				return -ENODEV;
		}
	}

	return 0;
}

static int tps65132_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		PR_DISP_ERR("%s: Failed to i2c_check_functionality \n", __func__);
		return -EIO;
	}
	if (!client->dev.of_node) {
		PR_DISP_ERR("%s: client->dev.of_node = NULL\n", __func__);
		return -ENOMEM;
	}

	ret = tps65132_add_i2c(client);
	if(ret < 0) {
		PR_DISP_ERR("%s: Failed to tps65132_add_i2c, ret=%d\n", __func__,ret);
		return ret;
	}

	return 0;
}

static const struct i2c_device_id tps65132_id[] = {
	{"tps_lcm_boost65132", 0}
};

static struct of_device_id tsp_match_table[] = {
	{.compatible = "disp-tps-65132",}
};

static struct i2c_driver tps65132_i2c_driver = {
	.driver = {
		.owner          = THIS_MODULE,
		.name           = "tps_lcm_boost_65132",
		.of_match_table = tsp_match_table,
		},
	.id_table = tps65132_id,
	.probe    = tps65132_i2c_probe,
	.command  = NULL,
};

static int __init tps65132_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&tps65132_i2c_driver);
	if (ret < 0)
		PR_DISP_ERR("%s: Failed to Register I2C driver, boost_voltage. ret=%d\n",
				__func__, ret);

	return ret;
}

static int htc_a32e_regulator_init(struct platform_device *pdev)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;

	PR_DISP_INFO("%s\n", __func__);
	if (!pdev) {
		PR_DISP_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		PR_DISP_ERR("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	pwrdata = devm_kzalloc(&pdev->dev,
				sizeof(struct dsi_power_data), GFP_KERNEL);
	if (!pwrdata) {
		PR_DISP_ERR("%s: FAILED to alloc pwrdata\n", __func__);
		return -ENOMEM;
	}

	
	pwrdata->vddio = regulator_get(NULL, "ncp6924_dcdc2");
	if (IS_ERR(pwrdata->vddio)) {
		PR_DISP_ERR("%s: could not get vddio vreg, rc=%ld\n",
			__func__, PTR_ERR(pwrdata->vddio));
		return PTR_ERR(pwrdata->vddio);
	}

	ret = regulator_set_voltage(pwrdata->vddio, 3100000, 3100000);
	if (ret) {
		PR_DISP_ERR("%s: set voltage failed on vddio vreg, rc=%d\n",
			__func__, ret);
		return ret;
	}

	ret = regulator_enable(pwrdata->vddio);
	if (ret) {
		PR_DISP_ERR("%s: enable voltage failed on vddio vreg, rc=%d\n", __func__, ret);
		return ret;
	}

	ctrl_pdata->dsi_pwrctrl_data = pwrdata;

	pwrdata->lcmio_en = of_get_named_gpio(pdev->dev.of_node,
						"htc,lcm_1v8_en-gpio", 0);
	pwrdata->lcmp5v = of_get_named_gpio(pdev->dev.of_node,
						"htc,lcm_p5v-gpio", 0);
	pwrdata->lcmn5v = of_get_named_gpio(pdev->dev.of_node,
						"htc,lcm_n5v-gpio", 0);
	pwrdata->lcm_bl_en = of_get_named_gpio(pdev->dev.of_node,
						"htc,lcm_bl_en-gpio", 0);
	if (strcmp(htc_get_bootmode(), bootstr))
		ret = tps65132_init();
	else
		PR_DISP_ERR("Recovery mode, discard boost voltage driver");

	panel_id = ctrl_pdata->panel_data.panel_info.panel_id;
	if (strcmp(htc_get_bootmode(), bootstr))
		htc_display_sysfs_init(1);

	return ret;
}

static int htc_a32e_regulator_deinit(struct platform_device *pdev)
{

	if (strcmp(htc_get_bootmode(), bootstr))
		htc_display_sysfs_init(0);

	return 0;
}

void htc_a32e_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;

	if (pdata == NULL) {
		PR_DISP_ERR("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pwrdata = ctrl_pdata->dsi_pwrctrl_data;

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		PR_DISP_DEBUG("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	PR_DISP_DEBUG("%s: enable = %d\n", __func__, enable);

	if (enable) {
		if (pdata->panel_info.first_power_on == 1) {
			PR_DISP_INFO("reset already on in first time\n");
			return;
		}

		if (pdata->panel_info.panel_id == PANEL_ID_A32E_EVM_TIANMA_HX8394D) {
			
			gpio_set_value(ctrl_pdata->rst_gpio, 1);
			usleep_range(5000, 5500);

			
			gpio_set_value(pwrdata->lcmp5v, 1);
			usleep_range(10000, 10500);
			
			gpio_set_value(pwrdata->lcmn5v, 1);
			usleep_range(2000, 2500);
			if (strcmp(htc_get_bootmode(), bootstr))
				tps65132_boost_on();
			else
				PR_DISP_ERR("Recovery mode, discard send boost i2c command");
			msleep(150);
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TIANMA_HX8394D) {
			
			gpio_set_value(ctrl_pdata->rst_gpio, 0);
			usleep_range(1000, 1500);
			gpio_set_value(ctrl_pdata->rst_gpio, 1);
			usleep_range(5000, 5500);

			
			gpio_set_value(pwrdata->lcmp5v, 1);
			usleep_range(10000, 10500);
			
			gpio_set_value(pwrdata->lcmn5v, 1);
			usleep_range(2000, 2500);
			if (strcmp(htc_get_bootmode(), bootstr))
				tps65132_boost_on();
			else
				PR_DISP_ERR("Recovery mode, discard send boost i2c command");
			msleep(150);
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TRULY_HX8394D) {
			
			gpio_set_value(ctrl_pdata->rst_gpio, 0);
			usleep_range(1000, 1500);
			gpio_set_value(ctrl_pdata->rst_gpio, 1);
			usleep_range(5000, 5500);

			
			gpio_set_value(pwrdata->lcmp5v, 1);
			usleep_range(10000, 10500);
			
			gpio_set_value(pwrdata->lcmn5v, 1);
			usleep_range(2000, 2500);
			if (strcmp(htc_get_bootmode(), bootstr))
				tps65132_boost_on();
			else
				PR_DISP_ERR("Recovery mode, discard send boost i2c command");
			msleep(180);
		}

		gpio_set_value(pwrdata->lcm_bl_en, 1);
	} else {
		gpio_set_value(pwrdata->lcm_bl_en, 0);

		if (pdata->panel_info.panel_id == PANEL_ID_A32E_EVM_TIANMA_HX8394D) {
			gpio_set_value(ctrl_pdata->rst_gpio, 0);
			usleep_range(2000, 2500);
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TIANMA_HX8394D) {
			gpio_set_value(ctrl_pdata->rst_gpio, 0);
			usleep_range(5000, 5500);
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TRULY_HX8394D) {
			gpio_set_value(ctrl_pdata->rst_gpio, 0);
			usleep_range(5000, 5500);
		}
	}

	PR_DISP_INFO("%s: enable = %d done\n", __func__, enable);
}

static int htc_a32e_panel_power_on(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_power_data *pwrdata = NULL;
	int ret = 0;

	PR_DISP_INFO("%s: enable = %d\n", __func__, enable);

	if (pdata == NULL) {
		PR_DISP_ERR("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pwrdata = ctrl_pdata->dsi_pwrctrl_data;

	if (!pwrdata) {
		PR_DISP_ERR("%s: pwrdata not initialized\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (pdata->panel_info.first_power_on == 1) {
			PR_DISP_INFO("power already on in first time\n");
			return ret;
		}

		if (pdata->panel_info.panel_id == PANEL_ID_A32E_EVM_TIANMA_HX8394D) {
			
			ret = regulator_enable(pwrdata->vddio);
			if (ret) {
				PR_DISP_ERR("%s: regulator_enable (ncp6924) failed (%d)\n", __func__, ret);
				return ret;
			}
			gpio_set_value(pwrdata->lcmio_en, 1);
			
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TIANMA_HX8394D) {
			
			ret = regulator_enable(pwrdata->vddio);
			if (ret) {
				PR_DISP_ERR("%s: regulator_enable (ncp6924) failed (%d)\n", __func__, ret);
				return ret;
			}
			gpio_set_value(pwrdata->lcmio_en, 1);
			
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TRULY_HX8394D) {
			
			ret = regulator_enable(pwrdata->vddio);
			if (ret) {
				PR_DISP_ERR("%s: regulator_enable (ncp6924) failed (%d)\n", __func__, ret);
				return ret;
			}
			gpio_set_value(pwrdata->lcmio_en, 1);
			
		}
	} else {
		if (pdata->panel_info.panel_id == PANEL_ID_A32E_EVM_TIANMA_HX8394D) {
			gpio_set_value(pwrdata->lcmn5v, 0);
			usleep_range(2000, 2500);

			gpio_set_value(pwrdata->lcmp5v, 0);
			usleep_range(2000, 2500);

			gpio_set_value(pwrdata->lcmio_en, 0);
			ret = regulator_disable(pwrdata->vddio);
			if (ret) {
				PR_DISP_ERR("%s: Falied to disable vddio regulator (ncp6924). ret = %d\n"
					, __func__, ret);
				return ret;
			}
			usleep_range(1000, 1500);
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TIANMA_HX8394D) {
			gpio_set_value(pwrdata->lcmn5v, 0);
			usleep_range(10000, 10500);
			gpio_set_value(pwrdata->lcmp5v, 0);
			usleep_range(10000, 10500);

			gpio_set_value(pwrdata->lcmio_en, 0);
			ret = regulator_disable(pwrdata->vddio);
			if (ret) {
				PR_DISP_ERR("%s: Falied to disable vddio regulator (ncp6924). ret = %d\n"
					, __func__, ret);
				return ret;
			}
			usleep_range(1000, 1500);
		} else if (pdata->panel_info.panel_id == PANEL_ID_A32E_TRULY_HX8394D) {
			gpio_set_value(pwrdata->lcmn5v, 0);
			usleep_range(10000, 10500);
			gpio_set_value(pwrdata->lcmp5v, 0);
			usleep_range(10000, 10500);

			gpio_set_value(pwrdata->lcmio_en, 0);
			ret = regulator_disable(pwrdata->vddio);
			if (ret) {
				PR_DISP_ERR("%s: Falied to disable vddio regulator (ncp6924). ret = %d\n"
					, __func__, ret);
				return ret;
			}
			usleep_range(1000, 1500);
		}
	}

	PR_DISP_INFO("%s: enable = %d done\n", __func__, enable);

#ifdef  CONFIG_HTC_PNPMGR
	
	if (enable)
		set_screen_status(true);
	else
		set_screen_status(false);
#endif

	return ret;
}

static struct mdss_dsi_pwrctrl dsi_pwrctrl = {
	.dsi_regulator_init = htc_a32e_regulator_init,
	.dsi_regulator_deinit = htc_a32e_regulator_deinit,
	.dsi_power_on = htc_a32e_panel_power_on,
	.dsi_panel_reset = htc_a32e_panel_reset,
};

static struct platform_device dsi_pwrctrl_device = {
	.name          = "mdss_dsi_pwrctrl",
	.id            = -1,
	.dev.platform_data = &dsi_pwrctrl,
};

int __init htc_8909_dsi_panel_power_register(void)
{
	PR_DISP_INFO("%s#%d\n", __func__, __LINE__);
	platform_device_register(&dsi_pwrctrl_device);

	return 0;
}

arch_initcall(htc_8909_dsi_panel_power_register);
