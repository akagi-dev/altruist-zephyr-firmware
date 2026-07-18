#include <altruist/provisioning.h>

static bool provisioning_active;

int altruist_provisioning_start_ap_mode(void)
{
	provisioning_active = true;
	return 0;
}

int altruist_provisioning_stop(void)
{
	provisioning_active = false;
	return 0;
}

bool altruist_provisioning_is_active(void)
{
	return provisioning_active;
}
