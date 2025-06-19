/**CFile****************************************************************

  FileName    [abc_namespaces.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Namespace logic.]

  Synopsis    []

  Author      [Alan Mishchenko]

  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - Nov 20, 2015.]

  Revision    []

***********************************************************************/

#ifndef ABC__misc__util__abc_namespaces_h
#define ABC__misc__util__abc_namespaces_h


////////////////////////////////////////////////////////////////////////
///                         NAMESPACES                               ///
////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
#  ifdef PABC_NAMESPACE
#    define PABC_NAMESPACE_HEADER_START namespace PABC_NAMESPACE {
#    define PABC_NAMESPACE_HEADER_END }
#    define PABC_NAMESPACE_CXX_HEADER_START PABC_NAMESPACE_HEADER_START
#    define PABC_NAMESPACE_CXX_HEADER_END PABC_NAMESPACE_HEADER_END
#    define PABC_NAMESPACE_IMPL_START namespace PABC_NAMESPACE {
#    define PABC_NAMESPACE_IMPL_END }
#    define PABC_NAMESPACE_PREFIX PABC_NAMESPACE::
#    define PABC_NAMESPACE_USING_NAMESPACE using namespace PABC_NAMESPACE;
#  else
#    define PABC_NAMESPACE_HEADER_START extern "C" {
#    define PABC_NAMESPACE_HEADER_END }
#    define PABC_NAMESPACE_CXX_HEADER_START
#    define PABC_NAMESPACE_CXX_HEADER_END
#    define PABC_NAMESPACE_IMPL_START
#    define PABC_NAMESPACE_IMPL_END
#    define PABC_NAMESPACE_PREFIX
#    define PABC_NAMESPACE_USING_NAMESPACE
#  endif // #ifdef PABC_NAMESPACE
#ifdef SATOKO_NAMESPACE
    #define SATOKO_NAMESPACE_HEADER_START namespace SATOKO_NAMESPACE {
    #define SATOKO_NAMESPACE_HEADER_END }
    #define SATOKO_NAMESPACE_CXX_HEADER_START PABC_NAMESPACE_HEADER_START
    #define SATOKO_NAMESPACE_CXX_HEADER_END PABC_NAMESPACE_HEADER_END
    #define SATOKO_NAMESPACE_IMPL_START namespace SATOKO_NAMESPACE {
    #define SATOKO_NAMESPACE_IMPL_END }
    #define SATOKO_NAMESPACE_PREFIX SATOKO_NAMESPACE::
    #define SATOKO_NAMESPACE_USING_NAMESPACE using namespace SATOKO_NAMESPACE;
#endif
#else
#  define PABC_NAMESPACE_HEADER_START
#  define PABC_NAMESPACE_HEADER_END
#  define PABC_NAMESPACE_CXX_HEADER_START
#  define PABC_NAMESPACE_CXX_HEADER_END
#  define PABC_NAMESPACE_IMPL_START
#  define PABC_NAMESPACE_IMPL_END
#  define PABC_NAMESPACE_PREFIX
#  define PABC_NAMESPACE_USING_NAMESPACE
#endif // #ifdef __cplusplus

#endif // #ifndef ABC__misc__util__abc_namespaces_h

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
