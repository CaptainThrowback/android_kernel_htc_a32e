
#if defined(CONFIG_ARCH_MSM8994)
#define NFC_READ_RFSKUID 1
#define NFC_GET_BOOTMODE 1
#elif defined(CONFIG_ARCH_MSM8909)
#define NFC_READ_RFSKUID 1
#define NFC_GET_BOOTMODE 1
#else
#define NFC_READ_RFSKUID 0
#define NFC_GET_BOOTMODE 0
#endif

#define NFC_BOOT_MODE_NORMAL 0
#define NFC_BOOT_MODE_FTM 1
#define NFC_BOOT_MODE_DOWNLOAD 2
#define NFC_BOOT_MODE_OFF_MODE_CHARGING 5





int pn544_htc_check_rfskuid(int in_is_alive);

int pn544_htc_get_bootmode(void);

int pn544_htc_pvdd_on (struct i2c_client *client);

void pn544_off_mode_charging (struct i2c_client *client);

