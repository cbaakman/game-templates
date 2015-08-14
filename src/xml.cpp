/* Copyright (C) 2015 Coos Baakman

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/


#include "xml.h"

#include "err.h"
#include <libxml/parser.h>

xmlDocPtr ParseXML(SDL_RWops *io)
{
    const size_t bufsize = 1024;
    size_t res;
    char buf [bufsize];

    xmlParserCtxtPtr ctxt;
    xmlDocPtr doc;

    // read first 4 bytes
    if ((res = io->read (io, buf, 1, 4)) < 4)
    {
        SetError ("Failed to read first xml bytes");
        return NULL;
    }

    // Create a progressive parsing context
    ctxt = xmlCreatePushParserCtxt (NULL, NULL, buf, res, NULL);
    if (! ctxt) {
        SetError ("Failed to create parser context!");
        return NULL;
    }

    // loop on the input getting the document data
    while ((res = io->read (io, buf, 1, bufsize)) > 0) {

        xmlParseChunk (ctxt, buf, res, 0);
    }

    // there is no more input, indicate the parsing is finished.
    xmlParseChunk (ctxt, buf, 0, 1);

    // check if it was well formed
    doc = ctxt->myDoc;
    res = ctxt->wellFormed;
    xmlFreeParserCtxt (ctxt);

    if (!res) {

        SetError ("xml document is not well formed");

        xmlFreeDoc (doc);
        doc = NULL;
    }

    return doc;
}
