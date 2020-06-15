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
	file_page->zeronum = 0;
	//printf("init\n");
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_map_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	off_t bytes_read=file_read_at(file_page->file,pg_round_down(kva),
		PGSIZE-(file_page->zeronum),file_page->pos);
	if (bytes_read != (PGSIZE-(file_page->zeronum))) {return false;}
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_map_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	if (!file_page->file->deny_write) {
		off_t bytes_read=file_write_at(file_page->file,pg_round_down(page->va),
			PGSIZE-(file_page->zeronum),file_page->pos);
		if (bytes_read != (PGSIZE-(file_page->zeronum))) {return false;}
	}
	return true;
}

/* Destory the file mapped page. PAGE will be freed by the caller. */
static void
file_map_destroy (struct page *page) {
	
	struct file_page *file UNUSED = &page->file;
	if(page->frame!=NULL){
		if(page->frame->kva!=NULL){
			if (pml4_is_accessed(thread_current()->pml4,pg_round_down(page->va))) {
				file_write_at(file->file,pg_round_down(page->frame->kva),
						PGSIZE-(file->zeronum),file->pos);
			}
			pml4_clear_page(page->frame->owner_thread->pml4, pg_round_down(page->va));}
		enum intr_level old_level;
		old_level = intr_disable();
		if (!lock_held_by_current_thread(&ft_lock)) {
			lock_acquire(&ft_lock);
		}
		list_remove(&(page->frame->ft_elem)); //after adding this, pt-write-code fails
		if(page->frame->kva!=NULL){palloc_free_page(pg_round_down(page->frame->kva));}
		lock_release(&ft_lock);
		intr_set_level(old_level);
		page->frame->page=NULL;
		page->frame->owner_thread=NULL;
		free(page->frame);
	}
	page->operations=NULL;
	page->va=NULL;
	/*<free process required if some parts of struct anon_page are created using memory allocation>*/
	file->file = NULL;
	file->pos = NULL;
	file->zeronum = NULL;
	return true;	
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	off_t bytes_read;
	struct struct_aux * aux;
	if ((size_t)addr % PGSIZE != 0|| offset % PGSIZE != 0) {return NULL;}
	void * ret = addr;
	length = length < (file_length(file)-offset) ? length : (file_length(file)-offset);
	if (spt_find_page(&thread_current()->spt,addr) != NULL) {return NULL;}
	while (length > 0){
		aux = (struct struct_aux*)malloc(sizeof(struct struct_aux));
		if (length > PGSIZE) {
			aux->read_byte=PGSIZE;
			length -=PGSIZE;
		} else {
			//printf("len %d\n",(int)length);
			aux->read_byte=length;
			length = 0;
		}
		aux->file=file_reopen(file);
		aux->pos = offset;
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, &lazy_load_segment, aux)){
			do_munmap(ret);
			return NULL;
		}
		offset += PGSIZE;
		addr += PGSIZE;
	}
	return ret;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	//printf("do munmap\n");
	struct page * page = spt_find_page(&thread_current()->spt,addr);
	//struct inode * inode;
	while (page != NULL){
		if (page_get_type(page) != VM_FILE) {
			break;
		}
		list_remove(&(page->spt_elem->spt_elem));
		vm_dealloc_page(page);
		
		addr += PGSIZE;
		page = spt_find_page(&thread_current()->spt,addr);
	}
}
