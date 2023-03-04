Project Name - Buffer Manager
_________________________________________________________________________

Name: Pradyumna Deshpande
Email: deshpradyumna@gmail.com
__________________________________________________________________________

A) File Structure: 

	Makefile
	README.txt
	buffer_mgr.c
	buffer_mgr.h
	buffer_mgr_stat.c
	buffer_mgr_stat.h
	dberror.c
	dberror.h
	dt.h
	storage_mgr.c
	storage_mgr.h
	test_assign2_1.c
	test_helper.h
__________________________________________________________________________

B) Steps to run and test:

1) Open terminal and navigate to the project root directory "BufferManager".
2) To verify that we are in the right directory, use ls to list the files. This will also show you the file structure.
3) To remove outdated object files, type "make clean".
4) To build and compile all project files, type "make".
5) To run the test cases type "make run".
__________________________________________________________________________

C) Page Replacement Strategies:

1) firstInFirstOutRS() - Function for FIFO. 
- First, we determine the first frame in the queue with fix count = 0 and smallest queue     position. 
- If all the pages are in use by the clients, no page can be evicted and hence new page cannot be pinned. Otherwise, we replace the page with the frame using replacePage() and then update the queue position of the frame to the latest(last-in).

2) leastRecentlyUsedRS() - function for LRU. 
- First, we determine the first frame in the buffer with fix count = 0 and least recently used position(lowest). 
- If all the pages are in use by the clients, no page can be evicted and hence new page cannot be pinned. Otherwise, we replace the page with the frame using replacePage() and update the buffer position of the frame to most recently used(highest).

3) clockRS() - function for Clock.
- If all the pages are in use by the clients, no page can be evicted and hence new page cannot be pinned. 
- We then determine the first frame in buffer with fix count = 0 and no second chance starting from frame least checked by algorithm. Until such a frame is found, the loop will keep running and unset all second chance flags for frames for which it is set.
- We replace the page with the frame using replacePage() and set newly added page's second chance to true since its hit.

4) leastFrequentlyUsedRS() - Function for LFU.
- First, we determine the first frame in the queue with fix count = 0 and lowest hit count OR same lowest hit count and added first
- If all the pages are in use by the clients, no page can be evicted and hence new page cannot be pinned. Otherwise, we replace the page with the frame using replacePage() and update the buffer position of the frame to most recently added(highest) and set hit to 1.

5) kLeastRecentlyUsedRS() - Function for LRU-K(LRU-3)
- First, we determine the first frame in the queue with fix count = 0 and third most recently used position.
- If all the pages are in use by the clients, no page can be evicted and hence new page cannot be pinned. Otherwise, we replace the page with the frame using replacePage() and update the buffer position of the frame to first most recently used(highest).
_____________________________________________________________________

D) Extra Data Structures, variables and funtions, and functionality used:

1) Struct PageFrame
	pinCount      - fix count of the page pinned to the frame.
	isDirty       - tells if the page pinned to the frame is dirty.
	pageNo        - page number of the page in page file which is pinned to the frame.
	pageData      - page data of the page in page file which is pinned to the frame.
	lruPos        - keeps track of how recently the page was used.
	fifoPos       - keeps track of page position in the queue.
	lfuHit        - keeps track of how many times the page was hit.
	lfuPos        - keeps track of how recently the page was added.
	clockChance   - tells if the page has second chance or not.

2) Variables
	readCnt       - count of how many times page read has been done from disk.
	writeCnt      - count of how many times page write has been done in disk.
	lruCounter    - holds the position of the to be next most recently used frame.
	fifoCounter   - keeps track of the last-in positon of the frame.
	lfuCounter    - holds the position of the latest added frame.
	clockCounter  - holds the position of the frame least checked by the algorithm.
	fHandle       - represents an open page file.

3) replacePage():
- It is a function that replaces a page in case of a page replacement strategy or in case of a non-full buffer pins page to the empty frame.
- If page to replace is dirty, write it back to disk first. We then allocate a block of memory to read a new page into the buffer. We then read the page and pin it to the frame.
- Note: This function contains same logic as page pinning in a non-full buffer, just an extra dirty bit check condition for page replacement case. Dirty bit will be false for the frame non-full buffer.

4) Making the Buffer Thread-safe: 
- In order to make the buffer thread safe, we are making it such that whenever client2 wants to edit a page (make it dirty); in such a case if a page has been already edited by client1 (the page is already dirty), then client 2 will get the latest updated page.
- This is done as follows: If page is already dirty, write it to disk and read the updated page to make the buffer thread-safe.
- If the page is not already dirty, then the initial content of the page is the latest and there is no need to update the content of the page as no modifications have been made (the dirty bit is false).
__________________________________________________________________________

E) Buffer Pool Related Functions:

1) initBufferPool():
- We first chek if the file exists. If it does not, return an error.
- We then allocate memory, initialize the page frames, the buffer pool and the buffer statistic variables.

2) shutdownBufferPool():
- We check if there are any pinned pages in the buffer pool. If there are pinned pages still in the buffer pool, the it cannot shut down. 
- We then write all the dirty pages to the disk by calling the method forceFlushPool, free all allocated memory, and shut down the buffer pool by setting all of its fields to NULL.

3) forceFlushPool():
- We check if a page is pinned and is dirty. If it is, we write the contents of the page to the disk by calling the function forcePage.
___________________________________________________________________________

F) Page Management Functions:

1) pinPage():
- Pins the page with page number pageNum. It is done either directly, or through a page replacement strategy.
- If a page is already pinned, we just increase the pinCount by 1.
- If the page is not already pinned and the buffer is not full, we find the first unpinned frame and pin the page using the function replacePage().
- If the buffer is full, we apply the provided page replacement strategy replace the page using the function replacePage().

2) unpinPage():
- We use pageNum to figure out which page to be unpinned and we do so by reducing the fix count by 1 in every iteration.
- Returns error if page to be unpinned does not exist in the buffer.

3) markDirty():
- First, we find a frame which contains the page to mark as dirty. 
- If the page is already dirty, we write it to the disk using forcePage and read the updated page to make the buffer thread-safe.
- If the page does not exist in the buffer, we throw an error.

4) forcePage():
- We iterate through all the fames in the buffer pool and find the frame which contains the page to write to the disk.
- We then check if the page is dirty. If it is, we write it to the disk.
- We return error if page does not exist.
______________________________________________________________________________

G) Statistics Functions:

1) getFrameContents():
- It returns an array of PageNumbers (of size numPages) where the ith element is the number of the page stored in the ith page frame.
- An empty frame is represented using the constant NO_PAGE.

2) getDirtyFlags():
- It returns an array of bools (of size numPages) where the ith element is TRUE if the page stored in the ith page frame is dirty or else it is FALSE.
- Empty page frames are considered as clean. Dirty bit will be FALSE.

3) getFixCounts():
- It returns an array of ints (of size numPages) where the ith element is the fix count of the page stored in the ith page frame.
- 0 is stored in array for empty page frames.

4) getNumReadIO():
- It returns the number of pages that have been read from page file since a buffer pool has been initialized.

5) getNumWriteIO():
- It returns the number of pages written to the page file since the buffer pool has been initialized.
__________________________________________________________________________________________