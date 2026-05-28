/*
 * Dream Cheeky USB Missile Launcher driver
 *
 * Copyright (C) 2007 Matthias Vallentin <vallentin@icsi.berkeley.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * derived from USB Skeleton driver 2.0
 * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 * also inspired from LEGO USB Tower driver
 * Copyright (C) 2003 David Glance <davidgsf@sourceforge.net>
 *               2001-2004 Juergen Stuber <starblue@users.sourceforge.net>
 *
 * Notes:
 * - Apparently it fails sometimes to submit the correction control URB in the
 *   interrupt-in-endpoint hanlder (-EINVAL).
 */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/slab.h>			/* kmalloc() */
#include <linux/usb.h>			/* USB stuff */
#include <linux/mutex.h>		/* mutexes */
#include <linux/ioctl.h>

//#include <asm/uaccess.h>		/* copy_*_user */
#include <linux/uaccess.h>
#define DEBUG_LEVEL_DEBUG		0x1F
#define DEBUG_LEVEL_INFO		0x0F
#define DEBUG_LEVEL_WARN		0x07
#define DEBUG_LEVEL_ERROR		0x03
#define DEBUG_LEVEL_CRITICAL	0x01

// #define DBG_DEBUG(fmt, args...) \
// if ((debug_level & DEBUG_LEVEL_DEBUG) == DEBUG_LEVEL_DEBUG) \
// 	printk( KERN_DEBUG "[debug] %s(%d): " fmt "\n", \
// 			__FUNCTION__, __LINE__, ## args)
// #define DBG_INFO(fmt, args...) \
// if ((debug_level & DEBUG_LEVEL_INFO) == DEBUG_LEVEL_INFO) \
// 	printk( KERN_DEBUG "[info]  %s(%d): " fmt "\n", \
// 			__FUNCTION__, __LINE__, ## args)
// #define DBG_WARN(fmt, args...) \
// if ((debug_level & DEBUG_LEVEL_WARN) == DEBUG_LEVEL_WARN) \
// 	printk( KERN_DEBUG "[warn]  %s(%d): " fmt "\n", \
// 			__FUNCTION__, __LINE__, ## args)
// #define DBG_ERR(fmt, args...) \
// if ((debug_level & DEBUG_LEVEL_ERROR) == DEBUG_LEVEL_ERROR) \
// 	printk( KERN_DEBUG "[err]   %s(%d): " fmt "\n", \
// 			__FUNCTION__, __LINE__, ## args)
// #define DBG_CRIT(fmt, args...) \
// if ((debug_level & DEBUG_LEVEL_CRITICAL) == DEBUG_LEVEL_CRITICAL) \
// 	printk( KERN_DEBUG "[crit]  %s(%d): " fmt "\n", \
// 			__FUNCTION__, __LINE__, ## args)

#define ML_DEFAULT 0
#define ML_THUNDER 1

//#define MISSILE_LAUNCHER ML_DEFAULT

//#if MISSILE_LAUNCHER == ML_THUNDER

/*
 * USB Missile Launcher Thunder version (with LEDs)
 */
//#define ML_VENDOR_ID	0x2123
//#define ML_PRODUCT_ID	0x1010

//#else

/*
 * USB Missile Launcher default version
 */
#define ML_VENDOR_ID	0x0a81
#define ML_PRODUCT_ID	0x0701

//#endif

#define ML_CTRL_BUFFER_SIZE 	8
#define ML_CTRL_REQUEST_TYPE	0x21
#define ML_CTRL_REQUEST		0x09
#define ML_CTRL_VALUE		0x0
#define ML_CTRL_INDEX		0x0

#define ML_STOP			0x20
#define ML_UP			0x02
#define ML_DOWN			0x01
// #if MISSILE_LAUNCHER == ML_THUNDER
// #define ML_LED			0x03
// #endif
#define ML_LEFT			0x04
#define ML_RIGHT		0x08
#define ML_UP_LEFT		(ML_UP | ML_LEFT)
#define ML_DOWN_LEFT		(ML_DOWN | ML_LEFT)
#define ML_UP_RIGHT		(ML_UP | ML_RIGHT)
#define ML_DOWN_RIGHT		(ML_DOWN | ML_RIGHT)
#define ML_FIRE			0x10

#define ML_MAX_UP		0x80 		/* 80 00 00 00 00 00 00 00 */
#define ML_MAX_DOWN		0x40		/* 40 00 00 00 00 00 00 00 */
#define ML_MAX_LEFT		0x04		/* 00 04 00 00 00 00 00 00 */
#define ML_MAX_RIGHT		0x08 		/* 00 08 00 00 00 00 00 00 */

//#ifdef CONFIG_USB_DYNAMIC_MINORS
#define ML_MINOR_BASE	0
//#else
// #define ML_MINOR_BASE	96
//#endif

struct usb_ml {
	struct usb_device 	*udev;
	struct usb_interface 	*interface;
	unsigned char		minor;
	char			serial_number[8];

	int			open_count;     /* Open count for this port */
	struct 			semaphore sem;	/* Locks this structure */
	spinlock_t		cmd_spinlock;	/* locks dev->command */

	char				*int_in_buffer;
	struct usb_endpoint_descriptor  *int_in_endpoint;
	struct urb 			*int_in_urb;
	int				int_in_running;

	char			*ctrl_buffer; /* 8 byte buffer for ctrl msg */
	struct urb		*ctrl_urb;
	struct usb_ctrlrequest  *ctrl_dr;     /* Setup packet information */
	int			correction_required;

	__u8			command;/* Last issued command */
};


static const struct usb_device_id ml_table [] = {
	{ USB_DEVICE(ML_VENDOR_ID, ML_PRODUCT_ID) },
	//{ USB_DEVICE_INFO(0x03,0x00,0x00) },
	{ }
};


MODULE_DEVICE_TABLE (usb, ml_table);

static int debug_level = DEBUG_LEVEL_INFO;
static int debug_trace = 0;
//module_param(debug_level, int, S_IRUGO | S_IWUSR);
//module_param(debug_trace, int, S_IRUGO | S_IWUSR);
//MODULE_PARM_DESC(debug_level, "debug level (bitmask)");
//MODULE_PARM_DESC(debug_trace, "enable function tracing");

/* Prevent races between open() and disconnect */

//below is global mutex declartion
static DEFINE_MUTEX(disconnect_mutex);
static struct usb_driver ml_driver;

/*static inline void ml_debug_data(const char *function, int size,
		const unsigned char *data)
{
	int i;
	if ((debug_level & DEBUG_LEVEL_DEBUG) == DEBUG_LEVEL_DEBUG) {
		printk(KERN_DEBUG "[debug] %s: length = %d, data = ",
		       function, size);
		for (i = 0; i < size; ++i)
			printk("%.2x ", data[i]);
		printk("\n");
	}
}*/

static void ml_abort_transfers(struct usb_ml *dev)
{
	if (! dev) {
		printk("dev is NULL");
		return;
	}

	if (! dev->udev) {
		printk("udev is NULL");
		return;
	}

	if ( dev->udev->state == USB_STATE_NOTATTACHED) {
		printk("udev not attached");
		return;
	}

	/* Shutdown transfer */
	if (dev->int_in_running) {
		dev->int_in_running = 0;
		mb();
		if (dev->int_in_urb)
			usb_kill_urb(dev->int_in_urb);
	}

	if (dev->ctrl_urb)
		usb_kill_urb(dev->ctrl_urb);
}

static inline void ml_delete(struct usb_ml *dev)
{
	ml_abort_transfers(dev);

	/* Free data structures. */
	if (dev->int_in_urb)
		usb_free_urb(dev->int_in_urb);
	if (dev->ctrl_urb)
		usb_free_urb(dev->ctrl_urb);

	kfree(dev->int_in_buffer);
	kfree(dev->ctrl_buffer);
	kfree(dev->ctrl_dr);
	kfree(dev);
}

static void ml_ctrl_callback(struct urb *urb)
{
	struct usb_ml *dev = urb->context;
	dev->correction_required = 0;	/* TODO: do we need race protection? */
}

static void ml_int_in_callback(struct urb *urb)
{
	struct usb_ml *dev = urb->context;
	int retval;

	//ml_debug_data(__FUNCTION__, urb->actual_length, urb->transfer_buffer);

	if (urb->status) {
		if (urb->status == -ENOENT ||
				urb->status == -ECONNRESET ||
				urb->status == -ESHUTDOWN) {
			return;
		} else {
			//DBG_ERR("non-zero urb status (%d)", urb->status);
			goto resubmit; /* Maybe we can recover. */
		}
	}

	if (urb->actual_length > 0) {
		spin_lock(&dev->cmd_spinlock);

		if (dev->int_in_buffer[0] & ML_MAX_UP && dev->command & ML_UP) {
			dev->command &= ~ML_UP;
			dev->correction_required = 1;
		} else if (dev->int_in_buffer[0] & ML_MAX_DOWN &&
				dev->command & ML_DOWN) {
			dev->command &= ~ML_DOWN;
			dev->correction_required = 1;
		}

		if (dev->int_in_buffer[1] & ML_MAX_LEFT
		    && dev->command & ML_LEFT) {
			dev->command &= ~ML_LEFT;
			dev->correction_required = 1;
		} else if (dev->int_in_buffer[1] & ML_MAX_RIGHT
			   && dev->command & ML_RIGHT) {
			dev->command &= ~ML_RIGHT;
			dev->correction_required = 1;
		}


		if (dev->correction_required) {
			dev->ctrl_buffer[0] = dev->command;
			spin_unlock(&dev->cmd_spinlock);
			retval = usb_submit_urb(dev->ctrl_urb, GFP_ATOMIC);
			if (retval) {
				//DBG_ERR("submitting correction control URB failed (%d)",
						//retval);
			}
		} else {
			spin_unlock(&dev->cmd_spinlock);
		}
	}

resubmit:
	/* Resubmit if we're still running. */
	if (dev->int_in_running && dev->udev) {
		retval = usb_submit_urb(dev->int_in_urb, GFP_ATOMIC);
		if (retval) {
			//DBG_ERR("resubmitting urb failed (%d)", retval);
			printk("resubmitting urb failed (%d)", retval);
			dev->int_in_running = 0;
		}
	}
}





static int ml_open(struct inode *inode, struct file *file)
{
	struct usb_ml *dev = NULL;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	printk("Open device");
	subminor = iminor(inode);

	mutex_lock(&disconnect_mutex);

	interface = usb_find_interface(&ml_driver, subminor);
	if (! interface) {
		printk("can't find device for minor %d", subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (! dev) {
		retval = -ENODEV;
		goto exit;
	}

	/* lock this device */
	if (down_interruptible (&dev->sem)) {
		printk("sem down failed");
		retval = -ERESTARTSYS;
		goto exit;
	}

	/* Increment our usage count for the device. */
	++dev->open_count;
	if (dev->open_count > 1)
		printk("open_count = %d", dev->open_count);

	/* Initialize interrupt URB. */
	usb_fill_int_urb(dev->int_in_urb, dev->udev,
			usb_rcvintpipe(dev->udev,
				       dev->int_in_endpoint->bEndpointAddress),
			dev->int_in_buffer,
			dev->int_in_endpoint->wMaxPacketSize,
			ml_int_in_callback,
			dev,
			dev->int_in_endpoint->bInterval);

	dev->int_in_running = 1;
	mb();

	retval = usb_submit_urb(dev->int_in_urb, GFP_KERNEL);
	if (retval) {
		printk("submitting int urb failed (%d)", retval);
		dev->int_in_running = 0;
		--dev->open_count;
		goto unlock_exit;
	}

	/* Save our object in the file's private structure. */
	file->private_data = dev;

unlock_exit:
	up(&dev->sem);

exit:
	mutex_unlock(&disconnect_mutex);
	return retval;
}

static int ml_release(struct inode *inode, struct file *file)
{
	struct usb_ml *dev = NULL;
	int retval = 0;

	printk("Release driver");
	dev = file->private_data;

	if (! dev) {
		printk("dev is NULL");
		retval =  -ENODEV;
		goto exit;
	}

	/* Lock our device */
	if (down_interruptible(&dev->sem)) {
		retval = -ERESTARTSYS;
		goto exit;
	}

	if (dev->open_count <= 0) {
		printk("device not opened");
		retval = -ENODEV;
		goto unlock_exit;
	}

	if (! dev->udev) {
		printk("device unplugged before the file was released");
		up (&dev->sem);	/* Unlock here as ml_delete frees dev. */
		ml_delete(dev);
		goto exit;
	}

	if (dev->open_count > 1)
		printk("open_count = %d", dev->open_count);

	ml_abort_transfers(dev);
	--dev->open_count;

unlock_exit:
	up(&dev->sem);

exit:
	return retval;
}




static long int ml_read(struct file *file , char __user *user_buf, size_t count, loff_t *ppos){
	printk("fuck read!!");
	return 0;
}



static ssize_t ml_write(struct file *file, const char __user *user_buf, size_t
		count, loff_t *ppos)
{
	struct usb_ml *dev;
	int retval = 0;
	bool policy;
// #if MISSILE_LAUNCHER == ML_THUNDER
// 	static int ml_led = 1;
// #endif


	void *modem_status;
	modem_status= kmalloc(1, GFP_KERNEL);
    if (!modem_status)
        return -ENOMEM;

	
	__u8 cmd = ML_STOP;

	printk("Send command");
	dev = file->private_data;

	/* Lock this object. */
	if (down_interruptible(&dev->sem)) {
		retval = -ERESTARTSYS;
		goto exit;
	}

	/* Verify that the device wasn't unplugged. */
	if (! dev->udev) {
		retval = -ENODEV;
		printk("No device or device unplugged (%d)", retval);
		goto unlock_exit;
	}

	/* Verify that we actually have some data to write. */
	if (count == 0)
		goto unlock_exit;

	/* We only accept one-byte writes. */
	if (count != 1)
		count = 1;

	if (copy_from_user(&cmd, user_buf, count)) {
		retval = -EFAULT;
		goto unlock_exit;
	}
	printk("cmd is 0x%02x",cmd);
	policy = (cmd == ML_STOP || cmd == ML_UP || cmd == ML_DOWN
		  || cmd == ML_LEFT || cmd == ML_RIGHT || cmd == ML_UP_LEFT
		  || cmd == ML_DOWN_LEFT || cmd == ML_UP_RIGHT
		  || cmd == ML_DOWN_RIGHT || cmd == ML_FIRE);
// #if MISSILE_LAUNCHER == ML_THUNDER
// 	policy = policy || (cmd == ML_LED);
// #endif
	if (!policy) {
		printk("illegal command issued");
		retval = -0x2a;		/* scnr */
		goto unlock_exit;
	}

	memset(modem_status, 0, sizeof(char)*8);
// #if MISSILE_LAUNCHER == ML_THUNDER
// 	if (cmd == ML_LED) {
// 		buf[0] = ML_LED;
// 		buf[1] = ml_led;
// 		ml_led = 1 - ml_led;
// 	} else {
	char buff[1];
	//memset(buff,0,1);
	buff[0]=cmd;//buff[1]=cmd;
	strncpy( modem_status, buff, 1);
	// modem_status = buff;
// 		buf[1] = cmd;
// 	}
// #else
	// buf[1] = cmd;
	// memset(&buf[1], 0, ML_CTRL_BUFFER_SIZE - 1);
	
// #endif
	/* The interrupt-in-endpoint handler also modifies dev->command. */
	spin_lock(&dev->cmd_spinlock);
	dev->command = cmd;
	spin_unlock(&dev->cmd_spinlock);

	retval = usb_control_msg(dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			ML_CTRL_REQUEST,
			ML_CTRL_REQUEST_TYPE,
			ML_CTRL_VALUE,
			ML_CTRL_INDEX,
			// &buf,
			modem_status,
			sizeof(modem_status),
			// USB_CTRL_SET_TIMEOUT);
			// 2000);
			HZ*5);

	if (retval < 0) {
		printk("usb_control_msg failed (%d)", retval);
		goto unlock_exit;
	}

	/* We should have written only one byte. */
	retval = count;

unlock_exit:
	up(&dev->sem);

exit:
	kfree(modem_status);
	return retval;
}


static struct file_operations ml_fops = {
	.owner =	THIS_MODULE,
	.write =	ml_write,
	.open =		ml_open,
	.release =	ml_release,
	// .read = ml_read,
};

static struct usb_class_driver ml_class = {
	//.name = "ml%d",
	.name = "ml%d",
	.fops = &ml_fops,
	.minor_base = 0,//ML_MINOR_BASE,
};

static int ml_probe(struct usb_interface *interface,
		    const struct usb_device_id *id){	
	// printk("fuck probe!!");
	// return 0;
	// dev_info(&interface->dev, "USB Driver Probed: Vendor ID : 0x%02x,\t"
    //          "Product ID : 0x%02x\n", id->idVendor, id->idProduct);
	// return 0;
	//printk("fuck!!");
	struct usb_device *udev; 
	
	struct usb_ml *dev;
	
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, int_end_size;
	int retval = -ENODEV;
	udev= interface_to_usbdev(interface);
	dev = NULL;
	printk("Probe missile launcher");

	if (! udev) {
		printk("udev is NULL");
		goto exit;
	}

	dev = kzalloc(sizeof(struct usb_ml), GFP_KERNEL);
	if (! dev) {
		printk("cannot allocate memory for struct usb_ml");
		retval = -ENOMEM;
		goto exit;
	}

	dev->command = ML_STOP;

	sema_init(&dev->sem, 1);
	spin_lock_init(&dev->cmd_spinlock);

	dev->udev = udev;
	dev->interface = interface;
	iface_desc = interface->cur_altsetting;

	/* Set up interrupt endpoint information. */
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
		     == USB_DIR_IN)
		    && ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
		    	== USB_ENDPOINT_XFER_INT))
			dev->int_in_endpoint = endpoint;

	}
	if (! dev->int_in_endpoint) {
		printk("could not find interrupt in endpoint");
		goto error;
	}

	int_end_size = dev->int_in_endpoint->wMaxPacketSize;

	dev->int_in_buffer = kmalloc(int_end_size, GFP_KERNEL);
	if (! dev->int_in_buffer) {
		printk("could not allocate int_in_buffer");
		retval = -ENOMEM;
		goto error;
	}

	dev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (! dev->int_in_urb) {
		printk("could not allocate int_in_urb");
		retval = -ENOMEM;
		goto error;
	}

	/* Set up the control URB. */
	dev->ctrl_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (! dev->ctrl_urb) {
		printk("could not allocate ctrl_urb");
		retval = -ENOMEM;
		goto error;
	}

	dev->ctrl_buffer = kzalloc(ML_CTRL_BUFFER_SIZE, GFP_KERNEL);
	if (! dev->ctrl_buffer) {
		printk("could not allocate ctrl_buffer");
		retval = -ENOMEM;
		goto error;
	}

	dev->ctrl_dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL);
	if (! dev->ctrl_dr) {
		printk("could not allocate usb_ctrlrequest");
		retval = -ENOMEM;
		goto error;
	}
	dev->ctrl_dr->bRequestType = ML_CTRL_REQUEST_TYPE;
	dev->ctrl_dr->bRequest = ML_CTRL_REQUEST;
	dev->ctrl_dr->wValue = ML_CTRL_VALUE;
	dev->ctrl_dr->wIndex = ML_CTRL_INDEX;
	dev->ctrl_dr->wLength = ML_CTRL_BUFFER_SIZE;

	usb_fill_control_urb(dev->ctrl_urb, dev->udev,
			usb_sndctrlpipe(dev->udev, 0),
			(unsigned char *)dev->ctrl_dr,
			dev->ctrl_buffer,
			ML_CTRL_BUFFER_SIZE,
			ml_ctrl_callback,
			dev);

	/* Retrieve a serial. */
	if (! usb_string(udev, udev->descriptor.iSerialNumber,
			 dev->serial_number, sizeof(dev->serial_number))) {
		printk("could not retrieve serial number");
		goto error;
	}

	/* Save our data pointer in this interface device. */
	usb_set_intfdata(interface, dev);

	/* We can register the device now, as it is ready. */
	retval = usb_register_dev(interface, &ml_class);
	if (retval) {
		printk("not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	dev->minor = interface->minor;

	printk("USB missile launcher now attached to /dev/ml%d",
			interface->minor - ML_MINOR_BASE);

exit:
	return retval;

error:
	ml_delete(dev);
	return retval;
}

static void ml_disconnect(struct usb_interface *interface)
{
	struct usb_ml *dev;
	int minor;

	mutex_lock(&disconnect_mutex);	/* Not interruptible */

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	down(&dev->sem); /* Not interruptible */

	minor = dev->minor;

	/* Give back our minor. */
	usb_deregister_dev(interface, &ml_class);

	/* If the device is not opened, then we clean up right now. */
	if (! dev->open_count) {
		up(&dev->sem);
		ml_delete(dev);
	} else {
		dev->udev = NULL;
		up(&dev->sem);
	}

	mutex_unlock(&disconnect_mutex);

	printk("USB missile launcher /dev/ml%d now disconnected",
			minor - ML_MINOR_BASE);
}

static struct usb_driver ml_driver = {
	.name = "ml_driver",
	//.id_table = ml_table,
	.probe = ml_probe,
	.disconnect = ml_disconnect,
	.id_table = ml_table,
};


static int __init usb_ml_init(void)
{
	int result;

	printk("Register driver");
	result = usb_register(&ml_driver);
	if (result) {
		printk("registering driver failed");
	} else {
		printk("driver registered successfully");
	}

	return result;
}


static void __exit usb_ml_exit(void)
{
	usb_deregister(&ml_driver);
	printk("module deregistered");
}

module_init(usb_ml_init);
module_exit(usb_ml_exit);

MODULE_AUTHOR("Matthias Vallentin");
MODULE_LICENSE("GPL");

// vim: ts=4
