//---
//
// License: MIT
//
// Description: OSSIM Amazon Web Services (AWS) plugin initialization
// code.
//
//---
// $Id$

#include "ossimAwsStreamFactory.h"

#include "ossimS3IStream.h"

ossim::AwsStreamFactory* ossim::AwsStreamFactory::m_instance = 0;

static bool trace = true; // tmp drb...

ossim::AwsStreamFactory::~AwsStreamFactory()
{
}

ossim::AwsStreamFactory* ossim::AwsStreamFactory::instance()
{
   if(!m_instance)
   {
      m_instance = new ossim::AwsStreamFactory();
   }

   return m_instance;
}

std::shared_ptr<ossim::istream> ossim::AwsStreamFactory::createIstream(
   const ossimString& connectionString, std::ios_base::openmode /*openMode*/) const
{
   // tmp drb
   if ( trace ) std::cout << "ossim::AwsStreamFactory::createIstream entered..." << std::endl;
   
   std::shared_ptr<ossimS3IStream> result = std::make_shared<ossimS3IStream>();

   if( result->open(connectionString) == false )
   {
      result.reset();
   }

   // tmp drb
   if ( trace )std::cout << "ossim::AwsStreamFactory::createIstream exited..." << std::endl;  
   
   return result;
}
      
std::shared_ptr<ossim::ostream> ossim::AwsStreamFactory::createOstream(
   const ossimString& /*connectionString*/, std::ios_base::openmode /*openMode*/) const
{
   std::shared_ptr<ossim::ostream> result(0);
   return result;
}

std::shared_ptr<ossim::iostream> ossim::AwsStreamFactory::createIOstream(
   const ossimString& /*connectionString*/, std::ios_base::openmode /*openMode*/) const
{
   std::shared_ptr<ossim::iostream> result(0);
   return result;
}

// Hidden from use:
ossim::AwsStreamFactory::AwsStreamFactory()
{
}

// Hidden from use:
ossim::AwsStreamFactory::AwsStreamFactory(const ossim::AwsStreamFactory& )
{
}
