#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

// declare the blockAllocMap array
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer() 
{
  // copy blockAllocMap blocks from disk to buffer (using readblock() of disk)
  // blocks 0 to 3
  for (int i = 0; i < 4; i++) 
  {
    Disk::readBlock(blockAllocMap + i * BLOCK_SIZE, i);
  }

  /* initialise metainfo of all the buffer blocks with
     dirty:false, free:true, timestamp:-1 and blockNum:-1
  */
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
  {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty = false;
    metainfo[bufferIndex].timeStamp = -1;
    metainfo[bufferIndex].blockNum = -1;
  }
}



StaticBuffer::~StaticBuffer() 
{
  // copy blockAllocMap blocks from buffer to disk(using writeblock() of disk)
  for (int i = 0; i < 4; i++) 
  {
    Disk::writeBlock(blockAllocMap + i * BLOCK_SIZE, i);
  }
  /*iterate through all the buffer blocks,
    write back blocks with metainfo as free:false,dirty:true
  */
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
  {
    if (metainfo[bufferIndex].free == false && metainfo[bufferIndex].dirty == true) 
    {
      Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
    }
  }
}



int StaticBuffer::getFreeBuffer(int blockNum) 
{
	//check if blockNum is valid
  if (blockNum < 0 || blockNum >= DISK_BLOCKS) 
	{
    return E_OUTOFBOUND;
  }

  // increase the timeStamp of all occupied buffers
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
  {
    if (metainfo[bufferIndex].free == false)
    {
      metainfo[bufferIndex].timeStamp++;
    }
  }

  // bufferNum stores the buffer that will be allocated
  int bufferNum = -1;

  // first look for a free buffer
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
  {
    if (metainfo[bufferIndex].free == true)
    {
      bufferNum = bufferIndex;
      break;
    }
  }

  // if no free buffer exists, find the LRU buffer
  if (bufferNum == -1)
  {
    int maxTimeStamp = -1;
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
      if (metainfo[bufferIndex].timeStamp > maxTimeStamp)
      {
        maxTimeStamp = metainfo[bufferIndex].timeStamp;
        bufferNum = bufferIndex;
      }
    }

    // write back the LRU buffer if it is dirty
    if (metainfo[bufferNum].dirty == true)
    {
      Disk::writeBlock(blocks[bufferNum], metainfo[bufferNum].blockNum);
    }
  }

  // update metadata for the allocated buffer
  metainfo[bufferNum].free = false;
  metainfo[bufferNum].dirty = false;
  metainfo[bufferNum].blockNum = blockNum;
  metainfo[bufferNum].timeStamp = 0;

  return bufferNum;
}



int StaticBuffer::getBufferNum(int blockNum) 
{
	//check if blockNum is valid
  if (blockNum < 0 || blockNum >= DISK_BLOCKS) 
	{
    return E_OUTOFBOUND;
  }
  // find and return the bufferIndex which corresponds to blockNum (check metainfo)
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
	{

    if (metainfo[bufferIndex].free == false &&
      metainfo[bufferIndex].blockNum == blockNum) 
		{
			return bufferIndex;
    }
  }
  // if block is not in the buffer
  return E_BLOCKNOTINBUFFER;
}



int StaticBuffer::setDirtyBit(int blockNum)
{
  // find the buffer corresponding to blockNum
  int bufferNum = getBufferNum(blockNum);
  // blockNum is outside the valid range
  if (bufferNum == E_OUTOFBOUND)
  {
    return E_OUTOFBOUND;
  }
  // block is not present in the buffer
  if (bufferNum == E_BLOCKNOTINBUFFER)
  {
    return E_BLOCKNOTINBUFFER;
  }
  // mark the buffer as dirty
  metainfo[bufferNum].dirty = true;
  return SUCCESS;
}


int StaticBuffer::getStaticBlockType(int blockNum)
{
  if(blockNum <= 0 || blockNum >= DISK_BLOCKS)
  {
    return E_OUTOFBOUND;
  }
  return (int)blockAllocMap[blockNum];
}