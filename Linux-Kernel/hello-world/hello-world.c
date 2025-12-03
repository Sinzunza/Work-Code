#include <linux/module.h>

static int my_init(void) {
    printk("Hello - Hello, Kernel!\n");
    return 0;
}

static void my_exit(void) {
    printk("Hello - Goodbye, Kernel!\n");
}

module_init(my_init);
module_exit(my_exit);

// This is not necessarily required, however, will give warning and kernel will mark as tained. Also won't be able to use certain kernel core code.
MODULE_LICENSE("GPL");
