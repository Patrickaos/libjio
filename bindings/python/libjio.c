
/*
 * Python bindings for libjio
 * Alberto Bertogli (albertogli@telpin.com.ar)
 * Aug/2004
 */


#include <Python.h>

#include <libjio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * This module provides two classes and some functions.
 *
 * The classes are jfile (created with open()) and jtrans (created with
 * jfile.new_trans()).
 *
 * The first one represents a journaled file where you operate using read(),
 * write() and so on; to close it, just call del(). This is similar to the
 * UNIX file.
 *
 * The second one represents a single transaction, which is composed of
 * several operations that get added by its add() method. It gets commited
 * with commit(), and rolled back with rollback().
 *
 * There rest of the module's functions are related to file checking, called
 * jfsck() and jfsck_cleanup(), which are just wrappers to the real C
 * functions.
 */

/*
 * Type definitions
 */

/* jfile */
typedef struct {
	PyObject_HEAD
	struct jfs *fs;
} jfileobject;
static PyTypeObject JFileType;

/* jtrans */
typedef struct {
	PyObject_HEAD
	struct jtrans *ts;
	jfileobject *jfile;
} jtransobject;
static PyTypeObject JTransType;


/*
 * The jfile object
 */

/* delete */
static void jf_dealloc(jfileobject *fp)
{
	if (fp->fs) {
		jclose(fp->fs);
		free(fp->fs);
	}
	PyObject_Del(fp);
}

/* read */
PyDoc_STRVAR(jf_read__doc,
"read(size)\n\
\n\
Read at most size bytes from the file, returns the string with\n\
the contents.\n\
It's a wrapper to jread().\n");

static PyObject *jf_read(jfileobject *fp, PyObject *args)
{
	long rv;
	long len;
	unsigned char *buf;
	PyObject *r;

	if (!PyArg_ParseTuple(args, "i:read", &len))
		return NULL;

	buf = malloc(len);
	if (buf == NULL)
		return PyErr_NoMemory();

	Py_BEGIN_ALLOW_THREADS
	rv = jread(fp->fs, buf, len);
	Py_END_ALLOW_THREADS

	if (rv < 0) {
		r = PyErr_SetFromErrno(PyExc_IOError);
	} else {
		r = PyString_FromStringAndSize(buf, rv);
	}

	free(buf);
	return r;
}

/* pread */
PyDoc_STRVAR(jf_pread__doc,
"pread(size, offset)\n\
\n\
Read size bytes from the file at the given offset, return a string with the\n\
contents.\n\
It's a wrapper to jpread().\n");

static PyObject *jf_pread(jfileobject *fp, PyObject *args)
{
	long rv;
	long len;
	long offset;
	unsigned char *buf;
	PyObject *r;

	if (!PyArg_ParseTuple(args, "il:pread", &len, &offset))
		return NULL;

	buf = malloc(len);
	if (buf == NULL)
		return PyErr_NoMemory();

	Py_BEGIN_ALLOW_THREADS
	rv = jpread(fp->fs, buf, len, offset);
	Py_END_ALLOW_THREADS

	if (rv < 0) {
		r = PyErr_SetFromErrno(PyExc_IOError);
	} else {
		r = PyString_FromStringAndSize(buf, rv);
	}

	free(buf);
	return r;
}

/* write */
PyDoc_STRVAR(jf_write__doc,
"write(buf)\n\
\n\
Write the contents of the given buffer (a string) to the file, returns the\n\
number of bytes written.\n\
It's a wrapper to jwrite().\n");

static PyObject *jf_write(jfileobject *fp, PyObject *args)
{
	long rv;
	unsigned char *buf;
	long len;

	if (!PyArg_ParseTuple(args, "s#:write", &buf, &len))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jwrite(fp->fs, buf, len);
	Py_END_ALLOW_THREADS

	return PyLong_FromLong(rv);
}

/* pwrite */
PyDoc_STRVAR(jf_pwrite__doc,
"pwrite(buf, offset)\n\
\n\
Write the contents of the given buffer (a string) to the file at the given\n\
offset, returns the number of bytes written.\n\
It's a wrapper to jpwrite().\n");

static PyObject *jf_pwrite(jfileobject *fp, PyObject *args)
{
	long rv;
	unsigned char *buf;
	long offset;
	long len;

	if (!PyArg_ParseTuple(args, "s#l:pwrite", &buf, &len, &offset))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jpwrite(fp->fs, buf, len, offset);
	Py_END_ALLOW_THREADS

	return PyLong_FromLong(rv);
}

/* truncate */
PyDoc_STRVAR(jf_truncate__doc,
"truncate(lenght)\n\
\n\
Truncate the file to the given size.\n\
It's a wrapper to jtruncate().\n");

static PyObject *jf_truncate(jfileobject *fp, PyObject *args)
{
	int rv;
	long lenght;

	if (!PyArg_ParseTuple(args, "l:truncate", &lenght))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jtruncate(fp->fs, lenght);
	Py_END_ALLOW_THREADS

	if (rv != 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyInt_FromLong(rv);
}

/* lseek */
PyDoc_STRVAR(jf_lseek__doc,
"lseek(offset, whence)\n\
\n\
Reposition the file pointer to the given offset, according to the directive\n\
whence as follows:\n\
SEEK_SET    The offset is set relative to the beginning of the file.\n\
SEEK_CUR    The offset is set relative to the current position.\n\
SEEK_END    The offset is set relative to the end of the file.\n\
\n\
These constants are defined in the module. See lseek's manpage for more\n\
information.\n\
It's a wrapper to jlseek().\n");

static PyObject *jf_lseek(jfileobject *fp, PyObject *args)
{
	long rv;
	int whence;
	long offset;

	if (!PyArg_ParseTuple(args, "li:lseek", &offset, &whence))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jlseek(fp->fs, offset, whence);
	Py_END_ALLOW_THREADS

	if (rv == -1)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* jsync */
PyDoc_STRVAR(jf_jsync__doc,
"jsync()\n\
\n\
Used with lingering transactions, see the library documentation for more\n\
detailed information.\n\
It's a wrapper to jsync().\n");

static PyObject *jf_jsync(jfileobject *fp, PyObject *args)
{
	long rv;

	if (!PyArg_ParseTuple(args, ":jsync"))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jsync(fp->fs);
	Py_END_ALLOW_THREADS

	return PyLong_FromLong(rv);
}

/* new_trans */
PyDoc_STRVAR(jf_new_trans__doc,
"new_trans()\n\
\n\
Returns an object representing a new empty transaction.\n\
It's a wrapper to jtrans_init().\n");

static PyObject *jf_new_trans(jfileobject *fp, PyObject *args)
{
	jtransobject *tp;

	if (!PyArg_ParseTuple(args, ":new_trans"))
		return NULL;

	tp = PyObject_New(jtransobject, &JTransType);
	if (tp == NULL)
		return NULL;

	tp->ts = malloc(sizeof(struct jtrans));
	if(tp->ts == NULL) {
		return PyErr_NoMemory();
	}

	/* increment the reference count, it's decremented on deletion */
	tp->jfile = fp;
	Py_INCREF(fp);

	jtrans_init(fp->fs, tp->ts);

	return (PyObject *) tp;
}


/* method table */
static PyMethodDef jfile_methods[] = {
	{ "read", (PyCFunction)jf_read, METH_VARARGS, jf_read__doc },
	{ "pread", (PyCFunction)jf_pread, METH_VARARGS, jf_pread__doc },
	{ "write", (PyCFunction)jf_write, METH_VARARGS, jf_write__doc },
	{ "pwrite", (PyCFunction)jf_pwrite, METH_VARARGS, jf_pwrite__doc },
	{ "truncate", (PyCFunction)jf_truncate, METH_VARARGS, jf_truncate__doc },
	{ "lseek", (PyCFunction)jf_lseek, METH_VARARGS, jf_lseek__doc },
	{ "jsync", (PyCFunction)jf_jsync, METH_VARARGS, jf_jsync__doc },
	{ "new_trans", (PyCFunction)jf_new_trans, METH_VARARGS, jf_new_trans__doc },
	{ NULL }
};

static PyObject *jf_getattr(jfileobject *fp, char *name)
{
	return Py_FindMethod(jfile_methods, (PyObject *)fp, name);
}

static PyTypeObject JFileType = {
	PyObject_HEAD_INIT(NULL)
	0,
	"libjio.jfile",
	sizeof(jfileobject),
	0,
	(destructor)jf_dealloc,
	0,
	(getattrfunc)jf_getattr,
};


/*
 * The jtrans object
 */

/* delete */
static void jt_dealloc(jtransobject *tp)
{
	if (tp->ts != NULL) {
		jtrans_free(tp->ts);
		free(tp->ts);
	}
	Py_DECREF(tp->jfile);
	PyObject_Del(tp);
}

/* add */
PyDoc_STRVAR(jt_add__doc,
"add(buf, offset)\n\
\n\
Add an operation to write the given buffer at the given offset to the\n\
transaction.\n\
It's a wrapper to jtrans_add().\n");

static PyObject *jt_add(jtransobject *tp, PyObject *args)
{
	long rv;
	long len;
	long offset;
	unsigned char *buf;

	if (!PyArg_ParseTuple(args, "s#l:add", &buf, &len, &offset))
		return NULL;

	rv = jtrans_add(tp->ts, buf, len, offset);

	return PyLong_FromLong(rv);
}

/* commit */
PyDoc_STRVAR(jt_commit__doc,
"commit()\n\
\n\
Commits a transaction.\n\
It's a wrapper to jtrans_commit().\n");

static PyObject *jt_commit(jtransobject *tp, PyObject *args)
{
	long rv;

	if (!PyArg_ParseTuple(args, ":commit"))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jtrans_commit(tp->ts);
	Py_END_ALLOW_THREADS

	if (rv < 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* rollback */
PyDoc_STRVAR(jt_rollback__doc,
"rollback()\n\
\n\
Rollbacks a transaction.\n\
It's a wrapper to jtrans_rollback().\n");

static PyObject *jt_rollback(jtransobject *tp, PyObject *args)
{
	long rv;

	if (!PyArg_ParseTuple(args, ":rollback"))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jtrans_rollback(tp->ts);
	Py_END_ALLOW_THREADS

	if (rv < 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* method table */
static PyMethodDef jtrans_methods[] = {
	{ "add", (PyCFunction)jt_add, METH_VARARGS, jt_add__doc },
	{ "commit", (PyCFunction)jt_commit, METH_VARARGS, jt_commit__doc },
	{ "rollback", (PyCFunction)jt_rollback, METH_VARARGS, jt_rollback__doc },
	{ NULL }
};

static PyObject *jt_getattr(jtransobject *tp, char *name)
{
	return Py_FindMethod(jtrans_methods, (PyObject *)tp, name);
}

static PyTypeObject JTransType = {
	PyObject_HEAD_INIT(NULL)
	0,
	"libjio.jtrans",
	sizeof(jtransobject),
	0,
	(destructor)jt_dealloc,
	0,
	(getattrfunc)jt_getattr,
};



/*
 * The module
 */

/* open */
PyDoc_STRVAR(jf_open__doc,
"open(name[, flags[, mode[, jflags]]])\n\
\n\
Opens a file, returns a file object.\n\
The arguments flags, mode and jflags are the same as jopen(); the constants\n\
needed are defined in the module.\n\
It's a wrapper to jopen().\n");

static PyObject *jf_open(PyObject *self, PyObject *args)
{
	int rv;
	char *file;
	int flags, mode, jflags;
	jfileobject *fp;

	flags = O_RDWR;
	mode = 0666;
	jflags = 0;

	if (!PyArg_ParseTuple(args, "s|iii:open", &file, &flags, &mode,
				&jflags))
		return NULL;

	fp = PyObject_New(jfileobject, &JFileType);
	if (fp == NULL)
		return NULL;

	fp->fs = malloc(sizeof(struct jfs));
	if (fp->fs == NULL) {
		return PyErr_NoMemory();
	}

	rv = jopen(fp->fs, file, flags, mode, jflags);
	if (rv < 0) {
		free(fp->fs);
		return PyErr_SetFromErrno(PyExc_IOError);
	}

	if (PyErr_Occurred()) {
		free(fp->fs);
		return NULL;
	}

	return (PyObject *) fp;
}

/* jfsck */
PyDoc_STRVAR(jf_jfsck__doc,
"jfsck(name)\n\
\n\
Checks the integrity of the file with the given name; returns a dictionary\n\
with all the different values of the check (equivalent to the 'struct\n\
jfsck_result'), or None if there was nothing to check.\n\
It's a wrapper to jfsck().\n");

static PyObject *jf_jfsck(PyObject *self, PyObject *args)
{
	int rv;
	char *name;
	struct jfsck_result res;
	PyObject *dict;

	if (!PyArg_ParseTuple(args, "s:jfsck", &name))
		return NULL;

	dict = PyDict_New();
	if (dict == NULL)
		return PyErr_NoMemory();

	Py_BEGIN_ALLOW_THREADS
	rv = jfsck(name, &res);
	Py_END_ALLOW_THREADS

	if (rv == J_ENOMEM) {
		return PyErr_NoMemory();
	} else if (rv != 0) {
		return Py_None;
	}

	PyDict_SetItemString(dict, "total", PyLong_FromLong(res.total));
	PyDict_SetItemString(dict, "invalid", PyLong_FromLong(res.invalid));
	PyDict_SetItemString(dict, "in_progress", PyLong_FromLong(res.in_progress));
	PyDict_SetItemString(dict, "broken", PyLong_FromLong(res.broken));
	PyDict_SetItemString(dict, "corrupt", PyLong_FromLong(res.corrupt));
	PyDict_SetItemString(dict, "apply_error", PyLong_FromLong(res.apply_error));
	PyDict_SetItemString(dict, "reapplied", PyLong_FromLong(res.reapplied));

	return dict;
}

/* jfsck_cleanup */
PyDoc_STRVAR(jf_jfsck_cleanup__doc,
"jfsck_cleanup()\n\
\n\
Clean the journal directory and leave it ready to use.\n\
It's a wrapper to jfsck_cleanup().\n");

static PyObject *jf_jfsck_cleanup(PyObject *self, PyObject *args)
{
	long rv;
	char *name;

	if (!PyArg_ParseTuple(args, "s:jfsck_cleanup", &name))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jfsck_cleanup(name);
	Py_END_ALLOW_THREADS

	return PyInt_FromLong(rv);
}

/* function table */
static PyMethodDef libjio_functions[] = {
	{ "open", (PyCFunction)jf_open, METH_VARARGS, jf_open__doc },
	{ "jfsck", (PyCFunction)jf_jfsck, METH_VARARGS, jf_jfsck__doc },
	{ "jfsck_cleanup", (PyCFunction)jf_jfsck_cleanup, METH_VARARGS, jf_jfsck_cleanup__doc },
	{ NULL, },
};

/* module initialization */
PyDoc_STRVAR(libjio__doc,
"libjio is a library to do transactional, journaled I/O\n\
You can find it at http://users.auriga.wearlab.de/~alb/libjio/\n\
\n\
Use the open() method to create a file object, and then operate on it.\n\
Please read the documentation for more information.\n");

PyMODINIT_FUNC initlibjio(void)
{
	PyObject* m;

	JFileType.ob_type = &PyType_Type;
	JTransType.ob_type = &PyType_Type;

	m = Py_InitModule3("libjio", libjio_functions, libjio__doc);

	Py_INCREF(&JFileType);
	PyModule_AddObject(m, "jfile", (PyObject *) &JFileType);

	Py_INCREF(&JTransType);
	PyModule_AddObject(m, "jtrans", (PyObject *) &JTransType);

	/* libjio's constants */
	PyModule_AddIntConstant(m, "J_NOLOCK", J_NOLOCK);
	PyModule_AddIntConstant(m, "J_NOROLLBACK", J_NOROLLBACK);
	PyModule_AddIntConstant(m, "J_LINGER", J_LINGER);
	PyModule_AddIntConstant(m, "J_COMMITTED", J_COMMITTED);
	PyModule_AddIntConstant(m, "J_ROLLBACKED", J_ROLLBACKED);
	PyModule_AddIntConstant(m, "J_ROLLBACKING", J_ROLLBACKING);
	PyModule_AddIntConstant(m, "J_ESUCCESS", J_ESUCCESS);
	PyModule_AddIntConstant(m, "J_ENOENT", J_ENOENT);
	PyModule_AddIntConstant(m, "J_ENOJOURNAL", J_ENOJOURNAL);
	PyModule_AddIntConstant(m, "J_ENOMEM", J_ENOMEM);

	/* open constants (at least the POSIX ones) */
	PyModule_AddIntConstant(m, "O_RDONLY", O_RDONLY);
	PyModule_AddIntConstant(m, "O_WRONLY", O_WRONLY);
	PyModule_AddIntConstant(m, "O_RDWR", O_RDWR);
	PyModule_AddIntConstant(m, "O_CREAT", O_CREAT);
	PyModule_AddIntConstant(m, "O_EXCL", O_EXCL);
	PyModule_AddIntConstant(m, "O_TRUNC", O_TRUNC);
	PyModule_AddIntConstant(m, "O_APPEND", O_APPEND);
	PyModule_AddIntConstant(m, "O_NONBLOCK", O_NONBLOCK);
	PyModule_AddIntConstant(m, "O_NDELAY", O_NDELAY);
	PyModule_AddIntConstant(m, "O_SYNC", O_SYNC);
	PyModule_AddIntConstant(m, "O_ASYNC", O_ASYNC);
	PyModule_AddIntConstant(m, "O_LARGEFILE", O_LARGEFILE);

	/* lseek constants */
	PyModule_AddIntConstant(m, "SEEK_SET", SEEK_SET);
	PyModule_AddIntConstant(m, "SEEK_CUR", SEEK_CUR);
	PyModule_AddIntConstant(m, "SEEK_END", SEEK_END);
}

