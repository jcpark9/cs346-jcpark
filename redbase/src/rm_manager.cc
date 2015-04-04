#include "rm.h"

RM_Manager::RM_Manager(PF_Manager &pfm)
{
  pfmanager_ = new PF_Manager;
  pPageData = NULL;
}

RM_Manager::~RM_Manager()
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}


RC RM_Manager::CreateFile(const char *fileName, int recordSize)
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}


RC RM_Manager::DestroyFile(const char *fileName)
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}


RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle)
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}


RC RM_Manager::CloseFile(RM_FileHandle &fileHandle)
{
  pageNum = INVALID_PAGE;
  pPageData = NULL;
}