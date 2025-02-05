/*-----------------------------------------------------------------------------
 * Filename        : ossimImageSourceFilter.i
 * Author          : Vipul Raheja
 * License         : See top level LICENSE.txt file.
 * Description     : Contains SWIG-Python of class ossimImageSourceFilter
 * -----------------------------------------------------------------------------*/

%module pyossim

%{

#include <ossim/imaging/ossimImageSource.h>
#include <ossim/imaging/ossimImageSourceFilter.h>
#include <ossim/base/ossimConnectableObjectListener.h>
#include <ossim/base/ossimConnectionEvent.h>

%}

#ifndef TYPE_DATA
#define TYPE_DATA

/* Handling the vector templates */
%include "std_vector.i"
namespace std
{
                %template(vectorossimUInt32) vector<ossim_uint32>;
                %template(vectorossimString) vector<ossimString>;
};

/* Include the header file  */
%import "ossim/base/ossimConstants.h";

/* Wrapping class ossimImageSourceFilter */
%include "ossim/imaging/ossimImageSourceFilter.h" 

#endif
