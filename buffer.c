/*
 * buffer.c
 *
 *  Created on: 2019
 *      Author: Simon Saadeh
 */

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/ioctl.h>
//--- IOCTL
#define MAGIC_VAL 'R'
#define GetNumData _IOR(MAGIC_VAL, 1, int32_t*)
#define GetNumReader _IOR(MAGIC_VAL, 2, unsigned short*)
#define GetBufSize _IOR(MAGIC_VAL,  3, int32_t*)
#define SetBufSize _IOW(MAGIC_VAL, 4, unsigned long)
#define IOC_MAXNR 5

#define READWRITE_BUFSIZE 16
#define DEFAULT_BUFSIZE 256

MODULE_LICENSE("Dual BSD/GPL");

static int buf_init(void);
static void buf_exit(void);
int absolue_index(void);
void buffer_resize(unsigned long arg, int newIndex);
int buf_open(struct inode *inode, struct file *flip);
int buf_release(struct inode *inode, struct file *flip);
ssize_t buf_write (struct file *flip, const char __user *ubuf, size_t count, loff_t *f_ops);
ssize_t buf_read (struct file *flip, char __user *ubuf, size_t count, loff_t *f_ops);
long buf_ioctl (struct file *flip, unsigned int cmd, unsigned long arg);
module_init(buf_init);
module_exit(buf_exit);

wait_queue_head_t wq;
wait_queue_head_t rq;

int Buffer_Var = 0;
module_param(Buffer_Var, int, S_IRUGO);

EXPORT_SYMBOL_GPL(Buffer_Var);

struct class *buffer_class;

struct BufStruct{
		unsigned int InIdx;
		unsigned int OutIdx;
		unsigned short BufFull;
		unsigned short BufEmpty;
		unsigned int BufSize;
		char *Buffer;
}Buffer;

struct Buf_Dev{
		char *ReadBuf;
		char *WriteBuf;
		struct semaphore SemBuf;
		unsigned short numWriter;
		unsigned short numReader;
		dev_t 			dev;
		struct cdev		cdev;
}BDev;

struct file_operations Buf_fops = {
	.owner				=	THIS_MODULE,
	.read					=	buf_read,
	.write				=	buf_write,
	.unlocked_ioctl 	=	buf_ioctl,
	.open					=	buf_open,
	.release				=	buf_release,
};

int BufIn (struct BufStruct *Buf, char *Data){
	if(Buf->BufFull)
		return -1;
	Buf->BufEmpty = 0;
	Buf->Buffer[Buf->InIdx] = *Data;
	Buf->InIdx = (Buf->InIdx + 1) % Buf->BufSize;
	printk(KERN_WARNING"Buf Out(%i:%u)\n", Buf->OutIdx, __LINE__);
	printk(KERN_WARNING"Buf Int(%i:%u)\n", Buf->InIdx, __LINE__);
	if(Buf->InIdx == Buf->OutIdx)
		Buf->BufFull = 1;
	return 0;
}

int BufOut (struct BufStruct *Buf, char *Data){
	if(Buf->BufEmpty)
		return -1;
	Buf->BufFull = 0;
 	*Data = Buf->Buffer[Buf->OutIdx];
	Buf->OutIdx = (Buf->OutIdx + 1) % Buf->BufSize;
	printk(KERN_WARNING"Buf Out(%i:%u)\n", Buf->OutIdx, __LINE__);
	printk(KERN_WARNING"Buf Int(%i:%u)\n", Buf->InIdx, __LINE__);
 	if(Buf->OutIdx == Buf->InIdx)
		Buf->BufEmpty = 1;
	return 0;
}

long buf_ioctl (struct file *flip, unsigned int cmd, unsigned long arg) {
	int index=0;
	int getbufsize=0;	
	index = absolue_index(); //nombre de data actuelle dans le buffer en absolu

	if (_IOC_TYPE(cmd) != MAGIC_VAL) 
		return -ENOTTY;
	if (_IOC_NR(cmd) > IOC_MAXNR) 
		return -ENOTTY;

   switch(cmd) {
	case GetNumData:
		put_user(index, (int __user *)arg);
		break;
	case GetNumReader:
		put_user(BDev.numReader, (int __user *)arg);
		break;
	case GetBufSize:
		put_user(Buffer.BufSize, (int __user *)arg); 
		break; 
	case SetBufSize:
		if (! capable(CAP_SYS_ADMIN)){
		return -EPERM;				
		}
		if ( (int) arg > index){ //si la nouvelle taille est plus grande que le nombre de data actuelle
		return -EAGAIN;
		}
		down(&BDev.SemBuf);
		get_user(getbufsize, (int __user *)arg);
		buffer_resize(getbufsize, index);
		up(&BDev.SemBuf);
		break;
	default:
		return -EAGAIN;
		break;
	}
	return 0;
}

int absolue_index(void){

	int absindex = (Buffer.InIdx - Buffer.OutIdx);
	if(absindex<0)
		absindex = ((Buffer.BufSize - Buffer.OutIdx) + (Buffer.InIdx + 1));
	
	return absindex;
}

void buffer_resize(unsigned long arg, int newIndex){ //fonction pour resize le buffer
	int i=0;
	int index =0;
	char * temp_buffer;

   	temp_buffer = kmalloc(arg * sizeof(char), GFP_KERNEL); //buffer temporaire
	memset(temp_buffer,0,arg);
	if(absolue_index()){
		for(i=Buffer.OutIdx;i<(Buffer.InIdx+1);i++){
			temp_buffer[index] = Buffer.Buffer[i];
			index++;	
		}
	}
	else{
		for(i = Buffer.OutIdx;i<Buffer.BufSize; i++ ){
			temp_buffer[index] = Buffer.Buffer[i];
			index++;
		}

		for(i = 0; i<(Buffer.InIdx+1); i++ ){
			temp_buffer[index] = Buffer.Buffer[i];
			index++;
		}
	}		

	kfree(Buffer.Buffer);
	Buffer.Buffer = kmalloc(arg * sizeof(char), GFP_KERNEL);
	memset(Buffer.Buffer, 0 ,arg);
	for(i=0;i<index;i++){
		Buffer.Buffer[i] = temp_buffer[i];	
	}
	Buffer.InIdx = newIndex;
	Buffer.OutIdx =0;
	Buffer.BufSize = arg;

	kfree(temp_buffer);

}

int buf_open(struct inode *inode, struct file *flip){
	printk(KERN_WARNING"Opened (%s:%u)\n", __FUNCTION__, __LINE__);
	down(&BDev.SemBuf);		
	if((flip->f_flags & O_ACCMODE) == O_RDONLY){
	BDev.numReader++;
	}
	else if((flip->f_flags & O_ACCMODE) == O_WRONLY){
	if(BDev.numWriter==1)
	{
	up(&BDev.SemBuf);
	printk(KERN_WARNING"Opened (%s:%u)\n", __FUNCTION__, __LINE__);
		return ENOTTY;
	}
	else{
		BDev.numWriter++;
		}
	}
	else if((flip->f_flags & O_ACCMODE) == O_RDWR){
	if(BDev.numWriter==1)
	{
	up(&BDev.SemBuf);
	printk(KERN_WARNING"Opened (%s:%u)\n", __FUNCTION__, __LINE__);
		return ENOTTY;
	}
	else{

		BDev.numWriter++;
		BDev.numReader++;
		}
	}
	up(&BDev.SemBuf);

	printk(KERN_WARNING"Opened (%s:%u)\n", __FUNCTION__, __LINE__);
	
	return 0;
}

int buf_release(struct inode *inode, struct file *flip){
	printk(KERN_WARNING"Opened (%s:%u)\n", __FUNCTION__, __LINE__);
	down(&BDev.SemBuf);	
	if((flip->f_flags & O_ACCMODE) == O_RDONLY)
	{
		BDev.numReader--;
	}
	if((flip->f_flags & O_ACCMODE) == O_WRONLY)
	{
		BDev.numWriter--;
	}
	if((flip->f_flags & O_ACCMODE) == O_RDWR)
	{
		BDev.numWriter--;
		BDev.numReader--;
	}
	up(&BDev.SemBuf);

	printk(KERN_WARNING"Released (%s:%u)\n", __FUNCTION__, __LINE__);
	
	return 0;
}

ssize_t buf_write(struct file *flip, const char __user *ubuf, size_t count, loff_t *f_ops){
	int i =0;
	unsigned long ret;
	int j=0, Reste=0, l=0;
	int NmbDeBoucle=0;
	int TempCount=0;
	int offset = 0;

	if(((Buffer.BufFull==1) && (flip->f_flags & O_NONBLOCK))){ 
		return -EAGAIN;
	}

	if(count>READWRITE_BUFSIZE){ //si le nombre de valeurs a ecrire est plus grand que le nombre maximum dans notre buffer intermediaire (16)
		j = count/READWRITE_BUFSIZE; //Nombre de pacquets de 16 valeurs qu'il y aura a envoyer
		Reste = count - (j*READWRITE_BUFSIZE); //le restant des valeurs apres avoir regrouper le "count" en pacquets de 16
		j++;
		NmbDeBoucle = j;	//Nombre de fois que nous devrons boucler dans la boucle pour envoyer nos valeurs puisqu'on peux envoyer seulement 16 maximum a la fois.
	}
	else{ //Si le nombre de valeurs a ecrire est plus petit ou egal a 16, on pourra les envoyer d'un seul coup donc nous allons boucler 1 fois dans la prochaine boucle
		j=1;
		Reste= count;
		NmbDeBoucle=j;
	}	

	for(i=0; i<NmbDeBoucle; i++){
		if(i>0)
			offset += 16;

		if(i==NmbDeBoucle-1){ //si c'est la derniere iteration de la boucle
			memset(BDev.WriteBuf,0,READWRITE_BUFSIZE);
			ret += copy_from_user(BDev.WriteBuf, ubuf+offset, Reste);
			TempCount = Reste; 
		}
		else{
			memset(BDev.WriteBuf,0,READWRITE_BUFSIZE);

			ret += copy_from_user(BDev.WriteBuf, ubuf+offset, 16);
			TempCount = 16;
		}

		if (ret){
			return -EFAULT;
		}

		for(l = 0; l<TempCount ; l++){
			if(Buffer.BufFull==1)//Buffer full et mode bloquant
			{
				wait_event(wq,absolue_index() >= count);
			}
			if(down_interruptible(&BDev.SemBuf))
				return -ERESTARTSYS;
			BufIn(&Buffer, &BDev.WriteBuf[l]);
			up(&BDev.SemBuf);
			wake_up(&rq);
		}
	}
	return (count - ret);
}

ssize_t buf_read (struct file *flip, char __user *ubuf, size_t count, loff_t *f_ops){
	int i = 0;
	unsigned long ret;
	int size_temp = 0;
	int offset=0;
	int charLeftToSend=0;
	int NewCount = absolue_index();

	if((Buffer.BufEmpty==1) && (flip->f_flags & O_NONBLOCK)){
		return -EAGAIN;
	}

	if((count>NewCount) && (flip->f_flags & O_NONBLOCK)){//Si on veut lire plus de valeurs qu'il y a de disponible et mode non-bloquant, on va lire seulement le nombre disponible.
		count = NewCount;
	}
	size_temp = (count+1);
	
	charLeftToSend = count / 16;
	charLeftToSend = count  - (16*charLeftToSend);

	while(size_temp>0){ //boucle tant qu'il y a des caracteres a lire
		size_temp--;	
		if(i==16){//lorsqu'on a lu 16 caracteres, il est temps de vider le ReadBuf parce qu'il est plein
			i=0;
			ret = copy_to_user(ubuf+offset, BDev.ReadBuf, READWRITE_BUFSIZE);
			memset(BDev.ReadBuf,0,READWRITE_BUFSIZE);
			if (ret)
				return -EFAULT;
			offset +=16;
		}
		if(size_temp==0 && (charLeftToSend != 0) ){//Lorsqu'il n'y a plus de caracteres a lire, on vide le ReadBuf
			ret = copy_to_user(ubuf+offset, BDev.ReadBuf, strlen(BDev.ReadBuf));
			memset(BDev.ReadBuf,0,READWRITE_BUFSIZE);
			if (ret)
				return -EFAULT;	
			return count;
		}

		
		if(Buffer.BufEmpty==1){//mode bloquant, on reveil le write
			wait_event(rq,absolue_index() >= count);
			if(down_interruptible(&BDev.SemBuf))
				return -ERESTARTSYS;
			BufOut(&Buffer, &BDev.ReadBuf[i]);
			up(&BDev.SemBuf);
		}
		else{
			if(down_interruptible(&BDev.SemBuf))
				return -ERESTARTSYS;
			BufOut(&Buffer, &BDev.ReadBuf[i]);//lit un caractere a la fois
			up(&BDev.SemBuf);
		}
		wake_up(&wq);
		i++;
	}
	return count;
}


static int __init buf_init(void){
	int result;
	
	printk(KERN_ALERT"BufferDev_init (%s:%u) => Hello, World !!!\n", __FUNCTION__, __LINE__);

	result = alloc_chrdev_region(&BDev.dev, 0, 1, "MyBufferDev");
	if (result < 0)
		printk(KERN_WARNING"BufferDev_init ERROR IN alloc_chrdev_region (%s:%s:%u)\n", __FILE__, __FUNCTION__, __LINE__);
	else
		printk(KERN_WARNING"BufferDev_init : MAJOR = %u MINOR = %u (Buffer_Var = %u)\n", MAJOR(BDev.dev), MINOR(BDev.dev), Buffer_Var);

	buffer_class = class_create(THIS_MODULE, "bufferclass");
	device_create(buffer_class, NULL, BDev.dev, &BDev, "etsele_cdev");
	cdev_init(&BDev.cdev, &Buf_fops);
	BDev.cdev.owner = THIS_MODULE;
	if (cdev_add(&BDev.cdev, BDev.dev, 1) < 0)
		printk(KERN_WARNING"BufferDev ERROR IN cdev_add (%s:%s:%u)\n", __FILE__, __FUNCTION__, __LINE__);

	Buffer.InIdx=0;
	Buffer.OutIdx=0;
	Buffer.BufFull=0;
	Buffer.BufEmpty=1;
	Buffer.BufSize=DEFAULT_BUFSIZE;

	sema_init(&BDev.SemBuf, 1); 
	init_waitqueue_head (&wq);
	init_waitqueue_head (&rq);
	
	Buffer.Buffer = kmalloc(DEFAULT_BUFSIZE * sizeof(char), GFP_KERNEL);
   	BDev.ReadBuf = kmalloc(READWRITE_BUFSIZE * sizeof(char), GFP_KERNEL);
	BDev.WriteBuf = kmalloc(READWRITE_BUFSIZE * sizeof(char), GFP_KERNEL);
	memset(Buffer.Buffer,' ',DEFAULT_BUFSIZE);
	memset(BDev.ReadBuf,' ',READWRITE_BUFSIZE);
	memset(BDev.WriteBuf,' ',READWRITE_BUFSIZE);
	
	BDev.numWriter=0;
	BDev.numReader=0;

	return 0;
}

static void __exit buf_exit(void){
	kfree(Buffer.Buffer);
	kfree(BDev.ReadBuf);
	kfree(BDev.WriteBuf);
	cdev_del(&BDev.cdev);
	unregister_chrdev_region(BDev.dev, 1);
	device_destroy (buffer_class, BDev.dev);
	class_destroy(buffer_class);
	printk(KERN_ALERT"Buffer exit(%s:%u) \n", __FUNCTION__, __LINE__);
}


