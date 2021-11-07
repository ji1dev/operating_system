// linux-5.11.22/kernel/sys_getburst.c
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
asmlinkage unsigned long long sys_getburst(void){
	unsigned long long res;
	struct task_struct *ts;
	ts = get_current(); // get pointer of current process
	res = ts->myburst;
	printk("Current PID: %d, Current CPU Burst: %llu\n", ts->pid, res);
	return res;
}
SYSCALL_DEFINE0(getburst){
	return sys_getburst();
}
