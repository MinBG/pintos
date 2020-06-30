#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"
#include "threads/synch.h"
#include "filesys/off_t.h"
void syscall_init (void);
void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
tid_t fork (char *thread_name);
int exec (char *file);
int wait (tid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
int dup2(int oldfd, int newfd);

#endif /* userprog/syscall.h */

