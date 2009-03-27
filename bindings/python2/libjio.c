
/*
 * Python bindings for libjio
 * Alberto Bertogli (albertito@blitiri.com.ar)
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
} jfile_object;

static PyTypeObject jfile_type;

/* jtrans */
typedef struct {
	PyObject_HEAD
	struct jtrans *ts;
	jfile_object *jfile;
} jtrans_object;

static PyTypeObject jtrans_type;


/*
 * The jfile object
 */

/* delete */
static void jf_dealloc(jfile_object *fp)
{
	if (fp->fs) {
		jclose(fp->fs);
		free(fp->fs);
	}
	PyObject_Del(fp);
}

/* fileno */
PyDoc_STRVAR(jf_fileno__doc,
"fileno()\n\
\n\
Return the file descriptor number for the file.\n");

static PyObject *jf_fileno(jfile_object *fp, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":fileno"))
		return NULL;

	return PyInt_FromLong(fp->fs->fd);
}

/* read */
PyDoc_STRVAR(jf_read__doc,
"read(size)\n\
\n\
Read at most size bytes from the file, returns the string with\n\
the contents.\n\
It's a wrapper to jread().\n");

static PyObject *jf_read(jfile_object *fp, PyObject *args)
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
		r = PyString_FromStringAndSize((char *) buf, rv);
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

static PyObject *jf_pread(jfile_object *fp, PyObject *args)
{
	long rv;
	long len;
	long long offset;
	unsigned char *buf;
	PyObject *r;

	if (!PyArg_ParseTuple(args, "iL:pread", &len, &offset))
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
		r = PyString_FromStringAndSize((char *) buf, rv);
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

static PyObject *jf_write(jfile_object *fp, PyObject *args)
{
	long rv;
	unsigned char *buf;
	int len;

	if (!PyArg_ParseTuple(args, "s#:write", &buf, &len))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jwrite(fp->fs, buf, len);
	Py_END_ALLOW_THREADS

	if (rv < 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* pwrite */
PyDoc_STRVAR(jf_pwrite__doc,
"pwrite(buf, offset)\n\
\n\
Write the contents of the given buffer (a string) to the file at the given\n\
offset, returns the number of bytes written.\n\
It's a wrapper to jpwrite().\n");

static PyObject *jf_pwrite(jfile_object *fp, PyObject *args)
{
	long rv;
	unsigned char *buf;
	long long offset;
	int len;

	if (!PyArg_ParseTuple(args, "s#L:pwrite", &buf, &len, &offset))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jpwrite(fp->fs, buf, len, offset);
	Py_END_ALLOW_THREADS

	if (rv < 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* truncate */
PyDoc_STRVAR(jf_truncate__doc,
"truncate(lenght)\n\
\n\
Truncate the file to the given size.\n\
It's a wrapper to jtruncate().\n");

static PyObject *jf_truncate(jfile_object *fp, PyObject *args)
{
	int rv;
	long long lenght;

	if (!PyArg_ParseTuple(args, "L:truncate", &lenght))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jtruncate(fp->fs, lenght);
	Py_END_ALLOW_THREADS

	if (rv != 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLongLong(rv);
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

static PyObject *jf_lseek(jfile_object *fp, PyObject *args)
{
	long long rv;
	int whence;
	long long offset;

	if (!PyArg_ParseTuple(args, "Li:lseek", &offset, &whence))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jlseek(fp->fs, offset, whence);
	Py_END_ALLOW_THREADS

	if (rv == -1)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLongLong(rv);
}

/* jsync */
PyDoc_STRVAR(jf_jsync__doc,
"jsync()\n\
\n\
Used with lingering transactions, see the library documentation for more\n\
detailed information.\n\
It's a wrapper to jsync().\n");

static PyObject *jf_jsync(jfile_object *fp, PyObject *args)
{
	long rv;

	if (!PyArg_ParseTuple(args, ":jsync"))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jsync(fp->fs);
	Py_END_ALLOW_THREADS

	if (rv < 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* jmove_journal */
PyDoc_STRVAR(jf_jmove_journal__doc,
"jmove_journal(newpath)\n\
\n\
Moves the journal directory to the new path; note that there MUST NOT BE\n\
anything else operating on the file.\n\
It's a wrapper to jmove_journal().\n");

static PyObject *jf_jmove_journal(jfile_object *fp, PyObject *args)
{
	long rv;
	char *newpath;

	if (!PyArg_ParseTuple(args, "s:jmove_journal", &newpath))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jmove_journal(fp->fs, newpath);
	Py_END_ALLOW_THREADS

	if (rv != 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* new_trans */
PyDoc_STRVAR(jf_new_trans__doc,
"new_trans()\n\
\n\
Returns an object representing a new empty transaction.\n\
It's a wrapper to jtrans_init().\n");

static PyObject *jf_new_trans(jfile_object *fp, PyObject *args)
{
	jtrans_object *tp;

	if (!PyArg_ParseTuple(args, ":new_trans"))
		return NULL;

	tp = PyObject_New(jtrans_object, &jtrans_type);
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
	{ "fileno", (PyCFunction) jf_fileno, METH_VARARGS, jf_fileno__doc },
	{ "read", (PyCFunction) jf_read, METH_VARARGS, jf_read__doc },
	{ "pread", (PyCFunction) jf_pread, METH_VARARGS, jf_pread__doc },
	{ "write", (PyCFunction) jf_write, METH_VARARGS, jf_write__doc },
	{ "pwrite", (PyCFunction) jf_pwrite, METH_VARARGS, jf_pwrite__doc },
	{ "truncate", (PyCFunction) jf_truncate, METH_VARARGS,
		jf_truncate__doc },
	{ "lseek", (PyCFunction) jf_lseek, METH_VARARGS, jf_lseek__doc },
	{ "jsync", (PyCFunction) jf_jsync, METH_VARARGS, jf_jsync__doc },
	{ "jmove_journal", (PyCFunction) jf_jmove_journal, METH_VARARGS,
		jf_jmove_journal__doc },
	{ "new_trans", (PyCFunction) jf_new_trans, METH_VARARGS,
		jf_new_trans__doc },
	{ NULL }
};

static PyObject *jf_getattr(jfile_object *fp, char *name)
{
	return Py_FindMethod(jfile_methods, (PyObject *)fp, name);
}

static PyTypeObject jfile_type = {
	PyObject_HEAD_INIT(NULL)
	0,
	"libjio.jfile",
	sizeof(jfile_object),
	0,
	(destructor)jf_dealloc,
	0,
	(getattrfunc)jf_getattr,
};


/*
 * The jtrans object
 */

/* delete */
static void jt_dealloc(jtrans_object *tp)
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

static PyObject *jt_add(jtrans_object *tp, PyObject *args)
{
	long rv;
	int len;
	long long offset;
	unsigned char *buf;

	if (!PyArg_ParseTuple(args, "s#L:add", &buf, &len, &offset))
		return NULL;

	rv = jtrans_add(tp->ts, buf, len, offset);
	if (rv == 0)
		return PyErr_SetFromErrno(PyExc_IOError);

	return PyLong_FromLong(rv);
}

/* commit */
PyDoc_STRVAR(jt_commit__doc,
"commit()\n\
\n\
Commits a transaction.\n\
It's a wrapper to jtrans_commit().\n");

static PyObject *jt_commit(jtrans_object *tp, PyObject *args)
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

static PyObject *jt_rollback(jtrans_object *tp, PyObject *args)
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
	{ "add", (PyCFunction) jt_add, METH_VARARGS, jt_add__doc },
	{ "commit", (PyCFunction) jt_commit, METH_VARARGS, jt_commit__doc },
	{ "rollback", (PyCFunction) jt_rollback, METH_VARARGS,
		jt_rollback__doc },
	{ NULL }
};

static PyObject *jt_getattr(jtrans_object *tp, char *name)
{
	return Py_FindMethod(jtrans_methods, (PyObject *)tp, name);
}

static PyTypeObject jtrans_type = {
	PyObject_HEAD_INIT(NULL)
	0,
	"libjio.jtrans",
	sizeof(jtrans_object),
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
	jfile_object *fp;

	flags = O_RDWR;
	mode = 0600;
	jflags = 0;

	if (!PyArg_ParseTuple(args, "s|iii:open", &file, &flags, &mode,
				&jflags))
		return NULL;

	fp = PyObject_New(jfile_object, &jfile_type);
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
"jfsck(name[, jdir])\n\
\n\
Checks the integrity of the file with the given name, using (optionally) jdir\n\
as the journal directory; returns a dictionary with all the different values\n\
of the check (equivalent to the 'struct jfsck_result'). If the path is\n\
incorrect, or there is no journal associated with it, an IOError will be\n\
raised.\n\
It's a wrapper to jfsck().\n");

static PyObject *jf_jfsck(PyObject *self, PyObject *args)
{
	int rv;
	char *name, *jdir = NULL;
	struct jfsck_result res;
	PyObject *dict;

	if (!PyArg_ParseTuple(args, "s|s:jfsck", &name, &jdir))
		return NULL;

	dict = PyDict_New();
	if (dict == NULL)
		return PyErr_NoMemory();

	Py_BEGIN_ALLOW_THREADS
	rv = jfsck(name, jdir, &res);
	Py_END_ALLOW_THREADS

	if (rv == J_ENOMEM) {
		Py_XDECREF(dict);
		return PyErr_NoMemory();
	} else if (rv != 0) {
		Py_XDECREF(dict);
		PyErr_SetObject(PyExc_IOError, PyInt_FromLong(rv));
		return NULL;
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
"jfsck_cleanup(name[, jdir])\n\
\n\
Clean the journal directory for the given file using (optionally) jdir as the\n\
journal directory, and leave it ready to use.\n\
It's a wrapper to jfsck_cleanup().\n");

static PyObject *jf_jfsck_cleanup(PyObject *self, PyObject *args)
{
	long rv;
	char *name, *jdir = NULL;

	if (!PyArg_ParseTuple(args, "s|s:jfsck_cleanup", &name, &jdir))
		return NULL;

	Py_BEGIN_ALLOW_THREADS
	rv = jfsck_cleanup(name, jdir);
	Py_END_ALLOW_THREADS

	if (rv != 1) {
		PyErr_SetObject(PyExc_IOError, PyInt_FromLong(rv));
		return NULL;
	}

	return PyInt_FromLong(rv);
}

/* function table */
static PyMethodDef libjio_functions[] = {
	{ "open", (PyCFunction) jf_open, METH_VARARGS, jf_open__doc },
	{ "jfsck", (PyCFunction) jf_jfsck, METH_VARARGS, jf_jfsck__doc },
	{ "jfsck_cleanup", (PyCFunction) jf_jfsck_cleanup, METH_VARARGS,
		jf_jfsck_cleanup__doc },
	{ NULL, },
};

/* module initialization */
PyDoc_STRVAR(libjio__doc,
"libjio is a library to do transactional, journaled I/O\n\
You can find it at http://blitiri.com.ar/p/libjio/\n\
\n\
Use the open() method to create a file object, and then operate on it.\n\
Please read the documentation for more information.\n");

PyMODINIT_FUNC initlibjio(void)
{
	PyObject* m;

	jfile_type.ob_type = &PyType_Type;
	jtrans_type.ob_type = &PyType_Type;

	m = Py_InitModule3("libjio", libjio_functions, libjio__doc);

	Py_INCREF(&jfile_type);
	PyModule_AddObject(m, "jfile", (PyObject *) &jfile_type);

	Py_INCREF(&jtrans_type);
	PyModule_AddObject(m, "jtrans", (PyObject *) &jtrans_type);

	/* libjio's constants */
	PyModule_AddIntConstant(m, "J_NOLOCK", J_NOLOCK);
	PyModule_AddIntConstant(m, "J_NOROLLBACK", J_NOROLLBACK);
	PyModule_AddIntConstant(m, "J_LINGER", J_LINGER);
	PyModule_AddIntConstant(m, "J_COMMITTED", J_COMMITTED);
	PyModule_AddIntConstant(m, "J_ROLLBACKED", J_ROLLBACKED);
	PyModule_AddIntConstant(m, "J_ROLLBACKING", J_ROLLBACKING);
	PyModule_AddIntConstant(m, "J_RDONLY", J_RDONLY);
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

	/* lseek constants */
	PyModule_AddIntConstant(m, "SEEK_SET", SEEK_SET);
	PyModule_AddIntConstant(m, "SEEK_CUR", SEEK_CUR);
	PyModule_AddIntConstant(m, "SEEK_END", SEEK_END);
}

