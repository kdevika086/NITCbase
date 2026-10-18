#include "Algebra.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>

bool isNumber(char *str);



int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) 
{
  int srcRelId = OpenRelTable::getRelId(srcRel);    
  if (srcRelId == E_RELNOTOPEN) 
  {
    return E_RELNOTOPEN;
  }

  AttrCatEntry attrCatEntry; 
	int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
  if (ret != SUCCESS) 
	{
    return E_ATTRNOTEXIST;
  }

  /*** Convert strVal to an attribute of data type NUMBER or STRING ***/
  int type = attrCatEntry.attrType;
  Attribute attrVal;
  if (type == NUMBER) 
	{
    if (isNumber(strVal)) 
		{       
      attrVal.nVal = atof(strVal);
    } 
		else 
		{
      return E_ATTRTYPEMISMATCH;
    }
  } 
	else if (type == STRING) 
	{
    strcpy(attrVal.sVal, strVal);
  }

  /*** Creating and opening the target relation ***/
  RelCatEntry relCatEntry;
  ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int src_nAttrs = relCatEntry.numAttrs;
  char attr_names[src_nAttrs][ATTR_SIZE];
  int attr_types[src_nAttrs];
  for (int i = 0; i < src_nAttrs; i++)
  {
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
    if (ret != SUCCESS)
    {
      return ret;
    }
    strcpy(attr_names[i], attrCatEntry.attrName);
    attr_types[i] = attrCatEntry.attrType;
  }

  ret = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_types);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0)
  {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

   /*** Selecting and inserting records into the target relation ***/
  Attribute record[src_nAttrs];

  RelCacheTable::resetSearchIndex(srcRelId);
  AttrCacheTable::resetSearchIndex(srcRelId, attr);

  //for stage 10 modification
  comparisonCount = 0;

  while (BlockAccess::search(srcRelId, record, attr, attrVal, op) == SUCCESS)
  {
    ret = BlockAccess::insert(targetRelId, record);
    if (ret != SUCCESS)
    {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }
  std::cout << "Number of comparisons: " << comparisonCount << std::endl;

  Schema::closeRel(targetRel);
  return SUCCESS;
}



bool isNumber(char *str) 
{
  int len;
  float ignore;
  /*
    sscanf returns the number of elements read, so if there is no float matching
    the first %f, ret will be 0, else it'll be 1

    %n gets the number of characters read. this scanf sequence will read the
    first float ignoring all the whitespace before and after. and the number of
    characters read that far will be stored in len. if len == strlen(str), then
    the string only contains a float with/without whitespace. else, there's other
    characters.
  */
  int ret = sscanf(str, "%f %n", &ignore, &len);
  return ret == 1 && len == strlen(str);
}



int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE])
{
  if (strcmp(relName, "RELATIONCAT") == 0 || strcmp(relName, "ATTRIBUTECAT") == 0)
  {
    return E_NOTPERMITTED;
  }

  // get the relation's rel-id using OpenRelTable::getRelId() method
  int relId = OpenRelTable::getRelId(relName);

  // if relation is not open in open relation table, return E_RELNOTOPEN
  if (relId == E_RELNOTOPEN)
  {
    return E_RELNOTOPEN;
  }

  // get the relation catalog entry from relation cache
  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  /* if relCatEntry.numAttrs != numberOfAttributes in relation, return E_NATTRMISMATCH */
  if (relCatEntry.numAttrs != nAttrs)
  {
    return E_NATTRMISMATCH;
  }

  Attribute recordValues[nAttrs] = {};

  //Converting 2D char array of record values to Attribute array recordValues
  for (int i = 0; i < nAttrs; i++)
  {
    // get the attr-cat entry for the i'th attribute from the attr-cache
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
    if (ret != SUCCESS)
    {
      return ret;
    }

    int type = attrCatEntry.attrType;
    if (type == NUMBER)
    {
      if (isNumber(record[i]))
      {
        recordValues[i].nVal = atof(record[i]);
      }
      else
      {
        return E_ATTRTYPEMISMATCH;
      }
    }
    else if (type == STRING)
    {
      strcpy(recordValues[i].sVal, record[i]);
    }
  }

  int retVal = BlockAccess::insert(relId, recordValues);
  return retVal;
}




int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) 
{
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN)
  {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int numAttrs = relCatEntry.numAttrs;
  char attrNames[numAttrs][ATTR_SIZE];
  int attrTypes[numAttrs];

  for (int i = 0; i < numAttrs; i++)
  {
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
    if (ret != SUCCESS)
    {
      return ret;
    }

    strcpy(attrNames[i], attrCatEntry.attrName);
    attrTypes[i] = attrCatEntry.attrType;
  }


  /*** Creating and opening the target relation ***/
  ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
  if (ret != SUCCESS)
  {
    return ret;
  }
  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0)
  {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }


  /*** Inserting projected records into the target relation ***/
  RelCacheTable::resetSearchIndex(srcRelId);
  Attribute record[numAttrs];

  while (BlockAccess::project(srcRelId, record) == SUCCESS)
  {
    ret = BlockAccess::insert(targetRelId, record);
    if (ret != SUCCESS)
    {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  Schema::closeRel(targetRel);
  return SUCCESS;
}



int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) 
{
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if (srcRelId == E_RELNOTOPEN)
  {
    return E_RELNOTOPEN;
  }

  RelCatEntry relCatEntry;
  int ret = RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  if (ret != SUCCESS)
  {
    return ret;
  }

  int src_nAttrs = relCatEntry.numAttrs;
  int attr_offset[tar_nAttrs];
  int attr_types[tar_nAttrs];

  /*** Checking if attributes of target are present in the source relation ***/
  char attr_names[tar_nAttrs][ATTR_SIZE];
  for (int i = 0; i < tar_nAttrs; i++)
  {
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
    if (ret != SUCCESS)
    {
      return E_ATTRNOTEXIST;
    }
    attr_offset[i] = attrCatEntry.offset;
    attr_types[i] = attrCatEntry.attrType;
    strcpy(attr_names[i], attrCatEntry.attrName);
  }


  /*** Creating and opening the target relation ***/
  ret = Schema::createRel(targetRel, tar_nAttrs, attr_names, attr_types);
  if (ret != SUCCESS)
  {
    return ret;
  } 

  int targetRelId = OpenRelTable::openRel(targetRel);
  if (targetRelId < 0)
  {
    Schema::deleteRel(targetRel);
    return targetRelId;
  }

  /*** Inserting projected records into the target relation ***/
  RelCacheTable::resetSearchIndex(srcRelId);
  Attribute record[src_nAttrs];

  while 
  (BlockAccess::project(srcRelId, record) == SUCCESS) 
  {
    Attribute proj_record[tar_nAttrs];

    for (int attr_iter = 0; attr_iter < tar_nAttrs; attr_iter++)
    {
      proj_record[attr_iter] = record[attr_offset[attr_iter]];
    }

    ret = BlockAccess::insert(targetRelId, proj_record);

    if (ret != SUCCESS) 
    {
      Schema::closeRel(targetRel);
      Schema::deleteRel(targetRel);
      return ret;
    }
  }

  Schema::closeRel(targetRel);
   return SUCCESS;
}
