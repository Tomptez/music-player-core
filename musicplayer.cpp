// Python module for playing audio
// part of MusicPlayer, https://github.com/albertz/music-player
// Copyright (c) 2012, Albert Zeyer, www.az2000.de
// All rights reserved.
// This code is under the 2-clause BSD license, see License.txt in the root directory of this project.

// compile:
// gcc -c musicplayer*.cpp -I /System/Library/Frameworks/Python.framework/Headers/
// libtool -dynamic -o musicplayer.so musicplayer*.o -framework Python -lavformat -lavutil -lavcodec -lswresample -lportaudio -lc

// loosely based on ffplay.c
// https://github.com/FFmpeg/ffmpeg/blob/master/ffplay.c

// Similar to PyFFmpeg. http://code.google.com/p/pyffmpeg/
// This module is intended to be much simpler/high-level, though.

// Pyton interface:
//	createPlayer() -> player object with:
//		queue: song generator
//		peekQueue(n): list of upcoming songs. this should not change the queue. this also might not be accurate but that doesn't matter. it might also return less. it is used for caching and gapless playback. if the queue returns other songs later, it will just ignore these peeked songs. otherwise, it will use these caches.
//		playing: True or False, initially False
//		volume: 1.0 is norm; this is just a factor to each sample. default is 0.9
//		volumeSmoothClip: smooth clipping, see below. set to (1,1) to disable. default is (0.95,10)
//		curSong: current song (read only)
//		curSongPos: current song pos (read only)
//		curSongLen: current song len (read only)
//		curSongGainFactor: current song gain. read from song.gain (see below). can also be written
//		seekAbs(t) / seekRel(t): seeking functions (t in seconds, accepts float)
//		nextSong(): skip to next song function
//	song object expected interface:
//		readPacket(bufSize): should return some string
//		seekRaw(offset, whence): should seek and return the current pos
//		gain: some gain in decible, e.g. calculated by calcReplayGain. if not present, is ignored
//		url: some url, can be anything printable (not used at the moment)
//	and other functions, see their embedded doc ...


#include "musicplayer.h"
#include "PythonHelpers.h"

#define module_name "musicplayer"



static PyMethodDef module_methods[] = {
	{"createPlayer",	(PyCFunction)pyCreatePlayer,	METH_NOARGS,	"creates new player"},
	{"getSoundDevices", (PyCFunction)pyGetSoundDevices, METH_NOARGS,	"get list of sound device names"},
	{"getMetadata",		pyGetMetadata,	METH_VARARGS,	"get metadata for Song"},
	{"calcAcoustIdFingerprint",		pyCalcAcoustIdFingerprint,	METH_VARARGS,	"calculate AcoustID fingerprint for Song"},
	{"calcBitmapThumbnail",		(PyCFunction)pyCalcBitmapThumbnail,	METH_VARARGS|METH_KEYWORDS,	"calculate bitmap thumbnail for Song"},
	{"calcReplayGain",		(PyCFunction)pyCalcReplayGain,	METH_VARARGS|METH_KEYWORDS,	"calculate ReplayGain for Song"},
	{"setFfmpegLogLevel",		pySetFfmpegLogLevel,	METH_VARARGS,	"set FFmpeg log level (av_log_set_level)"},
	{"enableDebugLog",	(PyCFunction)pyEnableDebugLog,	METH_VARARGS,	"enable/disable debug log"},
	{NULL,				NULL}	/* sentinel */
};

PyDoc_STRVAR(module_doc, "Music player.");

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef module_def = {
	PyModuleDef_HEAD_INIT,
	module_name,     /* m_name */
	module_doc,  /* m_doc */
	-1,                  /* m_size */
	module_methods,    /* m_methods */
	NULL,                /* m_reload */
	NULL,                /* m_traverse */
	NULL,                /* m_clear */
	NULL,                /* m_free */
};
#endif

static PyObject* EventClass = NULL;

static void init() {
	PyEval_InitThreads(); /* Start the interpreter's thread-awareness */
	initPlayerOutput();
	initPlayerDecoder();
}


#if PY_MAJOR_VERSION == 2
PyMODINIT_FUNC initmusicplayer(void)
#else
PyMODINIT_FUNC PyInit_musicplayer(void)
#endif
{
	//printf("initmusicplayer\n");
	init();
	if (PyType_Ready(&Player_Type) < 0)
		Py_FatalError("Can't initialize player type");

#if PY_MAJOR_VERSION == 2
	PyObject* m = Py_InitModule3(module_name, module_methods, module_doc);
#else
	PyObject* m = PyModule_Create(&module_def);
#endif
	if(!m) {
		Py_FatalError("Can't initialize " module_name " module");
		goto error;
	}

	if(EventClass == NULL) {
		PyObject* classDict = PyDict_New();
		assert(classDict);
		PyObject* className = PyString_FromString("Event");
		assert(className);
#if PY_MAJOR_VERSION == 2
		EventClass = PyClass_New(NULL, classDict, className);
#else
		PyObject* classBases = PyTuple_New(0);
		EventClass = PyObject_CallFunctionObjArgs((PyObject *)&PyType_Type, className, classBases, classDict, NULL);
		Py_XDECREF(classBases);
#endif
		assert(EventClass);
		Py_XDECREF(classDict); classDict = NULL;
		Py_XDECREF(className); className = NULL;
	}

	if(EventClass) {
		Py_INCREF(EventClass);
		PyModule_AddObject(m, "Event", EventClass); // takes the ref
	}

#if PY_MAJOR_VERSION == 2
error:
	Py_XDECREF(m);
	return;
#else
	return m;
error:
	return NULL;
#endif
}
