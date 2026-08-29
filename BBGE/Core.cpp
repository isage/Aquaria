/*
Copyright (C) 2007, 2010 - Bit-Blot

This file is part of Aquaria.

Aquaria is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "Core.h"
#include "Texture.h"
#include "AfterEffect.h"
#include "Particles.h"
#include "TTFFont.h"
#include "PerfLog.h"

#include <time.h>
#include <iostream>

#ifdef BBGE_BUILD_UNIX
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <assert.h>

#if __APPLE__
#include <Carbon/Carbon.h>
#endif

#if BBGE_BUILD_WINDOWS
#include <shlobj.h>
#include <direct.h>
#endif

static SDL_Window *gScreen=0;
static SDL_Renderer *gRenderer=0;

bool ignoreNextMouse=false;
Vector unchange;

#ifdef BBGE_BUILD_VFS
#include "ttvfs.h"
#endif

Core *core = 0;

#ifdef BBGE_BUILD_WINDOWS
	HICON icon_windows = 0;
#endif

#if !defined KMOD_GUI && !SDL_VERSION_ATLEAST(2, 0, 14)
	#define KMOD_GUI KMOD_META
#endif

void Core::initIcon()
{
#ifdef BBGE_BUILD_WINDOWS
	HINSTANCE handle = ::GetModuleHandle(NULL);
	//if (icon_windows)
	//	::DestroyIcon(icon_windows);

	icon_windows = ::LoadIcon(handle, "icon");

	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version)
	if (SDL_GetWindowWMInfo(gScreen, &wminfo) != 1)
	{
		//errorLog("wrong SDL version");
		// error: wrong SDL version
	}

	HWND hwnd = wminfo.info.win.window;

	::SetClassLong(hwnd, GCL_HICON, (LONG) icon_windows);
#endif
}

void Core::resetCamera()
{
	cameraPos = Vector(0,0);
}

ParticleEffect* Core::createParticleEffect(const std::string &name, const Vector &position, int layer, float rotz)
{
	ParticleEffect *e = new ParticleEffect();
	e->load(name);
	e->position = position;
	e->start();
	e->setDie(true);
	e->rotation.z = rotz;
	core->getTopStateData()->addRenderObject(e, layer);
	return e;
}

void Core::unloadDevice()
{
	for (int i = 0; i < renderObjectLayers.size(); i++)
	{
		RenderObjectLayer *r = &renderObjectLayers[i];
		RenderObject *robj = r->getFirst();
		while (robj)
		{
			robj->unloadDevice();
			robj = r->getNext();
		}
	}
	frameBuffer.unloadDevice();

	if (afterEffectManager)
		afterEffectManager->unloadDevice();
}

void Core::reloadDevice()
{
	for (int i = 0; i < renderObjectLayers.size(); i++)
	{
		RenderObjectLayer *r = &renderObjectLayers[i];
		r->reloadDevice();
		RenderObject *robj = r->getFirst();
		while (robj)
		{
			robj->reloadDevice();
			robj = r->getNext();
		}
	}
	frameBuffer.reloadDevice();

	if (afterEffectManager)
		afterEffectManager->reloadDevice();
}

void Core::resetGraphics(int w, int h, int fullscreen, int vsync, int bpp)
{
	if (fullscreen == -1)
		fullscreen = _fullscreen;

	if (vsync == -1)
		vsync = _vsync;

	if (w == -1)
		w = width;

	if (h == -1)
		h = height;

	if (bpp == -1)
		bpp = _bpp;

	unloadDevice();
	unloadResources();

	shutdownGraphicsLibrary();

	initGraphicsLibrary(w, h, fullscreen, vsync, bpp);
	
	enable2DWide(w, h);

	reloadResources();
	reloadDevice();


	resetTimer();
}

void Core::toggleScreenMode(int t)
{
	sound->pause();
	resetGraphics(-1, -1, t);
	cacheRender();
	resetTimer();
	sound->resume();
}

void Core::updateCursorFromJoystick(float dt, int spd)
{
	//debugLog("updating mouse from joystick");

	core->mouse.position += joystick.position*dt*spd;

/*
	if (!joystick.position.isZero())
		setMousePosition(core->mouse.position);
	*/

	doMouseConstraint();
}

void Core::setWindowCaption(const std::string &caption, const std::string &icon)
{
    if (gScreen)
        SDL_SetWindowTitle(gScreen, caption.c_str());
}

RenderObjectLayer *Core::getRenderObjectLayer(int i)
{
	if (i == LR_NONE)
		return 0;
	return &renderObjectLayers[i];
}

void Core::translateMatrixStack(float x, float y, float z)
{
	core->transform.translate(x, y, z);
}

void Core::scaleMatrixStack(float x, float y, float z)
{
	core->transform.scale(x, y, z);
}

void Core::rotateMatrixStack(float x, float y, float z)
{
	core->transform.rotate(0, 0, 1, z);
}

void Core::applyMatrixStackToWorld()
{
}

void Core::rotateMatrixStack(float z)
{
	core->transform.rotate(0, 0, 1, z);
}

bool Core::getShiftState()
{
	return getKeyState(KEY_LSHIFT) || getKeyState(KEY_RSHIFT);
}

bool Core::getAltState()
{
	return getKeyState(KEY_LALT) || getKeyState(KEY_RALT);
}

bool Core::getCtrlState()
{
	return getKeyState(KEY_LCONTROL) || getKeyState(KEY_RCONTROL);
}

bool Core::getMetaState()
{
	return getKeyState(KEY_LMETA) || getKeyState(KEY_RMETA);
}

void Core::errorLog(const std::string &s)
{
	messageBox("Error!", s);
	debugLog(s);
}

void Core::messageBox(const std::string &title, const std::string &msg)
{
	::messageBox(title, msg);
}

void Core::debugLog(const std::string &s)
{
    std::string log = "[" + std::to_string(SDL_GetTicks()) + "] " + s;
	if (debugLogActive)
	{
		_logOut << log << std::endl;
	}
#ifdef _DEBUG
	std::cout << log << std::endl;
#endif
}

#ifdef BBGE_BUILD_WINDOWS
static bool checkWritable(const std::string& path, bool warn, bool critical)
{
	bool writeable = false;
	std::string f = path + "/~chk_wrt.tmp";
	FILE *fh = fopen(f.c_str(), "w");
	if(fh)
	{
		writeable = fwrite("abcdef", 5, 1, fh) == 1;
		fclose(fh);
		unlink(f.c_str());
	}
	if(!writeable)
	{
		if(warn)
		{
			std::ostringstream os;
			os << "Trying to use \"" << path << "\" as user data path, but it is not writeable.\n"
				<< "Please make sure the game is allowed to write to that directory.\n"
				<< "You can move the game to another location and run it there,\n"
				<< "or try running it as administrator, that may help as well.";
			if(critical)
				os << "\n\nWill now exit.";
			MessageBoxA(NULL, os.str().c_str(), "Need to write but can't!", MB_OK | MB_ICONERROR);
		}
		if(critical)
			exit(1);
	}
	return writeable;
}
#endif


const float SORT_DELAY = 10;
Core::Core(const std::string &filesystem, const std::string& extraDataDir, int numRenderLayers, const std::string &appName, int particleSize, std::string userDataSubFolder)
: ActionMapper(), StateManager(), appName(appName)
{
	sound = NULL;
	screenCapScale = Vector(1,1,1);
	timeUpdateType = TIMEUPDATE_DYNAMIC;
	_extraDataDir = extraDataDir;

	fixedFPS = 60;

	if (userDataSubFolder.empty())
		userDataSubFolder = appName;
		
#if defined(BBGE_BUILD_UNIX)
	const char *envr = getenv("HOME");
	if (envr == NULL)
        envr = ".";  // oh well.
	const std::string home(envr);

	createDir(home);  // just in case.

	// "/home/icculus/.Aquaria" or something. Spaces are okay.
	#ifdef BBGE_BUILD_MACOSX
	const std::string prefix("Library/Application Support/");
	#else
	const std::string prefix(".");
	#endif

	userDataFolder = home + "/" + prefix + userDataSubFolder;
	createDir(userDataFolder);
	debugLogPath = userDataFolder + "/";
	createDir(userDataFolder + "/screenshots");
	std::string prefpath(getPreferencesFolder());
	createDir(prefpath);

#else
	debugLogPath = "";
	userDataFolder = ".";

	#ifdef BBGE_BUILD_WINDOWS
	{
		if(checkWritable(userDataFolder, true, true)) // working dir?
		{
			puts("Using working directory as user directory.");
		}
		// TODO: we may want to use a user-specific path under windows as well
		// if the code below gets actually used, pass 2x false to checkWritable() above.
		// not sure about this right now -- FG
		/*else
		{
			puts("Working directory is not writable...");
			char pathbuf[MAX_PATH];
			if(SHGetSpecialFolderPathA(NULL, &pathbuf[0], CSIDL_APPDATA, 0))
			{
				userDataFolder = pathbuf;
				userDataFolder += '/';
				userDataFolder += userDataSubFolder;
				for(uint32 i = 0; i < userDataFolder.length(); ++i)
					if(userDataFolder[i] == '\\')
						userDataFolder[i] = '/';
				debugLogPath = userDataFolder + "/";
				puts(("Using \"" + userDataFolder + "\" as user directory.").c_str());
				createDir(userDataFolder);
				checkWritable(userDataFolder, true, true);
			}
			else
				puts("Failed to retrieve appdata path, using working dir."); // too bad, but can't do anything about it
		}
		*/
	}
	#endif
#endif

	_logOut.open((debugLogPath + "debug.log").c_str());
	debugLogActive = true;

	debugLogTextures = true;
	
	grabInputOnReentry = -1;

	srand(time(NULL));
	old_dt = 0;
	current_dt = 0;

	aspectX = 4;
	aspectY = 3;

	virtualOffX = virtualOffY = 0;
	auxiliaryCaptureNeeded = false;
	vw2 = 0;
	vh2 = 0;

	viewOffX = viewOffY = 0;

	/*
	aspectX = 1440;  //4.0f;
	aspectY = 900;   //3.0f;
	*/

	particleManager = new ParticleManager(particleSize);
	nowTicks = thenTicks = 0;
	_hasFocus = false;
	lib_graphics = lib_sound = lib_input = false;
	clearColor = Vector(0,0,0);
	updateCursorFromMouse = true;
	mouseConstraint = false;
	mouseCircle = 0;
	overrideStartLayer = 0;
	overrideEndLayer = 0;
	coreVerboseDebug = false;
	frameOutputMode = false;
	updateMouse = true;
	particlesPaused = false;
	joystickAsMouse = false;
	currentLayerPass = 0;
	flipMouseButtons = 0;
	joystickOverrideMouse = false;
	joystickEnabled = false;
	doScreenshot = false;
	baseCullRadius = 1;
	width = height = 0;
	afterEffectManagerLayer = 0;
	renderObjectLayers.resize(1);
	invGlobalScale = 1.0;
	invGlobalScaleSqr = 1.0;
	renderObjectCount = 0;
	avgFPS.resize(1);
	minimized = false;
	sortFlag = true;
	sortTimer = SORT_DELAY;
	numSavedScreenshots = 0;
	shuttingDown = false;
	clearedGarbageFlag = false;
	nestedMains = 0;
	afterEffectManager = 0;
	loopDone = false;
	core = this;

	#ifdef BBGE_BUILD_WINDOWS
		hRC = 0;
		hDC = 0;
		hWnd = 0;
	#endif

	for (int i = 0; i < KEY_MAXARRAY; i++)
	{
		keys[i] = 0;
	}

	aspect = (aspectX/aspectY);//320.0f/240.0f;
	//1.3333334f;

	globalResolutionScale = globalScale = Vector(1,1,1);

	initRenderObjectLayers(numRenderLayers);

	initPlatform(filesystem);
}

void Core::initPlatform(const std::string &filesystem)
{
#if defined(BBGE_BUILD_MACOSX) && !defined(BBGE_BUILD_MACOSX_NOBUNDLEPATH)
	// FIXME: filesystem not handled
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	//CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
	CFURLRef resourcesURL = CFBundleCopyBundleURL(mainBundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
	{
		// error!
		debugLog("CFURLGetFileSystemRepresentation");
	}
	CFRelease(resourcesURL);
	debugLog(path);
	chdir(path);
#elif defined(BBGE_BUILD_UNIX)
	if (!filesystem.empty())
	{
		if (chdir(filesystem.c_str()) == 0)
			return;
		else
			debugLog("Failed to chdir to filesystem path " + filesystem);
	}
#ifdef BBGE_DATA_PREFIX
	if (chdir(BBGE_DATA_PREFIX) == 0 && chdir(appName.c_str()) == 0)
		return;
	else
		debugLog("Failed to chdir to filesystem path " BBGE_DATA_PREFIX + appName);
#endif
	char path[PATH_MAX];
	// always a symlink to this process's binary, on modern Linux systems.
	const ssize_t rc = readlink("/proc/self/exe", path, sizeof (path));
	if ( (rc == -1) || (rc >= sizeof (path)) )
	{
		// error!
		debugLog("readlink");
	}
	else
	{
		path[rc] = '\0';
		char *ptr = strrchr(path, '/');
		if (ptr != NULL)
		{
			*ptr = '\0';
			debugLog(path);
			if (chdir(path) != 0)
				debugLog("Failed to chdir to executable path" + std::string(path));
		}
	}
#endif
#ifdef BBGE_BUILD_WINDOWS
	if(filesystem.length())
	{
		if(_chdir(filesystem.c_str()) != 0)
		{
			debugLog("chdir failed: " + filesystem);
		}
	}
	// FIXME: filesystem not handled
#endif
}

std::string Core::getPreferencesFolder()
{
#ifdef BBGE_BUILD_UNIX
	return userDataFolder + "/preferences";
#endif
#ifdef BBGE_BUILD_WINDOWS
	return "";
#endif
}

std::string Core::getUserDataFolder()
{
	return userDataFolder;
}

#if BBGE_BUILD_UNIX
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// based on code I wrote for PhysicsFS: http://icculus.org/physfs/
//  the zlib license on physfs allows this cut-and-pasting.
static int locateOneElement(char *buf)
{
	char *ptr;
	DIR *dirp;

	if (access(buf, F_OK) == 0)
		return(1);  // quick rejection: exists in current case.

	ptr = strrchr(buf, '/');  // find entry at end of path.
	if (ptr == NULL)
	{
		dirp = opendir(".");
		ptr = buf;
	}
	else
	{
		*ptr = '\0';
		dirp = opendir(buf);
		*ptr = '/';
		ptr++;  // point past dirsep to entry itself.
	}

	struct dirent *dent;
	while ((dent = readdir(dirp)) != NULL)
	{
		if (strcasecmp(dent->d_name, ptr) == 0)
		{
			strcpy(ptr, dent->d_name); // found a match. Overwrite with this case.
			closedir(dirp);
			return(1);
		}
	}

	// no match at all...
	closedir(dirp);
	return(0);
}
#endif


std::string Core::adjustFilenameCase(const char *_buf)
{
#ifdef BBGE_BUILD_UNIX  // any case is fine if not Linux.
	int rc = 1;
	char *buf = (char *) alloca(strlen(_buf) + 1);
	strcpy(buf, _buf);

	char *ptr = buf;
	while ((ptr = strchr(ptr + 1, '/')) != 0)
	{
		*ptr = '\0';  // block this path section off
		rc = locateOneElement(buf);
		*ptr = '/'; // restore path separator
		if (!rc)
			break;  // missing element in path.
	}

	// check final element...
	if (rc)
		rc = locateOneElement(buf);

	#if 0
	if (strcmp(_buf, buf) != 0)
	{
		fprintf(stderr, "Corrected filename case: '%s' => '%s (%s)'\n",
		        _buf, buf, rc ? "found" : "not found");
	}
	#endif

	return std::string(buf);
#else
	return std::string(_buf);
#endif
}


Core::~Core()
{
	if (particleManager)
	{
		delete particleManager;
	}
	if (sound)
	{
		delete sound;
		sound = 0;
	}
	debugLog("~Core()");
	_logOut.close();
	core = 0;
}

bool Core::hasFocus()
{
	return _hasFocus;
}

void Core::setInputGrab(bool on)
{
	if (isWindowFocus())
	{
//		SDL_SetWindowMouseGrab(gScreen, on ? true : false);
//		SDL_SetWindowKeyboardGrab(gScreen, on ? true : false);
	}
}

void Core::setReentryInputGrab(int on)
{
	if (grabInputOnReentry == -1)
	{
		setInputGrab(on);
	}
	else
	{
		setInputGrab(grabInputOnReentry);
	}
}

bool Core::isFullscreen()
{
	return _fullscreen;
}

bool Core::isShuttingDown()
{
	return shuttingDown;
}

void Core::init()
{
	setupFileAccess();

	flags.set(CF_CLEARBUFFERS);
	quitNestedMainFlag = false;

	if(!SDL_Init(0))
	{
		exit_error("Failed to init SDL");
	}
	

	loopDone = false;
	clearedGarbageFlag = false;

	initInputCodeMap();

	initLocalization();

}

void Core::initRenderObjectLayers(int num)
{
	renderObjectLayers.resize(num);
	renderObjectLayerOrder.resize(num);
	for (int i = 0; i < num; i++)
	{
		renderObjectLayerOrder[i] = i;
	}
}

bool Core::initSoundLibrary(const std::string &defaultDevice)
{
	debugLog("Creating SoundManager");
	sound = new SoundManager(defaultDevice);
	debugLog("Done");
	return sound != 0;
}

Vector Core::getGameCursorPosition()
{
	return getGamePosition(mouse.position);
}

Vector Core::getGamePosition(const Vector &v)
{
	return cameraPos + (v * invGlobalScale);
}

bool Core::getMouseButtonState(int m)
{
	int mcode=m;

	switch(m)
	{
	case 0: mcode=1; break;
	case 1: mcode=3; break;
	case 2: mcode=2; break;
	}

	SDL_MouseButtonFlags mousestate = SDL_GetMouseState(NULL,NULL);

	return mousestate & SDL_BUTTON_MASK(mcode);
}

bool Core::getKeyState(int k)
{
	if (k >= KEY_MAXARRAY || k < 0)
	{
		return 0;
	}
	return keys[k];
}


Vector joychange;
Vector lastjoy;
void readJoystickData()
{
}

void readMouseData()
{
}

void readKeyData()
{

}
//#endif


bool Core::initJoystickLibrary(int numSticks)
{
	//joystickEnabled = false;
	SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD);

	if (numSticks > 0)
		joystick.init(0);

	joystickEnabled = true;
	/*
	numJoysticks = Joystick::deviceCount();
	std::ostringstream os;
	os << "Found " << numJoysticks << " joysticks";
	debugLog(os.str());
	if (numJoysticks > 0)
	{
		if (numJoysticks > 4)
			numJoysticks = 4;

		// HACK: memory leak... add code to clean this up!
		for (int i = 0; i < numJoysticks; i++) {
			joysticks[i] = new Joystick(i);
			joysticks[i]->open();

			// Print the name of the joystick.
			char name[MAX_PATH];
			joysticks[i]->deviceName(name);
			std::ostringstream os;
			os << "   Joystick " << i << ": " << name;
			debugLog(os.str());
		}
		joystickEnabled = true;
		return true;
	}
	*/

	return true;
}

bool Core::initInputLibrary()
{
	core->mouse.position = Vector(getWindowWidth()/2, getWindowHeight()/2);

	for (int i = 0; i < KEY_MAXARRAY; i++)
	{
		keys[i] = 0;
	}

	return true;
}

void Core::onUpdate(float dt)
{
	if (minimized) return;
	ActionMapper::onUpdate(dt);
	StateManager::onUpdate(dt);


	core->mouse.lastPosition = core->mouse.position;
	core->mouse.lastScrollWheel = core->mouse.scrollWheel;

	readKeyData();
	readMouseData();
	readJoystickData();
	pollEvents();
	joystick.update(dt);






	/*
	std::ostringstream os;
	os << "x: " << joystate.lX << " y: " << joystate.lY;
	os << " frx: " << joystate.lFRx << " fry: " << joystate.lFRy;
	debugLog(os.str());
	*/

	/*
	if (joystickOverrideMouse && !joychange.isZero())
	{
		Vector joy(joystate.lX, joystate.lY);
		//core->mouse.position += joychange * 0.001f;
		core->mouse.position = Vector(400,300) + ((joy * 600) / (65536/2))-300;
	}
	*/


	/*

	*/


	/*
	if (mouse.position.x < 0)
		mouse.position.x = 0;
	if (mouse.position.x > core->getVirtualWidth())
		mouse.position.x = core->getVirtualWidth();
	if (mouse.position.y < 0)
		mouse.position.y = 0;
	if (mouse.position.y > core->getVirtualHeight())
		mouse.position.y = core->getVirtualHeight();
	*/

	onMouseInput();

	//core->mouse.change = core->mouse.position - core->mouse.lastPosition;

	//core->mouse.scrollWheelChange = core->mouse.scrollWheel - core->mouse.lastScrollWheel;





	//script.update(dt);

	globalScale.update(dt);
	core->globalScaleChanged();

	if (afterEffectManager)
	{
		afterEffectManager->update(dt);
	}

	if (!sortFlag)
	{
		if (sortTimer>0)
		{
			sortTimer -= dt;
			if (sortTimer <= 0)
			{
				sortTimer = SORT_DELAY;
				sort();
			}
		}
	}
}

void Core::globalScaleChanged()
{
	invGlobalScale = 1.0f/globalScale.x;
	invGlobalScaleSqr = invGlobalScale * invGlobalScale;
}

Vector Core::getClearColor()
{
	return clearColor;
}

void Core::setClearColor(const Vector &c)
{
	clearColor = c;
}

unsigned int Core::dbg_numRenderCalls = 0;


bool Core::initGraphicsLibrary(int width, int height, bool fullscreen, int vsync, int bpp, bool recreate)
{	
	static bool didOnce = false;
	
	aspectX = width;
	aspectY = height;

	aspect = (aspectX/aspectY);

	

	this->width = width;
	this->height = height;
	_vsync = vsync;
	_fullscreen = fullscreen;
	_bpp = bpp;

	_hasFocus = false;

	//setenv("SDL_VIDEO_CENTERED", "1", 1);
	//SDL_putenv("SDL_VIDEO_WINDOW_POS=400,300");

	if (recreate)
	{
		if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
		{
			exit_error(std::string("SDL Error: ") + std::string(SDL_GetError()));
		}
	}

	setWindowCaption(appName, appName);

	initIcon();
    // Create window

	//if (!didOnce)
	{
		Uint64 flags = 0;
		if (fullscreen)
			flags |= SDL_WINDOW_FULLSCREEN;
		gScreen = SDL_CreateWindow(appName.c_str(), width, height, flags);
		if (gScreen == NULL)
		{
			std::ostringstream os;
			os << "Couldn't set resolution [" << width << "x" << height << "]\n" << SDL_GetError();
			errorLog(os.str());
			SDL_Quit();
			exit(0);
		}
		SDL_SetWindowPosition(gScreen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

		gRenderer = SDL_CreateRenderer(gScreen, NULL);
		if (gRenderer == NULL)
		{
			std::ostringstream os;
			os << "Couldn't create SDL renderer!\n" << SDL_GetError();
			errorLog(os.str());
			SDL_Quit();
			exit(0);
		}

		if (!TTF_Init())
		{
			std::ostringstream os;
			os << "Couldn't init SDL_ttf!\n" << SDL_GetError();
			errorLog(os.str());
		}
	}

	setWindowCaption(appName, appName);

	SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
	SDL_RenderClear(gRenderer);
	SDL_RenderPresent(gRenderer);
	SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
	SDL_RenderClear(gRenderer);
	SDL_RenderPresent(gRenderer);
	const char *name = SDL_GetCurrentVideoDriver();
//	SDL_SetWindowMouseGrab(gScreen, true);
//	SDL_SetWindowKeyboardGrab(gScreen, true);

	std::ostringstream os2;
	os2 << "Video Driver Name [" << name << "]";
	debugLog(os2.str());

	SDL_HideCursor();
	SDL_PumpEvents();

	for (int i = 0; i < KEY_MAXARRAY; i++)
	{
		keys[i] = 0;
	}

	core->loadBaseTransform();

	setClearColor(clearColor);
	
	clearBuffers();
	showBuffer();

	lib_graphics = true;

	_hasFocus = true;

	enumerateScreenModes();

	if (!didOnce)
		didOnce = true;

	// init success
	return true;
}

void Core::enumerateScreenModes()
{
	screenModes.clear();
    SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    int modecount = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(primaryDisplay, &modecount);
    if (!modes || modecount == 0) {
        debugLog("No modes available!");
        if (modes) SDL_free(modes);
        return;
    }
    for (int i = 0; i < modecount; i++) {
        const SDL_DisplayMode* mode = modes[i];
        if (mode->w && mode->h && (mode->w > mode->h))
        {
            screenModes.push_back(ScreenMode(i, mode->w, mode->h, std::ceil(mode->refresh_rate)));
        }
    }
    SDL_free(modes);
}

void Core::shutdownSoundLibrary()
{
}

void Core::shutdownGraphicsLibrary(bool killVideo)
{
	if (killVideo) {
		SDL_SetWindowMouseGrab(gScreen, false);
		SDL_SetWindowKeyboardGrab(gScreen, false);
		ttfShutdown();
		if (gRenderer)
		{
			SDL_DestroyRenderer(gRenderer);
			gRenderer = 0;
		}
		SDL_DestroyWindow(gScreen);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);

		gScreen = 0;
	}

	_hasFocus = false;

	lib_graphics = false;

#ifdef BBGE_BUILD_WINDOWS
	if (icon_windows)
	{
		::DestroyIcon(icon_windows);
		icon_windows = 0;
	}
#endif

}

void Core::quit()
{
	enqueueJumpState("STATE_QUIT");
	//loopDone = true;
	//popAllStates();
}

void Core::applyState(const std::string &state)
{
	if (nocasecmp(state, "state_quit")==0)
	{
		loopDone = true;
	}
	StateManager::applyState(state);
}

#ifdef BBGE_BUILD_WINDOWS
void centerWindow(HWND hwnd)
{
    int x, y;
    HWND hwndDeskTop;
    RECT rcWnd, rcDeskTop;
    // Get a handle to the desktop window
    hwndDeskTop = ::GetDesktopWindow();
    // Get dimension of desktop in a rect
    ::GetWindowRect(hwndDeskTop, &rcDeskTop);
    // Get dimension of main window in a rect
    ::GetWindowRect(hwnd, &rcWnd);
    // Find center of desktop
	x = (rcDeskTop.right - rcDeskTop.left)/2;
	y = (rcDeskTop.bottom - rcDeskTop.top)/2;
    x -= (rcWnd.right - rcWnd.left)/2;
	y -= (rcWnd.bottom - rcWnd.top)/2;
    // Set top and left to center main window on desktop
    ::SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
//	::ShowWindow(hwnd, 1);
}
#endif

bool Core::createWindow(int width, int height, int bits, bool fullscreen, std::string windowTitle)
{
	this->width = width;
	this->height = height;

	redBits = greenBits = blueBits = alphaBits = 0;
	return true;
}

// No longer part of C/C++ standard
#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

void Core::setPixelScale(int pixelScaleX, int pixelScaleY)
{
	/*
	piScaleX = pixelScaleX;
	piScaleY = pixelScaleY;
	*/
	virtualWidth = pixelScaleX;
	//MAX(virtualWidth, 800);
	virtualHeight = pixelScaleY;//int((pixelScale*aspectY)/aspectX);					//assumes 4:3 aspect ratio
	this->baseCullRadius = 1.1f * sqrtf(sqr(getVirtualWidth()/2) + sqr(getVirtualHeight()/2));

	std::ostringstream os;
	os << "virtual(" << virtualWidth << ", " << virtualHeight << ")";
	debugLog(os.str());
	
	vw2 = virtualWidth/2;
	vh2 = virtualHeight/2;

	center = Vector(baseVirtualWidth/2, baseVirtualHeight/2);


	virtualOffX = 0;
	virtualOffY = 0;

	int diff = 0;

	diff = virtualWidth-baseVirtualWidth;
	if (diff > 0)
		virtualOffX = ((virtualWidth-baseVirtualWidth)/2);
	else
		virtualOffX = 0;


	diff = virtualHeight-baseVirtualHeight;
	if (diff > 0)
		virtualOffY = ((virtualHeight-baseVirtualHeight)/2);
	else
		virtualOffY = 0;
}

// forcePixelScale used by Celu

void Core::enable2DWide(int rx, int ry)
{
	float aspect = float(rx) / float(ry);
	if (aspect >= 1.3f)
	{
		int vw = int(float(baseVirtualHeight) * (float(rx)/float(ry)));
		//vw = MAX(vw, baseVirtualWidth);
		core->enable2D(vw, baseVirtualHeight, 1);
	}
	else
	{
		int vh = int(float(baseVirtualWidth) * (float(ry)/float(rx)));
		//vh = MAX(vh, baseVirtualHeight);
		core->enable2D(baseVirtualWidth, vh, 1);
	}
}

void Core::enable2D(int pixelScaleX, int pixelScaleY, bool forcePixelScale)
{
    int vpw = width, vph = height;
    if (gScreen)
        SDL_GetWindowSizeInPixels(gScreen, &vpw, &vph);
    int viewPort[4] = { 0, 0, vpw, vph };

	float vw=0,vh=0;

	viewOffX = viewOffY = 0;

	float aspect = float(width)/float(height);

	if (aspect >= 1.4f)
	{
		vw = float(baseVirtualWidth * viewPort[3]) / float(baseVirtualHeight);

		viewOffX = (viewPort[2] - vw) * 0.5f;
	}
	else if (aspect < 1.3f)
	{
		vh = float(baseVirtualHeight * viewPort[2]) / float(baseVirtualWidth);

		viewOffY = (viewPort[3] - vh) * 0.5f;
	}



	if (forcePixelScale || (pixelScaleX!=0 && core->width!=pixelScaleX) || (pixelScaleY!=0 && core->height!=pixelScaleY))
	{
		float widthFactor = core->width/float(pixelScaleX);
		float heightFactor = core->height/float(pixelScaleY);
		//float heightFactor = 
		core->globalResolutionScale = Vector(widthFactor,heightFactor,1.0f);
		setPixelScale(pixelScaleX, pixelScaleY);

	}
	setPixelScale(pixelScaleX, pixelScaleY);
}

void Core::quitNestedMain()
{
	if (getNestedMains() > 1)
	{
		quitNestedMainFlag = true;
	}
}

void Core::resetTimer()
{
	nowTicks = thenTicks = SDL_GetTicks();

	for (int i = 0; i < avgFPS.size(); i++)
	{
		avgFPS[i] = 0;
	}
}

void Core::setDockIcon(const std::string &ident)
{
}

void Core::setMousePosition(const Vector &p)
{
	Vector lp = core->mouse.position;

	core->mouse.position = p;
	float px = p.x + virtualOffX;
	float py = p.y;// + virtualOffY;

	SDL_WarpMouseInWindow(gScreen, px * (float(width)/float(virtualWidth)), py * (float(height)/float(virtualHeight)));

	/*
	ignoreNextMouse = true;
	unchange = core->mouse.position - lp;
	*/

	/*
	std::ostringstream os;
	os << "setting position (" << p.x << ", " << p.y << ")";
	debugLog(os.str());
	*/
}

// used to update all render objects either uniformly or as part of a time sliced update process
void Core::updateRenderObjects(float dt)
{
	for (int c = 0; c < renderObjectLayers.size(); c++)
	{

		RenderObjectLayer *rl = &renderObjectLayers[c];

		if (!rl->update)
			continue;

		for (RenderObject *r = rl->getFirst(); r; r = rl->getNext())
		{
			r->update(dt);
		}
	}

	if (loopDone)
		return;

	if (clearedGarbageFlag)
	{
		clearedGarbageFlag = false;
	}
}

std::string Core::getEnqueuedJumpState()
{
	return this->enqueuedJumpState;
}

int screenshotNum = 0;
std::string getScreenshotFilename()
{
	while (true)
	{
		std::ostringstream os;
		os << core->getUserDataFolder() << "/screenshots/screen" << screenshotNum << ".tga";
		screenshotNum ++;
        std::string str(os.str());
		if (!core->exists(str))  // keep going until we hit an unused filename.
			return str;
	}
}

uint32 Core::getTicks()
{
	return SDL_GetTicks();
}

float Core::stopWatch(int d)
{
	if (d)
	{
		stopWatchStartTime = getTicks()/1000.0f;
		return stopWatchStartTime;
	}
	else
	{
		return (getTicks()/1000.0f) - stopWatchStartTime;
	}

	return 0;
}

bool Core::isWindowFocus()
{
	return ((SDL_GetWindowFlags(gScreen) & SDL_WINDOW_INPUT_FOCUS) != 0);
}

void Core::onBackgroundUpdate()
{
	SDL_Delay(200);
}

void Core::main(float runTime)
{
	bool verbose = coreVerboseDebug;
	if (verbose) debugLog("entered Core::main");
	// cannot nest loops when the game is over
	if (loopDone) return;

	//QueryPerformanceCounter((LARGE_INTEGER*)&lastTime);
	//QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
	float dt;
	float counter = 0;
	int frames = 0;
	float real_dt = 0;
	//std::ofstream out("debug.log");

#if (!defined(_DEBUG) || defined(BBGE_BUILD_UNIX))
	bool wasInactive = false;
#endif

	nowTicks = thenTicks = SDL_GetTicks();

	//int i;

	nestedMains++;
	// HACK: Why block this?
	/*
	if (nestedMains > 1 && runTime <= 0)
		return;
	*/

	while((runTime == -1 && !loopDone) || (runTime >0))									// Loop That Runs While done=FALSE
	{
		BBGE_PROF(Core_main);

		if (timeUpdateType == TIMEUPDATE_DYNAMIC)
		{
			nowTicks = SDL_GetTicks();
		}
		/*
		else
		{
			if (nowTicks == 0)
			{
				nowTicks = SDL_GetTicks();
			}
		}
		*/
		dt = (nowTicks-thenTicks)/1000.0;
		thenTicks = nowTicks;
		//thenTicks = SDL_GetTicks();

		if (verbose) debugLog("avgFPS");
		if (!avgFPS.empty())
		{
			/*
			if (avgFPS[0] <= 0)
			{
				for (int i = 0; i < avgFPS.size(); i++)
					avgFPS[i] = dt;
			}
			*/
			int i = 0;
			for (i = avgFPS.size()-1; i > 0; i--)
			{
				avgFPS[i] = avgFPS[i-1];
			}
			avgFPS[0] = dt;

			float c=0;
			int n = 0;
			for (i = 0; i < avgFPS.size(); i++)
			{
				if (avgFPS[i] > 0)
				{
					c += avgFPS[i];
					n ++;
				}
			}
			if (n > 0) // && n == avgFPS.size() ??
			{
				c /= n;
				dt = c;
			}
			/*
			std::ostringstream os;
			os << dt;
			debugLog(os.str());
			*/
		}

#if !defined(_DEBUG)
		if (verbose) debugLog("checking window active");

		if (lib_graphics && (wasInactive || !settings.runInBackground))
		{
			if (isWindowFocus())
			{
				_hasFocus = true;
				if (wasInactive)
				{
					debugLog("WINDOW ACTIVE");
					
					setReentryInputGrab(1);

					wasInactive = false;
				}
			}
			else
			{
				if (_hasFocus)
				{
					if (!wasInactive)
						debugLog("WINDOW INACTIVE");

					wasInactive = true;
					_hasFocus = false;

					setReentryInputGrab(0);

					sound->pause();

					core->joystick.rumble(0,0,0);

					while (!isWindowFocus())
					{
						pollEvents();
						//debugLog("app not in input focus");
						onBackgroundUpdate();

						resetTimer();
					}

					debugLog("app back in focus, reset");

					// Don't do this on Linux, it's not necessary and causes big stalls.
					//  We don't actually _lose_ the device like Direct3D anyhow.
					#if defined(BBGE_BUILD_WINDOWS)
					if (_fullscreen)
					{
						// calls reload device - reloadDevice()
						resetGraphics(width, height);
					}
					#endif

					resetTimer();

					sound->resume();

					resetTimer();
					
					SDL_HideCursor();

					continue;
				}
			}
		}
#endif

		if (timeUpdateType == TIMEUPDATE_FIXED)
		{
			real_dt = dt;
			dt = 1.0f/float(fixedFPS);
		}

		old_dt = dt;

		if (verbose) debugLog("modify dt");
		modifyDt(dt);

		current_dt = dt;

		if (verbose) debugLog("check runtime/quit");

		if (quitNestedMainFlag)
		{
			quitNestedMainFlag = false;
			break;
		}
		if (runTime>0)
		{
			runTime -= dt;
			if (runTime < 0)
				runTime = 0;
		}

		// UPDATE
		if (verbose) debugLog("post processing fx update");
		PerfLog::beginUpdate();
		postProcessingFx.update(dt);

		if (verbose) debugLog("update eventQueue");
		eventQueue.update(dt);

		if (verbose) debugLog("Update render objects");

		updateRenderObjects(dt);

		if (verbose) debugLog("Update particle manager");

		if (particleManager)
			particleManager->update(dt);

		if (verbose) debugLog("sound update");
		sound->update(dt);

		if (verbose) debugLog("onUpdate");
		onUpdate(dt);
		PerfLog::endUpdate();

		if (nestedMains == 1)
			clearGarbage();

		if (loopDone)
			break;

		updateCullData();

		dbg_numRenderCalls = 0;

		if (settings.renderOn)
		{
			if (verbose) debugLog("dark layer prerender");
			if (darkLayer.isUsed())
			{
				darkLayer.preRender();
			}

			// PerfLog begins here, not inside Core::render() itself -
			// render() can be called nested (DarkLayer::preRender() just
			// above does exactly that), and beginFrame()/endFrame() are
			// only meaningful wrapping the single outermost, actually-
			// presented frame per iteration of this loop.
			PerfLog::beginFrame();

			if (verbose) debugLog("render");
			// applySmartCaptureGating=true only here - this is
			// specifically the normal, per-frame, outermost render()
			// call. Every other call site in the codebase
			// (ScreenTransition::capture(), DarkLayer's nested call,
			// screenshot code) keeps the parameter at its default
			// (false), completely unaffected by Step 3's gating - see
			// the detailed reasoning at the capturing-decision site in
			// Core::render() itself.
			//
			// KNOWN, UNRESOLVED ISSUE (x86_64 Linux only, not this
			// project's target platform - confirmed by direct testing
			// across multiple SDL renderer backends on that platform,
			// Vulkan/OpenGL/software, all affected identically, and
			// confirmed fine on the actual target, PS Vita/32-bit ARM,
			// even with this gating enabled): enabling this parameter at
			// all - even though real diagnostic data confirmed its skip
			// branch never actually triggers in the specific test
			// session that surfaced this (BitBlotLogo.cpp adds a
			// ShockEffect at the very start of the intro, keeping
			// AfterEffectManager's effects list non-empty throughout,
			// correctly preventing a skip) - causes the intro to render
			// black until fading to white near the end, on that one
			// platform only. Root cause not found: verified the
			// suspected memory-safety explanations directly rather than
			// leaving them as assumptions - auxiliaryCaptureNeeded is
			// correctly initialized in Core::Core()'s constructor body
			// (not read before that runs), and AfterEffectManager's
			// `effects` is a direct (non-pointer) std::vector member,
			// which the C++ standard guarantees is properly default-
			// constructed the instant `new AfterEffectManager(...)`
			// runs, regardless of platform - ruling out the two most
			// plausible uninitialized-memory explanations. If x86_64
			// Linux testing shows the intro going black again, this is
			// the known, confirmed-architecture-specific cause and
			// exactly where to resume investigating; it does not affect
			// the target platform this project ships to.
			render(-1, -1, true, true);

			if (verbose) debugLog("showBuffer");
			showBuffer();

			PerfLog::endFrame();

			BBGE_PROF(STOP);

			if (verbose) debugLog("clearGarbage");
			if (nestedMains == 1)
				clearGarbage();


			if (verbose) debugLog("frame counter");
			frames++;

			counter += dt;
			if (counter > 1)
			{
				fps = frames;
				frames = counter = 0;
			}
		}

		sound->setListenerPos(screenCenter.x, screenCenter.y);

		if (doScreenshot)
		{
			if (verbose) debugLog("screenshot");

			doScreenshot = false;

			saveScreenshotTGA(getScreenshotFilename());
			prepScreen(0);
		}
		
		// wait
		if (timeUpdateType == TIMEUPDATE_FIXED)
		{
			static float avg_diff=0;
			static int avg_diff_count=0;

			float diff = (1.0f/float(fixedFPS)) - real_dt;

			avg_diff_count++;
			avg_diff += diff;
			
			char buf[256];
			sprintf(buf, "real_dt: %5.4f \n realFPS: %5.4f \n fixedFPS: %5.4f \n diff: %5.4f \n delay: %5.4f \n avgdiff: %5.8f", float(real_dt), float(real_dt>0?(1.0f/real_dt):0.0f), float(fixedFPS), float(diff), float(diff*1000), float(avg_diff/(float)avg_diff_count));
			fpsDebugString = buf;

			/*
			std::ostringstream os;
			os << "real_dt: " << real_dt << "\n realFPS: " << (1.0/real_dt) << "\n fixedFPS: " << fixedFPS << "\n diff: " << diff << "\n delay: " << diff*1000;
			fpsDebugString = os.str();
			*/

			nowTicks = SDL_GetTicks();
			
			if (diff > 0)
			{
				//Sleep(diff*1000);
				//SDL_Delay(diff*1000);
				while ((SDL_GetTicks() - nowTicks) < (diff*1000))
				{
					//wend, bitch
				}
			}

			//nowTicks = SDL_GetTicks();
		}	
	}
	if (verbose) debugLog("bottom of function");
	quitNestedMainFlag = false;
	if (nestedMains==1)
		clearGarbage();
	nestedMains--;
	if (verbose) debugLog("exit Core::main");
}

// less than through pointer
bool RenderObject_lt(RenderObject* x, RenderObject* y)
{
	return x->getSortDepth() < y->getSortDepth();
}

// greater than through pointer
bool RenderObject_gt(RenderObject* x, RenderObject* y)
{
	return x->getSortDepth() > y->getSortDepth();
}

void Core::sortLayer(int layer)
{
	if (layer >= 0 && layer < renderObjectLayers.size())
		renderObjectLayers[layer].sort();
}

void Core::sort()
{
	/*
	if (sortEnabled)
		renderObjects.sort(RenderObject_lt);
	*/
	// sort layeres independantly

	/*
	for (int i = renderObjects.size()-1; i >= 0; i--)
	{
		bool flipped = false;
		for (int j = 0; j < i; j++)
		{
			//position.z
			//position.z
			//!renderObjects[j]->parent && !renderObjects[j+1]->parent &&
			if (renderObjects[j]->getSortDepth() > renderObjects[j+1]->getSortDepth())
			{
				RenderObject *temp;
				temp = renderObjects[j];
				renderObjects[j] = renderObjects[j+1];
				renderObjects[j+1] = temp;
				flipped = true;
			}
		}
		if (!flipped) break;
	}
	*/

}

void Core::clearBuffers()
{
	if (flags.get(CF_CLEARBUFFERS))
	{
		if (gRenderer)
		{
			Uint8 r = (Uint8)(clearColor.x * 255);
			Uint8 g = (Uint8)(clearColor.y * 255);
			Uint8 b = (Uint8)(clearColor.z * 255);
			SDL_SetRenderDrawColor(gRenderer, r, g, b, 255);
			SDL_RenderClear(gRenderer);
		}
	}
}

void Core::setupRenderPositionAndScale()
{
	core->transform.scale(globalScale.x*globalResolutionScale.x*screenCapScale.x, globalScale.y*globalResolutionScale.y*screenCapScale.y, globalScale.z*globalResolutionScale.z);
	core->transform.translate(-(cameraPos.x+cameraOffset.x), -(cameraPos.y+cameraOffset.y), -(cameraPos.z+cameraOffset.z));
}


void Core::initFrameBuffer()
{
	frameBuffer.init(-1, -1, true);
}

void Core::setMouseConstraint(bool on)
{
/*
	if (mouseConstraint && !on)
	{
		setMousePosition(mouse.position);
	}
	*/
	mouseConstraint = on;
}

void Core::setMouseConstraintCircle(const Vector& pos, float circle)
{
	mouseConstraint = true;
	mouseCircle = circle;
	mouseConstraintCenter = pos;
	mouseConstraintCenter.z = 0;
}

/*
void Core::clearKeys()
{
	for (int i = 0; i < KEY_MAXARRAY; i++)
	{
		keys[i] = 0;
	}
}
*/

int Core::getVirtualOffX()
{
	return virtualOffX;
}

int Core::getVirtualOffY()
{
	return virtualOffY;
}

void Core::centerMouse()
{
	setMousePosition(Vector((virtualWidth/2) - core->getVirtualOffX(), virtualHeight/2));
}

bool Core::doMouseConstraint()
{
	if (mouseConstraint)
	{
		//- core->getVirtualOffX()
		//- virtualOffX
		Vector h = mouseConstraintCenter;
		Vector d = mouse.position - h;
		if (!d.isLength2DIn(mouseCircle))
		{
			d.setLength2D(mouseCircle);
			mouse.position = h+d;
			//warpMouse = true;
			return true;
		}
	}
	return false;
}

typedef std::map<SDL_Keycode,int> sdlKeyMap;

static sdlKeyMap *initSDLKeymap(void)
{
	sdlKeyMap *_retval = new sdlKeyMap;
	sdlKeyMap &retval = *_retval;

	#define SETKEYMAP(gamekey,sdlkey) retval[sdlkey] = gamekey

	SETKEYMAP(KEY_LSUPER, SDLK_LGUI);
	SETKEYMAP(KEY_RSUPER, SDLK_RGUI);
	SETKEYMAP(KEY_LMETA, SDLK_LGUI);
	SETKEYMAP(KEY_RMETA, SDLK_RGUI);
	SETKEYMAP(KEY_PRINTSCREEN, SDLK_PRINTSCREEN);
	SETKEYMAP(KEY_NUMPAD1, SDLK_KP_1);
	SETKEYMAP(KEY_NUMPAD2, SDLK_KP_2);
	SETKEYMAP(KEY_NUMPAD3, SDLK_KP_3);
	SETKEYMAP(KEY_NUMPAD4, SDLK_KP_4);
	SETKEYMAP(KEY_NUMPAD5, SDLK_KP_5);
	SETKEYMAP(KEY_NUMPAD6, SDLK_KP_6);
	SETKEYMAP(KEY_NUMPAD7, SDLK_KP_7);
	SETKEYMAP(KEY_NUMPAD8, SDLK_KP_8);
	SETKEYMAP(KEY_NUMPAD9, SDLK_KP_9);
	SETKEYMAP(KEY_NUMPAD0, SDLK_KP_0);

	SETKEYMAP(KEY_BACKSPACE, SDLK_BACKSPACE);

	//SETKEYMAP(KEY_CAPSLOCK, DIK_CAPSLOCK);
	//SETKEYMAP(KEY_CIRCUMFLEX, DIK_CIRCUMFLEX);
	SETKEYMAP(KEY_LALT, SDLK_LALT);
	SETKEYMAP(KEY_RALT, SDLK_RALT);
	SETKEYMAP(KEY_LSHIFT, SDLK_LSHIFT);
	SETKEYMAP(KEY_RSHIFT, SDLK_RSHIFT);
	SETKEYMAP(KEY_LCONTROL, SDLK_LCTRL);
	SETKEYMAP(KEY_RCONTROL, SDLK_RCTRL);
	SETKEYMAP(KEY_NUMPADMINUS, SDLK_KP_MINUS);
	SETKEYMAP(KEY_NUMPADPERIOD, SDLK_KP_PERIOD);
	SETKEYMAP(KEY_NUMPADPLUS, SDLK_KP_PLUS);
	SETKEYMAP(KEY_NUMPADSLASH, SDLK_KP_DIVIDE);
	SETKEYMAP(KEY_NUMPADSTAR, SDLK_KP_MULTIPLY);
	SETKEYMAP(KEY_PGDN, SDLK_PAGEDOWN);
	SETKEYMAP(KEY_PGUP, SDLK_PAGEUP);
	SETKEYMAP(KEY_APOSTROPHE, SDLK_APOSTROPHE);
	SETKEYMAP(KEY_EQUALS, SDLK_EQUALS);
	SETKEYMAP(KEY_SEMICOLON, SDLK_SEMICOLON);
	SETKEYMAP(KEY_LBRACKET, SDLK_LEFTBRACKET);
	SETKEYMAP(KEY_RBRACKET, SDLK_RIGHTBRACKET);
	SETKEYMAP(KEY_TILDE, SDLK_GRAVE);
	SETKEYMAP(KEY_0, SDLK_0);
	SETKEYMAP(KEY_1, SDLK_1);
	SETKEYMAP(KEY_2, SDLK_2);
	SETKEYMAP(KEY_3, SDLK_3);
	SETKEYMAP(KEY_4, SDLK_4);
	SETKEYMAP(KEY_5, SDLK_5);
	SETKEYMAP(KEY_6, SDLK_6);
	SETKEYMAP(KEY_7, SDLK_7);
	SETKEYMAP(KEY_8, SDLK_8);
	SETKEYMAP(KEY_9, SDLK_9);
	SETKEYMAP(KEY_A, SDLK_A);
	SETKEYMAP(KEY_B, SDLK_B);
	SETKEYMAP(KEY_C, SDLK_C);
	SETKEYMAP(KEY_D, SDLK_D);
	SETKEYMAP(KEY_E, SDLK_E);
	SETKEYMAP(KEY_F, SDLK_F);
	SETKEYMAP(KEY_G, SDLK_G);
	SETKEYMAP(KEY_H, SDLK_H);
	SETKEYMAP(KEY_I, SDLK_I);
	SETKEYMAP(KEY_J, SDLK_J);
	SETKEYMAP(KEY_K, SDLK_K);
	SETKEYMAP(KEY_L, SDLK_L);
	SETKEYMAP(KEY_M, SDLK_M);
	SETKEYMAP(KEY_N, SDLK_N);
	SETKEYMAP(KEY_O, SDLK_O);
	SETKEYMAP(KEY_P, SDLK_P);
	SETKEYMAP(KEY_Q, SDLK_Q);
	SETKEYMAP(KEY_R, SDLK_R);
	SETKEYMAP(KEY_S, SDLK_S);
	SETKEYMAP(KEY_T, SDLK_T);
	SETKEYMAP(KEY_U, SDLK_U);
	SETKEYMAP(KEY_V, SDLK_V);
	SETKEYMAP(KEY_W, SDLK_W);
	SETKEYMAP(KEY_X, SDLK_X);
	SETKEYMAP(KEY_Y, SDLK_Y);
	SETKEYMAP(KEY_Z, SDLK_Z);

	SETKEYMAP(KEY_LEFT, SDLK_LEFT);
	SETKEYMAP(KEY_RIGHT, SDLK_RIGHT);
	SETKEYMAP(KEY_UP, SDLK_UP);
	SETKEYMAP(KEY_DOWN, SDLK_DOWN);

	SETKEYMAP(KEY_DELETE, SDLK_DELETE);
	SETKEYMAP(KEY_SPACE, SDLK_SPACE);
	SETKEYMAP(KEY_RETURN, SDLK_RETURN);
	SETKEYMAP(KEY_PERIOD, SDLK_PERIOD);
	SETKEYMAP(KEY_MINUS, SDLK_MINUS);
	SETKEYMAP(KEY_CAPSLOCK, SDLK_CAPSLOCK);
	SETKEYMAP(KEY_SYSRQ, SDLK_SYSREQ);
	SETKEYMAP(KEY_TAB, SDLK_TAB);
	SETKEYMAP(KEY_HOME, SDLK_HOME);
	SETKEYMAP(KEY_END, SDLK_END);
	SETKEYMAP(KEY_COMMA, SDLK_COMMA);
	SETKEYMAP(KEY_SLASH, SDLK_SLASH);

	SETKEYMAP(KEY_F1, SDLK_F1);
	SETKEYMAP(KEY_F2, SDLK_F2);
	SETKEYMAP(KEY_F3, SDLK_F3);
	SETKEYMAP(KEY_F4, SDLK_F4);
	SETKEYMAP(KEY_F5, SDLK_F5);
	SETKEYMAP(KEY_F6, SDLK_F6);
	SETKEYMAP(KEY_F7, SDLK_F7);
	SETKEYMAP(KEY_F8, SDLK_F8);
	SETKEYMAP(KEY_F9, SDLK_F9);
	SETKEYMAP(KEY_F10, SDLK_F10);
	SETKEYMAP(KEY_F11, SDLK_F11);
	SETKEYMAP(KEY_F12, SDLK_F12);
	SETKEYMAP(KEY_F13, SDLK_F13);
	SETKEYMAP(KEY_F14, SDLK_F14);
	SETKEYMAP(KEY_F15, SDLK_F15);

	SETKEYMAP(KEY_ESCAPE, SDLK_ESCAPE);
	//SETKEYMAP(KEY_ANYKEY, 4059);
	//SETKEYMAP(KEY_MAXARRAY, SDLK_LAST+1

	#undef SETKEYMAP

	return _retval;
}

static int mapSDLKeyToGameKey(const SDL_Keycode val)
{
	static sdlKeyMap *keymap = NULL;
	if (keymap == NULL)
		keymap = initSDLKeymap();

	return (*keymap)[val];
}


void Core::pollEvents()
{
	bool warpMouse=false;

	/*
	Uint8 *keystate = SDL_GetKeyState(NULL);
	for (int i = 0; i < KEY_MAXARRAY; i++)
	{
		keys[i] = keystate[i];
	}
	*/

	if (updateMouse)
	{
		float x, y;
		SDL_MouseButtonFlags mousestate = SDL_GetMouseState(&x,&y);

		if (mouse.buttonsEnabled)
		{
			mouse.buttons.left		= mousestate & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)?DOWN:UP;
			mouse.buttons.right		= mousestate & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)?DOWN:UP;
			mouse.buttons.middle	= mousestate & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)?DOWN:UP;

			mouse.pure_buttons = mouse.buttons;

			if (flipMouseButtons)
			{
				std::swap(mouse.buttons.left, mouse.buttons.right);
			}
		}
		else
		{
			mouse.buttons.left = mouse.buttons.right = mouse.buttons.middle = UP;
		}

		mouse.scrollWheelChange = 0;
		mouse.change = Vector(0,0);
	}





	SDL_Event event;

	

	while ( SDL_PollEvent (&event) ) {
		switch (event.type) {
			case SDL_EVENT_KEY_DOWN:
			{
				#if __APPLE__
				if ((event.key.key == SDLK_Q) && (event.key.mod & SDL_KMOD_GUI))
				#else
				if ((event.key.key == SDLK_F4) && (event.key.mod & SDL_KMOD_ALT))
				#endif
				{
					quitNestedMain();
					quit();
				}

				if ((event.key.key == SDLK_G) && (event.key.mod & SDL_KMOD_CTRL))
				{
					// toggle mouse grab with the magic hotkey.
					grabInputOnReentry = (grabInputOnReentry)?0:-1;
					setReentryInputGrab(1);
				}
				else if (_hasFocus)
				{
					keys[mapSDLKeyToGameKey(event.key.key)] = 1;
				}
			}
			break;

			case SDL_EVENT_KEY_UP:
			{
				if (_hasFocus)
				{
					keys[mapSDLKeyToGameKey(event.key.key)] = 0;
				}
			}
			break;

			case SDL_EVENT_MOUSE_MOTION:
			{
				if (_hasFocus && updateMouse)
				{
					mouse.lastPosition = mouse.position;

					mouse.position.x = ((event.motion.x) * (float(virtualWidth)/float(getWindowWidth()))) - getVirtualOffX();
					mouse.position.y = event.motion.y * (float(virtualHeight)/float(getWindowHeight()));

					mouse.change = mouse.position - mouse.lastPosition;

					if (doMouseConstraint()) warpMouse = true;
				}
			}
			break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			{
					SDL_Quit();
					_exit(0);
					//loopDone = true;
					//quit();
			}
			break;

			case SDL_EVENT_MOUSE_WHEEL:
			{
				if (_hasFocus && updateMouse)
				{
					if (event.wheel.y > 0)
						mouse.scrollWheelChange = 1;
					else if (event.wheel.y < 0)
						mouse.scrollWheelChange = -1;
				}
			}
			break;
			case SDL_EVENT_QUIT:
				SDL_Quit();
				_exit(0);
				//loopDone = true;
				//quit();
			break;
			default:
			break;
		}
	}

	if (updateMouse)
	{
		mouse.scrollWheel += mouse.scrollWheelChange;

		if (warpMouse)
		{
			setMousePosition(mouse.position);
		}
	}

}

#define _VLN(x, y, x2, y2) glVertex2f(x, y); glVertex2f(x2, y2);

void Core::print(int x, int y, const char *str, float sz)
{
    // TODO: reimplement
	(void)x; (void)y; (void)str; (void)sz;
}

void Core::cacheRender()
{
	render();
	// what if the screen was full white? then you wouldn't want to clear buffers
	//clearBuffers();
	showBuffer();
	resetTimer();
}

void Core::updateCullData()
{
	cullRadius = baseCullRadius * invGlobalScale;
	cullRadiusSqr = cullRadius * cullRadius;
	screenCenter = cullCenter = cameraPos + Vector(400.0f*invGlobalScale,300.0f*invGlobalScale);
}

void Core::render(int startLayer, int endLayer, bool useFrameBufferIfAvail, bool applySmartCaptureGating)
{

	BBGE_PROF(Core_render);

	if (startLayer == -1 && endLayer == -1 && overrideStartLayer != 0)
	{
		startLayer = overrideStartLayer;
		endLayer = overrideEndLayer;
	}

	globalScaleChanged();

	if (core->minimized) return;
	onRender();

	RenderObject::lastTextureApplied = 0;

	updateCullData();



	renderObjectCount = 0;
	processedRenderObjectCount = 0;
	totalRenderObjectCount = 0;


	// The main scene now always renders into frameBuffer's
	// target texture rather than conditionally (this replaces the old
	// GL FBO capture that only engaged when afterEffectManager needed
	// it).
	// showBuffer() blits this texture to the real backbuffer once
	// per frame before presenting. This is a deliberate behavior change
	// from the old conditional-capture optimization, made so every
	// capture-based effect (AfterEffect, WaterSurfaceRender,
	// ScreenTransition) always has a texture to read from
	// Only engage the main-scene capture if we're the outermost render
	// call (target is still the real backbuffer / NULL). If some outer
	// caller already redirected the render target (e.g.
	// DarkLayer::preRender() calling core->render(layer,layer,false)
	// while its own separate FrameBuffer is bound), respect that and just
	// draw directly into it instead of hijacking the target for our own
	// frameBuffer - matching the original GL code's behavior of always
	// drawing into whatever FBO happened to already be bound.
	//
	// Step 3 of the performance optimization plan, applied carefully
	// given this exact subsystem's regression history in this project:
	// `applySmartCaptureGating` defaults to false, meaning every
	// existing call site (ScreenTransition::capture() explicitly calling
	// core->render() itself to snapshot a specific moment, DarkLayer's
	// nested call, screenshot code) is completely unaffected and keeps
	// the exact behavior above - always capture whenever a fresh outer
	// call happens. Only Core::main()'s own per-frame call opts in to
	// the new gating below, and only for that one, specific call site:
	// real diagnostic data from this project confirmed the unconditional
	// capture happens every frame regardless of scene complexity
	// (targetSwitches stayed pinned at exactly 2.0 whether a scene drew
	// 8 objects or 360), which is genuine, measured, constant overhead
	// with no benefit in the common case where none of the three
	// consumers below actually need this frame's capture.
	//
	// The three real consumers, checked directly rather than assumed:
	// - AfterEffectManager: only genuinely needs the capture when a real
	//   distortion effect is running (effects.size() > 0) - its `active`
	//   flag alone isn't a safe signal here, since it's also true purely
	//   because frameBuffer exists, unrelated to whether an effect is
	//   actually active this frame.
	// - WaterSurfaceRender: signaled via the coarse, conservative
	//   core->auxiliaryCaptureNeeded flag (synced every frame from
	//   DSQ::onUpdate(), since Core/BBGE can't reference Aquaria-layer
	//   types like Game/WaterSurfaceRender directly).
	// - ScreenTransition: not checked here at all, deliberately -
	//   ScreenTransition::capture() calls core->render() itself,
	//   explicitly, with applySmartCaptureGating left at its default
	//   (false), so it always gets the original, unconditional capture
	//   behavior regardless of what's happening below.
	bool smartGateSaysSkip = false;
	if (applySmartCaptureGating)
	{
		bool afterEffectNeedsIt = afterEffectManager && !afterEffectManager->effects.empty();
		smartGateSaysSkip = !afterEffectNeedsIt && !auxiliaryCaptureNeeded;
	}

	SDL_Renderer *renderCheckRenderer = core->getRenderer();
	bool capturing = core->frameBuffer.isInited()
		&& renderCheckRenderer
		&& (SDL_GetRenderTarget(renderCheckRenderer) == NULL)
		&& !smartGateSaysSkip;
	if (capturing)
		core->frameBuffer.startCapture();

	// Diagnostic counters (throttled, same cadence as the rest of
	// PerfLog) - only meaningful when applySmartCaptureGating is true,
	// to directly confirm via the next test run how often this is
	// actually skipping the capture, and that nothing looks wrong
	// (e.g. a suspiciously high skip rate in a water-heavy area would be
	// a red flag worth investigating before trusting this further).
	if (applySmartCaptureGating)
	{
		if (smartGateSaysSkip)
			PerfLog::countCaptureSkipped();
		else
			PerfLog::countCaptureEngaged();
	}

	core->loadBaseTransform();
	clearBuffers();

	setupRenderPositionAndScale();

	/*
	//default
	if (renderObjectLayerOrder.empty())
	{
		renderObjectLayerOrder.resize(renderObjectLayers.size());
		for (int i = 0; i < renderObjectLayerOrder.size(); i++)
		{
			renderObjectLayerOrder[i] = i;
		}
	}
	*/
	RenderObject::rlayer = 0;

	for (int c = 0; c < renderObjectLayerOrder.size(); c++)
	//for (int i = 0; i < renderObjectLayers.size(); i++)
	{
		int i = renderObjectLayerOrder[c];
		if (i == -1) continue;
		if ((startLayer != -1 && endLayer != -1) && (i < startLayer || i > endLayer)) continue;

		if (i == postProcessingFx.layer)
		{
			postProcessingFx.preRender();
		}
		if (i == postProcessingFx.renderLayer)
		{
			postProcessingFx.render();
		}

		if (darkLayer.isUsed() )
		{
			/*
			if (i == darkLayer.getLayer())
			{
				darkLayer.preRender();
			}
			*/
			if (i == darkLayer.getRenderLayer())
			{
				darkLayer.render();
			}

			if (i == darkLayer.getLayer() && startLayer != i)
			{
				continue;
			}
		}

		if (afterEffectManager && afterEffectManager->active && i == afterEffectManagerLayer)
		{
			afterEffectManager->render();
		}

		RenderObjectLayer *r = &renderObjectLayers[i];
		RenderObject::rlayer = r;
		if (r->visible)
		{
			if (r->startPass == r->endPass)
			{
				r->renderPass(RenderObject::RENDER_ALL);
			}
			else
			{
				for (int pass = r->startPass; pass <= r->endPass; pass++)
				{
					r->renderPass(pass);
				}
			}
		}
	}

	if (capturing)
	{
		// AfterEffectManager::render() (if it ran this frame - see the
		// afterEffectManagerLayer check above) calls
		// core->frameBuffer.endCapture() itself, mid-loop, then draws its
		// warped grid directly onto the real backbuffer, intentionally -
		// any layers after it in this loop then also draw straight to
		// the backbuffer, stacking on top. In that case capture is
		// already ended and the captured content has already been drawn
		// (warped) - blitting frameBuffer's texture again here would
		// stomp all of that. Only blit if capture is still active,
		// meaning nothing already handled it this frame.
		SDL_Renderer *renderer = core->getRenderer();
		bool stillCapturing = renderer && core->frameBuffer.getTexture()
			&& (SDL_GetRenderTarget(renderer) == core->frameBuffer.getTexture());
		if (stillCapturing)
		{
			core->frameBuffer.endCapture();
			// Only blit to screen if this was the outermost call -
			// endCapture() restoring us to the real backbuffer (target
			// now NULL) is how we can tell. If we were nested inside
			// another capture (e.g. DarkLayer::preRender() calling
			// core->render() for a single layer while its own separate
			// FrameBuffer is the active render target), endCapture()
			// restores *that* target instead, and the caller (DarkLayer)
			// owns compositing its own result - not us. Blitting
			// unconditionally here would clear and overwrite whatever
			// that outer capture was building.
			if (SDL_GetRenderTarget(renderer) == NULL)
			{
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
				SDL_RenderClear(renderer);
				SDL_RenderTexture(renderer, core->frameBuffer.getTexture(), NULL, NULL);
			}
		}
	}
}

void Core::showBuffer()
{
	BBGE_PROF(Core_showBuffer);
	if (gRenderer)
		SDL_RenderPresent(gRenderer);
}

SDL_Renderer *Core::getRenderer()
{
	return gRenderer;
}

// WARNING: only for use during shutdown
// otherwise, textures will try to remove themselves
// when destroy is called on them
void Core::clearResources()
{
	if(resources.size())
	{
		debugLog("Warning: The following resources were not cleared:");
		for(size_t i = 0; i < resources.size(); ++i)
			debugLog(resources[i]->name);
		resources.clear(); // nothing we can do; refcounting is messed up
	}
}

void Core::shutdownInputLibrary()
{
}

void Core::shutdownJoystickLibrary()
{
	if (joystickEnabled) {
		joystick.shutdown();
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
		joystickEnabled = false;
	}
}

void Core::clearRenderObjects()
{
	for (int i = 0; i < renderObjectLayers.size(); i++)
	{
		/*
		for (int j = 0; j < renderObjectLayers[i].renderObjects.size(); j++)
		{
			RenderObject *r = renderObjectLayers[i].renderObjects[j];
		*/
		RenderObject *r = renderObjectLayers[i].getFirst();
		while (r)
		{
			if (r)
			{
				removeRenderObject(r, DESTROY_RENDER_OBJECT);
			}
			r = renderObjectLayers[i].getNext();
		}
	}
}

void Core::shutdown()
{
	// pop all the states


	debugLog("Core::shutdown");
	shuttingDown = true;

	debugLog("Shutdown Joystick Library...");
		shutdownJoystickLibrary();
	debugLog("OK");

	debugLog("Shutdown Input Library...");
		shutdownInputLibrary();
	debugLog("OK");

	debugLog("Shutdown All States...");
		popAllStates();
	debugLog("OK");

	debugLog("Clear State Instances...");
		clearStateInstances();
	debugLog("OK");

	debugLog("Clear All Remaining RenderObjects...");
		clearRenderObjects();
	debugLog("OK");

	debugLog("Clear All Resources...");
		clearResources();
	debugLog("OK");


	debugLog("Clear State Objects...");
		clearStateObjects();
	debugLog("OK");

	if (afterEffectManager)
	{
		debugLog("Delete AEManager...");
			delete afterEffectManager;
			afterEffectManager = 0;
		debugLog("OK");
	}


	if (sound)
	{
		debugLog("Shutdown Sound Library...");
			sound->stopAll();
			delete sound;
			sound = 0;
		debugLog("OK");
	}

	debugLog("Core's framebuffer...");
		frameBuffer.unloadDevice();
	debugLog("OK");

	debugLog("Shutdown Graphics Library...");
		shutdownGraphicsLibrary();
	debugLog("OK");


#ifdef BBGE_BUILD_VFS
	debugLog("Unload VFS...");
		vfs.Clear();
	debugLog("OK");
#endif


	debugLog("SDL Quit...");
		SDL_Quit();
	debugLog("OK");
}

//util funcs

void Core::instantQuit()
{
    SDL_Event event;
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

bool Core::exists(const std::string &filename)
{
	return ::exists(filename, false); // defined in Base.cpp
}

CountedPtr<Texture> Core::findTexture(const std::string &name)
{
	//stringToUpper(name);
	//std::ofstream out("texturefind.log");
	int sz = resources.size();
	for (int i = 0; i < sz; i++)
	{
		//out << resources[i]->name << " is " << name << " ?" << std::endl;
		//NOTE: ensure all names are lowercase before this point
		if (resources[i]->name == name)
		{
			return resources[i];
		}
	}
	return 0;
}

// This handles unix/win32 relative paths: ./rel/path
// Unix abs paths: /home/user/...
// Win32 abs paths: C:/Stuff/.. and also C:\Stuff\...
#define ISPATHROOT(x) (x[0] == '.' || x[0] == '/' || ((x).length() > 1 && x[1] == ':'))

std::string Core::getTextureLoadName(const std::string &texture)
{
	std::string loadName = texture;

	if (texture.empty() || !ISPATHROOT(texture))
	{
		if (texture.find(baseTextureDirectory) == std::string::npos)
			loadName = baseTextureDirectory + texture;
	}
	return loadName;
}

std::pair<CountedPtr<Texture>, TextureLoadResult> Core::doTextureAdd(const std::string &texture, const std::string &loadName, std::string internalTextureName)
{
	if (texture.empty() || !ISPATHROOT(texture))
	{
		if (texture.find(baseTextureDirectory) != std::string::npos)
			internalTextureName = internalTextureName.substr(baseTextureDirectory.size(), internalTextureName.size());
	}

	if (internalTextureName.size() > 4)
	{
		if (internalTextureName[internalTextureName.size()-4] == '.')
		{
			internalTextureName = internalTextureName.substr(0, internalTextureName.size()-4);
		}
	}

	stringToLowerUserData(internalTextureName);
	CountedPtr<Texture> t = core->findTexture(internalTextureName);
	if (t)
		return std::make_pair(t, TEX_SUCCESS);

	t = new Texture;
	t->name = internalTextureName;
	unsigned res = TEX_FAILED;

	if(t->load(loadName))
		res |= (TEX_LOADED | TEX_SUCCESS);
	else
	{
		t->width = 64;
		t->height = 64;
	}

	return std::make_pair(t, (TextureLoadResult)res);
}

CountedPtr<Texture> Core::addTexture(const std::string &textureName, TextureLoadResult *pLoadResult /* = 0 */)
{
	BBGE_PROF(Core_addTexture);

	if (textureName.empty())
	{
		if(pLoadResult)
			*pLoadResult = TEX_FAILED;
		return NULL;
	}

	std::pair<CountedPtr<Texture>, TextureLoadResult> texResult;
	std::string texture = textureName;
	stringToLowerUserData(texture);
	std::string internalTextureName = texture;
	std::string loadName = getTextureLoadName(texture);

	if (!texture.empty() && texture[0] == '@')
	{
		texture = secondaryTexturePath + texture.substr(1, texture.size());
		loadName = texture;
	}
	else if (!secondaryTexturePath.empty() && texture[0] != '.' && texture[0] != '/')
	{
		std::string t = texture;
		std::string ln = loadName;
		texture = secondaryTexturePath + texture;
		loadName = texture;
		texResult = doTextureAdd(texture, loadName, internalTextureName);
		if (!texResult.second)
			texResult = doTextureAdd(t, ln, internalTextureName);
	}
	else
		texResult = doTextureAdd(texture, loadName, internalTextureName);

	addTexture(texResult.first.content());

	if(debugLogTextures)
	{
		if (texResult.second & TEX_LOADED)
		{
			std::ostringstream os;
			os << "LOADED TEXTURE FROM DISK: [" << internalTextureName << "] idx: " << resources.size()-1;
			debugLog(os.str());
		}
		else if(!(texResult.second & TEX_SUCCESS))
		{
			std::ostringstream os;
			os << "FAILED TO LOAD TEXTURE: [" << internalTextureName << "] idx: " << resources.size()-1;
			debugLog(os.str());
		}
	}
	if(pLoadResult)
		*pLoadResult = texResult.second;
	return texResult.first;
}

void Core::addRenderObject(RenderObject *o, int layer)
{
	if (!o) return;
	o->layer = layer;
	if (layer < 0 || layer >= renderObjectLayers.size())
	{
		std::ostringstream os;
		os << "attempted to add render object to invalid layer [" << layer << "]";
		errorLog(os.str());
	}
	renderObjectLayers[layer].add(o);
}

void Core::switchRenderObjectLayer(RenderObject *o, int toLayer)
{
	if (!o) return;
	renderObjectLayers[o->layer].remove(o);
	renderObjectLayers[toLayer].add(o);
	o->layer = toLayer;
}

void Core::unloadResources()
{
	for (int i = 0; i < resources.size(); i++)
	{
		resources[i]->unload();
	}
}

void Core::onReloadResources()
{
}

void Core::reloadResources()
{
	for (int i = 0; i < resources.size(); i++)
	{
		resources[i]->reload();
	}
	onReloadResources();
}

void Core::addTexture(Texture *r)
{
	for(size_t i = 0; i < resources.size(); ++i)
		if(resources[i] == r)
			return;

	resources.push_back(r);
	if (r->name.empty())
	{
		debugLog("Empty name resource added");
	}
}

void Core::removeTexture(Texture *res)
{
	std::vector<Texture*> copy;
	copy.swap(resources);

	for (size_t i = 0; i < copy.size(); ++i)
	{
		if (copy[i] == res)
		{
			copy[i]->destroy();
			copy[i] = copy.back();
			copy.pop_back();
			break;
		}
	}

	resources.swap(copy);
}

void Core::deleteRenderObjectMemory(RenderObject *r)
{
	//if (!r->allocStatic)
	delete r;
}

void Core::removeRenderObject(RenderObject *r, RemoveRenderObjectFlag flag)
{
	if (r)
	{
		if (r->layer != LR_NONE && !renderObjectLayers[r->layer].empty())
		{
			renderObjectLayers[r->layer].remove(r);
		}
		if (flag != DO_NOT_DESTROY_RENDER_OBJECT )
		{
			r->destroy();

			deleteRenderObjectMemory(r);
		}
	}
}


void Core::enqueueRenderObjectDeletion(RenderObject *object)
{
	if (!object->_dead) // && !object->staticallyAllocated)
	{
		garbage.push_back (object);
		object->_dead = true;
	}
}

void Core::clearGarbage()
{
	BBGE_PROF(Core_clearGarbage);
	// HACK: optimize this (use a list instead of a queue)

	for (RenderObjectList::iterator i = garbage.begin(); i != garbage.end(); i++)
	{
		removeRenderObject(*i, DO_NOT_DESTROY_RENDER_OBJECT);

		(*i)->destroy();
	}

	for (RenderObjectList::iterator i = garbage.begin(); i != garbage.end(); i++)
	{
		deleteRenderObjectMemory(*i);
	}

	garbage.clear();
}

bool Core::canChangeState()
{
	return (nestedMains<=1);
}

/*
int Core::getVirtualWidth()
{
	return virtualWidth;
}

int Core::getVirtualHeight()
{
	return virtualHeight;
}
*/

// Take a screenshot of the specified region of the screen and store it
// in a 32bpp pixel buffer.  delete[] the returned buffer when it's no
// longer needed.
unsigned char *Core::grabScreenshot(int x, int y, int w, int h)
{
	unsigned int size = sizeof(unsigned char) * w * h * 4;
	unsigned char *imageData = new unsigned char[size];
	memset(imageData, 0, size);

	SDL_Renderer *renderer = core->getRenderer();
	if (!renderer)
		return imageData;

	SDL_Rect rect = { x, y, w, h };
	SDL_Surface *surf = SDL_RenderReadPixels(renderer, &rect);
	if (!surf)
		return imageData;

	SDL_Surface *converted = surf;
	if (surf->format != SDL_PIXELFORMAT_RGBA32)
	{
		converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
		SDL_DestroySurface(surf);
		if (!converted)
			return imageData;
	}

	if (SDL_MUSTLOCK(converted))
		SDL_LockSurface(converted);

	const int rowBytes = w * 4;
	const unsigned char *src = (const unsigned char*)converted->pixels;
	for (int row = 0; row < h && row < converted->h; row++)
	{
		memcpy(imageData + (size_t)row * rowBytes, src + (size_t)row * converted->pitch, rowBytes);
	}

	if (SDL_MUSTLOCK(converted))
		SDL_UnlockSurface(converted);
	SDL_DestroySurface(converted);

	// Force all alpha values to 255 (matches original behavior).
	unsigned char *c = imageData;
	for (int i = 0; i < w*h; i++, c += 4)
	{
		c[3] = 255;
	}

	return imageData;
}

// Like grabScreenshot(), but grab from the center of the screen.
unsigned char *Core::grabCenteredScreenshot(int w, int h)
{
	return grabScreenshot(core->width/2 - w/2, core->height/2 - h/2, w, h);
}

// takes a screen shot and saves it to a TGA image
int Core::saveScreenshotTGA(const std::string &filename)
{
	int w = getWindowWidth(), h = getWindowHeight();
	unsigned char *imageData = grabCenteredScreenshot(w, h);
	return tgaSave(filename.c_str(),w,h,32,imageData);
}

void Core::saveCenteredScreenshotTGA(const std::string &filename, int sz)
{
	int w=sz, h=sz;
	int hsm = (w * 3.0f) / 4.0f;
	unsigned char *imageData = grabCenteredScreenshot(w, hsm);

	int imageDataSize = sizeof(unsigned char) * w * hsm * 4;
	int tgaImageSize = sizeof(unsigned char) * w * h * 4;
	unsigned char *tgaImage = new unsigned char[tgaImageSize];
	memcpy(tgaImage, imageData, imageDataSize);
	memset(tgaImage + imageDataSize, 0, tgaImageSize - imageDataSize);
	delete[] imageData;

	int savebits = 32;
	tgaSave(filename.c_str(),w,h,savebits,tgaImage);
}

void Core::saveSizedScreenshotTGA(const std::string &filename, int sz, int crop34)
{
	debugLog("saveSizedScreenshot");

	int w, h;
	unsigned char *imageData;
	w = sz;
	h = sz;
	float fsz = (float)sz;

	unsigned int size = sizeof(unsigned char) * w * h * 3;
	imageData = (unsigned char *)malloc(size);
	memset(imageData, 0, size);

	float wbit = fsz;//+1;
	float hbit = ((fsz)*(3.0f/4.0f));

	int width = core->width-1;
	int height = core->height-1;
	int diff = 0;

	if (crop34)
	{
		width = int((core->height*4.0f)/3.0f);
		diff = (core->width - width)/2;
		width--;
	}

	float zx = wbit/(float)width;
	float zy = hbit/(float)height;

	float copyw = w*(1/zx);
	float copyh = h*(1/zy);



	std::ostringstream os;
	os << "wbit: " << wbit << " hbit: " << hbit << std::endl;
	os << "zx: " << zx << " zy: " << zy << std::endl;
	os << "w: " << w << " h: " << h << std::endl;
	os << "width: " << width << " height: " << height << std::endl;
	os << "copyw: " << copyw << " copyh: " << copyh << std::endl;
	debugLog(os.str());

	SDL_Renderer *renderer = core->getRenderer();
	if (renderer && core->frameBuffer.isInited() && core->frameBuffer.getTexture())
	{
		SDL_Texture *outTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
		if (outTex)
		{
			SDL_Texture *prevTarget = SDL_GetRenderTarget(renderer);
			SDL_SetRenderTarget(renderer, outTex);

			SDL_Texture *srcTex = core->frameBuffer.getTexture();
			SDL_FRect srcRect = { (float)diff, 0.0f, (float)width, (float)height };
			SDL_BlendMode prevBlend = SDL_BLENDMODE_BLEND;
			SDL_GetTextureBlendMode(srcTex, &prevBlend);
			SDL_SetTextureBlendMode(srcTex, SDL_BLENDMODE_NONE);
			SDL_RenderTexture(renderer, srcTex, &srcRect, NULL);
			SDL_SetTextureBlendMode(srcTex, prevBlend);

			SDL_Surface *surf = SDL_RenderReadPixels(renderer, NULL);
			SDL_SetRenderTarget(renderer, prevTarget);

			if (surf)
			{
				SDL_Surface *converted = surf;
				if (surf->format != SDL_PIXELFORMAT_RGB24)
				{
					converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGB24);
					SDL_DestroySurface(surf);
				}
				if (converted)
				{
					if (SDL_MUSTLOCK(converted)) SDL_LockSurface(converted);
					int rowBytes = w * 3;
					for (int row = 0; row < h && row < converted->h; row++)
						memcpy(imageData + (size_t)row * rowBytes,
							(unsigned char*)converted->pixels + (size_t)row * converted->pitch, rowBytes);
					if (SDL_MUSTLOCK(converted)) SDL_UnlockSurface(converted);
					SDL_DestroySurface(converted);
				}
			}
			SDL_DestroyTexture(outTex);
		}
	}

	int savebits = 24;
	tgaSave(filename.c_str(),w,h,savebits,imageData);

	debugLog("done");
}

void Core::save64x64ScreenshotTGA(const std::string &filename)
{
	int w = 64, h = 64;
	unsigned char *imageData = (unsigned char *)malloc(sizeof(unsigned char) * w * h * 4);
	memset(imageData, 0, (size_t)w * h * 4);

	SDL_Renderer *renderer = core->getRenderer();
	if (renderer && core->frameBuffer.isInited() && core->frameBuffer.getTexture())
	{
		SDL_Texture *thumbTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
		if (thumbTex)
		{
			SDL_Texture *prevTarget = SDL_GetRenderTarget(renderer);
			SDL_SetRenderTarget(renderer, thumbTex);

			SDL_Texture *srcTex = core->frameBuffer.getTexture();
			SDL_BlendMode prevBlend = SDL_BLENDMODE_BLEND;
			SDL_GetTextureBlendMode(srcTex, &prevBlend);
			SDL_SetTextureBlendMode(srcTex, SDL_BLENDMODE_NONE);
			SDL_RenderTexture(renderer, srcTex, NULL, NULL); // scales to fill thumbTex
			SDL_SetTextureBlendMode(srcTex, prevBlend);

			SDL_Surface *surf = SDL_RenderReadPixels(renderer, NULL);
			SDL_SetRenderTarget(renderer, prevTarget);

			if (surf)
			{
				SDL_Surface *converted = surf;
				if (surf->format != SDL_PIXELFORMAT_RGBA32)
				{
					converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
					SDL_DestroySurface(surf);
				}
				if (converted)
				{
					if (SDL_MUSTLOCK(converted)) SDL_LockSurface(converted);
					int rowBytes = w * 4;
					for (int row = 0; row < h && row < converted->h; row++)
						memcpy(imageData + (size_t)row * rowBytes,
							(unsigned char*)converted->pixels + (size_t)row * converted->pitch, rowBytes);
					if (SDL_MUSTLOCK(converted)) SDL_UnlockSurface(converted);
					SDL_DestroySurface(converted);
				}
			}
			SDL_DestroyTexture(thumbTex);
		}
	}

	// Force alpha to 255 (matches original behavior).
	unsigned char *c = imageData;
	for (int i = 0; i < w*h; i++, c += 4)
		c[3] = 255;

	tgaSave(filename.c_str(),64,64,32,imageData);

	// do NOT free imageData here
	// it IS freed in tgaSave
	//free(imageData);
}




// saves an array of pixels as a TGA image (frees the image data passed in)
int Core::tgaSave(	const char	*filename,
		short int	width,
		short int	height,
		unsigned char	pixelDepth,
		unsigned char	*imageData) {

	unsigned char cGarbage = 0, type,mode,aux;
	short int iGarbage = 0;
	int i;
	FILE *file;

// open file and check for errors
	file = fopen(adjustFilenameCase(filename).c_str(), "wb");
	if (file == NULL) {
		delete [] imageData;
		return (int)false;
	}

// compute image type: 2 for RGB(A), 3 for greyscale
	mode = pixelDepth / 8;
	if ((pixelDepth == 24) || (pixelDepth == 32))
		type = 2;
	else
		type = 3;

// write the header
	if (fwrite(&cGarbage, sizeof(unsigned char), 1, file) != 1
		|| fwrite(&cGarbage, sizeof(unsigned char), 1, file) != 1
		|| fwrite(&type, sizeof(unsigned char), 1, file) != 1
		|| fwrite(&iGarbage, sizeof(short int), 1, file) != 1
		|| fwrite(&iGarbage, sizeof(short int), 1, file) != 1
		|| fwrite(&cGarbage, sizeof(unsigned char), 1, file) != 1
		|| fwrite(&iGarbage, sizeof(short int), 1, file) != 1
		|| fwrite(&iGarbage, sizeof(short int), 1, file) != 1
		|| fwrite(&width, sizeof(short int), 1, file) != 1
		|| fwrite(&height, sizeof(short int), 1, file) != 1
		|| fwrite(&pixelDepth, sizeof(unsigned char), 1, file) != 1
		|| fwrite(&cGarbage, sizeof(unsigned char), 1, file) != 1)
	{
		fclose(file);
		delete [] imageData;
		return (int)false;
	}

// convert the image data from RGB(A) to BGR(A)
	if (mode >= 3)
	for (i=0; i < width * height * mode ; i+= mode) {
		aux = imageData[i];
		imageData[i] = imageData[i+2];
		imageData[i+2] = aux;
	}

// save the image data
	if (fwrite(imageData, sizeof(unsigned char),
			width * height * mode, file) != width * height * mode)
	{
		fclose(file);
		delete [] imageData;
		return (int)false;
	}

	fclose(file);
	delete [] imageData;

	return (int)true;
}

// saves a series of files with names "filenameX"
int Core::tgaSaveSeries(char		*filename,
			 short int		width,
			 short int		height,
			 unsigned char	pixelDepth,
			 unsigned char	*imageData) {

	char *newFilename;
	int status;

// compute the new filename by adding the
// series number and the extension
	newFilename = (char *)malloc(sizeof(char) * strlen(filename)+8);

	sprintf(newFilename,"%s%d",filename,numSavedScreenshots);

// save the image
	status = tgaSave(newFilename,width,height,pixelDepth,imageData);

//increase the counter
	if (status == (int)true)
		numSavedScreenshots++;
	free(newFilename);
	return(status);
}

 void Core::screenshot()
 {
	 doScreenshot = true;
//	ilutGLScreenie();
 }


 #include "DeflateCompressor.h"

 // saves an array of pixels as a TGA image (frees the image data passed in)
int Core::zgaSave(	const char	*filename,
		short int	w,
		short int	h,
		unsigned char	depth,
		unsigned char	*imageData) {

	ByteBuffer::uint8 type,mode,aux, pixelDepth = depth;
	ByteBuffer::uint8 cGarbage = 0;
	ByteBuffer::uint16 iGarbage = 0;
	ByteBuffer::uint16 width = w, height = h;

// open file and check for errors
	FILE *file = fopen(adjustFilenameCase(filename).c_str(), "wb");
	if (file == NULL) {
		delete [] imageData;
		return (int)false;
	}

// compute image type: 2 for RGB(A), 3 for greyscale
	mode = pixelDepth / 8;
	if ((pixelDepth == 24) || (pixelDepth == 32))
		type = 2;
	else
		type = 3;

// convert the image data from RGB(A) to BGR(A)
	if (mode >= 3)
	for (int i=0; i < width * height * mode ; i+= mode) {
		aux = imageData[i];
		imageData[i] = imageData[i+2];
		imageData[i+2] = aux;
	}

	ZlibCompressor z;
	z.SetForceCompression(true);
	z.reserve(width * height * mode + 30);
	z	<< cGarbage
		<< cGarbage
		<< type
		<< iGarbage
		<< iGarbage
		<< cGarbage
		<< iGarbage
		<< iGarbage
		<< width
		<< height
		<< pixelDepth
		<< cGarbage;

	z.append(imageData, width * height * mode);
	z.Compress(3);

// save the image data
	if (fwrite(z.contents(), 1, z.size(), file) != z.size())
	{
		fclose(file);
		delete [] imageData;
		return (int)false;
	}

	fclose(file);
	delete [] imageData;

	return (int)true;
}



#include "ttvfs_zip/VFSZipArchiveLoader.h"

void Core::setupFileAccess()
{
#ifdef BBGE_BUILD_VFS
	debugLog("Init VFS...");

	if(!ttvfs::checkCompat())
		exit_error("ttvfs not compatible");

	ttvfs_setroot(&vfs);

	vfs.AddLoader(new ttvfs::DiskLoader);
	vfs.AddArchiveLoader(new ttvfs::VFSZipArchiveLoader);

	vfs.Mount("override", "");

	// If we ever want to read from a container...
	//vfs.AddArchive("aqfiles.zip");

	if(_extraDataDir.length())
	{
		debugLog("Mounting extra data dir: " + _extraDataDir);
		vfs.Mount(_extraDataDir.c_str(), "");
	}

	debugLog("Done");
#endif
}

void Core::initLocalization()
{
	InStream in(localisePath("data/localecase.txt"));
	if(!in)
	{
		debugLog("data/localecase.txt does not exist, using internal locale data");
		return;
	}

	std::string low, up;
	std::map<unsigned char, unsigned char> trans;
	while(in)
	{
		in >> low >> up;
		trans[low[0]] = up[0];
	}
	initCharTranslationTables(trans);
}

