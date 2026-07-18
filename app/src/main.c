#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <altruist/identity.h>

#if defined(CONFIG_ALTRUIST_CONFIG)
#include <altruist/config.h>
#endif

int main(void)
{
	int rc = 0;

#if defined(CONFIG_ALTRUIST_CONFIG)
	rc = altruist_config_init();
#endif

	printk("%s\n", CONFIG_ALTRUIST_BOOT_MESSAGE);
#if defined(CONFIG_ALTRUIST_CONFIG)
	if (rc < 0) {
		printk("Config init failed: %d\n", rc);
	}
#endif
	printk("Running on board: %s\n", CONFIG_BOARD);

	if (IS_ENABLED(CONFIG_ALTRUIST_IDENTITY)) {
		rc = altruist_identity_init();
		if (rc != 0) {
			printk("Identity initialization failed (altruist_identity_init): %d\n", rc);
		}
	}

	return 0;
}
