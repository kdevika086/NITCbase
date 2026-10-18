#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// constructor 1
BlockBuffer::BlockBuffer(char blockType)
{
  if (blockType == 'R')
  {
    this->blockNum = getFreeBlock(REC);
  }
  else if (blockType == 'I')
  {
    this->blockNum = getFreeBlock(IND_INTERNAL);
  }
  else if (blockType == 'L')
  {
    this->blockNum = getFreeBlock(IND_LEAF);
  }
}



// constructor 2
BlockBuffer::BlockBuffer(int blockNum) 
{
  this->blockNum= blockNum;
}



// constructor 1
RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer('R') {}



// constructor 2
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}



// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) 
{

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) 
  {
    return ret;   // return any errors that might have occured in the process
  }

  // populate the numEntries, numAttrs and numSlots fields in *head
  memcpy(&head->numSlots, bufferPtr + 24, 4);
  memcpy(&head->numAttrs, bufferPtr + 20, 4);
  memcpy(&head->numEntries, bufferPtr + 16, 4);
  memcpy(&head->rblock, bufferPtr + 12, 4);
  memcpy(&head->lblock, bufferPtr + 8, 4);
  memcpy(&head->pblock, bufferPtr + 4, 4);
  memcpy(&head->blockType, bufferPtr, 4);

  return SUCCESS;
}



// load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum) 
{
  struct HeadInfo head;

  // get the header using this.getHeader() function
  this->getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) 
  {
    return ret;
  }

  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr+ HEADER_SIZE+ slotCount + (recordSize*slotNum);

  // load the record into the rec data structure
  memcpy(rec, slotPointer, recordSize);

  return SUCCESS;
}



int RecBuffer::setRecord(union Attribute *rec, int slotNum)
{
  unsigned char *bufferPtr;
  // get the buffer containing the block
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }
  // get the header of the block
  struct HeadInfo head;
  ret = this->getHeader(&head);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;
  // check if slotNum is valid
  if (slotNum < 0 || slotNum >= slotCount)
  {
    return E_OUTOFBOUND;
  }

  // calculate the size of one record
  int recordSize = attrCount * ATTR_SIZE;
  // point to the required record in the buffer
  unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

  // copy the input record into the buffer
  memcpy(slotPointer, rec, recordSize);

  // mark the buffer as dirty
  ret = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret != SUCCESS)
  {
    return ret;
  }

  return SUCCESS;
}



int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) 
{
  // check whether the block is already present in the buffer
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  // blockNum is outside the valid range
  if (bufferNum == E_OUTOFBOUND)
  {
    return E_OUTOFBOUND;
  }

  // block is already present in the buffer
  if (bufferNum != E_BLOCKNOTINBUFFER)
  {
    // increment timestamp of all other occupied buffers
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
      if (bufferIndex != bufferNum && StaticBuffer::metainfo[bufferIndex].free == false)
      {
        StaticBuffer::metainfo[bufferIndex].timeStamp++;
      }
    }
    // this buffer was just accessed, so reset its timestamp
    StaticBuffer::metainfo[bufferNum].timeStamp = 0;
  }
  else
  {
    // block is not in buffer, so allocate a buffer
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
    if (bufferNum == E_OUTOFBOUND)
    {
      return E_OUTOFBOUND;
    }
    // load the block from disk into the allocated buffer
    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }
  // store pointer to the buffer containing the block
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}




int RecBuffer::getSlotMap(unsigned char *slotMap) 
{
  unsigned char *bufferPtr;
  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) 
  {
    return ret;
  }
  // get the header of the block using getHeader() function
  struct HeadInfo head;
  this->getHeader(&head);
  int slotCount =head.numSlots;
  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;
  // copy the values from `slotMapInBuffer` to `slotMap` (size is `slotCount`)
  memcpy(slotMap, slotMapInBuffer, slotCount);
  return SUCCESS;
}



int compareAttrs(Attribute attr1, Attribute attr2, int attrType)
{ 
  double diff;
  if(attrType == STRING)
  {
    diff=strcmp(attr1.sVal, attr2.sVal);
  }
  else
  {
    diff=attr1.nVal- attr2.nVal;
  }
  if(diff>0)
  {
    return 1;
  }
  if(diff<0)
  {
    return -1;
  }
  return 0;
}



int BlockBuffer::setHeader(struct HeadInfo *head)
{
  unsigned char *bufferPtr;
  // get the starting address of the buffer containing the block using
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    // return the value returned by the call.
  if (ret != SUCCESS)
  {
    return ret;
  }
  // cast bufferPtr to type HeadInfo*
  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  // copy the fields of the HeadInfo pointed to by head (except reserved) to
  // the header of the block (pointed to by bufferHeader)
  bufferHeader->blockType = head->blockType;
  bufferHeader->pblock = head->pblock;
  bufferHeader->lblock = head->lblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->numAttrs = head->numAttrs;
  bufferHeader->numSlots = head->numSlots;


  // update dirty bit by calling StaticBuffer::setDirtyBit()
  ret = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret != SUCCESS)
  {
    return ret;
  }
  return SUCCESS;
}



int BlockBuffer::setBlockType(int blockType)
{
  unsigned char *bufferPtr;
  /* get the starting address of the buffer containing the block*/
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if(ret!=SUCCESS)
  {
    return ret;
  }

  // store the input block type in the first 4 bytes of the buffer.
  *((int32_t *)bufferPtr) = blockType;

  // update the StaticBuffer::blockAllocMap entry 
  StaticBuffer::blockAllocMap[this->blockNum] = blockType;

  // update dirty bit by calling StaticBuffer::setDirtyBit()
  ret = StaticBuffer::setDirtyBit(this->blockNum);
  if(ret!=SUCCESS)
  {
    return ret;
  }
  return SUCCESS;
}



int BlockBuffer::getFreeBlock(int blockType)
{
  int freeBlock=-1;
  // find a free buffer using StaticBuffer::getFreeBuffer() .
  for(int blockNum = 0; blockNum < DISK_BLOCKS; blockNum++)
  {
    if(StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
    {
      freeBlock = blockNum;
      break;
    }
  }
  if(freeBlock == -1)
  {
    return E_DISKFULL;
  }
  // set the object's blockNum to the block number of the free block.
  this->blockNum = freeBlock;
  //allocate buffer for this block
  int bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
  if(bufferNum == E_OUTOFBOUND)
  {
    return E_OUTOFBOUND;
  }
  // initialize the header of the block passing a struct HeadInfo with values
  // pblock: -1, lblock: -1, rblock: -1, numEntries: 0, numAttrs: 0, numSlots: 0
  // to the setHeader() function.
  struct HeadInfo head;

  head.pblock = -1;
  head.lblock = -1;
  head.rblock = -1;
  head.numEntries = 0;
  head.numAttrs = 0;
  head.numSlots = 0;
  head.blockType = blockType;

  int ret = setHeader(&head);
  if(ret != SUCCESS)
  {
    return ret;
  }

  // update the block type of the block to the input block type using setBlockType().
  ret = setBlockType(blockType);
  if(ret != SUCCESS)
  {
    return ret;
  }
  // return block number of the free block.
  return this->blockNum;
}



int RecBuffer::setSlotMap(unsigned char *slotMap) 
{
  unsigned char *bufferPtr;
  //get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr(&bufferPtr).
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if(ret!=SUCCESS)
  {
    return ret;
  }

  // get the header of the block using the getHeader() function
  struct HeadInfo head;
  ret = getHeader(&head);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int numSlots = head.numSlots;

  // the slotmap starts at bufferPtr + HEADER_SIZE. Copy the contents of the
  // argument `slotMap` to the buffer replacing the existing slotmap.
  // Note that size of slotmap is `numSlots`
  memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

  // update dirty bit using StaticBuffer::setDirtyBit
  ret = StaticBuffer::setDirtyBit(this->blockNum);
  if (ret != SUCCESS)
  {
    return ret;
  }
  return SUCCESS;
}



int BlockBuffer::getBlockNum()
{
  return this->blockNum;
}



void BlockBuffer::releaseBlock()
{

  // if blockNum is INVALID_BLOCKNUM (-1), or it is invalidated already, do nothing
  if (this->blockNum == INVALID_BLOCKNUM)
  {
    return;
  }

  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  // if the block is present in the buffer, free the buffer
  if (bufferNum != E_BLOCKNOTINBUFFER)
  {
    StaticBuffer::metainfo[bufferNum].free = true;
  }

  // Mark the block as unused in the block allocation map
  StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;

  // set the object's blockNum to INVALID_BLOCK (-1)
  this->blockNum = INVALID_BLOCKNUM;
}



IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType)
{
}



IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum)
{
}



IndInternal::IndInternal() : IndBuffer('I')
{
}



IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum)
{
}



IndLeaf::IndLeaf() : IndBuffer('L')
{
}




IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum)
{
}



int IndInternal::getEntry(void *ptr, int indexNum) 
{
  if(indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL)
  {
    return E_OUTOFBOUND;
  }

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if(ret != SUCCESS)
  {
    return ret;
  }
  struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

  memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
  memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
  memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

  return SUCCESS;
}



int IndLeaf::getEntry(void *ptr, int indexNum) 
{
  if(indexNum < 0 || indexNum >= MAX_KEYS_LEAF)
  {
    return E_OUTOFBOUND;
  }

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if(ret != SUCCESS)
  {
    return ret;
  }
  unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
  memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

  return SUCCESS;
}


//empty definition
int IndInternal::setEntry(void *ptr, int indexNum) 
{
  return 0;
}


//empty definition
int IndLeaf::setEntry(void *ptr, int indexNum) 
{
  return 0;
}



