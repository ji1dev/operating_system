// linux-5.11.22/keernel/sys_calc_mul.c
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
asmlinkage long sys_calc_mul(int a, int b, int *ans){
	int res = a*b, err;
	printk("result on sys_calc_mul: %d\n", res);
	if((err = copy_to_user(ans, &res, sizeof(int)) > 0)) return -1;
	return 0;
}
SYSCALL_DEFINE3(calc_mul, int, a, int, b, int*, ans){
	return sys_calc_mul(a, b, ans);
}
