#include "BlockAccess.h"
#include <cstdio>
#include <cstring>

//for stage 10 question 2 modification
long long comparisonCount = 0;

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) 
{
	// get the previous search index of the relation relId from the relation cache
	// (use RelCacheTable::getSearchIndex() function)
	RecId prevRecId;
	int ret= RelCacheTable::getSearchIndex(relId, &prevRecId);
	if(ret!=SUCCESS)
	{
		return RecId{-1, -1};
	}
	// let block and slot denote the record id of the record being currently checked
	int block;
	int slot;
	// if the current search index record is invalid(i.e. both block and slot = -1)
	if (prevRecId.block == -1 && prevRecId.slot == -1)
	{
		// (no hits from previous search; search should start from the
		// first record itself)
		RelCatEntry relCatEntry;
		// get the first record block of the relation from the relation cache
		// (use RelCacheTable::getRelCatEntry() function of Cache Layer)
		ret=RelCacheTable::getRelCatEntry(relId, &relCatEntry);
		if(ret!=SUCCESS)
		{
			return RecId{-1, -1};
		}
		block = relCatEntry.firstBlk;
		slot = 0;
	}
	else
	{
		// (there is a hit from previous search; search should start from
		// the record next to the search index record)
		block = prevRecId.block;
		slot = prevRecId.slot+1;
	}

	/* The following code searches for the next record in the relation
	that satisfies the given condition
	We start from the record id (block, slot) and iterate over the remaining
	records of the relation
	*/
	while (block != -1)
	{
		/* create a RecBuffer object for block (use RecBuffer Constructor for
		existing block) */
		RecBuffer recBuffer(block);
		// get the record with id (block, slot) using RecBuffer::getRecord()
		// get header of the block using RecBuffer::getHeader() function
		HeadInfo head;
		ret= recBuffer.getHeader(&head);
		if(ret!=SUCCESS)
		{
			return RecId{-1,-1};
		}
		// get slot map of the block using RecBuffer::getSlotMap() function
		int slotCount=head.numSlots;
		unsigned char slotMap[slotCount];
		ret=recBuffer.getSlotMap(slotMap);
		if(ret!=SUCCESS)
		{
			return RecId{-1,-1};
		}
		// If slot >= the number of slots per block(i.e. no more slots in this block)
		if(slot>=slotCount)
		{
			block=head.rblock;
			slot=0;
			continue;
		}

		if(slotMap[slot]==SLOT_UNOCCUPIED)
		{
			slot++;
			continue;
		}

		// compare record's attribute value to the the given attrVal as below:
		Attribute record[head.numAttrs];
    ret = recBuffer.getRecord(record, slot);
    if (ret != SUCCESS) 
		{
      return RecId{-1, -1};
    }
		AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS)
		{
      return RecId{-1, -1};
    }

		Attribute recordAttr = record[attrCatEntry.offset];
		
		comparisonCount++;
		int cmpVal = compareAttrs(recordAttr, attrVal, attrCatEntry.attrType);

		/* Next task is to check whether this record satisfies the given condition.
		It is determined based on the output of previous comparison and
		the op value received.
		The following code sets the cond variable if the condition is satisfied.
		*/
		if (
			(op == NE && cmpVal != 0) ||    // if op is "not equal to"
			(op == LT && cmpVal < 0) ||     // if op is "less than"
			(op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
			(op == EQ && cmpVal == 0) ||    // if op is "equal to"
			(op == GT && cmpVal > 0) ||     // if op is "greater than"
			(op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
		) 
		{
			/*
			set the search index in the relation cache as
			the record id of the record that satisfies the given condition
			(use RelCacheTable::setSearchIndex function)
			*/
			RecId currentRecId;
      currentRecId.block = block;
      currentRecId.slot = slot;
      // Update search index to this newly found record
      ret = RelCacheTable::setSearchIndex(relId, &currentRecId);
      if (ret != SUCCESS) 
			{
        return RecId{-1, -1};
      }
      return currentRecId;
		}
		slot++;
	}

	// no record in the relation with Id relid satisfies the given condition
	return RecId{-1, -1};
}



int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{
	/* reset the searchIndex of the relation catalog using
			RelCacheTable::resetSearchIndex() */
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute newRelationName;    // set newRelationName with newName
	strcpy(newRelationName.sVal, newName);

  // search the relation catalog for an entry with "RelName" = newRelationName
	char relNameAttr[ATTR_SIZE];
	strcpy(relNameAttr, RELCAT_ATTR_RELNAME);
	RecId recId = linearSearch(RELCAT_RELID, relNameAttr, newRelationName, EQ);
	// If relation with name newName already exists (result of linearSearch
	//                                               is not {-1, -1})
	if (recId.block != -1 && recId.slot != -1)
		return E_RELEXIST;

	/* reset the searchIndex of the relation catalog using
			RelCacheTable::resetSearchIndex() */
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute oldRelationName;    // set oldRelationName with oldName
	strcpy(oldRelationName.sVal, oldName);

  // search the relation catalog for an entry with "RelName" = oldRelationName
	recId = linearSearch(RELCAT_RELID, relNameAttr, oldRelationName, EQ);
	
	if(recId.block == -1 && recId.slot == -1)
	{
		return E_RELNOTEXIST;
	}

	/* get the relation catalog record of the relation to rename using a RecBuffer
			on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
	*/
	RecBuffer recBuffer(recId.block);
	
	Attribute record[RELCAT_NO_ATTRS];
	int ret = recBuffer.getRecord(record, recId.slot);
	if (ret != SUCCESS)
	{
		return ret;
	}

	int numAttrs = (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;


	strcpy(record[RELCAT_REL_NAME_INDEX].sVal, newName);

	// set back the record value using RecBuffer.setRecord
	ret = recBuffer.setRecord(record, recId.slot);
	if (ret != SUCCESS)
	{
		return ret;
	}


	// reset the searchIndex of the attribute catalog using
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	for (int i = 0; i <numAttrs; i++)
	{
  		char attrRelName[ATTR_SIZE];
		strcpy(attrRelName, ATTRCAT_ATTR_RELNAME);
		RecId attrRecId = linearSearch(ATTRCAT_RELID, attrRelName, oldRelationName, EQ);
    if (attrRecId.block == -1 && attrRecId.slot == -1)
    {
      return E_RELNOTEXIST;
    }

    RecBuffer attrBuffer(attrRecId.block);

    Attribute attrRecord[ATTRCAT_NO_ATTRS];
    ret = attrBuffer.getRecord(attrRecord, attrRecId.slot);
    if (ret != SUCCESS)
    {
      return ret;
    }

    strcpy(attrRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);

    ret = attrBuffer.setRecord(attrRecord, attrRecId.slot);
    if (ret != SUCCESS)
    {
      return ret;
    }
	}
	return SUCCESS;
}



int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

	// reset the searchIndex of the relation catalog using
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute relNameAttr;    // set relNameAttr to relName
	strcpy(relNameAttr.sVal, relName);

	char relNameAttrName[ATTR_SIZE];
	strcpy(relNameAttrName, RELCAT_ATTR_RELNAME);

	// Search for the relation with name relName in relation catalog using linearSearch()
	RecId relRecId = linearSearch(RELCAT_RELID, relNameAttrName, relNameAttr, EQ);
	
	// If relation with name relName does not exist (search returns {-1,-1})
	if (relRecId.block == -1 && relRecId.slot == -1)	
		return E_RELNOTEXIST;

	// reset the searchIndex of the attribute catalog using
	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	/* declare variable attrToRenameRecId used to store the attr-cat recId
	of the attribute to rename */
	RecId attrToRenameRecId{-1, -1};
	Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

	/* iterate over all Attribute Catalog Entry record corresponding to the
		relation to find the required attribute */
	while (true) 
	{
		// linear search on the attribute catalog for RelName = relNameAttr
		char attrRelNameAttr[ATTR_SIZE];
		strcpy(attrRelNameAttr, ATTRCAT_ATTR_RELNAME);
		RecId attrRecId = linearSearch(ATTRCAT_RELID,attrRelNameAttr,relNameAttr,EQ);
		if (attrRecId.block == -1 && attrRecId.slot == -1)
		{
		break;
		}

		/* Get the record from the attribute catalog using RecBuffer.getRecord
			into attrCatEntryRecord */
		RecBuffer attrBuffer(attrRecId.block);
		int ret = attrBuffer.getRecord( attrCatEntryRecord,attrRecId.slot);
    if (ret != SUCCESS)
    {
      return ret;
    }
		// if attrCatEntryRecord.attrName = oldName
		//     attrToRenameRecId = block and slot of this record
		if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0)
		{
    	attrToRenameRecId = attrRecId;
		}
		// if attrCatEntryRecord.attrName = newName
		//     return E_ATTREXIST;
		if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
		{
    	return E_ATTREXIST;
		}
	}

	// if attrToRenameRecId == {-1, -1}
	//     return E_ATTRNOTEXIST;
	if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1)
	{
    return E_ATTRNOTEXIST;
	}


	RecBuffer attrBuffer(attrToRenameRecId.block);

	int ret = attrBuffer.getRecord(attrCatEntryRecord,attrToRenameRecId.slot);
	if (ret != SUCCESS)
	{
		return ret;
	}

	strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newName);

	ret = attrBuffer.setRecord(attrCatEntryRecord,attrToRenameRecId.slot);
	if (ret != SUCCESS)
	{
		return ret;
	}

	return SUCCESS;
}



int BlockAccess::insert(int relId, Attribute *record) 
{
	// get the relation catalog entry from relation cache
	RelCatEntry relCatEntry;
	int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
	if (ret != SUCCESS)
	{
		return ret;
	}

	int blockNum = relCatEntry.firstBlk;

	// rec_id will be used to store where the new record will be inserted
	RecId rec_id = {-1, -1};

	int numOfSlots = relCatEntry.numSlotsPerBlk;
	int numOfAttributes = relCatEntry.numAttrs;
	int prevBlockNum = -1;

	/*
		Traversing the linked list of existing record blocks of the relation
		until a free slot is found OR
		until the end of the list is reached
	*/
	while (blockNum != -1) 
	{
		// create a RecBuffer object for blockNum (using appropriate constructor!)
		RecBuffer recBuffer(blockNum);	

		// get header of block(blockNum) using RecBuffer::getHeader() function
		HeadInfo head;
    ret = recBuffer.getHeader(&head);
    if (ret != SUCCESS)
    {
      return ret;
    }

		// get slot map of block(blockNum) using RecBuffer::getSlotMap() function
		unsigned char slotMap[numOfSlots];
    ret = recBuffer.getSlotMap(slotMap);
    if (ret != SUCCESS)
    {
      return ret;
    }

		// search for free slot in the block 'blockNum' and store it's rec-id in rec_id
		for (int slot = 0; slot < numOfSlots; slot++)
    {
			if (slotMap[slot] == SLOT_UNOCCUPIED)
			{
				rec_id.block = blockNum;
				rec_id.slot = slot;
				break;
			}
    }

		// free slot found
		if (rec_id.block != -1)
    {
      break;
    }

		//free slot not found
		prevBlockNum = blockNum;
    blockNum = head.rblock;
	}

	//  if no free slot is found in existing record blocks (rec_id = {-1, -1})
	if (rec_id.block == -1)
	{
		// if relation is RELCAT, do not allocate any more blocks
		if (relId == RELCAT_RELID)
    {
      return E_MAXRELATIONS;
    }

		// Otherwise,
		RecBuffer newRecBuffer;
		ret = newRecBuffer.getBlockNum();
		if (ret == E_DISKFULL) 
		{
			return E_DISKFULL;
		}

		// Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
		rec_id.block = ret;
    rec_id.slot = 0;

		//set header for newly allocated block
		HeadInfo head;
		head.blockType = REC;
		head.pblock = -1;
		head.lblock = prevBlockNum;
		head.rblock = -1;
		head.numEntries = 0;
		head.numAttrs = numOfAttributes;
		head.numSlots = numOfSlots;

		ret = newRecBuffer.setHeader(&head);
		if (ret != SUCCESS)
		{
			return ret;
		}

		unsigned char slotMap[numOfSlots];
		for (int i = 0; i < numOfSlots; i++)
		{
			slotMap[i] = SLOT_UNOCCUPIED;
		}

		ret = newRecBuffer.setSlotMap(slotMap);
		if (ret != SUCCESS)
		{
			return ret;
		}

		if (prevBlockNum != -1)
		{
			// create a RecBuffer object for prevBlockNum
			RecBuffer prevRecBuffer(prevBlockNum);

			// get the header of the block prevBlockNum
			HeadInfo prevHead;
			ret = prevRecBuffer.getHeader(&prevHead);
			if (ret != SUCCESS)
			{
				return ret;
			}

			// update the rblock field of the header to the new block
			prevHead.rblock = rec_id.block;

			ret = prevRecBuffer.setHeader(&prevHead);
			if (ret != SUCCESS)
			{
				return ret;
			
			}
		}
		else
		{
			// update first block field in the relation catalog entry to the new block
			relCatEntry.firstBlk = rec_id.block;
			ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
			if (ret != SUCCESS)
			{
				return ret;
			}
		}

		// update last block field in the relation catalog entry to the new block
		relCatEntry.lastBlk = rec_id.block;
		ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
		if (ret != SUCCESS)
		{
			return ret;
		}
	}

	// create a RecBuffer object for rec_id.block
	RecBuffer recBuffer(rec_id.block);

	// insert the record into rec_id'th slot using RecBuffer.setRecord())
	ret = recBuffer.setRecord(record, rec_id.slot);
	if (ret != SUCCESS)
	{
		return ret;
	}

	unsigned char slotMap[numOfSlots];
	ret = recBuffer.getSlotMap(slotMap);
	if (ret != SUCCESS)
	{
		return ret;
	}

	slotMap[rec_id.slot] = SLOT_OCCUPIED;
	ret = recBuffer.setSlotMap(slotMap);
	if (ret != SUCCESS)
	{
		return ret;
	}

	// increment the numEntries field in the header of the block to which record was inserted
	HeadInfo head;
	ret = recBuffer.getHeader(&head);
	if (ret != SUCCESS)
	{
		return ret;
	}

	head.numEntries++;
	ret = recBuffer.setHeader(&head);
	if (ret != SUCCESS)
	{
		return ret;
	}

	// Increment the number of records field in the relation cache entry for the relation
	relCatEntry.numRecs++;
	ret = RelCacheTable::setRelCatEntry(relId, &relCatEntry);
	if (ret != SUCCESS)
	{
		return ret;
	}

	/* B+ Tree Insertions */
	int flag = SUCCESS;

	for (int attrOffset = 0; attrOffset < numOfAttributes; attrOffset++)
	{
		AttrCatEntry attrCatEntry;

		ret = AttrCacheTable::getAttrCatEntry(
			relId, attrOffset, &attrCatEntry);

		if (ret != SUCCESS)
			return ret;

		if (attrCatEntry.rootBlock != -1)
		{
			int retVal = BPlusTree::bPlusInsert(
				relId, attrCatEntry.attrName,
				record[attrOffset], rec_id);

			if (retVal == E_DISKFULL)
				flag = E_INDEX_BLOCKS_RELEASED;
			else if (retVal != SUCCESS)
			{
				printf("DEBUG: BPlusTree::bPlusInsert returned %d\n", retVal);
				return retVal;
			}
		}
	}

	return flag;
}



int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) 
{
	if (strcmp(relName, RELCAT_RELNAME) == 0 ||
    strcmp(relName, ATTRCAT_RELNAME) == 0)
	{
		return E_NOTPERMITTED;
	}

	// reset the searchIndex of the relation catalog
	RelCacheTable::resetSearchIndex(RELCAT_RELID);

	Attribute relNameAttr; 
	strcpy(relNameAttr.sVal, relName);

	//  linearSearch on the relation catalog for RelName = relNameAttr
	char relNameAttrName[ATTR_SIZE];
	strcpy(relNameAttrName, RELCAT_ATTR_RELNAME);
	RecId relCatRecId = linearSearch(
		RELCAT_RELID,
		relNameAttrName,
		relNameAttr,
		EQ
	);
	if (relCatRecId.block == -1 && relCatRecId.slot == -1)
	{
		return E_RELNOTEXIST;
	}

	RecBuffer relCatBuffer(relCatRecId.block);
	Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
	int ret = relCatBuffer.getRecord(relCatEntryRecord, relCatRecId.slot);
	if (ret != SUCCESS)
	{
		return ret;
	}

	int firstBlock =(int)relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
	int numAttrs =(int)relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

	//Delete all the record blocks of the relation
	int currentBlock = firstBlock;
	while (currentBlock != -1)
	{
		RecBuffer recBuffer(currentBlock);
		HeadInfo head;
		ret = recBuffer.getHeader(&head);
		if (ret != SUCCESS)
		{
			return ret;
		}
		int nextBlock = head.rblock;
		recBuffer.releaseBlock();
		currentBlock = nextBlock;
	}

	//Delete all attribute catalog entries corresponding to this relation.

	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
	int numberOfAttributesDeleted = 0;

	while(true) 
	{
		RecId attrCatRecId=linearSearch(
			ATTRCAT_RELID,
			(char *)ATTRCAT_ATTR_RELNAME,
			relNameAttr,
			EQ
		);
		if (attrCatRecId.block == -1 && attrCatRecId.slot == -1)
		{
			break;
		}

		numberOfAttributesDeleted++;

		// create a RecBuffer for attrCatRecId.block
		RecBuffer attrCatBuffer(attrCatRecId.block);
		// get the header of the block
		HeadInfo attrCatHead;
		ret = attrCatBuffer.getHeader(&attrCatHead);
		if (ret != SUCCESS)
		{
			return ret;
		}
		// get the record corresponding to attrCatRecId.slot
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
		ret = attrCatBuffer.getRecord(
			attrCatRecord,
			attrCatRecId.slot
		);
		if (ret != SUCCESS)
		{
			return ret;
		}

		// declare variable rootBlock which will be used to store the root
		// block field from the attribute catalog record.
		int rootBlock = (int)attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
		// (This will be used later to delete any indexes if it exists)

		// Update the Slotmap for the block by setting the slot as SLOT_UNOCCUPIED
		unsigned char slotMap[attrCatHead.numSlots];
		ret = attrCatBuffer.getSlotMap(slotMap);
		if (ret != SUCCESS)
		{
			return ret;
		}
		slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
		ret = attrCatBuffer.setSlotMap(slotMap);
		if (ret != SUCCESS)
		{
			return ret;
		}

		/* Decrement the numEntries in the header of the block corresponding to
			the attribute catalog entry and then set back the header
			using RecBuffer.setHeader */
		attrCatHead.numEntries--;
		ret = attrCatBuffer.setHeader(&attrCatHead);
		if (ret != SUCCESS)
		{
			return ret;
		}

		/* If number of entries become 0, releaseBlock is called after fixing
			the linked list.
		*/
		if (attrCatHead.numEntries == 0) 
		{
			int leftBlock = attrCatHead.lblock;
			int rightBlock = attrCatHead.rblock;

			// create a RecBuffer for lblock and call appropriate methods
			if (leftBlock != -1)
			{
				RecBuffer leftBuffer(leftBlock);
				HeadInfo leftHead;
				ret = leftBuffer.getHeader(&leftHead);
				if (ret != SUCCESS)
				{
					return ret;
				}
				leftHead.rblock = rightBlock;
				ret = leftBuffer.setHeader(&leftHead);
				if (ret != SUCCESS)
				{
					return ret;
				}
			}
			if (rightBlock != -1) 
			{
				RecBuffer rightBuffer(rightBlock);
				HeadInfo rightHead;
				ret = rightBuffer.getHeader(&rightHead);
				if (ret != SUCCESS)
				{
					return ret;
				}
				rightHead.lblock = leftBlock;
				ret = rightBuffer.setHeader(&rightHead);
				if (ret != SUCCESS)
				{
					return ret;
				}
			} 
			else 
			{
				// (the block being released is the "Last Block" of the relation.)
				RelCatEntry attrCatRelEntry;
				ret = RelCacheTable::getRelCatEntry(
					ATTRCAT_RELID,
					&attrCatRelEntry
				);
				if (ret != SUCCESS)
				{
					return ret;
				}
				attrCatRelEntry.lastBlk = leftBlock;
				ret = RelCacheTable::setRelCatEntry(
					ATTRCAT_RELID,
					&attrCatRelEntry
				);
				if (ret != SUCCESS)
				{
					return ret;
				}
			}
			attrCatBuffer.releaseBlock();
		}
		

		if (rootBlock != -1) 
		{
			ret = BPlusTree::bPlusDestroy(rootBlock);
			if (ret != SUCCESS)
				return ret;
		}
	}

	// Get RELCAT block header
	HeadInfo relCatHead;
	ret = relCatBuffer.getHeader(&relCatHead);
	if (ret != SUCCESS)
	{
		return ret;
	}

	// Decrement number of entries in RELCAT block
	relCatHead.numEntries--;
	ret = relCatBuffer.setHeader(&relCatHead);
	if (ret != SUCCESS)
	{
		return ret;
	}

	// Get RELCAT slot map
	unsigned char relCatSlotMap[relCatHead.numSlots];
	ret = relCatBuffer.getSlotMap(relCatSlotMap);
	if (ret != SUCCESS)
	{
		return ret;
	}

	// Mark the relation's slot as free
	relCatSlotMap[relCatRecId.slot] = SLOT_UNOCCUPIED;
	ret = relCatBuffer.setSlotMap(relCatSlotMap);
	if (ret != SUCCESS)
	{
		return ret;
	}


	RelCatEntry relCatEntry;
	ret = RelCacheTable::getRelCatEntry(
		RELCAT_RELID,
		&relCatEntry
	);
	if (ret != SUCCESS)
	{
		return ret;
	}

	relCatEntry.numRecs--;
	ret = RelCacheTable::setRelCatEntry(
		RELCAT_RELID,
		&relCatEntry
	);
	if (ret != SUCCESS)
	{
		return ret;
	}

	RelCatEntry attrCatRelEntry;
	ret = RelCacheTable::getRelCatEntry(
		ATTRCAT_RELID,
		&attrCatRelEntry
	);
	if (ret != SUCCESS)
	{
		return ret;
	}

	attrCatRelEntry.numRecs -= numberOfAttributesDeleted;
	ret = RelCacheTable::setRelCatEntry(
		ATTRCAT_RELID,
		&attrCatRelEntry
	);
	if (ret != SUCCESS)
	{
		return ret;
	}
	return SUCCESS;
}



int BlockAccess::project(int relId, Attribute *record) 
{
	RecId prevRecId;
  RelCacheTable::getSearchIndex(relId, &prevRecId);

	int block, slot;
	if (prevRecId.block == -1 && prevRecId.slot == -1)
	{
		// (new project operation. start from beginning)
		RelCatEntry relCatEntry;
		RelCacheTable::getRelCatEntry(relId, &relCatEntry);
		block = relCatEntry.firstBlk;
		slot = 0;
	}
	else
	{
		// (a project/search operation is already in progress)
		block = prevRecId.block;
    slot = prevRecId.slot + 1;		
	}


	// The following code finds the next record of the relation
	while (block != -1)
	{
		RecBuffer recBuffer(block);

		HeadInfo head;
		recBuffer.getHeader(&head);

		unsigned char slotMap[head.numSlots];
		recBuffer.getSlotMap(slotMap);

		if(slot >= head.numSlots)
		{
			// (no more slots in this block)
			block = head.rblock;
			slot = 0;
		}
		else if (slotMap[slot] == SLOT_UNOCCUPIED)
		{ 
			// (i.e slot-th entry in slotMap contains SLOT_UNOCCUPIED)
			slot++;
		}
		else 
		{
			// (the next occupied slot / record has been found)
			break;
		}
	}

	if (block == -1)
	{
		// (a record was not found. all records exhausted)
		return E_NOTFOUND;
	}

	// declare nextRecId to store the RecId of the record found
	RecId nextRecId{block, slot};
  RelCacheTable::setSearchIndex(relId, &nextRecId);

	RecBuffer recBuffer(nextRecId.block);
  recBuffer.getRecord(record, nextRecId.slot);

	return SUCCESS;
}



int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) 
{
	// Declare a variable called recid to store the searched record
	RecId recId;
	AttrCatEntry attrCatEntry;
	int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS) 
	{
    return ret;
  }

	// If index does not exist for the attribute
  if (attrCatEntry.rootBlock == -1) 
	{
    // Perform linear search
    recId = linearSearch(relId, attrName, attrVal, op);
  }

	else 
	{
		// if index exists then perform B+ tree search
    recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
	}


	// No record satisfying the condition was found
  if (recId.block == -1 && recId.slot == -1) 
	{
    return E_NOTFOUND;
  }

	// Fetch the actual record using its record ID
  RecBuffer recBuffer(recId.block);
  ret = recBuffer.getRecord(record, recId.slot);
  if (ret != SUCCESS) 
	{
    return ret;
  }

	return SUCCESS;
}