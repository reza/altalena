#pragma once
#include "LuaObject.h"

#define method(class, name) {#name, &class::name}

enum LUA_RT_CONTROL
{
	LUA_RT_ALLOW_NONE = 0x00,
	LUA_RT_ALLOW_NEW  = 0x01,
	LUA_RT_ALLOW_GC   = 0x10,
	LUA_RT_ALLOW_ALL  = LUA_RT_ALLOW_GC | LUA_RT_ALLOW_NEW
};

typedef struct { void *pT; luaobject *lo;} userdataType;

template <typename T> class Luna {
	public:
	

	typedef int (T::*mfp)(lua_State *L);
	typedef struct { const char *name; mfp mfunc; } RegType;

	static void PushObject(lua_State *L, T *obj)
	{

		// create user data
		userdataType *ud =
			static_cast<userdataType*>(lua_newuserdata(L, sizeof(userdataType)));
		ud->pT = obj;  // store pointer to object in userdata
		ud->lo = static_cast<luaobject*>(obj);

		luaL_getmetatable(L, T::className);  // lookup metatable in Lua registry
		lua_setmetatable(L, -2);

	};

	static void RegisterObject(lua_State *L, T *obj, int index, const char *name)
	{
		
		
		int i = lua_gettop(L);
		lua_rawgeti (L, LUA_REGISTRYINDEX, index);
		i = lua_gettop(L);

		lua_pushstring (L, name); // push the key
		i = lua_gettop(L);

		// create user data
		userdataType *ud =
			static_cast<userdataType*>(lua_newuserdata(L, sizeof(userdataType)));
		ud->pT = obj;  // store pointer to object in userdata
		ud->lo = static_cast<luaobject*>(obj);

		
		i = lua_gettop(L);
		

		luaL_getmetatable(L, T::className);  // lookup metatable in Lua registry
		i = lua_gettop(L);
		lua_setmetatable(L, -2);
		i = lua_gettop(L);

		
		lua_settable (L, -3);

	};


	static void RegisterType(lua_State *L, LUA_RT_CONTROL luartcontrol = LUA_RT_ALLOW_ALL , lua_CFunction creator = NULL) {
		lua_newtable(L);
		int methods = lua_gettop(L);

		luaL_newmetatable(L, T::className);
		int metatable = lua_gettop(L);

		// store method table in globals so that
		// scripts can add functions written in Lua.
		lua_pushstring(L, T::className);
		lua_pushvalue(L, methods);
		lua_settable(L, LUA_GLOBALSINDEX);

		lua_pushliteral(L, "__metatable");
		lua_pushvalue(L, methods);
		lua_settable(L, metatable);  // hide metatable from Lua getmetatable()

		lua_pushliteral(L, "__index");
		lua_pushvalue(L, methods);
		lua_settable(L, metatable);

		lua_pushliteral(L, "__tostring");
		lua_pushcfunction(L, tostring_T);
		lua_settable(L, metatable);

		// borisu added to recognize the class
		// see bridge macros for usage
		if (boost::is_base_of<T,ActiveObject>::value == true)
		{
			printf("ok");
		}

		lua_pushstring(L, "className");
		lua_pushstring(L, T::className);
		lua_settable(L, metatable);

		lua_pushstring(L, "activeobject");
		lua_pushboolean(L, is_base_of<luaobject,T>::value);
		lua_settable(L, metatable);
		
		
		if (luartcontrol | LUA_RT_ALLOW_GC)
		{
			lua_pushliteral(L, "__gc");
			lua_pushcfunction(L, gc_T);
			lua_settable(L, metatable);
		}

		if (luartcontrol | LUA_RT_ALLOW_NEW)
		{
			lua_newtable(L);                // mt for method table
			int mt = lua_gettop(L);
			lua_pushliteral(L, "__call");
			lua_pushcfunction(L, creator == NULL ? new_T : creator);
			lua_pushliteral(L, "new");
			lua_pushvalue(L, -2);           // dup new_T function
			lua_settable(L, methods);       // add new_T to method table
			lua_settable(L, mt);            // mt.__call = new_T
			lua_setmetatable(L, methods);
		}
		
		

		// fill method table with methods from class T
		for (RegType *l = T::methods; l->name; l++) {
			/* edited by Snaily: shouldn't it be const RegType *l ... ? */
			lua_pushstring(L, l->name);
			lua_pushlightuserdata(L, (void*)l);
			lua_pushcclosure(L, thunk, 1);
			lua_settable(L, methods);
		}

		lua_pop(L, 2);  // drop metatable and method table
	}

	// get userdata from Lua stack and return pointer to T object
	static T *check(lua_State *L, int narg) {
		userdataType *ud =
			static_cast<userdataType*>(luaL_checkudata(L, narg, T::className));
		if(!ud) luaL_typerror(L, narg, T::className);
		return static_cast<T*>(ud->pT);  // pointer to T object
	}

private:
	Luna();  // hide default constructor

	// member function dispatcher
	static int thunk(lua_State *L) {
		// stack has userdata, followed by method args
		T *obj = check(L, 1);  // get 'self', or if you prefer, 'this'
		lua_remove(L, 1);  // remove self so member function args start at index 1
		// get member function from upvalue
		RegType *l = static_cast<RegType*>(lua_touserdata(L, lua_upvalueindex(1)));
		return (obj->*(l->mfunc))(L);  // call member function
	}

	// create a new T object and
	// push onto the Lua stack a userdata containing a pointer to T object
	static int new_T(lua_State *L) {
		lua_remove(L, 1);   // use classname:new(), instead of classname.new()

		T *obj = new T(L);  // call constructor for T objects

		userdataType *ud =
			static_cast<userdataType*>(lua_newuserdata(L, sizeof(userdataType)));
		ud->pT = obj;  // store pointer to object in userdata
		ud->lo = static_cast<luaobject*>(obj);
		luaL_getmetatable(L, T::className);  // lookup metatable in Lua registry
		lua_setmetatable(L, -2);
		return 1;  // userdata containing pointer to T object
	}

	// garbage collection metamethod
	static int gc_T(lua_State *L) {
		userdataType *ud = static_cast<userdataType*>(lua_touserdata(L, 1));
		T *obj = static_cast<T*>(ud->pT);
		delete obj;  // call destructor for T objects
		return 0;
	}

	static int tostring_T (lua_State *L) {
		char buff[32];
		userdataType *ud = static_cast<userdataType*>(lua_touserdata(L, 1));
		T *obj = static_cast<T*>(ud->pT);
		sprintf_s(buff, 32,"%p", obj);
		lua_pushfstring(L, "%s (%s)", T::className, buff);
		return 1;
	}

	

};
