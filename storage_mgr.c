/*
storage_mgr.c
Author: Pradyumna Deshpande
*/

// Imports
#include <stdio.h>
#include <stdlib.h>
#include "storage_mgr.h"
#include "dberror.h"

FILE *file;            //File pointer (will be reused throughout the code)

// Initializing the file pointer as NULL
void initStorageManager(void) {
    file = NULL;
    printf("Operation: Initialized Storage Manager.\n");
}

// Create a new file with 1 page containing 0 bytes of data
RC createPageFile(char *fileName) {
    file = fopen(fileName, "w+");
    SM_PageHandle filePageSize = (char *) calloc(PAGE_SIZE, sizeof(char));          // Memory allocated to page using calloc (as calloc uses initialization)
    fwrite(filePageSize, sizeof(char), PAGE_SIZE, file);                            // Create new empty page with allocated size

    free(filePageSize);                                                             // Free allocated memory

    // In case any error while creating the file
    if(file == NULL) {
        printf("Operation Create: Could not create file.\n");
        return RC_FILE_NOT_FOUND;
    }

    fclose(file);                                                                   // Close the file

    printf("Operation: File '%s' created successfully with 1 page.\n", fileName);
    return RC_OK;
}

// Open existing file and save details in File Handle
RC openPageFile(char *fileName, SM_FileHandle *fHandle) {
    file = fopen(fileName, "r+");

    // In case file doesn't exist
    if(file == NULL) {
        printf("Operation Open: File '%s' doesn't exist.\n", fileName);
        return RC_FILE_NOT_FOUND;
    }

    fHandle->fileName = fileName;
    fHandle->curPagePos = 0;
    fHandle->mgmtInfo = file;

    fseek(file, 0, SEEK_END);                                                       // Traverse to the end of file
    fHandle->totalNumPages = ftell(file) / PAGE_SIZE;

    printf("Operation: Opened file '%s' and header data saved successfully.\n", fileName);
    return RC_OK;
}

// Close the open file
RC closePageFile(SM_FileHandle *fHandle) {
    
    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    file = fHandle->mgmtInfo;

    // In case file is already closed or doesn't exist
    if(fclose(file) != 0) {
        printf("Operation Close: File is already closed or doesn't exist.\n");
        RC_message = "Unable to close file. The file may be already closed or doesn't exist.";
        return RC_FILE_NOT_FOUND;
    }

    printf("Operation: File '%s' closed successfully.\n", fHandle->fileName);
    return RC_OK;
}

// Delete file
RC destroyPageFile(char *fileName) {

    // In case file is open or already deleted
    if(remove(fileName) == -1) {
        printf("Operation: File not deleted.\n");
        RC_message = "Unable to delete file. The file may not be closed or already deleted.";
        return RC_FILE_NOT_FOUND;
    }

    printf("Operation: File '%s' deleted successfully.\n", fileName);
    return RC_OK;
}

// Read the given page
RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    // Invalid page number. Page doesn't exist
    if(pageNum >= fHandle->totalNumPages || pageNum < 0) {
        printf("Operation Read: Page %d in '%s' doesn't exist.\n", pageNum, fHandle->fileName);
        return RC_READ_NON_EXISTING_PAGE;
    }

    file = fHandle->mgmtInfo;
    fseek(file, pageNum*PAGE_SIZE, SEEK_SET);                                       // Seek to start of the file and traverse to the required page
    fread(memPage, sizeof(char), PAGE_SIZE, file);                                  // Read contents of the page
    fHandle->curPagePos = pageNum;

    printf("Operation: Read complete from page %d in '%s'.\n", pageNum, fHandle->fileName);
    return RC_OK;
}

// Get current page
int getBlockPos(SM_FileHandle *fHandle) {
    return fHandle->curPagePos;
}

// Read first page
RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {
    
    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return readBlock(0, fHandle, memPage);
}

// Read last page
RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return readBlock(fHandle->totalNumPages - 1, fHandle, memPage);
}

// Read previous page from current page position
RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return readBlock(getBlockPos(fHandle) - 1, fHandle, memPage);
}

// Read current page
RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return readBlock(getBlockPos(fHandle), fHandle, memPage);
}

// Read next page from current page position
RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return readBlock(getBlockPos(fHandle) + 1, fHandle, memPage);
}

// Write to the given page
RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    // Invalid page number. Page doesn't exist
    if(pageNum >= fHandle->totalNumPages || pageNum < 0) {
        printf("Operation Write: Page %d in '%s' doesn't exist.\n", pageNum, fHandle->fileName);
        return RC_WRITE_FAILED;
    }

    file = fHandle->mgmtInfo;
    fseek(file, pageNum*PAGE_SIZE, SEEK_SET);                                       // Seek to start of file and traverse to the required page
    fwrite(memPage, sizeof(char), PAGE_SIZE, file);                                 // Write to the page
    fHandle->curPagePos = pageNum;

    printf("Operation: Write complete to page %d in '%s'.\n", pageNum, fHandle->fileName);
    return RC_OK;
}

// Write to the current page
RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return writeBlock(getBlockPos(fHandle), fHandle, memPage);
}

// Add 1 new page to the file
RC appendEmptyBlock(SM_FileHandle *fHandle) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    file = fHandle->mgmtInfo;
    SM_PageHandle filePageSize = (char *) calloc(PAGE_SIZE, sizeof(char));          // Memory allocated to page using calloc (as calloc uses initialization)
    
    fseek(file, 0, SEEK_END);                                                       // Traverse to the end of file
    fwrite(filePageSize, sizeof(char), PAGE_SIZE, file);                            // Append new empty page with allocated size
    fHandle->totalNumPages++;
    fHandle->curPagePos = getBlockPos(fHandle) + 1;

    free(filePageSize);                                                             // Free allocated memory

    printf("Operation: New page appended. Total page count %d.\n", fHandle->totalNumPages);
    return RC_OK;
}

// Change page count by adding pages to the file if given_page_count > current_page_count
RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle) {

    // Check if given file handle is valid
    if(fHandle == NULL) {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    // Keep adding pages till current page count is less than given page count
    while(fHandle->totalNumPages < numberOfPages) {
        appendEmptyBlock(fHandle);
    }

    return RC_OK;
}

