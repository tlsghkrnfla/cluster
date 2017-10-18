



struct page *cluster_pagecache_get_page(struct address_space *mapping, pgoff_t offset,
	int fgp_flags, gfp_t gfp_mask)
{
	struct page *page;

repeat:
	page = find_get_entry(mapping, offset);
	if (radix_tree_exceptional_entry(page))
		page = NULL;
	if (!page)
		goto no_page;

	if (fgp_flags & FGP_LOCK) {
		if (fgp_flags & FGP_NOWAIT) {
			if (!trylock_page(page)) {
				page_cache_release(page);
				return NULL;
			}
		} else {
			lock_page(page);
		}

		/* Has the page been truncated? */
		if (unlikely(page->mapping != mapping)) {
			unlock_page(page);
			page_cache_release(page);
			goto repeat;
		}
		VM_BUG_ON_PAGE(page->index != offset, page);
	}

	if (page && (fgp_flags & FGP_ACCESSED))
		mark_page_accessed(page);

no_page:
	if (!page && (fgp_flags & FGP_CREAT)) {
		int err;
		if ((fgp_flags & FGP_WRITE) && mapping_cap_account_dirty(mapping))
			gfp_mask |= __GFP_WRITE;
		if (fgp_flags & FGP_NOFS)
			gfp_mask &= ~__GFP_FS;

		//////////////////// need revise
		//////////////////////
		page = __page_cache_alloc(gfp_mask);
		if (!page)
			return NULL;

		if (WARN_ON_ONCE(!(fgp_flags & FGP_LOCK)))
			fgp_flags |= FGP_LOCK;

		/* Init accessed so avoid atomic mark_page_accessed later */
		if (fgp_flags & FGP_ACCESSED)
			__SetPageReferenced(page);

		err = add_to_page_cache_lru(page, mapping, offset,
				gfp_mask & GFP_RECLAIM_MASK);
		if (unlikely(err)) {
			page_cache_release(page);
			page = NULL;
			if (err == -EEXIST)
				goto repeat;
		}
	}

	return page;
}
EXPORT_SYMBOL(pagecache_get_page);

struct page *cluster_grab_cache_page_write_begin(struct address_space *mapping,
					pgoff_t index, unsigned flags)
{
	struct page *page;
	int fgp_flags = FGP_LOCK|FGP_ACCESSED|FGP_WRITE|FGP_CREAT;

	if (flags & AOP_FLAG_NOFS)
		fgp_flags |= FGP_NOFS;

	page = cluster_pagecache_get_page(mapping, index, fgp_flags,
			mapping_gfp_mask(mapping));
	if (page)
		wait_for_stable_page(page);

	return page;
}
EXPORT_SYMBOL(grab_cache_page_write_begin);

//////////////////////////////
#ifdef CONFIG_EXT4_FS_ENCRYPTION
static int ext4_block_write_begin(struct page *page, loff_t pos, unsigned len,
				  get_block_t *get_block)
{
	unsigned from = pos & (PAGE_CACHE_SIZE - 1);
	unsigned to = from + len;
	struct inode *inode = page->mapping->host;
	unsigned block_start, block_end;
	sector_t block;
	int err = 0;
	unsigned blocksize = inode->i_sb->s_blocksize;
	unsigned bbits;
	struct buffer_head *bh, *head, *wait[2], **wait_bh = wait;
	bool decrypt = false;

	BUG_ON(!PageLocked(page));
	BUG_ON(from > PAGE_CACHE_SIZE);
	BUG_ON(to > PAGE_CACHE_SIZE);
	BUG_ON(from > to);

	if (!page_has_buffers(page))
		create_empty_buffers(page, blocksize, 0);
	head = page_buffers(page);
	bbits = ilog2(blocksize);
	block = (sector_t)page->index << (PAGE_CACHE_SHIFT - bbits);

	for (bh = head, block_start = 0; bh != head || !block_start;
	    block++, block_start = block_end, bh = bh->b_this_page) {
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (PageUptodate(page)) {
				if (!buffer_uptodate(bh))
					set_buffer_uptodate(bh);
			}
			continue;
		}
		if (buffer_new(bh))
			clear_buffer_new(bh);
		if (!buffer_mapped(bh)) {
			WARN_ON(bh->b_size != blocksize);
			err = get_block(inode, block, bh, 1);
			if (err)
				break;
			if (buffer_new(bh)) {
				unmap_underlying_metadata(bh->b_bdev,
							  bh->b_blocknr);
				if (PageUptodate(page)) {
					clear_buffer_new(bh);
					set_buffer_uptodate(bh);
					mark_buffer_dirty(bh);
					continue;
				}
				if (block_end > to || block_start < from)
					zero_user_segments(page, to, block_end,
							   block_start, from);
				continue;
			}
		}
		if (PageUptodate(page)) {
			if (!buffer_uptodate(bh))
				set_buffer_uptodate(bh);
			continue;
		}
		if (!buffer_uptodate(bh) && !buffer_delay(bh) &&
		    !buffer_unwritten(bh) &&
		    (block_start < from || block_end > to)) {
			ll_rw_block(READ, 1, &bh);
			*wait_bh++ = bh;
			decrypt = ext4_encrypted_inode(inode) &&
				S_ISREG(inode->i_mode);
		}
	}
	/*
	 * If we issued read requests, let them complete.
	 */
	while (wait_bh > wait) {
		wait_on_buffer(*--wait_bh);
		if (!buffer_uptodate(*wait_bh))
			err = -EIO;
	}
	if (unlikely(err))
		page_zero_new_buffers(page, from, to);
	else if (decrypt)
		err = ext4_decrypt(page);
	return err;
}
#endif
/////////////////////////////////

static int cluster_ext4_da_write_begin(struct file *file, struct address_space *mapping,
			       loff_t pos, unsigned len, unsigned flags,
			       struct page **pagep, void **fsdata)
{
	int ret, retries = 0;
	struct page *page;
	pgoff_t index;
	struct inode *inode = mapping->host;
	handle_t *handle;

	index = pos >> PAGE_CACHE_SHIFT;

	if (ext4_nonda_switch(inode->i_sb)) {
		*fsdata = (void *)FALL_BACK_TO_NONDELALLOC;
		return ext4_write_begin(file, mapping, pos,
					len, flags, pagep, fsdata);
	}
	*fsdata = (void *)0;
	trace_ext4_da_write_begin(inode, pos, len, flags);

	if (ext4_test_inode_state(inode, EXT4_STATE_MAY_INLINE_DATA)) {
		ret = ext4_da_write_inline_data_begin(mapping, inode,
						      pos, len, flags,
						      pagep, fsdata);
		if (ret < 0)
			return ret;
		if (ret == 1)
			return 0;
	}

	/*
	 * grab_cache_page_write_begin() can take a long time if the
	 * system is thrashing due to memory pressure, or if the page
	 * is being written back.  So grab it first before we start
	 * the transaction handle.  This also allows us to allocate
	 * the page (if needed) without using GFP_NOFS.
	 */
retry_grab:
	page = cluster_grab_cache_page_write_begin(mapping, index, flags);
	if (!page)
		return -ENOMEM;
	unlock_page(page);

	/*
	 * With delayed allocation, we don't log the i_disksize update
	 * if there is delayed block allocation. But we still need
	 * to journalling the i_disksize update if writes to the end
	 * of file which has an already mapped buffer.
	 */
retry_journal:
	handle = ext4_journal_start(inode, EXT4_HT_WRITE_PAGE,
				ext4_da_write_credits(inode, pos, len));
	if (IS_ERR(handle)) {
		page_cache_release(page);
		return PTR_ERR(handle);
	}

	lock_page(page);
	if (page->mapping != mapping) {
		/* The page got truncated from under us */
		unlock_page(page);
		page_cache_release(page);
		ext4_journal_stop(handle);
		goto retry_grab;
	}
	/* In case writeback began while the page was unlocked */
	wait_for_stable_page(page);

#ifdef CONFIG_EXT4_FS_ENCRYPTION
	ret = ext4_block_write_begin(page, pos, len,
				     ext4_da_get_block_prep);
#else
	ret = __block_write_begin(page, pos, len, ext4_da_get_block_prep);
#endif
	if (ret < 0) {
		unlock_page(page);
		ext4_journal_stop(handle);
		/*
		 * block_write_begin may have instantiated a few blocks
		 * outside i_size.  Trim these off again. Don't need
		 * i_size_read because we hold i_mutex.
		 */
		if (pos + len > inode->i_size)
			ext4_truncate_failed_write(inode);

		if (ret == -ENOSPC &&
		    ext4_should_retry_alloc(inode->i_sb, &retries))
			goto retry_journal;

		page_cache_release(page);
		return ret;
	}

	*pagep = page;
	return ret;
}

/*
 * Check if we should update i_disksize
 * when write to the end of file but not require block allocation
 */
static int ext4_da_should_update_i_disksize(struct page *page,
					    unsigned long offset)
{
	struct buffer_head *bh;
	struct inode *inode = page->mapping->host;
	unsigned int idx;
	int i;

	bh = page_buffers(page);
	idx = offset >> inode->i_blkbits;

	for (i = 0; i < idx; i++)
		bh = bh->b_this_page;

	if (!buffer_mapped(bh) || (buffer_delay(bh)) || buffer_unwritten(bh))
		return 0;
	return 1;
}

static int ext4_da_write_end(struct file *file,
			     struct address_space *mapping,
			     loff_t pos, unsigned len, unsigned copied,
			     struct page *page, void *fsdata)
{
	struct inode *inode = mapping->host;
	int ret = 0, ret2;
	handle_t *handle = ext4_journal_current_handle();
	loff_t new_i_size;
	unsigned long start, end;
	int write_mode = (int)(unsigned long)fsdata;

	if (write_mode == FALL_BACK_TO_NONDELALLOC)
		return ext4_write_end(file, mapping, pos,
				      len, copied, page, fsdata);

	trace_ext4_da_write_end(inode, pos, len, copied);
	start = pos & (PAGE_CACHE_SIZE - 1);
	end = start + copied - 1;

	/*
	 * generic_write_end() will run mark_inode_dirty() if i_size
	 * changes.  So let's piggyback the i_disksize mark_inode_dirty
	 * into that.
	 */
	new_i_size = pos + copied;
	if (copied && new_i_size > EXT4_I(inode)->i_disksize) {
		if (ext4_has_inline_data(inode) ||
		    ext4_da_should_update_i_disksize(page, end)) {
			ext4_update_i_disksize(inode, new_i_size);
			/* We need to mark inode dirty even if
			 * new_i_size is less that inode->i_size
			 * bu greater than i_disksize.(hint delalloc)
			 */
			ext4_mark_inode_dirty(handle, inode);
		}
	}

	if (write_mode != CONVERT_INLINE_DATA &&
	    ext4_test_inode_state(inode, EXT4_STATE_MAY_INLINE_DATA) &&
	    ext4_has_inline_data(inode))
		ret2 = ext4_da_write_inline_data_end(inode, pos, len, copied,
						     page);
	else
		ret2 = generic_write_end(file, mapping, pos, len, copied,
							page, fsdata);

	copied = ret2;
	if (ret2 < 0)
		ret = ret2;
	ret2 = ext4_journal_stop(handle);
	if (!ret)
		ret = ret2;

	return ret ? ret : copied;
}


/////////////////////////////////////////////


ssize_t
generic_file_direct_write(struct kiocb *iocb, struct iov_iter *from, loff_t pos)
{
	struct file	*file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode	*inode = mapping->host;
	ssize_t		written;
	size_t		write_len;
	pgoff_t		end;
	struct iov_iter data;

	write_len = iov_iter_count(from);
	end = (pos + write_len - 1) >> PAGE_CACHE_SHIFT;

	written = filemap_write_and_wait_range(mapping, pos, pos + write_len - 1);
	if (written)
		goto out;

	/*
	 * After a write we want buffered reads to be sure to go to disk to get
	 * the new data.  We invalidate clean cached page from the region we're
	 * about to write.  We do this *before* the write so that we can return
	 * without clobbering -EIOCBQUEUED from ->direct_IO().
	 */
	if (mapping->nrpages) {
		written = invalidate_inode_pages2_range(mapping,
					pos >> PAGE_CACHE_SHIFT, end);
		/*
		 * If a page can not be invalidated, return 0 to fall back
		 * to buffered write.
		 */
		if (written) {
			if (written == -EBUSY)
				return 0;
			goto out;
		}
	}

	data = *from;
	written = mapping->a_ops->direct_IO(iocb, &data, pos);

	/*
	 * Finally, try again to invalidate clean pages which might have been
	 * cached by non-direct readahead, or faulted in by get_user_pages()
	 * if the source of the write was an mmap'ed region of the file
	 * we're writing.  Either one is a pretty crazy thing to do,
	 * so we don't support it 100%.  If this invalidation
	 * fails, tough, the write still worked...
	 */
	if (mapping->nrpages) {
		invalidate_inode_pages2_range(mapping,
					      pos >> PAGE_CACHE_SHIFT, end);
	}

	if (written > 0) {
		pos += written;
		iov_iter_advance(from, written);
		if (pos > i_size_read(inode) && !S_ISBLK(inode->i_mode)) {
			i_size_write(inode, pos);
			mark_inode_dirty(inode);
		}
		iocb->ki_pos = pos;
	}
out:
	return written;
}
EXPORT_SYMBOL(generic_file_direct_write);

/*
 * Find or create a page at the given pagecache position. Return the locked
 * page. This function is specifically for buffered writes.
 */
struct page *grab_cache_page_write_begin(struct address_space *mapping,
					pgoff_t index, unsigned flags)
{
	struct page *page;
	int fgp_flags = FGP_LOCK|FGP_ACCESSED|FGP_WRITE|FGP_CREAT;

	if (flags & AOP_FLAG_NOFS)
		fgp_flags |= FGP_NOFS;

	page = pagecache_get_page(mapping, index, fgp_flags,
			mapping_gfp_mask(mapping));
	if (page)
		wait_for_stable_page(page);

	return page;
}
EXPORT_SYMBOL(grab_cache_page_write_begin);

ssize_t generic_perform_write(struct file *file,
				struct iov_iter *i, loff_t pos)
{
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = 0;

	/*
	 * Copies from kernel address space cannot fail (NFSD is a big user).
	 */
	if (!iter_is_iovec(i))
		flags |= AOP_FLAG_UNINTERRUPTIBLE;

	do {
		struct page *page;
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */
		size_t copied;		/* Bytes copied from user */
		void *fsdata;

		offset = (pos & (PAGE_CACHE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_count(i));

again:
		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

		if (fatal_signal_pending(current)) {
			status = -EINTR;
			break;
		}

		//status = a_ops->write_begin(file, mapping, pos, bytes, flags,
		//				&page, &fsdata);
		status = cluster_ext4_da_write_begin(file, mapping, pos, bytes, flags,
															&page, &fsdata);


		if (unlikely(status < 0))
			break;

		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);
		flush_dcache_page(page);

		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
		if (unlikely(status < 0))
			break;
		copied = status;

		cond_resched();

		iov_iter_advance(i, copied);
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_single_seg_count(i));
			goto again;
		}
		pos += copied;
		written += copied;

		balance_dirty_pages_ratelimited(mapping);
	} while (iov_iter_count(i));

	return written ? written : status;
}
EXPORT_SYMBOL(generic_perform_write);

/**
 * __generic_file_write_iter - write data to a file
 * @iocb:	IO state structure (file, offset, etc.)
 * @from:	iov_iter with data to write
 *
 * This function does all the work needed for actually writing data to a
 * file. It does all basic checks, removes SUID from the file, updates
 * modification times and calls proper subroutines depending on whether we
 * do direct IO or a standard buffered write.
 *
 * It expects i_mutex to be grabbed unless we work on a block device or similar
 * object which does not need locking at all.
 *
 * This function does *not* take care of syncing data in case of O_SYNC write.
 * A caller has to handle it. This is mainly due to the fact that we want to
 * avoid syncing under i_mutex.
 */
ssize_t __generic_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct address_space * mapping = file->f_mapping;
	struct inode 	*inode = mapping->host;
	ssize_t		written = 0;
	ssize_t		err;
	ssize_t		status;

	/* We can write back this queue in page reclaim */
	current->backing_dev_info = inode_to_bdi(inode);
	err = file_remove_privs(file);
	if (err)
		goto out;

	err = file_update_time(file);
	if (err)
		goto out;

	if (iocb->ki_flags & IOCB_DIRECT) {
		loff_t pos, endbyte;

		written = generic_file_direct_write(iocb, from, iocb->ki_pos);
		/*
		 * If the write stopped short of completing, fall back to
		 * buffered writes.  Some filesystems do this for writes to
		 * holes, for example.  For DAX files, a buffered write will
		 * not succeed (even if it did, DAX does not handle dirty
		 * page-cache pages correctly).
		 */
		if (written < 0 || !iov_iter_count(from) || IS_DAX(inode))
			goto out;

		status = generic_perform_write(file, from, pos = iocb->ki_pos);
		/*
		 * If generic_perform_write() returned a synchronous error
		 * then we want to return the number of bytes which were
		 * direct-written, or the error code if that was zero.  Note
		 * that this differs from normal direct-io semantics, which
		 * will return -EFOO even if some bytes were written.
		 */
		if (unlikely(status < 0)) {
			err = status;
			goto out;
		}
		/*
		 * We need to ensure that the page cache pages are written to
		 * disk and invalidated to preserve the expected O_DIRECT
		 * semantics.
		 */
		endbyte = pos + status - 1;
		err = filemap_write_and_wait_range(mapping, pos, endbyte);
		if (err == 0) {
			iocb->ki_pos = endbyte + 1;
			written += status;
			invalidate_mapping_pages(mapping,
						 pos >> PAGE_CACHE_SHIFT,
						 endbyte >> PAGE_CACHE_SHIFT);
		} else {
			/*
			 * We don't know how much we wrote, so just return
			 * the number of bytes which were direct-written
			 */
		}
	} else {
		written = generic_perform_write(file, from, iocb->ki_pos);
		if (likely(written > 0))
			iocb->ki_pos += written;
	}
out:
	current->backing_dev_info = NULL;
	return written ? written : err;
}
EXPORT_SYMBOL(__generic_file_write_iter);

/**
 * generic_file_write_iter - write data to a file
 * @iocb:	IO state structure
 * @from:	iov_iter with data to write
 *
 * This is a wrapper around __generic_file_write_iter() to be used by most
 * filesystems. It takes care of syncing the file in case of O_SYNC file
 * and acquires i_mutex as needed.
 */
ssize_t generic_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	ssize_t ret;

	mutex_lock(&inode->i_mutex);
	ret = generic_write_checks(iocb, from);
	if (ret > 0)
		ret = __generic_file_write_iter(iocb, from);
	mutex_unlock(&inode->i_mutex);

	if (ret > 0) {
		ssize_t err;

		err = generic_write_sync(file, iocb->ki_pos - ret, ret);
		if (err < 0)
			ret = err;
	}
	return ret;
}
EXPORT_SYMBOL(generic_file_write_iter);



