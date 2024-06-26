/*
* Copyright (C) 2015 German Aerospace Center (DLR/SC)
*
* Created: 2010-08-13 Markus Litz <Markus.Litz@dlr.de>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "tixi.h"
#include "tixiData.h"
#include "tixiInternal.h"
#include "xpathFunctions.h"
#include "uidHelper.h"
#include "tixiUtils.h"
#include "webMethods.h"
#include "namespaceFunctions.h"
#include "libxml/xmlschemas.h"

/**
   @file Auxiliary routines used to implement the interface.
*/


#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif


extern void printMsg(MessageType type, const char* message, ...);

void xmlStructuredErrorHandler(void * userData, xmlError const* error) {
  printMsg(MESSAGETYPE_ERROR, "%s:%i: %s", error->file, error->line, error->message);
}


InternalReturnCode clearMemoryList(TixiDocument* document)
{

  TixiMemoryListEntry* current = document->memoryListHead;

  while (current) {

    TixiMemoryListEntry* next = (TixiMemoryListEntry*) current->next;
    free(current->memory);
    free(current);
    current = next;
  }
  document->memoryListHead = NULL;
  document->memoryListTail = NULL;
  return SUCCESS;
}

void freeTixiDocument(TixiDocument* document)
{
  if (document->xmlFilename) {
    free(document->xmlFilename);
    document->xmlFilename = NULL;
  }

  if (document->validationFilename) {
    free(document->validationFilename);
    document->validationFilename = NULL;
  }

  if (document->dirname) {
    free(document->dirname);
    document->dirname = NULL;
  }

  if (document->filename) {
    free(document->filename);
    document->filename = NULL;
  }
  clearMemoryList(document);
  uid_clearUIDList(document);

  if (document->xpathContext) {
      xmlXPathFreeContext(document->xpathContext);
      document->xpathContext = NULL;
  }

  if (document->xpathCache) {
      XPathFreeCache(document->xpathCache);
      document->xpathCache = NULL;
  }
  xmlFreeDoc(document->docPtr);

  free(document);
}

InternalReturnCode addDocumentToList(TixiDocument* document, TixiDocumentHandle* handle)
{

  TixiDocumentListEntry* currentEntry = documentListHead;
  static int handleCounter = 0;

  if (documentListHead) {

    while (currentEntry->next) {
      currentEntry = (TixiDocumentListEntry*) currentEntry->next;
    }

    currentEntry->next = (struct TixiDocumentListEntry*)
        malloc(sizeof(TixiDocumentListEntry));

    if (!currentEntry->next) {

      return MEMORY_ALLOCATION_FAILED;
    }

    currentEntry = (TixiDocumentListEntry*) currentEntry->next;
  }
  else {

    currentEntry = (TixiDocumentListEntry*) malloc(sizeof(TixiDocumentListEntry));

    if (currentEntry) {
      documentListHead = currentEntry;
    }
    else {
      return MEMORY_ALLOCATION_FAILED;
    }
  }

  currentEntry->document = document;
  currentEntry->next = NULL;

  handleCounter++;
  *handle = handleCounter;

  return SUCCESS;
}

ReturnCode removeDocumentFromList(TixiDocumentHandle handle)
{

  TixiDocumentListEntry* currentEntry = documentListHead;
  TixiDocumentListEntry* previousEntry = NULL;

  ReturnCode returnValue = SUCCESS;

  if (documentListHead) {

    while (currentEntry) {

      if (currentEntry->document->handle != handle) {
        previousEntry = currentEntry;
        currentEntry = (TixiDocumentListEntry*) currentEntry->next;
      }
      else {
        if (previousEntry) {
          previousEntry->next = currentEntry->next;
          free(currentEntry);
          currentEntry = NULL;
          returnValue = SUCCESS;
        }
        else {
          documentListHead = (TixiDocumentListEntry*) currentEntry->next;
          free(currentEntry);
          currentEntry = NULL;
          returnValue = SUCCESS;
        }
      }
    }
  }
  else {
    returnValue = FAILED;
  }

  return returnValue;
}

TixiDocument* getDocument(TixiDocumentHandle handle)
{

  TixiDocumentListEntry* currentEntry = documentListHead;
  TixiDocument* returnValue = NULL;

  if (currentEntry) {
    while (currentEntry && returnValue == NULL) {

      if (currentEntry->document->handle != handle) {
        currentEntry = (TixiDocumentListEntry*) currentEntry->next;
      }
      else {
        returnValue = currentEntry->document;
      }
    }
  }
  else {
    returnValue = NULL;
  }

  return returnValue;
}

InternalReturnCode addToMemoryList(TixiDocument* document, void* memory)
{

  TixiMemoryListEntry* currentEntry = document->memoryListTail;

  if (currentEntry) {

    currentEntry->next = (struct TixiMemoryListEntry*) malloc(sizeof(TixiMemoryListEntry));

    if (currentEntry->next) {
      currentEntry = (TixiMemoryListEntry*) currentEntry->next;
      document->memoryListTail = currentEntry;
    }
    else {
      return MEMORY_ALLOCATION_FAILED;
    }
  }
  else {

    currentEntry = (TixiMemoryListEntry*) malloc(sizeof(TixiMemoryListEntry));

    if (currentEntry) {
      document->memoryListHead = currentEntry;
      document->memoryListTail = currentEntry;
    }
    else {
      return MEMORY_ALLOCATION_FAILED;
    }
  }

  currentEntry->memory = memory;
  currentEntry->next = NULL;

  return SUCCESS;
}

ReturnCode checkExistence(const xmlXPathContextPtr xpathContext, const char* elementPath, xmlXPathObjectPtr* xpathObject)
{

  *xpathObject = NULL;

  *xpathObject = xmlXPathEvalExpression((const xmlChar*) elementPath, xpathContext);
  if (!(*xpathObject)) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid XPath expression \"%s\"\n", elementPath);
    //xmlXPathFreeObject(*xpathObject);
    *xpathObject = NULL;
    return INVALID_XPATH;
  }

  if (xmlXPathNodeSetIsEmpty((*xpathObject)->nodesetval)) {
    xmlXPathFreeObject(*xpathObject);
    *xpathObject = NULL;
    return ELEMENT_NOT_FOUND;
  }

  return SUCCESS;
}



ReturnCode checkExternalNode(const xmlNodePtr element)
{
  ReturnCode error = FAILED;
  xmlChar* xmlAttribute = NULL;

  /* check attribute externalDataNodePath */
  xmlAttribute = xmlGetProp(element, (const xmlChar*) EXTERNAL_DATA_XML_ATTR_NODEPATH);
  if (xmlAttribute != NULL) {
    error = SUCCESS;
  }
  else {
    return FAILED;
  }
  xmlFree(xmlAttribute);

  /* check attribute externalFileName */
  xmlAttribute = xmlGetProp(element, (const xmlChar*) EXTERNAL_DATA_XML_ATTR_FILENAME);
  if (xmlAttribute != NULL) {
    error = SUCCESS;
  }
  else {
    return FAILED;
  }
  xmlFree(xmlAttribute);

  /* check attribute externalDataDirectory */
  xmlAttribute = xmlGetProp(element, (const xmlChar*) EXTERNAL_DATA_XML_ATTR_DIRECTORY);
  if (xmlAttribute != NULL) {
    error = SUCCESS;
  }
  else {
    return FAILED;
  }
  xmlFree(xmlAttribute);
  return error;
}


ReturnCode checkElement(const xmlXPathContextPtr xpathContext, const char* elementPathDirty,
                        xmlNodePtr* element)
{

  xmlNodeSetPtr nodes = NULL;
  char elementPath[1024];

  xmlXPathObjectPtr xpathObject = NULL;

  /* remove trailing slash */
  strncpy(elementPath, elementPathDirty, 1024);
  if(strlen(elementPath) > 1 && elementPath[strlen(elementPath)-1] == '/') {
    elementPath[strlen(elementPath)-1] = '\0';
  }

  /* Evaluate Expression */
  xpathObject = xmlXPathEvalExpression((xmlChar*) elementPath, xpathContext);
  if (!xpathObject) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid XPath expression \"%s\"\n", elementPath);
    return INVALID_XPATH;
  }

  if (xmlXPathNodeSetIsEmpty(xpathObject->nodesetval)) {
    char * errorStr = buildString("Error: element %s not found!", elementPath);
    xmlXPathFreeObject(xpathObject);

    printMsg(MESSAGETYPE_STATUS, errorStr);
    free(errorStr);
    return ELEMENT_NOT_FOUND;
  }

  nodes = xpathObject->nodesetval;
  assert(nodes);

  if (nodes->nodeNr > 1) {
    printMsg(MESSAGETYPE_ERROR,
             "Error: Element chosen by XPath \"%s\" expression is not unique. \n", elementPath);
    xmlXPathFreeObject(xpathObject);
    return ELEMENT_PATH_NOT_UNIQUE;
  }

  assert(nodes->nodeTab[0]);

  if (nodes->nodeTab[0]->type == XML_ELEMENT_NODE ||
      nodes->nodeTab[0]->type == XML_DOCUMENT_NODE ||
      nodes->nodeTab[0]->type == XML_TEXT_NODE ) {
    *element = nodes->nodeTab[0];
    xmlXPathFreeObject(xpathObject);
    return SUCCESS;
  }
  else {
    printMsg(MESSAGETYPE_ERROR,
             "Error: XPath expression \"%s\"does not point to an element node.\n", elementPath);
    xmlXPathFreeObject(xpathObject);
    return NOT_AN_ELEMENT;
  }
}


ReturnCode getCoordinateValue(TixiDocument* document, char* pointPath,
                              int pointIndex, char* name, int ignoreError, double* value)
{
  xmlNodePtr coordinate = NULL;
  ReturnCode error = -1;
  size_t suffixStringLength = 0;
  size_t pointPathLength = strlen(pointPath);
  size_t coordinatePathStringLength = 0;
  char* coordinatePath = NULL;
  char* suffixString = buildString("[%d]/%1s", pointIndex, name);

  if (!suffixString) {
    printMsg(MESSAGETYPE_ERROR, "Internal Error: Failed to allocate memory in getCoordinateValue.\n");
    return FAILED;
  }

  suffixStringLength = strlen(suffixString);

  coordinatePathStringLength = suffixStringLength + pointPathLength + 1;

  coordinatePath = (char*) malloc(coordinatePathStringLength * sizeof(char));
  strcpy(coordinatePath, pointPath);
  strcat(coordinatePath, suffixString);

  error = checkElement(document->xpathContext, coordinatePath, &coordinate);

  free(coordinatePath);
  free(suffixString);
  coordinatePath = NULL;
  suffixString = NULL;
  if (!error) {

    char* textPtr = (char*) xmlNodeListGetString(document->docPtr, coordinate->children, 0);

    if (textPtr) {
      *value = atof(textPtr);
      xmlFree(textPtr);
    }
    else {
      error = FAILED;
    }
  }
  else {
    if (!ignoreError) {
      printMsg(MESSAGETYPE_ERROR,
               "Internal Error: point element \"%s\" has no %s-coordinate.\n", pointPath, name);
    }
    error = COORDINATE_NOT_FOUND;
  }

  return error;
}

ReturnCode getPoint(const TixiDocumentHandle handle, const char* parentPath, const int pointIndex,
                    int ignoreMissingElements, double* x, double* y, double* z)
{
  ReturnCode error = -1;
  ReturnCode errorX = -1;
  ReturnCode errorY = -1;
  ReturnCode errorZ = -1;

  TixiDocument* document = getDocument(handle);
  xmlNodePtr parent = NULL;
  char* pointPath = NULL;
  char* coordinatePathBuffer = NULL;
  int nPointElements = 0;

  if (!document) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid document handle.\n");
    return INVALID_HANDLE;
  }

  if (pointIndex < 1) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid point index %d\n", pointIndex);
    return INDEX_OUT_OF_RANGE;
  }

  error = checkElement(document->xpathContext, parentPath, &parent);

  if (!error) {

    xmlXPathObjectPtr xpathPointObject = NULL;

    size_t lengthPointPath = (strlen(parentPath) +      /* length of path to parent   */
                              1);      				    /* trailing \0                */

    pointPath = (char*) malloc(sizeof(char) * lengthPointPath);

    strcpy(pointPath, parentPath);

    /* pad with trailing blanks, so we can check for the size in getCoordinateValue */

    error = checkExistence(document->xpathContext, pointPath, &xpathPointObject);

    if (xpathPointObject) {
      nPointElements = xpathPointObject->nodesetval->nodeNr;
      xmlXPathFreeObject(xpathPointObject);
    }

    if (error == ELEMENT_NOT_FOUND) {
      printMsg(MESSAGETYPE_ERROR, "Error: No point element found in element \"%s\".\n", parentPath);
      error = NO_POINT_FOUND;
    }
    else if (!error) {
      if (pointIndex > nPointElements) {
        printMsg(MESSAGETYPE_ERROR, "Error: Index (%d) larger than number of point elements.\n", pointIndex);
        error = INDEX_OUT_OF_RANGE;
      }
      else {
        errorX = getCoordinateValue(document, pointPath, pointIndex, "x", ignoreMissingElements, x);
        errorY = getCoordinateValue(document, pointPath, pointIndex, "y", ignoreMissingElements, y);
        errorZ = getCoordinateValue(document, pointPath, pointIndex, "z", ignoreMissingElements, z);

        if( errorX && errorY && errorZ) {
          error = NO_POINT_FOUND;
        }
        else {
          error = SUCCESS;
        }
      }

    }
  }

  free(coordinatePathBuffer);
  free(pointPath);
  return error;
}



void checkLibxml2Version()
{

  static int testPerformed = 0;

  if (!testPerformed) {
    LIBXML_TEST_VERSION;
  }

}


char* buildString(const char* format, ...)
{

  int bufferLength = 10;        /* initial guess for the buffer length */
  int nChars = -1;
  va_list variableList;
  char* buffer = NULL;

  buffer = (char*) malloc(bufferLength * sizeof(char));

  if (buffer) {

    while (nChars < 0) {

      /* try to print into buffer */

      va_start(variableList, format);
      nChars = VSNPRINTF(buffer, bufferLength, format, variableList);
      va_end(variableList);

      if (nChars < bufferLength && nChars > -1) {
        break;
      }

      /* msvc and glibc < 2.1: VSNPRINTF returns a negative integer on failure */
      if (nChars < 0) {

        bufferLength *= 2;
        //PRINT_DEBUG2("nChar = %d buffer length = %d, reallocating\n", nChars, bufferLength);
      }
      /*
               glibc 2.0: VSNPRINTF returns the length ( numbers of chars) of the result
               without the trailing \0 if the length would not exceed bufferLength.
            */
      else {

        bufferLength = nChars + 1;
        //PRINT_DEBUG2("nChar = %d buffer length = %d, reallocating\n", nChars, bufferLength);
      }

      nChars = -1;
      buffer = (char*) realloc(buffer, bufferLength * sizeof(char));

      if (!buffer) {
        break;
      }
    }
  }
  return buffer;
}

char* vectorToString(const double* vector, int numElements, const char* format)
{
    int i = 0, stringSize = 0, delim_size = strlen(VECTOR_SEPARATOR);
    // the output string
    char* stringVector = NULL;

    /* calculate the size of the resulting string */
    for (i=0; i<numElements; i++) {
        // according to C99 Standard, n=0 allows a null pointer
        // effectively just counting the written bytes this way
        stringSize += snprintf(NULL, 0, format, vector[i]) + delim_size;
    }

    /* allocate memory */
    stringVector = (char *) malloc(sizeof(char) * (stringSize + 1));

    /* copy strings to stringVector */
    stringVector[0] = '\0';

    if (numElements > 0) {
        char* formatExt = buildString("%s%s", VECTOR_SEPARATOR, format);

        stringSize = sprintf(stringVector, format , vector[0]);
        for (i=1; i < numElements; i++) {

          // by using the pointer stringVector + stringSize, we can avoid
          // computing the string length as done by strcat
          stringSize += sprintf(stringVector + stringSize, formatExt, vector[i]);
        }
        free(formatExt);
    }

    return stringVector;
}

char* loadExternalFileToString(const char* filename)
{
  if (isURIPath(filename) != 0) {
    // local file
    return loadFileToString(filename);
  }
  else if (string_startsWith(filename, "file://") == 0) {
    char* localPath = uriToLocalPath(filename);
    char* result = NULL;
    if (!localPath) {
      return NULL;
    }
    result = loadFileToString(localPath);

    free(localPath);
    return result;
  }
  else {
    return curlGetURLInMemory(filename);
  }
}


ReturnCode loadExternalDataNode(TixiDocument* aTixiDocument, xmlNodePtr externalDataNode, int* fileCounter)
{
    int handle = aTixiDocument->handle;
    char* externalDataNodeXPath, *externalDataDirectoryXPath, *externalDataDirectory, *resolvedDirectory;
    int externalFileCount = 0;
    ReturnCode error = SUCCESS;
    int iNode = 0;

    /* get nodes XPath */
    externalDataNodeXPath = (char*) xmlGetNodePath(externalDataNode);


    /* now get the subdirectory */
    externalDataDirectoryXPath = buildString("%s/%s", externalDataNodeXPath, EXTERNAL_DATA_NODE_NAME_PATH);

    error = tixiGetTextElement(handle, externalDataDirectoryXPath, &externalDataDirectory);
    free(externalDataDirectoryXPath);
    if (error) {
      printMsg(MESSAGETYPE_ERROR, "Error: openExternalFiles returns %d. No path defined in externaldata node!\n", error);
      xmlFree(externalDataNodeXPath);
      return OPEN_FAILED;
    }

    // resolv data directory (in case of relative paths)
    resolvedDirectory = resolveDirectory(aTixiDocument->dirname, externalDataDirectory);

    /* now get number and names of all external files */
    tixiGetNamedChildrenCount(handle, externalDataNodeXPath, EXTERNAL_DATA_NODE_NAME_FILENAME, &externalFileCount);
    if (externalFileCount == 0) {
      printMsg(MESSAGETYPE_ERROR, "Error: no filename nodes defined in externalData node.\n");
      xmlFree(externalDataNodeXPath);
      free(resolvedDirectory);
      return OPEN_FAILED;
    }

    for (iNode = 1; iNode <= externalFileCount; iNode++) {
      char* externalFileName, *externalFullFileName, *newDocumentString, *fileNameXPath;
      xmlDocPtr xmlDocument = NULL;

      fileNameXPath = buildString("%s/filename[%d]", externalDataNodeXPath, iNode);

      tixiGetTextElement(handle, fileNameXPath, &externalFileName);
      free(fileNameXPath);

      /* Build complete filename */
      externalFullFileName = buildString("%s%s", resolvedDirectory, externalFileName);

      /* open files */
      newDocumentString = loadExternalFileToString(externalFullFileName);
      if (newDocumentString == NULL) {
        printMsg(MESSAGETYPE_ERROR, "\nError in fetching external file \"%s\".\n", externalFullFileName);
        free(externalFullFileName);
        xmlFree(externalDataNodeXPath);
        free(resolvedDirectory);
        return OPEN_FAILED;
      }

      /* now parse the file to DOM */
      xmlDocument = xmlReadMemory(newDocumentString, (int) strlen(newDocumentString), "urlResource", NULL, 0);
      free(newDocumentString);

      if (xmlDocument) {
        xmlNodePtr rootToInsert = xmlDocGetRootElement(xmlDocument);

        xmlNodePtr parent = externalDataNode->parent;
        if (parent) {
          xmlChar* nodePathNew = NULL;
          char* dataURI = localPathToURI(externalDataDirectory);
          xmlNodePtr nodeToInsert = xmlDocCopyNode(rootToInsert, aTixiDocument->docPtr, 1);

          /* add metadata to node, to allow saving external node data */
          xmlSetProp(nodeToInsert, (xmlChar*) EXTERNAL_DATA_XML_ATTR_FILENAME, (xmlChar*) externalFileName);

          /* save the sub-directory */
          xmlSetProp(nodeToInsert, (xmlChar*) EXTERNAL_DATA_XML_ATTR_DIRECTORY, (xmlChar*) dataURI);
          free(dataURI);

          /* save the external data node position */
          nodePathNew = xmlGetNodePath(parent);
          xmlSetProp(nodeToInsert, (xmlChar*) EXTERNAL_DATA_XML_ATTR_NODEPATH, nodePathNew);
          xmlFree(nodePathNew);

          /* replace externalData node with xml file's content */
          xmlReplaceNode(externalDataNode, nodeToInsert);

          /* file could be loaded and parsed, increase the counter */
          (*fileCounter)++;
        }

        xmlFreeDoc(xmlDocument);
      }
      else {
        printMsg(MESSAGETYPE_WARNING,
                 "Document %s will be ignored. No valid XML document!\n",
                 externalFullFileName);

        /* remove external data node */
        xmlUnlinkNode(externalDataNode);
      }
      free(externalFullFileName);
    } /* end for files */

    free(resolvedDirectory);
    free(externalDataNodeXPath);
    xmlFreeNode(externalDataNode);

    return SUCCESS;

}

ReturnCode openExternalFiles(TixiDocument* aTixiDocument, int* number)
{
  int iNode = 0;

  assert(aTixiDocument != NULL);
  *number = 0;

  while(1) {
    // loop until there are no externaldata nodes included

    xmlXPathObjectPtr xpathObject = XPathEvaluateExpression(aTixiDocument, "//externaldata");
    xmlNodeSetPtr nodeset = NULL;
    xmlNodePtr cur = NULL;
    ReturnCode errCode = SUCCESS;

    if (!xpathObject) {
      // no more external data, stop
      break;
    }

    nodeset = xpathObject->nodesetval;
    if (!nodeset || nodeset->nodeNr < 1) {
      break;
    }

    // goto the first node that is an element
    for (iNode = 0; iNode < nodeset->nodeNr; ++iNode) {
      cur = nodeset->nodeTab[iNode];
      if (cur->type == XML_ELEMENT_NODE) {
        break; // for loop
      }
    }
    if (iNode == nodeset->nodeNr) {
      // no element node found
      XPathClearCache(aTixiDocument->xpathCache);
      break; // while loop
    }

    XPathClearCache(aTixiDocument->xpathCache);

    errCode = loadExternalDataNode(aTixiDocument, cur, number);
    if (errCode != SUCCESS) {
        return errCode;
    }
  }

  return SUCCESS;
}


void removeExternalNodeLinks(xmlNodePtr aNodePtr)
{
    xmlNodePtr cur_node = NULL;

    /* find external data nodes */
    for (cur_node = aNodePtr; cur_node; cur_node = cur_node->next) {

      /* recurse down with the next element */
      removeExternalNodeLinks(cur_node->children);

      if( checkExternalNode( cur_node ) == SUCCESS) {
          /* remove attributes */
          xmlUnsetProp(cur_node, (xmlChar*) EXTERNAL_DATA_XML_ATTR_FILENAME);
          xmlUnsetProp(cur_node, (xmlChar*) EXTERNAL_DATA_XML_ATTR_DIRECTORY);
          xmlUnsetProp(cur_node, (xmlChar*) EXTERNAL_DATA_XML_ATTR_NODEPATH);
      }
    }
}


xmlNodePtr createExternalNode(const char* urlPath, const char* filename)
{
    xmlNodePtr externalNode = xmlNewNode(NULL, (xmlChar*) EXTERNAL_DATA_NODE_NAME);
    xmlNodePtr pathNode = xmlNewNode(NULL, (xmlChar*) EXTERNAL_DATA_NODE_NAME_PATH);
    xmlNodePtr filenameNode = xmlNewNode(NULL, (xmlChar*) EXTERNAL_DATA_NODE_NAME_FILENAME);

    xmlAddChild( externalNode, pathNode );
    xmlAddChild( externalNode, filenameNode );

    if (urlPath) {
        xmlNodePtr pathNodeText = xmlNewText( (xmlChar *) urlPath );
        xmlAddChild( pathNode, pathNodeText );
    }

    if (filename) {
        xmlNodePtr filenameNodeText = xmlNewText( (xmlChar *) filename );
        xmlAddChild( filenameNode, filenameNodeText );
    }

    return externalNode;
}

ReturnCode saveExternalFiles(xmlNodePtr aNodePtr, TixiDocument* aTixiDocument)
{
  TixiDocumentHandle handle = aTixiDocument->handle;
  xmlNodePtr cur_node = NULL;
  xmlNodePtr copiedNode = NULL;
  xmlNodePtr externalNode = NULL;
  char* externalDataDirectory = NULL;
  char* externalFileName = NULL;
  char* fullExternalFileName = NULL;
  char* externalDataNodePath = NULL;
  xmlDocPtr xmlDocument = NULL;

  /* find external data nodes */
  for (cur_node = aNodePtr; cur_node; cur_node = cur_node->next) {

    /* recurse down with the next element */
    saveExternalFiles(cur_node->children, aTixiDocument);

    if( checkExternalNode( cur_node ) != SUCCESS) {
      continue;
    }

    if ( cur_node->type == XML_ELEMENT_NODE ) {
      char* dirResolved = NULL;
      char* includetNodeName = (char*) xmlGetNodePath(cur_node);

      /* collect node information - externalFileName */
      tixiGetTextAttribute(handle, includetNodeName,
                           EXTERNAL_DATA_XML_ATTR_FILENAME, &externalFileName);

      /* collect node information - externalDataDirectory */
      tixiGetTextAttribute(handle, includetNodeName,
                           EXTERNAL_DATA_XML_ATTR_DIRECTORY, &externalDataDirectory);

      /* collect node information - externalDataNodePath */
      tixiGetTextAttribute(handle, includetNodeName,
                           EXTERNAL_DATA_XML_ATTR_NODEPATH, &externalDataNodePath);

      free(includetNodeName);


      /* remove attributes */
      xmlUnsetProp(cur_node, (xmlChar*) EXTERNAL_DATA_XML_ATTR_FILENAME);
      xmlUnsetProp(cur_node, (xmlChar*) EXTERNAL_DATA_XML_ATTR_DIRECTORY);
      xmlUnsetProp(cur_node, (xmlChar*) EXTERNAL_DATA_XML_ATTR_NODEPATH);


      /* create new document */
      xmlDocument = xmlNewDoc((xmlChar*) "1.0");
      if (!xmlDocument) {
        printMsg(MESSAGETYPE_ERROR, "Error in TIXI::saveExternalFiles ==> Could not create new document.\n");
        return FAILED;
      }

      /* deep copy of nodes from external files */
      copiedNode = xmlDocCopyNode(cur_node, xmlDocument, 1);

      xmlDocSetRootElement(xmlDocument, copiedNode);

      dirResolved = resolveDirectory(aTixiDocument->dirname, externalDataDirectory);

      /* only save to local paths */
      if(string_startsWith(dirResolved, "file://") == 0) {
        char* externalDataDirectoryNotUrl = uriToLocalPath(dirResolved);
        assert(externalDataDirectoryNotUrl);

        fullExternalFileName = buildString("%s%s", externalDataDirectoryNotUrl, externalFileName);
        xmlSaveFormatFileEnc(fullExternalFileName, xmlDocument, "utf-8", 1);
        free(fullExternalFileName);
        free(externalDataDirectoryNotUrl);
      }
      free(dirResolved);
      xmlFreeDoc(xmlDocument);

      /* create external data node structure */
      externalNode = createExternalNode(externalDataDirectory, externalFileName);
      xmlReplaceNode(cur_node, externalNode);

      /* remove old node */
      xmlFreeNode(cur_node);

      cur_node = externalNode;

    }
  }
  return SUCCESS;
}

void removeExternalNodes(xmlNodePtr aNodePtr, TixiDocument* aTixiDocument)
{
  xmlNodePtr cur_node = NULL;

  /* find external data nodes */
  cur_node = aNodePtr;
  while (cur_node) {
    /* recurse down with the next element */
    removeExternalNodes(cur_node->children, aTixiDocument);

    if ( checkExternalNode( cur_node ) == SUCCESS && cur_node->type == XML_ELEMENT_NODE ) {
      xmlNodePtr next = cur_node->next;
      xmlUnlinkNode(cur_node);
      xmlFreeNode(cur_node);
      cur_node = next;
    }
    else {
      cur_node = cur_node->next;
    }
  }
}


xmlNodePtr getParentNodeToXPath(TixiDocumentHandle handle, const char* elementPath)
{

  TixiDocument* document = getDocument(handle);
  xmlXPathObjectPtr xpathObject = NULL;
  xmlNodeSetPtr nodes = NULL;
  xmlNodePtr parent = NULL;


  if (!document) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid document handle.\n");
    return parent;
  }

  xpathObject = xmlXPathEvalExpression((xmlChar*) elementPath, document->xpathContext);

  if (!xpathObject) {
    printMsg(MESSAGETYPE_ERROR, "Error: unable to evaluate xpath expression \"%s\"\n", elementPath);
    return parent;
  }

  if (xmlXPathNodeSetIsEmpty(xpathObject->nodesetval)) {
    printMsg(MESSAGETYPE_ERROR, "Error: No element found at XPath expression \"%s\"\n", elementPath);
    xmlXPathFreeObject(xpathObject);
    return parent;
  }

  nodes = xpathObject->nodesetval;

  assert(nodes);

  if (nodes->nodeNr > 1) {
    printMsg(MESSAGETYPE_ERROR,
             "Error: Element chosen by XPath \"%s\" expression is not unique. \n", elementPath);
    xmlXPathFreeObject(xpathObject);
    return parent;
  }

  parent = nodes->nodeTab[0]->parent;

  xmlXPathFreeObject(xpathObject);
  return parent;
}


ReturnCode genericAddTextAttribute(xmlXPathContextPtr xpathContext, const char* elementPath,
                                   const char* attributeName, const char* attributeValue)
{
  ReturnCode error = -1;
  xmlNodePtr parent = NULL;
  xmlAttrPtr attributePtr = NULL;

  if (!attributeName) {
    printMsg(MESSAGETYPE_ERROR, "Error:  No attribute name specified.\n");
    return NO_ATTRIBUTE_NAME;
  }

  if (!xmlValidateNameValue((xmlChar*) attributeName)) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid element name \"%s\"\n", attributeName);
    return INVALID_XML_NAME;
  }

  error = checkElement(xpathContext, elementPath, &parent);
  if (!error) {
    char* prefix = NULL;
    char* name = NULL;
    ReturnCode errorCode = FAILED;

    extractPrefixAndName(attributeName, &prefix, &name);

    if (!prefix) {
      attributePtr = xmlSetProp(parent, (xmlChar*) attributeName, (xmlChar*) attributeValue);
    }
    else {
      xmlNsPtr ns = xmlSearchNs(parent->doc, parent, (xmlChar *) prefix);
      if (!ns) {
        printMsg(MESSAGETYPE_ERROR, "Error: unknown namespace prefix \"%s\".\n",
                 prefix);
        attributePtr = NULL;
        errorCode = INVALID_NAMESPACE_PREFIX;
      }
      else {
        attributePtr = xmlSetNsProp(parent, ns, (xmlChar*) name, (xmlChar*) attributeValue);
      }

      free(prefix);
    }
    free(name);

    if (!attributePtr) {
      printMsg(MESSAGETYPE_ERROR,
               "Error: Failed to add attribute \"%s\" to element \"%s\".\n",
               attributeName, elementPath);
      return errorCode;
    }
    return SUCCESS;
  }
  else {
    return error;
  }
}



ReturnCode getNodePtrFromElementPath(TixiDocumentHandle handle, const char* elementPath, xmlNodePtr* pNodePrt)
{
  TixiDocument* document = getDocument(handle);
  xmlXPathObjectPtr xpathObject = NULL;
  xmlNodeSetPtr nodes = NULL;

  if (!document) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid document handle.\n");
    return INVALID_HANDLE;
  }

  xpathObject = xmlXPathEvalExpression((xmlChar*) elementPath, document->xpathContext);

  if (!xpathObject) {
    printMsg(MESSAGETYPE_ERROR, "Error: unable to evaluate xpath expression \"%s\"\n", elementPath);
    return INVALID_XPATH;
  }

  if (xmlXPathNodeSetIsEmpty(xpathObject->nodesetval)) {
    printMsg(MESSAGETYPE_ERROR, "Error: No element found at XPath expression \"%s\"\n", elementPath);
    xmlXPathFreeObject(xpathObject);
    return ELEMENT_NOT_FOUND;
  }

  nodes = xpathObject->nodesetval;

  assert(nodes);

  if (nodes->nodeNr > 1) {
    printMsg(MESSAGETYPE_ERROR,
             "Error: Element chosen by XPath \"%s\" expression is not unique. \n", elementPath);
    xmlXPathFreeObject(xpathObject);
    return ELEMENT_PATH_NOT_UNIQUE;
  }

  *pNodePrt = nodes->nodeTab[0];
  xmlXPathFreeObject(xpathObject);

  return SUCCESS;
}


int copyDocument(const TixiDocumentHandle oldTixiDocumentHandle, TixiDocumentHandle* newTixiDocumentHandle)
{
  TixiDocument* srcDocument = getDocument(oldTixiDocumentHandle);
  TixiDocument* dstDocument = NULL;
  xmlDocPtr xmlDocument = NULL;
  xmlNodePtr rootNode = NULL;

  if (!srcDocument) {
    printMsg(MESSAGETYPE_ERROR, "Error in TIXI::copyDocument => Invalid document handle.\n");
    return FAILED;
  }

  /* make a deep copy of the document */
  xmlDocument = xmlCopyDoc(srcDocument->docPtr, 1);

  if (!xmlDocument) {
    printMsg(MESSAGETYPE_ERROR, "Error in TIXI::copyDocument => Could not copy document.\n");
    return FAILED;
  }

  rootNode = xmlDocGetRootElement(xmlDocument);
  if (!rootNode) {
    printMsg(MESSAGETYPE_ERROR, "Error in TIXI::copyDocument => Could not get root node in source document.\n");
    return EROROR_CREATE_ROOT_NODE;
  }

  dstDocument = (TixiDocument*) malloc(sizeof(TixiDocument));

  /* copy attributes from source document */
  if (srcDocument->xmlFilename != NULL) {
    dstDocument->xmlFilename = (char*) malloc(sizeof(char) * strlen(srcDocument->xmlFilename) + 1);
    strcpy(dstDocument->xmlFilename, srcDocument->xmlFilename);
  }
  else {
    dstDocument->xmlFilename = NULL;
  }

  if (srcDocument->dirname != NULL) {
    dstDocument->dirname = (char*) malloc(sizeof(char) * strlen(srcDocument->dirname) + 1);
    strcpy(dstDocument->dirname, srcDocument->dirname);
  }
  else {
    dstDocument->dirname = NULL;
  }

  if (srcDocument->filename != NULL) {
    dstDocument->filename = (char*) malloc(sizeof(char) * strlen(srcDocument->filename) + 1);
    strcpy(dstDocument->filename, srcDocument->filename);
  }
  else {
    dstDocument->filename = NULL;
  }

  if (srcDocument->validationFilename != NULL) {
    dstDocument->validationFilename = (char*) malloc(sizeof(char) * strlen(srcDocument->validationFilename) + 1);
    strcpy(dstDocument->validationFilename, srcDocument->validationFilename);
  }
  else {
    dstDocument->validationFilename = NULL;
  }

  dstDocument->docPtr = xmlDocument;
  dstDocument->currentNode = rootNode;
  dstDocument->isValid = srcDocument->isValid;
  dstDocument->status = srcDocument->status;
  dstDocument->memoryListHead = NULL;
  dstDocument->memoryListTail = NULL;
  dstDocument->uidListHead = NULL;
  dstDocument->hasIncludedExternalFiles = srcDocument->hasIncludedExternalFiles;
  dstDocument->usePrettyPrint = srcDocument->usePrettyPrint;
  dstDocument->xpathContext = xmlXPathNewContext(xmlDocument);
  dstDocument->xpathCache = XPathNewCache();

  if (addDocumentToList(dstDocument, &(dstDocument->handle)) != SUCESS) {
    printMsg(MESSAGETYPE_ERROR, "Error in TIXI::copyDocument => Failed  adding document to document list.");
    return FAILED;
  }

  *newTixiDocumentHandle = dstDocument->handle;
  return SUCCESS;
}


ReturnCode saveDocument (TixiDocumentHandle handle, const char* xmlFilename, InternalSaveMode saveMode)
{
  TixiDocument* document = getDocument(handle);
  TixiDocument* cpyDoc = NULL;
  xmlNodePtr rootNode = NULL;
  TixiDocumentHandle newHandle;

  if (!xmlFilename) {
    printMsg(MESSAGETYPE_ERROR, "Error: No filename given.\n");
    return FAILED;
  }

  if (!document) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid document handle.\n");
    return INVALID_HANDLE;
  }

  if (saveMode == COMPLETE) {
    if (xmlSaveFormatFileEnc(xmlFilename, document->docPtr, "utf-8", document->usePrettyPrint) == -1) {
      printMsg(MESSAGETYPE_ERROR, "Error: Failed in writing document to file.\n");
      return FAILED;
    };

  }
  else if (saveMode == SPLITTED) {
    /* save included nodes back to external files */
    /* first create a copy and don't chance the original document */
    copyDocument(handle, &newHandle);
    cpyDoc = getDocument(newHandle);

    // update directory and file name
    if (cpyDoc->dirname) {
      free(cpyDoc->dirname);
    }
    if (cpyDoc->filename) {
      free(cpyDoc->filename);
    }
    strip_dirname(xmlFilename, &cpyDoc->dirname, &cpyDoc->filename);

    rootNode = xmlDocGetRootElement(cpyDoc->docPtr);
    saveExternalFiles(rootNode, cpyDoc);

    if (xmlSaveFormatFileEnc(xmlFilename, cpyDoc->docPtr, "utf-8", document->usePrettyPrint) == -1) {
      printMsg(MESSAGETYPE_ERROR, "Error: Failed in writing document to file.\n");
      return FAILED;
    };
    removeDocumentFromList(newHandle);
    freeTixiDocument(cpyDoc);

  }
  else if (saveMode == REMOVED) {
    copyDocument(handle, &newHandle);
    cpyDoc = getDocument(newHandle);

    rootNode = xmlDocGetRootElement(cpyDoc->docPtr);
    removeExternalNodes(rootNode, cpyDoc);

    if (xmlSaveFormatFileEnc(xmlFilename, cpyDoc->docPtr, "utf-8", cpyDoc->usePrettyPrint) == -1) {
      printMsg(MESSAGETYPE_ERROR, "Error: Failed in writing document to file.\n");
      return FAILED;
    };

  }
  else {
    return FAILED;
  }

  free(document->xmlFilename);
  document->xmlFilename = (char*) malloc(sizeof(char) * (strlen(xmlFilename) + 1));
  strcpy(document->xmlFilename, xmlFilename);

  document->status = SAVED;
  return SUCCESS;
}



ReturnCode validateSchema(const TixiDocumentHandle handle, xmlDocPtr* schema_doc, int withDefaults)
{
  TixiDocument* document = getDocument(handle);
  xmlSchemaParserCtxtPtr parser_ctxt;
  xmlSchemaPtr schema;
  xmlSchemaValidCtxtPtr valid_ctxt;
  int is_valid;

  if (*schema_doc == NULL) {
    /* the schema cannot be loaded or is not well-formed */
    return OPEN_SCHEMA_FAILED;
  }

  parser_ctxt = xmlSchemaNewDocParserCtxt(*schema_doc);
  if (parser_ctxt == NULL) {
    printMsg(MESSAGETYPE_ERROR, "Error: validateSchema: unable to create a parser context for the schema.\n");
    xmlFreeDoc(*schema_doc);
    return FAILED;
  }
  schema = xmlSchemaParse(parser_ctxt);
  if (schema == NULL) {
    printMsg(MESSAGETYPE_ERROR, "Error: validateSchema: the schema itself is not valid.\n");
    xmlSchemaFreeParserCtxt(parser_ctxt);
    xmlFreeDoc(*schema_doc);
    return FAILED;
  }
  valid_ctxt = xmlSchemaNewValidCtxt(schema);
  if (valid_ctxt == NULL) {
    printMsg(MESSAGETYPE_ERROR, "Error: validateSchema: unable to create a validation context for the schema.\n");
    xmlSchemaFree(schema);
    xmlSchemaFreeParserCtxt(parser_ctxt);
    xmlFreeDoc(*schema_doc);
    return FAILED;
  }

  xmlSchemaSetValidStructuredErrors(valid_ctxt, xmlStructuredErrorHandler, stderr);

  if (withDefaults) {
    xmlSchemaSetValidOptions(valid_ctxt, XML_SCHEMA_VAL_VC_I_CREATE);
  }
  is_valid = (xmlSchemaValidateDoc(valid_ctxt, document->docPtr) == 0);
  xmlSchemaFreeValidCtxt(valid_ctxt);
  xmlSchemaFree(schema);
  xmlSchemaFreeParserCtxt(parser_ctxt);
  xmlFreeDoc(*schema_doc);

  if (is_valid) {
    return SUCCESS;
  }
  else {
    return NOT_SCHEMA_COMPLIANT;
  }
}

char* generateXPathFromNodePtr(TixiDocumentHandle handle, xmlNodePtr aNodePtr)
{
  xmlNodePtr nodePtr = aNodePtr;
  TixiDocument* document = getDocument(handle);
  char* generatedXPath = NULL;
  char* textPtr = NULL;
  char* tmpText = NULL;
  char* nodeName = NULL;
  size_t size = 0;
  generatedXPath = malloc(1 * sizeof(char));
  generatedXPath[0] = '\0';

  if (!document) {
    printMsg(MESSAGETYPE_ERROR, "Error: Invalid document handle.\n");
    free(generatedXPath);
    return NULL;
  }

  while ((nodePtr != NULL) && (nodePtr->parent != NULL)) {
    if (nodePtr->type == XML_ELEMENT_NODE) {

      /* handle uIDs */
      textPtr = (char*) xmlGetProp(nodePtr, (xmlChar*) "uID");
      if (textPtr) {
        // do something with the attribute...
        size = strlen((char*) nodePtr->name) + strlen(textPtr) + strlen("[@uID=\"\"]") + 1;
        nodeName = (char*) malloc(size * sizeof(char));
        sprintf(nodeName, "%s[@uID=\"%s\"]", nodePtr->name, textPtr);
        xmlFree(textPtr);
      }
      else {
        nodeName = (char*) malloc((strlen((char*) nodePtr->name) + 1) * sizeof(char));
        strcpy(nodeName, (char*) nodePtr->name);
      }

      tmpText = (char*) malloc((strlen(generatedXPath) + 1) * sizeof(char));
      strcpy(tmpText, generatedXPath);
      free(generatedXPath);
      generatedXPath = (char*) malloc((strlen(tmpText) + strlen(nodeName) + 2) * sizeof(char));     // + slash + null
      sprintf(generatedXPath, "/%s%s", nodeName, tmpText);
      free(tmpText);
      free(nodeName);
    }
    nodePtr = nodePtr->parent;
  }

  addToMemoryList(document, (void*) generatedXPath);
  return generatedXPath;
}

int isParent(xmlNodePtr possibleParent, xmlNodePtr n)
{
    // recurse n up until parent is found
    while (n) {
        if (n == possibleParent) {
            return 1;
        }

        n = n->parent;
    }
    
    return 0;
}

int getChildNodeCount(const xmlNodePtr nodePtr)
{
    xmlNodePtr node = 0;
    int i = 0;
    for (node = nodePtr->children; node; node = node->next, i++);
    return i--;
}

