// Day 1 - Basic network device. We will create a virtual network device which is more basic and easier to implement than a real network device.
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h> // provides net_device struct and related functionality
#include <linux/etherdevice.h> // provides ether_setup
#include <linux/skbuff.h> // provides skbuff struct and related functionality
#include <linux/if_ether.h> // provides ntohs, specifically defines Ethernet protocol constants and structures
// A lot of these header files' code is indirectly included from other already included header files, however, standard is to always include all the
// header files you are using explicitly.

// net_device struct, is the standard way of representing network devices
// you don't allocate this manually (you can but you shouldn't), you use alloc_netdev. alloc_netdev takes care of correctly allocating the net_device struct.
static struct net_device *inzunet_dev; // net_device struct, is the standard way of representing network devices

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

// module initialization funciton, not required but your code is useless unless you have an init function. Your module would only be useful really to provide helper
// functionality.
// __init is a macro that tells the kernel that the function is only needed during initialization and the memory for it can be freed afterwards
static int __init inzunet_init(void)
{
    int err;

    // allocates a net_device called dev and passes it to inzunet_setup to set it up and then returns that dev here which we assign to our already declared inzunet_dev
    inzunet_dev = alloc_netdev(0, "inzunet%d", NET_NAME_UNKNOWN, inzunet_setup);
    if (!inzunet_dev)
        return -ENOMEM;

    err = register_netdev(inzunet_dev); // registers the device to the kernel networking subsystem
    if (err) {
        pr_err("inzunet: register_netdev failed: %d\n", err);
        free_netdev(inzunet_dev); // free the net_device if it failed to register
        return err;
    }

    pr_info("inzunet: module loaded, device=%s\n", inzunet_dev->name);
    return 0;
}

// module cleanup function, is required.
// __exit is a macro that tells the kernel that the function is only used when module is unloaded and if module is built-in then it can discard this code since it
// won't be unloaded. Note: modules can be compiled and built-in or loaded later.
static void __exit inzunet_exit(void)
{
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
