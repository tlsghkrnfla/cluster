#include <linux/file.h>
#include <linux/pagemap.h>

struct page *cluster_readpage(struct address_space *, pgoff_t);
struct page *cluster_do_page_cache_readahead(struct address_space *, struct file *, pgoff_t,
																unsigned long, unsigned long);
struct page *cluster_page_cache_sync_readahead(struct address_space *, struct file_ra_state *,
														struct file *, pgoff_t, unsigned long);
void cluster_page_cache_async_readahead(struct address_space *, struct file_ra_state *, struct file *,
												struct page *, pgoff_t offset, unsigned long req_size);

