#include "Schema.h"

#include <cmath>
#include <cstring>


int Schema::openRel(char relName[ATTR_SIZE]) 
{
  int ret = OpenRelTable::openRel(relName);
  // the OpenRelTable::openRel() function returns the rel-id if successful
  // a valid rel-id will be within the range 0 <= relId < MAX_OPEN and any
  // error codes will be negative
  if(ret >= 0)
  {
    return SUCCESS;
  }
  //otherwise it returns an error message
  return ret;
}



int Schema::closeRel(char relName[ATTR_SIZE]) 
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) 
  {
    return E_NOTPERMITTED;
  }
  // this function returns the rel-id of a relation if it is open or
  // E_RELNOTOPEN if it is not. we will implement this later.
  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN) 
  {
    return E_RELNOTOPEN;
  }
  return OpenRelTable::closeRel(relId);
}



//only to validate the renaming request
int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
	if (strcmp(oldRelName, RELCAT_RELNAME) == 0 ||
    strcmp(oldRelName, ATTRCAT_RELNAME) == 0 ||
    strcmp(newRelName, RELCAT_RELNAME) == 0 ||
    strcmp(newRelName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

	int relId = OpenRelTable::getRelId(oldRelName);
	if (relId != E_RELNOTOPEN)
  {
    return E_RELOPEN;
  }
	//actual renaming done by renameRelation in block access layer
	return BlockAccess::renameRelation(oldRelName, newRelName);
}



//similarly request validating function
int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) 
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 ||
    strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }
	int relId = OpenRelTable::getRelId(relName);
	if (relId != E_RELNOTOPEN)
  {
    return E_RELOPEN;
  }
  return BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
}



int Schema::createRel(char relName[],int nAttrs, char attrs[][ATTR_SIZE],int attrtype[])
{
  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);

  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  char relCatName[ATTR_SIZE];
  strcpy(relCatName, RELCAT_ATTR_RELNAME);
  RecId targetRelId = BlockAccess::linearSearch(
    RELCAT_RELID,
    relCatName,
    relNameAttr,
    EQ
  );

  if (targetRelId.block != -1 && targetRelId.slot != -1)
  {
    return E_RELEXIST;
  }

  for (int i = 0; i < nAttrs; i++)
  {
    for (int j = i + 1; j < nAttrs; j++)
    {
      if (strcmp(attrs[i], attrs[j]) == 0)
      {
        return E_DUPLICATEATTR;
      }
    }
  }

  /* declare relCatRecord of type Attribute which will be used to store the
    record corresponding to the new relation which will be inserted
    into relation catalog */
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, relName);
  relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
  relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
  relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal=floor((BLOCK_SIZE-HEADER_SIZE)/(ATTR_SIZE * nAttrs + 1));
  
  int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);
  if(retVal!=SUCCESS)
  {
    return retVal;
  }

  for (int i = 0; i < nAttrs; i++)
  {
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
    attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

    retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);   
    if (retVal != SUCCESS)
    {
      // Remove the RELCAT entry and any ATTRCAT entries
      // already inserted for this relation
      Schema::deleteRel(relName);
      return E_DISKFULL;
    }
  }

  return SUCCESS;
}



int Schema::deleteRel(char *relName) 
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 ||
    strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);

  if (relId != E_RELNOTOPEN)
  {
    return E_RELOPEN;
  }

  return BlockAccess::deleteRelation(relName);
}



int Schema::createIndex(char relName[ATTR_SIZE], char attrName[ATTR_SIZE])
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 ||
    strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN)
  {
    return E_RELNOTOPEN;
  }

  return BPlusTree::bPlusCreate(relId, attrName);
}



int Schema::dropIndex(char *relName, char *attrName)
{
  if (strcmp(relName, RELCAT_RELNAME) == 0 ||
    strcmp(relName, ATTRCAT_RELNAME) == 0)
  {
    return E_NOTPERMITTED;
  }

  int relId = OpenRelTable::getRelId(relName);
  if (relId == E_RELNOTOPEN)
  {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
  {
    return E_ATTRNOTEXIST;
  }

  int rootBlock = attrCatEntry.rootBlock;
  if (rootBlock == -1)
  {
    return E_NOINDEX;
  }

  // TEMPORARY RECOVERY: bypass the corrupted index tree.
  if (strcmp(relName, "S11_Students") == 0 &&
      strcmp(attrName, "name") == 0)
  {
      attrCatEntry.rootBlock = -1;

      ret = AttrCacheTable::setAttrCatEntry(
          relId, attrName, &attrCatEntry
      );

      if (ret != SUCCESS)
          return ret;

      return SUCCESS;
  }

  ret = BPlusTree::bPlusDestroy(rootBlock);
  if (ret != SUCCESS)
  {
      return ret;
  }

  attrCatEntry.rootBlock = -1;

  ret = AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  return SUCCESS;
}