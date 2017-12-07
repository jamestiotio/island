#include "ApiLoader.h"

#include <dlfcn.h>
#include <link.h>
#include <iostream>

// declare function pointer type to register_fun function
typedef void ( *register_api_fun_p_t )( void * );

struct pal_api_loader_o {
	const char *        mApiName             = nullptr;
	const char *        mRegisterApiFuncName = nullptr;
	const char *        mPath                = nullptr;
	void *              mLibraryHandle       = nullptr;
	Pal_File_Watcher_o *mFileWatcher         = nullptr;
};

// ----------------------------------------------------------------------

static void unload_library( void *handle_, const char *path ) {
	if ( handle_ ) {
		auto result = dlclose( handle_ );
		// std::cout << "Closed library handle: " << std::hex << handle_ << std::endl;
		if ( result ) {
			std::cerr << "ERROR dlclose: " << dlerror() << std::endl;
		}
		auto handle = dlopen( path, RTLD_NOLOAD );
		if ( handle ) {
			std::cerr << "ERROR dlclose: "
			          << "handle " << std::hex << ( void * )handle << " staying resident.";
		}
		handle_ = nullptr;
	}
}

// ----------------------------------------------------------------------

static void *load_library( const char *lib_name ) {

	std::cout << "Loading Library    : '" << lib_name << "'" << std::endl;

	// We may pre-load any library dependencies so that these won't get deleted
	// when our main plugin gets reloaded.
	//
	// TODO: allow modules to specify resident library dependencies
	//
	// We manually load symbols for libraries upon which our plugins depend -
	// and make sure these are loaded with the NO_DELETE flag so that dependent
	// libraries will not be reloaded if a module which uses the library is unloaded.
	//
	// This is necessary since with linux linking against a library does not mean
	// its symbols are actually loaded, symbols are loaded lazily by default,
	// which means they are only loaded when the library is first used by the module
	// against which the library was linked.

	//	    static auto handleglfw = dlopen( "libglfw.so", RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE );
	//		static auto handlevk   = dlopen( "libvulkan.so", RTLD_NOW  | RTLD_GLOBAL | RTLD_NODELETE );

	void *handle = dlopen( lib_name, RTLD_NOW | RTLD_LOCAL );
	// std::cout << "Open library handle: " << std::hex << handle << std::endl;

	if ( !handle ) {
		auto loadResult = dlerror();
		std::cerr << "ERROR: " << loadResult << std::endl;
	}

	return handle;
}

// ----------------------------------------------------------------------

static bool load_library_persistent( const char *lib_name ) {
	void *lib_handle = dlopen( lib_name, RTLD_NOLOAD );
	if ( !lib_handle ) {
		lib_handle = dlopen( lib_name, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE );
		if ( !lib_handle ) {
			auto loadResult = dlerror();
			std::cerr << "ERROR: " << loadResult << std::endl;
		} else {
			std::cout << "Loaded library persistently: " << lib_name << ", handle:  " << std::hex << lib_handle << std::endl;
		}
	}
	return ( lib_handle != nullptr );
}

// ----------------------------------------------------------------------

static pal_api_loader_o *create( const char *path_ ) {
	pal_api_loader_o *tmp = new pal_api_loader_o{};
	tmp->mPath            = path_;
	return tmp;
};

// ----------------------------------------------------------------------

static void destroy( pal_api_loader_o *obj ) {
	unload_library( obj->mLibraryHandle, obj->mPath );
	delete obj;
};

// ----------------------------------------------------------------------

static bool load( pal_api_loader_o *obj ) {
	unload_library( obj->mLibraryHandle, obj->mPath );
	obj->mLibraryHandle = load_library( obj->mPath );
	return ( obj->mLibraryHandle != nullptr );
}

// ----------------------------------------------------------------------

static bool register_api( pal_api_loader_o *obj, void *api_interface, const char *register_api_fun_name ) {
	// define function pointer we will use to initialise api
	register_api_fun_p_t fptr;

	// load function pointer to initialisation method
	fptr = reinterpret_cast<register_api_fun_p_t>( dlsym( obj->mLibraryHandle, register_api_fun_name ) );
	if ( !fptr ) {
		std::cerr << "ERROR: " << dlerror() << std::endl;
		return false;
	}
	// Initialize the API. This means telling the API to populate function
	// pointers inside the struct which we are passing as parameter.
	std::cout << "Registering API via: '" << register_api_fun_name << "'" << std::endl;
	//	std::cout << "fptr                  = " << std::hex << (void*)fptr << std::endl;
	//	std::cout << "Api interface address = " << std::hex << api_interface << std::endl;
	( *fptr )( api_interface );
	return true;
}

// ----------------------------------------------------------------------

bool pal_register_api_loader_i( pal_api_loader_i *api ) {
	api->create                = create;
	api->destroy               = destroy;
	api->load                  = load;
	api->register_api          = register_api;
	api->loadLibraryPersistent = load_library_persistent;
	return true;
};

// ----------------------------------------------------------------------
// LINUX: these methods are for auditing library loading.
// to enable, start app with environment variable `LD_AUDIT` set to path of
// libpal_api_loader.so:
// EXPORT LD_AUDIT=./pal_api_loader/libpal_api_loader.so

extern "C" unsigned int
la_version( unsigned int version ) {
	std::cout << "\t AUDIT: loaded autiting interface" << std::endl;
	std::cout << std::flush;
	return version;
}

extern "C" unsigned int
la_objclose( uintptr_t *cookie ) {
	std::cout << "\t AUDIT: objclose: " << std::hex << cookie << std::endl;
	std::cout << std::flush;
	return 0;
}

extern "C" void
la_activity( uintptr_t *cookie, unsigned int flag ) {
	printf( "\t AUDIT: la_activity(): cookie = %p; flag = %s\n", cookie,
	        ( flag == LA_ACT_CONSISTENT ) ? "LA_ACT_CONSISTENT" : ( flag == LA_ACT_ADD ) ? "LA_ACT_ADD" : ( flag == LA_ACT_DELETE ) ? "LA_ACT_DELETE" : "???" );
	std::cout << std::flush;
};

extern "C" unsigned int
la_objopen( struct link_map *map, Lmid_t lmid, uintptr_t *cookie ) {
	printf( "\t AUDIT: la_objopen(): loading \"%s\"; lmid = %s; cookie=%p\n",
	        map->l_name,
	        ( lmid == LM_ID_BASE ) ? "LM_ID_BASE" : ( lmid == LM_ID_NEWLM ) ? "LM_ID_NEWLM" : "???",
	        cookie );
	std::cout << std::flush;
	return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}

extern "C" char *
la_objsearch( const char *name, uintptr_t *cookie, unsigned int flag ) {
	printf( "\t AUDIT: la_objsearch(): name = %s; cookie = %p", name, cookie );
	printf( "; flag = %s\n",
	        ( flag == LA_SER_ORIG ) ? "LA_SER_ORIG" : ( flag == LA_SER_LIBPATH ) ? "LA_SER_LIBPATH" : ( flag == LA_SER_RUNPATH ) ? "LA_SER_RUNPATH" : ( flag == LA_SER_DEFAULT ) ? "LA_SER_DEFAULT" : ( flag == LA_SER_CONFIG ) ? "LA_SER_CONFIG" : ( flag == LA_SER_SECURE ) ? "LA_SER_SECURE" : "???" );

	return const_cast<char *>( name );
}
