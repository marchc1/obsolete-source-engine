#pragma once

#include "CGMODVariant.h"

class IGMODDataTable
{
public:
	virtual const char* GetKey( int ) const = 0;
	virtual const CGMODVariant& GetValue( int ) const = 0;
	virtual int IncrementIterator( int& ) const = 0; // Increment Iterator
	virtual const CGMODVariant& Get( int ) const = 0; // Probably returns CGMODVariant
	virtual void Set( int, const CGMODVariant& ) = 0;
	virtual bool HasKey( int ) const = 0;
	virtual const CGMODVariant& GetLocal( const char* ) const = 0;
	virtual void SetLocal( const char*, const CGMODVariant& ) = 0;
	virtual void ClearLocal( const char* ) = 0;
	virtual void Clear() = 0;
	virtual int Begin() const = 0; // Start Iterator
	virtual void End() const = 0; // Stop Iterator
};