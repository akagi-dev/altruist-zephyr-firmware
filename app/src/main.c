#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
	printk("%s\n", CONFIG_ALTRUIST_BOOT_MESSAGE);
	printk("Running on board: %s\n", CONFIG_BOARD);
	return 0;
}
