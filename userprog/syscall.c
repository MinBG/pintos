#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "lib/kernel/console.h"
#include "lib/string.h"
#include "threads/synch.h"
#include "threads/interrupt.h"
#include "vm/file.h"

struct lock read_write_lock;

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&read_write_lock);
}

/* The main system call interface */

void
syscall_handler (struct intr_frame *f UNUSED) { 
	// TODO: Your implementation goes here.
	//printf ("system call!\n");
	thread_current()->rsp_saver=f->rsp;
	switch(f->R.rax){
		case SYS_HALT: 
			halt();
			break;
		case SYS_EXIT:
			exit((int)f->R.rdi);
			break;
		case SYS_FORK: 		
			memcpy(&thread_current()->tf, f, sizeof (struct intr_frame));
			f->R.rax = (uint64_t)fork((char *)f->R.rdi);
			break;
		case SYS_EXEC: 
			f->R.rax = (uint64_t)exec((char*)f->R.rdi);
			break;
		case SYS_WAIT: 
			f->R.rax = (uint64_t)wait((tid_t)f->R.rdi);
			break;
		case SYS_CREATE: 
			f->R.rax = (uint64_t)create((const char *)f->R.rdi,(unsigned) f->R.rsi);
			break;
		case SYS_REMOVE: 
			f->R.rax = (uint64_t)remove((const char *)f->R.rdi);
			break;
		case SYS_OPEN: 
			f->R.rax = (uint64_t)open((const char *)f->R.rdi);
			break;
		case SYS_FILESIZE: 
			f->R.rax = (uint64_t)filesize((int)f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = (uint64_t)read((int)f->R.rdi,(void*)f->R.rsi, (unsigned)f->R.rdx);
			break;
		case SYS_WRITE: 
			f->R.rax = (uint64_t)write((int)f->R.rdi, (const void *)f->R.rsi, (unsigned)f->R.rdx);
			break;
		case SYS_SEEK: 
			seek((int)f->R.rdi,(unsigned)f->R.rsi);
			break;
		case SYS_TELL: 
			f->R.rax = (uint64_t)tell((int)f->R.rdi);
			break;
		case SYS_CLOSE: 
			close((int)f->R.rdi);
			break;
		/* Project 3 and optionally project 4. */
		case SYS_MMAP:
			f->R.rax =(uint64_t)mmap((void *)f->R.rdi,(size_t)f->R.rsi,(int)f->R.rdx,(int)f->R.r10,(off_t)f->R.r8);
			break;
		case SYS_MUNMAP:
			munmap((void *)f->R.rdi);
			break;
		default: 
			exit(-1);
	}
} 


void
halt(void) {
	power_off();
}

void
exit(int status) {
	enum intr_level old_level;
	old_level = intr_disable();
	thread_current()->exit_status=status;
	printf("%s: exit(%d)\n", thread_current()->name,status);
	for (int i = 2;i<128;i++) {
		if (thread_current()->fd_list[i] != NULL) {
			file_close(thread_current()->fd_list[i]);
			thread_current()->fd_list[i] = NULL;
		}
	}
	while (!list_empty(&thread_current()->child)) {
		struct thread * t = list_entry(list_begin(&thread_current()->child),struct thread,child_elem);
		wait(t->tid);
	}
	intr_set_level(old_level);
	thread_exit();
}

tid_t
fork (char *thread_name){
	if(is_kernel_vaddr(thread_name)){exit(-1);}
	tid_t result = process_fork(thread_name,&thread_current()->tf);
	return result;
}


int
exec (char *file){
	enum intr_level old_level;
	if(is_kernel_vaddr(file)){exit(-1);}
	int str_length=strlen(file);
	char file_name[str_length+1];
	strlcpy(file_name,file,str_length+1);
	process_exec(file_name);
	return -1;
	NOT_REACHED ();
}

int
wait (tid_t pid){
	return (int) process_wait(pid);
}

bool
create (const char *file, unsigned initial_size){
	if(is_kernel_vaddr(file)){exit(-1);}
	if (file == NULL) {exit(-1);}
	return filesys_create(file, initial_size);
}

bool
remove (const char *file){
	if(is_kernel_vaddr(file)){exit(-1);}
	return filesys_remove(file);
}

int
open (const char *file){
	if(file==NULL){exit(-1);} 
	enum intr_level old_level;
	struct file * opened;
	
	old_level = intr_enable();
	opened = filesys_open(file);
	intr_set_level(old_level);
	if (opened == NULL) {
		intr_set_level(old_level);
		return -1; } 
	if (thread_current()->fd_num > 126) {
		file_close(opened);
		intr_set_level(old_level);	
		return -1;
	}
	if (strcmp(thread_current()->name,file) == 0) {file_deny_write(opened);}
	thread_current()->fd_list[thread_current()->fd_num+2] = opened;

	thread_current()->fd_num += 1;
//	printf("%d fd\n",thread_current()->fd_num);
	return thread_current()->fd_num+1;
}

int
filesize (int fd){
	if((fd<0)||(fd>127)){ exit(-1); }
	struct file * opened = thread_current()->fd_list[fd];
	return (int) file_length(opened);
}


int
read (int fd, void *buffer, unsigned length){
	enum intr_level old_level;
	if(is_kernel_vaddr(buffer)){exit(-1);}
	if((spt_find_page(&(thread_current()->spt),buffer)->writable)==false){exit(-1);}
	if((fd<0)||(fd>127)){ //not valid fd
		exit(-1);
	}
	old_level = intr_enable();
	lock_acquire(&read_write_lock);
	int return_value=0;
	if (fd == 0) {
		while (length > 0) {
			*(char *)buffer ++= input_getc();
			length -= 1;
		}
		thread_current()->fd_list[0] = buffer;
		return_value=(int) strlen(buffer);
		lock_release(&read_write_lock);
		intr_set_level(old_level);
		return return_value;
	}
	struct file * opened = thread_current()->fd_list[fd];

//	file_deny_write(opened);
	return_value=(int) file_read(opened, buffer, (off_t) length);
	lock_release(&read_write_lock);
	intr_set_level(old_level);
	return return_value;
}

int
write (int fd, const void *buffer, unsigned length){ // denying write code must be added - file->deny_write (bool) 
	if((fd<0)||(fd>127)){ //not valid fd
		exit(-1);
}
	enum intr_level old_level = intr_enable();
	lock_acquire(&read_write_lock);
	int return_value=0;
	if (fd == 1) { // write in console
		putbuf(buffer, length);
		return_value=length;
		lock_release(&read_write_lock);
		intr_set_level(old_level);
		return return_value;
		}
	else if (fd >=2) {
		struct file * opened = thread_current()->fd_list[fd];

		return_value=(int) file_write(opened, buffer, (off_t) length);
		lock_release(&read_write_lock);
		intr_set_level(old_level);
		return return_value;
	}
	lock_release(&read_write_lock);
	intr_set_level(old_level);
	return return_value;
}





void
seek (int fd, unsigned position){
	if (fd<0||fd>127) {exit(-1);}
	struct file * opened = thread_current()->fd_list[fd];
	file_seek(opened,(off_t) position);
}

unsigned
tell (int fd){
	if (fd<0||fd>127) {exit(-1);}
	struct file * opened = thread_current()->fd_list[fd];
	return (unsigned) file_tell(opened);
}

void
close (int fd){
	if (fd<0||fd>127) {exit(-1);}
	struct file * opened = thread_current()->fd_list[fd];
	thread_current()->fd_list[fd] = NULL;
	if (opened == NULL) {exit(-1);}

	file_close(opened);
}


/* Project 3 and optionally project 4. */
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset){
	if(0x1000< addr && addr < KERN_BASE){
		if (0x1000< (addr+length) && (addr+length) < KERN_BASE){
			if (fd == 0 || fd == 1 || length == 0 || addr == 0 || addr == NULL) {return NULL;}
			struct file * opened = thread_current()->fd_list[fd];
			if (opened == NULL) {return NULL;}
			void * ret = do_mmap(addr, length, writable, opened, offset);
			return ret;
		}
	}
	return NULL;
}

void munmap (void *addr){
	//printf("munmap\n");
	if(is_kernel_vaddr(addr)){exit(-1);}
	do_munmap(addr);
}

