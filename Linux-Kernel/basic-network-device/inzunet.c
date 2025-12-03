// Day 1 - Basic network device. We will create a virtual network device which is more basic and easier to implement than a real network device.
// Day 2 - Adds atomic counters and /proc stats
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h> // provides net_device struct and related functionality
#include <linux/etherdevice.h> // provides ether_setup
#include <linux/skbuff.h> // provides skbuff struct and related functionality
#include <linux/if_ether.h> // provides ntohs, specifically defines Ethernet protocol constants and structures
#include <linux/proc_fs.h> // provides proc_create and remove_proc_entry, api for creating and managing /proc filesystem entries
#include <linux/seq_file.h> // provides seq_file, api for seq_file which is convenient way for kernel modules to implement readable /proc or /sys files
#include <linux/atomic.h> // provides atomic code, api for atomic operations which allow safely reading, writing, and modifying concurrently without using locks
// Note: A lot of these header files' code is indirectly included from other already included header files, however, standard is to always include all the
// header files you are using explicitly.

#define PROC_NAME "inzunet_stats" // defines name of proc file, it will appear as /proc/inzunet_stats

// we define this structure and will use it for the net_device private area below
struct inzunet_priv {
        atomic64_t tx_packets;
        atomic64_t tx_bytes;
};

// net_device struct, is the standard way of representing network devices
// you don't allocate this manually (you can but you shouldn't), you use alloc_netdev. alloc_netdev takes care of correctly allocating the net_device struct.
static struct net_device *inzunet_dev;
static struct proc_dir_entry *inzunet_proc_entry; // holds to /proc/inzunet_stats file entry for cleanup

// Create functions for net_device_ops, recall that net_device_ops manages the callback functions for the net_device's operations
static int inzunet_open(struct net_device *dev)
{
    netif_start_queue(dev); // starts transmit queue, defined in netdevice.h
    pr_info("inzunet: opened\n"); // macro used for printing informational kernel messages, wraps printk
    return 0;
}

static int inzunet_stop(struct net_device *dev)
{
    netif_stop_queue(dev); // stops transmit queue
    pr_info("inzunet: stopped\n");
    return 0;
}

// define ndo_start_xmit operation - called by the core kernel when it wants to send a packet out this device.
// ndo means net_device_ops and xmit means transmit.
static netdev_tx_t inzunet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    // netdev_priv(dev) is a helper function that returns a pointer to the private memory area of a net_device struct
    // there are no fields. This is an area where you can add custom stuff. In this case we defined the custom stuff up top in our struct.
    struct inzunet_priv *priv = netdev_priv(dev);

    atomic64_inc(&priv->tx_packets); // increments the number of TX packets  using atomic operation
        atomic64_add(skb->len, &priv->tx_bytes); // increments the total byte count using atomic operation

    pr_info("inzunet: xmit len=%u proto=0x%04x\n", skb->len, ntohs(skb->protocol));
    // virtual net devices don't actually transmit the packet since theres no real hardware, so no transmission logic needed to be implemented here
    dev_kfree_skb(skb); // after transmit code is done, the skb needs to be freed and this also decrements reference count since skb can be shared across layers
    return NETDEV_TX_OK; // tells the core kernel that the packet was successfully handled by this driver for transmission
}

// net_device_ops manages the callback functions for the network device's operations
// these are the basics for a network device to function
static const struct net_device_ops inzunet_netdev_ops = {
    .ndo_open       = inzunet_open, // optional, but packet transmission won't happen without this
    .ndo_stop       = inzunet_stop, // optional, but packet transmission won't happen without this
    .ndo_start_xmit = inzunet_start_xmit, // required, if null the kernel core will refuse to register it
};

// called by alloc_netdev, used to initialize the net_device that alloc_netdev creates
static void inzunet_setup(struct net_device *dev)
{
    ether_setup(dev); // set up ethernet-like defaults for your net_device
    dev->netdev_ops = &inzunet_netdev_ops;
    dev->flags |= IFF_NOARP; // disables ARP, ARP is optional since it's virtual
    // Set transmit queue length, since we immediately free the packet, this should never really fill up.
    // When full, the kernel core starts and stops transmit queue automatically. That doesn't mean the tx queue ceases to exist, it always exists, it just stops momentarily.
    // By stopping it just means the kernel stops accepting packets for transmit. So apps sending to the kernel are blocked. Packets won't be dropped unless the app can't wait
    // and tries sending the packet.
    // Each net_device has 1 tx queue. Note: it can have more if there is multiple CPUs and you allow it, that would be represented by netdev->num_tx_queues
    dev->tx_queue_len = 1000;
}

// for /proc files which provides an interface to kernel data and processes, you need to define a show function which prints the contents whenever a user reads it like "cat file"
static int inzunet_proc_show(struct seq_file *m, void *v)
{
        struct inzunet_priv *priv; // even if not used immediately, standard is to define all new variables up top

    // if the device is missing, unlikely but safe, output zeros
        if (!inzunet_dev) {
                seq_printf(m, "tx_packets=0\ntx_bytes=0\n");
                return 0;
        }

        priv = netdev_priv(inzunet_dev); // retrieve private counters
    // print each counter on it's own line
        seq_printf(m,
                   "tx_packets=%lld\ntx_bytes=%lld\n",
                   (long long)atomic64_read(&priv->tx_packets),
                   (long long)atomic64_read(&priv->tx_bytes));
        return 0;
}

static int inzunet_proc_open(struct inode *inode, struct file *file)
{
        return single_open(file, inzunet_proc_show, NULL);
}

// file operations for a /proc entry
static const struct proc_ops inzunet_proc_ops = {
    .proc_open    = inzunet_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

// module initialization funciton, not required but your code is useless unless you have an init function. Your module would only be useful really to provide helper
// functionality.
// __init is a macro that tells the kernel that the function is only needed during initialization and the memory for it can be freed afterwards
static int __init inzunet_init(void)
{
    int err;
    struct inzunet_priv *priv;

    // allocates a net_device called dev and passes it to inzunet_setup to set it up and then returns that dev here which we assign to our already declared inzunet_dev
    inzunet_dev = alloc_netdev(0, "inzunet%d", NET_NAME_UNKNOWN, inzunet_setup);
        struct inzunet_priv *priv;
    if (!inzunet_dev)
        return -ENOMEM;

        // initialize priv counters to zero
    priv = netdev_priv(inzunet_dev);
        atomic64_set(&priv->tx_packets, 0);
        atomic64_set(&priv->tx_bytes, 0);

    err = register_netdev(inzunet_dev); // registers the device to the kernel networking subsystem
    if (err) {
        pr_err("inzunet: register_netdev failed: %d\n", err);
        free_netdev(inzunet_dev); // free the net_device if it failed to register
        return err;
    }

        // create /proc entry at /proc/inzunet_stats
        inzunet_proc_entry = proc_create(PROC_NAME, 0444, NULL, &inzunet_proc_ops);
        if (!inzunet_proc_entry) {
                pr_warn("inzunet: failed to create /proc/%s (continuing without proc)\n", PROC_NAME);
                /* not fatal - continue (but user won't have stats via /proc) */
        }

    pr_info("inzunet: module loaded, device=%s\n", inzunet_dev->name);
    return 0;
}

// module cleanup function, is required.
// __exit is a macro that tells the kernel that the function is only used when module is unloaded and if module is built-in then it can discard this code since it
// won't be unloaded. Note: modules can be compiled and built-in or loaded later.
static void __exit inzunet_exit(void)
{
        if (inzunet_proc_entry) { // remove proc entry if created
                remove_proc_entry(PROC_NAME, NULL);
                inzunet_proc_entry = NULL;
        }

    if (inzunet_dev) { // remove inzunet net device if created
        unregister_netdev(inzunet_dev);
        free_netdev(inzunet_dev);
        pr_info("inzunet: module unloaded\n");
    }
}

module_init(inzunet_init);
module_exit(inzunet_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sergio Inzunza");
MODULE_DESCRIPTION("Minimal virtual NIC for learning");
