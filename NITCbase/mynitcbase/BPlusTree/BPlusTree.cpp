#include "BPlusTree.h"

#include <cstring>
#include <cstdio>



RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) 
{
  // declare searchIndex which will be used to store search index for attrName.
  IndexId searchIndex;

  int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
  if(ret != SUCCESS)
  {
    return RecId{-1, -1};
  }

  AttrCatEntry attrCatEntry;
  ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if(ret != SUCCESS)
  {
    return RecId{-1, -1};
  }

  // declare variables block and index which will be used during search
  int block, index;

  if (searchIndex.block == -1 && searchIndex.index == -1) 
  {
    // First search: start from root
    block = attrCatEntry.rootBlock;
    index = 0;

    // No B+ tree exists for this attribute
    if (block == -1) 
    {
      return RecId{-1, -1};
    }
  } 
  else 
  {
    // Continue from the entry after the previous hit
    block = searchIndex.block;
    index = searchIndex.index + 1;  // search is resumed from the next index.

    // load block into leaf using IndLeaf::IndLeaf().
    IndLeaf leaf(block);

    // declare leafHead which will be used to hold the header of leaf.
    HeadInfo leafHead;
    ret = leaf.getHeader(&leafHead);
    if(ret != SUCCESS)
    {
      return RecId{-1, -1};
    }

    if (index >= leafHead.numEntries) 
    {
      block = leafHead.rblock;
      index = 0;
      if (block == -1) 
      {
        // (end of linked list reached - the search is done.)
        return RecId{-1, -1};
      }
    }
  }

  /******  Traverse through all the internal nodes according to value
            of attrVal and the operator op                             ******/

  /* (This section is only needed when
      - search restarts from the root block (when searchIndex is reset by caller)
      - root is not a leaf
      If there was a valid search index, then we are already at a leaf block
      and the test condition in the following loop will fail)
  */

  while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) 
  { 

    // load the block into internalBlk using IndInternal::IndInternal().
    IndInternal internalBlk(block);

    HeadInfo intHead;

    ret = internalBlk.getHeader(&intHead);
    if(ret != SUCCESS)
    {
      return RecId{-1, -1};
    }

    // declare intEntry which will be used to store an entry of internalBlk.
    InternalEntry intEntry;

    if (op == NE || op == LT || op == LE) 
    {
      /*
      - NE: need to search the entire linked list of leaf indices of the B+ Tree,
      starting from the leftmost leaf index. Thus, always move to the left.

      - LT and LE: the attribute values are arranged in ascending order in the
      leaf indices of the B+ Tree. Values that satisfy these conditions, if
      any exist, will always be found in the left-most leaf index. Thus,
      always move to the left.
      */

      internalBlk.getEntry(&intEntry, 0);
      block = intEntry.lChild;
    } 
    else 
    {
      bool found = false;

      for(int i = 0; i < intHead.numEntries; i++)
      {
        internalBlk.getEntry(&intEntry, i);

        comparisonCount++;
        int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);

        if((op == EQ || op == GE) && cmpVal >= 0)
        {
          block = intEntry.lChild;
          found = true;
          break;
        }

        if(op == GT && cmpVal > 0)
        {
          block = intEntry.lChild;
          found = true;
          break;
        }
      }

      if (!found) 
      {
        internalBlk.getEntry(&intEntry, intHead.numEntries - 1);
        block = intEntry.rChild;
      }
    }
  }

  // NOTE: `block` now has the block number of a leaf index block.
  /******  Identify the first leaf index entry from the current position
    that satisfies our condition (moving right)             ******/

  while (block != -1) 
  {
    // load the block into leafBlk using IndLeaf::IndLeaf().
    IndLeaf leafBlk(block);
    HeadInfo leafHead;

    // load the header to leafHead using BlockBuffer::getHeader().
    ret = leafBlk.getHeader(&leafHead);
    if(ret != SUCCESS)
    {
      return RecId{-1, -1};
    }

    // declare leafEntry which will be used to store an entry from leafBlk
    Index leafEntry;

    while (index < leafHead.numEntries) 
    {
      ret = leafBlk.getEntry(&leafEntry, index);
      if(ret != SUCCESS)
      {
        return RecId{-1, -1};
      }
      
      comparisonCount++;
      int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);
      if ( (op == EQ && cmpVal == 0) ||
        (op == LE && cmpVal <= 0) ||
        (op == LT && cmpVal < 0) ||
        (op == GT && cmpVal > 0) ||
        (op == GE && cmpVal >= 0) ||
        (op == NE && cmpVal != 0)
      ) 

      {
        // (entry satisfying the condition found)
        IndexId newSearchIndex;
        newSearchIndex.block = block;
        newSearchIndex.index = index;

        ret = AttrCacheTable::setSearchIndex(relId, attrName, &newSearchIndex);
        if(ret != SUCCESS)
        {
          return RecId{-1, -1};
        }
        return RecId{leafEntry.block, leafEntry.slot};
      } 
      else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) 
      {
        /*future entries will not satisfy EQ, LE, LT since the values
        are arranged in ascending order in the leaves */
        return RecId{-1, -1};
      }

      // search next index.
      ++index;
    }

    /*only for NE operation do we have to check the entire linked list;
    for all the other op it is guaranteed that the block being searched
    will have an entry, if it exists, satisying that op. */
    if (op != NE) 
    {
      break;
    }
    block = leafHead.rblock;
    index = 0;
  }

  // no entry satisying the op was found
  return RecId{-1, -1};
}



int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE])
{
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
  {
    return E_NOTPERMITTED;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }
  if (attrCatEntry.rootBlock != -1)
  {
    return SUCCESS;
  }

  IndLeaf rootBlockBuf;
  int rootBlock = rootBlockBuf.getBlockNum();
  if (rootBlock < 0)
  {
    return rootBlock;
  }

  attrCatEntry.rootBlock = rootBlock;
  ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  RelCatEntry relCatEntry;
  ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int block = relCatEntry.firstBlk;

  while (block != -1)
  {
    RecBuffer recBuffer(block);

    unsigned char slotMap[relCatEntry.numSlotsPerBlk];
    ret = recBuffer.getSlotMap(slotMap);
    if (ret != SUCCESS)
    {
      return ret;
    }

    for (int slot = 0; slot < relCatEntry.numSlotsPerBlk; slot++)
    {
      if (slotMap[slot] == SLOT_OCCUPIED)
      {
        Attribute record[relCatEntry.numAttrs];
        ret = recBuffer.getRecord(record, slot);
        if (ret != SUCCESS)
        {
          return ret;
        }

        RecId recId{block, slot};
        ret = bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId);
        if (ret == E_DISKFULL)
        {
          return E_DISKFULL;
        }
        if (ret != SUCCESS)
        {
          printf("DEBUG: bPlusInsert failed: ret=%d, record block=%d, slot=%d\n",
           ret, recId.block, recId.slot);
          return ret;
        }
      }
    }

    HeadInfo head;
    ret = recBuffer.getHeader(&head);

    if (ret != SUCCESS)
    {
      return ret;
    }
    block = head.rblock;
  }
  return SUCCESS;
}



int BPlusTree::bPlusDestroy(int rootBlockNum)
{
  if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
  {
    return E_OUTOFBOUND;
  }

  int type = StaticBuffer::getStaticBlockType(rootBlockNum);
  if (type == IND_LEAF)
  {
    IndLeaf leaf(rootBlockNum);
    leaf.releaseBlock();

    return SUCCESS;
  }
  else if (type == IND_INTERNAL)
  {
    IndInternal internalBlk(rootBlockNum);

    HeadInfo head;
    int ret = internalBlk.getHeader(&head);
    if (ret != SUCCESS)
    {
      return ret;
    }

    InternalEntry entry;
    for (int i = 0; i < head.numEntries; i++)
    {
      ret = internalBlk.getEntry(&entry, i);
      if (ret != SUCCESS)
      {
        return ret;
      }

      if (i == 0)
      {
        ret = bPlusDestroy(entry.lChild);
        if (ret != SUCCESS)
        {
          return ret;
        }
      }

      ret = bPlusDestroy(entry.rChild);
      if (ret != SUCCESS)
      {
        return ret;
      }
    }

    internalBlk.releaseBlock();
    return SUCCESS;
  }
  else
  {
    return E_INVALIDBLOCK;
  }
}



int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId)
{
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int rootBlock = attrCatEntry.rootBlock;
  if (rootBlock == -1)
  {
    return E_NOINDEX;
  }

  int leafBlkNum = findLeafToInsert(rootBlock, attrVal, attrCatEntry.attrType);
  if (leafBlkNum < 0)
  {
    printf("DEBUG: findLeafToInsert failed: ret=%d, root=%d, record=(%d,%d)\n",
      leafBlkNum, rootBlock, recId.block, recId.slot);
    return leafBlkNum;
  }
  Index entry;
  entry.attrVal = attrVal;
  entry.block = recId.block;
  entry.slot = recId.slot;

  ret = insertIntoLeaf(relId, attrName, leafBlkNum, entry);
  if (ret != SUCCESS)
  {
    printf("DEBUG: insertIntoLeaf failed: ret=%d, leaf=%d, record=(%d,%d)\n",
      ret, leafBlkNum, recId.block, recId.slot);
  }
  if (ret == E_DISKFULL)
  {
    bPlusDestroy(rootBlock);
    attrCatEntry.rootBlock = -1;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
    return E_DISKFULL;
  }
  return ret;
}



int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType)
{
  int blockNum = rootBlock;
  while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF)
  {
    IndInternal internalBlk(blockNum);

    HeadInfo head;
    int ret = internalBlk.getHeader(&head);
    if (ret != SUCCESS)
    {
      return ret;
    }

    InternalEntry entry;
    bool found = false;
    for (int i = 0; i < head.numEntries; i++)
    {
      ret = internalBlk.getEntry(&entry, i);
      if (ret != SUCCESS)
      {
        return ret;
      }

      if (compareAttrs(entry.attrVal, attrVal, attrType) >= 0)
      {
        blockNum = entry.lChild;
        found = true;
        break;
      }
    }

    if (!found)
    {
      ret = internalBlk.getEntry(&entry, head.numEntries - 1);
      if (ret != SUCCESS)
      {
        return ret;
      }
      blockNum = entry.rChild;
    }
  }
  return blockNum;
}



int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry)
{
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  IndLeaf leaf(blockNum);
  HeadInfo blockHeader;
  ret = leaf.getHeader(&blockHeader);
  if (ret != SUCCESS)
  {
    return ret;
  }

  Index indices[blockHeader.numEntries + 1];
  int i = 0;
  bool inserted = false;
  for (int j = 0; j < blockHeader.numEntries; j++)
  {
    Index current;
    ret = leaf.getEntry(&current, j);
    if (ret != SUCCESS)
    {
      return ret;
    }

    if (!inserted && compareAttrs(indexEntry.attrVal, current.attrVal, attrCatEntry.attrType) < 0)
    {
      indices[i++] = indexEntry;
      inserted = true;
    }
    indices[i++] = current;
  }

  if (!inserted)
  {
    indices[i++] = indexEntry;
  }
  if (blockHeader.numEntries != MAX_KEYS_LEAF)
  {
    blockHeader.numEntries++;
    ret = leaf.setHeader(&blockHeader);
    if (ret != SUCCESS)
    {
      return ret;
    }

    for (int j = 0; j < blockHeader.numEntries; j++)
    {
      ret = leaf.setEntry(&indices[j], j);
      if (ret != SUCCESS)
      {
        return ret;
      }
    }
    return SUCCESS;
  }
  int newRightBlk = splitLeaf(blockNum, indices);
  if (newRightBlk < 0)
  {
    return newRightBlk;
  }

  if (blockHeader.pblock != -1)
  {
    InternalEntry newEntry;
    newEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal;
    newEntry.lChild = blockNum;
    newEntry.rChild = newRightBlk;
    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, newEntry);
  }
  else
  {
    ret = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
  }
  if (ret == E_DISKFULL)
  {
    return E_DISKFULL;
  }
  return ret;
}



int BPlusTree::splitLeaf(int leafBlockNum, Index indices[])
{
  IndLeaf rightBlk;
  IndLeaf leftBlk(leafBlockNum);

  int rightBlkNum = rightBlk.getBlockNum();
  int leftBlkNum = leftBlk.getBlockNum();
  if (rightBlkNum < 0)
    return rightBlkNum;

  HeadInfo leftBlkHeader, rightBlkHeader;
  int ret = leftBlk.getHeader(&leftBlkHeader);
  if (ret != SUCCESS)
  {
    return ret;
  }
  
  ret = rightBlk.getHeader(&rightBlkHeader);
  if (ret != SUCCESS)
  {
    return ret;
  }

  rightBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
  rightBlkHeader.pblock = leftBlkHeader.pblock;
  rightBlkHeader.lblock = leftBlkNum;
  rightBlkHeader.rblock = leftBlkHeader.rblock;

  ret = rightBlk.setHeader(&rightBlkHeader);
  if (ret != SUCCESS)
  {
    return ret;
  }

  leftBlkHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
  leftBlkHeader.rblock = rightBlkNum;

  ret = leftBlk.setHeader(&leftBlkHeader);
  if (ret != SUCCESS)
  {
    return ret;
  }
  

  for (int i = 0; i < (MAX_KEYS_LEAF + 1) / 2; i++)
  {
    ret = leftBlk.setEntry(&indices[i], i);
    if (ret != SUCCESS)
    {
      return ret;
    }

    ret = rightBlk.setEntry(&indices[i + (MAX_KEYS_LEAF + 1) / 2], i);
    if (ret != SUCCESS)
    {
      return ret;
    }
  }
  return rightBlkNum;
}



int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE],
  int intBlockNum, InternalEntry intEntry)
{
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
    return ret;

  IndInternal intBlk(intBlockNum);

  HeadInfo blockHeader;
  ret = intBlk.getHeader(&blockHeader);
  if (ret != SUCCESS)
    return ret;

  InternalEntry internalEntries[blockHeader.numEntries + 1];

  int i = 0;
  bool inserted = false;

  for (int j = 0; j < blockHeader.numEntries; j++)
  {
    InternalEntry current;
    ret = intBlk.getEntry(&current, j);
    if (ret != SUCCESS)
      return ret;

    if (!inserted &&
      compareAttrs(intEntry.attrVal, current.attrVal,
      attrCatEntry.attrType) < 0)
    {
      internalEntries[i++] = intEntry;
      current.lChild = intEntry.rChild;
      inserted = true;
    }

    internalEntries[i++] = current;
  }

  if (!inserted)
  {
    if (blockHeader.numEntries > 0)
      internalEntries[i - 1].rChild = intEntry.lChild;
    internalEntries[i++] = intEntry;
  }

  if (blockHeader.numEntries != MAX_KEYS_INTERNAL)
  {
    blockHeader.numEntries++;
    ret = intBlk.setHeader(&blockHeader);
    if (ret != SUCCESS)
      return ret;

    for (int j = 0; j < blockHeader.numEntries; j++)
    {
      ret = intBlk.setEntry(&internalEntries[j], j);
      if (ret != SUCCESS)
        return ret;
    }

    return SUCCESS;
  }

  int newRightBlk = splitInternal(intBlockNum, internalEntries);

  if (newRightBlk < 0)
  {
    return newRightBlk;
  }

  if (blockHeader.pblock != -1)
  {
    InternalEntry newEntry;
    newEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
    newEntry.lChild = intBlockNum;
    newEntry.rChild = newRightBlk;

    ret = insertIntoInternal(relId, attrName, blockHeader.pblock, newEntry);
  }
  else
  {
    ret = createNewRoot(relId, attrName,
      internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
      intBlockNum, newRightBlk);
  }

  if (ret == E_DISKFULL)
    return E_DISKFULL;

  if (ret != SUCCESS)
    return ret;

  return SUCCESS;
}



int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[])
{
  IndInternal rightBlk;
  IndInternal leftBlk(intBlockNum);

  int rightBlkNum = rightBlk.getBlockNum();
  int leftBlkNum = leftBlk.getBlockNum();

  if (rightBlkNum < 0)
    return rightBlkNum;

  HeadInfo leftBlkHeader, rightBlkHeader;

  int ret = leftBlk.getHeader(&leftBlkHeader);
  if (ret != SUCCESS)
    return ret;

  ret = rightBlk.getHeader(&rightBlkHeader);
  if (ret != SUCCESS)
    return ret;

  rightBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;
  rightBlkHeader.pblock = leftBlkHeader.pblock;

  ret = rightBlk.setHeader(&rightBlkHeader);
  if (ret != SUCCESS)
    return ret;

  leftBlkHeader.numEntries = MAX_KEYS_INTERNAL / 2;

  ret = leftBlk.setHeader(&leftBlkHeader);
  if (ret != SUCCESS)
    return ret;

  // Populate the left and right internal blocks.
  for (int i = 0; i < MAX_KEYS_INTERNAL / 2; i++)
  {
    ret = leftBlk.setEntry(&internalEntries[i], i);
    if (ret != SUCCESS)
      return ret;

    ret = rightBlk.setEntry(
      &internalEntries[i + MAX_KEYS_INTERNAL / 2 + 1], i);
    if (ret != SUCCESS)
      return ret;
  }

  // Update the parent pointers of all children in the new right block.
  for (int i = 0; i < MAX_KEYS_INTERNAL / 2; i++)
  {
    int entryIndex = i + MAX_KEYS_INTERNAL / 2 + 1;

    BlockBuffer childBlk(internalEntries[entryIndex].lChild);
    HeadInfo childHeader;

    ret = childBlk.getHeader(&childHeader);
    if (ret != SUCCESS)
      return ret;

    childHeader.pblock = rightBlkNum;

    ret = childBlk.setHeader(&childHeader);
    if (ret != SUCCESS)
      return ret;

    BlockBuffer rightChildBlk(internalEntries[entryIndex].rChild);

    ret = rightChildBlk.getHeader(&childHeader);
    if (ret != SUCCESS)
      return ret;

    childHeader.pblock = rightBlkNum;

    ret = rightChildBlk.setHeader(&childHeader);
    if (ret != SUCCESS)
      return ret;
  }

  return rightBlkNum;
}



int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE],
  Attribute attrVal, int lChild, int rChild)
{
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
    return ret;

  IndInternal newRootBlk;
  int newRootBlkNum = newRootBlk.getBlockNum();

  if (newRootBlkNum < 0)
  {
    return newRootBlkNum;
  }

  HeadInfo rootHeader;
  ret = newRootBlk.getHeader(&rootHeader);
  if (ret != SUCCESS)
    return ret;

  rootHeader.numEntries = 1;
  ret = newRootBlk.setHeader(&rootHeader);
  if (ret != SUCCESS)
    return ret;

  InternalEntry rootEntry;
  rootEntry.lChild = lChild;
  rootEntry.attrVal = attrVal;
  rootEntry.rChild = rChild;

  ret = newRootBlk.setEntry(&rootEntry, 0);
  if (ret != SUCCESS)
    return ret;

  // Update the parent block of the left child.
  BlockBuffer leftChildBlk(lChild);
  HeadInfo childHeader;

  ret = leftChildBlk.getHeader(&childHeader);
  if (ret != SUCCESS)
    return ret;

  childHeader.pblock = newRootBlkNum;
  ret = leftChildBlk.setHeader(&childHeader);
  if (ret != SUCCESS)
    return ret;

  // Update the parent block of the right child.
  BlockBuffer rightChildBlk(rChild);

  ret = rightChildBlk.getHeader(&childHeader);
  if (ret != SUCCESS)
    return ret;

  childHeader.pblock = newRootBlkNum;
  ret = rightChildBlk.setHeader(&childHeader);
  if (ret != SUCCESS)
    return ret;

  // Update the root block number in the attribute cache.
  attrCatEntry.rootBlock = newRootBlkNum;
  ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
    return ret;

  return SUCCESS;
}