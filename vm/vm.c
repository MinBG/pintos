/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"

void frame_table_init (void){

		list_init(&frame_table);
		lock_init(&ft_lock);
}


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	frame_table_init();
	list_init(&(swap_table.swap_table_list));
	lock_init(&swap_lock);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	
	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;
	//printf("\n alloc page with initializer function entered\n");
	/* Check whether the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
		struct page* page=(struct page*)malloc(sizeof(struct page));

		/*using vm_type enum defined in vm.h will make things easier*/
		
		/*using vm_type enum defined in vm.h will make things easier*/
		/*fetch the initializer according to the vm type =>mentioned at TODO above*/

		bool * initializer=NULL;
		switch(type){
			case VM_ANON:
				initializer=&anon_initializer;
				break;

			case VM_FILE:
				initializer=&file_map_initializer;
				break;

#ifdef EFILESYS  /*For project 4 */
			case VM_PAGE_CACHE:/* this is for project 4. I think not need to be implemented  */
				
				break;
#endif
		}
		uninit_new(page,pg_round_down(upage),init,type,aux,initializer); /*here, page means struct page*/
		
		page->writable=writable;
		page->is_stack=false;
		page->page_id=page_id;
		page_id++;

		spt_insert_page(spt, page); /*here, page means struct page*/
		//bool succ = vm_do_claim_page(page);
		//if(succ){printf("alloc with initializer function - do claim page success \n");}
		//printf("alloc page with initializer function finished\n");
		return true;

	}	
err:
	//printf("null page\n");
	//printf("alloc page with initializer function finished\n");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	//printf("spt find page function-entered\n");
	//printf("1");
	struct page *page = NULL;
	/* TODO: Fill this function. */	
	struct page *temp_page_pointer=NULL;
	struct list *temp_list = &spt->spt_list;
	if (!list_empty(temp_list)) {
		for (struct list_elem * i = list_begin(&spt->spt_list); i != list_end(&spt->spt_list); i = i->next) {
			temp_page_pointer=( (struct supplemental_page_table_elem*)list_entry( i ,struct supplemental_page_table_elem ,spt_elem))->page;
			if(temp_page_pointer->va==pg_round_down(va)){
				page=temp_page_pointer;
				break;
			}
		}
	}

	//printf("spt find page function-finished\n");
	//printf("2\n");
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	//printf("spt insert page function-entered\n");
	int succ = false;

	/* TODO: Fill this function. */	
	/*document says this function should check that virtual address does not exist in spt */

	if (spt_find_page(spt, page->va)!=NULL){
		return succ;
	}
	if (!lock_held_by_current_thread(&spt_lock)) {
		lock_acquire(&spt_lock);
	}
	struct supplemental_page_table_elem *spt_elem_insert = 
		(struct supplemental_page_table_elem *) malloc(sizeof(struct supplemental_page_table_elem));

	spt_elem_insert->page = page; // spt_elem->page should be given page through argument
	page->spt_elem=spt_elem_insert;
	//struct thread* owner=spt->owner_thread;
	//spt_elem_insert->owner_thread=owner;

	list_push_back (&spt->spt_list,  &spt_elem_insert->spt_elem); //push to list
	if (spt_find_page(spt, page->va)!=NULL){
		succ = true;
	}
	//printf("spt insert page function-finished\n");
	if (lock_held_by_current_thread(&spt_lock)){
			lock_release(&spt_lock);}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	/*I will use clock algorithm. Refer to lecture 08_caching_virtual_memory ppt*/ 
	enum intr_level old_level;
	int max_unaccessed=0;
	old_level = intr_disable();
	if (!lock_held_by_current_thread(&ft_lock)) {
	lock_acquire(&ft_lock);
	} 
	for (struct list_elem * i = list_begin(&frame_table); i != list_end(&frame_table); i = i->next) {
		struct frame*temp_frame=(struct frame*)list_entry( i ,struct frame ,ft_elem);
		struct page* temp_page=temp_frame->page;
		if(!pml4_is_accessed(temp_frame->owner_thread->pml4,pg_round_down(temp_page->va))){
			temp_page->unaccessed_time=0;
			pml4_set_accessed(temp_frame->owner_thread->pml4,pg_round_down(temp_page->va),false);
		}else{
			temp_page->unaccessed_time++;
			if(temp_page->unaccessed_time>=max_unaccessed){
				victim=temp_frame;
				break;
			}
		}
	}

	if (victim == NULL) {
		struct list_elem *i = list_begin(&frame_table);
		struct frame*temp_frame=(struct frame*)list_entry( i ,struct frame ,ft_elem);
		victim = temp_frame;
	}
	lock_release(&ft_lock);
	intr_set_level(old_level);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	/* TODO: swap out the victim and return the evicted frame. */
	enum intr_level old_level;
	old_level = intr_enable();
	if (!lock_held_by_current_thread(&ft_lock)) {
	lock_acquire(&ft_lock);
	}
	struct frame *frame=vm_get_victim();
	if(frame==NULL){return frame;}
	/*spt table and struct page must not be destroyed. must be alive*/
	swap_out(frame->page);
	if(frame->kva!=NULL){pml4_clear_page(frame->owner_thread->pml4, pg_round_down(frame->page->va));}
	list_remove(&(frame->ft_elem));
	frame->owner_thread=thread_current();
	frame->page=NULL;
	if (lock_held_by_current_thread(&ft_lock)){
		lock_release(&ft_lock);}
	//printf("\nevict end\n");
	return frame;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
   enum intr_level old_level;
   void *page = palloc_get_page(PAL_USER);
   if (page == NULL) { //page allocation failure 
      frame=vm_evict_frame();
   } else {
      frame = (struct frame *)malloc(sizeof(struct frame));
      frame->kva=  page;
   }
   
   /*todo: allocates a frame. initialize its members and returns it*/
   if (frame == NULL) {
      return NULL;
   }
   
   old_level = intr_disable();
   if (!lock_held_by_current_thread(&ft_lock)) {
      lock_acquire(&ft_lock);
   }
   list_push_back(&frame_table, &frame->ft_elem);  /*add frame to frame table.*/
   if (lock_held_by_current_thread(&ft_lock)) {
      lock_release(&ft_lock);}
   intr_set_level(old_level);
   /* code flow :vm_claim_page->vm_do_claim_page->vm_get_frame  */
   frame->owner_thread=thread_current();

   ASSERT (frame != NULL);
   //ASSERT (frame->page == NULL);
   return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	//printf("entered stack_growth\n");
	void *target=pg_round_down(addr);
	uint32_t diff=((uint32_t)stack_btm-(uint32_t)target)/PGSIZE;
	//printf("allocate %d pages\n", diff);
	while(stack_btm!=target){
		//printf("yaho\n");
		if( !vm_alloc_page_with_initializer(VM_ANON,pg_round_down(stack_btm-PGSIZE),true,NULL,NULL)){return false;}
		if(!vm_claim_page(pg_round_down(stack_btm-PGSIZE))){return false;}
		struct page*stack_page=spt_find_page(&(thread_current()->spt), pg_round_down(stack_btm-PGSIZE));
		if(stack_page==NULL){return false;}
		stack_page->is_stack=true;
		stack_btm=stack_btm-PGSIZE;
	}


}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
      bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
   //addr is same as fault addr
   //printf("\nvm try handle function entered\n");
   struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
   struct page *page = NULL;
   /* TODO: Validate the fault */
   /* TODO: Your code goes here */
   page=spt_find_page(spt,addr);
   uintptr_t rsp;
   //if(write&&(!(page->writable))){exit(-1);}
   if(!not_present){exit(-1);}
   if (page==NULL){
      if(user){rsp=f->rsp;
      }else{rsp=thread_current()->rsp_saver;}
      if(USER_STACK-(uint32_t)pg_round_down(addr)<=0x100000 && (uint32_t*)addr>=(rsp-64)){
         vm_stack_growth(addr);
         return true;
      }else{
         return false;
      }
   }

   return vm_do_claim_page (page);
}


/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&(thread_current()->spt),va);
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	//printf("pml4 set page func\n");	
	uint64_t *pml4=thread_current()->pml4;
	pml4_set_page(pml4, pg_round_down(page->va), frame->kva,page->writable); //-> do this at swap_in
	//printf("pml4 set page func finished\n");

	/*return value should indicate whether the operation was successful of not*/
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	list_init(&spt->spt_list); //initialize spt_list
	spt->owner_thread = thread_current(); //denote which thread owns this spt 
	lock_init(&spt_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
 struct supplemental_page_table *src UNUSED) {
	//printf("copy\n");
	struct list * src_list = &src->spt_list;
	struct list * dst_list = &dst->spt_list;
	if (!lock_held_by_current_thread(&spt_lock)){
			lock_acquire(&spt_lock);}
	for (struct list_elem *i = list_begin(src_list); i != list_end(src_list); i = i->next) {
		struct page* temp_page=( (struct supplemental_page_table_elem*)list_entry( i ,struct supplemental_page_table_elem ,spt_elem))->page;
		
		struct struct_aux * aux_info=NULL; 
		bool * initializer;
		struct vm_initializer * init = NULL;
		struct file * file;
		if (temp_page->operations->type == VM_UNINIT) {
			if (temp_page->uninit.aux != NULL) {
				struct struct_aux * aux = temp_page->uninit.aux;
				aux_info=(struct struct_aux*)malloc(sizeof(struct struct_aux));
				aux_info->file=file_reopen(aux->file);
				aux_info->read_byte=aux->read_byte;
				aux_info->pos=aux->pos;
			}
			if (temp_page->uninit.init != NULL) {
				init = temp_page->uninit.init;
			}
		}
		if (!vm_alloc_page_with_initializer(page_get_type(temp_page), temp_page->va, temp_page->writable, init,aux_info)){
			if (lock_held_by_current_thread(&spt_lock)){lock_release(&spt_lock);}
			return false;}
		struct page *npage = spt_find_page(dst,temp_page->va);
		if (npage==NULL) {
			if (lock_held_by_current_thread(&spt_lock)){lock_release(&spt_lock);}
			return false;}
		npage->writable=temp_page->writable;
		npage->is_stack=temp_page->is_stack;
		npage->page_id=page_id;
		page_id++;
		if (!vm_do_claim_page(npage)) {
			if (lock_held_by_current_thread(&spt_lock)){lock_release(&spt_lock);}
			return false;}
		if (temp_page->frame != NULL) {
			memcpy(npage->frame->kva, temp_page->frame->kva, PGSIZE);
		}
		if (temp_page->operations->type == VM_FILE) {
			npage->file.file = temp_page->file.file;
			npage->file.pos = temp_page->file.pos;
			npage->file.zeronum = temp_page->file.zeronum;
		}
	}
	if (lock_held_by_current_thread(&spt_lock)){
			lock_release(&spt_lock);}
	return true;
}



/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	//printf("supplemental page table kill\n");
	struct list * src_list = &spt->spt_list;
	//if(list_empty(src_list)){printf("list is empty\n");}
	if (!lock_held_by_current_thread(&spt_lock)){lock_acquire(&spt_lock);}
	while (list_size(src_list) != 0) {
		//printf("testing: %d\n",(int)list_size(src_list));
		struct list_elem *i = list_pop_front(src_list);
		struct supplemental_page_table_elem * temp_spt_elem = (struct supplemental_page_table_elem*) list_entry( i ,struct supplemental_page_table_elem ,spt_elem);
		struct page *temp_page= temp_spt_elem->page;

		/*if (temp_page->operations->type == VM_FILE) {
			swap_out(temp_page);
		}*/

		vm_dealloc_page(temp_page);
		temp_spt_elem->page=NULL;
		list_remove(&(temp_spt_elem->spt_elem));
		free(temp_spt_elem);
	}
	if (lock_held_by_current_thread(&spt_lock)){lock_release(&spt_lock);}
	//printf("all deleted?:%d\n",list_size(src_list));
}

