#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#if defined(CONFIG_ALTRUIST_CONFIG)
#include <altruist/config.h>
#endif

int main(void)
{
#if defined(CONFIG_ALTRUIST_CONFIG)
	int rc = altruist_config_init();
#endif

	printk("%s\n", CONFIG_ALTRUIST_BOOT_MESSAGE);
#if defined(CONFIG_ALTRUIST_CONFIG)
	if (rc < 0) {
		printk("Config init failed: %d\n", rc);
	}
#endif
	printk("Running on board: %s\n", CONFIG_BOARD);
	return 0;
}
