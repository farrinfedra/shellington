#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>

#include <linux/ioctl.h>

#define PSTRAVERSE_B _IOW('a','a',int*)
#define PSTRAVERSE_D _IOW('a','b',int*)

#define MEMORY_SIZE 2048
#define MAX_ARR_SIZE 1024
void* ptr;
int value = 0;
dev_t dev = 0;


static struct class *dev_class;
static struct cdev my_cdev;

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .unlocked_ioctl = my_ioctl,
};

//QUEUE FUNCTIONS
struct task_struct* queue[MAX_ARR_SIZE];
int front = 0;
int rear = -1;
int queueItemCount = 0;

void insert(struct task_struct* task){
	queue[++rear] = task;
    queueItemCount++;
}

struct task_struct* removeTask(void){
   struct task_struct* task = queue[front++];
   queueItemCount--;
   return task;  
}
//END OF THE QUEUE IMPLEMENTATION


static int __init my_driver_init(void)
{
	/* Allocating Major number dynamically*/
	/* &dev is address of the device */
	/* <0 to check if major number is created */
	if((alloc_chrdev_region(&dev, 0, 1, "my_Dev")) < 0) {
		printk(KERN_INFO"Cannot allocate the major number...\n");
	}

	printk(KERN_INFO"Major = %d Minor =  %d..\n", MAJOR(dev),MINOR(dev));

	/* creating cdev structure*/
	cdev_init(&my_cdev, &fops);

	/* adding character device to the system */
	if((cdev_add(&my_cdev, dev, 1)) < 0) {
		printk(KERN_INFO "Cannot add the device to the system...\n");
		goto r_class;
	}	 

	/* creating struct class */
	if((dev_class =  class_create(THIS_MODULE, "my_class")) == NULL) {
		printk(KERN_INFO " cannot create the struct class...\n");
		goto r_class;
	}

	/* creating device */

	if((device_create(dev_class, NULL, dev, NULL, "my_device")) == NULL) {
		printk(KERN_INFO " cannot create the device ..\n");
		goto r_device;
	}

	printk(KERN_INFO"Device driver insert...done properly...");
	return 0;

r_device: 
	class_destroy(dev_class);

r_class:
	unregister_chrdev_region(dev, 1);
	return -1;
}

void __exit my_driver_exit(void) {
	device_destroy(dev_class, dev);
	class_destroy(dev_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "Device driver is removed successfully...\n");
}

void recursive_dfs(int tabCount, struct task_struct *task){
	struct list_head *list;
	struct task_struct *child;
	int i =0 ;
	
	while(i < tabCount){
		printk(KERN_CONT "%s", "-");
		i++;
	} 

	printk(KERN_CONT "%s-%d\n", task->comm, task->pid);
	tabCount++;
	list_for_each(list, &task->children){
		child= list_entry(list, struct task_struct, sibling);
		recursive_dfs(tabCount, child);
	}
}


void traverse_dfs(pid_t target_pid){
	struct task_struct *task;

	for_each_process(task){
		if (task->pid == target_pid){
		printk(KERN_INFO "Starting Depth First Search for %s-%d\n", task->comm, task->pid);
		recursive_dfs(0, task);
		}
	}
	printk(KERN_INFO "Depth First Search is completed.");

}


void bfs(void){
	struct task_struct *task;
	struct list_head *list;
	struct task_struct *child;
	while(queueItemCount != 0){
		task = removeTask();
		
		list_for_each(list, &task->children){
			child= list_entry(list, struct task_struct, sibling);
			printk(KERN_INFO "%s-%d", child->comm, child->pid);
			insert(child);
		}
	}
}

void traverse_bfs(pid_t target_pid){
	struct task_struct *task;

	for_each_process(task){
		if (task->pid == target_pid){
				printk(KERN_INFO "Starting Bread First Search for %s-%d", task->comm, task->pid);
				insert(task);
				printk(KERN_INFO "%s-%d", task->comm, task->pid);
				bfs();
		}
	}
	printk(KERN_INFO "Bread First Search is completed.");

}




static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if( copy_from_user(&value ,(int*) arg, sizeof(value)) )
		{
			pr_err("Error occured on copy_from_user at PSTRAVERSE_B!");
		}
	switch(cmd) {
		case PSTRAVERSE_B:
				traverse_bfs(value);
				break;
		case PSTRAVERSE_D:
				traverse_dfs(value);
				break;
		default:
				pr_info("Default\n");
				break;
	}
	return 0;
}


module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Farrin Marouf Sofian, Atakan Kara");
MODULE_DESCRIPTION("Shellington kernee");


