//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
 
#include <stdarg.h>
#include "dt_send.h"
#include "dt.h"
#include "dt_recv.h"
#include "dt_encode.h"
#include "convar.h"
#include "commonmacros.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"
#include "dt_stack.h"
#ifdef BUILD_GMOD
#include "basehandle.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define PROPINDEX_NUMBITS 12
#define MAX_TOTAL_SENDTABLE_PROPS	(1 << PROPINDEX_NUMBITS)


ConVar g_CV_DTWatchEnt( "dtwatchent", "-1", 0, "Watch this entities data table encoding." );
ConVar g_CV_DTWatchVar( "dtwatchvar", "", 0, "Watch the named variable." );
ConVar g_CV_DTWarning( "dtwarning", "0", 0, "Print data table warnings?" );
ConVar g_CV_DTWatchClass( "dtwatchclass", "", 0, "Watch all fields encoded with this table." );



// ----------------------------------------------------------------------------- //
//
// CBuildHierarchyStruct
//
// Used while building a CSendNode hierarchy.
//
// ----------------------------------------------------------------------------- //
class CBuildHierarchyStruct
{
public:
	const ExcludeProp	*m_pExcludeProps;
	int					m_nExcludeProps;

	const SendProp		*m_pDatatableProps[MAX_TOTAL_SENDTABLE_PROPS];
	int					m_nDatatableProps;
	
	const SendProp		*m_pProps[MAX_TOTAL_SENDTABLE_PROPS];
	unsigned char		m_PropProxyIndices[MAX_TOTAL_SENDTABLE_PROPS];
	int					m_nProps;

	unsigned char m_nPropProxies;
};


// ----------------------------------------------------------------------------- //
// CSendNode.
// ----------------------------------------------------------------------------- //

CSendNode::CSendNode()
{
	m_iDatatableProp = -1;
	m_pTable = NULL;
	
	m_iFirstRecursiveProp = m_nRecursiveProps = 0;

	m_DataTableProxyIndex = DATATABLE_PROXY_INDEX_INVALID; // set it to a questionable value.
}

CSendNode::~CSendNode()
{
	int c = GetNumChildren();
	for ( int i = c - 1 ; i >= 0 ; i-- )
	{
		delete GetChild( i );
	}
	m_Children.Purge();
}

// ----------------------------------------------------------------------------- //
// CSendTablePrecalc
// ----------------------------------------------------------------------------- //

bool PropOffsetLT( const unsigned short &a, const unsigned short &b )
{
	return a < b;
}

CSendTablePrecalc::CSendTablePrecalc() : 
	m_PropOffsetToIndexMap( 0, 0, PropOffsetLT )
{
	m_pDTITable = NULL;
	m_pSendTable = 0;
	m_nDataTableProxies = 0;
}


CSendTablePrecalc::~CSendTablePrecalc()
{
	if ( m_pSendTable )
		m_pSendTable->m_pPrecalc = 0;
}


const ExcludeProp* FindExcludeProp(
	char const *pTableName,
	char const *pPropName,
	const ExcludeProp *pExcludeProps, 
	int nExcludeProps)
{
	for ( int i=0; i < nExcludeProps; i++ )
	{
		if ( stricmp(pExcludeProps[i].m_pTableName, pTableName) == 0 && stricmp(pExcludeProps[i].m_pPropName, pPropName ) == 0 )
			return &pExcludeProps[i];
	}

	return NULL;
}


// Fill in a list of all the excluded props.
static bool SendTable_GetPropsExcluded( const SendTable *pTable, ExcludeProp *pExcludeProps, int &nExcludeProps, int nMaxExcludeProps )
{
	for(int i=0; i < pTable->m_nProps; i++)
	{
		SendProp *pProp = &pTable->m_pProps[i];

		if ( pProp->IsExcludeProp() )
		{
			char const *pName = pProp->GetExcludeDTName();

			ErrorIfNot( pName,
				("Found an exclude prop missing a name.")
			);
			
			ErrorIfNot( nExcludeProps < nMaxExcludeProps,
				("SendTable_GetPropsExcluded: Overflowed max exclude props with %s.", pName)
			);

			pExcludeProps[nExcludeProps].m_pTableName = pName;
			pExcludeProps[nExcludeProps].m_pPropName = pProp->GetName();
			nExcludeProps++;
		}
		else if ( pProp->GetDataTable() )
		{
			if( !SendTable_GetPropsExcluded( pProp->GetDataTable(), pExcludeProps, nExcludeProps, nMaxExcludeProps ) )
				return false;
		}
	}

	return true;
}


// Set the datatable proxy indices in all datatable SendProps.
static void SetDataTableProxyIndices_R( 
	CSendTablePrecalc *pMainTable, 
	CSendNode *pCurTable,
	CBuildHierarchyStruct *bhs )
{
	for ( int i=0; i < pCurTable->GetNumChildren(); i++ )
	{
		CSendNode *pNode = pCurTable->GetChild( i );
		const SendProp *pProp = bhs->m_pDatatableProps[pNode->m_iDatatableProp];

		if ( pProp->GetFlags() & SPROP_PROXY_ALWAYS_YES )
		{
			pNode->SetDataTableProxyIndex( DATATABLE_PROXY_INDEX_NOPROXY );
		}
		else
		{
			pNode->SetDataTableProxyIndex( pMainTable->GetNumDataTableProxies() );
			pMainTable->SetNumDataTableProxies( pMainTable->GetNumDataTableProxies() + 1 );
		}

		SetDataTableProxyIndices_R( pMainTable, pNode, bhs );
	}
}

// Set the datatable proxy indices in all datatable SendProps.
static void SetRecursiveProxyIndices_R( 
	SendTable *pBaseTable,
	CSendNode *pCurTable,
	int &iCurProxyIndex )
{
	if ( iCurProxyIndex >= CDatatableStack::MAX_PROXY_RESULTS )
		Error( "Too many proxies for datatable %s.", pBaseTable->GetName() );

	pCurTable->SetRecursiveProxyIndex( iCurProxyIndex );
	iCurProxyIndex++;
	
	for ( int i=0; i < pCurTable->GetNumChildren(); i++ )
	{
		CSendNode *pNode = pCurTable->GetChild( i );
		SetRecursiveProxyIndices_R( pBaseTable, pNode, iCurProxyIndex );
	}
}


void SendTable_BuildHierarchy( 
	CSendNode *pNode,
	const SendTable *pTable, 
	CBuildHierarchyStruct *bhs
	);


void SendTable_BuildHierarchy_IterateProps(
	CSendNode *pNode,
	const SendTable *pTable, 
	CBuildHierarchyStruct *bhs,
	const SendProp *pNonDatatableProps[MAX_TOTAL_SENDTABLE_PROPS],
	int &nNonDatatableProps )
{
	int i;
	for ( i=0; i < pTable->m_nProps; i++ )
	{
		const SendProp *pProp = &pTable->m_pProps[i];

		if ( pProp->IsExcludeProp() || 
			pProp->IsInsideArray() || 
			FindExcludeProp( pTable->GetName(), pProp->GetName(), bhs->m_pExcludeProps, bhs->m_nExcludeProps ) )
		{
			continue;
		}

		if ( pProp->GetType() == DPT_DataTable )
		{
			if ( pProp->GetFlags() & SPROP_COLLAPSIBLE )
			{
				// This is a base class.. no need to make a new CSendNode (and trigger a bunch of
				// unnecessary send proxy calls in the datatable stacks).
				SendTable_BuildHierarchy_IterateProps( 
					pNode,
					pProp->GetDataTable(), 
					bhs, 
					pNonDatatableProps, 
					nNonDatatableProps );
			}
			else
			{
				// Setup a child datatable reference.
				CSendNode *pChild = new CSendNode;

				// Setup a datatable prop for this node to reference (so the recursion
				// routines can get at the proxy).
				if ( bhs->m_nDatatableProps >= ARRAYSIZE( bhs->m_pDatatableProps ) )
					Error( "Overflowed datatable prop list in SendTable '%s'.", pTable->GetName() );
				
				bhs->m_pDatatableProps[bhs->m_nDatatableProps] = pProp;
				pChild->m_iDatatableProp = bhs->m_nDatatableProps;
				++bhs->m_nDatatableProps;

				pNode->m_Children.AddToTail( pChild );

				// Recurse into the new child datatable.
				SendTable_BuildHierarchy( pChild, pProp->GetDataTable(), bhs );
			}
		}
		else
		{
			if ( nNonDatatableProps >= MAX_TOTAL_SENDTABLE_PROPS )
				Error( "SendTable_BuildHierarchy: overflowed non-datatable props with '%s'.", pProp->GetName() );
			
			pNonDatatableProps[nNonDatatableProps] = pProp;
			++nNonDatatableProps;
		}
	}
}


void SendTable_BuildHierarchy( 
	CSendNode *pNode,
	const SendTable *pTable, 
	CBuildHierarchyStruct *bhs
	)
{
	pNode->m_pTable = pTable;
	pNode->m_iFirstRecursiveProp = bhs->m_nProps;
	
	Assert( bhs->m_nPropProxies < 255 );
	unsigned char curPropProxy = bhs->m_nPropProxies;
	++bhs->m_nPropProxies;

	const SendProp *pNonDatatableProps[MAX_TOTAL_SENDTABLE_PROPS];
	int nNonDatatableProps = 0;
	
	// First add all the child datatables.
	SendTable_BuildHierarchy_IterateProps(
		pNode,
		pTable,
		bhs,
		pNonDatatableProps,
		nNonDatatableProps );

	
	// Now add the properties.

	// Make sure there's room, then just copy the pointers from the loop above.
	ErrorIfNot( bhs->m_nProps + nNonDatatableProps < ARRAYSIZE( bhs->m_pProps ),
		("SendTable_BuildHierarchy: overflowed prop buffer.")
	);
	
	for ( int i=0; i < nNonDatatableProps; i++ )
	{
		bhs->m_pProps[bhs->m_nProps] = pNonDatatableProps[i];
		bhs->m_PropProxyIndices[bhs->m_nProps] = curPropProxy;
		++bhs->m_nProps;
	}

	pNode->m_nRecursiveProps = bhs->m_nProps - pNode->m_iFirstRecursiveProp;
}

void SendTable_SortByPriority(CBuildHierarchyStruct *bhs)
{
	int i, start = 0;

	while( true )
	{
		for ( i = start; i < bhs->m_nProps; i++ )
		{
			const SendProp *p = bhs->m_pProps[i];
			unsigned char c = bhs->m_PropProxyIndices[i];

			if ( p->GetFlags() & SPROP_CHANGES_OFTEN )
			{
				bhs->m_pProps[i] = bhs->m_pProps[start];
				bhs->m_PropProxyIndices[i] = bhs->m_PropProxyIndices[start];
				bhs->m_pProps[start] = p;
				bhs->m_PropProxyIndices[start] = c;
				start++;
				break;
			}
		}

		if ( i == bhs->m_nProps )
			return; 
	}
}


void CalcPathLengths_R( CSendNode *pNode, CUtlVector<int> &pathLengths, int curPathLength, int &totalPathLengths )
{
	pathLengths[pNode->GetRecursiveProxyIndex()] = curPathLength;
	totalPathLengths += curPathLength;
	
	for ( int i=0; i < pNode->GetNumChildren(); i++ )
	{
		CalcPathLengths_R( pNode->GetChild( i ), pathLengths, curPathLength+1, totalPathLengths );
	}
}


void FillPathEntries_R( CSendTablePrecalc *pPrecalc, CSendNode *pNode, CSendNode *pParent, int &iCurEntry )
{
	// Fill in this node's path.
	CSendTablePrecalc::CProxyPath &outProxyPath = pPrecalc->m_ProxyPaths[ pNode->GetRecursiveProxyIndex() ];
	outProxyPath.m_iFirstEntry = (unsigned short)iCurEntry;

	// Copy all the proxies leading to the parent.
	if ( pParent )
	{
		CSendTablePrecalc::CProxyPath &parentProxyPath = pPrecalc->m_ProxyPaths[pParent->GetRecursiveProxyIndex()];
		outProxyPath.m_nEntries = parentProxyPath.m_nEntries + 1;

		for ( int i=0; i < parentProxyPath.m_nEntries; i++ )
			pPrecalc->m_ProxyPathEntries[iCurEntry++] = pPrecalc->m_ProxyPathEntries[parentProxyPath.m_iFirstEntry+i];
		
		// Now add this node's own proxy.
		pPrecalc->m_ProxyPathEntries[iCurEntry].m_iProxy = pNode->GetRecursiveProxyIndex();
		pPrecalc->m_ProxyPathEntries[iCurEntry].m_iDatatableProp = pNode->m_iDatatableProp;
		++iCurEntry;
	}
	else
	{
		outProxyPath.m_nEntries = 0;
	}

	for ( int i=0; i < pNode->GetNumChildren(); i++ )
	{
		FillPathEntries_R( pPrecalc, pNode->GetChild( i ), pNode, iCurEntry );
	}
}


void SendTable_GenerateProxyPaths( CSendTablePrecalc *pPrecalc, int nProxyIndices )
{
	// Initialize the array.
	pPrecalc->m_ProxyPaths.SetSize( nProxyIndices );
	for ( int i=0; i < nProxyIndices; i++ )
		pPrecalc->m_ProxyPaths[i].m_iFirstEntry = pPrecalc->m_ProxyPaths[i].m_nEntries = 0xFFFF;
	
	// Figure out how long the path down the tree is to each node.
	int totalPathLengths = 0;
	CUtlVector<int> pathLengths;
	pathLengths.SetSize( nProxyIndices );
	memset( pathLengths.Base(), 0, sizeof( pathLengths[0] ) * nProxyIndices );
	CalcPathLengths_R( pPrecalc->GetRootNode(), pathLengths, 0, totalPathLengths );
	
	// 
	int iCurEntry = 0;
	pPrecalc->m_ProxyPathEntries.SetSize( totalPathLengths );
	FillPathEntries_R( pPrecalc, pPrecalc->GetRootNode(), NULL, iCurEntry );
}


bool CSendTablePrecalc::SetupFlatPropertyArray()
{
	SendTable *pTable = GetSendTable();

	// First go through and set SPROP_INSIDEARRAY when appropriate, and set array prop pointers.
	SetupArrayProps_R<SendTable, SendTable::PropType>( pTable );

	// Make a list of which properties are excluded.
	ExcludeProp excludeProps[MAX_EXCLUDE_PROPS];
	int nExcludeProps = 0;
	if( !SendTable_GetPropsExcluded( pTable, excludeProps, nExcludeProps, MAX_EXCLUDE_PROPS ) )
		return false;

	// Now build the hierarchy.
	CBuildHierarchyStruct bhs;
	bhs.m_pExcludeProps = excludeProps;
	bhs.m_nExcludeProps = nExcludeProps;
	bhs.m_nProps = bhs.m_nDatatableProps = 0;
	bhs.m_nPropProxies = 0;
	SendTable_BuildHierarchy( GetRootNode(), pTable, &bhs );

	SendTable_SortByPriority( &bhs );
	
	// Copy the SendProp pointers into the precalc.	
	MEM_ALLOC_CREDIT();
	m_Props.CopyArray( bhs.m_pProps, bhs.m_nProps );
	m_DatatableProps.CopyArray( bhs.m_pDatatableProps, bhs.m_nDatatableProps );
	m_PropProxyIndices.CopyArray( bhs.m_PropProxyIndices, bhs.m_nProps );

	// Assign the datatable proxy indices.
	SetNumDataTableProxies( 0 );
	SetDataTableProxyIndices_R( this, GetRootNode(), &bhs );
	
	int nProxyIndices = 0;
	SetRecursiveProxyIndices_R( pTable, GetRootNode(), nProxyIndices );

	SendTable_GenerateProxyPaths( this, nProxyIndices );
	return true;
}


// ---------------------------------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------------------------------- //

// Compares two arrays of bits.
// Returns true if they are equal.
bool AreBitArraysEqual(
	void const *pvBits1,
	void const *pvBits2,
	int nBits ) 
{
	unsigned int const *pBits1 = (unsigned int const *)pvBits1;
	unsigned int const *pBits2 = (unsigned int const *)pvBits2;

	// Compare words.
	int nWords = nBits >> 5;
	for ( int i = 0 ; i < nWords; ++i )
	{
		if ( pBits1[i] != pBits2[i] )
			return false;
	}

	if ( nBits & 31 )
	{
		// Compare remaining bits.
		unsigned int mask = (1 << (nBits & 31)) - 1;
		return ((pBits1[nWords] ^ pBits2[nWords]) & mask) == 0;
	}

	return true;
}


// Does a fast memcmp-based test to determine if the two bit arrays are different.
// Returns true if they are equal.
bool CompareBitArrays(
	void const *pPacked1,
	void const *pPacked2,
	int nBits1,
	int nBits2
	)
{
	if( nBits1 >= 0 && nBits1 == nBits2 )
	{
		if ( pPacked1 == pPacked2 )
		{
			return true;
		}
		else
		{
			return AreBitArraysEqual( pPacked1, pPacked2, nBits1 );
		}
	}
	else
		return false;
}

// Looks at the DTWatchEnt and DTWatchProp console variables and returns true
// if the user wants to watch this property.
bool ShouldWatchThisProp( const SendTable *pTable, int objectID, const char *pPropName )
{
	if(g_CV_DTWatchEnt.GetInt() != -1 &&
		g_CV_DTWatchEnt.GetInt() == objectID)
	{
		const char *pStr = g_CV_DTWatchVar.GetString();
		if ( pStr && pStr[0] != 0 )
		{
			return stricmp( pStr, pPropName ) == 0;
		}
		else
		{
			return true;
		}
	}

	if ( g_CV_DTWatchClass.GetString()[ 0 ] && Q_stristr( pTable->GetName(), g_CV_DTWatchClass.GetString() ) )
		return true;

	return false;
}

bool Sendprop_UsingDebugWatch()
{
	if ( g_CV_DTWatchEnt.GetInt() != -1 )
		return true;

	if ( g_CV_DTWatchClass.GetString()[ 0 ] )
		return true; 

	return false;
}


// Prints a datatable warning into the console.
void DataTable_Warning( const char *pInMessage, ... )
{
	char msg[4096];
	va_list marker;
	
#if 0
	#if !defined(_DEBUG)
		if(!g_CV_DTWarning.GetInt())
			return;
	#endif
#endif

	va_start(marker, pInMessage);
	Q_vsnprintf( msg, sizeof( msg ), pInMessage, marker);
	va_end(marker);

	Warning( "DataTable warning: %s", msg );
}

#ifdef BUILD_GMOD
VariantInfoType s_VariantInfo[CGMODVariant_Count] =
{
	// CGMODVariant_NIL
	{
		NULL,
		NULL,
		NULL,
		NULL,
	},

	// CGMODVariant_Float
	{
		F_Write,
		F_Read,
		F_Skip,
		F_Compare,
	},

	// CGMODVariant_Int
	{
		I_Write,
		I_Read,
		I_Skip,
		I_Compare,
	},

	// CGMODVariant_Bool
	{
		B_Write,
		B_Read,
		B_Skip,
		B_Compare,
	},

	// CGMODVariant_Vector
	{
		V_Write,
		V_Read,
		V_Skip,
		V_Compare,
	},

	// CGMODVariant_Angle
	{
		A_Write,
		A_Read,
		A_Skip,
		A_Compare,
	},

	// CGMODVariant_Entity
	{
		E_Write,
		E_Read,
		E_Skip,
		E_Compare,
	},

	// CGMODVariant_String
	{
		S_Write,
		S_Read,
		S_Skip,
		S_Compare,
	},
};

CThreadLocalInt<int> CGMODDataTable::s_ReferenceTick;
CThreadLocalInt<int> CGMODDataTable::s_TargetTick;
CThreadLocalInt<CGMODDataTable*> CGMODDataTable::s_CurrentTable;

CGMODVariant var_null;
const char* CGMODDataTable::GetKey( int index ) const
{
	return nullptr;
}

const CGMODVariant& CGMODDataTable::GetValue( int index ) const
{
	return var_null;
}

int CGMODDataTable::IncrementIterator( int& index ) const
{
	return m_pVars.NextInorder( index );
}

const CGMODVariant& CGMODDataTable::Get( int index ) const
{
	return var_null;
}

void CGMODDataTable::Set( int index, const CGMODVariant& pValue )
{
}

bool CGMODDataTable::HasKey( int index ) const
{
	return false;
}

const CGMODVariant& CGMODDataTable::GetLocal( const char* pKey ) const
{
	return var_null;
}

void CGMODDataTable::SetLocal( const char* pKey, const CGMODVariant& pValue )
{
}

void CGMODDataTable::ClearLocal( const char* pKey )
{
}

void CGMODDataTable::Clear()
{
	m_pVars.RemoveAll();

	// ToDo: Look throu all vars and call free? does Gmod use malloc?
}

int CGMODDataTable::Begin() const
{
	return m_pVars.FirstInorder();
}

void CGMODDataTable::End() const
{
	// ToDo
}

CGMODDataTable::CGMODDataTable( FnGMODTableProxy proxy ) // proxy is only used Clientside
{
	somevar1 = NULL;
	hopefullyourproxy = proxy;
}

void CGMODDataTable::WriteProps( bf_read* a, bf_write* b, int objectID )
{
}

void CGMODDataTable::Encode( void* pStruct, bf_write* bf )
{
	/* Structure
		12 bits - varCount - How many vars got written
		1 bit - magic - the magic bit. idk. If varCount is 13 bits big, it will cause issues later on in decode. (It did when I tested it in gmod.)
		
		var loop:
			12 bits - keyIndex - The Index of the key in the stringtable
			3 bits - varType - the type of the value

		values:
			float: 32 bits
			int: 32 bits
			bool: 1 bit
			vector: 96 bits
			angle: 96 bits
			entity: 16 bits
			string:
				9 bits - length - the length of the string
				8 bits * length - the string
	*/

	CGMODDataTable* dt = (CGMODDataTable*)pStruct; // It's not done like this right? Idk.
	bf->WriteUBitLong( dt->m_pVars.Count(), 12 );
	bf->WriteOneBit( 0 ); // What does it actually do?

	for ( int i=0; i<dt->m_pVars.Count(); ++i )
	{
		CUtlMap<unsigned short, CGMODDataTable::Entry, unsigned short>::Node_t var = dt->m_pVars[i];
		bf->WriteUBitLong( var.elem.keyIndex, 12 );
		bf->WriteUBitLong( var.elem.value.type, 3 );

		s_VariantInfo[ var.elem.value.type ].Write( bf, var.elem.value );
	}
}

void CGMODDataTable::Decode( void* pStruct, bf_read* bf )
{
	int vars = bf->ReadUBitLong( 12 );
	bool unknown = bf->ReadOneBit(); // What does it actually do? Do we nuke all vars when we read it and it's true?

	// How do we know where to put it :<
	for ( int i=0; i<vars; ++i )
	{
		CUtlMap<unsigned short, CGMODDataTable::Entry, unsigned short>::Node_t var = m_pVars[i];
		int keyIndex = bf->ReadUBitLong( 12 );
		int valType = bf->ReadUBitLong( 3 );

		// ToDo
		//s_VariantInfo[ valType ].Read( bf );
	}
}

void CGMODDataTable::CopyFrom( void* idk, const void* idk2, CGMODDataTable* dt )
{
}

bool CGMODDataTable::IsEmpty() const
{
	return true;
}

int CGMODDataTable::Compare( bf_read* p1, bf_read* p2, CGMODDataTable* dt, int targetTick )
{
	return 0;
}

void CGMODDataTable::Skip( bf_read* bf )
{
}

void F_Write( bf_write* bf, const CGMODVariant& var )
{
	float value = var.m_Float;
	if ( var.type == 7 )
	{
		value = V_atof( var.m_pString );
	}

	bf->WriteFloat( value );
}

void F_Read( bf_read* bf, CGMODVariant& var )
{
	var.m_Float = bf->ReadFloat();
	var.type = CGMODVariant_Number;
}

void F_Skip( bf_read* bf )
{
	bf->SeekRelative( 32 );
}

bool F_Compare( bf_read* bf1, bf_read* bf2 )
{
	return bf1->ReadFloat() == bf2->ReadFloat();
}

void I_Write( bf_write* bf, const CGMODVariant& var )
{
	int value = var.m_Int;
	if ( var.type == 7 )
	{
		value = V_atof( var.m_pString );
	}

	bf->WriteBitLong( value, 32, true );
}

void I_Read( bf_read* bf, CGMODVariant& var )
{
	var.m_Float = bf->ReadBitLong( 32, true );
	var.type = CGMODVariant_Int;
}

void I_Skip( bf_read* bf )
{
	bf->SeekRelative( 32 );
}

bool I_Compare( bf_read* bf1, bf_read* bf2 )
{
	return bf1->ReadBitLong( 32, true ) == bf2->ReadBitLong( 32, true );
}

void B_Write( bf_write* bf, const CGMODVariant& var )
{
	bool value = var.m_Bool;
	if ( var.type == 7 )
	{
		value = V_stricmp( var.m_pString, "true" ) == 0;
	}

	bf->WriteOneBit( value );
}

void B_Read( bf_read* bf, CGMODVariant& var )
{
	var.m_Bool = bf->ReadOneBit();
	var.type = CGMODVariant_Bool;
}

void B_Skip( bf_read* bf )
{
	bf->SeekRelative( 1 );
}

bool B_Compare( bf_read* bf1, bf_read* bf2 )
{
	return bf1->ReadOneBit() == bf2->ReadOneBit();
}

void V_Write( bf_write* bf, const CGMODVariant& var )
{
	Vector value = var.m_Vec;
	if ( var.type == 7 )
	{
		// This is getting out of hand xd
		sscanf( var.m_pString, "%f %f %f", &value.x, &value.y, &value.z );
	}

	bf->WriteBitVec3Coord( value );
}

void V_Read( bf_read* bf, CGMODVariant& var )
{
	bf->ReadBitVec3Coord( var.m_Vec );
	var.type = CGMODVariant_Vector;
}

void V_Skip( bf_read* bf ) // GMOD actually reads the floats
{
	bf->SeekRelative( 96 ); // 32(float) * 3
}

bool V_Compare( bf_read* bf1, bf_read* bf2 )
{
	return bf1->ReadFloat() == bf2->ReadFloat() &&  bf1->ReadFloat() == bf2->ReadFloat() &&  bf1->ReadFloat() == bf2->ReadFloat(); // Should work
}

void A_Write( bf_write* bf, const CGMODVariant& var )
{
	QAngle value = var.m_Ang;
	if ( var.type == 7 )
	{
		// This is really getting out of hand
		sscanf( var.m_pString, "%f %f %f", &value.x, &value.y, &value.z );
	}

	bf->WriteBitAngles( value );
}

void A_Read( bf_read* bf, CGMODVariant& var )
{
	bf->ReadBitAngles( var.m_Ang );
	var.type = CGMODVariant_Angle;
}

void A_Skip( bf_read* bf ) // GMOD actually reads the floats
{
	bf->SeekRelative( 96 ); // 32(float) * 3
}

bool A_Compare( bf_read* bf1, bf_read* bf2 )
{
	return bf1->ReadFloat() == bf2->ReadFloat() && bf1->ReadFloat() == bf2->ReadFloat() && bf1->ReadFloat() == bf2->ReadFloat(); // Should work
}

void E_Write( bf_write* bf, const CGMODVariant& var )
{
	int value = var.m_Ent;
	if ( var.type == 7 )
	{
		// Why
		value = V_atoi( var.m_pString );
	}

	bf->WriteShort( value );
}

void E_Read( bf_read* bf, CGMODVariant& var )
{
	var.m_Ent = bf->ReadShort();
	var.type = CGMODVariant_Entity;
}

void E_Skip( bf_read* bf )
{
	bf->SeekRelative( 16 );
}

bool E_Compare( bf_read* bf1, bf_read* bf2 )
{
	return bf1->ReadShort() == bf2->ReadShort();
}

void S_Write( bf_write* bf, const CGMODVariant& var )
{
	const char* string = var.m_pString;

	switch( var.type )
	{
		case CGMODVariant_Number:
			// ToDo %g
			break;
		case CGMODVariant_Vector:
			// ToDo %g %g %g
			break;
		case CGMODVariant_Angle:
			// ToDo %g %g %g
			break;
		case CGMODVariant_Int:
			// ToDo %d
			break;
		case CGMODVariant_Entity:
			// ToDo %d
			break;
		case CGMODVariant_Bool:
			string = var.m_Bool ? "true" : "false";
			break;
		default:
			break;
	}

	int length = strlen( string );
	bf->WriteUBitLong( length, 9 );
	bf->WriteBytes( string, length );
}

void S_Read( bf_read* bf, CGMODVariant& var )
{
	var.m_Length = bf->ReadUBitLong( 9 ) + 1;
	if ( var.m_pString )
	{
		var.m_pString = (char*)realloc( var.m_pString, var.m_Length * sizeof(char) );
	} else {
		var.m_pString = (char*)malloc( var.m_Length * sizeof(char) );
	}

	bf->ReadBytes( var.m_pString, var.m_Length );
	var.m_pString[ var.m_Length ] = '\0';
}

void S_Skip( bf_read* bf )
{
	int length = bf->ReadUBitLong( 9 );
	bf->SeekRelative( 8 * length ); // Skip it
}

bool S_Compare( bf_read* bf1, bf_read* bf2 )
{
	int length1 = bf1->ReadUBitLong( 9 );
	int length2 = bf2->ReadUBitLong( 9 );
	if ( length1 != length2 )
		return false;

	for ( int i=0; i<length1; ++i )
	{
		if ( bf1->ReadChar() != bf2->ReadChar() )
			return false;
	}

	return true;
}
#endif