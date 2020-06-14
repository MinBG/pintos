/* file.c: Implementation of memory mapped file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"

static bool file_map_swap_in (struct page *page, void *kva);
static bool file_map_swap_out (struct page *page);
static void file_map_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_map_swap_in,
	.swap_out = file_map_swap_out,
	.destroy = file_map_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file mapped page */
bool
file_map_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_map_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_map_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	
	//file_write_at();
}

/* Destory the file mapped page. PAGE will be freed by the caller. */
static void
file_map_destroy (struct page *page) {
	
	struct file_page *file UNUSED = &page->file;
	file_write_at(file->file,page->va,PGSIZE-file->zeronum,file->pos);
	page->operations=NULL;
	page->va=NULL;
	if(page->frame!=NULL){
		page->frame->page=NULL;
		page->frame->owner_thread=NULL;
		free(page->frame);
	}
	file_close(file->file);
	file->file = NULL;
	file->pos = NULL;
	file->zeronum = NULL;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	off_t bytes_read;
	struct struct_aux * aux;
	if ((int)addr % PGSIZE !=0) {return NULL;}
	while (length > 0){
		//printf("do mmap len:%d\n",length);
		if (spt_find_page(&thread_current()->spt,addr) != NULL) {return NULL;}
		aux = (struct struct_aux*)malloc(sizeof(struct struct_aux));
		aux->file=file_duplicate(file);
		aux->read_byte=PGSIZE;
		aux->pos = offset;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, &lazy_load_segment, aux)){
			return NULL;
		}
		if (length > PGSIZE) {
			length -=PGSIZE;
			offset += PGSIZE;
			addr += PGSIZE;
		} else {
			length = 0;
			offset += PGSIZE;
			addr += PGSIZE;
		}
	}
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	printf("do munmap\n");
	struct page * page = spt_find_page(&thread_current()->spt,addr);
	struct inode * inode;
	size_t len;
	while (page != NULL){
		if (page_get_type(page) != VM_FILE) {
			break;
		}
		if (page->operations->type == VM_UNINIT) {
			struct struct_aux * aux = page->uninit.aux;
			if (inode == NULL) {inode = file_get_inode(aux->file);
			} else if (file_get_inode(aux->file) != inode) {
				break;
			}
			len = aux->pos;
		} else {
			len = page->file.pos;
			if (inode == NULL) {inode = file_get_inode(page->file.file);}
		}
		printf("do munmap len:%d\n",len);
		vm_dealloc_page(page);
		list_remove(&page->spt_elem);
		addr -= PGSIZE;
		page = spt_find_page(&thread_current()->spt,addr);
	}
}
