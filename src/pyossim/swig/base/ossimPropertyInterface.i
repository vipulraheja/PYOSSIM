/*-----------------------------------------------------------------------------
 * Filename        : ossimPropertyInterface.i
 * Author          : Vipul Raheja
 * License         : See top level LICENSE.txt file.
 * Description     : Contains SWIG-Python of class ossimPropertyInterface
 * -----------------------------------------------------------------------------*/

%module pyossim

%{
#include <ossim/base/ossimPropertyInterface.h>
%}

#ifndef TYPE_DATA
#define TYPE_DATA

/* Wrapping the class ossimPropertyInterface */
%include "ossim/base/ossimConstants.h"
%include "ossim/base/ossimPropertyInterface.h"

#endif
