
/**********************************************************************************************
* Char devices' driver that implement a module Linux that offers a service similar to those   *
* that are offered by Windows "Mailslots". It handles up to 256 different instances that can  *
* be concurrently accessed by active threads. The runtime behaviour of a mailslot can be      *
* changed through IOCTL commands (e.g. max message size per mailslot) or change the behaviour *
* of any I/O session targeting it (blocking/non-blocking policies).                           *
**********************************************************************************************/

/* Include headers */
#include <linux/kernel.h>	// printk() log level and so on
#include <linux/module.h>	// Module support
#include <linux/uaccess.h>	// For controlled transfer from/to userspace
#include <linux/slab.h>		// kzalloc() and kfree()
#include <linux/fs.h>		// For struct file_operations and others
#include <linux/cdev.h>		// Character devices
#include <linux/mutex.h>	// Atomic access to resources

#include "ioctl_cmd.h"		// IOCTL commands, author's defined

/* Module details */
MODULE_AUTHOR( "Riccardo Vecchi <vecchi.1467420@studenti.uniroma1.it>" );
MODULE_DESCRIPTION( "A Linux kernel subsystem services similar to those that are offered by Windows \"Mailslots\"" );
MODULE_VERSION( "1.0" );
MODULE_LICENSE( "GPL" );

/* Parameters */
#define DEVICE_NAME "mailslot"
#define FIRST_MINOR 0
#define INSTANCES 256
#define MAILSLOT_STORAGE 64
#define DEFAULT_MESSAGE_SIZE 128
#define MAXIMUM_MESSAGE_SIZE 512

#define BLOCKING 0
#define NONBLOCKING 1

#define SUCCESS 0

/* Prototypes */
int init_module( void );
void cleanup_module( void );
static int mailslot_open( struct inode*, struct file* );
static int mailslot_release( struct inode*, struct file* );
static ssize_t mailslot_read( struct file*, char*, size_t, loff_t* );
static ssize_t mailslot_write( struct file*, const char*, size_t, loff_t* );
static long mailslot_ioctl( struct file*, unsigned int, unsigned long );
static void __deallocate_instances( void );
static int __get_slot( struct file* );
static int __get_blocking_policy( struct file* );
static int __mailslot_lock( int, int ); 
static void __mailslot_unlock( int );


/* Message struct */
struct message {
	char* content;
	size_t length;
	struct message* next;
};

/* Mailslot instance struct */
struct mailslot {
	wait_queue_head_t read_queue, write_queue;	// Wait queues for processes
	struct mutex mutex;		// Mutual exclusion on mailslot (device)
	struct message* head;	// FIFO head
	struct message* tail;	// FIFO tail
	size_t max_msg_size;
	int msg_count;
};

/* File operations struct */
static struct file_operations fops = {
	.owner = THIS_MODULE,	// This field is used to prevent the module from being unloaded while its operations are in use
	.open = mailslot_open,
	.release = mailslot_release,
	.read = mailslot_read,
	.write = mailslot_write,
	.unlocked_ioctl = mailslot_ioctl
};

static struct cdev* mailslot_cdev;
static struct mailslot* mailslot[INSTANCES]; // Array of pointers to mailslots
static dev_t dev;  // It stores the device numbers (MAJOR and MINOR)


/* Function implementation */

int init_module( void ) {

	int i, error;

	printk( KERN_INFO "INITIALIZING MAILSLOT DRIVER..." );

	// Allocate memory for slots
	for ( i = 0; i < INSTANCES; i++ ) {

		mailslot[i] = kzalloc( sizeof(struct mailslot), GFP_KERNEL );
		
		if ( !mailslot[i] ) {
			printk( KERN_WARNING "ERROR: ALLOCATION FOR MAILSLOT %d FAILED!", i );
			__deallocate_instances();
			return -ENOMEM;
		}

		init_waitqueue_head( &mailslot[i]->read_queue );
		init_waitqueue_head( &mailslot[i]->write_queue );
		mutex_init( &mailslot[i]->mutex );
		mailslot[i]->msg_count = 0;
		mailslot[i]->max_msg_size = DEFAULT_MESSAGE_SIZE;		
	}
	
	// Char device setup
	error = alloc_chrdev_region( &dev, FIRST_MINOR, INSTANCES, DEVICE_NAME );	// Note: major DINAMICALLY chosen for reliability

	if ( error ) {
		printk( KERN_WARNING "ERROR: ALLOCATION OF CHRDEV REGION FAILED!" );	
		__deallocate_instances();
		return error;
	}

	mailslot_cdev = cdev_alloc();	// Allocates and return a cdev structure

	if ( mailslot_cdev == NULL ) {
		printk( KERN_WARNING "ERROR: ALLOCATION OF THE CDEV STRUCTURE FAILED!" );
		unregister_chrdev_region( dev, INSTANCES );
		__deallocate_instances();
		return -ENOMEM;
	}

	cdev_init( mailslot_cdev, &fops );	// Initializes a cdev structure, making it ready to add to the system with cdev_add

	error = cdev_add( mailslot_cdev, dev, INSTANCES );	// It adds a char device to the system, making it live immediately
	
	if ( error ) {
		printk( KERN_WARNING "ERROR: ADDITION OF THE CHAR DEVICE FAILED!" );
		cdev_del( mailslot_cdev );
		unregister_chrdev_region( dev, INSTANCES );
		__deallocate_instances();
		return error;
	}

	printk( KERN_INFO "INITIALIZATION OF MAILSLOT DRIVER CORRECTLY EXECUTED! MAJOR: %d\n", MAJOR(dev) );

	return SUCCESS;

}


void cleanup_module( void ) {

	printk( KERN_INFO "CLEANING UP MAILSLOT MODULE AND QUIT..." );

	// Delete device's structure
	cdev_del( mailslot_cdev );
	unregister_chrdev_region( dev, INSTANCES );

	// Deallocate memory
	__deallocate_instances();

	printk( KERN_INFO "MAILSLOT DRIVER MODULE SUCCESSFULLY UNREGISTERED. MAJOR: %d\n", MAJOR(dev) );

}


static int mailslot_open( struct inode* inode, struct file* filp ) {

	int slot = __get_slot( filp );

	printk( KERN_INFO "OPENING MAILSLOT...\nMAILSLOT SUCCESSFULLY OPENED! SLOT N°: %d", slot );

	return SUCCESS;

}


static int mailslot_release( struct inode* inode, struct file* filp ) {

	int slot = __get_slot( filp );

	printk( KERN_INFO "CLOSING MAILSLOT...\nMAILSLOT SUCCESSFULLY CLOSED! SLOT N°: %d", slot );

	return SUCCESS;

}


static ssize_t mailslot_read( struct file* filp, char __user* buff, size_t len, loff_t* off ) {

	struct message* tmp;
	char* msg_body;
	size_t bytes_left, msg_len; 
	int slot, non_blocking, interrupted;

	slot = __get_slot( filp );
	non_blocking = __get_blocking_policy( filp );

	printk( KERN_INFO "MAILSLOT READING..." );
	non_blocking ? printk( KERN_INFO "A NON-BLOCKING POLICY IS USED..." ) : printk( KERN_INFO "A BLOCKING POLICY IS USED..." );
	
	if ( len == 0 ) {
		printk( KERN_WARNING "ERROR: REQUESTED TO READ 0 BYTE!" ); 
		return -EINVAL;
	}
	
	if ( buff == NULL ) {
		printk( KERN_WARNING "ERROR: READ FUNCTION CALLED WITH NULL BUFFER PARAMETER!" ); 
		return -EINVAL;	
	}	
	
	if ( non_blocking ) {

		if ( __mailslot_lock( slot, NONBLOCKING ) == -EAGAIN ) return -EAGAIN;

		if ( mailslot[slot]->msg_count == 0 ) {
			printk( KERN_INFO "THE MAILSLOT IS EMPTY. SLOT N°: %d", slot );
			__mailslot_unlock( slot );
			return -EAGAIN;
		}
	}		
	else { // The default behaviour is a blocking policy

		if ( __mailslot_lock( slot, BLOCKING ) == -EINTR ) return -EINTR;
		
		while ( mailslot[slot]->msg_count == 0 ) {
			__mailslot_unlock( slot );
			interrupted = wait_event_interruptible_exclusive( mailslot[slot]->read_queue, mailslot[slot]->msg_count > 0 );
			if ( interrupted ) return -EINTR;	 
			if ( __mailslot_lock( slot, BLOCKING ) == -EINTR ) return -EINTR;		
		} 				
	}

	msg_body = mailslot[slot]->head->content;
	msg_len = mailslot[slot]->head->length;

	if ( msg_len > len ) {
		printk( KERN_WARNING "ERROR: CAN'T READ. BUFFER TOO LITTLE!" );
		__mailslot_unlock( slot );
		return -EMSGSIZE;
	}

	if ( non_blocking ) {
		pagefault_disable();
		bytes_left = copy_to_user( buff, msg_body, msg_len );
		pagefault_enable();
	}
	else bytes_left = copy_to_user( buff, msg_body, msg_len );

	if ( bytes_left > 0 ) {
		printk( KERN_WARNING "ERROR: CAN'T GET THE MESSAGE FROM MAILSLOT! SLOT N°: %d", slot );
		__mailslot_unlock( slot );
		return -EFAULT;
	}

	printk( KERN_INFO "MESSAGE LENGTH:  %zu BYTES", msg_len );
	printk( KERN_INFO "MESSAGE CONTENT: %s", msg_body );

	tmp = mailslot[slot]->head;

	if ( mailslot[slot]->msg_count > 1 )
		mailslot[slot]->head = mailslot[slot]->head->next;

	kfree( tmp->content );
	kfree( tmp );

	if ( --(mailslot[slot]->msg_count) )
		printk( KERN_INFO "THERE ARE %d MORE MESSAGES IN THE MAILSLOT. SLOT N°: %d", mailslot[slot]->msg_count, slot );

	__mailslot_unlock( slot );

	wake_up_interruptible( &mailslot[slot]->write_queue );

	return msg_len;

}


static ssize_t mailslot_write( struct file* filp, const char __user* buff, size_t len, loff_t* off ) {

	struct message* new_msg;
	size_t bytes_left; 
	int slot, non_blocking, interrupted;

	slot = __get_slot( filp );
	non_blocking = __get_blocking_policy( filp );
	
	printk( KERN_INFO "MAILSLOT WRITING..." );
	non_blocking ? printk( KERN_INFO "A NON-BLOCKING POLICY IS USED..." ) : printk( KERN_INFO "A BLOCKING POLICY IS USED..." );
	
	if ( len == 0 ) {
		printk( KERN_WARNING "ERROR: REQUESTED TO WRITE A 0 BYTE MESSAGE!" ); 
		return -EINVAL;
	}
	
	if ( buff == NULL ) {
		printk( KERN_WARNING "ERROR: WRITE FUNCTION CALLED WITH NULL BUFFER PARAMETER!" ); 
		return -EINVAL;	
	}	
		
	if ( non_blocking ) {

		if ( __mailslot_lock( slot, NONBLOCKING ) == -EAGAIN ) return -EAGAIN;

		if ( mailslot[slot]->msg_count == MAILSLOT_STORAGE ) {
			printk( KERN_WARNING "ERROR: CAN'T WRITE. THE MAILSLOT IS FULL! SLOT N°: %d", slot );
			__mailslot_unlock( slot );
			return -EAGAIN;
		}
	}
	else { // The default behaviour is a blocking policy

		if ( __mailslot_lock( slot, BLOCKING ) == -EINTR ) return -EINTR;
		
		while ( mailslot[slot]->msg_count == MAILSLOT_STORAGE ) {
			__mailslot_unlock( slot );
			interrupted = wait_event_interruptible_exclusive( mailslot[slot]->write_queue, mailslot[slot]->msg_count < MAILSLOT_STORAGE );
			if ( interrupted ) return -EINTR;			 
			if ( __mailslot_lock( slot, BLOCKING ) == -EINTR ) return -EINTR;		
		} 				
	}

	if ( len > mailslot[slot]->max_msg_size ) {
		printk( KERN_WARNING "ERROR: CAN'T WRITE. MESSAGE TOO BIG! MAXIMUM NUMBER OF CHARACTERS IS %zu!", mailslot[slot]->max_msg_size ); // - 1 because of null-byte terminator
		__mailslot_unlock( slot );
		return -EPERM;
	}

	new_msg = kzalloc( sizeof(struct message), non_blocking ? GFP_ATOMIC : GFP_KERNEL );
	if ( !new_msg ) {
		printk( KERN_WARNING "ERROR: FAILED TO ALLOCATE MEMORY FOR THE MESSAGE STRUCT" );
		__mailslot_unlock( slot );
		return non_blocking ? -EAGAIN : -ENOMEM;
	}

	new_msg->length = len;
	new_msg->content = kzalloc( len, non_blocking ? GFP_ATOMIC : GFP_KERNEL );
	if ( !new_msg->content ) {
		printk( KERN_WARNING "ERROR: FAILED TO ALLOCATE MEMORY FOR THE MESSAGE CONTENT" );
		kfree( new_msg );
		__mailslot_unlock( slot );
		return non_blocking ? -EAGAIN : -ENOMEM;
	}
	
	if ( non_blocking ) {
		pagefault_disable();
		bytes_left = copy_from_user( new_msg->content, buff, len );
		pagefault_enable();
	}
	else bytes_left = copy_from_user( new_msg->content, buff, len );

	if ( bytes_left > 0 ) {
		printk( KERN_WARNING "ERROR: CAN'T DELIVER THE MESSAGE TO MAILSLOT! SLOT N°: %d", slot );
		kfree( new_msg->content );
		kfree( new_msg );
		__mailslot_unlock( slot );
		return -EFAULT;
	}
	
	printk( KERN_INFO "MESSAGE LENGTH:  %zu BYTES", new_msg->length );
	printk( KERN_INFO "MESSAGE CONTENT: %s", new_msg->content );

	if ( mailslot[slot]->msg_count == 0 ) 
		mailslot[slot]->head = new_msg;
	else
		mailslot[slot]->tail->next = new_msg;

	mailslot[slot]->tail = new_msg;
	
	mailslot[slot]->msg_count++;

	printk( KERN_INFO "MESSAGE CORRECTLY DELIVERED TO MAILSLOT! SLOT N°: %d", slot );
	printk( KERN_INFO "THE MAILSLOT HAS %d NEW MESSAGES NOW! SLOT N°: %d", mailslot[slot]->msg_count, slot );

	__mailslot_unlock( slot );

	wake_up_interruptible( &mailslot[slot]->read_queue );

	return len;

}


static long mailslot_ioctl( struct file* filp, unsigned int cmd, unsigned long arg ) {
	
	int slot, non_blocking, error;
	
	slot = __get_slot( filp );
	non_blocking = __get_blocking_policy( filp );

	switch ( cmd ) {

		case SET_BLOCKING:
			printk( KERN_INFO "SET BLOCKING POLICY! SLOT N°: %d", slot );	
			filp->f_flags &= ~O_NONBLOCK;	// AND bit a bit because O_NONBLOCK is a bit mask
			break;

		case SET_NONBLOCKING:
			printk( KERN_INFO "SET NON-BLOCKING POLICY! SLOT N°: %d", slot );
			filp->f_flags |= O_NONBLOCK;	// OR bit a bit because O_NONBLOCK is a bit mask
			break;

		case SET_MAXIMUM_MSG_SIZE:
			printk( KERN_INFO "SETTING NEW MAXIMUM MESSAGE SIZE (%d)...", (int) arg );
			if ( arg == 0 || arg > MAXIMUM_MESSAGE_SIZE ) {
				printk( KERN_WARNING "ERROR: THE MAXIMUM SETTABLE MESSAGE SIZE IS FROM 1 TO %d BYTES!", (int) MAXIMUM_MESSAGE_SIZE );
				return -EINVAL;
			}
			
			error = __mailslot_lock( slot, non_blocking );
			
			if ( (non_blocking) && (error == -EAGAIN) ) return -EAGAIN;
			else if ( !(non_blocking) && (error == -EINTR) ) return -EINTR;
			
			mailslot[slot]->max_msg_size = arg;
			printk( KERN_INFO "MAXIMUM MESSAGE SIZE SETTED TO %zu BYTES! SLOT N°: %d", mailslot[slot]->max_msg_size, slot );
			__mailslot_unlock( slot );
			break;

		default:
			printk( KERN_WARNING "ERROR: IOCTL COMMAND NOT IDENTIFIED! CODE: %u", cmd );
			return -ENOTTY;
			
	}
	
	return SUCCESS;
	
}


static void __deallocate_instances( void ) {

	struct message* tmp;
	int i, j;

	for ( i = 0; i < INSTANCES; i++ ) {

		if ( mailslot[i] == NULL ) return;

		for ( j = 0; j < mailslot[i]->msg_count; j++ ) {
		
			tmp = mailslot[i]->head->next;
			kfree( mailslot[i]->head->content );
			kfree( mailslot[i]->head );
			mailslot[i]->head = tmp;
			
		}

		kfree( mailslot[i] );

	}	

}


static int __get_slot( struct file* filp ) {

	return iminor( filp->f_path.dentry->d_inode ) - FIRST_MINOR;

}


static int __get_blocking_policy( struct file* filp ) {

	return filp->f_flags & O_NONBLOCK ? NONBLOCKING : BLOCKING;
	
}


static int __mailslot_lock( int slot, int non_blocking ) {
	
	if ( non_blocking ) {
		if ( mutex_trylock( &mailslot[slot]->mutex ) == 0 ) { 
			printk( KERN_WARNING "ERROR: FAILED TO ACQUIRE THE LOCK - NONBLOCKING POLICY" );
			return -EAGAIN;
		}				
	}
	
	else { // The default behaviour is a blocking policy
		if ( mutex_lock_interruptible( &mailslot[slot]->mutex ) == -EINTR ) {
			printk( KERN_WARNING "ERROR: FAILED TO ACQUIRE THE LOCK - BLOCKING POLICY" );
			return -EINTR;
		}	
	}
	
	return SUCCESS;
	
}


static void __mailslot_unlock( int slot ) {

	mutex_unlock( &mailslot[slot]->mutex );

}

