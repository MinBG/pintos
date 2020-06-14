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
	//printf("vm_anon_init\n");
	struct disk *swap_disk = disk_get(1,1);
	if (swap_disk != NULL) {
		//printf("anon init success\n");
		swap_table.sector_max=disk_size(swap_disk)/512-1;
		swap_table.sector_available=(bool*)malloc(sizeof(bool)*disk_size(swap_disk)/512);
		int i=0;
		for(i=0;i<swap_table.sector_max+1;i++){
				*(swap_table.sector_available+i)=true;
		}
	}
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
	struct anon_page *anon_page = &page->anon;
	if(list_empty(&(swap_table.swap_table_list))){
		return false;
	}
	struct swap_table_elem *swap_table_node=NULL;
	for (struct list_elem * i = list_begin(&swap_table.swap_table_list); 
		i != list_end(&swap_table.swap_table_list); i = i->next) {
		if (((struct swap_table_elem*)list_entry( i ,struct swap_table_elem 
			,swap_table_elem))->page_id==page->page_id){
			swap_table_node=(struct swap_table_elem*)list_entry( i ,
				struct swap_table_elem ,swap_table_elem);
			break;
		}
	}
	if(swap_table_node==NULL){return false;}
	int i=0;
	for(i=0;i<8;i++){
		swap_table.sector_available[swap_table_node->sector_place[i]]=true;
		disk_read(swap_disk,swap_table_node->sector_place[i], kva+i*512);
	}
	list_remove(swap_table_node);
	free(swap_table_node);
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if(!is_swap_table_list_initialized){
		list_init(&(swap_table.swap_table_list));
	}
	struct swap_table_elem * swap_table_node=(struct swap_table_elem*)malloc(
		sizeof(struct swap_table_elem));
	swap_table_node->va=page->va;
	uint32_t i, j=0;
	for(i=0;i<=swap_table.sector_max;i++){ //find available sectors
		if(swap_table.sector_available[i]==true){
			swap_table_node->sector_place[j]=i;
			j++;
		}
		if(j>=7){
			break;
		}
	}
	if(j<7){
		free(swap_table_node);
		PANIC("no disk slots availble");
	} //if available sectors are less than 8, kernel panic                                           
	for(i=0;i<8;i++){  
		swap_table.sector_available[swap_table_node->sector_place[i]]=false; 
		disk_write(swap_disk, swap_table_node->sector_place[i],(page->frame->kva+i*512)); 
	}
	list_push_back(&swap_table.swap_table_list, &swap_table_node->swap_table_elem);
	page->frame=NULL;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	//printf("anon destroy\n");
	struct anon_page *anon_page = &page->anon;
	page->operations=NULL;
	page->va=NULL;
	if(page->frame!=NULL){
		page->frame->page=NULL;
		page->frame->owner_thread=NULL;
		free(page->frame);
	}
	/*
	<free process required if some parts of struct anon_page are created using memory allocation>


	*/
}
