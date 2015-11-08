#include "global.h"
#include "LuaReference.h"

REGISTER_CLASS_TRAITS( LuaReference, new LuaReference(*pCopy) )

LuaReference::LuaReference()
{
	m_iReference = LUA_NOREF;
}

LuaReference::~LuaReference()
{
	Unregister();
}

LuaReference::LuaReference( const LuaReference &cpy )
{
	if( cpy.m_iReference == LUA_NOREF || cpy.m_iReference == LUA_REFNIL )
	{
		m_iReference = cpy.m_iReference;
	}
	else
	{
		/* Make a new reference. */
		Lua *L = LUA->Get();
		lua_rawgeti( L, LUA_REGISTRYINDEX, cpy.m_iReference );
		m_iReference = luaL_ref( L, LUA_REGISTRYINDEX );
		LUA->Release( L );
	}
}

LuaReference &LuaReference::operator=( const LuaReference &cpy )
{
	if( this == &cpy )
		return *this;

	Unregister();

	if( cpy.m_iReference == LUA_NOREF || cpy.m_iReference == LUA_REFNIL )
	{
		m_iReference = cpy.m_iReference;
	}
	else
	{
		/* Make a new reference. */
		Lua *L = LUA->Get();
		lua_rawgeti( L, LUA_REGISTRYINDEX, cpy.m_iReference );
		m_iReference = luaL_ref( L, LUA_REGISTRYINDEX );
		LUA->Release( L );
	}

	return *this;
}

void LuaReference::SetFromStack( Lua *L )
{
	if( m_iReference != LUA_NOREF )
		luaL_unref( L, LUA_REGISTRYINDEX, m_iReference );
	m_iReference = luaL_ref( L, LUA_REGISTRYINDEX );
}

void LuaReference::SetFromNil()
{
	Unregister();
	m_iReference = LUA_REFNIL;
}

void LuaReference::DeepCopy()
{
	/* Call DeepCopy(t, u), where t is our referenced object and u is the new table. */
	Lua *L = LUA->Get();

	/* Arg 1 (t): */
	this->PushSelf( L );

	/* Arg 2 (u): */
	lua_newtable( L );

	lua_pushvalue( L, -1 );
	this->SetFromStack( L );

	LuaHelpers::DeepCopy( L );

	LUA->Release( L );
}

void LuaReference::PushSelf( lua_State *L ) const
{
	lua_rawgeti( L, LUA_REGISTRYINDEX, m_iReference );
}

bool LuaReference::IsSet() const
{
	return m_iReference != LUA_NOREF;
}

bool LuaReference::IsNil() const
{
	return m_iReference == LUA_REFNIL;
}

int LuaReference::GetLuaType() const
{
	Lua *L = LUA->Get();
	this->PushSelf( L );
	int iRet = lua_type( L, -1 );
	lua_pop( L, 1 );
	LUA->Release( L );

	return iRet;
}

void LuaReference::Unregister()
{
	if( LUA == NULL || m_iReference == LUA_NOREF )
		return; // nothing to do

	Lua *L = LUA->Get();
	luaL_unref( L, LUA_REGISTRYINDEX, m_iReference );
	LUA->Release( L );
	m_iReference = LUA_NOREF;
}

bool LuaReference::SetFromExpression( const RString &sExpression )
{
	Lua *L = LUA->Get();

	bool bSuccess = LuaHelpers::RunExpression( L, sExpression );
	this->SetFromStack( L );

	LUA->Release( L );
	return bSuccess;
}

RString LuaReference::Serialize() const
{
	/* Call Serialize(t), where t is our referenced object. */
	Lua *L = LUA->Get();
	lua_getglobal( L, "Serialize" );

	ASSERT_M( !lua_isnil(L, -1), "Serialize() missing" );
	ASSERT_M( lua_isfunction(L, -1), "Serialize() not a function" );

	/* Arg 1 (t): */
	this->PushSelf( L );

	lua_call( L, 1, 1 );

	/* The return value is a string, which we store in m_sSerializedData. */
	const char *pString = lua_tostring( L, -1 );
	ASSERT_M( pString != NULL, "Serialize() didn't return a string" );

	RString sRet = pString;
	lua_pop( L, 1 );

	LUA->Release( L );

	return sRet;
}

/** @brief Utilities for working with Lua. */
namespace LuaHelpers
{
	template<> bool FromStack<LuaReference>( lua_State *L, LuaReference &Object, int iOffset )
	{
		lua_pushvalue( L, iOffset );
		Object.SetFromStack( L );
		return true;
	}

	template<> bool FromStack<apActorCommands>( lua_State *L, apActorCommands &Object, int iOffset )
	{
		LuaReference *pRef = new LuaReference;
		FromStack( L, *pRef, iOffset );
		Object = apActorCommands( pRef );
		return true;
	}
}

LuaTable::LuaTable()
{
	Lua *L = LUA->Get();
	lua_newtable( L );
	this->SetFromStack(L);
	LUA->Release( L );
}

void LuaTable::Set( Lua *L, const RString &sKey )
{
	int iTop = lua_gettop( L );
	this->PushSelf( L );
	lua_pushvalue( L, iTop ); // push the value
	lua_setfield( L, -2, sKey );
	lua_settop( L, iTop-1 ); // remove all of the above
}

void LuaTable::Get( Lua *L, const RString &sKey )
{
	this->PushSelf( L );
	lua_getfield( L, -1, sKey );
	lua_remove( L, -2 ); // remove self
}

/** @brief Utilities for working with Lua. */
namespace LuaHelpers
{
	template<> void Push<LuaReference>( lua_State *L, const LuaReference &Object ) 
	{
		Object.PushSelf( L );
	}
}

/*
 * (c) 2005 Glenn Maynard, Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
