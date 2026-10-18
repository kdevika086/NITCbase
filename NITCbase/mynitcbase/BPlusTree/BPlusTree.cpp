#include "BPlusTree.h"

#include <cstring>



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