/*
buffer_mgr.c
Author: Pradyumna Deshpande
*/

#include<stdio.h>
#include<stdlib.h>

#include "buffer_mgr.h"
#include "storage_mgr.h"

// Need to initialize buffer stat variables globally as well or else will be initialized with garbage values
int readCnt = 0;
int writeCnt = 0;
int lruCounter = 0;
int fifoCounter = 0;
int lfuCounter = 0;
int clockCounter = 0;
SM_FileHandle *fHandle = NULL;

// Struct for storing page frames. This struct will be be stored in mgmtData of given buffer pool object
typedef struct Frame {
    int pinCnt;
    bool isDirty;
    PageNumber pageNo;
    SM_PageHandle pageData;
    int lruPos;
    int fifoPos;
    int lfuPos;
    int lfuHit;
    bool clockChance;
} Frame;

// Function to replace page in case of a replacement strategy or in case of pinning to empty frame
RC replacePage(char *stratName, int frameToEvict, Frame *fr, BM_PageHandle *const page, BM_BufferPool *const bm, const PageNumber pageNum) {
    // If page to replace is dirty, write it back to disk first
    if(fr[frameToEvict].isDirty) {
        page->pageNum = fr[frameToEvict].pageNo;
        page->data = fr[frameToEvict].pageData;

        int success = forcePage(bm, page);
        if(success != RC_OK) {
            printf("%s: Could not write dirty page %d to disk.\n", stratName, page->pageNum);
            return RC_WRITE_FAILED;
        }
    }

    // Block to read new page into the buffer
    fHandle = (SM_FileHandle*)malloc(sizeof(SM_FileHandle));

    fr[frameToEvict].pinCnt++;
    fr[frameToEvict].pageNo = pageNum;

    fr[frameToEvict].pageData = (char*)malloc(PAGE_SIZE);
    int success = openPageFile(bm->pageFile, &fHandle);
                
    if(success != RC_OK) {
        printf("%s: Could not open file. File doesn't exist.\n", stratName);
        return RC_FILE_NOT_FOUND;
    }

    ensureCapacity(pageNum+1, &fHandle);
    success = readBlock(pageNum, &fHandle, fr[frameToEvict].pageData);

    if(success != RC_OK) {
        printf("%s: Could not read page. Page doesn't exist.\n", stratName);
        closePageFile(&fHandle);
        return RC_READ_NON_EXISTING_PAGE;
    }

    readCnt++;

    page->data = fr[frameToEvict].pageData;
    page->pageNum = pageNum;

    return RC_OK;
}

// Page Replacement Strategy Functions

// Function for FIFO
RC firstInFirstOutRS(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum, Frame *fr) {
    int min = INT_MAX;
    int frameToEvict = -1;
    
    // Determine the first frame in queue with fix count = 0 and smallest queue position
    for(int frame = 0; frame < bm->numPages; frame++) {
        if(fr[frame].pinCnt == 0 && fr[frame].fifoPos < min) {
            min = fr[frame].fifoPos;
            frameToEvict = frame;
        }
    }

    // If all pages are in use by clients, no page can be evicted and hence new page cannot be pinned.
    if(frameToEvict == -1) {
        printf("FIFO: Could not pin page. All Pages are in use.\n");
        return RC_WRITE_FAILED;
    }

    int success = replacePage("FIFO", frameToEvict, fr, page, bm, pageNum);
    if(success != RC_OK) {
        return success;
    }

    // Update queue position of frame to latest (highest)
    fr[frameToEvict].fifoPos = fifoCounter;
    fifoCounter++;

    closePageFile(&fHandle);

    printf("FIFO: Page pinned to frame %d.\n", frameToEvict);
    return RC_OK;
}

// Function for LRU
RC leastRecentlyUsedRS(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum, Frame *fr) {
    int min = INT_MAX;
    int frameToEvict = -1;
    
    // Determine the frame in buffer with fix count = 0 and least recently used
    for(int frame = 0; frame < bm->numPages; frame++) {
        if(fr[frame].pinCnt == 0 && fr[frame].lruPos < min) {
            min = fr[frame].lruPos;
            frameToEvict = frame;
        }
    }

    // If all pages are in use by clients, no page can be evicted and hence new page cannot be pinned.
    if(frameToEvict == -1) {
        printf("LRU: Could not pin page. All Pages are in use.\n");
        return RC_WRITE_FAILED;
    }

    int success = replacePage("LRU", frameToEvict, fr, page, bm, pageNum);
    if(success != RC_OK) {
        return success;
    }
    
    // Update lruPos to most recently used (highest)
    fr[frameToEvict].lruPos = lruCounter;
    lruCounter++;

    closePageFile(&fHandle);

    printf("LRU: Page pinned to frame %d.\n", frameToEvict);
    return RC_OK;
}

// Function for LRU-k (LRU-3)
RC kLeastRecentlyUsedRS(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum, Frame *fr) {
    int min = INT_MAX;
    int frameToEvict = -1;
    
    // Determine the frame in buffer with fix count = 0 and 3rd most recently used
    for(int frame = 0; frame < bm->numPages; frame++) {
        if(fr[frame].pinCnt == 0 && fr[frame].lruPos < min) {
            min = fr[frame].lruPos;
            frameToEvict = frame;
        }
    }

    // If all pages are in use by clients, no page can be evicted and hence new page cannot be pinned.
    if(frameToEvict == -1) {
        printf("LRU-K (LRU-3): Could not pin page. All Pages are in use.\n");
        return RC_WRITE_FAILED;
    }

    int success = replacePage("LRU-K (LRU-3)", frameToEvict, fr, page, bm, pageNum);
    if(success != RC_OK) {
        return success;
    }
    
    // Update lruPos to most recently used (highest)
    fr[frameToEvict].lruPos = lruCounter;
    lruCounter++;

    closePageFile(&fHandle);

    printf("LRU-K (LRU-3): Page pinned to frame %d.\n", frameToEvict);
    return RC_OK;
}

// Function for Clock
RC clockRS(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum, Frame *fr) {
    int frameToEvict = -1;
    int frame = 0; 
    
    while(frame < bm->numPages) {
        if(fr[frame].pinCnt == 0) {
            break;
        }

        frame++;
    }

    // If all pages are in use by clients, no page can be evicted and hence new page cannot be pinned.
    if(frame == bm->numPages) {
        printf("CLOCK: Could not pin page. All Pages are in use.\n");
        return RC_WRITE_FAILED;
    }

    // Determine the first frame in buffer with fix count = 0 and no second chance starting from frame least checked by algorithm
    while(clockCounter < bm->numPages) {
        if(fr[clockCounter].pinCnt == 0 && !fr[frame].clockChance) {
            frameToEvict = clockCounter;
            break;
        }
        
        if(fr[clockCounter].clockChance) {
            fr[clockCounter].clockChance = false;
        }

        clockCounter = (clockCounter + 1) % bm->numPages;
    }

    int success = replacePage("CLOCK", frameToEvict, fr, page, bm, pageNum);
    if(success != RC_OK) {
        return success;
    }

    // Set newly added page's second chance to true since its hit
    fr[frameToEvict].clockChance = true;

    closePageFile(&fHandle);

    printf("CLOCK: Page pinned to frame %d.\n", frameToEvict);
    return RC_OK;
}

// Function for LFU
RC leastFrequentlyUsedRS(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum, Frame *fr) {
    int earliestAdded = INT_MAX;
    int lowestHitCnt = INT_MAX;
    int frameToEvict = -1;
    
    // Determine the frame in buffer with fix count = 0 and lowest hit count OR same lowest hit count and added first
    for(int frame = 0; frame < bm->numPages; frame++) {
        if((fr[frame].pinCnt == 0) && (fr[frame].lfuHit < lowestHitCnt || (fr[frame].lfuHit == lowestHitCnt && fr[frame].lfuPos < earliestAdded))) {
            earliestAdded = fr[frame].lfuPos;
            lowestHitCnt = fr[frame].lfuHit;
            frameToEvict = frame;
        }
    }

    // If all pages are in use by clients, no page can be evicted and hence new page cannot be pinned.
    if(frameToEvict == -1) {
        printf("LFU: Could not pin page. All Pages are in use.\n");
        return RC_WRITE_FAILED;
    }

    int success = replacePage("LFU", frameToEvict, fr, page, bm, pageNum);
    if(success != RC_OK) {
        return success;
    }
    
    // Update lfuPos to most recently added (highest) and set page hit to 1
    fr[frameToEvict].lfuPos = lfuCounter;
    lfuCounter++;

    fr[frameToEvict].lfuHit = 1;

    closePageFile(&fHandle);

    printf("LFU: Page pinned to frame %d.\n", frameToEvict);
    return RC_OK;
}

// Buffer Manager Interface Pool Handling

// Function to Initialize buffer pool with default values and allocate memory
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData) {
    fHandle = (SM_FileHandle*)malloc(sizeof(SM_FileHandle));
    
    int success = openPageFile((char*) pageFileName, &fHandle);

    // If file doesn't exist
    if(success != RC_OK) {
        printf("Could not open file. File doesn't exist.\n");
        return RC_FILE_NOT_FOUND;
    }

    closePageFile(&fHandle);

    Frame *fr = (Frame*)malloc(sizeof(Frame) * numPages);

    // Initialize Page Frames
    for(int frame = 0; frame < numPages; frame++) {
        fr[frame].pinCnt = 0;
        fr[frame].isDirty = false;
        fr[frame].pageNo = NO_PAGE;
        fr[frame].pageData = (char*)malloc(PAGE_SIZE);
        fr[frame].lruPos = INT_MAX;
        fr[frame].fifoPos = INT_MAX;
        fr[frame].lfuPos = INT_MAX;
        fr[frame].lfuHit = 0;
        fr[frame].clockChance = false;
    }

    // Initialize Buffer
    bm->pageFile = (char*) pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = fr;

    // Initialize buffer stat variables
    readCnt = 0;
    writeCnt = 0;
    fifoCounter = 0;
    lruCounter = 0;
    lfuCounter = 0;
    clockCounter = 0;

    printf("Buffer Pool Initialized.\n");
    return RC_OK;
}

// Function to De-allocate memory and shut down the buffer pool
RC shutdownBufferPool(BM_BufferPool *const bm) {
    Frame *fr = (Frame*) bm->mgmtData;

    // Check if all pages have fix count = 0
    for(int frame = 0; frame < bm->numPages; frame++) {
        if(fr[frame].pinCnt > 0) {
            printf("Operation Shut Down: Cannot shut down buffer pool. There are page(s) still in use.\n");
            return RC_IM_KEY_ALREADY_EXISTS;
        }
    }

    // Write all dirty pages to disk
    int success = forceFlushPool(bm);
    if(success != RC_OK) {
        printf("Operation Shut Down: Could not write all dirty pages to disk.\n");
        return RC_WRITE_FAILED;
    }

    // Free memory allocated to store page data in each frame
    for(int frame = 0; frame < bm->numPages; frame++) {
        free(fr[frame].pageData);
    }

    // Free memory allocated to frames
    free(fr);

    // Set buffer pool fields to NULL and 0
    bm->pageFile = NULL;
    bm->numPages = 0;
    bm->mgmtData = NULL;

    printf("Operation Shut Down: Successfully shut down buffer pool.\n");
    return RC_OK;
}

// Function to Write all dirty pages to disk
RC forceFlushPool(BM_BufferPool *const bm) {
    Frame *fr = (Frame*) bm->mgmtData;
    

    for(int frame = 0; frame < bm->numPages; frame++) {
        // If page fix count = 0 and page is dirty, call force page to write page to disk
        if(fr[frame].pinCnt == 0 && fr[frame].isDirty) {
            BM_PageHandle *const page = (BM_PageHandle*)malloc(sizeof(BM_PageHandle));

            page->data = fr[frame].pageData;
            page->pageNum = fr[frame].pageNo;

            int success = forcePage(bm, page);
            if(success != RC_OK) {
                printf("Operation Flush Pool: Could not write dirty page %d to disk.\n", page->pageNum);
                return RC_WRITE_FAILED;
            }
        }
    }

    printf("Operation Flush Pool: All dirty pages written to disk.\n");
    return RC_OK;
}

// Buffer Manager Interface Access Pages

// Function to mark all pages dirty
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page) {
    Frame *fr = (Frame*) bm->mgmtData;

    for(int frame = 0; frame < bm->numPages; frame++) {
        // Find frame which contains page to mark as dirty
        if(fr[frame].pageNo == page->pageNum) {
            // If page is already dirty, write it to disk and read the updated page to make the buffer thread-safe
            if(fr[frame].isDirty) {
                int success = forcePage(bm, page);
                if(success != RC_OK) {
                    printf("Operation Dirty: Could not write dirty page %d to disk.\n", page->pageNum);
                    return RC_WRITE_FAILED;
                }

                fHandle = (SM_FileHandle*)malloc(sizeof(SM_FileHandle));

                success = openPageFile(bm->pageFile, &fHandle);
                if(success != RC_OK) {
                    printf("Operation Dirty: Could not open file. File doesn't exist.\n");
                    return RC_FILE_NOT_FOUND;
                }

                ensureCapacity(page->pageNum+1, &fHandle);
                success = readBlock(page->pageNum, &fHandle, fr[frame].pageData);

                if(success != RC_OK) {
                    printf("Operation Dirty: Could not read page. Page doesn't exist.\n");
                    closePageFile(&fHandle);
                    return RC_READ_NON_EXISTING_PAGE;
                }

                closePageFile(&fHandle);

                readCnt++;
            }

            fr[frame].isDirty = true;

            if(bm->strategy == RS_LRU) {
                // Update lruPos to most recently used (highest)

                fr[frame].lruPos = lruCounter;
                lruCounter++;
            }
            
            printf("Operation Dirty: Page %d at frame %d marked as dirty.\n", page->pageNum, frame);
            return RC_OK;
        }
    }

    printf("Operation Dirty: Page %d does not exist.\n", page->pageNum);
    return RC_READ_NON_EXISTING_PAGE;
}

// Function to unpin page from frame
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page) {
    Frame *fr = (Frame*) bm->mgmtData;

    for(int frame = 0; frame < bm->numPages; frame++) {
        // Decrease fix count by 1
        if(fr[frame].pageNo == page->pageNum) {
            fr[frame].pinCnt--;

            if(bm->strategy == RS_LRU) {
                // Update lruPos to most recently used (highest)

                fr[frame].lruPos = lruCounter;
                lruCounter++;
            }

            printf("Operation Unpin: Page %d unpinned from frame %d.\n", page->pageNum, frame);
            return RC_OK;
        }
    }

    printf("Operation Unpin: Page %d does not exist in the buffer.\n", page->pageNum);
    return RC_READ_NON_EXISTING_PAGE;
}

// Function to write a specific dirty page to disk
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page) {
    Frame *fr = (Frame*) bm->mgmtData;

    for(int frame = 0; frame < bm->numPages; frame++) {
        // Find frame which contains page to write to disk and if it is dirty
        if(fr[frame].pageNo == page->pageNum && fr[frame].isDirty) {
            fHandle = (SM_FileHandle*)malloc(sizeof(SM_FileHandle));

            int success = openPageFile(bm->pageFile, &fHandle);
            if(success != RC_OK) {
                printf("Operation Force Page: Could not open file. File doesn't exist.\n");
                return RC_FILE_NOT_FOUND;
            }

            ensureCapacity(page->pageNum+1, &fHandle);
            success = writeBlock(fr[frame].pageNo, &fHandle, fr[frame].pageData);

            if(success != RC_OK) {
                printf("Operation Force Page: Could not write page. Page doesn't exist.\n");
                closePageFile(&fHandle);
                return RC_WRITE_FAILED;
            }

            closePageFile(&fHandle);

            writeCnt++;

            fr[frame].isDirty = false;

            if(bm->strategy == RS_LRU) {
                // Update lruPos to most recently used (highest)

                fr[frame].lruPos = lruCounter;
                lruCounter++;
            }
            
            printf("Operation Force Page: Page %s at frame %d written to disk.\n", page->data, frame);
            return RC_OK;
        }
    }

    printf("Operation Force Page: Page %d does not exist or is not dirty.\n", page->pageNum);
    return RC_WRITE_FAILED;
}

// Function to pin page to frame. Done either directly or through a page replacement strategy
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum) {
    Frame *fr = (Frame*) bm->mgmtData;

    // Check if page is already pinned to a frame
    for(int frame = 0; frame < bm->numPages; frame++) {
        // If it is, inecrease fix count by 1
        if(fr[frame].pageNo == pageNum) {
            fr[frame].pinCnt++;

            page->data = fr->pageData;
            page->pageNum = pageNum;

            printf("LRU: %d, FIFO: %d", lruCounter, fifoCounter);
            if(bm->strategy == RS_LRU) {
                // Update lruPos to most recently used (highest)
                fr[frame].lruPos = lruCounter;
                lruCounter++;
            } else if(bm->strategy == RS_LFU) {
                // Increment lfuHit
                fr[frame].lfuHit++;
            } else if(bm->strategy == RS_CLOCK) {
                // Page is hit, so set second chance to true
                fr[frame].clockChance = true;
            }
            
            bm->mgmtData = fr;

            printf("Page is already pinned to frame %d. Increased pin count to %d.\n", frame, fr[frame].pinCnt);
            return RC_OK;
        }
    }

    // If page is not pinned to any frame
    for(int frame = 0; frame < bm->numPages; frame++) {
        // If there is an empty frame (Meaning buffer is not full). Pin page to 1st unpinned frame
        if(fr[frame].pageNo == NO_PAGE) {
            // This function contains same logic as page pinning just an extra dirty check condition which will be false for empty frame
            // So used this function to reduce Lines Of Code
            replacePage("Operation Pin", frame, fr, page, bm, pageNum);

            if(bm->strategy == RS_LRU) {
                // Update lruPos to most recently used (highest)
                fr[frame].lruPos = lruCounter;
                lruCounter++;
            } else if(bm->strategy == RS_FIFO) {
                // Update queue position of frame to latest (highest)
                fr[frame].fifoPos = fifoCounter;
                fifoCounter++;
            } else if(bm->strategy == RS_LFU) {
                // Update lfuPos to most recently added (highest) and increment lfuHit
                fr[frame].lfuPos = lfuCounter;
                lfuCounter++;

                fr[frame].lfuHit++;
            }  else if(bm->strategy == RS_CLOCK) {
                // Page is hit, so set second chance to true
                fr[frame].clockChance = true;
            }

            bm->mgmtData = fr;

            closePageFile(&fHandle);

            printf("Operation Pin: Buffer is not full. Page %d pinned to frame %d.\n", pageNum, frame);
            
            return RC_OK;
        }
    }

    // Buffer is full. Implement page replacement strategy
    switch(bm->strategy) {
        case RS_FIFO:
            return firstInFirstOutRS(bm, page, pageNum, fr);
	
        case RS_LRU:
            return leastRecentlyUsedRS(bm, page, pageNum, fr);

	    case RS_CLOCK:
            return clockRS(bm, page, pageNum, fr);

	    case RS_LFU:
            return leastFrequentlyUsedRS(bm, page, pageNum, fr);

	    case RS_LRU_K:
            return kLeastRecentlyUsedRS(bm, page, pageNum, fr);

        default:
            printf("Page Replacement Strategy doesn't exist.\n");
            return RC_IM_KEY_NOT_FOUND;
    }
}

// Statistics Interface

// Funtion to Get page numbers pinned to each frame
PageNumber *getFrameContents (BM_BufferPool *const bm) {
    PageNumber *pageNos = (PageNumber*)malloc(sizeof(PageNumber)*bm->numPages);
    Frame *fr = (Frame*) bm->mgmtData;

    for(int frame = 0; frame < bm->numPages; frame++) {
        pageNos[frame] = fr[frame].pageNo;
    }

    return pageNos;
}

// Funtion to Get dirty flags of pages pinned to each frame
bool *getDirtyFlags (BM_BufferPool *const bm) {
    bool *dirtyFlags = (bool*)malloc(sizeof(bool)*bm->numPages);
    Frame *fr = (Frame*) bm->mgmtData;

    for(int frame = 0; frame < bm->numPages; frame++) {
        dirtyFlags[frame] = fr[frame].isDirty;
    }

    return dirtyFlags;
}

// Funtion to Get fix counts of pages pinned to each frame
int *getFixCounts (BM_BufferPool *const bm) {
    int *pinCnts = (PageNumber*)malloc(sizeof(int)*bm->numPages);
    Frame *fr = (Frame*) bm->mgmtData;

    for(int frame = 0; frame < bm->numPages; frame++) {
        pinCnts[frame] = fr[frame].pinCnt;
    }

    return pinCnts;
}

// Funtion to Get count of how many times page read has been done from disk
int getNumReadIO (BM_BufferPool *const bm) {
    return readCnt;
}

// Funtion to Get count of how many times page write has been done in disk
int getNumWriteIO (BM_BufferPool *const bm) {
    return writeCnt;
}
