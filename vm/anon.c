/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	if(is_disk_set){
		swap_disk = disk_get(1,1);
		swap_table.sector_max=disk_size(swap_disk)/512-1;
		swap_table.sector_available=(bool*)malloc(sizeof(bool)*disk_size(swap_disk)/512);
		int i=0;
		for(i=0;i<swap_table.sector_max+1;i++){
			*(swap_table.sector_available+i)=true;
		}
	} else {is_disk_set = true;}
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	//printf("anon initializer\n");
//	vm_anon_init();
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	//printf("swap in, id:%d\n",page->page_id);
	struct anon_page *anon_page = &page->anon;
	if(list_empty(&(swap_table.swap_table_list))){
		return false;
	}
	struct swap_table_elem *swap_table_node=NULL;
	//printf("test1");
	for (struct list_elem * i = list_begin(&swap_table.swap_table_list); 
		i != list_end(&swap_table.swap_table_list); i = i->next) {
		if (((struct swap_table_elem*)list_entry( i ,struct swap_table_elem 
			,swap_table_elem))->page_id==page->page_id){
			swap_table_node=(struct swap_table_elem*)list_entry( i ,
				struct swap_table_elem ,swap_table_elem);
			break;
		}
	}
	//if (page->va == swap_table_node->va) {printf("wow");}
	//printf("test2");
	if(swap_table_node==NULL){return false;}
	int i=0;
	for(i=0;i<8;i++){
		swap_table.sector_available[swap_table_node->sector_place[i]]=true;
		disk_read(swap_disk,swap_table_node->sector_place[i], kva+i*512);
	}
	//printf("test3");
	list_remove(&swap_table_node->swap_table_elem);
	swap_table_node->va = NULL;
	swap_table_node->page_id = NULL;
	free(swap_table_node);
	//printf("end\n");
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	//printf("swap out, id:%d\n",page->page_id);
	struct anon_page *anon_page = &page->anon;

	struct swap_table_elem * swap_table_node=(struct swap_table_elem*)malloc(
		sizeof(struct swap_table_elem));
	swap_table_node->va=page->va;
	swap_table_node->page_id=page->page_id;
	uint32_t i, j=0;
	for(i=0;i<=swap_table.sector_max;i++){ //find available sectors
		if(swap_table.sector_available[i]==true){
			swap_table_node->sector_place[j]=i;
			j++;
		}
		if(j==8){
			break;
		}
	}
	if(j<8){
		free(swap_table_node);
		PANIC("no disk slots availble");
	} //if available sectors are less than 8, kernel panic                                           
	for(i=0;i<8;i++){  
		swap_table.sector_available[swap_table_node->sector_place[i]]=false; 
		disk_write(swap_disk, swap_table_node->sector_place[i],(page->frame->kva+i*512)); 
	}
	list_push_back(&swap_table.swap_table_list, &swap_table_node->swap_table_elem);
	page->frame=NULL;
	//printf("done\n");
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
   //printf("\n anon destroy function entered\n");
   struct anon_page *anon_page = &page->anon;
   if(page->frame!=NULL){
      if(page->frame->kva!=NULL){pml4_clear_page(page->frame->owner_thread->pml4, pg_round_down(page->va));}
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
   /*
   <free process required if some parts of struct anon_page are created using memory allocation>


   */
   return true;
}
