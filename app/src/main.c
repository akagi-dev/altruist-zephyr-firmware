#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <altruist/identity.h>

int main(void)
{
	int rc;

	printk("%s\n", CONFIG_ALTRUIST_BOOT_MESSAGE);
	printk("Running on board: %s\n", CONFIG_BOARD);

	if (IS_ENABLED(CONFIG_ALTRUIST_IDENTITY)) {
		rc = altruist_identity_init();
		if (rc != 0) {
			printk("Identity initialization failed (altruist_identity_init): %d\n", rc);
		}
	}

	return 0;
}
