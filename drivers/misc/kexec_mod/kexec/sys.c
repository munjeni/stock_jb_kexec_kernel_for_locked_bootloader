/*
 *  linux/kernel/sys.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/utsname.h>
#include <linux/mman.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/prctl.h>
#include <linux/highuid.h>
#include <linux/fs.h>
#include <linux/perf_event.h>
#include <linux/resource.h>
#include <linux/kernel.h>
#include <linux/kexec.h>
#include <linux/workqueue.h>
#include <linux/capability.h>
#include <linux/device.h>
#include <linux/key.h>
#include <linux/times.h>
#include <linux/posix-timers.h>
#include <linux/security.h>
#include <linux/dcookies.h>
#include <linux/suspend.h>
#include <linux/tty.h>
#include <linux/signal.h>
#include <linux/cn_proc.h>
#include <linux/getcpu.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/seccomp.h>
#include <linux/cpu.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/fs_struct.h>
#include <linux/gfp.h>
#include <linux/syscore_ops.h>
#include <linux/version.h>
#include <linux/ctype.h>

#include <linux/compat.h>
#include <linux/syscalls.h>
#include <linux/kprobes.h>
#include <linux/user_namespace.h>

#include <linux/kmsg_dump.h>
/* Move somewhere else to avoid recompiling? */
#include <generated/utsrelease.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/unistd.h>

static DEFINE_MUTEX(kexec_reboot_mutex);
BLOCKING_NOTIFIER_HEAD(reboot_notifier_list);

/*
 * Reboot system call: for obvious reasons only root may call it,
 * and even root needs to set up some magic numbers in the registers
 * so that some mistake won't make this reboot the whole machine.
 * You can also set the meaning of the ctrl-alt-del-key here.
 *
 * reboot doesn't sync: do that yourself before calling this.
 */
asmlinkage long kexec_call_reboot(int magic1, int magic2, unsigned int cmd, void __user *arg)
{
	char buffer[256];
	int ret = 0;

	/* We only trust the superuser with rebooting the system. */
	if (!capable(CAP_SYS_BOOT))
		return -EPERM;

	/* For safety, we require "magic" arguments. */
	if (magic1 != LINUX_REBOOT_MAGIC1 ||
	    (magic2 != LINUX_REBOOT_MAGIC2 &&
	                magic2 != LINUX_REBOOT_MAGIC2A &&
			magic2 != LINUX_REBOOT_MAGIC2B &&
	                magic2 != LINUX_REBOOT_MAGIC2C))
		return -EINVAL;

	/* Instead of trying to make the power_off code look like
	 * halt when pm_power_off is not set do it the easy way.
	 */
	if ((cmd == LINUX_REBOOT_CMD_POWER_OFF) && !pm_power_off)
		cmd = LINUX_REBOOT_CMD_HALT;

	mutex_lock(&kexec_reboot_mutex);
	switch (cmd) {
	case LINUX_REBOOT_CMD_RESTART:
		kernel_restart(NULL);
		break;

	case LINUX_REBOOT_CMD_HALT:
		kernel_halt();
		do_exit(0);
		panic("cannot halt");

	case LINUX_REBOOT_CMD_POWER_OFF:
		kernel_power_off();
		do_exit(0);
		break;

	case LINUX_REBOOT_CMD_RESTART2:
		if (strncpy_from_user(&buffer[0], arg, sizeof(buffer) - 1) < 0) {
			ret = -EFAULT;
			break;
		}
		buffer[sizeof(buffer) - 1] = '\0';

		kernel_restart(buffer);
		break;

	case LINUX_REBOOT_CMD_KEXEC:
		ret = kernel_kexec();
		break;

#ifdef CONFIG_HIBERNATION
	case LINUX_REBOOT_CMD_SW_SUSPEND:
		ret = hibernate();
		break;
#endif

	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&kexec_reboot_mutex);
	return ret;
}
