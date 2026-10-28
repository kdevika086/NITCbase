#include "OpenRelTable.h"
#include <cstring>
#include <cstdlib>


OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

OpenRelTable::OpenRelTable() 
{

  // initialise all values in relCache and attrCache to be nullptr and all entries
  // in tableMetaInfo to be free
  for (int i = 0; i < MAX_OPEN; ++i) 
  {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    tableMetaInfo[i].free = true;
  }

  // load the relation and attribute catalog into the relation cache (we did this already)
	
	/**** setting up Relation Catalog relation in the Relation Cache Table****/
  RecBuffer relCatBlock(RELCAT_BLOCK);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  // allocate this on the heap because we want it to persist outside this function
  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;


  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/
  Attribute attrCatRelRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(attrCatRelRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);

  struct RelCacheEntry attrCatRelCacheEntry;
  RelCacheTable::recordToRelCatEntry(attrCatRelRecord, &attrCatRelCacheEntry.relCatEntry);
  attrCatRelCacheEntry.recId.block = RELCAT_BLOCK;
  attrCatRelCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));

  *(RelCacheTable::relCache[ATTRCAT_RELID]) = attrCatRelCacheEntry;

  // load the relation and attribute catalog into the attribute cache (we did this already)

  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  AttrCacheEntry* prev = nullptr;
  for (int i = 0; i < ATTRCAT_NO_ATTRS; i++) 
	{
		attrCatBlock.getRecord(attrCatRecord, i);
		AttrCacheEntry* entry =(AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
		entry->dirty = false;
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);
		entry->recId.block = ATTRCAT_BLOCK;
		entry->recId.slot = i;
		entry->next = nullptr;
		//linking part
		if (i == 0) 
		{
			AttrCacheTable::attrCache[RELCAT_RELID] = entry;
		}
		else 
		{
			prev->next = entry;
		}
		prev = entry;
  }

  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/
  AttrCacheEntry* prevAttr = nullptr;
  for (int i = 6; i < ATTRCAT_NO_ATTRS * 2; i++) 
	{
		attrCatBlock.getRecord(attrCatRecord, i);
		AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
		entry->dirty = false;
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &entry->attrCatEntry);

		entry->recId.block = ATTRCAT_BLOCK;
		entry->recId.slot = i;
		entry->next = nullptr;
		//linking part
		if (i == 6) 
		{
			AttrCacheTable::attrCache[ATTRCAT_RELID] = entry;
		}
		else 
		{	
			prevAttr->next = entry;
		}
		prevAttr = entry;
	}


  /************ Setting up tableMetaInfo entries ************/
  // in the tableMetaInfo array
  //   set free = false for RELCAT_RELID and ATTRCAT_RELID
  //   set relname for RELCAT_RELID and ATTRCAT_RELID
	tableMetaInfo[RELCAT_RELID].free = false;
	strcpy(tableMetaInfo[RELCAT_RELID].relName, "RELATIONCAT");

	tableMetaInfo[ATTRCAT_RELID].free = false;
	strcpy(tableMetaInfo[ATTRCAT_RELID].relName, "ATTRIBUTECAT");

}



OpenRelTable::~OpenRelTable()
{
  // Close all open relations (from rel-id = 2 onwards)
  for (int i = 2; i < MAX_OPEN; ++i)
  {
    if (!tableMetaInfo[i].free)
    {
      OpenRelTable::closeRel(i);
    }
  }

  /**** Closing the Attribute Catalog relation in the relation cache ****/
  if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty)
  {
    RelCatEntry relCatEntry = RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry;
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

    RecId recId =RelCacheTable::relCache[ATTRCAT_RELID]->recId;
    RecBuffer relCatBlock(recId.block);
    relCatBlock.setRecord(relCatRecord, recId.slot);
  }

  // Free the relation cache entry of ATTRIBUTECAT
  free(RelCacheTable::relCache[ATTRCAT_RELID]);
  RelCacheTable::relCache[ATTRCAT_RELID] = nullptr;


  /**** Closing the Relation Catalog relation in the relation cache ****/
  if (RelCacheTable::relCache[RELCAT_RELID]->dirty)
  {
    RelCatEntry relCatEntry =RelCacheTable::relCache[RELCAT_RELID]->relCatEntry;
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

    RecId recId =RelCacheTable::relCache[RELCAT_RELID]->recId;
    RecBuffer relCatBlock(recId.block);

    relCatBlock.setRecord(relCatRecord, recId.slot);
  }

  // Free the relation cache entry of RELATIONCAT
  free(RelCacheTable::relCache[RELCAT_RELID]);
  RelCacheTable::relCache[RELCAT_RELID] = nullptr;


  /**** Free attribute cache entries of RELATIONCAT ****/
  AttrCacheEntry* current =AttrCacheTable::attrCache[RELCAT_RELID];
  while (current != nullptr)
  {
    AttrCacheEntry* next = current->next;
    free(current);
    current = next;
  }
  AttrCacheTable::attrCache[RELCAT_RELID] = nullptr;


  /**** Free attribute cache entries of ATTRIBUTECAT ****/
  current =AttrCacheTable::attrCache[ATTRCAT_RELID];

  while (current != nullptr)
  {
    AttrCacheEntry* next = current->next;
    free(current);
    current = next;
  }
  AttrCacheTable::attrCache[ATTRCAT_RELID] = nullptr;
}



int OpenRelTable::getFreeOpenRelTableEntry() 
{
	for (int i = 0; i < MAX_OPEN; i++) 
	{
		if (tableMetaInfo[i].free == true) 
		{
			return i;
		}
	}
	return E_CACHEFULL;
}



int OpenRelTable::getRelId(char relName[ATTR_SIZE]) 
{
	for(int i=0; i<MAX_OPEN; i++)
	{
		if(!tableMetaInfo[i].free &&  strcmp(tableMetaInfo[i].relName, relName)==0)
		{
			return i;
		}
	}

	return E_RELNOTOPEN;
}



int OpenRelTable::openRel(char relName[ATTR_SIZE]) 
{

	int relId = OpenRelTable::getRelId(relName);
  if(relId != E_RELNOTOPEN)
	{
    return relId;
  }

  // find a free slot in the Open Relation Table
	relId = OpenRelTable::getFreeOpenRelTableEntry();
  if (relId == E_CACHEFULL)
	{
    return E_CACHEFULL;
  }

  /****** Setting up Relation Cache entry for the relation ******/
	RecId searchIndex;
	searchIndex.block = -1;
	searchIndex.slot = -1;
	RelCacheTable::setSearchIndex(RELCAT_RELID, &searchIndex);

	Attribute attrVal;
	strcpy(attrVal.sVal, relName);

  /* search for the entry with relation name, relName, in the Relation Catalog using
      BlockAccess::linearSearch().
      Care should be taken to reset the searchIndex of the relation RELCAT_RELID
      before calling linearSearch().*/
	RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,attrVal,EQ);

  if (relcatRecId.block == -1 && relcatRecId.slot == -1) 
	{
    return E_RELNOTEXIST;
  }
  /* read the record entry corresponding to relcatRecId and create a relCacheEntry
      on it using RecBuffer::getRecord() and RelCacheTable::recordToRelCatEntry().
      update the recId field of this Relation Cache entry to relcatRecId.
      use the Relation Cache entry to set the relId-th entry of the RelCacheTable.
    NOTE: make sure to allocate memory for the RelCacheEntry using malloc()
  */
	RecBuffer relCatBlock(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	relCatBlock.getRecord(relCatRecord,relcatRecId.slot);

	RelCacheEntry relCacheEntry;
	RelCacheTable::recordToRelCatEntry(relCatRecord,&relCacheEntry.relCatEntry);
	relCacheEntry.recId = relcatRecId;

	RelCacheTable::relCache[relId] =(RelCacheEntry*)malloc(sizeof(RelCacheEntry));
	*(RelCacheTable::relCache[relId]) = relCacheEntry;


  /****** Setting up Attribute Cache entry for the relation ******/

  // let listHead be used to hold the head of the linked list of attrCache entries.
  AttrCacheEntry* listHead = nullptr;
	AttrCacheEntry* listTail = nullptr;
	
	searchIndex.block = -1;
	searchIndex.slot = -1;

	RelCacheTable::setSearchIndex(ATTRCAT_RELID, &searchIndex);

  /*iterate over all the entries in the Attribute Catalog corresponding to each
  attribute of the relation relName by multiple calls of BlockAccess::linearSearch()
  care should be taken to reset the searchIndex of the relation, ATTRCAT_RELID,
  corresponding to Attribute Catalog before the first call to linearSearch().*/

	// search for the first attribute of relName in the Attribute Catalog
	RecId attrcatRecId;

	while (true)
	{
		attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID,(char*)ATTRCAT_ATTR_RELNAME,attrVal,EQ);
		// no more attributes found
		if (attrcatRecId.block == -1 && attrcatRecId.slot == -1)
		{
			break;
		}

		// read the matching Attribute Catalog record
		RecBuffer attrCatBlock(attrcatRecId.block);
		Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

		attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);

		// create a new Attribute Cache entry
		AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    entry->dirty = false;
		AttrCacheTable::recordToAttrCatEntry(attrCatRecord,&entry->attrCatEntry);

		entry->recId = attrcatRecId;
		entry->next = nullptr;

		// add entry to the linked list
		if (listHead == nullptr)
		{
			listHead = entry;
			listTail = entry;
		}
		else
		{
			listTail->next = entry;
			listTail = entry;
		}
	}
  // set the relId-th entry of the AttrCacheTable to listHead
  AttrCacheTable::attrCache[relId] = listHead;
  /****** Setting up metadata in the Open Relation Table for the relation******/
  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}



int OpenRelTable::closeRel(int relId) 
{
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
  {
    return E_NOTPERMITTED;
  }
  if (relId < 0 || relId >= MAX_OPEN)
  {
    return E_OUTOFBOUND;
  }
  if (tableMetaInfo[relId].free)
  {
    return E_RELNOTOPEN;
  }

  /****** Releasing the Relation Cache entry of the relation ******/

  if (RelCacheTable::relCache[relId]->dirty)
  {
    /* Get the Relation Catalog entry from RelCacheTable::relCache
    Then convert it to a record using RelCacheTable::relCatEntryToRecord(). */
		RelCatEntry relCatEntry = RelCacheTable::relCache[relId]->relCatEntry;
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);
		// Get the location of the Relation Catalog entry
    RecId recId = RelCacheTable::relCache[relId]->recId;

    // declaring an object of RecBuffer class to write back to the buffer
    RecBuffer relCatBlock(recId.block);

    // Write back to the buffer using relCatBlock.setRecord() with recId.slot
		int ret = relCatBlock.setRecord(relCatRecord, recId.slot);
    if (ret != SUCCESS)
    {
      return ret;
    }
  }

  /****** Releasing the Attribute Cache entry of the relation ******/

  // free the memory allocated in the attribute caches which was
  // allocated in the OpenRelTable::openRel() function
	AttrCacheEntry* current = AttrCacheTable::attrCache[relId];
  while (current != nullptr)
  {
    if (current->dirty)
    {
      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

      AttrCacheTable::attrCatEntryToRecord(&current->attrCatEntry, attrCatRecord);

      RecId recId = current->recId;
      RecBuffer attrCatBlock(recId.block);

      int ret = attrCatBlock.setRecord(attrCatRecord, recId.slot);
      if (ret != SUCCESS)
      {
        return ret;
      }
    }
    AttrCacheEntry* next = current->next;
    free(current);
    current = next;
  }
  
  free(RelCacheTable::relCache[relId]);
	
  /****** Set the Open Relation Table entry of the relation as free ******/
	tableMetaInfo[relId].free = true;
  // update `metainfo` to set `relId` as a free slot
	RelCacheTable::relCache[relId] = nullptr;
  AttrCacheTable::attrCache[relId] = nullptr;

  return SUCCESS;
}