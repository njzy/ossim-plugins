//**************************************************************************************************
//
//     OSSIM Open Source Geospatial Data Processing Library
//     See top level LICENSE.txt file for license information
//
//**************************************************************************************************

#include "ossimPotraceUtil.h"
#include <ossim/base/ossimApplicationUsage.h>
#include <ossim/imaging/ossimImageHandlerRegistry.h>
#include <ossim/imaging/ossimImageSourceSequencer.h>
#include <ossim/base/ossimKeywordNames.h>
#include <ossim/base/ossimException.h>
#include <sstream>

using namespace std;

const char* ossimPotraceUtil::DESCRIPTION =
      "Computes vector representation of input raster image.";

static const string MODE_KW = "mode";


ossimPotraceUtil::ossimPotraceUtil()
:  m_mode (LINESTRING),
   m_outputToConsole(false)
{
}

ossimPotraceUtil::~ossimPotraceUtil()
{
}

void ossimPotraceUtil::setUsage(ossimArgumentParser& ap)
{
   // Add global usage options.
   ossimUtility::setUsage(ap);

   // Set the general usage:
   ossimApplicationUsage* au = ap.getApplicationUsage();
   ossimString usageString = ap.getApplicationName();
   usageString += " [options] <input-raster-file> [<output-vector-file>]";
   au->setCommandLineUsage(usageString);

   // Set the command line options:
   au->addCommandLineOption("--mode polygon|linestring",
         "Specifies whether to represent foreground-background boundary as polygons or line-strings"
         ". Polygons are closed regions surrounding either null or non-null pixels. Most viewers "
         "will represent polygons as solid blobs. Line-strings only outline the boundary but do not"
         " maintain sense of \"insideness\"");
}

bool ossimPotraceUtil::initialize(ossimArgumentParser& ap)
{
   string ts1;
   ossimArgumentParser::ossimParameter sp1(ts1);

   if (!ossimUtility::initialize(ap))
      return false;

   if ( ap.read("--mode", sp1))
      m_kwl.addPair(MODE_KW, ts1);

   // Remaining argument is input and output file names:
   if (ap.argc() > 1)
   {
      m_kwl.add(ossimKeywordNames::IMAGE_FILE_KW, ap.argv()[1]);
      if (ap.argc() > 2)
         m_kwl.add(ossimKeywordNames::OUTPUT_FILE_KW, ap.argv()[2]);
   }
   else
   {
      setUsage(ap);
      ap.reportError("Missing input raster filename.");
      ap.writeErrorMessages(ossimNotify(ossimNotifyLevel_NOTICE));
      return false;
   }

   initialize(m_kwl);
   return true;
}

void ossimPotraceUtil::initialize(const ossimKeywordlist& kwl)
{
   ossimString value;
   ostringstream xmsg;

   // Don't copy KWL if member KWL passed in:
   if (&kwl != &m_kwl)
   {
      // Start with clean options keyword list.
      m_kwl.clear();
      m_kwl.addList( kwl, true );
   }

   value = m_kwl.findKey(MODE_KW);
   if (value.contains("polygon"))
      m_mode = POLYGON;
   else if (value.contains("linestring"))
      m_mode = LINESTRING;
   else if (!value.empty())
   {
      xmsg <<"ossimPotraceUtil:"<<__LINE__<<" Unallowed mode requested: <"<<value<<">."
            <<endl;
      throw ossimException(xmsg.str());
   }

   m_inputRasterFname = m_kwl.findKey(ossimKeywordNames::IMAGE_FILE_KW);
   m_outputVectorFname = m_kwl.findKey(ossimKeywordNames::OUTPUT_FILE_KW);
   if (m_outputVectorFname.empty())
   {
      // Output to the active console stream
      m_outputVectorFname = m_inputRasterFname;
      m_outputVectorFname.setExtension("json");
      m_outputToConsole = true;
   }

   ossimUtility::initialize(kwl);

   m_bitmapFname = m_inputRasterFname;
   m_bitmapFname.setExtension("pbm");
}

bool ossimPotraceUtil::execute()
{
   // Open input image:
   m_inputHandler = ossimImageHandlerRegistry::instance()->open(m_inputRasterFname, 1, 0);
   if (!m_inputHandler.valid())
   {
      cout <<"ossimPotraceUtil:"<<__LINE__<<" Could not open input file <"<<m_inputRasterFname<<">"
            <<endl;
      return false;
   }
   // Vector coordinates are in image space. Need to convert to geographic lat/lon:
   ossimRefPtr<ossimImageGeometry> geom = m_inputHandler->getImageGeometry();
   if (!geom.valid())
   {
      cout <<"ossimPotraceUtil:"<<__LINE__<<" Encountered null image geometry!"<<endl;
      return false;
   }

   // Convert raster to bitmap:
   potrace_bitmap_t* potraceBitmap = convertToBitmap();
   if (!potraceBitmap)
      return false;

   // Perform vectorization:
   potrace_param_t* potraceParam = potrace_param_default();
   potraceParam->turdsize = 10;
   potraceParam->alphamax = 0;
   potrace_state_t* potraceOutput = potrace_trace(potraceParam, potraceBitmap);
   if (!potraceOutput)
   {
      cout <<"ossimPotraceUtil:"<<__LINE__<<" Null pointer returned from potrace_trace!"<<endl;
      return false;
   }

   ossimIrect rect = m_inputHandler->getImageRectangle();

   potrace_path_t* potracePaths = potraceOutput->plist;
   potrace_path_t* path = potracePaths;
   ossimDpt imgPt;
   ossimGpt gndPt;
   while (path)
   {
      for (int i=0; i<path->curve.n;)
      {
         // Convert up 3 possible vertices per segment
         for (int v=0; v<3; ++v)
         {
            if ((v == 0) && (path->curve.tag[i] = POTRACE_CORNER))
               continue;

            imgPt.x = path->curve.c[i][v].x;
            imgPt.y = path->curve.c[i][v].y;

            // Potrace reasonably assumes that active pixels on edge need to be bound by a polygon.
            // Override this behavior by avoiding consideration of edge pixels. This involves
            // editing the potrace paths returned by the trace algorithm, splitting the paths into
            // separate independent paths when they encounter an edge. Any segments lying near the
            // edge of the image rectangle will be removed from the path list.
            // TODO: This is tailored for use by the shoreline algorith in OSSIM core. This should
            // be generalized by giving the caller the choice of bounding shapes touching the edges.
            // (OLK 04/2016).

            // Check for edge of image condition and avoid any vertices and associated segment there
            //if ((imgPt.x == rect.ul().x) || (imgPt.x == rect.lr().x) ||
            //    (imgPt.y == rect.ul().y) || (imgPt.y == rect.lr().y))
            if ((fabs(imgPt.x-rect.ul().x)<=1) || (fabs(imgPt.x-rect.lr().x)<=1) ||
                (fabs(imgPt.y-rect.ul().y)<=1) || (fabs(imgPt.y-rect.lr().y)<=1))
            {
               // Need to flag as endpoint to the path to avoid forced closure:
               if (i>1)
                  path->curve.tag[i-1] = POTRACE_ENDPOINT;

               // Need to ignore this segment and split the containing path into two separate paths:
               potrace_path_t* new_path = new potrace_path_t;
               new_path->area = 1;
               new_path->sign = 1;
               new_path->next = path->next;
               new_path->sibling = 0; // Seems to work but scary
               new_path->childlist = 0;
               new_path->priv = path->priv; // No longer needed at this point in the game unless a
                                            // different backend than GeoJSON is used.
               path->next = new_path; // Link in the new path
               new_path->curve.n = path->curve.n - i - 1;
               new_path->curve.c = new potrace_dpoint_t[new_path->curve.n][3];
               new_path->curve.tag = new int[new_path->curve.n];
               for (int j=0; j<new_path->curve.n; j++)
               {
                  for (int u=0; u<3; ++u)
                  {
                     new_path->curve.c[j][u].x = path->curve.c[j+i+1][u].x;
                     new_path->curve.c[j][u].y = path->curve.c[j+i+1][u].y;
                  }
                  new_path->curve.tag[j] = path->curve.tag[j+i+1];
               }
               path->curve.n = i;

               break; // out of vertex loop
            }

            // Good segment, reproject to geographic:
            geom->localToWorld(imgPt, gndPt);
            path->curve.c[i][v].x = gndPt.lon;
            path->curve.c[i][v].y = gndPt.lat;
         }
         ++i;
      }
      path = path->next;
   }

   // Write to output vector file:
   if (!writeGeoJSON(potracePaths))
      return false;

   // Release memory:
   //potrace_state_free(potraceOutput);
   //free(potraceBitmap->map);
   delete potraceBitmap;
   free(potraceParam);

   return true;
}

void ossimPotraceUtil::getKwlTemplate(ossimKeywordlist& kwl)
{
   kwl.add(MODE_KW.c_str(), "polygon|linestring");
   kwl.add(ossimKeywordNames::IMAGE_FILE_KW, "<input-raster-file>");
   kwl.add(ossimKeywordNames::OUTPUT_FILE_KW, "<output-vector-file>");
}

potrace_bitmap_t* ossimPotraceUtil::convertToBitmap()
{
   potrace_bitmap_t* potraceBitmap = new potrace_bitmap_t;

   // Determine output bitmap size to even word boundary:
   ossimIrect rect = m_inputHandler->getImageRectangle();
   potraceBitmap->w = rect.width();
   potraceBitmap->h = rect.height();
   int pixelsPerWord = 8 * sizeof(int*);
   potraceBitmap->dy = (int) ceil((double)rect.width()/pixelsPerWord);

   // Allocate the bitmap memory:
   unsigned long bufLength = potraceBitmap->dy*potraceBitmap->h;
   potraceBitmap->map = new potrace_word[bufLength];

   // Prepare to sequence over all input image tiles and fill the bitmap image:
   ossimRefPtr<ossimImageSourceSequencer> sequencer =
         new ossimImageSourceSequencer(m_inputHandler.get());
   ossimRefPtr<ossimImageData> tile = sequencer->getNextTile();
   double null_pix = tile->getNullPix(0);
   unsigned long offset=0;
   ossimIpt pt_ul, pt, pt_lr;

   // Loop to fill bitmap buffer:
   while (tile.valid())
   {
      pt_ul = tile->getOrigin();
      pt_lr.x = pt_ul.x + tile->getWidth();
      pt_lr.y = pt_ul.y + tile->getHeight();
      if (pt_lr.x > rect.lr().x)
         pt_lr.x = rect.lr().x;
      if (pt_lr.y > rect.lr().y)
         pt_lr.y = rect.lr().y;

      // Nested loops over all pixels in input tile:
      for (pt.y=pt_ul.y; pt.y < pt_lr.y; ++pt.y)
      {
         offset = pt_ul.x/pixelsPerWord + pt.y*potraceBitmap->dy;
         for (pt.x=pt_ul.x; pt.x<pt_lr.x; )
         {
            // Loop to pack a word buffer with pixel on (non-null) or off bit in proper positions:
            unsigned long wordBuf = 0;
            for (int bitpos=pixelsPerWord-1; bitpos>=0; --bitpos)
            {
               unsigned long pixel = (tile->getPix(pt) != null_pix) ? 1 : 0;
               wordBuf |= pixel << bitpos;
               ++pt.x;
               if (pt.x >= pt_lr.x)
                  break;
            }
            potraceBitmap->map[offset++] = wordBuf;
         }
      }

      tile = sequencer->getNextTile();
   }

#if 0
   ossimFilename bmFile (m_outputVectorFname);
   bmFile.setExtension("pbm");
   FILE* pbm = fopen(bmFile.chars(), "w");
   potrace_writepbm(pbm, potraceBitmap);
#endif

   return potraceBitmap;
}

bool ossimPotraceUtil::writeGeoJSON(potrace_path_t* vectorList)
{
   ostringstream xmsg;

   FILE* outFile = fopen(m_outputVectorFname.chars(), "w");
   if (!outFile)
   {
      cout <<"ossimPotraceUtil:"<<__LINE__<<" Could not open output file <"<<m_outputVectorFname
            <<"> for writing."<<endl;
      return false;
   }

   potrace_geojson(outFile, vectorList, (int) (m_mode == LINESTRING));
   fclose(outFile);

   if (m_outputToConsole && m_consoleStream)
   {
      ifstream vectorFile (m_outputVectorFname.chars());
      if (vectorFile.fail())
      {
         xmsg <<"ossimPotraceUtil:"<<__LINE__<<" Error encountered opening temporary vector file at: "
               "<"<<m_outputVectorFname<<">."<<endl;
         throw ossimException(xmsg.str());
      }

      *m_consoleStream << vectorFile.rdbuf();
      vectorFile.close();
   }

   return true;
}
